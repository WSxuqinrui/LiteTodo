#include "theme.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <cmath>
#include <algorithm>
namespace Style {
const Fl_Color ink=16,muted=17,blue=18,line=19,pale=20,canvas=21,surface=22,green=23,red=24;
static bool is_dark=false;
bool dark(){return is_dark;}
void palette(int mode,int accent,bool system_dark){
    is_dark=mode==2||(mode==0&&system_dark);accent=std::clamp(accent,0,4);
    const unsigned char colors[5][3]={{57,106,212},{59,143,184},{174,114,49},{171,89,139},{62,135,103}};
    auto set=[](Fl_Color color,int r,int g,int b){Fl::set_color(color,static_cast<uchar>(r),static_cast<uchar>(g),static_cast<uchar>(b));};
    set(ink,is_dark?234:37,is_dark?237:43,is_dark?244:54);set(muted,is_dark?168:110,is_dark?177:119,is_dark?195:134);
    set(surface,is_dark?35:255,is_dark?39:255,is_dark?49:255);set(canvas,is_dark?25:247,is_dark?28:248,is_dark?36:250);set(line,is_dark?65:226,is_dark?72:229,is_dark?89:235);
    set(blue,colors[accent][0],colors[accent][1],colors[accent][2]);set(pale,is_dark?(colors[accent][0]+35)/3:(colors[accent][0]+255*8)/9,is_dark?(colors[accent][1]+39)/3:(colors[accent][1]+255*8)/9,is_dark?(colors[accent][2]+49)/3:(colors[accent][2]+255*8)/9);
    set(green,is_dark?88:54,is_dark?183:148,is_dark?137:107);set(red,is_dark?247:207,is_dark?124:76,is_dark?131:84);
    Fl::set_color(FL_FOREGROUND_COLOR,is_dark?234:37,is_dark?237:43,is_dark?244:54);Fl::set_color(FL_BACKGROUND2_COLOR,is_dark?35:255,is_dark?39:255,is_dark?49:255);
    Fl::redraw();
}
static void disc(double x,double y,double r){fl_begin_polygon();for(int i=0;i<48;++i){double a=i*6.283185307179586/48;fl_vertex(x+r*std::cos(a),y+r*std::sin(a));}fl_end_polygon();}
void pill(int x,int y,int w,int h,Fl_Color color){
    if(w<1||h<1)return;
    fl_color(color);double r=std::min(w,h)/2.0;
    if(w>=h){if(w>2*r)fl_rectf(x+static_cast<int>(r),y,w-2*static_cast<int>(r),h);disc(x+r,y+h/2.0,r);disc(x+w-r,y+h/2.0,r);}
    else{if(h>2*r)fl_rectf(x,y+static_cast<int>(r),w,h-2*static_cast<int>(r));disc(x+w/2.0,y+r,r);disc(x+w/2.0,y+h-r,r);}
}
}
