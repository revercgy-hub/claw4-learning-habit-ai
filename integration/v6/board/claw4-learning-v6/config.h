#pragma once
#include <driver/gpio.h>
// Metalio ca3aa3fa: do not substitute pins from an Espressif evaluation board.
#define DISPLAY_WIDTH 720
#define DISPLAY_HEIGHT 720
#define I2C_SDA_PIN GPIO_NUM_7
#define I2C_SCL_PIN GPIO_NUM_8
#define DISPLAY_RESET_PIN GPIO_NUM_3
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_52
#define BOOT_BUTTON_GPIO GPIO_NUM_35
#define AUDIO_BCLK GPIO_NUM_12
#define AUDIO_WS GPIO_NUM_10
#define AUDIO_DOUT GPIO_NUM_9
#define AUDIO_DIN GPIO_NUM_11
