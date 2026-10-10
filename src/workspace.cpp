#include "workspace.h"
#include "dates.h"
#include <algorithm>
#include <sstream>
#include <chrono>
#include <stdexcept>
namespace {
std::string stripped(std::string text){auto first=text.find_first_not_of(" \t\r\n");return first==std::string::npos?"":text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);}
int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
}
std::vector<Task> organize_text(const std::string& text){
    if(text.size()>65536)throw std::runtime_error(u8"整理内容不能超过 64 KB");
    std::vector<Task> tasks;std::istringstream input(text);std::string line;
    while(std::getline(input,line)){line=stripped(line);if(line.empty())continue;if(line.rfind("- ",0)==0||line.rfind("* ",0)==0)line=line.substr(2);if(line.rfind("[ ] ",0)==0)line=line.substr(4);if(line.rfind("[x] ",0)==0)line=line.substr(4);
        auto dot=line.find_first_of(".)");if(dot!=std::string::npos&&dot<4&&line.size()>dot+1&&line[dot+1]==' '&&std::all_of(line.begin(),line.begin()+dot,[](char c){return c>='0'&&c<='9';}))line=line.substr(dot+2);
        line=stripped(line);if(line.empty())continue;Task task;task.title=line;if(line.size()>2048)throw std::runtime_error(u8"单条任务内容过长");if(line.find(u8"后天")!=std::string::npos)task.due_at=day_after(2);else if(line.find(u8"明天")!=std::string::npos)task.due_at=day_after(1);else if(line.find(u8"今天")!=std::string::npos)task.due_at=day_after(0);
        if(line.find(u8"加急")!=std::string::npos)task.urgent=true;
        tasks.push_back(task);if(tasks.size()>200)throw std::runtime_error(u8"一次最多整理 200 项任务");
    }return tasks;
}
std::vector<std::filesystem::path> drop_files(const std::string& text){
    std::vector<std::filesystem::path> files;std::istringstream input(text);std::string line;while(std::getline(input,line)){line=stripped(line);if(line.empty()||line[0]=='#')continue;bool uri=line.rfind("file://",0)==0;if(uri){line=line.substr(7);if(line.rfind("localhost/",0)==0)line=line.substr(9);
#ifdef _WIN32
        if(line.size()>2&&line[0]=='/'&&line[2]==':')line.erase(0,1);
#endif
        std::string decoded;for(size_t i=0;i<line.size();++i){if(line[i]=='%'&&i+2<line.size()&&hex(line[i+1])>=0&&hex(line[i+2])>=0){decoded+=static_cast<char>(16*hex(line[i+1])+hex(line[i+2]));i+=2;}else decoded+=line[i];}line=decoded;}
        if(line.find('\0')!=std::string::npos)continue;
        auto file=std::filesystem::u8path(line);if(file.is_absolute()&&std::filesystem::is_regular_file(file)){files.push_back(file);if(files.size()>40)throw std::runtime_error(u8"一次最多拖入 40 个文件");}}
    return files;
}
std::filesystem::path daily_backup(Store& store,const std::filesystem::path& directory){
    std::filesystem::create_directories(directory);auto destination=directory/("auto-"+date_text(now_seconds())+".sqlite3");if(!std::filesystem::exists(destination))store.backup(destination);
    // 只修剪本软件生成的精确命名文件，手工备份、导入前快照及其他文件保留。
    std::vector<std::filesystem::path> files;for(const auto& item:std::filesystem::directory_iterator(directory)){auto name=item.path().filename().u8string();if(item.is_regular_file()&&name.size()==23&&name.rfind("auto-",0)==0&&name.substr(15)==".sqlite3"){try{parse_date(name.substr(5,10));files.push_back(item.path());}catch(...){}}}
    std::sort(files.begin(),files.end());while(files.size()>7){std::filesystem::remove(files.front());files.erase(files.begin());}return destination;
}
std::vector<Attachment> copy_attachments(const std::vector<Attachment>& existing,const std::vector<std::filesystem::path>& incoming,const std::filesystem::path& folder,std::vector<std::filesystem::path>& created){
    if(existing.size()+incoming.size()>40)throw std::runtime_error(u8"每项任务最多 40 个附件");
    auto result=existing;if(incoming.empty())return result;
    std::filesystem::create_directories(folder);
    for(const auto& source:incoming){
        if(!std::filesystem::is_regular_file(source)||std::filesystem::file_size(source)>50'000'000)throw std::runtime_error(u8"附件不是文件或超过 50 MB");
        // 为每次复制独占一个目录，失败清理不会碰到其他任务的文件。
        auto base=std::filesystem::absolute(folder);std::filesystem::path owned;
        for(int attempt=0;attempt<100;++attempt){auto candidate=base/(".attachment-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(attempt));if(std::filesystem::create_directory(candidate)){owned=candidate;break;}}
        if(owned.empty())throw std::runtime_error(u8"无法创建附件目录");
        auto file=owned/source.filename();
        try{std::filesystem::copy_file(source,file);}catch(...){std::error_code ignored;std::filesystem::remove(file,ignored);std::filesystem::remove(owned,ignored);throw;}
        created.push_back(file);result.push_back({source.filename().u8string(),file.u8string()});
    }return result;
}
