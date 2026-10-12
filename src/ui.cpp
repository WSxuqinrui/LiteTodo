#include "ui.h"
#include "widgets.h"
#include "dates.h"
#include "platform.h"
#include "settings.h"
#include "workspace.h"
#include "paths.h"
#include "instance.h"
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <FL/platform.H>
#endif

using namespace Style;
namespace {
enum class Page {Home,Edit,Calendar,Reminder,Settings,Confirm,Organize,Alert};
Fl_Box* label(const char* text,int size=12,Fl_Color color=ink){auto* b=new TextLabel(0,0,1,1);b->copy_label(text);b->labelsize(size);b->labelcolor(color);b->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);return b;}
Fl_Group* group(){auto* g=new Fl_Group(0,0,1,1);g->resizable(nullptr);return g;}
void place(Fl_Widget* widget,int x,int y,int w,int h){widget->resize(x,y,std::max(1,w),std::max(1,h));}
std::string trim(std::string value){auto first=value.find_first_not_of(" \t\r\n");return first==std::string::npos?"":value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);}
const char* names[]={u8"总览",u8"今日",u8"未开始",u8"进行中",u8"已完成",u8"搁置"};
const char* setting_keys[]={"autostart","start_hidden","close_to_tray","sort_due","floating","notifications","dense","auto_backup"};
const char* setting_defaults[]={"0","0","1","1","1","1","0","0"};
}

// 所有业务页面都是同一主窗口中的 Group，只有桌面浮球是独立窗口。
struct Ui::Impl {
    Store& store;std::filesystem::path data,anchor;
    InstanceLock* instance=nullptr;
    SettingsPanel* settings_panel=nullptr;
    KeyNote* organize_input=nullptr;TaskBoard* organize_preview=nullptr;Fl_Button *organize_back=nullptr,*organize_analyze=nullptr,*organize_save=nullptr,*organize_paste=nullptr;Fl_Box* organize_heading=nullptr;
    Fl_Box* attachment_label=nullptr;ChildList* attachment_list=nullptr;KeyInput* attachment_input=nullptr;Fl_Button* attachment_add=nullptr;std::vector<Attachment> draft_files;std::vector<std::filesystem::path> pending_files;
    struct Delivery{long long id,at;bool accepted;};
    struct DeliveryQueue{std::mutex mutex;std::vector<Delivery> results;};
    std::shared_ptr<DeliveryQueue> deliveries=std::make_shared<DeliveryQueue>();
    std::map<long long,long long> alerts;long long alert_id=0;std::string alert_reason;
    Fl_Box *alert_heading=nullptr,*alert_text=nullptr,*alert_info=nullptr;Fl_Button *alert_ack=nullptr,*alert_snooze=nullptr,*alert_view=nullptr,*alert_settings=nullptr;IconButton* alert_indicator=nullptr;
    std::set<long long> pending_notifications;std::map<long long,long long> retry_after;
    bool smoke=false,quit=false,console=false,draft_active=false,board_view=true,pinned=false;
    int result=0,filter=0,settings_section=-1,child_edit=-1,calendar_year=0,calendar_month=0;
    std::string organized_source;
    Page page=Page::Home;std::vector<Page> history;Task draft;
    std::unique_ptr<AppWindow> window;std::unique_ptr<Ball> ball;
    Fl_Group* sidebar=nullptr;Fl_Group* pages[8]{};Fl_Group* settings_landing=nullptr;Fl_Group* settings_detail=nullptr;
    SoftScroll* edit_scroll=nullptr;
    IconButton *logo=nullptr,*gear=nullptr,*pin=nullptr,*view_button=nullptr,*date_picker=nullptr;
    Fl_Box *brand=nullptr,*status=nullptr,*heading=nullptr,*date_label=nullptr,*summary=nullptr;
    Fl_Button *create_button=nullptr,*side_create=nullptr,*clear_category=nullptr,*resume=nullptr,*nav[6]{},*side_nav[6]{};
    Fl_Box *side_focus=nullptr,*side_category=nullptr;
    KeyInput *search=nullptr,*category_search=nullptr;
    TaskBoard* list=nullptr;
    Fl_Button *cancel_button=nullptr,*save_button=nullptr,*delete_button=nullptr,*due_chips[5]{},*set_reminder=nullptr,*add_child=nullptr;
    KeyNote *title=nullptr,*note=nullptr;KeyInput *source=nullptr,*category=nullptr,*child_input=nullptr;
    Fl_Box *edit_labels[5]{},*due_info=nullptr,*reminder_info=nullptr,*child_info=nullptr;
    ChildList* child_list=nullptr;
    Fl_Box *calendar_heading=nullptr,*weekdays[7]{};Fl_Button *calendar_back=nullptr,*previous_month=nullptr,*next_month=nullptr,*clear_date=nullptr,*cells[42]{};long long cell_dates[42]{};
    Fl_Box* reminder_heading=nullptr;Fl_Button *reminder_back=nullptr,*presets[5]{},*apply_reminder=nullptr;KeyInput* reminder_input=nullptr;
    Fl_Box *settings_heading=nullptr,*toggle_titles[8]{},*toggle_notes[8]{},*data_info=nullptr;
    Fl_Button *settings_back=nullptr,*settings_rows[6]{},*test_notify=nullptr,*backup_button=nullptr,*exit_button=nullptr;
    Toggle* toggles[8]{};Fl_Button* startup_mode[2]{};Fl_Box* startup_label=nullptr;KeyInput* backup_path=nullptr;
    Fl_Box* confirm_text=nullptr;Fl_Button *confirm_cancel=nullptr,*confirm_accept=nullptr;std::function<void()> confirmed_action;

