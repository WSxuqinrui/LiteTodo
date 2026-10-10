#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <functional>

struct sqlite3;

// 时间统一为 UTC Unix 秒；0 表示没有提醒。
struct Task {
    long long id=0;
    std::string title, note;
    bool done=false;
    long long remind_at=0;
    bool in_progress=false,urgent=false;
    long long due_at=0,created_at=0,completed_at=0;
    std::string source,category;
    int child_count=0,child_done=0,attachment_count=0;
    bool shelved=false;
};
struct Subtask {long long id=0;std::string title;bool done=false;};
struct Attachment {std::string name,path;};
long long now_seconds();

// 存储独立于 UI 和平台，SQL 全部参数化；每次修改均由 SQLite 原子提交。
class Store {
public:
    explicit Store(const std::filesystem::path& path);
    ~Store();
    Store(const Store&)=delete;
    Store& operator=(const Store&)=delete;
    void add_many(const std::vector<Task>& tasks);
    long long add(const std::string& title,const std::string& note,long long remind_at);
    std::vector<Task> list(const std::string& query="",int filter=0);
    void set_done(long long id,bool done);
    void set_progress(long long id,bool active);
    void set_urgent(long long id,bool urgent);
    // 搁置暂停提醒；开始或完成任务时自动结束搁置。
    void set_shelved(long long id,bool shelved);
    void update(long long id,const std::string& title,const std::string& note,long long remind_at);
    void remove(long long id);
    void reschedule(long long id,long long remind_at);
    std::vector<Task> due(long long at,const std::vector<long long>& excluded={});
    long long next_due(const std::vector<long long>& excluded={});
    void mark_notified(long long id);
    void confirm_notification(long long id,long long expected_time);
    void backup(const std::filesystem::path& destination);
    void restore_from(const std::filesystem::path& source);
    void relocate(const std::filesystem::path& destination,const std::function<void()>& publish={});
    // 整个编辑草稿一次提交；取消页面不调用此接口。
    long long save_task(const Task& task,const std::vector<Subtask>& children,const std::vector<Attachment>* files=nullptr);
    std::vector<Attachment> attachments(long long task);
    std::vector<Subtask> children(long long parent);
    void set_child_done(long long child,bool done);
    std::string setting(const std::string& key,const std::string& fallback="");
    void set_setting(const std::string& key,const std::string& value);
private:
    sqlite3* db_=nullptr;
    std::filesystem::path path_;
};
