#include "scheduler.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace js {

JobScheduler::JobScheduler(std::size_t worker_count,
                           std::shared_ptr<ISchedulingStrategy> strategy,
                           bool pin_workers)
    : worker_count_(worker_count),
      pin_workers_(pin_workers),
      strategy_(std::move(strategy)) {
    if (worker_count_ == 0) {
        throw std::invalid_argument("worker_count must be positive");
    }
    if (!strategy_) {
        throw std::invalid_argument("strategy must not be null");
    }
}

JobScheduler::~JobScheduler() {
    shutdown(true);
}

void JobScheduler::start() {
    SchedulerEvent state_event;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ != SchedulerState::Created) {
            return;
        }
        state_event.kind = SchedulerEvent::Kind::SchedulerStateChanged;
        state_event.old_state = state_;
        state_event.new_state = SchedulerState::Running;
        state_ = SchedulerState::Running;

        workers_.reserve(worker_count_);
        for (std::size_t i = 0; i < worker_count_; ++i) {
            workers_.emplace_back(&JobScheduler::worker_loop, this, i);
        }
    }
    publish(state_event);
}

void JobScheduler::pause() {
    SchedulerEvent event;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == SchedulerState::Running) {
            event.kind = SchedulerEvent::Kind::SchedulerStateChanged;
            event.old_state = state_;
            state_ = SchedulerState::Paused;
            event.new_state = state_;
            changed = true;
        }
    }
    if (changed) publish(event);
}

void JobScheduler::resume() {
    SchedulerEvent event;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == SchedulerState::Paused) {
            event.kind = SchedulerEvent::Kind::SchedulerStateChanged;
            event.old_state = state_;
            state_ = SchedulerState::Running;
            event.new_state = state_;
            changed = true;
        }
    }
    if (changed) {
        publish(event);
        cv_.notify_all();
    }
}

void JobScheduler::shutdown(bool drain) {
    std::vector<std::thread> workers;
    SchedulerEvent stopping_event;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == SchedulerState::Stopped ||
            state_ == SchedulerState::Stopping ||
            state_ == SchedulerState::Created) {
            if (state_ == SchedulerState::Created) {
                state_ = SchedulerState::Stopped;
            }
            return;
        }

        drain_on_shutdown_ = drain;
        stopping_event.kind = SchedulerEvent::Kind::SchedulerStateChanged;
        stopping_event.old_state = state_;
        state_ = SchedulerState::Stopping;
        stopping_event.new_state = state_;
        changed = true;

        if (!drain) {
            for (auto& item : queue_) {
                item.cancel_source->cancel();
                set_task_status_locked(item.id, TaskStatus::Cancelled);
            }
            queue_.clear();
            for (auto& [_, source] : cancellation_) {
                source->cancel();
            }
        }
        workers.swap(workers_);
    }

    if (changed) publish(stopping_event);
    cv_.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) worker.join();
    }

    SchedulerEvent stopped_event;
    {
        std::lock_guard<std::mutex> lock(mu_);
        stopped_event.kind = SchedulerEvent::Kind::SchedulerStateChanged;
        stopped_event.old_state = state_;
        state_ = SchedulerState::Stopped;
        stopped_event.new_state = state_;
    }
    publish(stopped_event);
}

std::uint64_t JobScheduler::submit(std::shared_ptr<ITask> task, int priority) {
    if (!task) {
        throw std::invalid_argument("task must not be null");
    }

    SchedulerEvent event;
    std::uint64_t id = 0;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == SchedulerState::Stopping ||
            state_ == SchedulerState::Stopped) {
            throw std::runtime_error("scheduler is stopping/stopped");
        }

        id = next_id_++;
        auto cancel_source = std::make_shared<CancellationSource>();
        queue_.push_back({id,
                          priority,
                          next_sequence_++,
                          std::move(task),
                          cancel_source});
        task_info_[id] = {id,
                          queue_.back().task->name(),
                          priority,
                          TaskStatus::Queued,
                          static_cast<std::size_t>(-1)};
        cancellation_[id] = std::move(cancel_source);

        event.kind = SchedulerEvent::Kind::TaskSubmitted;
        event.task = task_info_[id];
    }
    publish(event);
    cv_.notify_one();
    return id;
}

bool JobScheduler::cancel(std::uint64_t id) {
    SchedulerEvent event;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto source_it = cancellation_.find(id);
        auto info_it = task_info_.find(id);
        if (source_it == cancellation_.end() || info_it == task_info_.end()) {
            return false;
        }

        source_it->second->cancel();
        if (info_it->second.status == TaskStatus::Queued) {
            auto qit = std::find_if(queue_.begin(), queue_.end(),
                                    [id](const ScheduledItem& item) {
                                        return item.id == id;
                                    });
            if (qit != queue_.end()) {
                queue_.erase(qit);
            }
            info_it->second.status = TaskStatus::Cancelled;
            event.kind = SchedulerEvent::Kind::TaskCancelled;
            event.task = info_it->second;
            found = true;
        } else if (info_it->second.status == TaskStatus::Running) {
            // Cooperative cancellation. The task decides how quickly to stop.
            found = true;
        } else {
            return false;
        }
    }
    if (event.kind == SchedulerEvent::Kind::TaskCancelled) {
        publish(event);
    }
    return found;
}

