#pragma once

#include "observer.hpp"
#include "process.hpp"
#include "restart_policy.hpp"
#include "types.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ps {

class ProcessManager {
public:
    ProcessManager() = default;
    ~ProcessManager();

    bool add_process(ProcessSpec spec, std::shared_ptr<IRestartPolicy> policy);
    bool remove_process(const std::string& name);

    bool start(const std::string& name);
    bool stop(const std::string& name,
              std::chrono::milliseconds grace = std::chrono::seconds(2));
    bool kill(const std::string& name);
    bool restart(const std::string& name);

    std::vector<ProcessSnapshot> list() const;
    std::optional<ProcessSnapshot> get(const std::string& name) const;

    void add_observer(std::shared_ptr<IProcessObserver> observer);
    void shutdown();

private:
    std::shared_ptr<ManagedProcess> find_process(const std::string& name) const;
    void publish(const ProcessEvent& event);
    void on_exit(const std::string& name, int exit_code, bool signaled);
    void schedule_restart(const std::shared_ptr<ManagedProcess>& process,
                          const RestartDecision& decision);

    mutable std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<ManagedProcess>> processes_;
    std::vector<std::shared_ptr<IProcessObserver>> observers_;
    std::vector<std::thread> restart_threads_;
    std::atomic<bool> shutting_down_{false};
};

}  // namespace ps
