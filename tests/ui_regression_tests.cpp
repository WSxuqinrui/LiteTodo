// 原界面/存储的隔离回归；平台接口替换为无系统副作用的桩。
#include <filesystem>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#define private public
#include "../src/ui.h"
#undef private
#include "../src/ui.cpp"

static int opacity_calls=0,topmost_calls=0;
bool platform_topmost(Fl_Window*,bool enabled){if(enabled)++topmost_calls;return true;}
bool platform_init(const PlatformCallbacks&){return true;}
bool platform_notify(const std::string&,const std::string&,std::function<void(bool)>){return false;}
const char* platform_notification_feedback(){return "isolated test";}
const char* platform_notification_confirmation(){return "isolated test";}
bool platform_open_notification_settings(){return false;}
bool platform_notification_regression_test(){return true;}
bool platform_tray_available(){return true;}
void platform_window_icon(Fl_Window*){}
bool platform_autostart_supported(){return false;}
bool platform_autostart_enabled(){return false;}
bool platform_set_autostart(bool,const std::string&){return false;}
bool platform_system_dark(){return false;}
bool platform_opacity(Fl_Window*,int){++opacity_calls;return true;}
bool platform_desktop_supported(){return false;}
bool platform_desktop(Fl_Window*,bool){return false;}
bool platform_open_folder(const std::filesystem::path&){return false;}
bool platform_hotkey_supported(){return false;}
bool platform_hotkey(bool,unsigned int,int){return true;}
void platform_poll(){}
void platform_shutdown(){}
const char* platform_tray_description(){return "isolated test";}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    auto data=std::filesystem::temp_directory_path()/("litetodo-ui-regression-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(data);int failures=0,checks=0;
    {Store store(data/"tasks.sqlite3");Ui::Impl app(store,data,false,data);Fl::remove_timeout(Ui::Impl::tick,&app);
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++checks;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}app.cancel_edit();};
    test("file drop cannot change existing draft",[&]{app.open_editor();app.title->value("existing");auto file=data/"drop.txt";std::ofstream(file)<<"sample";app.ball->dropped(file.u8string());require(app.pending_files.empty()&&std::string(app.title->value())=="existing","Rejected drop still changed old draft");});
    test("hover restores hidden editor page",[&]{app.open_editor();app.title->value("hidden draft");app.window->hide();app.ball->hover_open();require(app.page==Page::Edit&&app.draft_active,"Hover navigated away from hidden editor");});
    test("child drag cannot delete another item",[&]{app.open_editor();app.child_list->items={{1,"first",false},{2,"second",false}};app.edit_scroll->scroll_to(0,400);Fl::check();Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;Fl::e_x=app.child_list->x()+20;Fl::e_y=app.child_list->y()+19;app.child_list->handle(FL_PUSH);Fl::e_x=app.child_list->x()+app.child_list->w()-20;Fl::e_y=app.child_list->y()+57;app.child_list->handle(FL_DRAG);app.child_list->handle(FL_RELEASE);require(app.child_list->items.size()==2&&!app.child_list->items[0].done,"Drag deleted or checked a child");});
    test("child click still completes intended item",[&]{app.open_editor();app.child_list->items={{0,"one",false}};app.edit_scroll->scroll_to(0,400);Fl::check();Fl::e_keysym=FL_Button+FL_LEFT_MOUSE;Fl::e_x=app.child_list->x()+20;Fl::e_y=app.child_list->y()+19;app.child_list->handle(FL_PUSH);app.child_list->handle(FL_RELEASE);require(app.child_list->items.size()==1&&app.child_list->items[0].done,"Normal child click stopped working");});
    test("child deletion clamps scroll offset",[&]{app.open_editor();for(int i=0;i<20;++i)app.child_list->items.push_back({0,"child",false});app.child_list->offset=20*38-app.child_list->h();app.child_list->action(19,2);require(app.child_list->offset<=19*38-app.child_list->h(),"Offset exceeded reduced child list");});
    test("attachment deletion clamps scroll offset",[&]{app.open_editor();for(int i=0;i<20;++i)app.draft_files.push_back({"file","C:/demo/file"});app.update_files();app.attachment_list->offset=20*38-app.attachment_list->h();app.attachment_list->action(19,2);require(app.attachment_list->offset<=19*38-app.attachment_list->h(),"Offset exceeded reduced attachment list");});
    test("show reapplies opacity",[&]{store.set_setting("opacity","80");app.window->hide();int previous=opacity_calls;app.show();require(opacity_calls>previous,"Show did not restore opacity");});
    test("ball show reapplies topmost",[&]{app.ball->hide();int previous=topmost_calls;app.ball->show();require(topmost_calls>previous,"Ball recreated without restoring topmost");});
    test("snooze cannot reauthorize stale draft",[&]{auto id=store.add("before snooze","",now_seconds()-1);auto task=store.get(id);app.open_editor(&task);Store other(data/"tasks.sqlite3");other.update(id,"other edit","",task.remind_at);app.present_alert({id,task.remind_at,false});app.dismiss_alert(true);bool blocked=false;try{app.save_edit();}catch(...){blocked=true;}require(blocked&&store.get(id).title=="other edit"&&app.draft_active,"Snooze refreshed stale draft token and overwrote other edit");});
    test("stale editor delete cannot remove replacement",[&]{auto id=store.add("old delete draft","",0);auto task=store.get(id);app.open_editor(&task);app.delete_button->do_callback();Store other(data/"tasks.sqlite3");other.remove(id);auto next=other.add("replacement","",0);bool blocked=false;try{app.confirmed_action();}catch(...){blocked=true;}require(blocked&&store.get(next).title=="replacement","Stale editor deleted replacement task");});
    app.window->hide();app.ball->hide();}
    std::filesystem::remove_all(data);std::cout<<checks<<" UI regression checks passed, "<<failures<<" failed\n";return failures?1:0;
}
