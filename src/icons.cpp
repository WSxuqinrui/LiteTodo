#include "icons.h"
#include "theme.h"
#include "raster.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <cmath>
#include <algorithm>
#include <initializer_list>
#include <utility>
namespace {
void path(std::initializer_list<std::pair<double,double>> points){fl_begin_line();for(auto p:points)fl_vertex(p.first,p.second);fl_end_line();}
void square(double x,double y,double w,double h){path({{x,y},{x+w,y},{x+w,y+h},{x,y+h},{x,y}});}
}
void draw_icon(Icon icon,int x,int y,int size,Fl_Color color){
    if(icon==Icon::Logo){Raster image(size,size,Style::canvas);double s=size/24.0;image.round(0,0,size,size,size*.27,fl_rgb_color(57,106,212));image.line(5*s,7*s,14*s,7*s,1.7*s,FL_WHITE);image.line(5*s,12*s,10*s,12*s,1.7*s,FL_WHITE);image.line(11*s,16*s,15*s,19*s,2*s,FL_WHITE);image.line(15*s,19*s,21*s,10*s,2*s,FL_WHITE);image.draw(x,y);return;}
    fl_push_matrix();fl_translate(x,y);fl_scale(size/24.0,size/24.0);fl_color(color);fl_line_style(FL_SOLID,std::max(1,static_cast<int>(std::round(size/16.0))));
    switch(icon){
    case Icon::Logo:path({{5,7},{14,7}});path({{5,12},{10,12}});path({{11,16},{15,19},{21,10}});break;
    case Icon::Plus:path({{12,5},{12,19}});path({{5,12},{19,12}});break;
    case Icon::Back:path({{10,5},{3,12},{10,19}});path({{3,12},{21,12}});break;
    case Icon::Search:fl_circle(10,10,6);path({{15,15},{21,21}});break;
    case Icon::Settings:fl_circle(12,12,7);fl_circle(12,12,2.5);for(int i=0;i<8;++i){double a=i*3.141592653589793/4;path({{12+7*std::cos(a),12+7*std::sin(a)},{12+10*std::cos(a),12+10*std::sin(a)}});}break;
    case Icon::Pin:path({{8,3},{16,3},{16,8},{19,13},{5,13},{8,8},{8,3}});path({{12,13},{12,21}});break;
    case Icon::Calendar:square(4,6,16,15);path({{4,10},{20,10}});path({{8,3},{8,8}});path({{16,3},{16,8}});break;
    case Icon::Clock:fl_circle(12,12,9);path({{12,6},{12,12},{17,15}});break;
    case Icon::Grid:for(int i=0;i<4;++i)square(3+(i%2)*11,3+(i/2)*11,7,7);break;
    case Icon::List:for(int i=0;i<3;++i){path({{3,5+i*7.0},{5,5+i*7.0}});path({{9,5+i*7.0},{21,5+i*7.0}});}break;
    case Icon::Check:path({{4,12},{9,17},{20,6}});break;
    case Icon::Play:path({{7,4},{20,12},{7,20},{7,4}});break;
    case Icon::Pause:path({{8,5},{8,19}});path({{16,5},{16,19}});break;
    case Icon::More:for(int i=0;i<3;++i)fl_circle(5+i*7,12,1);break;
    case Icon::Lightning:path({{13,2},{5,13},{11,13},{9,22},{20,10},{13,10},{13,2}});break;
    case Icon::Close:path({{5,5},{19,19}});path({{19,5},{5,19}});break;
    case Icon::Trash:square(6,7,12,14);path({{3,7},{21,7}});path({{9,7},{9,3},{15,3},{15,7}});break;
    case Icon::Sun:fl_circle(12,12,5);for(int i=0;i<8;++i){double a=i*3.141592653589793/4;path({{12+8*std::cos(a),12+8*std::sin(a)},{12+10*std::cos(a),12+10*std::sin(a)}});}break;
    case Icon::Ball:fl_circle(12,12,9);path({{12,3},{12,21}});fl_circle(7,12,1);break;
    case Icon::Chevron:path({{9,5},{16,12},{9,19}});break;
    }
    fl_pop_matrix();fl_line_style(0);
}
IconButton::IconButton(int x,int y,int w,int h,Icon icon,const char* hint):Fl_Button(x,y,w,h),icon_(icon){tooltip(hint);box(FL_RFLAT_BOX);color(Style::canvas);selection_color(Style::pale);labelsize(12);}
int IconButton::handle(int event){if(event==FL_ENTER){hover_=true;redraw();}if(event==FL_LEAVE){hover_=false;redraw();}return Fl_Button::handle(event);}
void IconButton::draw(){fl_draw_box(box(),x(),y(),w(),h(),selected_?Style::blue:(hover_||value()||Fl::focus()==this)?Style::pale:color());int size=icon_==Icon::Logo?28:20;bool text=label()&&label()[0];draw_icon(icon_,text?x()+10:x()+(w()-size)/2,y()+(h()-size)/2,size,selected_?FL_WHITE:hover_?Style::blue:Style::muted);if(text){fl_color(selected_?FL_WHITE:Style::ink);fl_font(FL_HELVETICA,labelsize());fl_draw(label(),x()+34,y(),w()-38,h(),FL_ALIGN_LEFT|FL_ALIGN_INSIDE,nullptr,0);}}
NavigationButton::NavigationButton(Icon icon,const char* text):Fl_Button(0,0,1,1),icon_(icon){copy_label(text);labelsize(14);box(FL_RFLAT_BOX);color(Style::surface);selection_color(Style::pale);}
void NavigationButton::draw(){fl_draw_box(box(),x(),y(),w(),h(),Style::line);fl_draw_box(box(),x()+1,y()+1,w()-2,h()-2,value()?selection_color():color());draw_icon(icon_,x()+16,y()+(h()-20)/2,20,Style::muted);fl_color(Style::ink);fl_font(labelfont(),labelsize());fl_draw(label(),x()+48,y(),w()-88,h(),FL_ALIGN_LEFT|FL_ALIGN_INSIDE,nullptr,0);draw_icon(Icon::Chevron,x()+w()-31,y()+(h()-16)/2,16,Style::muted);}