    Impl(Store& s,const std::filesystem::path& d,bool check,const std::filesystem::path& origin,InstanceLock* lock=nullptr):store(s),data(d),anchor(origin.empty()?d:origin),instance(lock),smoke(check){
        Fl::lock();fonts();palette(number("theme",0),number("accent",0),platform_system_dark());pinned=store.setting("pinned","0")=="1";console=store.setting("startup_mode","quick")=="console";board_view=store.setting("board_view","1")=="1";
        window=std::make_unique<AppWindow>(console?1120:500,780);window->begin();
        logo=new IconButton(10,9,36,36,Icon::Logo,u8"切换快捷面板 / 控制台");logo->callback([](Fl_Widget*,void* p){static_cast<Impl*>(p)->guard([&]{static_cast<Impl*>(p)->toggle_mode();});},this);
        brand=label(u8"轻待办",15);gear=new IconButton(0,0,76,36,Icon::Settings,u8"打开设置");gear->copy_label(u8"设置");gear->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->settings_panel->choose(-1);a->navigate(Page::Settings);},this);
        pin=new IconButton(0,0,76,36,Icon::Pin,u8"切换主窗口置顶");pin->copy_label(pinned?u8"已置顶":u8"置顶");pin->selected(pinned);pin->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->toggle_pin();});},this);
        status=label(u8"保存在本机 · 点击左上角切换控制台",11,muted);
        make_sidebar();make_home();make_editor();make_calendar();make_reminder();make_settings();make_confirm();make_organize();make_alert();window->end();
        window->before_pointer=[this](int x,int y){if(list->menu_id&&(x<list->x()||x>=list->x()+list->w()||y<list->y()||y>=list->y()+list->h())){list->menu_id=0;list->redraw();}};
        window->layout=[this]{layout();};window->shortcut=[this](int key,int state){return shortcut(key,state);};window->callback([](Fl_Widget*,void* p){static_cast<Impl*>(p)->close_main();},this);
        platform_window_icon(window.get());platform_init({[this]{show();},[this]{show();open_editor();},[this]{show();request_exit();},[this]{guard([&]{open_organize(true);});}});
        make_ball();refresh();visibility();layout();Fl::add_timeout(0.2,tick,this);if(instance)Fl::add_timeout(0.4,activation_tick,this);
    }
    ~Impl(){Fl::remove_timeout(tick,this);Fl::remove_timeout(activation_tick,this);Fl::remove_timeout(smoke_tick,this);Fl::remove_timeout(smoke_finish,this);platform_shutdown();}
    static void activation_tick(void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{if(a->instance&&a->instance->consume_activation())a->show();});Fl::repeat_timeout(0.4,activation_tick,a);}
    template<class Action> void guard(Action action){try{action();}catch(const std::exception& e){message(e.what());}}
    void message(const char* text){status->copy_label(text);status->redraw();}
    bool flag(int i){return store.setting(setting_keys[i],setting_defaults[i])=="1";}
    int number(const char* key,int fallback){try{return std::stoi(store.setting(key,std::to_string(fallback)));}catch(...){return fallback;}}
    static void tab_callback(Fl_Widget* widget,void* p){auto* a=static_cast<Impl*>(p);for(int i=0;i<6;++i)if(widget==a->nav[i]||widget==a->side_nav[i])a->filter=i;a->search->value("");a->list->offset=0;a->navigate(Page::Home);a->guard([&]{a->refresh();});}
    void make_sidebar(){
        sidebar=group();side_create=button(0,0,1,1,u8"＋  新增待办");side_create->callback([](Fl_Widget*,void* p){static_cast<Impl*>(p)->open_editor();},this);
        side_focus=label(u8"焦点与状态",11,muted);for(int i=0;i<6;++i){side_nav[i]=button(0,0,1,1,names[i]);side_nav[i]->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);side_nav[i]->callback(tab_callback,this);}
        side_category=label(u8"分类筛选",11,muted);category_search=new KeyInput(0,0,1,1,u8"输入分类名称");input(category_search);category_search->when(FL_WHEN_CHANGED);category_search->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->refresh();});},this);
        clear_category=button(0,0,1,1,u8"显示全部分类");clear_category->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->category_search->value("");a->guard([&]{a->refresh();});},this);sidebar->end();
    }
    void make_home(){
        pages[0]=group();heading=label(u8"今天，做一点重要的事",21);date_label=label("",11,muted);summary=label("",11,muted);
        create_button=button(0,0,1,1,u8"＋ 新增",true);create_button->callback([](Fl_Widget*,void* p){static_cast<Impl*>(p)->open_editor();},this);
        search=new KeyInput(0,0,1,1,u8"搜索待办、备注或来源…");input(search);search->when(FL_WHEN_CHANGED);search->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->list->offset=0;a->guard([&]{a->refresh();});},this);
        view_button=new IconButton(0,0,1,1,Icon::Grid,u8"切换看板 / 列表");view_button->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->board_view=!a->board_view;a->store.set_setting("board_view",a->board_view?"1":"0");a->refresh();});},this);
        for(int i=0;i<6;++i){nav[i]=button(0,0,1,1,names[i]);nav[i]->labelsize(11);nav[i]->callback(tab_callback,this);}
        list=new TaskBoard;list->action=[this](Task t,TaskAction a){guard([&]{task_action(t,a);});};
        list->load_children=[this](long long parent){return store.children(parent);};
        list->error=[this](const char* text){message(text);};
        list->complete_child=[this](long long child,bool done){guard([&]{
            if(draft_active){navigate(Page::Edit);message(u8"请先保存或取消当前草稿");return;}
            store.set_child_done(child,done);refresh();reschedule_tick();message(done?u8"子任务已完成":u8"子任务已恢复");
        });};
        resume=button(0,0,1,1,u8"继续编辑草稿");resume->callback([](Fl_Widget*,void* p){static_cast<Impl*>(p)->navigate(Page::Edit);},this);pages[0]->end();
    }
    void make_editor(){
        pages[1]=group();cancel_button=button(0,0,1,1,u8"← 取消");cancel_button->callback([](Fl_Widget*,void* p){static_cast<Impl*>(p)->cancel_edit();},this);
        save_button=button(0,0,1,1,u8"保存",true);save_button->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->save_edit();});},this);
        edit_scroll=new SoftScroll(0,0,1,1);
        const char* labels[]={u8"任务内容",u8"来源",u8"交付时间",u8"备注",u8"子任务"};for(int i=0;i<5;++i)edit_labels[i]=label(labels[i],12,muted);
        title=new KeyNote(0,0,1,1);input(title);title->maximum_size(2048);title->shortcut=[this](int k,int s){return shortcut(k,s);};
        category=new KeyInput(0,0,1,1,u8"未分类（可直接输入）");input(category);category->maximum_size(128);
        source=new KeyInput(0,0,1,1,u8"客户或项目名称");input(source);source->maximum_size(512);
        const char* days[]={u8"今天",u8"明天",u8"后天",u8"+3天",u8"+7天"};for(int i=0;i<5;++i){due_chips[i]=button(0,0,1,1,days[i]);due_chips[i]->labelsize(11);due_chips[i]->callback([](Fl_Widget* w,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{const int days[]={0,1,2,3,7};for(int i=0;i<5;++i)if(w==a->due_chips[i])a->draft.due_at=day_after(days[i]);a->update_draft_labels();});},this);}
        date_picker=new IconButton(0,0,1,1,Icon::Calendar,u8"选择或清除交付日期");date_picker->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);auto t=local_parts(a->draft.due_at?a->draft.due_at:now_seconds());a->calendar_year=t.tm_year+1900;a->calendar_month=t.tm_mon+1;a->fill_calendar();a->navigate(Page::Calendar);},this);
        set_reminder=button(0,0,1,1,u8"设置提醒");set_reminder->labelsize(11);set_reminder->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->reminder_input->value(a->draft.remind_at?date_text(a->draft.remind_at,true).c_str():"");a->navigate(Page::Reminder);},this);
        due_info=label("",11,muted);reminder_info=label("",11,muted);
        note=new KeyNote(0,0,1,1);input(note);note->maximum_size(16384);note->shortcut=[this](int k,int s){return shortcut(k,s);};
        child_info=label("",11,muted);child_list=new ChildList;child_list->action=[this](size_t index,int action){if(index>=child_list->items.size())return;
            if(action==0){child_list->items[index].done=!child_list->items[index].done;if(!child_list->items[index].done&&draft.done){draft.done=false;draft.in_progress=false;}}
            else if(action==1){child_edit=static_cast<int>(index);child_input->value(child_list->items[index].title.c_str());child_input->take_focus();add_child->copy_label(u8"更新");}
            else{child_list->items.erase(child_list->items.begin()+static_cast<int>(index));child_edit=-1;child_input->value("");add_child->copy_label(u8"添加");}
            update_draft_labels();};
        child_input=new KeyInput(0,0,1,1,u8"输入子任务，回车添加");input(child_input);child_input->shortcut=[this](int k,int s){if(k==FL_Enter&&!(s&(FL_CTRL|FL_ALT|FL_SHIFT))){guard([&]{save_child();});return true;}return shortcut(k,s);};
        add_child=button(0,0,1,1,u8"添加");add_child->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->save_child();});},this);
        attachment_label=label(u8"附件 · 完整路径或拖入文件",12,muted);attachment_list=new ChildList;attachment_list->action=[this](size_t index,int action){guard([&]{if(action==2){if(index<draft_files.size())draft_files.erase(draft_files.begin()+index);else pending_files.erase(pending_files.begin()+index-draft_files.size());update_files();}else if(index<draft_files.size())message(draft_files[index].path.c_str());else message(pending_files.at(index-draft_files.size()).u8string().c_str());});};
        attachment_input=new KeyInput(0,0,1,1,u8"输入文件的完整路径");input(attachment_input);attachment_add=button(0,0,1,1,u8"添加");attachment_add->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{auto path=std::filesystem::u8path(a->attachment_input->value());if(!path.is_absolute()||!std::filesystem::is_regular_file(path))throw std::runtime_error(u8"请输入已有文件的完整路径");a->pending_files.push_back(path);a->attachment_input->value("");a->update_files();});},this);
        delete_button=button(0,0,1,1,u8"删除此任务");delete_button->labelcolor(red);delete_button->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->confirm(u8"删除此任务及全部子任务？",u8"删除",[a,id=a->draft.id,token=a->draft.edit_token]{a->store.remove(id,token);a->cancel_edit();a->message(u8"任务已删除");});},this);edit_scroll->end();pages[1]->end();
    }
    void make_calendar(){
        pages[2]=group();calendar_back=button(0,0,1,1,u8"← 返回");calendar_back->callback(back_callback,this);calendar_heading=label("",21);
        previous_month=button(0,0,1,1,u8"‹ 上个月");next_month=button(0,0,1,1,u8"下个月 ›");for(auto* b:{previous_month,next_month})b->callback([](Fl_Widget* w,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{int m=a->calendar_month+(w==a->next_month?1:-1),y=a->calendar_year;if(m<1){m=12;--y;}if(m>12){m=1;++y;}calendar_time(y,m,1);a->calendar_month=m;a->calendar_year=y;a->fill_calendar();});},this);
        const char* days[]={u8"一",u8"二",u8"三",u8"四",u8"五",u8"六",u8"日"};for(int i=0;i<7;++i){weekdays[i]=label(days[i],12,muted);weekdays[i]->align(FL_ALIGN_CENTER);}
        for(int i=0;i<42;++i){cells[i]=button(0,0,1,1,"");cells[i]->callback([](Fl_Widget* w,void* p){auto* a=static_cast<Impl*>(p);for(int i=0;i<42;++i)if(a->cells[i]==w&&a->cell_dates[i])a->draft.due_at=a->cell_dates[i];a->update_draft_labels();a->back();},this);}
        clear_date=button(0,0,1,1,u8"不设交付日期");clear_date->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->draft.due_at=0;a->update_draft_labels();a->back();},this);pages[2]->end();
    }
    void make_reminder(){
        pages[3]=group();reminder_back=button(0,0,1,1,u8"← 返回");reminder_back->callback(back_callback,this);reminder_heading=label(u8"提醒这件事",21);
        const char* texts[]={u8"不设提醒",u8"15 秒后（快速测试）",u8"5 分钟后",u8"1 小时后",u8"交付日早上 9 点"};for(int i=0;i<5;++i){presets[i]=button(0,0,1,1,texts[i]);presets[i]->callback([](Fl_Widget* w,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{int i=0;while(i<5&&a->presets[i]!=w)++i;long long at=0;if(i==1)at=now_seconds()+15;if(i==2)at=now_seconds()+300;if(i==3)at=now_seconds()+3600;if(i==4){if(!a->draft.due_at)throw std::runtime_error(u8"请先设置交付日期");auto t=local_parts(a->draft.due_at);at=calendar_time(t.tm_year+1900,t.tm_mon+1,t.tm_mday,9); }a->draft.remind_at=at;a->update_draft_labels();a->back();});},this);}
        reminder_input=new KeyInput(0,0,1,1,u8"自定义：YYYY-MM-DD HH:MM");input(reminder_input);apply_reminder=button(0,0,1,1,u8"使用这个时间",true);apply_reminder->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->draft.remind_at=parse_date(a->reminder_input->value(),true);a->update_draft_labels();a->back();});},this);pages[3]->end();
    }
    void make_settings(){
        pages[4]=group();settings_back=button(0,0,1,1,u8"← 返回");settings_back->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);if(a->settings_panel->section>=0)a->settings_panel->choose(-1);else a->back();},this);settings_heading=label(u8"设置",21);
        settings_panel=new SettingsPanel(store,data);for(int i=0;i<6;++i)settings_rows[i]=settings_panel->landing[i];
        settings_panel->changed=[this](const std::string& key){if(key=="section"){settings_section=settings_panel->section;visibility();layout();}else apply_setting(key);};
        settings_panel->action=[this](const std::string& command){guard([&]{settings_action(command);});};settings_panel->message=[this](const char* text){message(text);};pages[4]->end();
    }
    void make_organize(){pages[6]=group();organize_back=button(0,0,1,1,u8"← 返回");organize_back->callback(back_callback,this);organize_heading=label(u8"本地文本整理 · 先预览，再保存",18);organize_input=new KeyNote(0,0,1,1);input(organize_input);organize_input->maximum_size(65536);organize_input->when(FL_WHEN_CHANGED);organize_input->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->organize_preview->tasks.clear();a->organize_preview->rebuild();a->organize_save->deactivate();},this);organize_paste=button(0,0,1,1,u8"粘贴文字");organize_paste->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);Fl::paste(*a->organize_input,1);},this);organize_analyze=button(0,0,1,1,u8"整理预览");organize_analyze->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->organize_preview->tasks=organize_text(a->organize_input->value());a->organized_source=a->organize_input->value();if(a->organize_preview->tasks.empty())a->organize_save->deactivate();else a->organize_save->activate();a->organize_preview->rebuild();a->message(u8"按行生成任务；请核对交付日期与加急标记");});},this);organize_save=button(0,0,1,1,u8"保存清单",true);organize_save->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{if(a->organized_source!=a->organize_input->value())throw std::runtime_error(u8"文字已修改，请重新整理预览");a->store.add_many(a->organize_preview->tasks);a->organize_preview->tasks.clear();a->organize_input->value("");a->navigate(Page::Home);a->refresh();a->message(u8"整理结果已保存到本机");});},this);organize_preview=new TaskBoard;organize_preview->interactive=false;organize_save->deactivate();pages[6]->end();}
    void open_organize(bool paste=false){if(store.setting("organize","1")!="1")throw std::runtime_error(u8"请先启用本地整理");show();navigate(Page::Organize);if(paste)Fl::paste(*organize_input,1);organize_input->take_focus();}
    void make_confirm(){pages[5]=group();confirm_text=label("",18);confirm_text->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);confirm_cancel=button(0,0,1,1,u8"取消");confirm_cancel->callback(back_callback,this);confirm_accept=button(0,0,1,1,u8"确认",true);confirm_accept->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{auto action=a->confirmed_action;if(action)action();});},this);pages[5]->end();}
    static void back_callback(Fl_Widget*,void* p){static_cast<Impl*>(p)->back();}
    void navigate(Page next,bool push=true){list->menu_id=0;if(push&&page!=next)history.push_back(page);page=next;visibility();layout();window->redraw();}
    void back(){Page next=Page::Home;if(!history.empty()){next=history.back();history.pop_back();}navigate(next,false);}
    void visibility(){for(int i=0;i<8;++i){pages[i]->box(FL_RFLAT_BOX);pages[i]->color(surface);if(i==static_cast<int>(page))pages[i]->show();else pages[i]->hide();}if(draft_active)resume->show();else resume->hide();
        const char* titles[]={u8"通用",u8"外观",u8"浮球",u8"提醒",u8"数据与备份",u8"本地整理"};settings_heading->copy_label(settings_section<0?u8"设置":titles[settings_section]);gear->selected(page==Page::Settings);if(draft.id)delete_button->show();else delete_button->hide();
    }
    // 每次缩放按可用宽高重新排布，禁止 Group 按初始坐标比例拉伸控件。
    void layout(){
        const int w=window->w(),h=window->h();bool side=console&&w>=800;int left=side?214:8,cw=w-left-8,cy=56,ch=h-98,x=left+22,y=cy+20,width=cw-44;
        place(logo,10,9,36,36);place(brand,54,10,w-280,34);place(alert_indicator,w-212,9,36,36);place(pin,w-170,9,76,36);place(gear,w-86,9,76,36);place(status,16,h-34,w-32,26);
        place(sidebar,8,56,194,ch);if(side)sidebar->show();else sidebar->hide();place(side_create,14,64,180,38);place(side_focus,18,118,172,22);for(int i=0;i<6;++i)place(side_nav[i],14,154+i*43,180,35);place(side_category,18,432,172,22);place(category_search,14,460,180,36);place(clear_category,14,506,180,32);
        for(auto* p:pages)place(p,left,cy,cw,ch);
        place(heading,x,y,width-108,32);place(create_button,x+width-96,y,96,36);place(date_label,x,y+39,width,20);place(search,x,y+72,width-(console?46:0),38);place(view_button,x+width-36,y+73,36,36);if(console)view_button->show();else view_button->hide();
        int tab_columns=width<540?3:6,tab_rows=6/tab_columns;for(int i=0;i<6;++i){place(nav[i],x+(i%tab_columns)*width/tab_columns,y+124+(i/tab_columns)*36,width/tab_columns-4,31);if(side)nav[i]->hide();else nav[i]->show();}
        int list_y=y+125+(side?0:tab_rows*36+10);place(list,x-2,list_y,width+4,std::max(80,cy+ch-42-list_y));place(summary,x,cy+ch-31,width-(draft_active?150:0),20);place(resume,x+width-146,cy+ch-35,146,28);
        int form_w=std::min(700,width),form_x=x+(width-form_w)/2;place(cancel_button,form_x,y,84,36);place(save_button,form_x+96,y,form_w-96,36);place(edit_scroll,form_x,y+50,form_w,ch-84);
        int fw=std::max(300,edit_scroll->w()-18),fx=form_x+6,base=edit_scroll->y()-edit_scroll->yposition();
        place(edit_labels[0],fx,base+6,fw,22);place(title,fx,base+34,fw,76);place(category,fx+fw-180,base+118,180,32);
        place(edit_labels[1],fx,base+160,fw,22);place(source,fx,base+188,fw,40);place(edit_labels[2],fx,base+246,fw,22);
        int chip=std::max(32,(fw-152)/5-5);for(int i=0;i<5;++i)place(due_chips[i],fx+i*(chip+5),base+276,chip,30);place(date_picker,fx+5*(chip+5),base+276,30,30);place(set_reminder,fx+fw-96,base+276,96,30);
        place(due_info,fx,base+315,fw,18);place(reminder_info,fx,base+340,fw,18);place(edit_labels[3],fx,base+377,fw,22);place(note,fx,base+405,fw,80);
        place(edit_labels[4],fx,base+503,80,22);place(child_info,fx+84,base+503,fw-84,22);place(child_list,fx,base+532,fw,140);place(child_input,fx,base+684,fw-82,34);place(add_child,fx+fw-72,base+684,72,34);place(attachment_label,fx,base+738,fw,24);place(attachment_list,fx,base+770,fw,104);place(attachment_input,fx,base+886,fw-82,34);place(attachment_add,fx+fw-72,base+886,72,34);place(delete_button,fx,base+944,fw,32);
        int compact_w=std::min(580,width),cx=x+(width-compact_w)/2;place(calendar_back,cx,y,92,36);place(calendar_heading,cx,y+65,compact_w,32);place(previous_month,cx,y+112,116,32);place(next_month,cx+compact_w-116,y+112,116,32);
        for(int i=0;i<7;++i)place(weekdays[i],cx+i*compact_w/7,y+163,compact_w/7,25);
        for(int i=0;i<42;++i)place(cells[i],cx+(i%7)*compact_w/7,y+198+(i/7)*39,compact_w/7-4,34);
        place(clear_date,cx,y+450,compact_w,34);
        place(reminder_back,cx,y,92,36);place(reminder_heading,cx,y+58,compact_w,32);for(int i=0;i<5;++i)place(presets[i],cx,y+112+i*46,compact_w,38);place(reminder_input,cx,y+360,compact_w,40);place(apply_reminder,cx,y+416,compact_w,36);
        place(settings_back,cx,y,92,36);place(settings_heading,cx,y+58,compact_w,32);settings_panel->arrange(cx,y+110,compact_w,std::max(100,ch-150));
        place(organize_back,x,y,92,36);place(organize_heading,x,y+48,width,30);place(organize_input,x,y+88,width,112);place(organize_paste,x,y+214,width/3-6,34);place(organize_analyze,x+width/3,y+214,width/3-6,34);place(organize_save,x+2*width/3,y+214,width/3,34);place(organize_preview,x,y+264,width,std::max(80,ch-304));
        place(confirm_text,cx,y+88,compact_w,130);place(confirm_cancel,cx,y+242,compact_w/2-8,36);place(confirm_accept,cx+compact_w/2+8,y+242,compact_w/2-8,36);
        place(alert_heading,cx,y+48,compact_w,36);place(alert_text,cx,y+102,compact_w,112);place(alert_info,cx,y+232,compact_w,90);place(alert_ack,cx,y+344,compact_w,40);place(alert_snooze,cx,y+398,compact_w/2-6,36);place(alert_view,cx+compact_w/2+6,y+398,compact_w/2-6,36);place(alert_settings,cx,y+452,compact_w,34);
        window->redraw();
    }
    void refresh(){
        auto all=store.list();int counts[6]{};int urgent=0;for(const auto& t:all){++counts[0];if(!t.done&&!t.shelved&&t.due_at>0&&t.due_at<end_of_today())++counts[1];++counts[t.done?4:t.shelved?5:t.in_progress?3:2];if(!t.done&&!t.shelved&&(t.urgent||(t.due_at&&t.due_at<day_after(0))))++urgent;}
        auto tasks=store.list(search->value());tasks.erase(std::remove_if(tasks.begin(),tasks.end(),[&](const Task& t){bool category_match=!category_search->value()[0]||t.category.find(category_search->value())!=std::string::npos;return !category_match||(filter==1&&(t.done||t.shelved||!t.due_at||t.due_at>=end_of_today()))||(filter==2&&(t.done||t.in_progress||t.shelved))||(filter==3&&(t.done||t.shelved||!t.in_progress))||(filter==4&&!t.done)||(filter==5&&(t.done||!t.shelved));}),tasks.end());
        list->tasks=std::move(tasks);list->refresh_children();list->full=console;list->board=console&&board_view&&filter==0;list->dense=flag(6);list->rebuild();
        for(int i=0;i<6;++i){auto text=std::string(names[i])+"  "+std::to_string(counts[i]);for(auto* b:{nav[i],side_nav[i]}){b->copy_label(text.c_str());b->color(i==filter?pale:canvas);b->labelcolor(i==filter?blue:muted);b->redraw();}}
        auto text=std::string(names[filter])+u8" · "+std::to_string(list->tasks.size())+u8" 项待办";heading->copy_label(text.c_str());date_label->copy_label(date_text(now_seconds()).c_str());
        text=u8"未开始 "+std::to_string(counts[2])+u8" · 进行中 "+std::to_string(counts[3])+u8" · 完成 "+std::to_string(counts[4]);summary->copy_label(text.c_str());if(ball){ball->count=counts[2]+counts[3];ball->urgent_count=urgent;ball->working_count=counts[3];ball->redraw();}prune_alerts(all);visibility();layout();
    }
    void toggle_mode(){console=!console;window->resize(window->x(),window->y(),console?1120:500,std::max(740,window->h()));refresh();message(console?u8"控制台 · 可切换看板和列表":u8"快捷面板 · 点击图标进入控制台");}
    void open_editor(const Task* task=nullptr){show();if(draft_active){navigate(Page::Edit);message(u8"请先保存或取消当前草稿");return;}
        draft=task?*task:Task{};draft_files=task?store.attachments(task->id):std::vector<Attachment>{};pending_files.clear();update_files();draft_active=true;child_edit=-1;title->value(draft.title.c_str());note->value(draft.note.c_str());source->value(draft.source.c_str());category->value(draft.category.c_str());child_list->items=task?store.children(task->id):std::vector<Subtask>{};child_list->offset=0;child_input->value("");add_child->copy_label(u8"添加");edit_scroll->scroll_to(0,0);update_draft_labels();navigate(Page::Edit);title->take_focus();}
    void cancel_edit(){draft_files.clear();pending_files.clear();draft_active=false;draft={};history.clear();navigate(Page::Home,false);guard([&]{refresh();});message(u8"已取消编辑");}
    void save_edit(){if(!trim(child_input->value()).empty())save_child();draft.title=title->value();draft.note=note->value();draft.source=source->value();draft.category=trim(category->value());std::vector<std::filesystem::path> created;try{auto files=copy_attachments(draft_files,pending_files,std::filesystem::u8path(store.setting("attachment_dir",(data/"attachments").u8string())),created);store.save_task(draft,child_list->items,&files);}catch(...){for(const auto& file:created){std::error_code error;std::filesystem::remove(file,error);std::filesystem::remove(file.parent_path(),error);}throw;}draft_files.clear();pending_files.clear();draft_active=false;draft={};history.clear();navigate(Page::Home,false);refresh();message(u8"任务和子任务已保存到本机");reschedule_tick();}
    void save_child(){auto text=trim(child_input->value());if(text.empty())throw std::runtime_error(u8"请输入子任务内容");if(child_edit>=0&&child_edit<static_cast<int>(child_list->items.size()))child_list->items[child_edit].title=text;else{if(child_list->items.size()>=200)throw std::runtime_error(u8"每项任务最多 200 条子任务");child_list->items.push_back({0,text,false});if(draft.done){draft.done=false;draft.in_progress=false;}}
        child_edit=-1;child_input->value("");add_child->copy_label(u8"添加");child_list->offset=std::max(0,static_cast<int>(child_list->items.size())*38-child_list->h());update_draft_labels();}
    void update_draft_labels(){const int days[]={0,1,2,3,7};for(int i=0;i<5;++i){due_chips[i]->color(draft.due_at==day_after(days[i])?pale:canvas);due_chips[i]->labelcolor(draft.due_at==day_after(days[i])?blue:ink);due_chips[i]->redraw();}
        auto text=draft.due_at?u8"交付："+date_text(draft.due_at):std::string(u8"未设交付日期");due_info->copy_label(text.c_str());text=draft.remind_at?u8"提醒："+date_text(draft.remind_at,true):std::string(u8"未设提醒");reminder_info->copy_label(text.c_str());int done=0;for(const auto& child:child_list->items)done+=child.done;text=std::to_string(done)+" / "+std::to_string(child_list->items.size())+u8" 完成 · 点击名称编辑";child_info->copy_label(text.c_str());child_list->normalize();child_list->redraw();window->redraw();}
    void fill_calendar(){auto t=local_parts(calendar_time(calendar_year,calendar_month,1));int start=(t.tm_wday+6)%7;int n[]={31,28,31,30,31,30,31,31,30,31,30,31};if(calendar_year%4==0&&(calendar_year%100!=0||calendar_year%400==0))n[1]=29;auto text=std::to_string(calendar_year)+u8" 年 "+std::to_string(calendar_month)+u8" 月";calendar_heading->copy_label(text.c_str());for(int i=0;i<42;++i){int day=i-start+1;cell_dates[i]=0;if(day>=1&&day<=n[calendar_month-1]){cell_dates[i]=calendar_time(calendar_year,calendar_month,day);cells[i]->copy_label(std::to_string(day).c_str());cells[i]->activate();cells[i]->show();}else{cells[i]->copy_label("");cells[i]->deactivate();cells[i]->hide();}cells[i]->color(cell_dates[i]&&cell_dates[i]==draft.due_at?pale:canvas);cells[i]->redraw();}}
    void task_action(const Task& task,TaskAction action){if(action==TaskAction::Edit){open_editor(&task);return;}if(draft_active){navigate(Page::Edit);message(u8"请先保存或取消当前草稿");return;}
        if(action==TaskAction::Delete){auto text=u8"删除「"+task.title+u8"」及其全部子任务？";confirm(text.c_str(),u8"删除",[this,id=task.id,token=task.edit_token]{store.remove(id,token);history.clear();navigate(Page::Home,false);refresh();reschedule_tick();message(u8"任务已删除");});return;}if(action==TaskAction::Shelve)store.set_shelved(task.id,!task.shelved);
        if(action==TaskAction::Defer){open_editor(&task);date_picker->do_callback();return;}if(action==TaskAction::Urgent)store.set_urgent(task.id,!task.urgent);if(action==TaskAction::Start)store.set_progress(task.id,!task.in_progress);if(action==TaskAction::Complete)store.set_done(task.id,!task.done);refresh();message(u8"任务状态已更新");reschedule_tick();}
    void update_files(){attachment_list->items.clear();for(const auto& file:draft_files)attachment_list->items.push_back({0,file.name,false});for(const auto& file:pending_files)attachment_list->items.push_back({0,file.filename().u8string()+u8" · 待保存",false});attachment_list->normalize();attachment_list->redraw();}
    void toggle_pin(){bool enabled=!pinned;if(enabled&&store.setting("desktop","0")=="1"){if(!platform_desktop(window.get(),false))throw std::runtime_error(u8"桌面模式未能退出");store.set_setting("desktop","0");settings_panel->reload();}if(!platform_topmost(window.get(),enabled))throw std::runtime_error(u8"当前桌面暂不支持置顶");pinned=enabled;store.set_setting("pinned",pinned?"1":"0");pin->selected(pinned);pin->copy_label(pinned?u8"已置顶":u8"置顶");message(pinned?u8"已置顶 · 再次点击取消":u8"已取消置顶");}
    void apply_setting(const std::string& key){
        if(key=="theme"||key=="accent"){palette(number("theme",0),number("accent",0),platform_system_dark());window->redraw();ball->redraw();}
        if(key=="opacity"&&!platform_opacity(window.get(),number("opacity",100)))throw std::runtime_error(u8"系统未能调整透明度");
        if(key=="desktop"){bool enabled=store.setting("desktop","0")=="1";if(!platform_desktop(window.get(),enabled))throw std::runtime_error(u8"当前桌面无法启用此模式");if(enabled){pinned=false;store.set_setting("pinned","0");pin->selected(false);pin->copy_label(u8"置顶");}platform_opacity(window.get(),number("opacity",100));}
        if(key=="hotkey_enabled"&&!platform_hotkey(store.setting("hotkey_enabled","0")=="1",number("hotkey_mod",3),number("hotkey_key",'N')))throw std::runtime_error(u8"快捷键已被占用，请换一组");
        if(key=="floating"||key.find("ball_")==0)apply_ball();
        if(key=="dense"||key=="sort_due")refresh();
        reschedule_tick();
    }
    std::filesystem::path full_path(const std::string& value){auto path=std::filesystem::u8path(trim(value));if(!path.is_absolute())throw std::runtime_error(u8"请输入完整的绝对路径");return path.lexically_normal();}
    void settings_action(const std::string& command){
        if(command=="exit"){request_exit();return;}if(command=="organize"){open_organize();return;}
        if(command=="notify"){send_notification(0,0,u8"这是一条测试提醒。系统未显示时，会在此窗口显示同样的提醒。");return;}
        if(command=="backup_now"){auto folder=data/"backups";std::filesystem::create_directories(folder);auto target=folder/("manual-"+std::to_string(now_seconds())+".sqlite3");store.backup(target);store.set_setting("last_backup",date_text(now_seconds(),true));settings_panel->reload();message(u8"已创建独立备份");return;}
        if(command=="export"){store.backup(full_path(settings_panel->field("backup_path")));message(u8"备份已导出");return;}
        if(command=="import"){auto target=full_path(settings_panel->field("backup_path"));confirm(u8"导入将替换当前任务与设置，并放弃未保存草稿。会先备份当前数据。",u8"备份并导入",[this,target]{store.restore_from(target);draft_active=false;draft_files.clear();pending_files.clear();pending_notifications.clear();retry_after.clear();alerts.clear();alert_indicator->hide();deliveries=std::make_shared<DeliveryQueue>();settings_panel->reload();settings_panel->paths(data);apply_setting("theme");platform_opacity(window.get(),number("opacity",100));if(!platform_hotkey(store.setting("hotkey_enabled","0")=="1",number("hotkey_mod",3),number("hotkey_key",78))){platform_hotkey(false,0,0);store.set_setting("hotkey_enabled","0");}store.set_setting("desktop","0");pinned=store.setting("pinned","0")=="1";if(!platform_topmost(window.get(),pinned))pinned=false;pin->selected(pinned);pin->copy_label(pinned?u8"已置顶":u8"置顶");apply_ball();settings_panel->reload();history.clear();navigate(Page::Home,false);refresh();message(u8"导入完成，原数据已留存备份");});return;}
        if(command=="open_data"||command=="open_attachments"){auto folder=command=="open_data"?data:std::filesystem::u8path(store.setting("attachment_dir",(data/"attachments").u8string()));std::filesystem::create_directories(folder);if(!platform_open_folder(folder))throw std::runtime_error(u8"系统未能打开文件夹");return;}
        if(command=="move_attachments"){auto target=full_path(settings_panel->field("attachment_path"));std::filesystem::create_directories(target);auto probe=target/(".write-check-"+std::to_string(now_seconds()));{std::ofstream out(probe);if(!out)throw std::runtime_error(u8"新目录不可写");}std::filesystem::remove(probe);store.set_setting("attachment_dir",target.u8string());settings_panel->paths(data);message(u8"新附件将保存到此目录，已有附件保留原位置");return;}
        if(command=="move_data"){
            auto target=full_path(settings_panel->field("data_path"));
            if(target==data)throw std::runtime_error(u8"请选择不同的新目录");
            if(std::filesystem::exists(target/".litetodo-location"))throw std::runtime_error(u8"新目录已有位置设置，请选择空目录");
            confirm(u8"复制数据库到新目录并记住位置。原目录和已有附件保留。",u8"复制并切换",[this,target]{
                std::unique_ptr<InstanceLock> next;
                if(instance){next=std::make_unique<InstanceLock>(target);if(!next->owns())throw std::runtime_error(u8"新目录正在被另一实例使用，请先退出该实例");}
                store.set_setting("attachment_dir",store.setting("attachment_dir",(data/"attachments").u8string()));
                std::filesystem::create_directories(target);
                store.relocate(target/"tasks.sqlite3",[this,target]{remember_data_directory(anchor,target);});
                if(next)instance->swap(*next);
                data=target;settings_panel->paths(data);back();message(u8"数据目录已切换，原目录仍保留");
                if(platform_autostart_enabled()&&!platform_set_autostart(true,data.u8string()))message(u8"目录已切换，请重新设置自动启动");
            });return;
        }
        if(command=="ball_reset"){ball->reset_position();message(u8"浮球已放回右上方");return;}
    }
    bool shortcut(int key,int state){if(settings_panel->recording()){guard([&]{settings_panel->capture(key,state);});return true;}if((state&FL_CTRL)&&(key=='n'||key=='N')){guard([&]{open_editor();});return true;}if((state&FL_CTRL)&&(key=='f'||key=='F')){navigate(Page::Home);search->take_focus();return true;}if((state&FL_CTRL)&&(key==FL_Enter||key==FL_KP_Enter)&&page==Page::Edit){guard([&]{save_edit();});return true;}
        if(key==FL_Escape&&page!=Page::Home){if(page==Page::Edit)cancel_edit();else if(page==Page::Settings)settings_back->do_callback();else back();return true;}return false;}
    void confirm(const char* text,const char* accept,std::function<void()> action){confirmed_action=std::move(action);confirm_text->copy_label(text);confirm_accept->copy_label(accept);navigate(Page::Confirm);}
    void request_exit(){if(draft_active)confirm(u8"还有未保存的草稿。放弃草稿并退出？",u8"放弃并退出",[this]{quit=true;});else quit=true;}
    void close_main(){if(flag(2)&&(platform_tray_available()||(ball&&ball->shown())))window->hide();else request_exit();}
    void show(){window->show();if(pinned)platform_topmost(window.get(),true);platform_opacity(window.get(),number("opacity",100));}
    void make_ball(){ball=std::make_unique<Ball>();int bx=1100,by=700;std::ifstream saved(data/"ball-position.txt");saved>>bx>>by;int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh,bx,by);ball->position(std::clamp(bx,sx,sx+std::max(0,sw-ball->w())),std::clamp(by,sy,sy+std::max(0,sh-ball->h())));ball->click=[this]{if(ball->double_click&&window->shown())window->hide();else{show();guard([&]{refresh();});}if(ball->chat)message(u8"先完成一件小事，今天也在前进。");};ball->hover_open=[this]{if(!window->shown()){show();if(page==Page::Alert&&!alerts.empty())update_alert();}};ball->dropped=[this](const std::string& text){guard([&]{auto files=drop_files(text);if(files.empty())throw std::runtime_error(u8"请拖入本机文件");if(draft_active){navigate(Page::Edit);message(u8"请先保存或取消当前草稿");return;}open_editor();pending_files.insert(pending_files.end(),files.begin(),files.end());if(!title->value()[0])title->value(files[0].stem().u8string().c_str());update_files();});};ball->right_click=[this]{guard([&]{open_editor();});};ball->position_saved=[this]{guard([&]{std::ofstream save(data/"ball-position.txt");save<<ball->x()<<' '<<ball->y();if(!save)throw std::runtime_error(u8"浮球位置未能保存");});};}
    void apply_ball(){ball->style=number("ball_style",1);ball->accent=number("ball_accent",0);ball->wire=store.setting("ball_wire","0")=="1";ball->edge=store.setting("ball_edge","0")=="1";ball->hover_expand=store.setting("ball_hover","0")=="1";ball->double_click=store.setting("ball_double","0")=="1";ball->chat=store.setting("ball_chat","0")=="1";ball->accept_drop=store.setting("ball_drop","1")=="1";ball->configure();if(flag(4)){ball->show();platform_topmost(ball.get(),true);}else{ball->hide();if(!window->shown()&&!platform_tray_available())show();}}
    void reschedule_tick(){Fl::remove_timeout(tick,this);Fl::add_timeout(0.2,tick,this);}
    void send_notification(long long id,long long at,const std::string& text){pending_notifications.insert(id);std::weak_ptr<DeliveryQueue> weak=deliveries;bool submitted=platform_notify(id?u8"任务提醒":u8"轻待办 · 测试通知",text,[weak,id,at](bool accepted){if(auto q=weak.lock()){{std::lock_guard<std::mutex> lock(q->mutex);q->results.push_back({id,at,accepted});}Fl::awake();}});message(submitted?u8"等待系统显示通知，最多 10 秒后反馈结果":platform_notification_feedback());if(!submitted){pending_notifications.erase(id);present_alert({id,at,false});}}
    void make_alert(){pages[7]=group();alert_heading=label(u8"有件事情提醒你",21);alert_text=label("",18);alert_text->labelfont(FL_HELVETICA_BOLD);alert_text->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);alert_info=label("",12,muted);alert_info->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
        alert_ack=button(0,0,1,1,u8"知道了",true);alert_ack->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->dismiss_alert(false);});},this);
        alert_snooze=button(0,0,1,1,u8"5 分钟后再提醒");alert_snooze->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{a->dismiss_alert(true);});},this);
        alert_view=button(0,0,1,1,u8"查看任务");alert_view->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->guard([&]{for(const auto& t:a->store.list())if(t.id==a->alert_id){a->open_editor(&t);return;}});},this);
        alert_settings=button(0,0,1,1,u8"打开系统通知设置");alert_settings->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);if(!platform_open_notification_settings())a->message(u8"请在系统设置中检查通知权限");},this);pages[7]->end();
        window->begin();alert_indicator=new IconButton(0,0,36,36,Icon::Clock,u8"查看尚未处理的提醒");alert_indicator->selected(true);alert_indicator->callback([](Fl_Widget*,void* p){auto* a=static_cast<Impl*>(p);a->update_alert();a->navigate(Page::Alert);},this);alert_indicator->hide();window->end();
    }
    void prune_alerts(const std::vector<Task>& all){for(auto it=alerts.begin();it!=alerts.end();){if(it->first&&std::none_of(all.begin(),all.end(),[&](const Task& t){return t.id==it->first&&!t.done&&!t.shelved&&t.remind_at==it->second;}))it=alerts.erase(it);else ++it;}if(alerts.empty()){alert_indicator->hide();if(page==Page::Alert)back();}else{alert_indicator->show();if(page==Page::Alert)update_alert();}}
    void update_alert(){if(alerts.empty()){if(page==Page::Alert)back();return;}alert_id=alerts.begin()->first;std::string text=u8"这是一条测试提醒。";if(alert_id)for(const auto& t:store.list())if(t.id==alert_id)text=t.title;alert_heading->copy_label((u8"待处理提醒 · "+std::to_string(alerts.size())+u8" 条").c_str());alert_text->copy_label(text.c_str());alert_info->copy_label((alert_reason+u8"。\n已改用应用内提醒，点「知道了」后才标记为已提醒。").c_str());if(alert_id){alert_view->activate();alert_snooze->activate();}else{alert_view->deactivate();alert_snooze->deactivate();}alert_indicator->show();window->redraw();}
    void present_alert(const Delivery& event){if(event.id){auto all=store.list();if(std::none_of(all.begin(),all.end(),[&](const Task& t){return t.id==event.id&&!t.done&&!t.shelved&&t.remind_at==event.at;}))return;}alerts[event.id]=event.at;retry_after.erase(event.id);alert_reason=platform_notification_feedback();update_alert();show();if(!draft_active&&page!=Page::Confirm)navigate(Page::Alert);else message(u8"有未处理提醒 · 点击顶部蓝色提醒按钮查看");}
    void dismiss_alert(bool snooze){
        auto found=alerts.find(alert_id);if(found==alerts.end())return;
        if(alert_id){if(snooze){
            auto current=store.get(alert_id);
            if(current.remind_at!=found->second||current.done||current.shelved)throw std::runtime_error(u8"此提醒已变化，请刷新任务后再操作");
            bool matching_draft=draft_active&&draft.id==alert_id&&draft.edit_token==current.edit_token;
            auto updated=store.reschedule(alert_id,now_seconds()+300,current.edit_token);
            // 只同步本次操作产生的新令牌；此前已失效的草稿继续保持冲突状态。
            if(matching_draft){draft.remind_at=updated.remind_at;draft.edit_token=updated.edit_token;update_draft_labels();}
        }else store.confirm_notification(alert_id,found->second);}
        alerts.erase(found);if(alerts.empty()){alert_indicator->hide();if(page==Page::Alert)back();}else update_alert();reschedule_tick();message(snooze?u8"将在 5 分钟后再次提醒":u8"已确认看到提醒");
    }
    void process_deliveries(){std::vector<Delivery> results;{std::lock_guard<std::mutex> lock(deliveries->mutex);results.swap(deliveries->results);}for(const auto& event:results){pending_notifications.erase(event.id);guard([&]{if(event.accepted){if(event.id)store.confirm_notification(event.id,event.at);retry_after.erase(event.id);message(platform_notification_confirmation());}else present_alert(event);});if(smoke)std::ofstream(data/"notification-result.txt",std::ios::app)<<event.id<<" "<<(event.accepted?platform_notification_confirmation():platform_notification_feedback())<<"\n";}if(!results.empty())reschedule_tick();}
    static void tick(void* p){auto* a=static_cast<Impl*>(p);platform_poll();if(!a->window->shown()&&!platform_tray_available()&&!a->ball->shown())a->show();long long next=0;
        a->guard([&]{if(a->flag(5)){std::vector<long long> excluded(a->pending_notifications.begin(),a->pending_notifications.end());for(const auto& alert:a->alerts)excluded.push_back(alert.first);for(const auto& task:a->store.due(now_seconds(),excluded)){a->send_notification(task.id,task.remind_at,task.title);excluded.push_back(task.id);}next=a->store.next_due(excluded);}
            auto today=date_text(now_seconds());if(today!=a->date_label->label())a->refresh();if(a->number("theme",0)==0&&Style::dark()!=platform_system_dark())a->apply_setting("theme");if(a->flag(7))daily_backup(a->store,a->data/"backups");
        });double wait=next?std::clamp(static_cast<double>(next-now_seconds()),1.0,30.0):30.0;Fl::repeat_timeout(wait,tick,a);}
    void capture(const char* filename){
        // 实际控件的离屏渲染，不等同于桌面截图、真实鼠标或输入法验证。
        Fl_Image_Surface surface(window->w(),window->h());Fl_Surface_Device::push_current(&surface);surface.draw(window.get());std::unique_ptr<Fl_RGB_Image> image(surface.image());Fl_Surface_Device::pop_current();if(!image||image->d()<3)throw std::runtime_error("UI render capture failed");std::ofstream ppm(data/filename,std::ios::binary);ppm<<"P6\n"<<image->w()<<" "<<image->h()<<"\n255\n";const auto* pixels=reinterpret_cast<const unsigned char*>(image->data()[0]);int stride=image->ld()?image->ld():image->w()*image->d();for(int y=0;y<image->h();++y)for(int x=0;x<image->w();++x)ppm.write(reinterpret_cast<const char*>(pixels+y*stride+x*image->d()),3);if(!ppm)throw std::runtime_error("Cannot save UI capture");
    }
    void check_layout(){for(auto* widget:std::array<Fl_Widget*,6>{logo,gear,search,list,save_button,edit_scroll})if(widget->x()<0||widget->y()<0||widget->x()+widget->w()>window->w()||widget->y()+widget->h()>window->h()||widget->w()<1||widget->h()<1)throw std::runtime_error("Responsive bounds failed");for(const auto& card:list->cards)if(card.w<100||card.x<0||card.x+card.w>list->w())throw std::runtime_error("Task card bounds failed");}
    void capture_ball(const char* filename){Fl_Image_Surface surface(80,80);Fl_Surface_Device::push_current(&surface);surface.draw(ball.get());std::unique_ptr<Fl_RGB_Image> image(surface.image());Fl_Surface_Device::pop_current();std::ofstream out(data/filename,std::ios::binary);out<<"P6\n80 80\n255\n";const auto* pixels=reinterpret_cast<const unsigned char*>(image->data()[0]);int stride=image->ld()?image->ld():80*image->d();for(int y=0;y<80;++y)for(int x=0;x<80;++x)out.write(reinterpret_cast<const char*>(pixels+y*stride+x*image->d()),3);}
