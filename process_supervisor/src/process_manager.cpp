#include "process_manager.hpp"

#include <algorithm>
#include <iostream>
#include <thread>

namespace ps {

ProcessManager::~ProcessManager() {
    shutdown();
}

bool ProcessManager::add_process(ProcessSpec spec,
                                 std::shared_ptr<IRestartPolicy> policy) {
    if (spec.name.empty() || spec.argv.empty() || !policy) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (processes_.count(spec.name) != 0) {
        return false;
    }

    const std::string name = spec.name;
    auto process = std::make_shared<ManagedProcess>(
        std::move(spec),
        std::move(policy),
        [this](const ProcessEvent& event) { publish(event); },
        [this](const std::string& process_name, int code, bool signaled) {
            on_exit(process_name, code, signaled);
        });
    processes_.emplace(name, std::move(process));
    return true;
}

bool ProcessManager::remove_process(const std::string& name) {
    std::shared_ptr<ManagedProcess> process;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = processes_.find(name);
        if (it == processes_.end()) {
            return false;
        }
        process = it->second;
        processes_.erase(it);
    }
    process->request_stop(std::chrono::seconds(1));
    process->join_background_threads();
    return true;
}

std::shared_ptr<ManagedProcess> ProcessManager::find_process(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = processes_.find(name);
    return it == processes_.end() ? nullptr : it->second;
}

bool ProcessManager::start(const std::string& name) {
    auto process = find_process(name);
    return process && process->start();
}

bool ProcessManager::stop(const std::string& name, std::chrono::milliseconds grace) {
    auto process = find_process(name);
    return process && process->request_stop(grace);
}

bool ProcessManager::kill(const std::string& name) {
    auto process = find_process(name);
    return process && process->force_kill();
}

bool ProcessManager::restart(const std::string& name) {
    auto process = find_process(name);
    if (!process) {
        return false;
    }
    const auto snap = process->snapshot();
    if (snap.pid > 0) {
        process->request_stop(std::chrono::milliseconds(500));
        process->join_background_threads();
    }
    process->increment_restart_count();
    return process->start();
}

std::vector<ProcessSnapshot> ProcessManager::list() const {
    std::vector<std::shared_ptr<ManagedProcess>> copy;
    {
        std::lock_guard<std::mutex> lock(mu_);
        copy.reserve(processes_.size());
        for (const auto& [_, process] : processes_) {
            copy.push_back(process);
        }
    }

    std::vector<ProcessSnapshot> result;
    result.reserve(copy.size());
    for (const auto& process : copy) {
        result.push_back(process->snapshot());
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return result;
}

std::optional<ProcessSnapshot> ProcessManager::get(const std::string& name) const {
    auto process = find_process(name);
    if (!process) {
        return std::nullopt;
    }
    return process->snapshot();
}

void ProcessManager::add_observer(std::shared_ptr<IProcessObserver> observer) {
    if (!observer) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    observers_.push_back(std::move(observer));
}

void ProcessManager::publish(const ProcessEvent& event) {
    std::vector<std::shared_ptr<IProcessObserver>> observers;
    {
        std::lock_guard<std::mutex> lock(mu_);
        observers = observers_;
    }
    for (const auto& observer : observers) {
        observer->on_process_event(event);
    }
}

void ProcessManager::on_exit(const std::string& name,
                             int exit_code,
                             bool signaled) {
    if (shutting_down_.load(std::memory_order_relaxed)) {
        return;
    }

    auto process = find_process(name);
    if (!process) {
        return;
    }

    const auto snapshot = process->snapshot();
    if (snapshot.state == ProcessState::Stopped ||
        snapshot.state == ProcessState::Stopping) {
        return;
    }

    auto policy = process->restart_policy();
    auto decision = policy->decide(exit_code,
                                   signaled,
                                   process->restart_count());
    if (!decision.should_restart) {
        return;
    }

    schedule_restart(process, decision);
}

void ProcessManager::schedule_restart(const std::shared_ptr<ManagedProcess>& process,
                                      const RestartDecision& decision) {
    publish({ProcessEvent::Kind::RestartScheduled,
             process->spec().name,
             process->snapshot().state,
             process->snapshot().state,
             decision.reason + ", delay=" + std::to_string(decision.delay.count()) + "ms"});

    std::lock_guard<std::mutex> lock(mu_);
    restart_threads_.emplace_back([this, process, decision]() {
        std::this_thread::sleep_for(decision.delay);
        if (shutting_down_.load(std::memory_order_relaxed)) {
            return;
        }
        process->join_background_threads();
        process->increment_restart_count();
        process->start();
    });
}

void ProcessManager::shutdown() {
    bool expected = false;
    if (!shutting_down_.compare_exchange_strong(expected, true)) {
        return;
    }

    std::vector<std::shared_ptr<ManagedProcess>> processes;
    std::vector<std::thread> restart_threads;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& [_, process] : processes_) {
            processes.push_back(process);
        }
        restart_threads.swap(restart_threads_);
    }

    for (auto& process : processes) {
        process->request_stop(std::chrono::milliseconds(500));
    }
    for (auto& process : processes) {
        process->join_background_threads();
    }
    for (auto& thread : restart_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

}  // namespace ps
