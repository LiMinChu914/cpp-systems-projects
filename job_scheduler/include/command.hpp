#pragma once

#include "scheduler.hpp"

#include <memory>
#include <string>
#include <vector>

namespace js {

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(JobScheduler& scheduler) = 0;
};

class SubmitSleepCommand final : public ICommand {
public:
    SubmitSleepCommand(std::string name, int ms, int priority)
        : name_(std::move(name)), ms_(ms), priority_(priority) {}
    void execute(JobScheduler& scheduler) override;
private:
    std::string name_;
    int ms_{0};
    int priority_{0};
};

class SubmitPrimeCommand final : public ICommand {
public:
    SubmitPrimeCommand(std::string name, std::uint64_t limit, int priority)
        : name_(std::move(name)), limit_(limit), priority_(priority) {}
    void execute(JobScheduler& scheduler) override;
private:
    std::string name_;
    std::uint64_t limit_{0};
    int priority_{0};
};

class SubmitChecksumCommand final : public ICommand {
public:
    SubmitChecksumCommand(std::string name, std::string path, int priority)
        : name_(std::move(name)), path_(std::move(path)), priority_(priority) {}
    void execute(JobScheduler& scheduler) override;
private:
    std::string name_;
    std::string path_;
    int priority_{0};
};

class CancelCommand final : public ICommand {
public:
    explicit CancelCommand(std::uint64_t id) : id_(id) {}
    void execute(JobScheduler& scheduler) override;
private:
    std::uint64_t id_{0};
};

class ListCommand final : public ICommand {
public:
    void execute(JobScheduler& scheduler) override;
};

class PauseCommand final : public ICommand {
public:
    void execute(JobScheduler& scheduler) override { scheduler.pause(); }
};

class ResumeCommand final : public ICommand {
public:
    void execute(JobScheduler& scheduler) override { scheduler.resume(); }
};

class StrategyCommand final : public ICommand {
public:
    explicit StrategyCommand(std::string name) : name_(std::move(name)) {}
    void execute(JobScheduler& scheduler) override;
private:
    std::string name_;
};

class HelpCommand final : public ICommand {
public:
    void execute(JobScheduler& scheduler) override;
};

class QuitCommand final : public ICommand {
public:
    explicit QuitCommand(bool& quit) : quit_(quit) {}
    void execute(JobScheduler&) override { quit_ = true; }
private:
    bool& quit_;
};

class CommandFactory {
public:
    static std::unique_ptr<ICommand> create(const std::vector<std::string>& tokens,
                                            bool& quit);
};

std::vector<std::string> tokenize(const std::string& line);

}  // namespace js
