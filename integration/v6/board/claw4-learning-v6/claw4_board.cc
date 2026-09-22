#include "wifi_board.h"
#include "config.h"
#include "claw4_audio.h"
#include "display/lcd_display.h"
#include "esp_lcd_nv3051f.h"
#include <driver/i2c_master.h>
#include <driver/uart.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_ldo_regulator.h>
#include <esp_lvgl_port.h>
#include <esp_log.h>
#include <mutex>

// Keep the upstream UI, but give this panel its native RGB888 frame buffers.
// The generic MipiLcdDisplay defaults to RGB565 partial transfers.
class Claw4Display final : public LcdDisplay {
public:
    Claw4Display(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel)
        : LcdDisplay(io, panel, 720, 720) {
        lv_init();
        lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
        ESP_ERROR_CHECK(lvgl_port_init(&port));
        lvgl_port_display_cfg_t config{};
        config.io_handle = io;
        config.panel_handle = panel;
        config.buffer_size = 720 * 720;
        config.double_buffer = true;
        config.hres = 720;
        config.vres = 720;
        config.color_format = LV_COLOR_FORMAT_RGB888;
        config.flags.buff_spiram = true;
        config.flags.full_refresh = true;
        lvgl_port_display_dsi_cfg_t dsi{};
        dsi.flags.avoid_tearing = true;
        display_ = lvgl_port_add_disp_dsi(&config, &dsi);
        ESP_ERROR_CHECK(display_ ? ESP_OK : ESP_FAIL);
        ESP_ERROR_CHECK(lvgl_port_lock(1000) ? ESP_OK : ESP_ERR_TIMEOUT);
        lv_display_add_event_cb(display_, [](lv_event_t*) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                ESP_LOGI("Claw4V6", "LVGL first refresh completed (physical image still requires confirmation)");
            }
        }, LV_EVENT_REFR_READY, nullptr);
        lvgl_port_unlock();
        ESP_LOGI("Claw4V6", "Native RGB888 display, panel double buffers, full refresh");
    }
};

// M0 scope: boot, display, touch, Wi-Fi and electrical audio. Camera/SD next.
// The original expansion latch is read before writing; unrelated rails are preserved.
class Claw4Board final : public WifiBoard {
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t expander_ = nullptr;
    std::mutex expander_mutex_;
    Display* display_ = nullptr;
    Claw4Audio* audio_ = nullptr;
    esp_ldo_channel_handle_t dsi_power_ = nullptr;
    esp_ldo_channel_handle_t sd_io_power_ = nullptr;

