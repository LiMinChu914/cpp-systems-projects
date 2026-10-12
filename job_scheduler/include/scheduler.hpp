#pragma once

#include "cancellation.hpp"
#include "observer.hpp"
#include "scheduling_strategy.hpp"
#include "task.hpp"
#include "types.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace js {

class JobScheduler {
public:
    JobScheduler(std::size_t worker_count,
                 std::shared_ptr<ISchedulingStrategy> strategy,
                 bool pin_workers = false);
    ~JobScheduler();

    JobScheduler(const JobScheduler&) = delete;
    JobScheduler& operator=(const JobScheduler&) = delete;

    void start();
    void pause();
    void resume();
    void shutdown(bool drain = true);

    std::uint64_t submit(std::shared_ptr<ITask> task, int priority = 0);
    bool cancel(std::uint64_t id);
    bool set_strategy(std::shared_ptr<ISchedulingStrategy> strategy);

    std::vector<TaskInfo> snapshot_tasks() const;
    SchedulerState state() const;
    std::string strategy_name() const;

    void add_observer(std::shared_ptr<ISchedulerObserver> observer);

private:
    struct ScheduledItem {
        std::uint64_t id{0};
        int priority{0};
        std::uint64_t sequence{0};
        std::shared_ptr<ITask> task;
        std::shared_ptr<CancellationSource> cancel_source;
    };

    void worker_loop(std::size_t worker_index);
    std::optional<ScheduledItem> take_next_locked();
    std::deque<ScheduledItemView> queue_view_locked() const;
    void publish(const SchedulerEvent& event);
    void transition_locked(SchedulerState next);
    void set_task_status_locked(std::uint64_t id,
                                TaskStatus status,
                                std::size_t worker_index = static_cast<std::size_t>(-1));
    void pin_current_thread(std::size_t worker_index);

    const std::size_t worker_count_;
    const bool pin_workers_;

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<ScheduledItem> queue_;
    std::unordered_map<std::uint64_t, TaskInfo> task_info_;
    std::unordered_map<std::uint64_t, std::shared_ptr<CancellationSource>> cancellation_;
    std::vector<std::shared_ptr<ISchedulerObserver>> observers_;
    std::vector<std::thread> workers_;
    std::shared_ptr<ISchedulingStrategy> strategy_;

    SchedulerState state_{SchedulerState::Created};
    bool drain_on_shutdown_{true};
    std::uint64_t next_id_{1};
    std::uint64_t next_sequence_{1};
};

}  // namespace js
