#include "store.h"
#include <sqlite3.h>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <set>
#include <algorithm>

namespace {
void checked(sqlite3* db,int code) {
    if(code!=SQLITE_OK && code!=SQLITE_DONE && code!=SQLITE_ROW)
        throw std::runtime_error(sqlite3_errmsg(db));
}
struct Query {
    sqlite3* db;
    sqlite3_stmt* stmt=nullptr;
    Query(sqlite3* d,const char* sql):db(d) { checked(db,sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr)); }
    ~Query(){sqlite3_finalize(stmt);}
    void text(int n,const std::string& s){checked(db,sqlite3_bind_text(stmt,n,s.c_str(),static_cast<int>(s.size()),SQLITE_TRANSIENT));}
    void number(int n,long long v){checked(db,sqlite3_bind_int64(stmt,n,v));}
    int step(){int r=sqlite3_step(stmt);checked(db,r);return r;}
};
void execute(sqlite3* db,const char* sql) {checked(db,sqlite3_exec(db,sql,nullptr,nullptr,nullptr));}
std::string text_column(sqlite3_stmt* q,int n){const auto* s=sqlite3_column_text(q,n);return s?reinterpret_cast<const char*>(s):"";}
const char* columns="id,title,note,done,remind_at,in_progress,urgent,due_at,source,category,created_at,completed_at,(SELECT count(*) FROM subtasks WHERE parent_id=tasks.id),(SELECT count(*) FROM subtasks WHERE parent_id=tasks.id AND done=1),(SELECT count(*) FROM attachments WHERE task_id=tasks.id),shelved,edit_token";
Task row(sqlite3_stmt* s){Task t;t.id=sqlite3_column_int64(s,0);t.title=text_column(s,1);t.note=text_column(s,2);t.done=sqlite3_column_int(s,3)!=0;t.remind_at=sqlite3_column_int64(s,4);t.in_progress=sqlite3_column_int(s,5)!=0;t.urgent=sqlite3_column_int(s,6)!=0;t.due_at=sqlite3_column_int64(s,7);t.source=text_column(s,8);t.category=text_column(s,9);t.created_at=sqlite3_column_int64(s,10);t.completed_at=sqlite3_column_int64(s,11);t.child_count=sqlite3_column_int(s,12);t.child_done=sqlite3_column_int(s,13);t.attachment_count=sqlite3_column_int(s,14);t.shelved=sqlite3_column_int(s,15)!=0;t.edit_token=text_column(s,16);return t;}
std::string validate_content(const std::string& raw,const std::string& note,long long at){
    auto begin=raw.find_first_not_of(" \t\r\n"),end=raw.find_last_not_of(" \t\r\n");
    if(begin==std::string::npos)throw std::runtime_error(u8"请先输入任务内容");
    std::string title=raw.substr(begin,end-begin+1);
    if(title.size()>2048||note.size()>16384||title.find('\0')!=std::string::npos||note.find('\0')!=std::string::npos)
        throw std::runtime_error(u8"内容过长或包含无效字符");
    if(at<0)throw std::runtime_error(u8"提醒时间无效");
    return title;
}
void changed(sqlite3* db){if(sqlite3_changes(db)!=1)throw std::runtime_error(u8"任务已不存在，请刷新列表");}
std::string exclusions(const std::vector<long long>& ids,int start){if(ids.size()>4096)throw std::runtime_error(u8"提醒队列过长");if(ids.empty())return "";std::string sql=" AND id NOT IN (";for(size_t i=0;i<ids.size();++i){if(i)sql+=',';sql+='?'+std::to_string(start+static_cast<int>(i));}return sql+")";}
}

long long now_seconds(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}

void Store::add_many(const std::vector<Task>& tasks){
    if(tasks.empty()||tasks.size()>200)throw std::runtime_error(u8"每次整理需要 1 到 200 项任务");
    for(const auto& t:tasks){validate_content(t.title,t.note,t.remind_at);if(t.due_at<0||t.source.size()>512||t.category.size()>128)throw std::runtime_error(u8"整理内容无效");}
    execute(db_,"BEGIN IMMEDIATE;");
    try{for(const auto& t:tasks){auto id=add(t.title,t.note,t.remind_at);Query q(db_,"UPDATE tasks SET due_at=?,urgent=?,source=?,category=? WHERE id=?");q.number(1,t.due_at);q.number(2,t.urgent);q.text(3,t.source);q.text(4,t.category);q.number(5,id);q.step();}execute(db_,"COMMIT;");}
    catch(...){sqlite3_exec(db_,"ROLLBACK;",nullptr,nullptr,nullptr);throw;}
}

