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

// Claw4 SD Slot 0 mapping copied from the read-only Metalio board reference.
// The reference asks integrators to verify these assignments against the
// hardware schematic; runtime SD success remains a device-level check.
#define SDMMC_CLK_PIN GPIO_NUM_43
#define SDMMC_CMD_PIN GPIO_NUM_44
#define SDMMC_D0_PIN GPIO_NUM_39
#define SDMMC_D1_PIN GPIO_NUM_40
#define SDMMC_D2_PIN GPIO_NUM_41
#define SDMMC_D3_PIN GPIO_NUM_42
#define SDMMC_LDO_CHAN_ID 4
