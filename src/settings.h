#pragma once
#include "store.h"
#include "widgets.h"
#include <FL/Fl_Scroll.H>
#include <memory>
#include <vector>
class SettingsPanel:public SoftScroll {
    struct Row;
    Store& store_;std::vector<std::unique_ptr<Row>> rows_;
    bool recording_=false;std::filesystem::path actual_data_;
    void add_toggle(int section,const char* title,const char* note,const char* key,const char* fallback);
    void add_choices(int section,const char* title,const char* note,const char* key,const std::vector<std::string>& choices,bool colors=false);
    void add_action(int section,const char* title,const char* note,const std::vector<std::pair<std::string,std::string>>& actions);
    void add_path(int section,const char* title,const char* note,const char* key,const std::vector<std::pair<std::string,std::string>>& actions);
    static void callback(Fl_Widget*,void*);
public:
    int section=-1;
    Fl_Button* landing[6]{};
    std::function<void(const std::string&)> changed,action;
    std::function<void(const char*)> message;
    SettingsPanel(Store& store,const std::filesystem::path& data);
    ~SettingsPanel()override;
    void choose(int section);
    void arrange(int x,int y,int w,int h);
    void scroll_to(int x,int y);
    void reload();
    void paths(const std::filesystem::path& data);
    std::string field(const std::string& key);
    void field(const std::string& key,const std::string& value);
    bool capture(int key,int state);
    bool recording()const{return recording_;}
};
