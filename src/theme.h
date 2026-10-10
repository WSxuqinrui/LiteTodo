#pragma once
#include <FL/Enumerations.H>
namespace Style {
extern const Fl_Color ink,muted,blue,line,pale,canvas,surface,green,red;
void palette(int mode,int accent,bool system_dark);
bool dark();
void pill(int x,int y,int w,int h,Fl_Color color);
}
