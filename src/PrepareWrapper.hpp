#ifndef PREPAREW
#define PREPAREW

#include <uv.h>
#include "HandleWrapperBase.hpp"
#include <functional>
#include "EventLoop.hpp"

class PrepareWrapper final : public HandleWrapperBase {

public:
    explicit PrepareWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_prepare_init(loop.getLoop(), &prepare_handle));
        prepare_handle.data = this;
    }

    ~PrepareWrapper() noexcept override {
        close();
    }

    void close(std::function<void(PrepareWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&prepare_handle), uv_close_callback);
        }
    }

    void start() noexcept {
        check_uv_error(uv_prepare_start(&prepare_handle, uv_prepare_callback));
    }

    void stop() noexcept {
        check_uv_error(uv_prepare_stop(&prepare_handle));
    }

    const bool isActive() const noexcept override {
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(&prepare_handle));
    }

    const bool isClosing() const noexcept override {
        return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&prepare_handle));
    }

private:
    uv_prepare_t prepare_handle{};
    std::function<void(PrepareWrapper*)> close_callback{};

    static void uv_prepare_callback(uv_prepare_t* handle) noexcept {
        auto* self = static_cast<PrepareWrapper*>(handle->data);
        PrepareEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<PrepareWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

#endif