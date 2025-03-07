#include <stdio.h>
#include <cstdlib> // Add this for malloc and free
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <string.h>

// Define I2C pins and instance
#define I2C_PORT i2c0
#define I2C_SDA 4       // GPIO pin for SDA
#define I2C_SCL 5       // GPIO pin for SCL
#define I2C_FREQ 400000 // 400 kHz

#define OLED_ADDR 0x3C // SSD1309 I2C address (0x3C or 0x3D)

// Display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// Command/data control byte
#define OLED_COMMAND 0x00
#define OLED_DATA 0x40

// Buffer for display
uint8_t display_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

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
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
        return;

    // Calculate byte position and bit offset
    uint16_t byte_idx = x + (y / 8) * OLED_WIDTH;
    uint8_t bit_position = y % 8;

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

    // Clear the buffer
    clear_display_buffer();

    // Draw a test pattern
    draw_test_pattern();

    // Update the display
    update_display();

    printf("OLED display initialized and test pattern drawn.\n");

    // Main loop
    while (1)
    {
        busy_wait_ms(1000);
    }
}