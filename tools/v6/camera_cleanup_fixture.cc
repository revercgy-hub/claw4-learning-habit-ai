// Inserts the two real Claw4Board camera methods from claw4_board.cc.
// Fake driver calls only; no alternate camera algorithm lives here.
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/time.h>

enum v4l2_buf_type { V4L2_BUF_TYPE_VIDEO_CAPTURE = 1 };
constexpr unsigned V4L2_CAP_DEVICE_CAPS = 1U << 0;
constexpr unsigned V4L2_CAP_VIDEO_CAPTURE = 1U << 1;
constexpr unsigned V4L2_CAP_STREAMING = 1U << 2;
constexpr unsigned V4L2_PIX_FMT_SBGGR8 = 123;
constexpr unsigned V4L2_FRMSIZE_TYPE_DISCRETE = 1;
constexpr unsigned V4L2_FRMSIZE_TYPE_STEPWISE = 2;
constexpr unsigned V4L2_FRMSIZE_TYPE_CONTINUOUS = 3;
constexpr unsigned V4L2_MEMORY_MMAP = 1;
constexpr unsigned V4L2_BUF_FLAG_DONE = 1;
constexpr unsigned V4L2_BUF_FLAG_ERROR = 2;
constexpr int PROT_READ = 1, PROT_WRITE = 2, MAP_SHARED = 1;
#define MAP_FAILED reinterpret_cast<void*>(-1)
#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME "/dev/video0"
#define O_RDONLY 0
#define CAMERA_XCLK_PIN 10
#define CAMERA_XCLK_FREQ_HZ 24000000
#define ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER 1
#define ESP_OK 0
#define ESP_FAIL 1
#define pdMS_TO_TICKS(x) (x)
struct v4l2_capability { unsigned capabilities=0, device_caps=0; };
struct v4l2_fmtdesc { unsigned index=0, type=0, pixelformat=0; };
struct v4l2_frmsizeenum {
    unsigned index=0, pixel_format=0, type=0;
    struct { unsigned width=0, height=0; } discrete;
    struct { unsigned min_width=0, min_height=0; } stepwise;
};
struct v4l2_pix_format { unsigned width=0, height=0, pixelformat=0, sizeimage=0; };
struct v4l2_format { unsigned type=0; struct { v4l2_pix_format pix; } fmt; };
struct v4l2_requestbuffers { unsigned count=0, type=0, memory=0; };
struct v4l2_buffer {
    unsigned type=0, memory=0, index=0, length=0, flags=0, bytesused=0;
    struct { unsigned offset=0; } m;
};
enum : unsigned long {
    VIDIOC_QUERYCAP=1, VIDIOC_ENUM_FMT, VIDIOC_ENUM_FRAMESIZES,
    VIDIOC_S_FMT, VIDIOC_REQBUFS, VIDIOC_QUERYBUF, VIDIOC_QBUF,
    VIDIOC_S_DQBUF_TIMEOUT, VIDIOC_STREAMON, VIDIOC_DQBUF, VIDIOC_STREAMOFF
};
using esp_err_t = int;
using gpio_num_t = int;
using esp_cam_sensor_xclk_handle_t = void*;
struct esp_cam_sensor_xclk_config_t {
    struct { int xclk_pin=0, xclk_freq_hz=0; } esp_clock_router_cfg;
};
struct esp_video_init_csi_config_t {
    int reset_pin=0, pwdn_pin=0;
    struct { bool init_sccb=false; int i2c_handle=0, freq=0; } sccb_config;
};
struct esp_video_init_config_t { esp_video_init_csi_config_t* csi=nullptr; };

