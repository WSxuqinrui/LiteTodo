#include "store.h"
#include "paths.h"
#include "ui.h"
#include "instance.h"
#include <FL/fl_ask.H>
#include <fstream>
#include <iostream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

int main(int argc,char** argv){
    try{
        bool smoke=false,portable=false;
        std::filesystem::path data;
        std::vector<std::string> arguments;
#ifdef _WIN32
        // Windows CRT 的窄 argv 不保证是 UTF-8，直接从 Unicode 命令行转换。
        int wide_count=0;auto wide_args=CommandLineToArgvW(GetCommandLineW(),&wide_count);
        if(!wide_args)throw std::runtime_error("Cannot read command line");
        for(int i=1;i<wide_count;++i){int n=WideCharToMultiByte(CP_UTF8,0,wide_args[i],-1,nullptr,0,nullptr,nullptr);std::string value(static_cast<size_t>(n),'\0');WideCharToMultiByte(CP_UTF8,0,wide_args[i],-1,value.data(),n,nullptr,nullptr);value.resize(n-1);arguments.push_back(value);}LocalFree(wide_args);
        (void)argc;(void)argv;
#else
        for(int i=1;i<argc;++i)arguments.emplace_back(argv[i]);
#endif
        for(size_t i=0;i<arguments.size();++i){const auto& arg=arguments[i];if(arg=="--data-dir"&&i+1<arguments.size())data=std::filesystem::u8path(arguments[++i]);else if(arg=="--ui-smoke")smoke=true;else if(arg=="--portable")portable=true;else throw std::runtime_error("Usage: LiteTodo [--portable] [--data-dir PATH] [--ui-smoke]");}
        if(smoke&&data.empty())throw std::runtime_error("--ui-smoke requires an isolated --data-dir");
        if(data.empty()){
            if(portable){
#ifdef _WIN32
                wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n>=32768)throw std::runtime_error("Cannot locate executable");data=std::filesystem::path(exe).parent_path()/"data";
#else
                throw std::runtime_error("Use --data-dir for explicit portable data on macOS/Linux");
#endif
            }else data=default_data_directory();
        }
        auto anchor=std::filesystem::absolute(data);data=resolve_data_directory(anchor);std::filesystem::create_directories(data);
        InstanceLock instance(data);
        if(!instance.owns()){instance.request_activation();return 0;}
        Store store(data/"tasks.sqlite3");Ui ui(store,data,smoke,anchor,&instance);return ui.run();
    }catch(const std::exception& e){
        std::cerr<<e.what()<<'\n';fl_alert("%s",e.what());return 1;
    }
}
