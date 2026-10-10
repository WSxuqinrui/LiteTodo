#include "widgets.h"
#include "dates.h"
#include "raster.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/fl_utf8.h>
#include <algorithm>
namespace Style {
static const auto input_box=static_cast<Fl_Boxtype>(FL_FREE_BOXTYPE);
static const auto note_box=static_cast<Fl_Boxtype>(FL_FREE_BOXTYPE+1);
void border(int x,int y,int w,int h,Fl_Color color){fl_draw_box(FL_RFLAT_BOX,x,y,w,h,line);fl_draw_box(FL_RFLAT_BOX,x+1,y+1,w-2,h-2,color);}
void fonts(){
    Fl::set_boxtype(input_box,border,14,4,28,8);
    // 多行框的文本、光标、选区及鼠标定位共同使用此内边距，避免只移绘制文字。
    Fl::set_boxtype(note_box,border,14,12,28,24);
#ifdef _WIN32
    Fl::set_font(FL_HELVETICA," Microsoft YaHei");Fl::set_font(FL_HELVETICA_BOLD,"BMicrosoft YaHei");
#elif defined(__APPLE__)
    Fl::set_font(FL_HELVETICA,"PingFang SC");Fl::set_font(FL_HELVETICA_BOLD,"PingFang SC Semibold");
#else
    Fl::set_font(FL_HELVETICA,"Noto Sans CJK SC");Fl::set_font(FL_HELVETICA_BOLD,"Noto Sans CJK SC Bold");
#endif
    Fl::scheme("gtk+");fl_message_font(FL_HELVETICA,14);
}
void input(Fl_Input* f){f->box(f->input_type()==FL_MULTILINE_INPUT?note_box:input_box);f->color(surface);f->textcolor(ink);f->cursor_color(blue);f->textsize(f->input_type()==FL_MULTILINE_INPUT?14:13);f->textfont(FL_HELVETICA);f->maximum_size(2048);}
Fl_Button* button(int x,int y,int w,int h,const char* text,bool primary){auto* b=new Fl_Button(x,y,w,h,text);b->box(FL_RFLAT_BOX);b->color(primary?blue:canvas);b->selection_color(pale);b->labelcolor(primary?FL_WHITE:ink);b->labelsize(13);return b;}
}
using namespace Style;
KeyInput::KeyInput(int x,int y,int w,int h,const char* hint):Fl_Input(x,y,w,h),hint_(hint){}
int KeyInput::handle(int event){if(event==FL_KEYBOARD&&!Fl::compose_state&&shortcut&&shortcut(Fl::event_key(),Fl::event_state()))return 1;return Fl_Input::handle(event);}
void KeyInput::draw(){Fl_Input::draw();if(!value()[0]&&Fl::focus()!=this){fl_push_clip(x()+14,y()+4,w()-28,h()-8);fl_color(muted);fl_font(textfont(),textsize());fl_draw(hint_,x()+15,y(),w()-30,h(),FL_ALIGN_LEFT|FL_ALIGN_INSIDE);fl_pop_clip();}}
int KeyNote::handle(int event){if(event==FL_KEYBOARD&&!Fl::compose_state&&shortcut&&shortcut(Fl::event_key(),Fl::event_state()))return 1;return Fl_Multiline_Input::handle(event);}
SoftScroll::SoftScroll(int x,int y,int w,int h):Fl_Scroll(x,y,w,h){type(VERTICAL);scrollbar_size(12);color(surface);resizable(nullptr);}
SoftScroll::Thumb SoftScroll::thumb()const{
    const int track_y=scrollbar.y()+6,track_h=std::max(1,scrollbar.h()-12);
    const int height=std::clamp(static_cast<int>(track_h*scrollbar.slider_size()),std::min(28,track_h),track_h);
    const double range=scrollbar.maximum()-scrollbar.minimum();
    const double ratio=range>0?std::clamp((scrollbar.value()-scrollbar.minimum())/range,0.0,1.0):0;
    const int width=hover_||dragging_?6:4;
    return {scrollbar.x()+(scrollbar.w()-width)/2,track_y+static_cast<int>(ratio*(track_h-height)),width,height,track_y,track_h};
}
void SoftScroll::draw(){
    Fl_Scroll::draw();if(!scrollbar.visible())return;
    // 覆盖系统风格的箭头和凹槽，内容仍按 FLTK 的真实可见区域裁切。
    fl_push_clip(scrollbar.x(),scrollbar.y(),scrollbar.w(),scrollbar.h());fl_color(surface);fl_rectf(scrollbar.x(),scrollbar.y(),scrollbar.w(),scrollbar.h());
    const auto bar=thumb();pill(bar.x,bar.y,bar.w,bar.h,hover_||dragging_?blue:fl_color_average(muted,surface,.45f));fl_pop_clip();
}
void SoftScroll::drag_to(int pointer_y){
    const auto bar=thumb();const int travel=bar.track_h-bar.h;
    const double ratio=travel>0?std::clamp((pointer_y-drag_offset_-bar.track_y)/static_cast<double>(travel),0.0,1.0):0;
    const int target=static_cast<int>(scrollbar.minimum()+ratio*(scrollbar.maximum()-scrollbar.minimum())+.5);
    scroll_to(xposition(),target);redraw();
}
int SoftScroll::handle(int event){
    const bool over=scrollbar.visible()&&Fl::event_inside(&scrollbar);
    if(event==FL_MOVE||event==FL_ENTER||event==FL_LEAVE){const bool hover=event!=FL_LEAVE&&over;if(hover!=hover_){hover_=hover;redraw();}if(over)return 1;}
    if(event==FL_PUSH&&over&&Fl::event_button()==FL_LEFT_MOUSE){const auto bar=thumb();drag_offset_=Fl::event_y()>=bar.y&&Fl::event_y()<bar.y+bar.h?Fl::event_y()-bar.y:bar.h/2;dragging_=true;drag_to(Fl::event_y());return 1;}
    if(event==FL_DRAG&&dragging_){drag_to(Fl::event_y());return 1;}
    if(event==FL_RELEASE&&dragging_){dragging_=false;hover_=over;redraw();return 1;}
    if(event==FL_HIDE){dragging_=hover_=false;}
    return Fl_Scroll::handle(event);
}
AppWindow::AppWindow(int w,int h):Fl_Double_Window(w,h,u8"轻待办"){color(canvas);resizable(this);size_range(440,620);}
void AppWindow::resize(int x,int y,int w,int h){Fl_Double_Window::resize(x,y,w,h);if(layout)layout();}
int AppWindow::handle(int event){if(event==FL_PUSH&&before_pointer)before_pointer(Fl::event_x(),Fl::event_y());if((event==FL_SHORTCUT||event==FL_KEYBOARD)&&!Fl::compose_state&&shortcut&&shortcut(Fl::event_key(),Fl::event_state()))return 1;return Fl_Double_Window::handle(event);}
namespace {
// FLTK 默认以空格分词，连续中文不会自动换行。按 UTF-8 字符边界测量，
// 卡片最多三行，超长部分以省略号标识；点击任务仍可读取完整正文。
std::string wrapped(const std::string& text,int width,int max_lines){
    std::string result,line;int rows=1;size_t at=0;
    while(at<text.size()){
        int bytes=std::max(1,fl_utf8len(static_cast<unsigned char>(text[at])));bytes=std::min(bytes,static_cast<int>(text.size()-at));std::string character=text.substr(at,bytes);bool newline=character=="\n";auto candidate=line+character;
        if(newline||(!line.empty()&&fl_width(candidate.c_str())>width)){
            if(rows==max_lines)break;
            result+=line+'\n';line.clear();++rows;if(newline){at+=bytes;continue;}
        }
        line+=character;at+=bytes;
    }
    if(at<text.size()){const std::string dots=u8"…";while(!line.empty()&&fl_width((line+dots).c_str())>width){size_t last=line.size()-1;while(last>0&&(static_cast<unsigned char>(line[last])&0xc0)==0x80)--last;line.erase(last);}line+=dots;}
    return result+line;
}
int title_height(const std::string& text,int width){fl_font(FL_HELVETICA_BOLD,17);auto display=wrapped(text,std::max(40,width),3);int rows=1+static_cast<int>(std::count(display.begin(),display.end(),'\n'));return std::max(26,rows*fl_height());}
int child_height(const std::string& text,int width){fl_font(FL_HELVETICA,13);auto display=wrapped(text,std::max(40,width),2);int rows=1+static_cast<int>(std::count(display.begin(),display.end(),'\n'));return std::max(34,rows*fl_height()+10);}
int child_summary_y(const Task& task,int width,bool full){return 19+title_height(task.title,width-34)+(full?24:0)+(full&&!task.note.empty()?36:0)+24;}
void clipped(const std::string& text,int x,int y,int w,int h,int size,Fl_Color color,bool wrap=false){fl_push_clip(x,y,w,h);fl_font(FL_HELVETICA,size);fl_color(color);auto display=wrapped(text,w,wrap?std::max(1,h/fl_height()):1);fl_draw(display.c_str(),x,y,w,h,FL_ALIGN_LEFT|FL_ALIGN_INSIDE|(wrap?FL_ALIGN_TOP:0),nullptr,0);fl_pop_clip();}
std::string due_label(const Task& t){if(!t.due_at)return u8"未设交付日期";if(t.due_at==day_after(0))return u8"今天交付";if(t.due_at==day_after(1))return u8"明天交付";return date_text(t.due_at);}
}
void TextLabel::draw(){draw_box();fl_push_clip(x(),y(),w(),h());fl_font(labelfont(),labelsize());fl_color(labelcolor());auto text=wrapped(label()?label():"",w(),std::max(1,h()/std::max(1,fl_height())));fl_draw(text.c_str(),x(),y(),w(),h(),FL_ALIGN_LEFT|FL_ALIGN_INSIDE|((align()&FL_ALIGN_WRAP)?FL_ALIGN_TOP:0),nullptr,0);fl_pop_clip();}
TaskBoard::TaskBoard():Fl_Widget(0,0,10,10){}
void TaskBoard::toggle_children(long long parent){
    if(expanded.count(parent)){expanded.erase(parent);child_cache.erase(parent);}
    else if(load_children){auto children=load_children(parent);child_cache[parent]=std::move(children);expanded.insert(parent);}
    rebuild();
}
void TaskBoard::refresh_children(){
    // 只查询已展开且仍可见的父任务，避免每次刷新遍历全部子任务。
    child_cache.clear();for(auto at=expanded.begin();at!=expanded.end();){
        auto found=std::find_if(tasks.begin(),tasks.end(),[&](const Task& t){return t.id==*at&&t.child_count>0;});
        if(found==tasks.end())at=expanded.erase(at);else{if(load_children)child_cache[*at]=load_children(*at);++at;}
    }
}
std::vector<TaskBoard::ChildRow> TaskBoard::child_rows(const Card& card)const{
    std::vector<ChildRow> rows;const auto& task=tasks[card.index];auto found=child_cache.find(task.id);if(!expanded.count(task.id)||found==child_cache.end())return rows;
    int top=card.y+child_summary_y(task,card.w,full)+23;
    for(const auto& child:found->second){int height=child_height(child.title,card.w-72);rows.push_back({card.x+16,top,card.w-32,height,child});top+=height;}
    return rows;
}
void TaskBoard::rebuild(){
    pressed_child_=0;menu_id=0;cards.clear();int columns=board&&w()>=760?3:1;int gap=14,cw=(w()-gap*(columns-1)-8)/columns;std::array<int,3> heights{board?35:0,board?35:0,board?35:0};
    for(size_t i=0;i<tasks.size();++i){const auto& t=tasks[i];int column=columns==3?(t.done?2:t.in_progress?1:0):0;
        int height=title_height(t.title,cw-34)+81+(full&&!t.note.empty()?36:0)+(full?24:0)+(t.child_count?24:0);if(dense)height-=6;
        auto children=child_cache.find(t.id);if(expanded.count(t.id)&&children!=child_cache.end()){height+=8;for(const auto& child:children->second)height+=child_height(child.title,cw-72);}
        cards.push_back({column*(cw+gap)+4,heights[column],cw,height,i});heights[column]+=height+10;
    }
    total=*std::max_element(heights.begin(),heights.begin()+columns);offset=std::clamp(offset,0,std::max(0,total-h()));redraw();
}
void TaskBoard::resize(int x,int y,int w,int h){Fl_Widget::resize(x,y,w,h);rebuild();}
void TaskBoard::draw(){
    fl_push_clip(x(),y(),w(),h());fl_color(surface);fl_rectf(x(),y(),w(),h());
    if(board){int columns=w()>=760?3:1;int cw=(w()-14*(columns-1)-8)/columns;const char* names[]={u8"未开始",u8"进行中",u8"已完成"};for(int c=0;c<columns;++c){int n=0;for(const auto& t:tasks)if(columns==1||c==(t.done?2:t.in_progress?1:0))++n;clipped(std::string(columns==1?u8"全部状态":names[c])+"   "+std::to_string(n),x()+c*(cw+14)+4,y(),cw,26,12,muted);}}
    if(tasks.empty())clipped(u8"暂时没有待办，点「新增」记录下一件事",x()+24,y()+h()/2-15,w()-48,30,14,muted);
    for(size_t index=0;index<cards.size();++index){const auto& c=cards[index];int left=x()+c.x,top=y()+c.y-offset;if(top+c.h<y()||top>y()+h())continue;const auto& t=tasks[c.index];bool active=interactive&&(t.id==selected||static_cast<int>(index)==hover);
        fl_draw_box(FL_RFLAT_BOX,left,top,c.w,c.h,active?blue:line);fl_draw_box(FL_RFLAT_BOX,left+1,top+1,c.w-2,c.h-2,surface);
        int th=title_height(t.title,c.w-34);fl_push_clip(left+16,top+14,c.w-32,th);fl_font(FL_HELVETICA_BOLD,17);fl_color(t.done?muted:ink);auto title=wrapped(t.title,c.w-34,3);fl_draw(title.c_str(),left+16,top+14,c.w-32,th,FL_ALIGN_LEFT|FL_ALIGN_TOP|FL_ALIGN_INSIDE,nullptr,0);fl_pop_clip();
        int meta=top+19+th;if(full){std::string source=t.source.empty()?u8"无来源":t.source;source+=u8" · "+(t.category.empty()?std::string(u8"未分类"):t.category);clipped(source,left+16,meta,c.w-32,18,11,muted);meta+=24;
            if(!t.note.empty()){clipped(t.note,left+16,meta,c.w-32,36,11,muted,true);meta+=36;}}
        Fl_Color tone=!t.done&&t.due_at&&t.due_at<day_after(0)?red:t.due_at==day_after(0)?blue:muted;
        draw_icon(Icon::Calendar,left+16,meta+3,12,tone);clipped(due_label(t),left+34,meta,c.w-52,18,11,tone);meta+=24;
        if(t.child_count){std::string progress=u8"子任务 "+std::to_string(t.child_done)+" / "+std::to_string(t.child_count);clipped(progress,left+16,meta,100,20,11,interactive?blue:muted);fl_color(line);fl_rectf(left+118,meta+9,std::max(12,c.w-155),3);fl_color(blue);fl_rectf(left+118,meta+9,std::max(12,c.w-155)*t.child_done/t.child_count,3);
            if(interactive){int cx=left+c.w-26,cy=meta+10;fl_color(blue);if(expanded.count(t.id)){fl_line(cx-4,cy+2,cx,cy-2);fl_line(cx,cy-2,cx+4,cy+2);}else{fl_line(cx-4,cy-2,cx,cy+2);fl_line(cx,cy+2,cx+4,cy-2);}}
        }
        for(const auto& row:child_rows(c)){int rx=x()+row.x,ry=y()+row.y-offset;if(ry+row.h<y()||ry>y()+h())continue;
            int cy=ry+row.h/2;fl_color(line);fl_line(rx+27,ry+row.h-1,rx+row.w,ry+row.h-1);
            pill(rx+1,cy-8,16,16,row.child.done?blue:line);if(row.child.done)draw_icon(Icon::Check,rx+2,cy-7,14,FL_WHITE);else pill(rx+2,cy-7,14,14,surface);
            clipped(row.child.title,rx+28,ry+5,row.w-36,row.h-10,13,row.child.done?muted:ink,true);
        }
        if(t.shelved||t.urgent){clipped(t.shelved?u8"已搁置":u8"加急",left+16,top+c.h-35,c.w-115,26,11,t.shelved?muted:red);}
        if(interactive){int by=top+c.h-40;for(int k=0;k<2;++k){if(k==0&&t.done)continue;int bx=left+c.w-78+k*36;fl_draw_box(FL_RFLAT_BOX,bx,by,30,30,line);fl_draw_box(FL_RFLAT_BOX,bx+1,by+1,28,28,k?pale:surface);draw_icon(k?Icon::More:t.in_progress?Icon::Pause:Icon::Play,bx+7,by+7,16,k?blue:muted);}}

    }
    if(total>h()){int thumb=std::max(22,h()*h()/total);fl_color(line);fl_rectf(x()+w()-3,y(),3,h());fl_color(muted);fl_rectf(x()+w()-3,y()+offset*(h()-thumb)/(total-h()),3,thumb);}
    if(menu_id){auto found=std::find_if(tasks.begin(),tasks.end(),[&](const Task& t){return t.id==menu_id;});if(found!=tasks.end()){
        int mx=x()+menu_x,my=y()+menu_y;fl_draw_box(FL_RFLAT_BOX,mx+2,my+3,158,168,line);fl_draw_box(FL_RFLAT_BOX,mx,my,158,168,line);fl_draw_box(FL_RFLAT_BOX,mx+1,my+1,156,166,surface);
        const char* labels[]={found->done?u8"恢复任务":u8"完成",found->urgent?u8"取消加急":u8"加急",found->shelved?u8"恢复待办":u8"搁置",u8"删除"};Icon icons[]={Icon::Check,Icon::Lightning,Icon::Pause,Icon::Trash};
        for(int i=0;i<4;++i){int ry=my+7+i*38;if(i==menu_hover)fl_draw_box(FL_RFLAT_BOX,mx+5,ry,148,34,pale);if(i==3){fl_color(line);fl_line(mx+12,ry-3,mx+146,ry-3);}auto color=i==3?red:(found->done&&i>0&&i<3)?muted:ink;draw_icon(icons[i],mx+15,ry+9,16,color);clipped(labels[i],mx+43,ry,104,34,13,color);}
    }}
    fl_pop_clip();
}
void TaskBoard::open_menu(size_t index){if(index>=cards.size())return;const auto& c=cards[index];menu_id=tasks[c.index].id;menu_hover=-1;menu_x=std::clamp(c.x+c.w-162,0,std::max(0,w()-162));int below=c.y-offset+c.h-7;menu_y=std::clamp(below+168<=h()?below:c.y-offset+c.h-213,0,std::max(0,h()-172));redraw();}
bool TaskBoard::menu_bounds_valid()const{return !menu_id||(menu_x>=0&&menu_y>=0&&menu_x+162<=w()&&menu_y+172<=h());}
int TaskBoard::handle(int event){
    if(event==FL_FOCUS)return 1;
    if(event==FL_UNFOCUS){pressed_child_=0;menu_id=0;redraw();return 1;}
    if(event==FL_MOUSEWHEEL){pressed_child_=0;menu_id=0;offset=std::clamp(offset+Fl::event_dy()*40,0,std::max(0,total-h()));redraw();return 1;}
    if(!interactive)return Fl_Widget::handle(event);
    if(menu_id){int px=Fl::event_x()-x()-menu_x,py=Fl::event_y()-y()-menu_y;bool inside=px>=0&&px<158&&py>=0&&py<168;int item=inside&&py>=7&&py<159?(py-7)/38:-1;
        if(event==FL_MOVE||event==FL_ENTER){menu_hover=item;redraw();return 1;}
        if(event==FL_KEYBOARD){if(Fl::event_key()==FL_Escape){menu_id=0;redraw();return 1;}if(Fl::event_key()==FL_Up||Fl::event_key()==FL_Down){menu_hover=(menu_hover+(Fl::event_key()==FL_Up?3:1)+4)%4;redraw();return 1;}if(Fl::event_key()==FL_Enter)item=std::max(0,menu_hover);else return Fl_Widget::handle(event);}
        if((event==FL_RELEASE&&Fl::event_button()==FL_LEFT_MOUSE)||event==FL_KEYBOARD){auto found=std::find_if(tasks.begin(),tasks.end(),[&](const Task& t){return t.id==menu_id;});Task task=found==tasks.end()?Task{}:*found;menu_id=0;redraw();if(item>=0&&item<4&&task.id&&(!task.done||item==0||item==3)){TaskAction choices[]={TaskAction::Complete,TaskAction::Urgent,TaskAction::Shelve,TaskAction::Delete};if(action)action(task,choices[item]);}return 1;}
        if(event==FL_PUSH)return 1;
    }
    int hit=-1;if(Fl::event_inside(this))for(size_t i=0;i<cards.size();++i){const auto& c=cards[i];if(Fl::event_x()>=x()+c.x&&Fl::event_x()<x()+c.x+c.w&&Fl::event_y()>=y()+c.y-offset&&Fl::event_y()<y()+c.y-offset+c.h){hit=static_cast<int>(i);break;}}
    if(event==FL_MOVE||event==FL_ENTER){if(hit!=hover){hover=hit;redraw();}return 1;}
    if(event==FL_LEAVE){hover=-1;redraw();return 1;}
    if(event==FL_PUSH&&Fl::event_button()==FL_LEFT_MOUSE){
        take_focus();pressed_child_=0;pressed_cancelled_=false;press_x_=Fl::event_x();press_y_=Fl::event_y();
        if(hit>=0)for(const auto& row:child_rows(cards[hit]))if(press_x_>=x()+row.x&&press_x_<x()+row.x+row.w&&press_y_>=y()+row.y-offset&&press_y_<y()+row.y-offset+row.h)pressed_child_=row.child.id;
        return 1;
    }
    // 拖离后取消整个手势，释放到父任务按钮或正文也不能误触其他动作。
    if(event==FL_DRAG){const int dx=Fl::event_x()-press_x_,dy=Fl::event_y()-press_y_;if(dx*dx+dy*dy>=25){pressed_child_=0;pressed_cancelled_=true;}return 1;}
    if(event==FL_RELEASE&&pressed_cancelled_){pressed_child_=0;pressed_cancelled_=false;return 1;}
    if(event==FL_RELEASE&&Fl::event_button()==FL_LEFT_MOUSE&&hit>=0){const auto card=cards[hit];auto task=tasks[card.index];int px=Fl::event_x()-x()-card.x,py=Fl::event_y()-(y()+card.y-offset);selected=task.id;
        const long long pressed_child=pressed_child_;pressed_child_=0;
        int summary_y=child_summary_y(task,card.w,full);
        if(task.child_count&&py>=summary_y-2&&py<summary_y+22){try{toggle_children(task.id);}catch(const std::exception& e){if(error)error(e.what());}return 1;}
        for(const auto& row:child_rows(card))if(Fl::event_x()>=x()+row.x&&Fl::event_x()<x()+row.x+row.w&&Fl::event_y()>=y()+row.y-offset&&Fl::event_y()<y()+row.y-offset+row.h){if(pressed_child==row.child.id&&complete_child)complete_child(row.child.id,!row.child.done);return 1;}
        if(py>=card.h-40&&py<card.h-10&&px>=card.w-42&&px<card.w-12){open_menu(hit);return 1;}
        auto which=py>=card.h-40&&py<card.h-10&&px>=card.w-78&&px<card.w-48&&!task.done?TaskAction::Start:TaskAction::Edit;redraw();if(action)action(task,which);return 1;
    }
    if(event==FL_RELEASE){pressed_child_=0;pressed_cancelled_=false;}
    if(event==FL_KEYBOARD&&Fl::event_key()==FL_Enter){for(const auto& t:tasks)if(t.id==selected){if(action)action(t,TaskAction::Edit);return 1;}}
    return Fl_Widget::handle(event);
}
ChildList::ChildList():Fl_Widget(0,0,10,10){}
void ChildList::draw(){fl_push_clip(x(),y(),w(),h());fl_color(canvas);fl_rectf(x(),y(),w(),h());if(items.empty())clipped(u8"把任务拆成几件容易完成的小事",x()+12,y(),w()-24,h(),12,muted);
    for(size_t i=0;i<items.size();++i){int top=y()+static_cast<int>(i)*38-offset;if(top+38<y()||top>y()+h())continue;fl_color(line);fl_line(x()+10,top+37,x()+w()-10,top+37);fl_color(items[i].done?blue:muted);fl_circle(x()+20,top+19,7);if(items[i].done)draw_icon(Icon::Check,x()+12,top+11,16,blue);clipped(items[i].title,x()+38,top+2,w()-80,34,12,items[i].done?muted:ink);draw_icon(Icon::Close,x()+w()-28,top+11,14,muted);}
    fl_pop_clip();}
