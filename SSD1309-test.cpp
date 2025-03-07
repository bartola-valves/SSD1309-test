#include <stdio.h>
#include <cstdlib> // Add this for malloc and free
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <string.h>
#include "fonts.h" // Add this for the font data

// Define I2C pins and instance
#define I2C_PORT i2c0
#define I2C_SDA 4       // GPIO pin for SDA
#define I2C_SCL 5       // GPIO pin for SCL
#define I2C_FREQ 400000 // 400 kHz

#define OLED_ADDR 0x3C // SSD1309 I2C address (0x3C or 0x3D)

// Display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// Portrait (90 degree) dimensions
#define PORTRAIT_WIDTH 64
#define PORTRAIT_HEIGHT 128

// Command/data control byte
#define OLED_COMMAND 0x00
#define OLED_DATA 0x40

// Display orientation options
#define ORIENTATION_NORMAL 0
#define ORIENTATION_90_DEGREES 1
#define ORIENTATION_180_DEGREES 2
#define ORIENTATION_270_DEGREES 3

// Buffer for display
uint8_t display_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

// Current display orientation
uint8_t display_orientation = ORIENTATION_NORMAL;

void draw_pixel(int16_t x, int16_t y, uint8_t color);

// Draw a character at the specified position
void draw_char(int16_t x, int16_t y, char c, uint8_t color, uint8_t size = 1)
{
    if ((c < 32) || (c > 126))
        c = '?';

    c -= 32; // Adjust to font array

    for (uint8_t i = 0; i < 5; i++)
    {
        uint8_t line = font5x7[c * 5 + i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (line & (1 << j))
            {
                if (size == 1)
                {
                    draw_pixel(x + i, y + j, color);
                }
                else
                {
                    for (uint8_t sx = 0; sx < size; sx++)
                    {
                        for (uint8_t sy = 0; sy < size; sy++)
                        {
                            draw_pixel(x + i * size + sx, y + j * size + sy, color);
                        }
                    }
                }
            }
        }
    }
}

// Draw text string at the specified position
void draw_text(int16_t x, int16_t y, const char *text, uint8_t color, uint8_t size = 1)
{
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    while (*text)
    {
        draw_char(cursor_x, cursor_y, *text++, color, size);
        cursor_x += 6 * size; // 5 pixels + 1 pixel space between chars

        // Handle line wrapping if needed
        if (cursor_x > OLED_WIDTH - 6 * size)
        {
            cursor_x = x;
            cursor_y += 8 * size;

            if (cursor_y > OLED_HEIGHT - 8 * size)
            {
                break; // Stop if we run out of screen space
            }
        }
    }
}

// Function to send a command to the display
void ssd1309_command(uint8_t cmd)
{
    uint8_t buf[2] = {OLED_COMMAND, cmd};
    i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, 2, false);
}

// Function to send data to the display
void ssd1309_data(uint8_t *data, size_t len)
{
    uint8_t *buf = (uint8_t *)malloc(len + 1);
    if (buf)
    {
        buf[0] = OLED_DATA;
        memcpy(buf + 1, data, len);
        i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, len + 1, false);
        free(buf);
    }
}

// Initialize the OLED display
void ssd1309_init()
{
    // Display off
    ssd1309_command(0xAE);

    // Set display clock divide ratio/oscillator frequency
    ssd1309_command(0xD5);
    ssd1309_command(0x80); // Default value

    // Set multiplex ratio
    ssd1309_command(0xA8);
    ssd1309_command(0x3F); // 1/64 duty

    // Set display offset
    ssd1309_command(0xD3);
    ssd1309_command(0x00);

    // Set display start line
    ssd1309_command(0x40);

    // Set charge pump
    ssd1309_command(0x8D);
    ssd1309_command(0x14); // Enable charge pump

    // Set memory addressing mode
    ssd1309_command(0x20);
    ssd1309_command(0x00); // Horizontal addressing mode

    // Set segment remap (0xA1 for flipping horizontally)
    ssd1309_command(0xA1);

    // Set COM output scan direction (0xC8 for flipping vertically)
    ssd1309_command(0xC8);

    // Set COM pins hardware configuration
    ssd1309_command(0xDA);
    ssd1309_command(0x12);

    // Set contrast control
    ssd1309_command(0x81);
    ssd1309_command(0xCF);

    // Set precharge period
    ssd1309_command(0xD9);
    ssd1309_command(0xF1);

    // Set VCOMH deselect level
    ssd1309_command(0xDB);
    ssd1309_command(0x40);

    // Entire display on
    ssd1309_command(0xA4);

    // Set normal display
    ssd1309_command(0xA6);

    // Deactivate scroll
    ssd1309_command(0x2E);

    // Display on
    ssd1309_command(0xAF);
}

// Clear display buffer
void clear_display_buffer()
{
    memset(display_buffer, 0, sizeof(display_buffer));
}

