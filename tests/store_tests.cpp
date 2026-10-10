#include "store.h"
#include "paths.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sqlite3.h>
#include "dates.h"
#include "workspace.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include "windows_command.h"
#endif

// 使用真实 SQLite 文件验证持久化，不依赖 assert（Release 不会跳过检查）。
static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("litetodo-store-test-" + std::to_string(now_seconds()));
    std::filesystem::create_directories(root);
    const auto path = root / std::filesystem::u8path("中文任务.sqlite3");
    int checks = 0;
    try {
        long long first = 0;
        // 模拟上一版真实数据，验证升级后仍能读取，而不是仅检查空数据库。
        const auto legacy = root / "legacy.sqlite3";
        sqlite3* old=nullptr;
        require(sqlite3_open(legacy.u8string().c_str(), &old)==SQLITE_OK, "Create legacy database");
        require(sqlite3_exec(old,"CREATE TABLE tasks(id INTEGER PRIMARY KEY,title TEXT NOT NULL,note TEXT NOT NULL DEFAULT '',done INTEGER NOT NULL DEFAULT 0,remind_at INTEGER NOT NULL DEFAULT 0,notified INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL); INSERT INTO tasks VALUES(1,'legacy','note',0,0,0,1); PRAGMA user_version=1;",nullptr,nullptr,nullptr)==SQLITE_OK,"Seed legacy data");
        sqlite3_close(old);
        { Store migrated(legacy); require(migrated.list().at(0).title=="legacy", "Migration preserves task"); ++checks; }
        sqlite3_open(legacy.u8string().c_str(), &old);
        sqlite3_stmt* version=nullptr;
        sqlite3_prepare_v2(old,"PRAGMA user_version",-1,&version,nullptr); sqlite3_step(version);
        const int schema=sqlite3_column_int(version,0); sqlite3_finalize(version); sqlite3_close(old);
        require(schema==5,"Shelved task migration to version 5"); ++checks;
        const auto legacy2=root/"legacy-v2.sqlite3";
        require(sqlite3_open(legacy2.u8string().c_str(),&old)==SQLITE_OK,"Create v2 database");
        require(sqlite3_exec(old,"CREATE TABLE tasks(id INTEGER PRIMARY KEY,title TEXT NOT NULL,note TEXT NOT NULL DEFAULT '',done INTEGER NOT NULL DEFAULT 0,remind_at INTEGER NOT NULL DEFAULT 0,notified INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL,in_progress INTEGER NOT NULL DEFAULT 0,urgent INTEGER NOT NULL DEFAULT 0); INSERT INTO tasks VALUES(1,'v2 task','details',0,1900000000,1,1,1,1); PRAGMA user_version=2;",nullptr,nullptr,nullptr)==SQLITE_OK,"Seed v2 running reminder");sqlite3_close(old);
        {Store migrated(legacy2);auto task=migrated.list().at(0);require(task.in_progress&&task.urgent&&task.remind_at==1900000000&&task.due_at==0,"v2 migration preserves state and independent deadline");++checks;}
        bool prior_backup=false;for(const auto& entry:std::filesystem::directory_iterator(root))if(entry.path().filename().u8string().find("legacy-v2.sqlite3.pre-v5-")==0){
            sqlite3* snapshot=nullptr;require(sqlite3_open_v2(entry.path().u8string().c_str(),&snapshot,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK,"Read pre-upgrade snapshot");sqlite3_stmt* statement=nullptr;sqlite3_prepare_v2(snapshot,"PRAGMA user_version",-1,&statement,nullptr);sqlite3_step(statement);prior_backup=sqlite3_column_int(statement,0)==2;sqlite3_finalize(statement);sqlite3_close(snapshot);
        }require(prior_backup,"Backup exists before schema upgrade");++checks;
#ifdef _WIN32
        const std::wstring executable=L"C:\\Program Files\\轻待办\\LiteTodo.exe",directory=L"D:\\研发 data\\";
        auto command=autostart_command(executable,directory);int count=0;auto parsed=CommandLineToArgvW(command.c_str(),&count);
        bool correct=parsed&&count==3&&parsed[0]==executable&&std::wstring(parsed[1])==L"--data-dir"&&parsed[2]==directory;if(parsed)LocalFree(parsed);
        require(correct,"System argv parser preserves Unicode, spaces and trailing slash in autostart data path");++checks;
#endif
        {
            Store store(path);
            require(store.list().empty(), "Fresh database must be empty"); ++checks;
            first = store.add(u8"复核 RS485 '参数' 🚀", u8"备注\n第二行", now_seconds()+3600);
            store.add("100% coverage _query", "", 0);
            require(store.list().size()==2, "Creation"); ++checks;
            require(store.list(u8"复核").size()==1, "Chinese search"); ++checks;
            require(store.list("%").size()==1, "Literal percent search"); ++checks;
            require(store.list("_").size()==1, "Literal underscore search"); ++checks;
            bool rejected=false;
            try { store.add(" \t\n", "", 0); } catch (...) { rejected=true; }
            require(rejected, "Reject blank title"); ++checks;
            store.set_done(first, true);
            require(store.list(u8"复核")[0].done, "Completion"); ++checks;
            store.set_done(first, false);
            require(!store.list(u8"复核")[0].done, "Undo completion"); ++checks;
            store.set_progress(first,true);
            require(store.list("",3).size()==1&&store.list("",1).size()==1,"Started task changes category"); ++checks;
            store.set_urgent(first,true);
            require(store.list().front().id==first&&store.list().front().urgent,"Urgent task sorted first"); ++checks;
            store.set_done(first,true);
            require(!store.list(u8"复核")[0].in_progress,"Completion clears running state"); ++checks;
            bool completed_rejected=false;
            try{store.set_progress(first,true);}catch(...){completed_rejected=true;}
            require(completed_rejected,"Cannot start completed task"); ++checks;
            store.set_done(first,false);
            store.set_urgent(first,false);
            store.update(first,u8"复核 RS485 '参数' 🚀",u8"备注\n第二行",now_seconds()+3600);
            require(store.list(u8"复核")[0].note==u8"备注\n第二行","Edit task persists details"); ++checks;
            bool invalid_edit=false;
            try{store.update(first," ","",0);}catch(...){invalid_edit=true;}
            require(invalid_edit&&store.list(u8"复核").size()==1,"Rejected edit preserves original"); ++checks;
            store.backup(root / "backup.sqlite3");
        }
        {
            Store reopened(path);
            require(reopened.list(u8"复核")[0].note==u8"备注\n第二行", "Reopen and UTF-8 note"); ++checks;
            Store backup(root / "backup.sqlite3");
            require(backup.list().size()==2, "Consistent backup"); ++checks;
            reopened.reschedule(first, now_seconds()-1);
            auto due=reopened.due(now_seconds());
            require(due.size()==1 && due[0].id==first, "Due reminder"); ++checks;
            reopened.mark_notified(first);
            require(reopened.due(now_seconds()).empty(), "No duplicate reminder"); ++checks;
            auto sent=reopened.list(u8"复核")[0];
            reopened.update(first,sent.title,sent.note,sent.remind_at);
            require(reopened.due(now_seconds()).empty(),"Text edit does not resend reminder"); ++checks;
            reopened.reschedule(first,now_seconds()-2);
            auto old_time=reopened.list(u8"复核")[0].remind_at;
            reopened.reschedule(first,old_time+1);
            reopened.confirm_notification(first,old_time);
            require(reopened.due(now_seconds()).size()==1,"Stale notification confirmation cannot consume rescheduled reminder"); ++checks;
            reopened.confirm_notification(first,old_time+1);
            require(reopened.due(now_seconds()).empty(),"Matching notification confirmation consumes reminder"); ++checks;
            reopened.reschedule(first, now_seconds()-1);
            require(reopened.due(now_seconds()).size()==1, "Reschedule resets reminder"); ++checks;
            reopened.set_progress(first,true);reopened.set_shelved(first,true);
            require(reopened.list("",4).size()==1&&reopened.due(now_seconds()).empty()&&reopened.next_due()==0&&!reopened.list("",4)[0].in_progress,"Shelving persists and pauses reminders");++checks;
            reopened.confirm_notification(first,reopened.list("",4)[0].remind_at);
            reopened.set_shelved(first,false);require(reopened.due(now_seconds()).size()==1,"Shelved notification cannot consume reminder; resume restores it");++checks;
            reopened.set_shelved(first,true);reopened.set_progress(first,true);require(!reopened.list("",3)[0].shelved,"Starting a shelved task resumes it");++checks;
            reopened.set_shelved(first,true);auto held=reopened.list("",4)[0];reopened.save_task(held,{});require(reopened.list("",4).size()==1,"Editor save preserves shelved state");++checks;
            {Store again(path);require(again.list("",4).size()==1,"Shelved state survives restart");++checks;}
            reopened.set_done(first, true);
            require(reopened.due(now_seconds()).empty(), "Completed task not reminded"); ++checks;
            reopened.remove(first);
            require(reopened.list().size()==1, "Deletion"); ++checks;
            Task project;project.title=u8"雷达液位计设计及完工";project.source=u8"启电研发";project.category=u8"工作";project.due_at=day_after(3);project.remind_at=day_after(1)+9*3600;
            auto parent=reopened.save_task(project,{{0,u8"确认接口",false},{0,u8"绘制原理图",false}});
            auto children=reopened.children(parent);
            require(children.size()==2&&reopened.list(u8"启电研发")[0].due_at==project.due_at,"Separate due date, source and children");++checks;
            bool blocked=false;try{reopened.set_done(parent,true);}catch(...){blocked=true;}
            require(blocked&&!reopened.list(u8"雷达")[0].done,"Incomplete children block parent completion");++checks;
            reopened.set_child_done(children[0].id,true);reopened.set_child_done(children[1].id,true);reopened.set_done(parent,true);
            require(reopened.list(u8"雷达")[0].child_done==2&&reopened.list(u8"雷达")[0].completed_at>0,"Complete children and parent progress");++checks;
            reopened.set_child_done(children[0].id,false);
            require(!reopened.list(u8"雷达")[0].done,"Reopening child reopens completed parent");++checks;
            auto draft=reopened.list(u8"雷达")[0];auto child_draft=reopened.children(parent);child_draft[0].title=u8"确认供电接口";
            require(reopened.children(parent)[0].title!=child_draft[0].title,"Unsaved draft does not modify database");++checks;
            reopened.save_task(draft,child_draft);
            require(reopened.children(parent)[0].id==children[0].id&&reopened.children(parent)[0].title==child_draft[0].title,"Child rename preserves identity");++checks;
            child_draft.push_back({children[0].id,"duplicate",true});bool bad_child=false;try{reopened.save_task(draft,child_draft);}catch(...){bad_child=true;}
            require(bad_child&&reopened.children(parent).size()==2,"Invalid child edit leaves original intact");++checks;
            reopened.set_setting("close_to_tray","0");require(reopened.setting("close_to_tray")=="0","Settings persistence");++checks;
            reopened.remove(parent);require(reopened.children(parent).empty(),"Parent deletion cascades children");++checks;
            require(date_text(parse_date("2024-02-29"))=="2024-02-29","Leap day roundtrip");++checks;
            bool bad_date=false;try{parse_date("2026-02-29");}catch(...){bad_date=true;}require(bad_date,"Reject non-existent date");++checks;
            require(date_text(parse_date("2026-12-31 23:59",true),true)=="2026-12-31 23:59","Reminder minute roundtrip");++checks;
        }
        {Store settings_reopened(path);require(settings_reopened.setting("close_to_tray")=="0","Settings persist across database reopen");++checks;
            auto safe=root/"restore-source.sqlite3";settings_reopened.backup(safe);auto before=settings_reopened.list().size();settings_reopened.add("temporary change","",0);settings_reopened.restore_from(safe);require(settings_reopened.list().size()==before,"Restore replaces tasks from validated snapshot");++checks;
            auto invalid=root/"invalid.sqlite3";{std::ofstream out(invalid);out<<"not a database";}bool rejected=false;try{settings_reopened.restore_from(invalid);}catch(...){rejected=true;}require(rejected&&settings_reopened.list().size()==before,"Invalid restore leaves live database unchanged");++checks;
            Task with_file;with_file.title="attachment task";std::vector<Attachment> files={{"manual.pdf","D:/test/manual.pdf"}};auto id=settings_reopened.save_task(with_file,{},&files);require(settings_reopened.attachments(id).size()==1&&settings_reopened.list("attachment")[0].attachment_count==1,"Attachments saved with parent transaction");++checks;
            auto relocated=root/"moved.sqlite3";settings_reopened.relocate(relocated);settings_reopened.add("new location task","",0);{Store original(path);require(original.list("new location").empty(),"Relocation preserves original database");++checks;}require(settings_reopened.list("new location").size()==1,"Relocated store remains writable");++checks;
        }
        {Store batch(root/"batch.sqlite3");auto tasks=organize_text(u8"- 今天加急复核\n1. 明天提交\n后天测试\n\n普通事项");require(tasks.size()==4&&tasks[0].urgent&&tasks[0].due_at==day_after(0)&&tasks[1].due_at==day_after(1)&&tasks[2].due_at==day_after(2),"Local organizer dates, urgency and list markers");++checks;
            batch.add_many(tasks);require(batch.list().size()==4&&batch.list(u8"复核")[0].urgent,"Batch preserves parsed fields");++checks;
            tasks.push_back(Task{});bool invalid=false;try{batch.add_many(tasks);}catch(...){invalid=true;}require(invalid&&batch.list().size()==4,"Batch validation cannot partially write");++checks;
            auto source=root/u8"中文 附件.txt";{std::ofstream out(source);out<<"original";}auto dropped=drop_files(source.u8string()+"\n"+(root/"missing").u8string());require(dropped.size()==1,"Drop accepts existing absolute files only");++checks;
            std::vector<std::filesystem::path> created;auto files=copy_attachments({},dropped,root/"attachments",created);require(files.size()==1&&created.size()==1&&std::filesystem::exists(source)&&std::filesystem::exists(created[0]),"Attachment copies preserve source");++checks;
            auto folder=root/"daily";std::filesystem::create_directories(folder);for(int i=1;i<=9;++i){std::ofstream out(folder/("auto-2020-01-0"+std::to_string(i)+".sqlite3"));out<<"old";}auto manual=folder/"manual-keep.sqlite3";{std::ofstream out(manual);out<<"keep";}auto unrelated=folder/"auto-invalid.sqlite3";{std::ofstream out(unrelated);out<<"keep";}daily_backup(batch,folder);int count=0;for(auto& item:std::filesystem::directory_iterator(folder))if(item.path().filename().u8string().size()==23)++count;require(count==7&&std::filesystem::exists(manual)&&std::filesystem::exists(unrelated),"Daily retention only removes seven-day managed snapshots");++checks;
            auto anchor=root/"anchor",destination=root/"destination";std::filesystem::create_directories(destination);remember_data_directory(anchor,destination);require(resolve_data_directory(anchor)==std::filesystem::absolute(destination),"Directory location persists");++checks;
            auto malformed=root/"malformed.sqlite3";batch.backup(malformed);sqlite3* broken=nullptr;sqlite3_open(malformed.u8string().c_str(),&broken);sqlite3_exec(broken,"DROP TABLE attachments;CREATE TABLE attachments(task_id INTEGER);",nullptr,nullptr,nullptr);sqlite3_close(broken);bool rejected=false;try{batch.restore_from(malformed);}catch(...){rejected=true;}require(rejected&&batch.list().size()==4,"Import rejects incomplete attachment schema before replacing data");++checks;
            auto unrelated_db=root/"unrelated.sqlite3";sqlite3_open(unrelated_db.u8string().c_str(),&broken);sqlite3_exec(broken,"CREATE TABLE unrelated(value TEXT);",nullptr,nullptr,nullptr);sqlite3_close(broken);rejected=false;try{batch.restore_from(unrelated_db);}catch(...){rejected=true;}require(rejected&&batch.list().size()==4,"Import rejects unrelated valid SQLite database");++checks;
            auto version3=root/"v3.sqlite3";batch.backup(version3);sqlite3_open(version3.u8string().c_str(),&broken);sqlite3_exec(broken,"DROP TABLE attachments;ALTER TABLE tasks DROP COLUMN shelved;PRAGMA user_version=3;",nullptr,nullptr,nullptr);sqlite3_close(broken);{Store upgraded(version3);require(upgraded.list().size()==4&&upgraded.attachments(upgraded.list()[0].id).empty(),"Version 3 upgrade preserves tasks and creates attachment table");++checks;}
            auto failing=root/"failed-relocate.sqlite3";bool failed=false;try{batch.relocate(failing,[]{throw std::runtime_error("pointer rejected");});}catch(...){failed=true;}batch.add("original remains writable","",0);{Store original(root/"batch.sqlite3");require(failed&&original.list("original remains").size()==1,"Failed directory publication keeps live original");++checks;}
        }
        {Store many(root/"many-reminders.sqlite3");for(int i=0;i<21;++i)many.add("due "+std::to_string(i),"",now_seconds()-1);auto first_batch=many.due(now_seconds());std::vector<long long> queued;for(const auto& t:first_batch)queued.push_back(t.id);auto second_batch=many.due(now_seconds(),queued);require(first_batch.size()==20&&second_batch.size()==1,"Queued reminders do not starve the 21st due task");++checks;queued.push_back(second_batch[0].id);require(many.next_due(queued)==0&&many.due(now_seconds(),queued).empty(),"Queued reminders do not keep polling or repeat");++checks;}
        std::filesystem::remove_all(root);
        std::cout << checks << " storage checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\nTest files: " << root << '\n';
        return 1;
    }
}
