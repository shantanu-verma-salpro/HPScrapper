#ifndef POLLW
#define POLLW

#include <uv.h>
#include "HandleWrapperBase.hpp"
#include <functional>
#include "EventLoop.hpp"

class PollWrapper final : public HandleWrapperBase {

public:
    explicit PollWrapper(const EventLoop& loop, int fd) noexcept {
        this->fd = fd;
        check_uv_error(uv_poll_init_socket(loop.getLoop(), &poll_handle, fd));
        poll_handle.data = this;
    }

    ~PollWrapper() noexcept override {
        close();
    }

    void close(std::function<void(PollWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&poll_handle), uv_close_callback);
        }
    }

    const auto& getFd() const noexcept{
        return fd;
    }

    void start(int events) noexcept {
        check_uv_error(uv_poll_start(&poll_handle, events, uv_poll_callback));
    }

    void stop() noexcept {
        check_uv_error(uv_poll_stop(&poll_handle));
    }

    const bool isActive() const noexcept override {
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(&poll_handle));
    }

    const bool isClosing() const noexcept override {
        return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&poll_handle));
    }

private:
    curl_socket_t fd {};
    uv_poll_t poll_handle{};
    std::function<void(PollWrapper*)> close_callback{};

    static void uv_poll_callback(uv_poll_t* handle, int status, int events) noexcept {
        auto* self = static_cast<PollWrapper*>(handle->data);
        PollEvent event;
        event.status = status;
        event.events = events;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<PollWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

#endif