Store::Store(const std::filesystem::path& path):path_(path) {
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    int result=sqlite3_open_v2(path.u8string().c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr);
    if(result!=SQLITE_OK){std::string error=db_?sqlite3_errmsg(db_):"Cannot open SQLite";if(db_)sqlite3_close(db_);db_=nullptr;throw std::runtime_error(error);}
    try {
        sqlite3_busy_timeout(db_,3000);
        int version=0;
        { Query q(db_,"PRAGMA user_version");q.step();version=sqlite3_column_int(q.stmt,0); }
        if(version>6)throw std::runtime_error(u8"数据来自更高版本，请使用对应版本打开");
        execute(db_,"PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON;");
        if(version>0&&version<6){
            auto copy=path_;copy+=std::filesystem::u8path(".pre-v6-"+std::to_string(now_seconds())+".sqlite3");
            int suffix=0;while(std::filesystem::exists(copy)){copy=path_;copy+=std::filesystem::u8path(".pre-v6-"+std::to_string(now_seconds())+"-"+std::to_string(++suffix)+".sqlite3");}
            backup(copy);
        }
        if(version==0)execute(db_,
            "BEGIN IMMEDIATE;"
            "CREATE TABLE tasks(id INTEGER PRIMARY KEY,title TEXT NOT NULL CHECK(length(title)>0),note TEXT NOT NULL DEFAULT '',done INTEGER NOT NULL DEFAULT 0 CHECK(done IN(0,1)),remind_at INTEGER NOT NULL DEFAULT 0,notified INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL);"
            "CREATE INDEX idx_reminder ON tasks(done,notified,remind_at);"
            "PRAGMA user_version=1; COMMIT;");
        // 原子升级保留旧数据；失败时 SQLite 回滚，禁止静默丢弃数据库。
        if(version<2)execute(db_,"BEGIN IMMEDIATE; ALTER TABLE tasks ADD COLUMN in_progress INTEGER NOT NULL DEFAULT 0 CHECK(in_progress IN(0,1)); ALTER TABLE tasks ADD COLUMN urgent INTEGER NOT NULL DEFAULT 0 CHECK(urgent IN(0,1)); PRAGMA user_version=2; COMMIT;");
        if(version<3)execute(db_,"BEGIN IMMEDIATE; ALTER TABLE tasks ADD COLUMN due_at INTEGER NOT NULL DEFAULT 0; ALTER TABLE tasks ADD COLUMN source TEXT NOT NULL DEFAULT ''; ALTER TABLE tasks ADD COLUMN category TEXT NOT NULL DEFAULT ''; ALTER TABLE tasks ADD COLUMN completed_at INTEGER NOT NULL DEFAULT 0; CREATE TABLE subtasks(id INTEGER PRIMARY KEY AUTOINCREMENT,parent_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,title TEXT NOT NULL CHECK(length(title)>0),done INTEGER NOT NULL DEFAULT 0 CHECK(done IN(0,1)),position INTEGER NOT NULL); CREATE INDEX idx_children ON subtasks(parent_id,position); CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL); PRAGMA user_version=3; COMMIT;");
        if(version<4)execute(db_,"BEGIN IMMEDIATE; CREATE TABLE attachments(id INTEGER PRIMARY KEY,task_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,name TEXT NOT NULL,path TEXT NOT NULL); CREATE INDEX idx_attachments ON attachments(task_id); PRAGMA user_version=4; COMMIT;");
        if(version<5)execute(db_,"BEGIN IMMEDIATE; ALTER TABLE tasks ADD COLUMN shelved INTEGER NOT NULL DEFAULT 0 CHECK(shelved IN(0,1)); PRAGMA user_version=5; COMMIT;");
        execute(db_,"BEGIN IMMEDIATE;");
        if(version<6)execute(db_,
            "ALTER TABLE tasks ADD COLUMN edit_token TEXT NOT NULL DEFAULT '';"
            "UPDATE tasks SET edit_token=lower(hex(randomblob(16)));");
        // 独立保存历史最大编号，删除最高编号或重启均不能复用任务身份。
        // 在同一写事务中分配并推进水位，多个 Store 连接也不会分配相同编号。
        execute(db_,
            "CREATE TABLE IF NOT EXISTS task_ids(singleton INTEGER PRIMARY KEY CHECK(singleton=1),high_water INTEGER NOT NULL);"
            "INSERT OR IGNORE INTO task_ids VALUES(1,0);"
            "UPDATE task_ids SET high_water=max(high_water,coalesce((SELECT max(id) FROM tasks),0)) WHERE singleton=1;"
            "CREATE TRIGGER IF NOT EXISTS tasks_id_insert AFTER INSERT ON tasks BEGIN UPDATE task_ids SET high_water=max(high_water,NEW.id) WHERE singleton=1; END;");
        // 标识由 SQLite 更新，其他连接修改关联记录也会使原草稿失效。
        // 单纯通知回执不改变业务内容，因此 notified 不参与草稿冲突。
        execute(db_,
            "CREATE TRIGGER IF NOT EXISTS tasks_edit_insert AFTER INSERT ON tasks BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=NEW.id; END;"
            "CREATE TRIGGER IF NOT EXISTS tasks_edit_update AFTER UPDATE OF title,note,done,remind_at,in_progress,urgent,due_at,source,category,created_at,completed_at,shelved ON tasks BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=NEW.id; END;"
            "CREATE TRIGGER IF NOT EXISTS children_edit_insert AFTER INSERT ON subtasks BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=NEW.parent_id; END;"
            "CREATE TRIGGER IF NOT EXISTS children_edit_update AFTER UPDATE ON subtasks BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=OLD.parent_id OR id=NEW.parent_id; END;"
            "CREATE TRIGGER IF NOT EXISTS children_edit_delete AFTER DELETE ON subtasks BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=OLD.parent_id; END;"
            "CREATE TRIGGER IF NOT EXISTS attachments_edit_insert AFTER INSERT ON attachments BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=NEW.task_id; END;"
            "CREATE TRIGGER IF NOT EXISTS attachments_edit_update AFTER UPDATE ON attachments BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=OLD.task_id OR id=NEW.task_id; END;"
            "CREATE TRIGGER IF NOT EXISTS attachments_edit_delete AFTER DELETE ON attachments BEGIN UPDATE tasks SET edit_token=lower(hex(randomblob(16))) WHERE id=OLD.task_id; END;");
        // 字段、令牌、触发器和版本号一同提交，启动失败不会留下半升级状态。
        execute(db_,"PRAGMA user_version=6; COMMIT;");
    } catch (...) {sqlite3_close(db_);db_=nullptr;throw;}
}
Store::~Store(){if(db_)sqlite3_close(db_);}