static std::string fault;
static int fault_n=1;
static int seen=0;
static std::vector<std::string> events;
static unsigned char frame_storage[1024];
static char log_buffer[256];
template <typename... A> void capture_log(const char* fmt, A... args) {
    std::snprintf(log_buffer, sizeof(log_buffer), fmt, args...);
}
#define ESP_LOGI(tag, fmt, ...) capture_log(fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) capture_log(fmt, ##__VA_ARGS__)
bool fail(const char* name) {
    if (fault != name) return false;
    return ++seen == fault_n;
}
void reset(const char* name="", int occurrence=1) {
    fault=name; fault_n=occurrence; seen=0; events.clear();
}
int open(const char*, int) {
    events.emplace_back("open");
    if (fail("open")) { errno=EIO; return -1; }
    return 4;
}
int close(int) {
    events.emplace_back("close");
    if (fail("close")) { errno=EIO; return -1; }
    return 0;
}
int ioctl(int, unsigned long command, void* value) {
    const char* name="unknown";
    switch (command) {
    case VIDIOC_QUERYCAP: name="querycap"; break;
    case VIDIOC_ENUM_FMT: name="enumfmt"; break;
    case VIDIOC_ENUM_FRAMESIZES: name="enumsize"; break;
    case VIDIOC_S_FMT: name="sfmt"; break;
    case VIDIOC_REQBUFS: name=static_cast<v4l2_requestbuffers*>(value)->count ? "reqbuf" : "release"; break;
    case VIDIOC_QUERYBUF: name="querybuf"; break;
    case VIDIOC_QBUF: name="qbuf"; break;
    case VIDIOC_S_DQBUF_TIMEOUT: name="timeout"; break;
    case VIDIOC_STREAMON: name="streamon"; break;
    case VIDIOC_DQBUF: name="dqbuf"; break;
    case VIDIOC_STREAMOFF: name="streamoff"; break;
    }
    events.emplace_back(name);
    if (fail(name)) { errno=EIO; return -1; }
    switch (command) {
    case VIDIOC_QUERYCAP: {
        auto* v=static_cast<v4l2_capability*>(value);
        v->capabilities=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING;
        break;
    }
    case VIDIOC_ENUM_FMT: {
        auto* v=static_cast<v4l2_fmtdesc*>(value);
        if (v->index) return -1;
        v->pixelformat=V4L2_PIX_FMT_SBGGR8;
        break;
    }
    case VIDIOC_ENUM_FRAMESIZES: {
        auto* v=static_cast<v4l2_frmsizeenum*>(value);
        if (v->index) return -1;
        v->type=V4L2_FRMSIZE_TYPE_DISCRETE;
        v->discrete.width=640; v->discrete.height=480;
        break;
    }
    case VIDIOC_S_FMT: {
        auto* v=static_cast<v4l2_format*>(value);
        v->fmt.pix.sizeimage=sizeof(frame_storage);
        break;
    }
    case VIDIOC_REQBUFS: {
        auto* v=static_cast<v4l2_requestbuffers*>(value);
        if (v->count) v->count=2;
        break;
    }
    case VIDIOC_QUERYBUF: {
        auto* v=static_cast<v4l2_buffer*>(value);
        v->length=sizeof(frame_storage); v->m.offset=v->index;
        break;
    }
    case VIDIOC_DQBUF: {
        auto* v=static_cast<v4l2_buffer*>(value);
        v->index=0; v->flags=V4L2_BUF_FLAG_DONE; v->bytesused=sizeof(frame_storage);
        break;
    }
    default: break;
    }
    return 0;
}
void* mmap(void*, size_t, int, int, int, unsigned) {
    events.emplace_back("map");
    if (fail("map_null")) { errno=EIO; return nullptr; }
    if (fail("map_failed")) { errno=EIO; return MAP_FAILED; }
    return frame_storage;
}
int munmap(void*, size_t) {
    events.emplace_back("unmap");
    if (fail("unmap")) { errno=EIO; return -1; }
    return 0;
}
int esp_cam_sensor_xclk_allocate(int, esp_cam_sensor_xclk_handle_t* h) {
    events.emplace_back("xclk_allocate");
    if (fail("xclk_allocate")) return ESP_FAIL;
    *h=frame_storage; return ESP_OK;
}
int esp_cam_sensor_xclk_start(void*, esp_cam_sensor_xclk_config_t*) {
    events.emplace_back("xclk_start"); return fail("xclk_start") ? ESP_FAIL : ESP_OK;
}
int esp_cam_sensor_xclk_stop(void*) {
    events.emplace_back("xclk_stop"); return fail("xclk_stop") ? ESP_FAIL : ESP_OK;
}
int esp_cam_sensor_xclk_free(void*) {
    events.emplace_back("xclk_free"); return fail("xclk_free") ? ESP_FAIL : ESP_OK;
}
int esp_video_init(esp_video_init_config_t*) {
    events.emplace_back("video_init"); return fail("video_init") ? ESP_FAIL : ESP_OK;
}
int esp_video_deinit() {
    events.emplace_back("video_deinit"); return fail("video_deinit") ? ESP_FAIL : ESP_OK;
}
const char* esp_err_to_name(int e) { return e == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }
void vTaskDelay(int) {}
class Claw4Board {
    int bus_=0;
public:
    void SetOutput(int, bool high) { events.emplace_back(high ? "power_off" : "power_on"); }
    bool CaptureSingleCameraFrame();
    void ProbeCameraSensor();
};
// INSERT_PRODUCTION_METHODS

