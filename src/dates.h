#pragma once
#include <ctime>
#include <string>
// 日期基于本机时区，交付日期为本地当天 00:00，提醒为独立时间戳。
std::tm local_parts(long long at);
long long calendar_time(int year,int month,int day,int hour=0,int minute=0);
long long day_after(int days);
long long end_of_today();
long long parse_date(const std::string& text,bool with_time=false);
std::string date_text(long long at,bool with_time=false);
