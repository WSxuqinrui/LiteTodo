#include "dates.h"
#include "store.h"
#include <cstdio>
#include <stdexcept>
std::tm local_parts(long long at){auto seconds=static_cast<time_t>(at);std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&seconds);
#else
    localtime_r(&seconds,&tm);
#endif
    return tm;
}
long long calendar_time(int year,int month,int day,int hour,int minute){
    if(year<1970||year>2100||month<1||month>12||day<1||day>31||hour<0||hour>23||minute<0||minute>59)throw std::runtime_error(u8"日期或时间超出范围");
    std::tm tm{};tm.tm_year=year-1900;tm.tm_mon=month-1;tm.tm_mday=day;tm.tm_hour=hour;tm.tm_min=minute;tm.tm_isdst=-1;
    auto at=std::mktime(&tm);auto actual=local_parts(at);
    if(at<0||actual.tm_year!=year-1900||actual.tm_mon!=month-1||actual.tm_mday!=day||actual.tm_hour!=hour||actual.tm_min!=minute)throw std::runtime_error(u8"该日期或本地时间不存在");
    return at;
}
long long day_after(int days){auto tm=local_parts(now_seconds());tm.tm_mday+=days;tm.tm_hour=tm.tm_min=tm.tm_sec=0;tm.tm_isdst=-1;return std::mktime(&tm);}
long long end_of_today(){return day_after(1);}
long long parse_date(const std::string& text,bool with_time){int year=0,month=0,day=0,hour=0,minute=0,end=0;
    int fields=with_time?std::sscanf(text.c_str(),"%d-%d-%d %d:%d%n",&year,&month,&day,&hour,&minute,&end):std::sscanf(text.c_str(),"%d-%d-%d%n",&year,&month,&day,&end);
    if(fields!=(with_time?5:3)||end!=static_cast<int>(text.size()))throw std::runtime_error(with_time?u8"请输入 YYYY-MM-DD HH:MM":u8"请输入 YYYY-MM-DD");
    return calendar_time(year,month,day,hour,minute);
}
std::string date_text(long long at,bool with_time){if(!at)return "";auto tm=local_parts(at);char text[48];std::strftime(text,sizeof(text),with_time?"%Y-%m-%d %H:%M":"%Y-%m-%d",&tm);return text;}
