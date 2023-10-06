#ifndef CHECKW
#define CHECKW

#include <uv.h>
#include "HandleWrapperBase.hpp"
#include <functional>
#include "EventLoop.hpp"

class CheckWrapper final : public HandleWrapperBase {

public:
    explicit CheckWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_check_init(loop.getLoop(), &check_handle));
        check_handle.data = this;
    }

    ~CheckWrapper() noexcept override {
        close();
    }

    void close(std::function<void(CheckWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&check_handle), uv_close_callback);
        }
    }

    void start() noexcept  {
        check_uv_error(uv_check_start(&check_handle, uv_check_callback));
    }

    void stop() noexcept {
        check_uv_error(uv_check_stop(&check_handle));
    }

    const bool isActive() const noexcept override {
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(&check_handle));
    }

    const bool isClosing() const noexcept override {
        return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&check_handle));
    }

private:
    uv_check_t check_handle{};
    std::function<void(CheckWrapper*)> close_callback{};

    static void uv_check_callback(uv_check_t* handle) noexcept {
        auto* self = static_cast<CheckWrapper*>(handle->data);
        CheckEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<CheckWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

#endif