#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>

namespace js {

struct ScheduledItemView {
    std::uint64_t id{0};
    int priority{0};
    std::uint64_t sequence{0};
};

// Strategy Pattern: queue selection policy is independent of worker logic.
class ISchedulingStrategy {
public:
    virtual ~ISchedulingStrategy() = default;
    virtual std::size_t choose(const std::deque<ScheduledItemView>& queue) = 0;
    virtual std::string name() const = 0;
};

class FifoScheduling final : public ISchedulingStrategy {
public:
    std::size_t choose(const std::deque<ScheduledItemView>& queue) override {
        if (queue.empty()) throw std::logic_error("empty queue");
        return 0;
    }
    std::string name() const override { return "fifo"; }
};

class PriorityScheduling final : public ISchedulingStrategy {
public:
    std::size_t choose(const std::deque<ScheduledItemView>& queue) override {
        if (queue.empty()) throw std::logic_error("empty queue");
        std::size_t best = 0;
        for (std::size_t i = 1; i < queue.size(); ++i) {
            if (queue[i].priority > queue[best].priority ||
                (queue[i].priority == queue[best].priority &&
                 queue[i].sequence < queue[best].sequence)) {
                best = i;
            }
        }
        return best;
    }
    std::string name() const override { return "priority"; }
};

class FairPriorityScheduling final : public ISchedulingStrategy {
public:
    explicit FairPriorityScheduling(std::uint64_t aging_every)
        : aging_every_(aging_every == 0 ? 1 : aging_every) {}

    std::size_t choose(const std::deque<ScheduledItemView>& queue) override {
        if (queue.empty()) throw std::logic_error("empty queue");
        ++ticks_;
        std::size_t best = 0;
        long long best_score = score(queue[0]);
        for (std::size_t i = 1; i < queue.size(); ++i) {
            long long candidate = score(queue[i]);
            if (candidate > best_score) {
                best = i;
                best_score = candidate;
            }
        }
        return best;
    }

    std::string name() const override { return "fair-priority"; }

private:
    long long score(const ScheduledItemView& item) const {
        const std::uint64_t waited = ticks_ > item.sequence ? ticks_ - item.sequence : 0;
        const long long aging = static_cast<long long>(waited / aging_every_);
        return static_cast<long long>(item.priority) * 1000 + aging;
    }

    std::uint64_t aging_every_{5};
    std::uint64_t ticks_{0};
};

class SchedulingStrategyFactory {
public:
    static std::shared_ptr<ISchedulingStrategy> create(const std::string& name) {
        if (name == "fifo") return std::make_shared<FifoScheduling>();
        if (name == "priority") return std::make_shared<PriorityScheduling>();
        if (name == "fair-priority") return std::make_shared<FairPriorityScheduling>(3);
        throw std::invalid_argument("unknown scheduling strategy: " + name);
    }
};

}  // namespace js
