#pragma once
#include "store.h"
#include "icons.h"
#include "theme.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_Scroll.H>
#include <functional>
#include <array>
#include <map>
#include <set>
namespace Style {
void fonts();
void input(Fl_Input* field);
Fl_Button* button(int x,int y,int w,int h,const char* text,bool primary=false);
}
class TextLabel:public Fl_Box {public:using Fl_Box::Fl_Box;void draw()override;};
class KeyInput:public Fl_Input {
    const char* hint_;
public:
    std::function<bool(int,int)> shortcut;
    KeyInput(int x,int y,int w,int h,const char* hint="");
    int handle(int event)override;
    void draw()override;
};
class KeyNote:public Fl_Multiline_Input {
public:
    std::function<bool(int,int)> shortcut;
    using Fl_Multiline_Input::Fl_Multiline_Input;
    int handle(int event)override;
};
// 保留 FLTK 对输入框、滚轮和内容布局的处理，统一右侧滚动条外观及命中区。
class SoftScroll:public Fl_Scroll {
    bool hover_=false,dragging_=false;
    int drag_offset_=0;
    void drag_to(int pointer_y);
public:
    struct Thumb {int x,y,w,h,track_y,track_h;};
    SoftScroll(int x,int y,int w,int h);
    Thumb thumb()const;
    void draw()override;
    int handle(int event)override;
};
class AppWindow:public Fl_Double_Window {
public:
    std::function<void()> layout;
    std::function<void(int,int)> before_pointer;
    std::function<bool(int,int)> shortcut;
    AppWindow(int w,int h);
    void resize(int x,int y,int w,int h)override;
    int handle(int event)override;
};
enum class TaskAction {Edit,Urgent,Start,Defer,Complete,Shelve,Delete};
class TaskBoard:public Fl_Widget {
    long long pressed_child_=0;
    int press_x_=0,press_y_=0;
    bool pressed_cancelled_=false;
public:
    struct Card {int x,y,w,h;size_t index;};
    struct ChildRow {int x,y,w,h;Subtask child;};
    std::vector<Task> tasks;
    std::vector<Card> cards;
    std::function<void(Task,TaskAction)> action;
    std::function<std::vector<Subtask>(long long)> load_children;
    std::function<void(long long,bool)> complete_child;
    std::function<void(const char*)> error;
    std::set<long long> expanded;
    std::map<long long,std::vector<Subtask>> child_cache;
    bool board=false,full=false,dense=false,interactive=true;
    int offset=0,total=0,hover=-1;
    long long selected=0;
    long long menu_id=0;
    int menu_x=0,menu_y=0,menu_hover=-1;
    void open_menu(size_t card);
    bool menu_bounds_valid()const;
    void toggle_children(long long parent);
    void refresh_children();
    std::vector<ChildRow> child_rows(const Card& card)const;
    TaskBoard();
    void rebuild();
    void resize(int x,int y,int w,int h)override;
    void draw()override;
    int handle(int event)override;
};
class ChildList:public Fl_Widget {
    int pressed_=-1,pressed_action_=-1,press_x_=0,press_y_=0;
    bool cancelled_=false;
public:
    std::vector<Subtask> items;
    std::function<void(size_t,int)> action;
    int offset=0;
    ChildList();
    void normalize();
    void resize(int x,int y,int w,int h)override;
    void draw()override;
    int handle(int event)override;
};
class Toggle:public Fl_Button {
public:
    Toggle();
    void draw()override;
    int handle(int event)override;
};
class Ball:public Fl_Double_Window {
    int anchor_x=0,anchor_y=0,start_x=0,start_y=0;
    int dock_side=0,dock_x=0,dock_y=0;
    bool moved=false;
    std::array<unsigned char,800> mask{};
    Fl_Bitmap shape_mask;
    static void hover_timeout(void* p);
public:
    enum class Mood {Relaxed,Happy,Working,Tired,Urgent};
    int count=0;
    int style=1,accent=0,urgent_count=0,working_count=0;
    Mood mood()const;
    bool wire=false,edge=false,hover_expand=false,double_click=false,chat=false,accept_drop=true;
    std::function<void()> click,right_click,position_saved;
    std::function<void()> hover_open;
    std::function<void(const std::string&)> dropped;
    Ball();
    ~Ball();
    void show()override;
    void reset_position();
    void configure();
    void dock(bool collapse);
    void draw()override;
    int handle(int event)override;
};
