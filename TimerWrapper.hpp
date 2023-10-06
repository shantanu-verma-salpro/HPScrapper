#ifndef TIMERW
#define TIMERW

#include <uv.h>
#include "HandleWrapperBase.hpp"
#include <functional>
#include "EventLoop.hpp"

class TimerWrapper final : public HandleWrapperBase {

public:
    explicit TimerWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_timer_init(loop.getLoop(), &timer_handle));
        timer_handle.data = this;
    }

    ~TimerWrapper() noexcept override {
        close();
    }

    void close(std::function<void(TimerWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&timer_handle), uv_close_callback);
        }
    }

    void start(uint64_t timeout, uint64_t repeat) noexcept{
        check_uv_error(uv_timer_start(&timer_handle, uv_timer_callback, timeout, repeat));
    }

    void stop() noexcept { check_uv_error(uv_timer_stop(&timer_handle)); }
    void again() noexcept { check_uv_error(uv_timer_again(&timer_handle));}
    void setRepeat(uint64_t repeat) noexcept { uv_timer_set_repeat(&timer_handle, repeat); }
    const uint64_t getRepeat() const noexcept { return uv_timer_get_repeat(&timer_handle); }
    const bool isActive() const noexcept override { return uv_is_active(reinterpret_cast<const uv_handle_t*>(&timer_handle)); }
    const bool isClosing() const noexcept override { return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&timer_handle)); }

private:
    uv_timer_t timer_handle{};
    std::function<void(TimerWrapper*)> close_callback{};

    static void uv_timer_callback(uv_timer_t* handle) noexcept {
        auto* self = static_cast<TimerWrapper*>(handle->data);
        TimerEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<TimerWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

#endif