long long Store::add(const std::string& raw,const std::string& note,long long remind_at){
    std::string title=validate_content(raw,note,remind_at);
    Query q(db_,"INSERT INTO tasks(id,title,note,remind_at,created_at) VALUES((SELECT high_water+1 FROM task_ids WHERE singleton=1),?1,?2,?3,?4)");
    q.text(1,title);q.text(2,note);q.number(3,remind_at);q.number(4,now_seconds());q.step();return sqlite3_last_insert_rowid(db_);
}
std::vector<Task> Store::list(const std::string& query,int filter){
    // 对 LIKE 特殊字符进行转义，使用户输入的 %/_ 仍作为普通文字查找。
    std::string pattern="%";for(char c:query){if(c=='%'||c=='_'||c=='\\')pattern+='\\';pattern+=c;}pattern+='%';
    if(filter<0||filter>4)throw std::runtime_error(u8"任务分类无效");
    std::string order=setting("sort_due","1")=="1"?"CASE WHEN due_at=0 THEN 9223372036854775807 ELSE due_at END,":"";
    std::string sql=std::string("SELECT ")+columns+" FROM tasks WHERE (title LIKE ?1 ESCAPE '\\' OR note LIKE ?1 ESCAPE '\\' OR source LIKE ?1 ESCAPE '\\' OR category LIKE ?1 ESCAPE '\\') AND (?2=0 OR (?2=1 AND done=0 AND in_progress=0 AND shelved=0) OR (?2=2 AND done=1) OR (?2=3 AND done=0 AND in_progress=1 AND shelved=0) OR (?2=4 AND done=0 AND shelved=1)) ORDER BY done,shelved,urgent DESC,"+order+"in_progress DESC,completed_at DESC,id DESC LIMIT 2000";
    Query q(db_,sql.c_str());
    q.text(1,pattern);q.number(2,filter);std::vector<Task> tasks;while(q.step()==SQLITE_ROW)tasks.push_back(row(q.stmt));return tasks;
}
Task Store::get(long long id){
    Query q(db_,(std::string("SELECT ")+columns+" FROM tasks WHERE id=?1").c_str());q.number(1,id);
    if(q.step()!=SQLITE_ROW)throw std::runtime_error(u8"任务已不存在，请刷新列表");
    return row(q.stmt);
}
void Store::set_done(long long id,bool done){
    Query q(db_,"UPDATE tasks SET done=?1,in_progress=0,shelved=0,completed_at=?3 WHERE id=?2 AND (?1=0 OR NOT EXISTS(SELECT 1 FROM subtasks WHERE parent_id=?2 AND done=0))");q.number(1,done);q.number(2,id);q.number(3,done?now_seconds():0);q.step();
    if(sqlite3_changes(db_)!=1)throw std::runtime_error(u8"请先完成全部子任务，或刷新已删除的任务");
}
void Store::set_progress(long long id,bool active){Query q(db_,"UPDATE tasks SET in_progress=?1,shelved=0 WHERE id=?2 AND done=0");q.number(1,active);q.number(2,id);q.step();changed(db_);}
void Store::set_shelved(long long id,bool shelved){Query q(db_,"UPDATE tasks SET shelved=?1,in_progress=0 WHERE id=?2 AND done=0");q.number(1,shelved);q.number(2,id);q.step();changed(db_);}
void Store::set_urgent(long long id,bool urgent){Query q(db_,"UPDATE tasks SET urgent=?1 WHERE id=?2");q.number(1,urgent);q.number(2,id);q.step();changed(db_);}
void Store::update(long long id,const std::string& raw,const std::string& note,long long at){
    auto title=validate_content(raw,note,at);
    // 只改文字时保留提醒已发送状态；日期变更才重新启用提醒。
    Query q(db_,"UPDATE tasks SET title=?1,note=?2,notified=CASE WHEN remind_at=?3 THEN notified ELSE 0 END,remind_at=?3 WHERE id=?4");
    q.text(1,title);q.text(2,note);q.number(3,at);q.number(4,id);q.step();changed(db_);
}
void Store::remove(long long id,const std::string& expected_token){
    Query q(db_,"DELETE FROM tasks WHERE id=?1 AND (?2='' OR edit_token=?2)");q.number(1,id);q.text(2,expected_token);q.step();
    if(sqlite3_changes(db_)!=1)throw std::runtime_error(u8"任务已被修改或删除，请刷新后重新确认");
}
Task Store::reschedule(long long id,long long at,const std::string& expected_token){
    if(at<0)throw std::runtime_error(u8"提醒时间无效");
    execute(db_,"BEGIN IMMEDIATE;");try{
        auto current=get(id);
        if(!expected_token.empty()&&current.edit_token!=expected_token)throw std::runtime_error(u8"任务已被修改，请刷新后重新设置提醒");
        Query q(db_,"UPDATE tasks SET remind_at=?1,notified=0 WHERE id=?2");q.number(1,at);q.number(2,id);q.step();changed(db_);
        auto updated=get(id);execute(db_,"COMMIT;");return updated;
    }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}