// Send display buffer to OLED
void update_display()
{
    // Set column address range
    ssd1309_command(0x21);
    ssd1309_command(0);   // Start column
    ssd1309_command(127); // End column

    // Set page address range
    ssd1309_command(0x22);
    ssd1309_command(0); // Start page
    ssd1309_command(7); // End page

    // Send the buffer
    ssd1309_data(display_buffer, sizeof(display_buffer));
}

// Draw a pixel at (x, y) with color (1: on, 0: off)
void draw_pixel(int16_t x, int16_t y, uint8_t color)
{
    int16_t temp_x = x;
    int16_t temp_y = y;

    // For 90 degrees rotation (portrait mode), we need to transform coordinates
    // but the transformation depends on whether we're working in "portrait space" or "landscape space"
    if (display_orientation == ORIENTATION_90_DEGREES)
    {
        // Check if coordinates are already in portrait space (x: 0-63, y: 0-127)
        if (x < PORTRAIT_WIDTH && y < PORTRAIT_HEIGHT)
        {
            // Convert from portrait space to landscape space coordinates
            temp_x = y;
            temp_y = PORTRAIT_WIDTH - 1 - x;
        }
        else
        {
            // Already in landscape space, use existing transformation
            temp_x = y;
            temp_y = OLED_WIDTH - 1 - x;
        }
    }
    else if (display_orientation == ORIENTATION_180_DEGREES)
    {
        temp_x = OLED_WIDTH - 1 - x;
        temp_y = OLED_HEIGHT - 1 - y;
    }
    else if (display_orientation == ORIENTATION_270_DEGREES)
    {
        temp_x = OLED_HEIGHT - 1 - y;
        temp_y = x;
    }

    // Bounds check for final hardware coordinates
    if (temp_x < 0 || temp_x >= OLED_WIDTH || temp_y < 0 || temp_y >= OLED_HEIGHT)
        return;

    // Calculate byte position and bit offset
    uint16_t byte_idx = temp_x + (temp_y / 8) * OLED_WIDTH;
    uint8_t bit_position = temp_y % 8;

    if (color)
    {
        display_buffer[byte_idx] |= (1 << bit_position);
    }
    else
    {
        display_buffer[byte_idx] &= ~(1 << bit_position);
    }
}

// Draw a test pattern - checkerboard
void draw_test_pattern()
{
    for (int y = 0; y < OLED_HEIGHT; y += 8)
    {
        for (int x = 0; x < OLED_WIDTH; x += 8)
        {
            for (int dy = 0; dy < 8; dy++)
            {
                for (int dx = 0; dx < 8; dx++)
                {
                    if ((x / 8 + y / 8) % 2 == 0)
                    {
                        draw_pixel(x + dx, y + dy, 1);
                    }
                    else
                    {
                        draw_pixel(x + dx, y + dy, 0);
                    }
                }
            }
        }
    }
}

// Scan the I2C bus for connected devices
void scan_i2c_bus()
{
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr_base = 0; addr_base < 8; addr_base++)
    {
        printf("%d0: ", addr_base);

        for (int addr_offset = 0; addr_offset < 16; addr_offset++)
        {
            int addr = (addr_base << 4) + addr_offset;

            // Skip address 0 (general call address) and addresses > 0x7F
            if (addr == 0 || addr > 0x7F)
            {
                printf("   ");
                continue;
            }

            // Try to read a byte from the device
            // If it acknowledges, then it exists
            uint8_t rx_data;
            int ret = i2c_read_blocking(I2C_PORT, addr, &rx_data, 1, false);

            if (ret >= 0)
            {
                printf("%02X ", addr); // Device found
            }
            else
            {
                printf("-- "); // No device
            }
        }
        printf("\n");
    }
    printf("\nScan complete.\n");
}

// Set display orientation
void set_display_orientation(uint8_t orientation)
{
    // We're only setting the software orientation, not changing hardware configuration
    display_orientation = orientation;
}

int main()
{
    stdio_init_all();

    // Initialize I2C
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    printf("Initializing SSD1309 OLED display...\n");

    // Scan I2C bus to find connected devices
    scan_i2c_bus();

    // The I2C address from scan can be used to update OLED_ADDR if needed
    // The address might be displayed as 0x3C or 0x78 depending on how it's represented
    // (0x3C is the 7-bit address, 0x78 is the 8-bit address with write bit)

    printf("Initializing SSD1309 OLED display at address 0x%02X...\n", OLED_ADDR);

    // Initialize the OLED display
    ssd1309_init();

    // Set the software orientation to portrait
    display_orientation = ORIENTATION_90_DEGREES;

    // Clear the buffer
    clear_display_buffer();

    // Draw text in portrait mode
    // In portrait mode, X is limited to 0-63, Y to 0-127
    draw_text(5, 10, "Portrait", 1, 1);
    draw_text(5, 30, "Mode", 1, 2);
    draw_text(5, 60, "64x128", 1, 1);
    draw_text(5, 80, "OLED", 1, 1);
    draw_text(5, 100, "Display", 1, 1);

    // Update the display
    update_display();

    printf("OLED display initialized in portrait mode.\n");

    // Main loop
    while (1)
    {
        // Animation or additional updates here
        busy_wait_ms(1000);
    }
}