    [[noreturn]] void HaltHardware(const char* operation, esp_err_t error) {
        // Do not proceed with unknown rail state or turn a missing device into
        // an endless reboot loop. Sleep keeps the console and idle task alive.
        for (;;) {
            ESP_LOGE("Claw4V6", "BOOT_BLOCKED component=TCA9555 operation=%s error=%s", operation, esp_err_to_name(error));
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    template<typename Operation>
    void ExpanderTransaction(const char* name, Operation operation) {
        esp_err_t error = ESP_FAIL;
        for (unsigned attempt = 1; attempt <= 3; ++attempt) {
            error = operation();
            if (error == ESP_OK) {
                if (attempt > 1) ESP_LOGI("Claw4V6", "I2C_RECOVERED operation=%s attempt=%u", name, attempt);
                return;
            }
            ESP_LOGW("Claw4V6", "I2C_RETRY operation=%s attempt=%u error=%s", name, attempt, esp_err_to_name(error));
            if (attempt < 3) {
                const auto reset = i2c_master_bus_reset(bus_);
                ESP_LOGI("Claw4V6", "I2C_BUS_RESET result=%s", esp_err_to_name(reset));
                vTaskDelay(pdMS_TO_TICKS(100 * attempt));
            }
        }
        HaltHardware(name, error);
    }

    uint16_t ReadExpander(uint8_t reg) {
        uint8_t data[2]{};
        ExpanderTransaction("read", [&] { return i2c_master_transmit_receive(expander_, &reg, 1, data, 2, 100); });
        return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    }
    void WriteExpander(uint8_t reg, uint16_t value) {
        uint8_t data[] = {reg, uint8_t(value), uint8_t(value >> 8)};
        ExpanderTransaction("write", [&] { return i2c_master_transmit(expander_, data, sizeof(data), 100); });
    }
    void SetOutput(uint8_t pin, bool high) {
        std::lock_guard<std::mutex> lock(expander_mutex_);
        uint16_t output = ReadExpander(2);
        output = high ? output | (1U << pin) : output & ~(1U << pin);
        WriteExpander(2, output); // Configure latch before changing direction.
        WriteExpander(6, ReadExpander(6) & ~(1U << pin));
    }
    void InitializeBus() {
        i2c_master_bus_config_t config{};
        config.i2c_port = I2C_NUM_1;
        config.sda_io_num = I2C_SDA_PIN;
        config.scl_io_num = I2C_SCL_PIN;
        config.clk_source = I2C_CLK_SRC_DEFAULT;
        config.glitch_ignore_cnt = 7;
        config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus_));
        i2c_device_config_t device{};
        device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device.device_address = 0x20;
        device.scl_speed_hz = 100000;
        ExpanderTransaction("probe", [&] { return i2c_master_probe(bus_, 0x20, 100); });
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_, &device, &expander_));
        SetOutput(8, false); // PA muted until official AudioService enables output.
        SetOutput(1, true);  // Local Wi-Fi audio route.
        SetOutput(6, true);  // Board audio module supplies I2S clocks.
        SetOutput(2, true);  // Camera remains powered down until explicit capture.
        ESP_LOGI("Claw4V6", "I2C/TCA9555 initialized");
    }
    void InitializeAudioModule() {
        uart_config_t config{};
        config.baud_rate = 115200;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &config));
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 26, 27, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 0, 0, nullptr, 0));
        vTaskDelay(pdMS_TO_TICKS(300));
        uart_write_bytes(UART_NUM_2, "AT+RX=2\r\n", 9);
        vTaskDelay(pdMS_TO_TICKS(700));
        uart_write_bytes(UART_NUM_2, "AT+MODE=1\r\n", 11);
        // A bounded read prevents a missing module from hanging board startup.
        uint8_t reply[128]{};
        const int bytes = uart_read_bytes(UART_NUM_2, reply, sizeof(reply), pdMS_TO_TICKS(2200));
        ESP_LOGI("Claw4V6", "Audio module local-mode response bytes=%d (clock test still required)", bytes);
    }
    void InitializeDisplay() {
        esp_ldo_channel_config_t power{};
        power.chan_id = 3;
        power.voltage_mv = 2500;
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&power, &dsi_power_));
        esp_lcd_dsi_bus_handle_t dsi = nullptr;
        esp_lcd_dsi_bus_config_t bus{};
        bus.bus_id = 0;
        bus.num_data_lanes = 2;
        bus.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
        bus.lane_bit_rate_mbps = 1000;
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus, &dsi));
        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_dbi_io_config_t dbi = NV3051F_PANEL_IO_DBI_CONFIG();
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi, &dbi, &io));
        esp_lcd_dpi_panel_config_t dpi{};
        dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
        dpi.dpi_clock_freq_mhz = 36;
        dpi.num_fbs = 2;
        dpi.in_color_format = LCD_COLOR_FMT_RGB888;
        dpi.out_color_format = LCD_COLOR_FMT_RGB888;
        dpi.video_timing.h_size = 720;
        dpi.video_timing.v_size = 720;
        dpi.video_timing.hsync_back_porch = 44;
        dpi.video_timing.hsync_pulse_width = 2;
        dpi.video_timing.hsync_front_porch = 46;
        dpi.video_timing.vsync_back_porch = 14;
        dpi.video_timing.vsync_pulse_width = 2;
        dpi.video_timing.vsync_front_porch = 16;
        nv3051f_vendor_config_t vendor{};
        vendor.mipi_config.dsi_bus = dsi;
        vendor.mipi_config.dpi_config = &dpi;
        esp_lcd_panel_dev_config_t panel_config{};
        panel_config.reset_gpio_num = DISPLAY_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 24;
        panel_config.vendor_config = &vendor;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3051f(io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_enable_dma2d(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        display_ = new Claw4Display(io, panel);
        GetBacklight()->SetBrightness(65);
        ESP_LOGI("Claw4V6", "NV3051F display initialized, native RGB888");
    }
    void InitializeTouch() {
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        config.scl_speed_hz = 100000;
        if (i2c_master_probe(bus_, config.dev_addr, 100) != ESP_OK) config.dev_addr = 0x14;
        esp_lcd_panel_io_handle_t io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus_, &config, &io));
        esp_lcd_touch_config_t touch_config{};
        touch_config.x_max = 720;
        touch_config.y_max = 720;
        touch_config.rst_gpio_num = GPIO_NUM_NC;
        touch_config.int_gpio_num = GPIO_NUM_NC;
        esp_lcd_touch_handle_t touch = nullptr;
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io, &touch_config, &touch));
        const lvgl_port_touch_cfg_t lv_touch = {.disp = lv_display_get_default(), .handle = touch};
        ESP_ERROR_CHECK(lvgl_port_add_touch(&lv_touch) ? ESP_OK : ESP_FAIL);
        ESP_LOGI("Claw4V6", "GT911 touch initialized");
    }
public:
    Claw4Board() {
        // Original Claw4 initializes this rail through its SD-card manager.
        // Keep the same 3.3V rail even before the SD test is integrated.
        esp_ldo_channel_config_t sd_power{};
        sd_power.chan_id = 4;
        sd_power.voltage_mv = 3300;
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&sd_power, &sd_io_power_));
        InitializeBus();
        InitializeAudioModule();
        InitializeDisplay();
        InitializeTouch();
        audio_ = new Claw4Audio([this](bool enable) { SetOutput(8, enable); });
    }
    Display* GetDisplay() override { return display_; }
    AudioCodec* GetAudioCodec() override { return audio_; }
    Backlight* GetBacklight() override {
        static PwmBacklight light(DISPLAY_BACKLIGHT_PIN, false);
        return &light;
    }
};
DECLARE_BOARD(Claw4Board);
