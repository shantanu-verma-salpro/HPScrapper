#ifndef IDLEW
#define IDLEW

#include <uv.h>
#include "HandleWrapperBase.hpp"
#include <functional>
#include "EventLoop.hpp"

class IdleWrapper final : public HandleWrapperBase {

public:
    explicit IdleWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_idle_init(loop.getLoop(), &idle_handle));
        idle_handle.data = this;
    }

    ~IdleWrapper() noexcept override {
        close();
    }

    void close(std::function<void(IdleWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&idle_handle), uv_close_callback);
        }
    }

    void start() noexcept  { check_uv_error(uv_idle_start(&idle_handle, uv_idle_callback)); }
    void stop() noexcept { check_uv_error(uv_idle_stop(&idle_handle)); }
    const bool isActive() const noexcept override { return uv_is_active(reinterpret_cast<const uv_handle_t*>(&idle_handle)); }
    const bool isClosing() const noexcept override { return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&idle_handle)); }

private:
    uv_idle_t idle_handle{};
    std::function<void(IdleWrapper*)> close_callback{};

    static void uv_idle_callback(uv_idle_t* handle) noexcept {
        auto* self = static_cast<IdleWrapper*>(handle->data);
        IdleEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<IdleWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

#endif