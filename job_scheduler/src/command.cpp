#include "command.hpp"
#include "scheduling_strategy.hpp"
#include "task.hpp"

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace js {

void SubmitSleepCommand::execute(JobScheduler& scheduler) {
    auto id = scheduler.submit(TaskFactory::sleep(name_, ms_), priority_);
    std::cout << "submitted task " << id << '\n';
}

void SubmitPrimeCommand::execute(JobScheduler& scheduler) {
    auto id = scheduler.submit(TaskFactory::primes(name_, limit_), priority_);
    std::cout << "submitted task " << id << '\n';
}

void SubmitChecksumCommand::execute(JobScheduler& scheduler) {
    auto id = scheduler.submit(TaskFactory::checksum(name_, path_), priority_);
    std::cout << "submitted task " << id << '\n';
}

void CancelCommand::execute(JobScheduler& scheduler) {
    if (!scheduler.cancel(id_)) {
        std::cerr << "cannot cancel task " << id_ << '\n';
    }
}

void ListCommand::execute(JobScheduler& scheduler) {
    std::cout << "scheduler=" << to_string(scheduler.state())
              << " strategy=" << scheduler.strategy_name() << '\n';
    std::cout << std::left
              << std::setw(8) << "ID"
              << std::setw(22) << "NAME"
              << std::setw(10) << "PRIORITY"
              << std::setw(14) << "STATUS"
              << "WORKER\n";
    for (const auto& item : scheduler.snapshot_tasks()) {
        std::cout << std::left
                  << std::setw(8) << item.id
                  << std::setw(22) << item.name
                  << std::setw(10) << item.priority
                  << std::setw(14) << to_string(item.status);
        if (item.worker_index == static_cast<std::size_t>(-1)) {
            std::cout << "-";
        } else {
            std::cout << item.worker_index;
        }
        std::cout << '\n';
    }
}

void StrategyCommand::execute(JobScheduler& scheduler) {
    try {
        scheduler.set_strategy(SchedulingStrategyFactory::create(name_));
        std::cout << "strategy changed to " << name_ << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
}

void HelpCommand::execute(JobScheduler&) {
    std::cout
        << "commands:\n"
        << "  sleep <name> <ms> [priority]\n"
        << "  primes <name> <limit> [priority]\n"
        << "  checksum <name> <path> [priority]\n"
        << "  cancel <id>\n"
        << "  strategy <fifo|priority|fair-priority>\n"
        << "  pause\n"
        << "  resume\n"
        << "  list\n"
        << "  help\n"
        << "  quit\n";
}

std::unique_ptr<ICommand> CommandFactory::create(const std::vector<std::string>& tokens,
                                                  bool& quit) {
    if (tokens.empty()) return nullptr;
    const auto& op = tokens[0];

    try {
        if (op == "sleep" && (tokens.size() == 3 || tokens.size() == 4)) {
            const int ms = std::stoi(tokens[2]);
            const int priority = tokens.size() == 4 ? std::stoi(tokens[3]) : 0;
            return std::make_unique<SubmitSleepCommand>(tokens[1], ms, priority);
        }
        if (op == "primes" && (tokens.size() == 3 || tokens.size() == 4)) {
            const auto limit = static_cast<std::uint64_t>(std::stoull(tokens[2]));
            const int priority = tokens.size() == 4 ? std::stoi(tokens[3]) : 0;
            return std::make_unique<SubmitPrimeCommand>(tokens[1], limit, priority);
        }
        if (op == "checksum" && (tokens.size() == 3 || tokens.size() == 4)) {
            const int priority = tokens.size() == 4 ? std::stoi(tokens[3]) : 0;
            return std::make_unique<SubmitChecksumCommand>(tokens[1], tokens[2], priority);
        }
        if (op == "cancel" && tokens.size() == 2) {
            return std::make_unique<CancelCommand>(std::stoull(tokens[1]));
        }
        if (op == "strategy" && tokens.size() == 2) {
            return std::make_unique<StrategyCommand>(tokens[1]);
        }
        if (op == "pause" && tokens.size() == 1) {
            return std::make_unique<PauseCommand>();
        }
        if (op == "resume" && tokens.size() == 1) {
            return std::make_unique<ResumeCommand>();
        }
        if (op == "list" && tokens.size() == 1) {
            return std::make_unique<ListCommand>();
        }
        if (op == "help" && tokens.size() == 1) {
            return std::make_unique<HelpCommand>();
        }
        if ((op == "quit" || op == "exit") && tokens.size() == 1) {
            return std::make_unique<QuitCommand>(quit);
        }
    } catch (const std::exception&) {
        return nullptr;
    }
    return nullptr;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool single = false;
    bool dbl = false;
    bool escape = false;

    auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };

    for (char ch : line) {
        if (escape) {
            current.push_back(ch);
            escape = false;
            continue;
        }
        if (ch == '\\' && !single) {
            escape = true;
            continue;
        }
        if (ch == '\'' && !dbl) {
            single = !single;
            continue;
        }
        if (ch == '"' && !single) {
            dbl = !dbl;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) && !single && !dbl) {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    if (single || dbl) throw std::runtime_error("unterminated quote");
    flush();
    return tokens;
}

}  // namespace js
