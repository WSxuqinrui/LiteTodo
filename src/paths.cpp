#include "paths.h"
#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <set>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

std::filesystem::path default_data_directory(){
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,0,path)))
        throw std::runtime_error("Cannot locate user data directory");
    return std::filesystem::path(path)/L"LiteTodoPrototype";
#else
    const char* home=std::getenv("HOME");
    if(!home||!*home)throw std::runtime_error("HOME is unavailable; use --data-dir");
#ifdef __APPLE__
    return std::filesystem::path(home)/"Library"/"Application Support"/"LiteTodoPrototype";
#else
    const char* xdg=std::getenv("XDG_DATA_HOME");
    if(xdg&&*xdg&&std::filesystem::path(xdg).is_absolute())return std::filesystem::path(xdg)/"LiteTodoPrototype";
    return std::filesystem::path(home)/".local"/"share"/"LiteTodoPrototype";
#endif
#endif
}
std::filesystem::path resolve_data_directory(const std::filesystem::path& anchor){
    auto current=std::filesystem::absolute(anchor).lexically_normal();std::set<std::filesystem::path> seen;
    for(int i=0;i<8;++i){if(!seen.insert(current).second)throw std::runtime_error(u8"数据目录设置存在循环");std::ifstream input(current/".litetodo-location",std::ios::binary);if(!input)return current;std::string text;std::getline(input,text);auto next=std::filesystem::u8path(text);if(!next.is_absolute())throw std::runtime_error(u8"数据位置设置需要绝对路径");current=next.lexically_normal();}throw std::runtime_error(u8"数据目录跳转过多");
}
void remember_data_directory(const std::filesystem::path& anchor,const std::filesystem::path& destination){
    std::filesystem::create_directories(anchor);auto target=anchor/".litetodo-location",temporary=anchor/".litetodo-location.new";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<std::filesystem::absolute(destination).u8string()<<'\n';if(!output)throw std::runtime_error(u8"数据位置未能保存");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(u8"数据位置未能更新");
#else
    std::filesystem::rename(temporary,target);
#endif
}
