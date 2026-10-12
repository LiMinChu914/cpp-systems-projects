#pragma once

#include "types.hpp"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace js {

// Observer Pattern: scheduler emits events to independent logging/metrics sinks.
class ISchedulerObserver {
public:
    virtual ~ISchedulerObserver() = default;
    virtual void on_event(const SchedulerEvent& event) = 0;
};

class ConsoleObserver final : public ISchedulerObserver {
public:
    void on_event(const SchedulerEvent& event) override {
        std::lock_guard<std::mutex> lock(mu_);
        switch (event.kind) {
            case SchedulerEvent::Kind::TaskSubmitted:
                std::cout << "[submit] id=" << event.task.id
                          << " name=" << event.task.name
                          << " prio=" << event.task.priority << '\n';
                break;
            case SchedulerEvent::Kind::TaskStarted:
                std::cout << "[start] id=" << event.task.id
                          << " worker=" << event.task.worker_index << '\n';
                break;
            case SchedulerEvent::Kind::TaskCompleted:
                std::cout << "[done] id=" << event.task.id << '\n';
                break;
            case SchedulerEvent::Kind::TaskFailed:
                std::cout << "[fail] id=" << event.task.id
                          << " reason=" << event.message << '\n';
                break;
            case SchedulerEvent::Kind::TaskCancelled:
                std::cout << "[cancel] id=" << event.task.id << '\n';
                break;
            case SchedulerEvent::Kind::SchedulerStateChanged:
                std::cout << "[scheduler] " << to_string(event.old_state)
                          << " -> " << to_string(event.new_state) << '\n';
                break;
        }
    }
private:
    std::mutex mu_;
};

class MetricsObserver final : public ISchedulerObserver {
public:
    void on_event(const SchedulerEvent& event) override {
        switch (event.kind) {
            case SchedulerEvent::Kind::TaskSubmitted:
                submitted_.fetch_add(1, std::memory_order_relaxed);
                break;
            case SchedulerEvent::Kind::TaskCompleted:
                completed_.fetch_add(1, std::memory_order_relaxed);
                break;
            case SchedulerEvent::Kind::TaskFailed:
                failed_.fetch_add(1, std::memory_order_relaxed);
                break;
            case SchedulerEvent::Kind::TaskCancelled:
                cancelled_.fetch_add(1, std::memory_order_relaxed);
                break;
            case SchedulerEvent::Kind::TaskStarted:
            case SchedulerEvent::Kind::SchedulerStateChanged:
                break;
        }
    }

    void print() const {
        std::cout << "metrics submitted=" << submitted_.load()
                  << " completed=" << completed_.load()
                  << " failed=" << failed_.load()
                  << " cancelled=" << cancelled_.load() << '\n';
    }

private:
    std::atomic<std::uint64_t> submitted_{0};
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> cancelled_{0};
};

}  // namespace js
