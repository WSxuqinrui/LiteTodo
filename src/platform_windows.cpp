#include "platform.h"
#include "windows_command.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <FL/Fl_Window.H>
#include <FL/platform.H>

namespace {
HWND owner=nullptr;
HICON icon=nullptr;
PlatformCallbacks handlers;
NOTIFYICONDATAW tray{};
bool active=false;
bool hotkey_registered=false;unsigned int registered_mod=0;int registered_key=0,registered_id=92;
UINT restart_message=0;
bool notification_busy=false;
ULONGLONG notification_started=0;
std::function<void(bool)> notification_completion;
const char* notification_feedback=u8"等待系统通知显示回执";
constexpr UINT_PTR notification_timer=91;
void settle_notification(bool shown){auto callback=std::move(notification_completion);notification_completion={};if(callback)callback(shown);}
void cancel_notification(){auto copy=tray;copy.uFlags=NIF_INFO;copy.szInfo[0]=0;Shell_NotifyIconW(NIM_MODIFY,&copy);}
constexpr UINT tray_message=WM_APP+71;
std::wstring wide(const std::string& value){
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,nullptr,0);
    if(n<=0)return L"";
    std::wstring result(static_cast<size_t>(n),L'\0');
    MultiByteToWideChar(CP_UTF8,0,value.c_str(),-1,result.data(),n);result.resize(n-1);return result;
}
void add_tray(){active=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE;tray.uVersion=NOTIFYICON_VERSION_4;if(active&&!Shell_NotifyIconW(NIM_SETVERSION,&tray)){Shell_NotifyIconW(NIM_DELETE,&tray);active=false;}}
// 删除超时请求的图标并换身份。旧回执带旧 uID，不能确认下一条提醒。
void renew_tray_identity(){if(active)Shell_NotifyIconW(NIM_DELETE,&tray);active=false;tray.uID=tray.uID%65534+1;add_tray();}
LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    if(message==WM_TIMER&&w==notification_timer){auto elapsed=GetTickCount64()-notification_started;if(elapsed>=10000&&notification_completion){notification_feedback=u8"系统没有返回显示回执，可能已关闭通知或处于勿扰状态";settle_notification(false);cancel_notification();renew_tray_identity();notification_busy=false;KillTimer(owner,notification_timer);}return 0;}
    if(message==WM_HOTKEY&&hotkey_registered&&w==static_cast<WPARAM>(registered_id)){if(handlers.organize)handlers.organize();return 0;}
    if(message==restart_message){notification_feedback=u8"桌面通知服务已重启，改用应用内提醒";settle_notification(false);notification_busy=false;KillTimer(owner,notification_timer);tray.uID=tray.uID%65534+1;add_tray();return 0;}
    if(message==tray_message){
        UINT event=LOWORD(l);
        if(HIWORD(l)!=tray.uID)return 0;
        if(event==NIN_BALLOONSHOW){if(notification_busy){notification_feedback=u8"系统已显示通知";settle_notification(true);KillTimer(owner,notification_timer);}return 0;}
        if(event==NIN_BALLOONHIDE||event==NIN_BALLOONTIMEOUT){if(notification_busy){if(notification_completion){notification_feedback=u8"系统撤回了未显示的通知";settle_notification(false);}notification_busy=false;KillTimer(owner,notification_timer);}return 0;}
        if(event==NIN_BALLOONUSERCLICK){if(notification_busy)settle_notification(true);notification_busy=false;KillTimer(owner,notification_timer);if(handlers.show)handlers.show();return 0;}
        if(event==WM_LBUTTONUP||event==NIN_SELECT||event==NIN_KEYSELECT){handlers.show();return 0;}
        if(event==WM_RBUTTONUP||event==WM_CONTEXTMENU){
            POINT p;GetCursorPos(&p);HMENU menu=CreatePopupMenu();
            AppendMenuW(menu,MF_STRING,1,L"打开轻待办");AppendMenuW(menu,MF_STRING,2,L"快速记录");
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,3,L"退出");
            SetForegroundWindow(hwnd);
            UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);
            DestroyMenu(menu);PostMessageW(hwnd,WM_NULL,0,0);
            if(command==1)handlers.show();else if(command==2)handlers.quick();else if(command==3)handlers.quit();
            return 0;
        }
    }
    return DefWindowProcW(hwnd,message,w,l);
}
HICON make_icon(){
    // 与可执行程序和窗口共用原创多尺寸资源。
    if(auto resource=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,0)))return resource;
    // 程序生成小图标，无需捆绑图标图片或字体。
    HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
    HBITMAP color=CreateCompatibleBitmap(screen,32,32),mask=CreateBitmap(32,32,1,1,nullptr);
    HGDIOBJ previous=SelectObject(dc,color);RECT r{0,0,32,32};HBRUSH brush=CreateSolidBrush(RGB(47,100,245));FillRect(dc,&r,brush);DeleteObject(brush);
    HPEN pen=CreatePen(PS_SOLID,3,RGB(255,255,255));auto oldpen=SelectObject(dc,pen);
    MoveToEx(dc,8,16,nullptr);LineTo(dc,13,21);LineTo(dc,24,10);SelectObject(dc,oldpen);DeleteObject(pen);
    SelectObject(dc,mask);PatBlt(dc,0,0,32,32,BLACKNESS);SelectObject(dc,previous);
    ICONINFO info{TRUE,0,0,mask,color};HICON result=CreateIconIndirect(&info);
    DeleteObject(color);DeleteObject(mask);DeleteDC(dc);ReleaseDC(nullptr,screen);return result;
}
}
bool platform_topmost(Fl_Window* window,bool enabled){
    auto hwnd=fl_win32_xid(window);
    return hwnd&&SetWindowPos(hwnd,enabled?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE;
}
void platform_window_icon(Fl_Window* window){window->icon(reinterpret_cast<const void*>(LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1))));}
bool platform_autostart_supported(){return true;}
bool platform_autostart_enabled(){HKEY key=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;auto result=RegQueryValueExW(key,L"LiteTodoPrototype",nullptr,nullptr,nullptr,nullptr);RegCloseKey(key);return result==ERROR_SUCCESS;}
bool platform_set_autostart(bool enabled,const std::string& data_directory){
    HKEY key=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    LSTATUS result=ERROR_SUCCESS;
    if(enabled){wchar_t file[32768];auto n=GetModuleFileNameW(nullptr,file,32768);auto directory=wide(data_directory);if(!n||n>=32768||directory.empty()){RegCloseKey(key);return false;}auto command=autostart_command(file,directory);result=RegSetValueExW(key,L"LiteTodoPrototype",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));}
    else{result=RegDeleteValueW(key,L"LiteTodoPrototype");if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}
    RegCloseKey(key);return result==ERROR_SUCCESS;
}
bool platform_system_dark(){DWORD light=1,size=sizeof(light);return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size)==ERROR_SUCCESS&&light==0;}
bool platform_opacity(Fl_Window* window,int percent){auto hwnd=fl_win32_xid(window);if(!hwnd)return false;percent=std::clamp(percent,45,100);auto style=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);if(percent==100){SetWindowLongPtrW(hwnd,GWL_EXSTYLE,style&~WS_EX_LAYERED);RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);return true;}SetWindowLongPtrW(hwnd,GWL_EXSTYLE,style|WS_EX_LAYERED);return SetLayeredWindowAttributes(hwnd,0,static_cast<BYTE>(255*percent/100),LWA_ALPHA)!=FALSE;}
bool platform_desktop_supported(){return false;}
bool platform_desktop(Fl_Window*,bool enabled){return !enabled;}
bool platform_open_folder(const std::filesystem::path& folder){return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;}
bool platform_hotkey_supported(){return true;}
bool platform_hotkey(bool enabled,unsigned int modifiers,int key){
    if(!owner)return !enabled;
    if(enabled&&hotkey_registered&&registered_mod==modifiers&&registered_key==key)return true;
    if(!enabled){if(hotkey_registered)UnregisterHotKey(owner,registered_id);hotkey_registered=false;return true;}
    int next=registered_id==92?93:92;
    // 新组合注册成功后才释放旧组合，冲突时原快捷键继续有效。
    if(!RegisterHotKey(owner,next,modifiers|MOD_NOREPEAT,key))return false;
    if(hotkey_registered)UnregisterHotKey(owner,registered_id);
    registered_id=next;hotkey_registered=true;registered_mod=modifiers;registered_key=key;return true;
}
bool platform_init(const PlatformCallbacks& callbacks){
    handlers=callbacks;WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LiteTodoPrototypeTray";
    RegisterClassW(&wc);restart_message=RegisterWindowMessageW(L"TaskbarCreated");
    owner=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    if(!owner)return false;
    icon=make_icon();tray.cbSize=sizeof(tray);tray.hWnd=owner;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;tray.uCallbackMessage=tray_message;tray.hIcon=icon;
    wcscpy_s(tray.szTip,L"轻待办 · 点击打开，右键退出");add_tray();return active;
}
bool platform_notify(const std::string& title,const std::string& text,std::function<void(bool)> completion){
    if(!active){notification_feedback=u8"托盘通知服务不可用";return false;}
    if(notification_busy){notification_feedback=u8"前一条系统通知仍在显示，本条改用应用内提醒";return false;}
    QUERY_USER_NOTIFICATION_STATE state=QUNS_ACCEPTS_NOTIFICATIONS;
    if(SUCCEEDED(SHQueryUserNotificationState(&state))&&state!=QUNS_ACCEPTS_NOTIFICATIONS&&state!=QUNS_APP){notification_feedback=u8"系统当前处于勿扰、全屏或忙碌状态";return false;}
    DWORD enabled=1,size=sizeof(enabled);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications",L"ToastEnabled",RRF_RT_REG_DWORD,nullptr,&enabled,&size)==ERROR_SUCCESS&&!enabled){notification_feedback=u8"Windows 系统通知已关闭";return false;}
    auto copy=tray;copy.uFlags=NIF_INFO|NIF_REALTIME;copy.dwInfoFlags=NIIF_INFO;copy.uTimeout=6000;
    auto a=wide(title),b=wide(text);wcsncpy_s(copy.szInfoTitle,a.c_str(),_TRUNCATE);wcsncpy_s(copy.szInfo,b.c_str(),_TRUNCATE);
    bool accepted=Shell_NotifyIconW(NIM_MODIFY,&copy)!=FALSE;
    if(accepted){notification_feedback=u8"等待系统通知显示回执";notification_busy=true;notification_started=GetTickCount64();notification_completion=std::move(completion);SetTimer(owner,notification_timer,500,nullptr);}
    else notification_feedback=u8"Windows 拒绝了通知请求";
    return accepted;
}
const char* platform_notification_feedback(){return notification_feedback;}
const char* platform_notification_confirmation(){return u8"系统已显示提醒";}
bool platform_open_notification_settings(){return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"ms-settings:notifications",nullptr,nullptr,SW_SHOWNORMAL))>32;}
bool platform_notification_regression_test(){
    if(!owner||notification_busy)return false;
    auto original=tray.uID;int confirmed=0;notification_busy=true;notification_started=GetTickCount64()-25000;notification_completion=[&](bool shown){if(shown)++confirmed;};
    proc(owner,tray_message,0,MAKELPARAM(NIN_BALLOONSHOW,original));proc(owner,WM_TIMER,notification_timer,0);
    bool ok=confirmed==1&&notification_busy;
    tray.uID=original%65534+1;notification_completion=[&](bool shown){if(shown)++confirmed;};
    for(UINT event:{NIN_BALLOONSHOW,NIN_BALLOONUSERCLICK,NIN_BALLOONHIDE})proc(owner,tray_message,0,MAKELPARAM(event,original));
    ok=ok&&confirmed==1&&notification_busy&&static_cast<bool>(notification_completion);
    proc(owner,tray_message,0,MAKELPARAM(NIN_BALLOONSHOW,tray.uID));proc(owner,tray_message,0,MAKELPARAM(NIN_BALLOONHIDE,tray.uID));
    ok=ok&&confirmed==2&&!notification_busy;notification_completion={};tray.uID=original;return ok;
}
bool platform_tray_available(){
    if(!active)return false;
    NOTIFYICONIDENTIFIER identity{};identity.cbSize=sizeof(identity);identity.hWnd=owner;identity.uID=tray.uID;
    RECT rectangle{};return SUCCEEDED(Shell_NotifyIconGetRect(&identity,&rectangle));
}
void platform_poll(){}
void platform_shutdown(){notification_completion={};notification_busy=false;if(owner)KillTimer(owner,notification_timer);if(hotkey_registered)UnregisterHotKey(owner,registered_id);hotkey_registered=false;if(active)Shell_NotifyIconW(NIM_DELETE,&tray);active=false;if(owner)DestroyWindow(owner);owner=nullptr;if(icon)DestroyIcon(icon);icon=nullptr;}
const char* platform_tray_description(){return active?u8"托盘已启用 · 关闭窗口后继续提醒":u8"托盘不可用 · 关闭窗口将退出";}