bool JobScheduler::set_strategy(std::shared_ptr<ISchedulingStrategy> strategy) {
    if (!strategy) return false;
    std::lock_guard<std::mutex> lock(mu_);
    strategy_ = std::move(strategy);
    cv_.notify_all();
    return true;
}

std::vector<TaskInfo> JobScheduler::snapshot_tasks() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<TaskInfo> result;
    result.reserve(task_info_.size());
    for (const auto& [_, info] : task_info_) {
        result.push_back(info);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });
    return result;
}

SchedulerState JobScheduler::state() const {
    std::lock_guard<std::mutex> lock(mu_);
    return state_;
}

std::string JobScheduler::strategy_name() const {
    std::lock_guard<std::mutex> lock(mu_);
    return strategy_->name();
}

void JobScheduler::add_observer(std::shared_ptr<ISchedulerObserver> observer) {
    if (!observer) return;
    std::lock_guard<std::mutex> lock(mu_);
    observers_.push_back(std::move(observer));
}

std::deque<ScheduledItemView> JobScheduler::queue_view_locked() const {
    std::deque<ScheduledItemView> view;
    for (const auto& item : queue_) {
        view.push_back({item.id, item.priority, item.sequence});
    }
    return view;
}

std::optional<JobScheduler::ScheduledItem> JobScheduler::take_next_locked() {
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto view = queue_view_locked();
    const std::size_t index = strategy_->choose(view);
    if (index >= queue_.size()) {
        throw std::logic_error("scheduling strategy returned invalid index");
    }
    auto iterator = queue_.begin() + static_cast<std::ptrdiff_t>(index);
    ScheduledItem item = std::move(*iterator);
    queue_.erase(iterator);
    return item;
}

void JobScheduler::set_task_status_locked(std::uint64_t id,
                                          TaskStatus status,
                                          std::size_t worker_index) {
    auto it = task_info_.find(id);
    if (it == task_info_.end()) return;
    it->second.status = status;
    it->second.worker_index = worker_index;
}

void JobScheduler::worker_loop(std::size_t worker_index) {
    pin_current_thread(worker_index);

    while (true) {
        std::optional<ScheduledItem> item;
        TaskInfo started_info;

        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [&]() {
                if (state_ == SchedulerState::Stopping) {
                    return !drain_on_shutdown_ || !queue_.empty() || queue_.empty();
                }
                return state_ == SchedulerState::Running && !queue_.empty();
            });

            if (state_ == SchedulerState::Stopping) {
                if (!drain_on_shutdown_ || queue_.empty()) {
                    return;
                }
            }
            if (state_ != SchedulerState::Running &&
                state_ != SchedulerState::Stopping) {
                continue;
            }
            if (queue_.empty()) {
                continue;
            }

            item = take_next_locked();
            if (!item.has_value()) continue;
            set_task_status_locked(item->id, TaskStatus::Running, worker_index);
            started_info = task_info_.at(item->id);
        }

        publish({SchedulerEvent::Kind::TaskStarted,
                 started_info,
                 SchedulerState::Running,
                 SchedulerState::Running,
                 {}});

        SchedulerEvent finish_event;
        finish_event.task = started_info;
        try {
            item->task->execute({worker_index, item->cancel_source->token()});
            const bool cancelled = item->cancel_source->token().cancelled();
            {
                std::lock_guard<std::mutex> lock(mu_);
                set_task_status_locked(item->id,
                                       cancelled ? TaskStatus::Cancelled
                                                 : TaskStatus::Completed,
                                       worker_index);
                finish_event.task = task_info_.at(item->id);
                cancellation_.erase(item->id);
            }
            finish_event.kind = cancelled
                                    ? SchedulerEvent::Kind::TaskCancelled
                                    : SchedulerEvent::Kind::TaskCompleted;
        } catch (const std::exception& error) {
            {
                std::lock_guard<std::mutex> lock(mu_);
                set_task_status_locked(item->id, TaskStatus::Failed, worker_index);
                finish_event.task = task_info_.at(item->id);
                cancellation_.erase(item->id);
            }
            finish_event.kind = SchedulerEvent::Kind::TaskFailed;
            finish_event.message = error.what();
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(mu_);
                set_task_status_locked(item->id, TaskStatus::Failed, worker_index);
                finish_event.task = task_info_.at(item->id);
                cancellation_.erase(item->id);
            }
            finish_event.kind = SchedulerEvent::Kind::TaskFailed;
            finish_event.message = "unknown exception";
        }
        publish(finish_event);
    }
}

void JobScheduler::publish(const SchedulerEvent& event) {
    std::vector<std::shared_ptr<ISchedulerObserver>> observers;
    {
        std::lock_guard<std::mutex> lock(mu_);
        observers = observers_;
    }
    for (const auto& observer : observers) {
        observer->on_event(event);
    }
}

void JobScheduler::transition_locked(SchedulerState next) {
    state_ = next;
}

void JobScheduler::pin_current_thread(std::size_t worker_index) {
#ifdef __linux__
    if (!pin_workers_) return;
    const unsigned cpu_count = std::thread::hardware_concurrency();
    if (cpu_count == 0) return;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(static_cast<int>(worker_index % cpu_count), &set);
    const int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) {
        std::cerr << "[warn] pthread_setaffinity_np failed: "
                  << std::strerror(rc) << '\n';
    }
#else
    (void)worker_index;
#endif
}

}  // namespace js
