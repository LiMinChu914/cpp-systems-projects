#pragma once

#include "process_manager.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ps {

// Command Pattern: every CLI action is an object with a uniform execute() API.
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual bool execute(ProcessManager& manager) = 0;
};

class StartCommand final : public ICommand {
public:
    explicit StartCommand(std::string name) : name_(std::move(name)) {}
    bool execute(ProcessManager& manager) override;
private:
    std::string name_;
};

class StopCommand final : public ICommand {
public:
    explicit StopCommand(std::string name) : name_(std::move(name)) {}
    bool execute(ProcessManager& manager) override;
private:
    std::string name_;
};

class KillCommand final : public ICommand {
public:
    explicit KillCommand(std::string name) : name_(std::move(name)) {}
    bool execute(ProcessManager& manager) override;
private:
    std::string name_;
};

class RestartCommand final : public ICommand {
public:
    explicit RestartCommand(std::string name) : name_(std::move(name)) {}
    bool execute(ProcessManager& manager) override;
private:
    std::string name_;
};

class ListCommand final : public ICommand {
public:
    bool execute(ProcessManager& manager) override;
};

class StatusCommand final : public ICommand {
public:
    explicit StatusCommand(std::string name) : name_(std::move(name)) {}
    bool execute(ProcessManager& manager) override;
private:
    std::string name_;
};

class AddCommand final : public ICommand {
public:
    AddCommand(std::string name,
               std::string policy,
               std::vector<std::string> argv)
        : name_(std::move(name)),
          policy_(std::move(policy)),
          argv_(std::move(argv)) {}
    bool execute(ProcessManager& manager) override;
private:
    std::string name_;
    std::string policy_;
    std::vector<std::string> argv_;
};

class HelpCommand final : public ICommand {
public:
    bool execute(ProcessManager& manager) override;
};

class QuitCommand final : public ICommand {
public:
    explicit QuitCommand(bool& quit_flag) : quit_flag_(quit_flag) {}
    bool execute(ProcessManager& manager) override;
private:
    bool& quit_flag_;
};

class CommandFactory {
public:
    static std::unique_ptr<ICommand> create(const std::vector<std::string>& tokens,
                                            bool& quit_flag);
};

}  // namespace ps
