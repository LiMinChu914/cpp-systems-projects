#include "task.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace js {

void SleepTask::execute(const TaskContext& context) {
    constexpr auto quantum = std::chrono::milliseconds(25);
    auto remaining = duration_;
    while (remaining.count() > 0) {
        if (context.cancellation.cancelled()) {
            return;
        }
        auto step = remaining < quantum ? remaining : quantum;
        std::this_thread::sleep_for(step);
        remaining -= step;
    }
}

bool PrimeCountTask::is_prime(std::uint64_t value) {
    if (value < 2) return false;
    if (value == 2) return true;
    if (value % 2 == 0) return false;
    for (std::uint64_t divisor = 3;
         divisor <= value / divisor;
         divisor += 2) {
        if (value % divisor == 0) return false;
    }
    return true;
}

void PrimeCountTask::execute(const TaskContext& context) {
    std::uint64_t count = 0;
    for (std::uint64_t value = 2; value <= limit_; ++value) {
        if ((value & 0x3FFU) == 0 && context.cancellation.cancelled()) {
            return;
        }
        if (is_prime(value)) {
            ++count;
        }
    }
    std::cout << "[task-result] " << label_ << " primes<= " << limit_
              << " = " << count << '\n';
}

void FileChecksumTask::execute(const TaskContext& context) {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path_);
    }

    // Educational FNV-1a style checksum; not cryptographic.
    std::uint64_t hash = 1469598103934665603ULL;
    char buffer[4096];
    while (input) {
        if (context.cancellation.cancelled()) {
            return;
        }
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }
    std::cout << "[task-result] " << label_ << " checksum=" << hash << '\n';
}

void FailingTask::execute(const TaskContext&) {
    throw std::runtime_error("intentional task failure");
}

std::shared_ptr<ITask> TaskFactory::sleep(std::string name, int milliseconds) {
    if (milliseconds < 0) {
        throw std::invalid_argument("sleep duration must be non-negative");
    }
    return std::make_shared<SleepTask>(std::move(name),
                                       std::chrono::milliseconds(milliseconds));
}

std::shared_ptr<ITask> TaskFactory::primes(std::string name,
                                           std::uint64_t limit) {
    return std::make_shared<PrimeCountTask>(std::move(name), limit);
}

std::shared_ptr<ITask> TaskFactory::checksum(std::string name,
                                             std::string path) {
    return std::make_shared<FileChecksumTask>(std::move(name), std::move(path));
}

std::shared_ptr<ITask> TaskFactory::failing(std::string name) {
    return std::make_shared<FailingTask>(std::move(name));
}

}  // namespace js
