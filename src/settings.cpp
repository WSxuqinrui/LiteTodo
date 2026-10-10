#include "settings.h"
#include "platform.h"
#include "raster.h"
#include <FL/Fl_Value_Slider.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <stdexcept>
using namespace Style;
struct SettingsPanel::Row {
    SettingsPanel* owner;int section,kind;std::string key,fallback;
    TextLabel *title,*note;Fl_Box* separator;
    std::vector<Fl_Widget*> controls;std::vector<std::string> commands,choices;
};
namespace {
TextLabel* text(const char* value,int size,Fl_Color color){auto* label=new TextLabel(0,0,1,1);label->copy_label(value);label->labelsize(size);label->labelcolor(color);label->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);return label;}
int number(const std::string& value,int fallback=0){try{return std::stoi(value);}catch(...){return fallback;}}
const char* sections[]={u8"通用",u8"外观",u8"浮球",u8"提醒",u8"数据与备份",u8"本地整理"};
const Fl_Color accents[]={fl_rgb_color(57,106,212),fl_rgb_color(59,143,184),fl_rgb_color(174,114,49),fl_rgb_color(171,89,139),fl_rgb_color(62,135,103)};
class AccentButton:public Fl_Button {
public:
    AccentButton():Fl_Button(0,0,1,1){}
    void draw()override{Raster image(w(),h(),surface);image.round(1,1,w()-2,h()-2,10,color());if(value()){double cx=w()/2.0,cy=h()/2.0;image.line(cx-7,cy,cx-2,cy+5,2,FL_WHITE);image.line(cx-2,cy+5,cx+8,cy-6,2,FL_WHITE);}image.draw(x(),y());if(!value()){fl_color(FL_WHITE);fl_font(FL_HELVETICA,12);fl_draw(label(),x(),y(),w(),h(),FL_ALIGN_CENTER);}if(Fl::focus()==this){fl_color(blue);fl_rect(x(),y(),w(),h());}}
};
class SoftSlider:public Fl_Value_Slider {
public:
    SoftSlider():Fl_Value_Slider(0,0,1,1){box(FL_NO_BOX);slider_size(0.04);}
    void draw()override{fl_push_clip(x(),y(),w(),h());fl_color(surface);fl_rectf(x(),y(),w(),h());fl_font(FL_HELVETICA,12);fl_color(ink);auto text=std::to_string(static_cast<int>(value()))+"%";fl_draw(text.c_str(),x(),y(),35,h(),FL_ALIGN_CENTER);int left=x()+45,length=std::max(1,w()-55),center=y()+h()/2;double ratio=std::clamp((value()-minimum())/(maximum()-minimum()),0.0,1.0);pill(left,center-2,length,4,line);int at=left+static_cast<int>(ratio*length);if(at>left)pill(left,center-2,at-left,4,blue);pill(at-8,center-8,16,16,blue);pill(at-6,center-6,12,12,surface);if(Fl::focus()==this){fl_color(blue);fl_rect(x(),y(),w(),h());}fl_pop_clip();}
};
}
SettingsPanel::SettingsPanel(Store& store,const std::filesystem::path& data):SoftScroll(0,0,1,1),store_(store){
    const Icon icons[]={Icon::Settings,Icon::Sun,Icon::Ball,Icon::Clock,Icon::List,Icon::Check};
    for(int i=0;i<6;++i){landing[i]=new NavigationButton(icons[i],sections[i]);landing[i]->callback([](Fl_Widget* w,void* p){auto* panel=static_cast<SettingsPanel*>(p);for(int i=0;i<6;++i)if(panel->landing[i]==w)panel->choose(i);},this);}
    add_toggle(0,u8"开机自动启动",u8"登录后打开当前数据目录。移动程序后需重新设置。","autostart","0");
    add_toggle(0,u8"启动时最小化到托盘",u8"有托盘或浮球时在后台启动。","start_hidden","0");
    add_toggle(0,u8"关闭按钮最小化到托盘",u8"关闭主窗口后继续提醒；可在这里彻底退出。","close_to_tray","1");
    add_toggle(0,u8"按交付时间排序",u8"加急始终靠前；关闭后按添加时间排序。","sort_due","1");
    add_choices(0,u8"启动界面",u8"选择下次启动时显示的界面。","startup_mode",{u8"快捷面板",u8"控制台"});add_action(0,u8"退出应用",u8"退出后不再发送提醒。",{{u8"退出轻待办","exit"}});
    add_choices(1,u8"主题",u8"跟随系统，或使用固定的浅色、深色主题。","theme",{u8"跟随系统",u8"浅色",u8"深色"});
    add_choices(1,u8"主题配色",u8"按钮、选中状态和进度统一使用这款配色。","accent",{u8"蓝",u8"青",u8"橙",u8"粉",u8"绿"},true);
    auto row=std::make_unique<Row>();row->owner=this;row->section=1;row->kind=3;row->key="opacity";row->fallback="100";row->title=text(u8"面板透明度",14,ink);row->note=text(u8"调整整个主窗口的透明度，最低 45%。",12,muted);row->separator=new Fl_Box(FL_FLAT_BOX,0,0,1,1,nullptr);row->separator->color(line);auto* slider=new SoftSlider;slider->type(FL_HOR_NICE_SLIDER);slider->bounds(45,100);slider->step(1);slider->color(canvas);slider->selection_color(blue);slider->textcolor(ink);slider->textsize(12);slider->callback(callback,row.get());row->controls.push_back(slider);rows_.push_back(std::move(row));
    add_toggle(1,u8"贴在桌面",u8"将面板放到桌面层；可在设置或托盘取消。此模式与置顶互斥。","desktop","0");add_toggle(1,u8"紧凑卡片",u8"减少卡片留白，保留更大、更清晰的任务标题。","dense","0");
    add_toggle(2,u8"启用浮球",u8"在桌面显示任务入口；右键进入当前窗口新增页。","floating","1");add_choices(2,u8"浮球样式",u8"小团子会随任务变表情：空闲微笑，8 项起疲惫，加急或逾期冒火。","ball_style",{u8"经典 · 数量",u8"小团子 · 表情"});add_choices(2,u8"浮球配色",u8"单独选择浮球颜色。","ball_accent",{u8"蓝",u8"青",u8"橙",u8"粉",u8"绿"},true);
    add_toggle(2,u8"线框模式",u8"减少浮球填色，使用描边和浅色背景。","ball_wire","0");add_toggle(2,u8"贴边隐藏",u8"拖到屏幕左、右或顶部时收起，鼠标经过后展开。","ball_edge","0");add_toggle(2,u8"鼠标经过展开待办",u8"停留片刻展开任务清单，贴边与自由浮球都有效；保留当前编辑页。","ball_hover","0");add_toggle(2,u8"双击开合面板",u8"双击浮球打开主窗口，再双击隐藏到托盘。","ball_double","0");add_action(2,u8"浮球位置",u8"恢复到当前屏幕右上方。",{{u8"放回原位","ball_reset"}});
    add_toggle(2,u8"伙伴互动提示",u8"点击和提醒时显示本地短句，不联网聊天。","ball_chat","0");add_toggle(2,u8"拖文件成为待办",u8"拖入文件生成任务草稿，保存后复制为附件。","ball_drop","1");
    add_toggle(3,u8"启用系统提醒",u8"交付日期与提醒独立。实际展示受系统权限和勿扰影响。","notifications","1");add_action(3,u8"通知测试",u8"等待系统反馈，未显示时使用应用内提醒；可直接打开系统通知设置。",{{u8"发送测试通知","notify"}});
    add_toggle(4,u8"自动备份",u8"每天一份，保留最近 7 份自动备份；手工备份另外保留。","auto_backup","0");add_action(4,u8"立即备份",u8"创建独立快照，不覆盖已有备份。",{{u8"立即备份","backup_now"}});
    add_path(4,u8"换机与找回",u8"导入前自动保存当前数据。备份文件只含数据库，附件需一并复制。","backup_path",{{u8"导出一份","export"},{u8"导入备份","import"}});
    add_path(4,u8"数据目录",u8"更改时复制到新目录并记住位置，原目录保留。新目录不能已有任务库。","data_path",{{u8"打开文件夹","open_data"},{u8"更改位置","move_data"}});
    add_path(4,u8"附件目录",u8"新附件保存在这里，已有附件仍保留原位置。","attachment_path",{{u8"打开文件夹","open_attachments"},{u8"更改位置","move_attachments"}});
    add_toggle(5,u8"启用本地整理",u8"按行拆分清单，识别今天、明天、后天和加急。确认草稿后才写入任务。","organize","1");add_action(5,u8"快速整理",u8"先复制文字，再粘贴到整理页；全程在本机处理，无需账号。",{{u8"打开本地整理","organize"}});add_toggle(5,u8"启用全局快捷键",u8"先复制文字，再按快捷键进入本地整理。","hotkey_enabled","0");add_action(5,u8"快捷键",u8"点击后按 Ctrl / Alt 与字母的组合；Esc 取消录制。",{{u8"Ctrl+Alt+N · 点击录制","record_hotkey"}});
    end();paths(data);reload();choose(-1);
}
SettingsPanel::~SettingsPanel()=default;
void SettingsPanel::add_toggle(int section,const char* title,const char* note,const char* key,const char* fallback){auto row=std::make_unique<Row>();row->owner=this;row->section=section;row->kind=0;row->key=key;row->fallback=fallback;row->title=text(title,14,ink);row->note=text(note,12,muted);row->separator=new Fl_Box(FL_FLAT_BOX,0,0,1,1,nullptr);row->separator->color(line);auto* toggle=new Toggle;toggle->callback(callback,row.get());row->controls.push_back(toggle);rows_.push_back(std::move(row));}
void SettingsPanel::add_choices(int section,const char* title,const char* note,const char* key,const std::vector<std::string>& choices,bool colors){auto row=std::make_unique<Row>();row->owner=this;row->section=section;row->kind=colors?2:1;row->key=key;row->fallback=std::string(key)=="startup_mode"?"quick":std::string(key)=="ball_style"?"1":"0";row->choices=choices;row->title=text(title,14,ink);row->note=text(note,12,muted);row->separator=new Fl_Box(FL_FLAT_BOX,0,0,1,1,nullptr);row->separator->color(line);for(const auto& choice:choices){auto* b=colors?static_cast<Fl_Button*>(new AccentButton):button(0,0,1,1,choice.c_str());b->copy_label(choice.c_str());b->callback(callback,row.get());row->controls.push_back(b);}rows_.push_back(std::move(row));}
void SettingsPanel::add_action(int section,const char* title,const char* note,const std::vector<std::pair<std::string,std::string>>& actions){auto row=std::make_unique<Row>();row->owner=this;row->section=section;row->kind=4;row->title=text(title,14,ink);row->note=text(note,12,muted);row->separator=new Fl_Box(FL_FLAT_BOX,0,0,1,1,nullptr);row->separator->color(line);for(const auto& action:actions){auto* b=button(0,0,1,1,action.first.c_str());b->copy_label(action.first.c_str());b->callback(callback,row.get());row->controls.push_back(b);row->commands.push_back(action.second);}rows_.push_back(std::move(row));}
void SettingsPanel::add_path(int section,const char* title,const char* note,const char* key,const std::vector<std::pair<std::string,std::string>>& actions){add_action(section,title,note,actions);auto& row=rows_.back();row->kind=5;row->key=key;auto* path=new KeyInput(0,0,1,1,u8"输入完整路径");input(path);path->maximum_size(4096);row->controls.insert(row->controls.begin(),path);}
void SettingsPanel::callback(Fl_Widget* w,void* p){auto* row=static_cast<Row*>(p);auto* owner=row->owner;try{
    if(row->kind==4||row->kind==5){size_t index=0;while(index<row->controls.size()&&row->controls[index]!=w)++index;if(row->kind==5)--index;auto command=row->commands.at(index);if(command=="record_hotkey"){owner->recording_=true;w->take_focus();w->copy_label(u8"请按快捷键…");w->redraw();}else if(owner->action)owner->action(command);return;}
    std::string value;if(row->kind==0)value=static_cast<Toggle*>(w)->value()?"1":"0";else if(row->kind==3)value=std::to_string(static_cast<int>(static_cast<Fl_Value_Slider*>(w)->value()));else{size_t i=0;while(i<row->controls.size()&&row->controls[i]!=w)++i;value=row->key=="startup_mode"?(i?"console":"quick"):std::to_string(i);}
    auto previous=owner->store_.setting(row->key,row->fallback);if(row->key=="autostart"){auto directory=owner->actual_data_.u8string();if(!platform_set_autostart(value=="1",directory))throw std::runtime_error(u8"系统未能保存自动启动设置");}
    else{owner->store_.set_setting(row->key,value);try{if(owner->changed)owner->changed(row->key);}catch(...){owner->store_.set_setting(row->key,previous);throw;}}
    owner->reload();if(owner->message)owner->message(u8"设置已保存");
}catch(const std::exception& e){owner->reload();if(owner->message)owner->message(e.what());}}
void SettingsPanel::reload(){for(auto& row:rows_){if(row->kind==0){auto* toggle=static_cast<Toggle*>(row->controls[0]);toggle->value(row->key=="autostart"?platform_autostart_enabled():store_.setting(row->key,row->fallback)=="1");bool supported=(row->key!="autostart"||platform_autostart_supported())&&(row->key!="desktop"||platform_desktop_supported())&&(row->key!="hotkey_enabled"||platform_hotkey_supported());if(supported)toggle->activate();else{toggle->deactivate();row->note->copy_label(row->key=="desktop"?u8"暂未开放：需要完善桌面重启时的窗口恢复。":u8"当前平台暂未适配此项，其他功能仍可使用。");}}
    else if(row->kind==1||row->kind==2){auto value=store_.setting(row->key,row->fallback);int picked=row->key=="startup_mode"?(value=="console"):number(value);for(size_t i=0;i<row->controls.size();++i){auto* b=static_cast<Fl_Button*>(row->controls[i]);if(row->kind==2){b->value(static_cast<int>(i)==picked);b->color(accents[i]);b->labelcolor(FL_WHITE);b->copy_label(static_cast<int>(i)==picked?u8"✓":row->choices[i].c_str());}else{b->color(static_cast<int>(i)==picked?pale:canvas);b->labelcolor(static_cast<int>(i)==picked?blue:ink);}b->redraw();}}
    else if(row->kind==3)static_cast<Fl_Value_Slider*>(row->controls[0])->value(std::clamp(number(store_.setting(row->key,row->fallback),100),45,100));}
    for(auto& row:rows_){if(!row->commands.empty()&&row->commands[0]=="record_hotkey"&&!recording_){int mods=number(store_.setting("hotkey_mod","3"),3),key=number(store_.setting("hotkey_key","78"),78);std::string caption=(mods&2)?"Ctrl+":"";caption+=(mods&1)?"Alt+":"";caption+=(mods&4)?"Shift+":"";caption+=static_cast<char>(key);caption+=u8" · 点击录制";row->controls[0]->copy_label(caption.c_str());}if(!row->commands.empty()&&row->commands[0]=="backup_now"){auto note=u8"上次："+store_.setting("last_backup",u8"尚未备份")+u8"。手工备份独立保留。";row->note->copy_label(note.c_str());}}
    redraw();}