std::vector<Task> Store::due(long long at,const std::vector<long long>& excluded){std::string sql=std::string("SELECT ")+columns+" FROM tasks WHERE done=0 AND shelved=0 AND notified=0 AND remind_at>0 AND remind_at<=?1"+exclusions(excluded,2)+" ORDER BY remind_at,id LIMIT 20";Query q(db_,sql.c_str());q.number(1,at);for(size_t i=0;i<excluded.size();++i)q.number(static_cast<int>(i)+2,excluded[i]);std::vector<Task> result;while(q.step()==SQLITE_ROW)result.push_back(row(q.stmt));return result;}
long long Store::next_due(const std::vector<long long>& excluded){std::string sql="SELECT min(remind_at) FROM tasks WHERE done=0 AND shelved=0 AND notified=0 AND remind_at>0"+exclusions(excluded,1);Query q(db_,sql.c_str());for(size_t i=0;i<excluded.size();++i)q.number(static_cast<int>(i)+1,excluded[i]);q.step();return sqlite3_column_int64(q.stmt,0);}
void Store::mark_notified(long long id){Query q(db_,"UPDATE tasks SET notified=1 WHERE id=?1");q.number(1,id);q.step();changed(db_);}
void Store::confirm_notification(long long id,long long expected_time){
    // 异步确认只能匹配当前提醒时间，忽略期间已删除或改期的旧请求。
    Query q(db_,"UPDATE tasks SET notified=1 WHERE id=?1 AND remind_at=?2 AND done=0 AND shelved=0");q.number(1,id);q.number(2,expected_time);q.step();
}
void Store::backup(const std::filesystem::path& destination){
    // 使用 SQLite Backup API 获取一致快照，不能直接复制活跃的 WAL 数据库。
    if(std::filesystem::exists(destination))throw std::runtime_error(u8"备份文件已存在，请选择新名称");
    sqlite3* target=nullptr;
    if(sqlite3_open(destination.u8string().c_str(),&target)!=SQLITE_OK){if(target)sqlite3_close(target);throw std::runtime_error(u8"无法创建备份文件");}
    sqlite3_backup* backup=sqlite3_backup_init(target,"main",db_,"main");
    int result=backup?sqlite3_backup_step(backup,-1):SQLITE_ERROR;
    if(backup){int finish=sqlite3_backup_finish(backup);if(result==SQLITE_DONE && finish!=SQLITE_OK)result=finish;}
    sqlite3_close(target);
    if(result!=SQLITE_DONE){std::error_code ignored;std::filesystem::remove(destination,ignored);throw std::runtime_error(u8"备份失败，原始数据保持不变");}
}
std::vector<Subtask> Store::children(long long parent){
    Query q(db_,"SELECT id,title,done FROM subtasks WHERE parent_id=?1 ORDER BY position,id");q.number(1,parent);std::vector<Subtask> rows;
    while(q.step()==SQLITE_ROW)rows.push_back({sqlite3_column_int64(q.stmt,0),text_column(q.stmt,1),sqlite3_column_int(q.stmt,2)!=0});
    return rows;
}
void Store::restore_from(const std::filesystem::path& source){
    // 外部文件只读，先在独立副本校验/升级，再原子导入当前数据库。
    sqlite3* input=nullptr;if(sqlite3_open_v2(source.u8string().c_str(),&input,SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK){if(input)sqlite3_close(input);throw std::runtime_error(u8"无法读取备份文件");}
    auto temporary=path_.parent_path()/(".import-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".sqlite3");
    try{
        {Query integrity(input,"PRAGMA quick_check");if(integrity.step()!=SQLITE_ROW||text_column(integrity.stmt,0)!="ok")throw std::runtime_error(u8"备份文件校验失败");}
        {Query identity(input,"SELECT id,title,note,done,remind_at,notified,created_at FROM tasks LIMIT 0");Query version(input,"PRAGMA user_version");version.step();auto n=sqlite3_column_int(version.stmt,0);if(n<1||n>6)throw std::runtime_error(u8"这不是支持的轻待办备份");}
        sqlite3* copy=nullptr;int opened=sqlite3_open(temporary.u8string().c_str(),&copy);if(opened!=SQLITE_OK){if(copy)sqlite3_close(copy);throw std::runtime_error(u8"无法创建导入副本");}auto transfer=sqlite3_backup_init(copy,"main",input,"main");int result=transfer?sqlite3_backup_step(transfer,-1):SQLITE_ERROR;int copied=transfer?sqlite3_backup_finish(transfer):SQLITE_ERROR;sqlite3_close(copy);if(result!=SQLITE_DONE||copied!=SQLITE_OK)throw std::runtime_error(u8"无法复制备份");sqlite3_close(input);input=nullptr;
        {Store candidate(temporary);candidate.list();candidate.setting("theme");
            // 逐项准备业务查询，避免仅有表名的损坏备份通过版本号检查。
            {Query schema(candidate.db_,"SELECT id,title,note,done,remind_at,notified,created_at,in_progress,urgent,due_at,source,category,completed_at,shelved FROM tasks LIMIT 0");}
            {Query schema(candidate.db_,"SELECT id,parent_id,title,done,position FROM subtasks LIMIT 0");}
            {Query schema(candidate.db_,"SELECT id,task_id,name,path FROM attachments LIMIT 0");}
            {Query schema(candidate.db_,"SELECT key,value FROM settings LIMIT 0");}
            {Query invalid(candidate.db_,"SELECT 1 FROM subtasks s LEFT JOIN tasks t ON t.id=s.parent_id WHERE t.id IS NULL UNION ALL SELECT 1 FROM attachments a LEFT JOIN tasks t ON t.id=a.task_id WHERE t.id IS NULL LIMIT 1");if(invalid.step()!=SQLITE_DONE)throw std::runtime_error(u8"备份存在孤立的子任务或附件");}
            {Query integrity(candidate.db_,"PRAGMA foreign_key_check");if(integrity.step()!=SQLITE_DONE)throw std::runtime_error(u8"备份的任务关联无效");}
            // 导入副本即使内容未变，也属于新一轮数据，旧窗口草稿不可继续写回。
            execute(candidate.db_,"UPDATE tasks SET edit_token=lower(hex(randomblob(16))); ");
            // 导入旧快照不能倒退当前库已用过的编号水位。
            {Query live(db_,"SELECT high_water FROM task_ids WHERE singleton=1");live.step();Query retain(candidate.db_,"UPDATE task_ids SET high_water=max(high_water,?1) WHERE singleton=1");retain.number(1,sqlite3_column_int64(live.stmt,0));retain.step();}
            auto before=path_.parent_path()/("before-import-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".sqlite3");backup(before);
            auto restore=sqlite3_backup_init(db_,"main",candidate.db_,"main");int code=restore?sqlite3_backup_step(restore,-1):SQLITE_ERROR;int finish=restore?sqlite3_backup_finish(restore):SQLITE_ERROR;if(code!=SQLITE_DONE||finish!=SQLITE_OK)throw std::runtime_error(u8"导入失败，原数据及导入前备份保留");
        }
    }catch(...){if(input)sqlite3_close(input);std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
    std::error_code ignored;std::filesystem::remove(temporary,ignored);
}
void Store::relocate(const std::filesystem::path& destination,const std::function<void()>& publish){
    // backup 拒绝已有文件；成功以后才拥有此候选文件的失败清理权。
    backup(destination);
    try{Store candidate(destination);candidate.list();if(publish)publish();std::swap(db_,candidate.db_);std::swap(path_,candidate.path_);}
    catch(...){for(const char* suffix:{"","-wal","-shm"}){auto file=destination;file+=suffix;std::error_code ignored;std::filesystem::remove(file,ignored);}throw;}
}
std::vector<Attachment> Store::attachments(long long task){Query q(db_,"SELECT name,path FROM attachments WHERE task_id=?1 ORDER BY id");q.number(1,task);std::vector<Attachment> items;while(q.step()==SQLITE_ROW)items.push_back({text_column(q.stmt,0),text_column(q.stmt,1)});return items;}
std::string Store::setting(const std::string& key,const std::string& fallback){Query q(db_,"SELECT value FROM settings WHERE key=?1");q.text(1,key);return q.step()==SQLITE_ROW?text_column(q.stmt,0):fallback;}
void Store::set_setting(const std::string& key,const std::string& value){
    if(key.empty()||key.size()>64||value.size()>4096||key.find('\0')!=std::string::npos||value.find('\0')!=std::string::npos)throw std::runtime_error(u8"设置内容无效");
    Query q(db_,"INSERT INTO settings VALUES(?1,?2) ON CONFLICT(key) DO UPDATE SET value=excluded.value");q.text(1,key);q.text(2,value);q.step();
}
long long Store::save_task(const Task& task,const std::vector<Subtask>& items,const std::vector<Attachment>* files){
    auto title=validate_content(task.title,task.note,task.remind_at);
    if(task.id<0||task.due_at<0||task.source.size()>512||task.category.size()>128||task.source.find('\0')!=std::string::npos||task.category.find('\0')!=std::string::npos||items.size()>200)throw std::runtime_error(u8"任务字段无效或子任务超过 200 条");
    std::set<long long> used;for(const auto& item:items){validate_content(item.title,"",0);if(item.id<0||(item.id&&!used.insert(item.id).second))throw std::runtime_error(u8"子任务标识重复或无效");}
    if(task.done&&std::any_of(items.begin(),items.end(),[](const Subtask& child){return !child.done;}))throw std::runtime_error(u8"请先完成全部子任务");
    if(files){if(files->size()>40)throw std::runtime_error(u8"每项任务最多 40 个附件");for(const auto& file:*files)if(file.name.empty()||file.name.size()>512||file.path.empty()||file.path.size()>4096||file.name.find('\0')!=std::string::npos||file.path.find('\0')!=std::string::npos)throw std::runtime_error(u8"附件字段无效");}
    execute(db_,"BEGIN IMMEDIATE;");try{
        long long id=task.id;
        if(id){auto current=get(id);if(task.edit_token.empty()||current.edit_token!=task.edit_token)throw std::runtime_error(u8"此任务已被修改或替换。草稿已保留，请取消后重新打开任务再编辑。");}
        auto old=children(id);for(long long child:used)if(std::none_of(old.begin(),old.end(),[&](const Subtask& s){return s.id==child;}))throw std::runtime_error(u8"子任务不属于当前任务");
        if(id){update(id,title,task.note,task.remind_at);}else id=add(title,task.note,task.remind_at);
        Query fields(db_,"UPDATE tasks SET due_at=?1,source=?2,category=?3,completed_at=CASE WHEN done!=?5 THEN ?7 ELSE completed_at END,done=?5,in_progress=?6,urgent=?8,shelved=?9 WHERE id=?4");fields.number(1,task.due_at);fields.text(2,task.source);fields.text(3,task.category);fields.number(4,id);fields.number(5,task.done);fields.number(6,task.in_progress&&!task.done&&!task.shelved);fields.number(7,task.done?now_seconds():0);fields.number(8,task.urgent);fields.number(9,task.shelved&&!task.done);fields.step();changed(db_);
        Query clear(db_,"DELETE FROM subtasks WHERE parent_id=?1");clear.number(1,id);clear.step();
        // 先保留已有 ID，再生成新 ID，避免自增标识与待恢复的旧标识冲突。
        for(int pass=0;pass<2;++pass)for(size_t i=0;i<items.size();++i){const auto& child=items[i];if((child.id!=0)!=(pass==0))continue;
            Query insert(db_,"INSERT INTO subtasks(id,parent_id,title,done,position) VALUES(?1,?2,?3,?4,?5)");if(child.id)insert.number(1,child.id);else checked(db_,sqlite3_bind_null(insert.stmt,1));insert.number(2,id);insert.text(3,validate_content(child.title,"",0));insert.number(4,child.done);insert.number(5,static_cast<long long>(i));insert.step();
        }
        if(files){Query clear_files(db_,"DELETE FROM attachments WHERE task_id=?1");clear_files.number(1,id);clear_files.step();for(const auto& file:*files){Query insert(db_,"INSERT INTO attachments(task_id,name,path) VALUES(?1,?2,?3)");insert.number(1,id);insert.text(2,file.name);insert.text(3,file.path);insert.step();}}
        execute(db_,"COMMIT;");return id;
    }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}
void Store::set_child_done(long long child,bool done){
    execute(db_,"BEGIN IMMEDIATE;");try{
        Query q(db_,"UPDATE subtasks SET done=?1 WHERE id=?2");q.number(1,done);q.number(2,child);q.step();changed(db_);
        if(!done){Query reopen(db_,"UPDATE tasks SET done=0,in_progress=0,completed_at=0 WHERE id=(SELECT parent_id FROM subtasks WHERE id=?1) AND done=1");reopen.number(1,child);reopen.step();}
        execute(db_,"COMMIT;");
    }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}
