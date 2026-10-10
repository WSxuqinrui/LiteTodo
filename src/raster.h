#pragma once
#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>
#include <algorithm>
#include <cmath>
#include <vector>

// 小尺寸矢量图形以四倍采样绘制，缩小时平均像素覆盖率。
// 只在重绘时生成，无动画计时器、图片解码库或额外字体依赖。
class Raster {
    int w_,h_;
    std::vector<unsigned char> high_;
    template<class Contains> void paint(double x0,double y0,double x1,double y1,Fl_Color color,Contains contains){
        unsigned char r,g,b;Fl::get_color(color,r,g,b);
        for(int y=std::max(0,int(std::floor(y0*4)));y<std::min(h_*4,int(std::ceil(y1*4)));++y)
            for(int x=std::max(0,int(std::floor(x0*4)));x<std::min(w_*4,int(std::ceil(x1*4)));++x)
                if(contains((x+.5)/4,(y+.5)/4)){auto at=(y*w_*4+x)*3;high_[at]=r;high_[at+1]=g;high_[at+2]=b;}
    }
public:
    Raster(int w,int h,Fl_Color background):w_(w),h_(h),high_(w*h*16*3){unsigned char r,g,b;Fl::get_color(background,r,g,b);for(size_t i=0;i<high_.size();i+=3){high_[i]=r;high_[i+1]=g;high_[i+2]=b;}}
    void round(double x,double y,double w,double h,double radius,Fl_Color c){radius=std::min(radius,std::min(w,h)/2);paint(x,y,x+w,y+h,c,[=](double px,double py){double dx=px-std::clamp(px,x+radius,x+w-radius),dy=py-std::clamp(py,y+radius,y+h-radius);return dx*dx+dy*dy<=radius*radius;});}
    void disc(double x,double y,double r,Fl_Color c){round(x-r,y-r,2*r,2*r,r,c);}
    void line(double x1,double y1,double x2,double y2,double width,Fl_Color c){double r=width/2,dx=x2-x1,dy=y2-y1,den=dx*dx+dy*dy;paint(std::min(x1,x2)-r,std::min(y1,y2)-r,std::max(x1,x2)+r,std::max(y1,y2)+r,c,[=](double x,double y){double t=den?std::clamp(((x-x1)*dx+(y-y1)*dy)/den,0.0,1.0):0;double a=x-x1-t*dx,b=y-y1-t*dy;return a*a+b*b<=r*r;});}
    void triangle(double ax,double ay,double bx,double by,double cx,double cy,Fl_Color c){paint(std::min({ax,bx,cx}),std::min({ay,by,cy}),std::max({ax,bx,cx}),std::max({ay,by,cy}),c,[=](double x,double y){auto edge=[](double x,double y,double a,double b,double c,double d){return (x-a)*(d-b)-(y-b)*(c-a);};double a=edge(x,y,ax,ay,bx,by),b=edge(x,y,bx,by,cx,cy),d=edge(x,y,cx,cy,ax,ay);return (a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0);});}
    void draw(int x,int y){std::vector<unsigned char> low(w_*h_*3);for(int py=0;py<h_;++py)for(int px=0;px<w_;++px)for(int c=0;c<3;++c){unsigned sum=0;for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx)sum+=high_[((py*4+sy)*w_*4+px*4+sx)*3+c];low[(py*w_+px)*3+c]=static_cast<unsigned char>((sum+8)/16);}Fl_RGB_Image image(low.data(),w_,h_,3);image.draw(x,y);}
};
