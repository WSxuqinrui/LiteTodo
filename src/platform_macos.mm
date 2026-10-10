#include "platform.h"
#include <algorithm>
#include <FL/Fl_Window.H>
#include <FL/platform.H>
#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>
#include <atomic>

namespace {PlatformCallbacks handlers;NSStatusItem* item=nil;std::atomic<bool> authorized{false};}

@interface LiteTodoActions:NSObject<UNUserNotificationCenterDelegate>
- (void)show:(id)sender;
- (void)quick:(id)sender;
- (void)quit:(id)sender;
@end
@implementation LiteTodoActions
- (void)show:(id)sender{(void)sender;[NSApp activateIgnoringOtherApps:YES];handlers.show();}
- (void)quick:(id)sender{(void)sender;[NSApp activateIgnoringOtherApps:YES];handlers.quick();}
- (void)quit:(id)sender{(void)sender;handlers.quit();}
- (void)userNotificationCenter:(UNUserNotificationCenter*)center willPresentNotification:(UNNotification*)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completion{
    (void)center;(void)notification;completion(UNNotificationPresentationOptionBanner|UNNotificationPresentationOptionSound);
}
@end
namespace {LiteTodoActions* actions=nil;}
bool platform_init(const PlatformCallbacks& callbacks){
    handlers=callbacks;actions=[LiteTodoActions new];item=[[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    if(!item)return false;item.button.title=@"✓";item.button.toolTip=@"轻待办";
    NSMenu* menu=[[NSMenu alloc] initWithTitle:@"轻待办"];
    NSArray<NSString*>* titles=@[@"打开轻待办",@"快速记录",@"退出"];
    SEL selectors[]={@selector(show:),@selector(quick:),@selector(quit:)};
    for(int i=0;i<3;++i){NSMenuItem* entry=[[NSMenuItem alloc] initWithTitle:titles[i] action:selectors[i] keyEquivalent:@""];entry.target=actions;[menu addItem:entry];}
    item.menu=menu;
    if([[NSBundle mainBundle] bundleIdentifier]){
        UNUserNotificationCenter* center=[UNUserNotificationCenter currentNotificationCenter];center.delegate=actions;
        [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings){authorized.store(settings.authorizationStatus==UNAuthorizationStatusAuthorized); }];
    }
    return true;
}
bool platform_notify(const std::string& title,const std::string& text,std::function<void(bool)> completion){
    if(![[NSBundle mainBundle] bundleIdentifier])return false;
    NSString* a=[[NSString alloc] initWithUTF8String:title.c_str()],*b=[[NSString alloc] initWithUTF8String:text.c_str()];if(!a||!b)return false;
    UNUserNotificationCenter* center=[UNUserNotificationCenter currentNotificationCenter];
    if(!authorized.load()){
        [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert|UNAuthorizationOptionSound completionHandler:^(BOOL granted,NSError* error){authorized.store(granted&&!error);}];
        // 等用户授权后再重试，不能提前把任务标成已经提醒。
        return false;
    }
    UNMutableNotificationContent* content=[UNMutableNotificationContent new];content.title=a;content.body=b;content.sound=[UNNotificationSound defaultSound];
    UNNotificationRequest* request=[UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString] content:content trigger:nil];
    [center addNotificationRequest:request withCompletionHandler:^(NSError* error){if(completion)completion(error==nil);}];
    // 确认回调由 UI 投递队列接收，数据库不在后台线程访问。
    return true;
}
bool platform_tray_available(){return item!=nil;}
void platform_window_icon(Fl_Window*){}
bool platform_autostart_supported(){return false;}
bool platform_autostart_enabled(){return false;}
bool platform_set_autostart(bool,const std::string&){return false;}
bool platform_system_dark(){return [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"]!=nil;}
bool platform_opacity(Fl_Window* window,int percent){NSWindow* native=(NSWindow*)fl_mac_xid(window);if(!native)return false;native.alphaValue=std::clamp(percent,45,100)/100.0;return true;}
bool platform_desktop_supported(){return false;}
bool platform_desktop(Fl_Window*,bool){return false;}
bool platform_open_folder(const std::filesystem::path& path){return [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[[NSString alloc] initWithUTF8String:path.u8string().c_str()]]];}
bool platform_hotkey_supported(){return false;}
bool platform_hotkey(bool,unsigned int,int){return false;}
void platform_poll(){}
bool platform_topmost(Fl_Window* window,bool enabled){
    NSWindow* native=(NSWindow*)fl_mac_xid(window);
    if(!native)return false;
    native.level=enabled?NSFloatingWindowLevel:NSNormalWindowLevel;
    return true;
}
void platform_shutdown(){if(item)[[NSStatusBar systemStatusBar] removeStatusItem:item];item=nil;actions=nil;}
const char* platform_tray_description(){return item?u8"菜单栏入口已启用 · 关闭窗口后继续提醒":u8"菜单栏入口不可用 · 关闭窗口将退出";}

const char* platform_notification_feedback(){return u8"请在系统设置中允许轻待办通知";}
const char* platform_notification_confirmation(){return u8"系统通知服务已接收提醒";}
bool platform_open_notification_settings(){return [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.Notifications-Settings.extension"]];}
