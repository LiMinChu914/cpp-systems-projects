#include "command.hpp"
#include "observer.hpp"
#include "scheduler.hpp"
#include "scheduling_strategy.hpp"
#include "task.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    using namespace js;

    std::size_t worker_count = 4;
    bool pin_workers = false;
    if (argc >= 2) {
        worker_count = static_cast<std::size_t>(std::stoul(argv[1]));
    }
    if (argc >= 3 && std::string(argv[2]) == "--pin") {
        pin_workers = true;
    }

    JobScheduler scheduler(worker_count,
                           SchedulingStrategyFactory::create("fifo"),
                           pin_workers);
    auto console = std::make_shared<ConsoleObserver>();
    auto metrics = std::make_shared<MetricsObserver>();
    scheduler.add_observer(console);
    scheduler.add_observer(metrics);
    scheduler.start();

    std::cout << "Concurrent Job Scheduler with " << worker_count << " workers\n"
              << "Type help for commands.\n";

    bool quit = false;
    std::string line;
    while (!quit && std::cout << "scheduler> " && std::getline(std::cin, line)) {
        try {
            auto tokens = tokenize(line);
            auto command = CommandFactory::create(tokens, quit);
            if (!command) {
                if (!tokens.empty()) std::cerr << "invalid command\n";
                continue;
            }
            command->execute(scheduler);
        } catch (const std::exception& error) {
            std::cerr << "command error: " << error.what() << '\n';
        }
    }

    scheduler.shutdown(true);
    metrics->print();
    return 0;
}
