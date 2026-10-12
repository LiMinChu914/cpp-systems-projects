#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace ps {

struct RestartDecision {
    bool should_restart{false};
    std::chrono::milliseconds delay{0};
    std::string reason;
};

// Strategy Pattern: restart behavior varies independently from process execution.
class IRestartPolicy {
public:
    virtual ~IRestartPolicy() = default;
    virtual RestartDecision decide(int exit_code,
                                   bool signaled,
                                   std::uint64_t restart_count) const = 0;
    virtual std::string name() const = 0;
};

class NeverRestart final : public IRestartPolicy {
public:
    RestartDecision decide(int, bool, std::uint64_t) const override {
        return {false, std::chrono::milliseconds(0), "policy=never"};
    }
    std::string name() const override { return "never"; }
};

class AlwaysRestart final : public IRestartPolicy {
public:
    explicit AlwaysRestart(std::chrono::milliseconds delay)
        : delay_(delay) {}

    RestartDecision decide(int, bool, std::uint64_t) const override {
        return {true, delay_, "policy=always"};
    }
    std::string name() const override { return "always"; }

private:
    std::chrono::milliseconds delay_;
};

class OnFailureBackoff final : public IRestartPolicy {
public:
    OnFailureBackoff(std::uint64_t max_restarts,
                     std::chrono::milliseconds base_delay,
                     std::chrono::milliseconds max_delay)
        : max_restarts_(max_restarts),
          base_delay_(base_delay),
          max_delay_(max_delay) {}

    RestartDecision decide(int exit_code,
                           bool signaled,
                           std::uint64_t restart_count) const override {
        const bool failed = signaled || exit_code != 0;
        if (!failed) {
            return {false, std::chrono::milliseconds(0), "clean exit"};
        }
        if (restart_count >= max_restarts_) {
            return {false, std::chrono::milliseconds(0), "restart limit reached"};
        }
        std::uint64_t multiplier = 1;
        for (std::uint64_t i = 0; i < restart_count && multiplier < 64; ++i) {
            multiplier *= 2;
        }
        auto delay = base_delay_ * static_cast<int>(multiplier);
        delay = std::min(delay, max_delay_);
        return {true, delay, "failure with exponential backoff"};
    }

    std::string name() const override { return "on-failure"; }

private:
    std::uint64_t max_restarts_;
    std::chrono::milliseconds base_delay_;
    std::chrono::milliseconds max_delay_;
};

// Factory Pattern: textual configuration is isolated from concrete policy types.
class RestartPolicyFactory {
public:
    static std::shared_ptr<IRestartPolicy> create(const std::string& policy) {
        if (policy == "never") {
            return std::make_shared<NeverRestart>();
        }
        if (policy == "always") {
            return std::make_shared<AlwaysRestart>(std::chrono::milliseconds(500));
        }
        if (policy == "on-failure") {
            return std::make_shared<OnFailureBackoff>(5,
                                                      std::chrono::milliseconds(250),
                                                      std::chrono::seconds(4));
        }
        throw std::invalid_argument("unknown restart policy: " + policy);
    }
};

}  // namespace ps
