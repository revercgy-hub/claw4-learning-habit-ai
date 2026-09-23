#include "wifi_board.h"
#include "config.h"
#include "sdkconfig.h"
#include "claw4_audio.h"
#include "board_algorithms.h"
#include "display/lcd_display.h"
#include "esp_lcd_nv3051f.h"
#include <driver/i2c_master.h>
#include <driver/sdmmc_host.h>
#include <driver/uart.h>
#include <esp_cam_sensor_xclk.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_idf_version.h>
#include <esp_ldo_regulator.h>
#include <esp_lvgl_port.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_video_device.h>
#include <esp_video_ioctl.h>
#include <esp_video_init.h>
#include <esp_vfs_fat.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <linux/videodev2.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>
#include <mutex>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

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

// M0 scope: boot, display, touch, Wi-Fi, electrical audio, SD mount and one-frame camera diagnostics.
// The camera returns to power-down after each boot-time diagnostic.
// The original expansion latch is read before writing; unrelated rails are preserved.
class Claw4Board final : public WifiBoard {
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t expander_ = nullptr;
    std::mutex expander_mutex_;
    Display* display_ = nullptr;
    Claw4Audio* audio_ = nullptr;
    esp_ldo_channel_handle_t dsi_power_ = nullptr;
    sd_pwr_ctrl_handle_t sd_power_control_ = nullptr;
    sdmmc_card_t* sd_card_ = nullptr;

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
        const auto error = claw4::RetryThree([&](unsigned attempt) {
            const auto error = operation();
            if (error == ESP_OK) {
                if (attempt > 1) ESP_LOGI("Claw4V6", "I2C_RECOVERED operation=%s attempt=%u", name, attempt);
            } else {
                ESP_LOGW("Claw4V6", "I2C_RETRY operation=%s attempt=%u error=%s", name, attempt, esp_err_to_name(error));
            }
            return error;
        }, [&] {
            const auto reset = i2c_master_bus_reset(bus_);
            ESP_LOGI("Claw4V6", "I2C_BUS_RESET result=%s", esp_err_to_name(reset));
        }, [](unsigned delay_ms) { vTaskDelay(pdMS_TO_TICKS(delay_ms)); });
        if (error != ESP_OK) HaltHardware(name, error);
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
    void SetInput(uint8_t pin) {
        std::lock_guard<std::mutex> lock(expander_mutex_);
        WriteExpander(6, ReadExpander(6) | (1U << pin));
    }
    esp_err_t ReadInputLevels(uint16_t* levels) {
        std::lock_guard<std::mutex> lock(expander_mutex_);
        uint8_t reg = 0; // TCA9555 input port 0/1 registers.
        uint8_t data[2]{};
        const esp_err_t error = i2c_master_transmit_receive(
            expander_, &reg, 1, data, sizeof(data), 100);
        if (error == ESP_OK) {
            *levels = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
        }
        return error;
    }
    void MonitorPowerKey() {
        constexpr uint16_t kPowerKeyInput = 1U << 5; // TCA9555 P0_5, active low.
        constexpr uint32_t kPollMs = 20;
        uint16_t input_levels = 0;
        esp_err_t read_error = ReadInputLevels(&input_levels);
        if (read_error != ESP_OK) {
            ESP_LOGW("Claw4V6", "POWER_KEY_DIAGNOSTIC unavailable at start error=%s",
                     esp_err_to_name(read_error));
            return;
        }
        const bool pressed = (input_levels & kPowerKeyInput) == 0;
        claw4::ActiveLowButtonDebouncer debounce(50, 1500);
        debounce.Begin(pressed, uint64_t(esp_timer_get_time() / 1000));
        ESP_LOGI("Claw4V6", "POWER_KEY_DIAGNOSTIC armed_after_boot_release=%d",
                 pressed ? 0 : 1);
        unsigned consecutive_read_errors = 0;
        for (;;) {
            read_error = ReadInputLevels(&input_levels);
            if (read_error != ESP_OK) {
                if (consecutive_read_errors++ % 50 == 0) {
                    ESP_LOGW("Claw4V6", "POWER_KEY_DIAGNOSTIC read error=%s",
                             esp_err_to_name(read_error));
                }
                vTaskDelay(pdMS_TO_TICKS(kPollMs));
                continue;
            }
            consecutive_read_errors = 0;
            const bool current_pressed = (input_levels & kPowerKeyInput) == 0;
            const auto event = debounce.Sample(
                current_pressed, uint64_t(esp_timer_get_time() / 1000));
            if (event == claw4::ButtonEvent::ShortPress) {
                ESP_LOGI("Claw4V6", "POWER_KEY_SHORT detected; diagnostic has no power side effects");
            } else if (event == claw4::ButtonEvent::LongPress) {
                ESP_LOGI("Claw4V6", "POWER_KEY_LONG detected; diagnostic has no power side effects");
            }
            vTaskDelay(pdMS_TO_TICKS(kPollMs));
        }
    }
    static void PowerKeyTask(void* context) {
        static_cast<Claw4Board*>(context)->MonitorPowerKey();
        vTaskDelete(nullptr);
    }
    void StartPowerKeyDiagnostic() {
        constexpr uint32_t kStackSize = 3072;
        constexpr UBaseType_t kPriority = 2;
        const BaseType_t result = xTaskCreate(PowerKeyTask, "v6_power_key",
                                              kStackSize, this, kPriority, nullptr);
        if (result != pdPASS) {
            ESP_LOGE("Claw4V6", "POWER_KEY_DIAGNOSTIC task creation failed");
        }
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
        SetOutput(3, true);  // SD card power is active-low; leave it off until probe.
        SetInput(5);         // User power-key input, active low.
        ESP_LOGI("Claw4V6", "I2C/TCA9555 initialized");
    }
    static esp_err_t SharedHostedSdmmcInit() { return ESP_OK; }
    static esp_err_t SharedHostedSdmmcDeinit() { return ESP_OK; }
    void MountSdCardDiagnostic() {
        // The board reference places the SD card on Slot 0 and ESP-Hosted C5
        // on Slot 1. Start only after NetworkEvent::Scanning proves Hosted
        // initialization has reached the Wi-Fi path.
        SetOutput(3, false); // TCA9555 SD rail is active-low.
        sd_pwr_ctrl_ldo_config_t ldo_config{};
        ldo_config.ldo_chan_id = SDMMC_LDO_CHAN_ID;
        esp_err_t error = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config,
                                                      &sd_power_control_);
        if (error != ESP_OK) {
            ESP_LOGW("Claw4V6", "SD_DIAGNOSTIC unavailable power_control=%s",
                     esp_err_to_name(error));
            return;
        }

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = 20000; // Conservative 20 MHz M0 probe.
        host.flags &= ~SDMMC_HOST_FLAG_DDR;
#if CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE && \
    (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
        host.init = SharedHostedSdmmcInit;
        host.deinit = SharedHostedSdmmcDeinit;
#endif
        host.pwr_ctrl_handle = sd_power_control_;

        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
        slot.clk = SDMMC_CLK_PIN;
        slot.cmd = SDMMC_CMD_PIN;
        slot.d0 = SDMMC_D0_PIN;
        slot.d1 = SDMMC_D1_PIN;
        slot.d2 = SDMMC_D2_PIN;
        slot.d3 = SDMMC_D3_PIN;
#endif
        slot.width = 4;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 1,
            .allocation_unit_size = 16 * 1024,
        };
        error = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot,
                                        &mount_config, &sd_card_);
        if (error != ESP_OK) {
            sd_card_ = nullptr;
            ESP_LOGW("Claw4V6", "SD_DIAGNOSTIC not_mounted error=%s; no format or file access",
                     esp_err_to_name(error));
            return;
        }

        const uint64_t capacity_bytes =
            uint64_t(sd_card_->csd.capacity) * sd_card_->csd.sector_size;
        ESP_LOGI("Claw4V6", "SD_DIAGNOSTIC mounted blocks=%lu sector_bytes=%u capacity_bytes=%llu",
                 static_cast<unsigned long>(sd_card_->csd.capacity),
                 unsigned(sd_card_->csd.sector_size),
                 static_cast<unsigned long long>(capacity_bytes));
    }
    static void SdCardTask(void* context) {
        auto* board = static_cast<Claw4Board*>(context);
        board->MountSdCardDiagnostic();
        board->LaunchCameraDiagnostic();
        vTaskDelete(nullptr);
    }
    void LaunchSdCardDiagnostic() {
        constexpr uint32_t kStackSize = 6144;
        constexpr UBaseType_t kPriority = 1;
        const BaseType_t result = xTaskCreate(SdCardTask, "v6_sd_probe",
                                              kStackSize, this, kPriority, nullptr);
        if (result != pdPASS) {
            ESP_LOGW("Claw4V6", "SD_DIAGNOSTIC task creation failed");
        }
    }

    bool CaptureSingleCameraFrame() {
        constexpr uint32_t kBufferCount = 2;
        constexpr uint32_t kMaxFrameBytes = 2 * 1024 * 1024;
        constexpr v4l2_buf_type kType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        struct MappedBuffer {
            void* address = nullptr;
            size_t length = 0;
        } buffers[kBufferCount];

        bool captured = false;
        bool buffers_requested = false;
        bool streaming = false;
        uint32_t mapped_count = 0;
        uint32_t frame_width = 0;
        uint32_t frame_height = 0;
        uint32_t frame_format = 0;
        uint32_t frame_bytes = 0;
        int cleanup_error = 0;
        const char* failed_stage = "open";
        int capture_error = 0;
        int fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
        if (fd >= 0) {
            do {
                v4l2_capability capability{};
                failed_stage = "query_capability";
                if (ioctl(fd, VIDIOC_QUERYCAP, &capability) != 0) {
                    capture_error = errno;
                    break;
                }
                const uint32_t capabilities =
                    (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
                        ? capability.device_caps : capability.capabilities;
                if ((capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0 ||
                    (capabilities & V4L2_CAP_STREAMING) == 0) {
                    capture_error = ENOTSUP;
                    break;
                }

                bool raw8_supported = false;
                for (uint32_t index = 0; index < 32; ++index) {
                    v4l2_fmtdesc description{};
                    description.index = index;
                    description.type = kType;
                    if (ioctl(fd, VIDIOC_ENUM_FMT, &description) != 0) break;
                    if (description.pixelformat == V4L2_PIX_FMT_SBGGR8) {
                        raw8_supported = true;
                        break;
                    }
                }
                failed_stage = "enumerate_raw8";
                if (!raw8_supported) {
                    capture_error = ENOTSUP;
                    break;
                }

                uint32_t selected_width = 0;
                uint32_t selected_height = 0;
                uint64_t selected_area = UINT64_MAX;
                for (uint32_t index = 0; index < 32; ++index) {
                    v4l2_frmsizeenum size{};
                    size.index = index;
                    size.pixel_format = V4L2_PIX_FMT_SBGGR8;
                    if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) != 0) break;
                    uint32_t width = 0;
                    uint32_t height = 0;
                    if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                        width = size.discrete.width;
                        height = size.discrete.height;
                    } else if (size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                               size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                        width = size.stepwise.min_width;
                        height = size.stepwise.min_height;
                    }
                    if (width == 0 || height == 0) continue;
                    const uint64_t area = uint64_t(width) * height;
                    if (area < selected_area) {
                        selected_area = area;
                        selected_width = width;
                        selected_height = height;
                    }
                }
                failed_stage = "enumerate_frame_size";
                if (selected_width == 0 || selected_height == 0) {
                    capture_error = ENOTSUP;
                    break;
                }

                v4l2_format format{};
                format.type = kType;
                format.fmt.pix.width = selected_width;
                format.fmt.pix.height = selected_height;
                format.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
                failed_stage = "set_frame_format";
                if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
                    capture_error = errno;
                    break;
                }
                if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_SBGGR8 ||
                    format.fmt.pix.width == 0 || format.fmt.pix.height == 0 ||
                    format.fmt.pix.sizeimage == 0 ||
                    format.fmt.pix.sizeimage > kMaxFrameBytes) {
                    capture_error = EOVERFLOW;
                    break;
                }

                v4l2_requestbuffers request{};
                request.count = kBufferCount;
                request.type = kType;
                request.memory = V4L2_MEMORY_MMAP;
                failed_stage = "request_buffers";
                if (ioctl(fd, VIDIOC_REQBUFS, &request) != 0) {
                    capture_error = errno;
                    break;
                }
                buffers_requested = true;
                if (request.count == 0 || request.count > kBufferCount) {
                    capture_error = ENOMEM;
                    break;
                }

                for (uint32_t index = 0; index < request.count; ++index) {
                    v4l2_buffer buffer{};
                    buffer.type = kType;
                    buffer.memory = V4L2_MEMORY_MMAP;
                    buffer.index = index;
                    failed_stage = "query_buffer";
                    if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) != 0) {
                        capture_error = errno;
                        break;
                    }
                    if (buffer.length == 0 || buffer.length > kMaxFrameBytes) {
                        capture_error = EOVERFLOW;
                        break;
                    }
                    buffers[index].length = buffer.length;
                    buffers[index].address = mmap(nullptr, buffer.length,
                                                  PROT_READ | PROT_WRITE, MAP_SHARED,
                                                  fd, buffer.m.offset);
                    if (buffers[index].address == MAP_FAILED) {
                        buffers[index].address = nullptr;
                        capture_error = errno;
                        break;
                    }
                    ++mapped_count;
                    failed_stage = "queue_buffer";
                    if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
                        capture_error = errno;
                        break;
                    }
                }
                if (capture_error != 0) break;

                timeval dequeue_timeout{};
                dequeue_timeout.tv_sec = 3;
                failed_stage = "set_dequeue_timeout";
                if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &dequeue_timeout) != 0) {
                    capture_error = errno;
                    break;
                }

                int type = kType;
                failed_stage = "stream_on";
                if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
                    capture_error = errno;
                    break;
                }
                streaming = true;

                failed_stage = "dequeue_frame";
                v4l2_buffer frame{};
                frame.type = kType;
                frame.memory = V4L2_MEMORY_MMAP;
                if (ioctl(fd, VIDIOC_DQBUF, &frame) != 0) {
                    capture_error = errno;
                } else if (frame.index >= mapped_count ||
                           (frame.flags & V4L2_BUF_FLAG_DONE) == 0 ||
                           (frame.flags & V4L2_BUF_FLAG_ERROR) != 0 ||
                           frame.bytesused == 0 ||
                           frame.bytesused > buffers[frame.index].length) {
                    capture_error = EIO;
                } else {
                    captured = true;
                    frame_width = format.fmt.pix.width;
                    frame_height = format.fmt.pix.height;
                    frame_format = format.fmt.pix.pixelformat;
                    frame_bytes = frame.bytesused;
                }
            } while (false);
        } else {
            capture_error = errno;
        }

        if (streaming) {
            int type = kType;
            if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) cleanup_error = errno;
        }
        for (uint32_t index = 0; index < mapped_count; ++index) {
            if (munmap(buffers[index].address, buffers[index].length) != 0 &&
                cleanup_error == 0) {
                cleanup_error = errno;
            }
        }
        if (buffers_requested) {
            v4l2_requestbuffers release{};
            release.count = 0;
            release.type = kType;
            release.memory = V4L2_MEMORY_MMAP;
            if (ioctl(fd, VIDIOC_REQBUFS, &release) != 0 && cleanup_error == 0) {
                cleanup_error = errno;
            }
        }
        if (fd >= 0 && close(fd) != 0 && cleanup_error == 0) cleanup_error = errno;

        if (captured) {
            ESP_LOGI("Claw4V6", "CAMERA_DIAGNOSTIC frame_capture=1 width=%u height=%u format=0x%08lx bytes=%u saved=0 uploaded=0 cleanup_error=%d",
                     unsigned(frame_width), unsigned(frame_height),
                     static_cast<unsigned long>(frame_format), unsigned(frame_bytes),
                     cleanup_error);
        } else {
            ESP_LOGW("Claw4V6", "CAMERA_DIAGNOSTIC frame_capture=0 stage=%s error=%d cleanup_error=%d",
                     failed_stage, capture_error, cleanup_error);
        }
        return captured && cleanup_error == 0;
    }

    void ProbeCameraSensor() {
        // Take one bounded RAW8 frame into driver-owned RAM, report only format
        // metadata, then release its buffers before powering the camera down.
        SetOutput(2, false); // CAM_PWDN is active-low.
        vTaskDelay(pdMS_TO_TICKS(200));

        esp_cam_sensor_xclk_handle_t xclk = nullptr;
        bool xclk_started = false;
        bool video_initialized = false;
        esp_err_t error = esp_cam_sensor_xclk_allocate(
            ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &xclk);
        if (error == ESP_OK) {
            esp_cam_sensor_xclk_config_t xclk_config{};
            xclk_config.esp_clock_router_cfg.xclk_pin = CAMERA_XCLK_PIN;
            xclk_config.esp_clock_router_cfg.xclk_freq_hz = CAMERA_XCLK_FREQ_HZ;
            error = esp_cam_sensor_xclk_start(xclk, &xclk_config);
            xclk_started = error == ESP_OK;
        }
        if (error == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(50));
            esp_video_init_csi_config_t csi_config{};
            csi_config.reset_pin = static_cast<gpio_num_t>(-1);
            csi_config.pwdn_pin = static_cast<gpio_num_t>(-1);
            csi_config.sccb_config.init_sccb = false;
            csi_config.sccb_config.i2c_handle = bus_;
            csi_config.sccb_config.freq = 100000;
            esp_video_init_config_t video_config{};
            video_config.csi = &csi_config;
            error = esp_video_init(&video_config);
            video_initialized = error == ESP_OK;
        }
        if (video_initialized) {
            ESP_LOGI("Claw4V6", "CAMERA_DIAGNOSTIC sensor_stack_initialized=1");
            CaptureSingleCameraFrame();
            const esp_err_t deinit_error = esp_video_deinit();
            if (deinit_error != ESP_OK) {
                ESP_LOGW("Claw4V6", "CAMERA_DIAGNOSTIC deinit=%s",
                         esp_err_to_name(deinit_error));
            }
        } else {
            ESP_LOGW("Claw4V6", "CAMERA_DIAGNOSTIC sensor_stack_initialized=0 error=%s frame_capture=0",
                     esp_err_to_name(error));
        }
        if (xclk_started) esp_cam_sensor_xclk_stop(xclk);
        if (xclk != nullptr) esp_cam_sensor_xclk_free(xclk);
        SetOutput(2, true); // Return the camera to its powered-down state.
    }
    static void CameraDiagnosticTask(void* context) {
        static_cast<Claw4Board*>(context)->ProbeCameraSensor();
        vTaskDelete(nullptr);
    }
    void LaunchCameraDiagnostic() {
        constexpr uint32_t kStackSize = 4096;
        constexpr UBaseType_t kPriority = 1;
        const BaseType_t result = xTaskCreate(CameraDiagnosticTask, "v6_camera_probe",
                                              kStackSize, this, kPriority, nullptr);
        if (result != pdPASS) {
            ESP_LOGW("Claw4V6", "CAMERA_DIAGNOSTIC task creation failed");
        }
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
        InitializeBus();
        InitializeAudioModule();
        InitializeDisplay();
        InitializeTouch();
        audio_ = new Claw4Audio([this](bool enable) { SetOutput(8, enable); });
#if CONFIG_CLAW4_M0_DIAGNOSTICS
        StartPowerKeyDiagnostic();
#endif
    }
    void RequestSdCardDiagnostic() { LaunchSdCardDiagnostic(); }
    Display* GetDisplay() override { return display_; }
    AudioCodec* GetAudioCodec() override { return audio_; }
    Backlight* GetBacklight() override {
        static PwmBacklight light(DISPLAY_BACKLIGHT_PIN, false);
        return &light;
    }
};
DECLARE_BOARD(Claw4Board);

void Claw4StartSdCardDiagnostic() {
    static_cast<Claw4Board&>(Board::GetInstance()).RequestSdCardDiagnostic();
}