void SettingsPanel::choose(int next){section=std::clamp(next,-1,5);recording_=false;scroll_to(0,0);for(int i=0;i<6;++i){if(section<0)landing[i]->show();else landing[i]->hide();}for(auto& row:rows_){bool shown=row->section==section;for(auto* w:row->controls){if(shown)w->show();else w->hide();}for(auto* w:std::array<Fl_Widget*,3>{row->title,row->note,row->separator}){if(shown)w->show();else w->hide();}}arrange(x(),y(),w(),h());if(changed)changed("section");}
void SettingsPanel::scroll_to(int,int target){int height=section<0?384:4;for(auto& row:rows_)if(row->section==section)height+=(row->kind==5?186:row->kind==0?124:140)+4;Fl_Scroll::scroll_to(0,std::clamp(target,0,std::max(0,height-h())));}
void SettingsPanel::arrange(int x,int y,int w,int h){Fl_Scroll::resize(x,y,w,h);scroll_to(0,yposition());int left=x+8,width=std::max(220,w-26),top=y-yposition()+4;for(int i=0;i<6;++i)landing[i]->resize(left,top+i*64,width,56);for(auto& row:rows_)if(row->section==section){int height=row->kind==5?186:row->kind==0?124:140;row->title->resize(left+8,top+8,width-(row->kind==0?80:16),26);row->note->resize(left+8,top+40,width-(row->kind==0?80:16),64);
    if(row->kind==0){row->note->resize(left+8,top+40,width-16,44);row->controls[0]->resize(left+width-56,top+86,48,28);}
    else if(row->kind==3)row->controls[0]->resize(left+8,top+100,width-16,28);
    else{int count=static_cast<int>(row->controls.size())-(row->kind==5?1:0);int gap=8,bw=(width-16-(count-1)*gap)/count;int base=row->kind==5?142:98;if(row->kind==5)row->controls[0]->resize(left+8,top+94,width-16,36);for(int i=0;i<count;++i)row->controls[i+(row->kind==5?1:0)]->resize(left+8+i*(bw+gap),top+base,bw,34);}
    row->separator->resize(left+8,top+height-1,width-16,1);top+=height+4;}redraw();}
