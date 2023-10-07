#ifndef EVENTL
#define EVENTL

#include <uv.h>

class EventLoop {
    private:
    uv_loop_t* loop;
public:
    EventLoop() noexcept {
        loop = uv_default_loop();
    }

    explicit EventLoop(uv_loop_t* custom_loop) noexcept : loop(custom_loop) {}

    ~EventLoop() noexcept {
        if (int result = uv_loop_close(loop); result == UV_EBUSY) {
            uv_walk(loop, on_uv_walk, nullptr);
            uv_loop_close(loop);
        }
    }

    inline uv_loop_t* getLoop() const noexcept {
        return loop;
    }

    inline void run(uv_run_mode mode = UV_RUN_DEFAULT) noexcept {
        uv_run(loop, mode);
    }

    template<typename... Args>
    int configure(uv_loop_option option, Args... args) {
        return uv_loop_configure(loop, option, args...);
    }

    inline void stop() noexcept {
        uv_stop(loop);
    }

    inline void update_time() noexcept {
        uv_update_time(loop);
    }

    inline const uint64_t now() const noexcept {
        return uv_now(loop);
    }

    inline int backend_fd() const noexcept {
        return uv_backend_fd(loop);
    }

    inline int backend_timeout() const noexcept {
        return uv_backend_timeout(loop);
    }

    inline void* data() const noexcept {
        return uv_loop_get_data(loop);
    }

    inline void set_data(void* data) noexcept {
        uv_loop_set_data(loop, data);
    }

private:

    static inline void on_uv_walk(uv_handle_t* handle, void* arg) noexcept {
        uv_close(handle, on_uv_close);
    }

    static inline void on_uv_close(uv_handle_t* handle) noexcept {
        // if(handle) delete handle;
    }
};

#endif