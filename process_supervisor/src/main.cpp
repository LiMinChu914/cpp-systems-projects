#include "command.hpp"
#include "observer.hpp"
#include "parser.hpp"
#include "process_manager.hpp"
#include "restart_policy.hpp"

#include <iostream>
#include <memory>
#include <string>

int main() {
    using namespace ps;

    ProcessManager manager;
    auto console = std::make_shared<ConsoleObserver>();
    auto metrics = std::make_shared<MetricsObserver>();
    manager.add_observer(console);
    manager.add_observer(metrics);

    // A small built-in process makes the demo immediately runnable.
    manager.add_process(
        ProcessSpec{"heartbeat",
                    {"/bin/sh", "-c", "i=0; while [ $i -lt 5 ]; do echo heartbeat:$i; i=$((i+1)); sleep 1; done"},
                    std::nullopt,
                    true},
        RestartPolicyFactory::create("never"));

    std::cout << "Linux Process Supervisor\n"
              << "Type 'help'. Built-in demo: start heartbeat\n";

    bool quit = false;
    std::string line;
    while (!quit && std::cout << "supervisor> " && std::getline(std::cin, line)) {
        try {
            auto tokens = CommandLineParser::tokenize(line);
            auto command = CommandFactory::create(tokens, quit);
            if (!command) {
                if (!tokens.empty()) {
                    std::cerr << "invalid command; type help\n";
                }
                continue;
            }
            command->execute(manager);
        } catch (const std::exception& error) {
            std::cerr << "command error: " << error.what() << '\n';
        }
    }

    manager.shutdown();
    metrics->print();
    return 0;
}
