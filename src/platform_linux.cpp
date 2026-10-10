#include "platform.h"
#include <algorithm>
#include <cstdlib>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_RGB_Image.H>
#include "icon-pixels.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <unistd.h>
#include <sys/wait.h>
#ifdef LITETODO_DBUS
#include <dbus/dbus.h>
#endif

namespace {
Display* display=nullptr;
Window icon=0;
bool active=false;
Window tray_host=0;
PlatformCallbacks handlers;
void events(int,void*){
    while(display&&XPending(display)){
        XEvent e;XNextEvent(display,&e);
        if(e.type==Expose){GC gc=XCreateGC(display,icon,0,nullptr);XSetForeground(display,gc,0x3469f6);XFillRectangle(display,icon,gc,0,0,24,24);XSetForeground(display,gc,0xffffff);XSetLineAttributes(display,gc,2,LineSolid,CapRound,JoinRound);XDrawLine(display,icon,gc,5,12,10,17);XDrawLine(display,icon,gc,10,17,19,7);XFreeGC(display,gc);}
        else if(e.type==DestroyNotify){active=false;if(e.xdestroywindow.window==icon)icon=0;tray_host=0;}
        else if(e.type==ButtonRelease){if(e.xbutton.button==Button1)handlers.show();else if(e.xbutton.button==Button3)handlers.quick();}
    }
}
}
bool platform_init(const PlatformCallbacks& callbacks){
    handlers=callbacks;display=XOpenDisplay(nullptr);if(!display)return false;
    std::string selection="_NET_SYSTEM_TRAY_S"+std::to_string(DefaultScreen(display));
    Window host=XGetSelectionOwner(display,XInternAtom(display,selection.c_str(),False));
    if(!host){XCloseDisplay(display);display=nullptr;return false;}
    tray_host=host;XSelectInput(display,host,StructureNotifyMask);
    icon=XCreateSimpleWindow(display,DefaultRootWindow(display),0,0,24,24,0,0,0x3469f6);
    XSelectInput(display,icon,ExposureMask|ButtonReleaseMask|StructureNotifyMask);
    unsigned long info[2]={0,1};Atom xembed=XInternAtom(display,"_XEMBED_INFO",False);
    XChangeProperty(display,icon,xembed,xembed,32,PropModeReplace,reinterpret_cast<unsigned char*>(info),2);
    XStoreName(display,icon,"LiteTodo");
    XEvent event{};event.xclient.type=ClientMessage;event.xclient.window=host;event.xclient.message_type=XInternAtom(display,"_NET_SYSTEM_TRAY_OPCODE",False);event.xclient.format=32;event.xclient.data.l[0]=CurrentTime;event.xclient.data.l[1]=0;event.xclient.data.l[2]=static_cast<long>(icon);
    active=XSendEvent(display,host,False,NoEventMask,&event)!=0;XFlush(display);
    Fl::add_fd(ConnectionNumber(display),FL_READ,events);return active;
}
bool platform_notify(const std::string& title,const std::string& text,std::function<void(bool)> completion){
#ifdef LITETODO_DBUS
    // 通过标准桌面通知 D-Bus 接口发送，无 shell 命令拼接。
    DBusError error;dbus_error_init(&error);DBusConnection* bus=dbus_bus_get(DBUS_BUS_SESSION,&error);
    if(!bus){dbus_error_free(&error);return false;}
    DBusMessage* message=dbus_message_new_method_call("org.freedesktop.Notifications","/org/freedesktop/Notifications","org.freedesktop.Notifications","Notify");
    if(!message){dbus_connection_unref(bus);return false;}
    DBusMessageIter args,actions,hints;dbus_message_iter_init_append(message,&args);
    const char* app="LiteTodo",*symbol="",*a=title.c_str(),*b=text.c_str();dbus_uint32_t replace=0;dbus_int32_t timeout=6000;
    bool ok=dbus_message_iter_append_basic(&args,DBUS_TYPE_STRING,&app)&&dbus_message_iter_append_basic(&args,DBUS_TYPE_UINT32,&replace)&&dbus_message_iter_append_basic(&args,DBUS_TYPE_STRING,&symbol)&&dbus_message_iter_append_basic(&args,DBUS_TYPE_STRING,&a)&&dbus_message_iter_append_basic(&args,DBUS_TYPE_STRING,&b);
    ok=ok&&dbus_message_iter_open_container(&args,DBUS_TYPE_ARRAY,"s",&actions)&&dbus_message_iter_close_container(&args,&actions);
    ok=ok&&dbus_message_iter_open_container(&args,DBUS_TYPE_ARRAY,"{sv}",&hints)&&dbus_message_iter_close_container(&args,&hints)&&dbus_message_iter_append_basic(&args,DBUS_TYPE_INT32,&timeout);
    // 有界等待系统服务应答；服务不存在时返回失败，由主界面保留到期提示。
    DBusMessage* reply=ok?dbus_connection_send_with_reply_and_block(bus,message,800,&error):nullptr;
    bool accepted=reply&&!dbus_message_get_error_name(reply);
    if(reply)dbus_message_unref(reply);dbus_message_unref(message);dbus_connection_unref(bus);dbus_error_free(&error);
    if(accepted&&completion)completion(true);return accepted;
#else
    (void)title;(void)text;(void)completion;return false;
#endif
}
bool platform_tray_available(){
    if(!display||!active||!icon)return false;
    std::string selection="_NET_SYSTEM_TRAY_S"+std::to_string(DefaultScreen(display));
    Window current=XGetSelectionOwner(display,XInternAtom(display,selection.c_str(),False));
    return current&&current==tray_host;
}
void platform_window_icon(Fl_Window* window){static Fl_RGB_Image image(app_icon_pixels,64,64,4);window->icon(&image);}
bool platform_autostart_supported(){return false;}
bool platform_autostart_enabled(){return false;}
bool platform_set_autostart(bool,const std::string&){return false;}
bool platform_system_dark(){const char* theme=getenv("GTK_THEME");return theme&&std::string(theme).find("dark")!=std::string::npos;}
bool platform_opacity(Fl_Window* window,int percent){auto connection=XOpenDisplay(nullptr);if(!connection)return false;unsigned long alpha=static_cast<unsigned long>(0xffffffffULL*std::clamp(percent,45,100)/100);auto xid=static_cast<Window>(window->os_id());bool ok=xid!=0;if(ok){auto property=XInternAtom(connection,"_NET_WM_WINDOW_OPACITY",False);XChangeProperty(connection,xid,property,XA_CARDINAL,32,PropModeReplace,reinterpret_cast<unsigned char*>(&alpha),1);XFlush(connection);}XCloseDisplay(connection);return ok;}
bool platform_desktop_supported(){return false;}
bool platform_desktop(Fl_Window*,bool){return false;}
bool platform_open_folder(const std::filesystem::path& path){pid_t child=fork();if(child<0)return false;if(child==0){pid_t grandchild=fork();if(grandchild==0){execlp("xdg-open","xdg-open",path.c_str(),static_cast<char*>(nullptr));_exit(127);} _exit(grandchild<0?127:0);}int status=0;waitpid(child,&status,0);return WIFEXITED(status)&&WEXITSTATUS(status)==0;}
bool platform_hotkey_supported(){return false;}
bool platform_hotkey(bool,unsigned int,int){return false;}
void platform_poll(){
    if(!display){auto saved=handlers;if(saved.show)platform_init(saved);return;}
    events(0,nullptr);
    if(!platform_tray_available()){
        // 宿主重启后重新挂接；没有宿主时明确回退到主窗口/浮球入口。
        auto saved=handlers;platform_shutdown();platform_init(saved);
    }
}
bool platform_topmost(Fl_Window* window,bool enabled){
    Display* connection=XOpenDisplay(nullptr);if(!connection)return false;
    Window xid=static_cast<Window>(window->os_id());if(!xid){XCloseDisplay(connection);return false;}
    XEvent e{};e.xclient.type=ClientMessage;e.xclient.window=xid;e.xclient.message_type=XInternAtom(connection,"_NET_WM_STATE",False);e.xclient.format=32;
    e.xclient.data.l[0]=enabled?1:0;e.xclient.data.l[1]=XInternAtom(connection,"_NET_WM_STATE_ABOVE",False);e.xclient.data.l[3]=1;
    bool submitted=XSendEvent(connection,DefaultRootWindow(connection),False,SubstructureRedirectMask|SubstructureNotifyMask,&e)!=0;
    XFlush(connection);XCloseDisplay(connection);return submitted;
}
void platform_shutdown(){if(display){Fl::remove_fd(ConnectionNumber(display));if(icon)XDestroyWindow(display,icon);XCloseDisplay(display);}display=nullptr;icon=0;tray_host=0;active=false;}
const char* platform_tray_description(){return active?u8"X11 托盘已连接 · 左键打开，右键快速记录":u8"桌面没有 XEmbed 托盘 · 关闭窗口将退出";}

const char* platform_notification_feedback(){return u8"通知服务未接受请求，请检查桌面通知服务";}
const char* platform_notification_confirmation(){return u8"桌面通知服务已接收提醒";}
bool platform_open_notification_settings(){return false;}