int ChildList::handle(int e){if(e==FL_MOUSEWHEEL){offset=std::clamp(offset+Fl::event_dy()*38,0,std::max(0,static_cast<int>(items.size())*38-h()));redraw();return 1;}if(e==FL_PUSH)return Fl::event_button()==FL_LEFT_MOUSE;
    if(e==FL_RELEASE&&Fl::event_button()==FL_LEFT_MOUSE){int i=(Fl::event_y()-y()+offset)/38;if(Fl::event_inside(this)&&i>=0&&i<static_cast<int>(items.size())&&action)action(static_cast<size_t>(i),Fl::event_x()<x()+35?0:Fl::event_x()>x()+w()-38?2:1);return 1;}return Fl_Widget::handle(e);}
Toggle::Toggle():Fl_Button(0,0,48,28){type(FL_TOGGLE_BUTTON);tooltip(u8"点击切换开关");}
void Toggle::draw(){Raster image(w(),h(),surface);image.round(1,1,w()-2,h()-2,(h()-2)/2.0,value()?blue:line);int size=h()-8;image.disc(value()?w()-size/2.0-4:size/2.0+4,h()/2.0,size/2.0,FL_WHITE);image.draw(x(),y());if(Fl::focus()==this){fl_color(blue);fl_rect(x(),y(),w(),h());}}
int Toggle::handle(int event){int result=Fl_Button::handle(event);if(event==FL_ENTER||event==FL_LEAVE||event==FL_FOCUS||event==FL_UNFOCUS)redraw();return result;}
Ball::Ball():Fl_Double_Window(80,80,u8"轻待办浮球"),shape_mask(mask.data(),80,80){border(0);set_non_modal();end();for(int y=0;y<80;++y)for(int x=0;x<80;++x)if((x-40)*(x-40)+(y-40)*(y-40)<=39*39)mask[y*10+x/8]|=static_cast<unsigned char>(1<<(x%8));shape(&shape_mask);}
Ball::~Ball(){Fl::remove_timeout(hover_timeout,this);}
void Ball::show(){
    // Windows 隐藏会销毁 HWND，但 FLTK 仍缓存上次形状的尺寸。
    // 主窗口隐藏时也会重建其非模态浮球。新窗口每次都重新提交形状，
    // 使同尺寸重新显示、浮球开关和屏幕缩放都不会沿用旧 HWND 的缓存。
    if(!shown())shape(&shape_mask);
    Fl_Double_Window::show();
}
Ball::Mood Ball::mood()const{return urgent_count>0?Mood::Urgent:count>=8?Mood::Tired:count==0?Mood::Happy:working_count>0?Mood::Working:Mood::Relaxed;}
void Ball::draw(){
    // 基类负责提交原生窗口形状。跳过它会导致 Windows 显示方形白底。
    Fl_Double_Window::draw();
    const Fl_Color colors[]={fl_rgb_color(91,125,219),fl_rgb_color(72,159,185),fl_rgb_color(205,149,79),fl_rgb_color(193,120,167),fl_rgb_color(106,167,130)};
    auto color=colors[std::clamp(accent,0,4)];auto state=mood();auto background=state==Mood::Urgent?fl_rgb_color(255,231,211):fl_rgb_color(237,240,252);Raster image(80,80,background);auto body=wire?fl_rgb_color(255,255,252):color;auto eye=wire?color:fl_rgb_color(255,255,252);
    image.disc(40,40,38,color);image.disc(40,40,36,background);
    if(style==1){
        if(state==Mood::Urgent){auto fire=fl_rgb_color(245,123,54);for(int i=0;i<3;++i){int x=24+i*16;image.triangle(x-9,30,x-5,8+i%2*5,x+10,29,fire);image.triangle(x-4,30,x,17,x+7,30,fl_rgb_color(255,192,71));}}
        else{image.triangle(16,33,19,16,32,31,body);image.triangle(48,31,61,16,64,33,body);image.triangle(20,29,21,22,28,30,fl_rgb_color(246,178,193));image.triangle(52,30,59,22,60,29,fl_rgb_color(246,178,193));}
        image.round(12,23,56,43,21,body);
        if(wire){image.round(14,25,52,39,19,fl_rgb_color(255,255,252));}
        image.disc(21,49,4,fl_rgb_color(246,178,193));image.disc(59,49,4,fl_rgb_color(246,178,193));
        if(state==Mood::Tired){image.line(25,40,33,43,2.5,eye);image.line(47,43,55,40,2.5,eye);image.line(27,46,32,46,2,eye);image.line(48,46,53,46,2,eye);image.disc(40,53,3,eye);image.disc(40,53,1.5,body);image.disc(61,35,3,fl_rgb_color(146,209,242));}
        else if(state==Mood::Urgent){image.line(26,35,33,38,2,eye);image.line(47,38,54,35,2,eye);image.round(28,42,4,7,2,eye);image.round(48,42,4,7,2,eye);image.disc(40,54,3,eye);}
        else if(state==Mood::Happy){image.line(25,44,29,40,2.5,eye);image.line(29,40,33,44,2.5,eye);image.line(47,44,51,40,2.5,eye);image.line(51,40,55,44,2.5,eye);image.line(36,51,40,54,2,eye);image.line(40,54,44,51,2,eye);}
        else{image.round(27,39,4,9,2,eye);image.round(49,39,4,9,2,eye);image.line(35,52,40,55,2,eye);image.line(40,55,45,52,2,eye);if(state==Mood::Working){image.line(23,35,34,34,1.5,eye);image.line(46,34,57,35,1.5,eye);}}
        image.round(25,62,30,16,8,FL_WHITE);
    }
    image.draw(0,0);fl_color(style==1?color:color);fl_font(FL_HELVETICA_BOLD,style==1?11:24);fl_draw(std::to_string(count).c_str(),0,style==1?63:18,80,style==1?15:30,FL_ALIGN_CENTER);if(style==0){fl_font(FL_HELVETICA,11);fl_draw(u8"待办",0,49,80,16,FL_ALIGN_CENTER);}
}
void Ball::hover_timeout(void* p){auto* ball=static_cast<Ball*>(p);if(ball->hover_expand&&ball->hover_open&&!ball->moved)ball->hover_open();}
void Ball::configure(){Fl::remove_timeout(hover_timeout,this);if(!edge&&dock_side){position(dock_x,dock_y);dock_side=0;}redraw();}
void Ball::reset_position(){int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh,x(),y());dock_side=0;position(sx+std::max(0,sw-w()-20),sy+20);if(position_saved)position_saved();}
void Ball::dock(bool collapse){if(!dock_side)return;int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh,dock_x+w()/2,dock_y+h()/2);position(collapse?(dock_side==1?sx-w()+14:dock_side==2?sx+sw-14:dock_x):dock_x,collapse&&dock_side==3?sy-h()+14:dock_y);}
int Ball::handle(int e){if(e==FL_DND_ENTER||e==FL_DND_DRAG||e==FL_DND_RELEASE)return accept_drop?1:0;if(e==FL_PASTE&&accept_drop){if(dropped)dropped(std::string(Fl::event_text(),Fl::event_length()));return 1;}
    if(e==FL_ENTER){dock(false);moved=false;Fl::remove_timeout(hover_timeout,this);if(hover_expand)Fl::add_timeout(.28,hover_timeout,this);return 1;}
    if(e==FL_LEAVE){Fl::remove_timeout(hover_timeout,this);dock(true);return 1;}
    if(e==FL_PUSH){Fl::remove_timeout(hover_timeout,this);if(Fl::event_button()==FL_RIGHT_MOUSE){if(right_click)right_click();return 1;}dock(false);dock_side=0;anchor_x=Fl::event_x_root();anchor_y=Fl::event_y_root();start_x=x();start_y=y();moved=false;return 1;}
    if(e==FL_DRAG){Fl::remove_timeout(hover_timeout,this);int dx=Fl::event_x_root()-anchor_x,dy=Fl::event_y_root()-anchor_y;if(dx*dx+dy*dy>=25)moved=true;if(moved){int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh,Fl::event_x_root(),Fl::event_y_root());position(std::clamp(start_x+dx,sx,sx+std::max(0,sw-w())),std::clamp(start_y+dy,sy,sy+std::max(0,sh-h())));}return 1;}
    if(e==FL_RELEASE&&Fl::event_button()==FL_LEFT_MOUSE){if(moved){if(edge){int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh,x()+w()/2,y()+h()/2);dock_x=x();dock_y=y();if(x()<=sx+12){dock_side=1;dock_x=sx;}else if(x()+w()>=sx+sw-12){dock_side=2;dock_x=sx+sw-w();}else if(y()<=sy+12){dock_side=3;dock_y=sy;}}if(position_saved)position_saved();dock(true);}else if((!double_click||Fl::event_clicks()>0)&&click)click();return 1;}return Fl_Double_Window::handle(e);
}
