#include "instance.h"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

struct InstanceLock::Impl {
    std::filesystem::path directory;
    bool owned=false;
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
    ~Impl(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
#else
    int file=-1;
    ~Impl(){if(file>=0)close(file);}
#endif
};
InstanceLock::InstanceLock(const std::filesystem::path& directory):impl_(std::make_unique<Impl>()){
    std::filesystem::create_directories(directory);
    // 互斥依赖实际锁文件的系统身份，软链接/目录别名会落到同一个文件。
    // 不依赖部分 Windows/沙箱环境不支持的 canonical 路径查询。
    impl_->directory=std::filesystem::absolute(directory).lexically_normal();
    auto lock=impl_->directory/".litetodo-instance.lock";
#ifdef _WIN32
    impl_->file=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(impl_->file==INVALID_HANDLE_VALUE)throw std::runtime_error(u8"无法创建数据目录的运行锁，请检查目录权限");
    OVERLAPPED position{};
    impl_->owned=LockFileEx(impl_->file,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&position)!=FALSE;
    if(!impl_->owned&&GetLastError()!=ERROR_LOCK_VIOLATION)throw std::runtime_error(u8"无法锁定数据目录，未打开任务库");
#else
    impl_->file=open(lock.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(impl_->file<0)throw std::runtime_error(u8"无法创建数据目录的运行锁，请检查目录权限");
    impl_->owned=flock(impl_->file,LOCK_EX|LOCK_NB)==0;
    if(!impl_->owned&&errno!=EWOULDBLOCK&&errno!=EAGAIN)throw std::runtime_error(u8"无法锁定数据目录，未打开任务库");
#endif
}
InstanceLock::~InstanceLock()=default;
bool InstanceLock::owns()const{return impl_->owned;}
void InstanceLock::request_activation(){
    if(owns())return;
    // 目录创建是幂等请求；不读写锁文件内容，也不覆盖任意用户文件。
    std::filesystem::create_directory(impl_->directory/".litetodo-activate");
}
bool InstanceLock::consume_activation(){
    if(!owns())return false;
    auto request=impl_->directory/".litetodo-activate";
    std::error_code error;
    if(!std::filesystem::is_directory(request,error))return false;
    return std::filesystem::remove(request,error);
}
void InstanceLock::swap(InstanceLock& other)noexcept{impl_.swap(other.impl_);}