#ifdef _WIN32
    void check_ball_region(const char* cause){
        // 检查真实 HWND 的物理像素区域，不能用离屏图片代替透明窗口验收。
        Fl::check();const auto handle=fl_win32_xid(ball.get());RECT client{};GetClientRect(handle,&client);HRGN region=CreateRectRgn(0,0,0,0);
        const int shaped=GetWindowRgn(handle,region);const int width=client.right,height=client.bottom;
        const bool round=width>0&&height>0&&shaped!=ERROR&&!PtInRegion(region,0,0)&&!PtInRegion(region,width-1,0)&&!PtInRegion(region,0,height-1)&&!PtInRegion(region,width-1,height-1)&&PtInRegion(region,width/2,height/2);
        DeleteObject(region);if(!round)throw std::runtime_error(cause);
        if(!(GetWindowLongPtrW(handle,GWL_EXSTYLE)&WS_EX_TOPMOST))throw std::runtime_error("Floating window lost native topmost state");
    }
    void check_ball_lifecycle(){
        check_ball_region("Ball native window still has rectangular corners");
        auto previous=store.setting("opacity","100");store.set_setting("opacity","80");apply_setting("opacity");
        window->hide();show();Fl::check();BYTE alpha=0;DWORD flags=0;COLORREF color=0;
        if(!GetLayeredWindowAttributes(fl_win32_xid(window.get()),&color,&alpha,&flags)||alpha!=204)throw std::runtime_error("Main window lost configured opacity after reopening");
        store.set_setting("opacity",previous);apply_setting("opacity");
        for(int i=0;i<3;++i){ball->hide();ball->show();check_ball_region("Reopening ball loses native round region");window->hide();show();check_ball_region("Main window hide/show loses ball round region");}
        const int bx=ball->x(),by=ball->y();ball->position(bx+16,by+16);ball->wire=!ball->wire;ball->redraw();check_ball_region("Ball move/repaint loses round region");ball->wire=!ball->wire;ball->position(bx,by);ball->redraw();
        const int screen=ball->screen_num();const float scale=Fl::screen_scale(screen);Fl::screen_scale(screen,scale*1.5f);check_ball_region("Ball 150 percent scale loses round region");Fl::screen_scale(screen,scale);check_ball_region("Ball scale restore loses round region");
        std::ofstream(data/"native-window-state.txt")<<"Main opacity after HWND recreation: PASS (80 percent, alpha 204)\nBall topmost after hide/show and scale: PASS\n";
    }
