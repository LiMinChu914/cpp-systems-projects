#pragma once

#include <atomic>
#include <memory>

namespace js {

class CancellationState {
public:
    void cancel() { cancelled_.store(true, std::memory_order_release); }
    bool cancelled() const { return cancelled_.load(std::memory_order_acquire); }
private:
    std::atomic<bool> cancelled_{false};
};

class CancellationToken {
public:
    CancellationToken() = default;
    explicit CancellationToken(std::shared_ptr<CancellationState> state)
        : state_(std::move(state)) {}
    bool cancelled() const { return state_ && state_->cancelled(); }
private:
    std::shared_ptr<CancellationState> state_;
};

class CancellationSource {
public:
    CancellationSource() : state_(std::make_shared<CancellationState>()) {}
    CancellationToken token() const { return CancellationToken(state_); }
    void cancel() { state_->cancel(); }
private:
    std::shared_ptr<CancellationState> state_;
};

}  // namespace js