void SettingsPanel::paths(const std::filesystem::path& data){actual_data_=data;field("data_path",data.u8string());field("attachment_path",store_.setting("attachment_dir",(data/"attachments").u8string()));field("backup_path",(data/("backup-"+std::to_string(now_seconds())+".sqlite3")).u8string());}
std::string SettingsPanel::field(const std::string& key){for(auto& row:rows_)if(row->kind==5&&row->key==key)return static_cast<KeyInput*>(row->controls[0])->value();return "";}
void SettingsPanel::field(const std::string& key,const std::string& value){for(auto& row:rows_)if(row->kind==5&&row->key==key)static_cast<KeyInput*>(row->controls[0])->value(value.c_str());}
bool SettingsPanel::capture(int key,int state){
    if(!recording_)return false;
    if(key==FL_Escape){recording_=false;reload();return true;}
    if(key==FL_Control_L||key==FL_Control_R||key==FL_Alt_L||key==FL_Alt_R||key==FL_Shift_L||key==FL_Shift_R)return true;
    if(!(state&(FL_CTRL|FL_ALT))||!((key>='a'&&key<='z')||(key>='A'&&key<='Z')||(key>='0'&&key<='9'))){if(message)message(u8"请使用 Ctrl 或 Alt 加字母/数字");return true;}
    if(key>='a'&&key<='z')key-=32;
    auto old_mod=store_.setting("hotkey_mod","3"),old_key=store_.setting("hotkey_key","78");
    int modifiers=((state&FL_ALT)?1:0)|((state&FL_CTRL)?2:0)|((state&FL_SHIFT)?4:0);
    try{store_.set_setting("hotkey_mod",std::to_string(modifiers));store_.set_setting("hotkey_key",std::to_string(key));if(changed)changed("hotkey_enabled");recording_=false;}
    catch(const std::exception& e){store_.set_setting("hotkey_mod",old_mod);store_.set_setting("hotkey_key",old_key);recording_=false;if(message)message(e.what());}
    reload();return true;
}