int count(const char* name) {
    int n=0; for (const auto& event : events) if (event == name) ++n;
    return n;
}
void verify_capture_failure(Claw4Board& board, const char* stage,
                            int occurrence, int maps, bool requested, bool streamed) {
    reset(stage, occurrence);
    assert(!board.CaptureSingleCameraFrame());
    assert(count("close") == (std::strcmp(stage, "open") ? 1 : 0));
    assert(count("unmap") == maps);
    assert(count("release") == (requested ? 1 : 0));
    assert(count("streamoff") == (streamed ? 1 : 0));
}
int main() {
    Claw4Board board;
    reset(); assert(board.CaptureSingleCameraFrame());
    assert(count("streamoff")==1 && count("unmap")==2 &&
           count("release")==1 && count("close")==1);
    verify_capture_failure(board, "open", 1, 0, false, false);
    verify_capture_failure(board, "querycap", 1, 0, false, false);
    verify_capture_failure(board, "enumfmt", 1, 0, false, false);
    verify_capture_failure(board, "enumsize", 1, 0, false, false);
    verify_capture_failure(board, "sfmt", 1, 0, false, false);
    verify_capture_failure(board, "reqbuf", 1, 0, true, false);
    verify_capture_failure(board, "querybuf", 1, 0, true, false);
    verify_capture_failure(board, "map_null", 1, 0, true, false);
    verify_capture_failure(board, "map_failed", 1, 0, true, false);
    verify_capture_failure(board, "qbuf", 1, 1, true, false);
    verify_capture_failure(board, "querybuf", 2, 1, true, false);
    verify_capture_failure(board, "map_null", 2, 1, true, false);
    verify_capture_failure(board, "timeout", 1, 2, true, false);
    verify_capture_failure(board, "streamon", 1, 2, true, false);
    verify_capture_failure(board, "dqbuf", 1, 2, true, true);
    for (const char* stage : {"streamoff", "unmap", "release", "close"}) {
        reset(stage); assert(!board.CaptureSingleCameraFrame());
        assert(count("close")==1 && count("release")==1 && count("unmap")==2);
    }
    for (const char* stage : {"xclk_allocate", "xclk_start", "video_init",
                               "video_deinit", "xclk_stop", "xclk_free"}) {
        reset(stage); board.ProbeCameraSensor();
        assert(count("power_on")==1 && count("power_off")==1);
        assert(count("xclk_free")==int(std::strcmp(stage, "xclk_allocate") != 0));
        assert(count("xclk_stop")==
               int(std::strcmp(stage, "xclk_allocate") != 0 &&
                   std::strcmp(stage, "xclk_start") != 0));
        assert(count("video_deinit")==
               int(std::strcmp(stage, "xclk_allocate") != 0 &&
                   std::strcmp(stage, "xclk_start") != 0 &&
                   std::strcmp(stage, "video_init") != 0));
    }
    reset(); board.ProbeCameraSensor();
    assert(count("power_on")==1 && count("power_off")==1 &&
           count("video_deinit")==1 && count("xclk_stop")==1 && count("xclk_free")==1);
    std::puts("PASS: 27 production camera cleanup scenarios; no hardware claims.");
}
