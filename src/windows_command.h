#pragma once
#include <string>
// Windows argv 引用规则：引号前和参数尾部的反斜杠需要翻倍，普通路径保留。
inline std::wstring quote_windows_argument(const std::wstring& value){
    std::wstring result=L"\"";size_t slashes=0;
    for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}result.append(c==L'"'?slashes*2+1:slashes,L'\\');result+=c;slashes=0;}
    result.append(slashes*2,L'\\');return result+L"\"";
}
inline std::wstring autostart_command(const std::wstring& executable,const std::wstring& data){return quote_windows_argument(executable)+L" --data-dir "+quote_windows_argument(data);}