#endif
    void check_quick_children(){
        auto task=store.list(u8"本周工作").at(0);list->toggle_children(task.id);refresh();
        auto rows=[&]{for(const auto& card:list->cards)if(list->tasks[card.index].id==task.id)return list->child_rows(card);throw std::runtime_error("Expanded parent missing");};
        auto click=[&](const TaskBoard::ChildRow& row){int ox=Fl::e_x,oy=Fl::e_y,ok=Fl::e_keysym;Fl::e_x=list->x()+row.x+8;Fl::e_y=list->y()+row.y-list->offset+row.h/2;Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;list->handle(FL_PUSH);list->handle(FL_RELEASE);Fl::e_x=ox;Fl::e_y=oy;Fl::e_keysym=ok;};
        auto children=rows();if(children.size()!=2)throw std::runtime_error("Main-page child expansion failed");capture("quick-children-render.ppm");
        int ox=Fl::e_x,oy=Fl::e_y,ok=Fl::e_keysym;Fl::e_x=list->x()+children[0].x+8;Fl::e_y=list->y()+children[0].y-list->offset+children[0].h/2;Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;list->handle(FL_PUSH);Fl::e_y=list->y()+children[1].y-list->offset+children[1].h/2;list->handle(FL_DRAG);list->handle(FL_RELEASE);Fl::e_x=ox;Fl::e_y=oy;Fl::e_keysym=ok;
        if(store.list(u8"本周工作")[0].child_done!=1)throw std::runtime_error("Dragging between child rows changed completion");
        auto parent_card=*std::find_if(list->cards.begin(),list->cards.end(),[&](const TaskBoard::Card& card){return list->tasks[card.index].id==task.id;});
        for(int destination=0;destination<3;++destination){ox=Fl::e_x;oy=Fl::e_y;ok=Fl::e_keysym;Fl::e_x=list->x()+children[0].x+8;Fl::e_y=list->y()+children[0].y-list->offset+children[0].h/2;Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;list->handle(FL_PUSH);Fl::e_x=list->x()+parent_card.x+(destination==0?parent_card.w-63:destination==1?parent_card.w-27:40);Fl::e_y=list->y()+parent_card.y-list->offset+(destination<2?parent_card.h-25:20);list->handle(FL_DRAG);list->handle(FL_RELEASE);Fl::e_x=ox;Fl::e_y=oy;Fl::e_keysym=ok;
            if(page!=Page::Home||list->menu_id||store.list(u8"本周工作")[0].in_progress)throw std::runtime_error("Child drag triggered a parent action");}
        click(children[1]);if(page!=Page::Home||store.list(u8"本周工作")[0].child_done!=2)throw std::runtime_error("Quick child check did not save in main page");
        if(store.list(u8"本周工作")[0].done)throw std::runtime_error("Quick child check unexpectedly completed parent");
        store.set_done(task.id,true);refresh();children=rows();click(children[1]);if(store.list(u8"本周工作")[0].done||store.list(u8"本周工作")[0].child_done!=1)throw std::runtime_error("Quick child reopen did not restore parent");
        // 模式、窄窗口与滚动后的命中必须共用实际布局，点击子项不能进入编辑页。
        console=true;window->resize(window->x(),window->y(),1120,780);refresh();children=rows();click(children[1]);if(page!=Page::Home||store.list(u8"本周工作")[0].child_done!=2)throw std::runtime_error("Board quick child hit failed");
        console=false;window->resize(window->x(),window->y(),440,620);refresh();children=rows();click(children[1]);if(page!=Page::Home||store.list(u8"本周工作")[0].child_done!=1)throw std::runtime_error("Narrow quick child hit failed");
        open_editor();title->value(u8"尚未保存的子任务操作保护");navigate(Page::Home);children=rows();click(children[1]);if(page!=Page::Edit||std::string(title->value())!=u8"尚未保存的子任务操作保护"||store.list(u8"本周工作")[0].child_done!=1)throw std::runtime_error("Quick child action overwrote draft");cancel_edit();
        list->toggle_children(task.id);if(!rows().empty())throw std::runtime_error("Child collapse failed");window->resize(window->x(),window->y(),500,780);refresh();
        Task large;large.title=u8"长子任务与滚动回归";std::vector<Subtask> many;for(int i=0;i<200;++i)many.push_back({0,i==199?std::string(u8"最后一项：确认这一段较长的中文子任务换行后，仍然能够在主页面滚动并直接完成，不会被底部的父任务按钮覆盖。"):u8"检查项 "+std::to_string(i+1),false});
        auto large_id=store.save_task(large,many);search->value(large.title.c_str());refresh();list->toggle_children(large_id);const auto card=list->cards.at(0);auto large_rows=list->child_rows(card);if(large_rows.size()!=200||large_rows.back().y+large_rows.back().h>card.y+card.h-40)throw std::runtime_error("Long child rows overlap task actions");list->offset=std::max(0,list->total-list->h());click(large_rows.back());if(store.list(large.title)[0].child_done!=1||page!=Page::Home)throw std::runtime_error("Scrolled final child quick check failed");store.remove(large_id);search->value("");list->offset=0;refresh();
    }
    void check_editor_scroll(){
        open_editor();Fl::check();if(Fl::box_dx(title->box())<14||Fl::box_dy(title->box())<12)throw std::runtime_error("Multiline cursor padding missing");capture("new-editor-render.ppm");
        const auto bar=edit_scroll->thumb();int ox=Fl::e_x,oy=Fl::e_y,ok=Fl::e_keysym;Fl::e_x=edit_scroll->scrollbar.x()+edit_scroll->scrollbar.w()/2;Fl::e_y=bar.y+bar.h/2;Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;edit_scroll->handle(FL_PUSH);Fl::e_y=bar.track_y+bar.track_h-2;edit_scroll->handle(FL_DRAG);edit_scroll->handle(FL_RELEASE);Fl::e_x=ox;Fl::e_y=oy;Fl::e_keysym=ok;Fl::check();if(edit_scroll->yposition()<100)throw std::runtime_error("Soft scrollbar drag did not move editor");capture("editor-scroll-bottom-render.ppm");edit_scroll->scroll_to(0,0);cancel_edit();
    }
    void check_new_interactions(){
        // 只操作本程序的控件和隔离数据库，验证菜单路径与状态变更。
        auto id=store.add(u8"卡片菜单回归验证","",0);refresh();
        auto select=[&](int item){for(size_t i=0;i<list->cards.size();++i)if(list->tasks[list->cards[i].index].id==id){list->open_menu(i);if(!list->menu_bounds_valid())throw std::runtime_error("Inline task menu exceeds viewport");list->menu_hover=item;int old=Fl::e_keysym;Fl::e_keysym=FL_Enter;list->handle(FL_KEYBOARD);Fl::e_keysym=old;return;}throw std::runtime_error("Menu task missing");};
        select(1);if(!store.list(u8"卡片菜单回归")[0].urgent)throw std::runtime_error("Menu urgency failed");
        select(2);if(!store.list(u8"卡片菜单回归")[0].shelved)throw std::runtime_error("Menu shelving failed");
        select(2);if(store.list(u8"卡片菜单回归")[0].shelved)throw std::runtime_error("Menu resume failed");
        for(const auto& c:list->cards)if(list->tasks[c.index].id==id){int ox=Fl::e_x,oy=Fl::e_y,ok=Fl::e_keysym;Fl::e_x=list->x()+c.x+c.w-63;Fl::e_y=list->y()+c.y-list->offset+c.h-25;Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;list->handle(FL_RELEASE);Fl::e_x=ox;Fl::e_y=oy;Fl::e_keysym=ok;break;}
        if(!store.list(u8"卡片菜单回归")[0].in_progress)throw std::runtime_error("Direct start button failed");
        select(0);if(!store.list(u8"卡片菜单回归")[0].done)throw std::runtime_error("Menu completion failed");
        select(3);if(page!=Page::Confirm)throw std::runtime_error("Card deletion bypassed inline confirmation");confirm_cancel->do_callback();if(store.list(u8"卡片菜单回归").empty())throw std::runtime_error("Cancelled deletion removed task");select(3);confirm_accept->do_callback();if(!store.list(u8"卡片菜单回归").empty())throw std::runtime_error("Confirmed card deletion failed");
        for(size_t i=0;i<list->cards.size();++i){list->open_menu(i);if(!list->menu_bounds_valid())throw std::runtime_error("Bottom card menu clipped");}list->open_menu(0);capture("task-menu-render.ppm");list->menu_id=0;
        ball->hover_expand=true;ball->edge=false;ball->configure();window->hide();ball->handle(FL_ENTER);auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(380);while(std::chrono::steady_clock::now()<deadline)Fl::wait(.02);ball->handle(FL_LEAVE);if(!window->shown()||page!=Page::Home)throw std::runtime_error("Free ball hover did not restore task list");
        open_editor();title->value(u8"悬停应保留的草稿");ball->handle(FL_ENTER);deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(380);while(std::chrono::steady_clock::now()<deadline)Fl::wait(.02);ball->handle(FL_LEAVE);if(page!=Page::Edit||std::string(title->value())!=u8"悬停应保留的草稿")throw std::runtime_error("Hover overwrote visible draft");cancel_edit();
        ball->style=1;ball->wire=false;ball->count=0;ball->urgent_count=0;if(ball->mood()!=Ball::Mood::Happy)throw std::runtime_error("Empty ball mood failed");capture_ball("ball-happy-render.ppm");ball->count=12;if(ball->mood()!=Ball::Mood::Tired)throw std::runtime_error("Busy ball mood failed");capture_ball("ball-tired-render.ppm");ball->urgent_count=1;if(ball->mood()!=Ball::Mood::Urgent)throw std::runtime_error("Urgent mood priority failed");capture_ball("ball-urgent-render.ppm");ball->wire=true;capture_ball("ball-wire-render.ppm");apply_ball();refresh();
        id=store.add(u8"应用内提醒验证","",now_seconds()-1);auto task=store.list(u8"应用内提醒验证")[0];present_alert({id,task.remind_at,false});if(page!=Page::Alert||store.due(now_seconds()).empty())throw std::runtime_error("Fallback consumed unseen reminder");capture("notification-fallback-render.ppm");alert_ack->do_callback();if(!store.due(now_seconds()).empty())throw std::runtime_error("Reminder acknowledgement failed");store.reschedule(id,now_seconds()-1);task=store.list(u8"应用内提醒验证")[0];present_alert({id,task.remind_at,false});alert_snooze->do_callback();if(store.list(u8"应用内提醒验证")[0].remind_at<now_seconds()+290)throw std::runtime_error("Reminder snooze failed");store.remove(id);refresh();
        id=store.add(u8"编辑期间延后回归","",now_seconds()-1);task=store.list(u8"编辑期间延后回归")[0];open_editor(&task);present_alert({id,task.remind_at,false});alert_indicator->do_callback();alert_snooze->do_callback();if(page!=Page::Edit||draft.remind_at<now_seconds()+290)throw std::runtime_error("Snooze did not update active draft");save_edit();if(store.list(u8"编辑期间延后回归")[0].remind_at<now_seconds()+290)throw std::runtime_error("Draft save reverted snoozed reminder");store.remove(id);refresh();
#ifdef _WIN32
        if(!platform_notification_regression_test())throw std::runtime_error("Old notification receipt confirmed a newer request");
#endif
        // 真实调用系统测试通知，等待回执或应用内兜底；不等同于人工目视验收。
        settings_action("notify");
    }
    static void smoke_tick(void* p){auto* a=static_cast<Impl*>(p);try{
#ifdef _WIN32
        a->check_ball_lifecycle();
#endif
        a->open_editor();a->title->value(u8"整理本周工作计划");a->source->value(u8"个人计划 · 演示项目");a->category->value(u8"工作");a->note->value(u8"整理待办事项，安排优先级，留出休息时间。");a->due_chips[0]->do_callback();a->child_input->value(u8"列出本周重点事项");a->add_child->do_callback();a->child_list->items[0].done=true;a->child_input->value(u8"安排时间并复核计划");a->add_child->do_callback();a->save_button->do_callback();auto t=a->store.list(u8"本周工作");if(t.size()!=1||t[0].child_count!=2||t[0].child_done!=1||t[0].due_at!=day_after(0))throw std::runtime_error("Task/children/shortcut save failed");long long first=t[0].id;
        Task second;second.title=u8"阅读新书，记录心得";second.source=u8"阅读清单";second.due_at=day_after(3);second.note=u8"每天阅读一小节，记录喜欢的段落和想法。";auto second_id=a->store.save_task(second,{});a->store.set_progress(second_id,true);
        Task third;third.title=u8"准备周末出行：确认路线、天气与随身物品";third.due_at=day_after(7);a->store.save_task(third,{});
        auto running=a->store.list(u8"阅读新书");a->open_editor(&running[0]);a->child_input->value(u8"整理阅读笔记");a->add_child->do_callback();a->save_button->do_callback();running=a->store.list(u8"阅读新书");if(!running[0].in_progress)throw std::runtime_error("Adding a child lost parent running state");
        a->open_editor(&running[0]);a->child_list->items[0].done=true;a->save_button->do_callback();running=a->store.list(u8"阅读新书");a->open_editor(&running[0]);a->child_list->action(0,0);a->save_button->do_callback();if(!a->store.list(u8"阅读新书")[0].in_progress)throw std::runtime_error("Reopening a child lost parent running state");
        t=a->store.list(u8"本周工作");a->open_editor(&t[0]);a->child_input->value(u8"这项未保存，不应写入数据库");a->add_child->do_callback();a->cancel_button->do_callback();if(a->store.children(first).size()!=2)throw std::runtime_error("Cancel leaked child draft");
        a->check_quick_children();a->check_editor_scroll();
        a->search->value(u8"本周工作");a->search->do_callback();if(a->list->tasks.size()!=1)throw std::runtime_error("Search failed");a->search->value("");a->search->do_callback();a->capture("ui-render.ppm");
        a->logo->do_callback();if(!a->console)throw std::runtime_error("Logo mode switch failed");fl_font(FL_HELVETICA,14);for(const auto& card:a->list->cards)if(a->list->tasks[card.index].title==third.title&&card.h<2*fl_height()+105)throw std::runtime_error("Long Chinese title did not wrap");a->capture("console-render.ppm");a->view_button->do_callback();a->capture("console-list-render.ppm");a->view_button->do_callback();
        for(auto size:std::array<std::pair<int,int>,5>{{{440,620},{760,650},{1120,780},{1440,900},{500,780}}}){a->window->resize(a->window->x(),a->window->y(),size.first,size.second);a->check_layout();if(size.first==440)a->capture("narrow-render.ppm");if(size.first==1440)a->capture("wide-render.ppm");}
        a->console=false;a->refresh();t=a->store.list(u8"本周工作");a->open_editor(&t[0]);a->capture("editor-render.ppm");a->edit_scroll->scroll_to(0,250);a->capture("subtasks-render.ppm");a->edit_scroll->scroll_to(0,0);a->date_picker->do_callback();if(a->page!=Page::Calendar)throw std::runtime_error("Inline calendar failed");a->capture("calendar-render.ppm");a->calendar_back->do_callback();a->set_reminder->do_callback();a->capture("reminder-render.ppm");a->reminder_back->do_callback();a->gear->do_callback();a->capture("settings-render.ppm");a->settings_rows[0]->do_callback();a->capture("general-render.ppm");a->window->resize(a->window->x(),a->window->y(),440,620);a->capture("general-narrow-render.ppm");a->window->resize(a->window->x(),a->window->y(),500,780);a->settings_back->do_callback();a->settings_back->do_callback();if(a->page!=Page::Edit||!a->draft_active)throw std::runtime_error("Settings back lost draft");
        int old_key=Fl::e_keysym,old_state=Fl::e_state,old_length=Fl::e_length;char* old_text=Fl::e_text;Fl::e_keysym=FL_Enter;Fl::e_state=FL_CTRL;Fl::e_text=const_cast<char*>("\r");Fl::e_length=1;a->note->handle(FL_KEYBOARD);Fl::e_keysym=old_key;Fl::e_state=old_state;Fl::e_text=old_text;Fl::e_length=old_length;if(a->draft_active||a->page!=Page::Home)throw std::runtime_error("Synthetic Ctrl+Enter failed");
        // 本程序自身控件回调验证，不注入外部窗口或改动真实用户数据。
        a->pin->do_callback();if(!a->pinned||!a->pin->selected())throw std::runtime_error("Pin selected feedback failed");a->capture("pin-render.ppm");a->pin->do_callback();
        a->gear->do_callback();a->settings_panel->choose(1);a->capture("appearance-render.ppm");a->store.set_setting("theme","2");a->apply_setting("theme");a->settings_panel->reload();a->capture("dark-render.ppm");a->store.set_setting("theme","1");a->apply_setting("theme");a->settings_panel->reload();
        a->window->resize(a->window->x(),a->window->y(),440,620);a->settings_panel->scroll_to(0,600);a->capture("appearance-bottom-render.ppm");
        if(a->settings_panel->y()+a->settings_panel->h()>a->window->h()-34)throw std::runtime_error("Settings viewport clipped footer");
        a->settings_panel->choose(2);a->capture("ball-settings-render.ppm");a->settings_panel->scroll_to(0,900);a->capture("ball-bottom-render.ppm");a->store.set_setting("ball_style","1");a->store.set_setting("ball_wire","1");a->apply_ball();a->ball->reset_position();if(a->ball->style!=1||!a->ball->wire)throw std::runtime_error("Ball configuration failed");
        a->settings_panel->choose(4);a->settings_action("backup_now");a->capture("backup-settings-render.ppm");a->settings_panel->scroll_to(0,1000);a->capture("backup-bottom-render.ppm");
        a->settings_panel->choose(5);a->capture("organize-settings-render.ppm");a->open_organize();a->organize_input->value(u8"今天加急核对接口\n明天提交报告");a->organize_analyze->do_callback();if(a->organize_preview->tasks.size()!=2)throw std::runtime_error("Organizer preview failed");a->capture("organizer-render.ppm");a->organize_input->value(u8"更改后任务");a->organize_input->do_callback();if(a->organize_save->active()||!a->organize_preview->tasks.empty())throw std::runtime_error("Edited organizer preview stayed valid");a->organize_save->do_callback();if(!a->store.list(u8"今天加急核对接口").empty())throw std::runtime_error("Stale organizer preview saved");a->organize_input->value(u8"今天加急核对接口\n明天提交报告");a->organize_analyze->do_callback();a->organize_save->do_callback();if(a->store.list(u8"今天加急核对接口").size()!=1||a->page!=Page::Home)throw std::runtime_error("Organizer confirmation save failed");
        a->open_editor();a->title->value(u8"附件存储验证");auto attachment_source=a->data/u8"原始 中文附件.txt";{std::ofstream original(attachment_source);original<<"keep";}a->attachment_input->value(attachment_source.u8string().c_str());a->attachment_add->do_callback();a->cancel_edit();if(!std::filesystem::exists(attachment_source)||!a->store.list(u8"附件存储验证").empty())throw std::runtime_error("Cancel attachment modified source");
        a->open_editor();a->title->value(u8"附件存储验证");a->attachment_input->value(attachment_source.u8string().c_str());a->attachment_add->do_callback();a->save_edit();auto attached=a->store.list(u8"附件存储验证");if(attached.size()!=1||a->store.attachments(attached[0].id).size()!=1)throw std::runtime_error("Attachment UI save failed");
        a->window->resize(a->window->x(),a->window->y(),500,780);a->filter=0;a->search->value("");a->refresh();
        auto restore_source=a->data/"restore-ui.sqlite3";a->store.backup(restore_source);auto original_count=a->store.list().size();a->store.add("temporary import change","",0);a->settings_panel->field("backup_path",restore_source.u8string());std::weak_ptr<DeliveryQueue> old_queue=a->deliveries;a->settings_action("import");if(a->page!=Page::Confirm)throw std::runtime_error("Import did not request inline confirmation");a->confirm_accept->do_callback();if(a->page!=Page::Home||a->store.list().size()!=original_count||!old_queue.expired())throw std::runtime_error("Import flow or queue reset failed");
        a->check_new_interactions();a->store.backup(a->data/"ui-backup.sqlite3");a->window->hide();a->show();int business_windows=0;for(auto* w=Fl::first_window();w;w=Fl::next_window(w))if(w!=a->ball.get())++business_windows;if(business_windows!=1)throw std::runtime_error("Business popup created");
        std::ofstream report(a->data/"ui-smoke.txt");report<<"UI callbacks: PASS\nInline editor/calendar/reminder/settings: PASS\nSingle business window: PASS\nSubtask save/cancel and preserve running parent: PASS\nLogo mode switch, board/list and search: PASS\nFive responsive sizes and card bounds: PASS\nLong Chinese title wraps: PASS\nSettings back retains draft: PASS\nSynthetic Ctrl+Enter: PASS\nPin selected feedback, dark palette and settings scrolling: PASS\nLocal organizer preview invalidation/commit and attachment cancel/save: PASS\nBackup creation/import, notification queue reset and ball configuration: PASS\nTray available: "<<platform_tray_available()<<"\nHide/show: PASS\nReal pointer, IME and mixed-monitor DPI: manual verification required\n";report.close();Fl::add_timeout(25,smoke_finish,a);
    }catch(const std::exception& e){std::ofstream(a->data/"ui-smoke.txt")<<"FAIL: "<<e.what()<<'\n';a->result=1;a->quit=true;}}
    static void smoke_finish(void* p){auto* a=static_cast<Impl*>(p);if(a->pending_notifications.count(0)){std::ofstream(a->data/"ui-exit.txt")<<"FAIL: notification callback never resolved\n";a->result=1;}else{a->capture("notification-result-render.ppm");std::ofstream(a->data/"ui-smoke.txt",std::ios::app)<<"Native round window region: PASS\nRound region after ball/main hide/show, move and 150 percent scale: PASS\nMain-page subtask check/reopen, board/narrow and draft protection: PASS\nMultiline padding and soft scrollbar drag: PASS\nInline menu start/urgent/shelve/resume/complete/delete/cancel: PASS\nFree-ball hover and draft preservation: PASS\nHappy/tired/urgent mood priority: PASS\nFallback acknowledgement and snooze: PASS\nSystem notification result: "<<(a->alerts.count(0)?"application fallback":"system confirmed")<<"\n";std::ofstream(a->data/"ui-exit.txt")<<"Smoke requested clean exit\n";}a->quit=true;}
    int run(){show();platform_opacity(window.get(),number("opacity",100));if(store.setting("desktop","0")=="1")guard([&]{apply_setting("desktop");});if(store.setting("hotkey_enabled","0")=="1")guard([&]{apply_setting("hotkey_enabled");});apply_ball();if(!smoke&&flag(1)&&(platform_tray_available()||ball->shown()))window->hide();if(smoke)Fl::add_timeout(0.8,smoke_tick,this);while(!quit){Fl::wait(30);process_deliveries();}deliveries.reset();return result;}
};
Ui::Ui(Store& store,const std::filesystem::path& data,bool smoke,const std::filesystem::path& anchor,InstanceLock* instance):impl_(std::make_unique<Impl>(store,data,smoke,anchor,instance)){}
Ui::~Ui()=default;
int Ui::run(){return impl_->run();}
