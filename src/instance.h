#pragma once
#include <filesystem>
#include <memory>

// 锁文件由系统锁持有，不依赖 PID 文本；进程退出后操作系统自动释放。
class InstanceLock {
public:
    explicit InstanceLock(const std::filesystem::path& directory);
    ~InstanceLock();
    InstanceLock(const InstanceLock&)=delete;
    InstanceLock& operator=(const InstanceLock&)=delete;
    bool owns()const;
    void request_activation();
    bool consume_activation();
    void swap(InstanceLock& other)noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
