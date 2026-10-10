#pragma once
#include <FL/Fl_Button.H>
enum class Icon {Logo,Plus,Back,Search,Settings,Pin,Calendar,Clock,Grid,List,Check,Close,Trash,Sun,Ball,Chevron,Play,Pause,More,Lightning};
void draw_icon(Icon icon,int x,int y,int size,Fl_Color color);
class IconButton:public Fl_Button {
    Icon icon_;
    bool hover_=false,selected_=false;
public:
    IconButton(int x,int y,int w,int h,Icon icon,const char* tooltip);
    void draw()override;
    int handle(int event)override;
    void selected(bool value){selected_=value;redraw();}
    bool selected()const{return selected_;}
};
class NavigationButton:public Fl_Button {
    Icon icon_;
public:
    NavigationButton(Icon icon,const char* text);
    void draw()override;
};
