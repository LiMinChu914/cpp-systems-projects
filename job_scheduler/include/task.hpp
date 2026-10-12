#pragma once

#include "cancellation.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace js {

struct TaskContext {
    std::size_t worker_index{0};
    CancellationToken cancellation;
};

// Command Pattern: each unit of work is a command object.
class ITask {
public:
    virtual ~ITask() = default;
    virtual void execute(const TaskContext& context) = 0;
    virtual std::string name() const = 0;
};

class SleepTask final : public ITask {
public:
    SleepTask(std::string label, std::chrono::milliseconds duration)
        : label_(std::move(label)), duration_(duration) {}
    void execute(const TaskContext& context) override;
    std::string name() const override { return label_; }
private:
    std::string label_;
    std::chrono::milliseconds duration_;
};

class PrimeCountTask final : public ITask {
public:
    PrimeCountTask(std::string label, std::uint64_t limit)
        : label_(std::move(label)), limit_(limit) {}
    void execute(const TaskContext& context) override;
    std::string name() const override { return label_; }
private:
    static bool is_prime(std::uint64_t value);
    std::string label_;
    std::uint64_t limit_;
};

class FileChecksumTask final : public ITask {
public:
    FileChecksumTask(std::string label, std::string path)
        : label_(std::move(label)), path_(std::move(path)) {}
    void execute(const TaskContext& context) override;
    std::string name() const override { return label_; }
private:
    std::string label_;
    std::string path_;
};

class FailingTask final : public ITask {
public:
    explicit FailingTask(std::string label) : label_(std::move(label)) {}
    void execute(const TaskContext&) override;
    std::string name() const override { return label_; }
private:
    std::string label_;
};

// Factory Pattern: parse a compact task type into a polymorphic task object.
class TaskFactory {
public:
    static std::shared_ptr<ITask> sleep(std::string name, int milliseconds);
    static std::shared_ptr<ITask> primes(std::string name, std::uint64_t limit);
    static std::shared_ptr<ITask> checksum(std::string name, std::string path);
    static std::shared_ptr<ITask> failing(std::string name);
};

}  // namespace js
