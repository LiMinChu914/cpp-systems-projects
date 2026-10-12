#include "command.hpp"
#include "restart_policy.hpp"

#include <iomanip>
#include <iostream>

namespace ps {

bool StartCommand::execute(ProcessManager& manager) {
    if (!manager.start(name_)) {
        std::cerr << "cannot start " << name_ << '\n';
        return false;
    }
    return true;
}

bool StopCommand::execute(ProcessManager& manager) {
    if (!manager.stop(name_)) {
        std::cerr << "cannot stop " << name_ << '\n';
        return false;
    }
    return true;
}

bool KillCommand::execute(ProcessManager& manager) {
    if (!manager.kill(name_)) {
        std::cerr << "cannot kill " << name_ << '\n';
        return false;
    }
    return true;
}

bool RestartCommand::execute(ProcessManager& manager) {
    if (!manager.restart(name_)) {
        std::cerr << "cannot restart " << name_ << '\n';
        return false;
    }
    return true;
}

bool ListCommand::execute(ProcessManager& manager) {
    auto entries = manager.list();
    std::cout << std::left
              << std::setw(18) << "NAME"
              << std::setw(12) << "STATE"
              << std::setw(10) << "PID"
              << std::setw(10) << "EXIT"
              << "RESTARTS\n";
    for (const auto& entry : entries) {
        std::cout << std::left
                  << std::setw(18) << entry.name
                  << std::setw(12) << to_string(entry.state)
                  << std::setw(10) << entry.pid
                  << std::setw(10) << entry.exit_code
                  << entry.restart_count << '\n';
    }
    return true;
}

bool StatusCommand::execute(ProcessManager& manager) {
    auto entry = manager.get(name_);
    if (!entry.has_value()) {
        std::cerr << "unknown process: " << name_ << '\n';
        return false;
    }
    std::cout << "name=" << entry->name
              << " state=" << to_string(entry->state)
              << " pid=" << entry->pid
              << " exit=" << entry->exit_code
              << " restarts=" << entry->restart_count << '\n';
    return true;
}

bool AddCommand::execute(ProcessManager& manager) {
    try {
        auto restart_policy = RestartPolicyFactory::create(policy_);
        ProcessSpec spec;
        spec.name = name_;
        spec.argv = argv_;
        if (!manager.add_process(std::move(spec), std::move(restart_policy))) {
            std::cerr << "cannot add process " << name_ << '\n';
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return false;
    }
}

bool HelpCommand::execute(ProcessManager&) {
    std::cout
        << "commands:\n"
        << "  add <name> <never|always|on-failure> <program> [args...]\n"
        << "  start <name>\n"
        << "  stop <name>\n"
        << "  kill <name>\n"
        << "  restart <name>\n"
        << "  status <name>\n"
        << "  list\n"
        << "  help\n"
        << "  quit\n";
    return true;
}

bool QuitCommand::execute(ProcessManager&) {
    quit_flag_ = true;
    return true;
}

std::unique_ptr<ICommand> CommandFactory::create(const std::vector<std::string>& tokens,
                                                  bool& quit_flag) {
    if (tokens.empty()) {
        return nullptr;
    }
    const auto& op = tokens[0];
    if (op == "start" && tokens.size() == 2) {
        return std::make_unique<StartCommand>(tokens[1]);
    }
    if (op == "stop" && tokens.size() == 2) {
        return std::make_unique<StopCommand>(tokens[1]);
    }
    if (op == "kill" && tokens.size() == 2) {
        return std::make_unique<KillCommand>(tokens[1]);
    }
    if (op == "restart" && tokens.size() == 2) {
        return std::make_unique<RestartCommand>(tokens[1]);
    }
    if (op == "status" && tokens.size() == 2) {
        return std::make_unique<StatusCommand>(tokens[1]);
    }
    if (op == "list" && tokens.size() == 1) {
        return std::make_unique<ListCommand>();
    }
    if (op == "help" && tokens.size() == 1) {
        return std::make_unique<HelpCommand>();
    }
    if ((op == "quit" || op == "exit") && tokens.size() == 1) {
        return std::make_unique<QuitCommand>(quit_flag);
    }
    if (op == "add" && tokens.size() >= 4) {
        std::vector<std::string> argv(tokens.begin() + 3, tokens.end());
        return std::make_unique<AddCommand>(tokens[1], tokens[2], std::move(argv));
    }
    return nullptr;
}

}  // namespace ps
