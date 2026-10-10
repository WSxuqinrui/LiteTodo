#pragma once
#include <functional>
#include <string>
#include <filesystem>
struct PlatformCallbacks {
    std::function<void()> show,quick,quit,organize;
};
class Fl_Window;
// 浮球和预览需要桌面置顶；Linux 由窗口管理器决定是否接受。
bool platform_topmost(Fl_Window* window,bool enabled);
// 初始化失败时 UI 保持可见。
bool platform_init(const PlatformCallbacks& callbacks);
// true 表示请求已提交。Windows completion 等待 BALLOONSHOW；未显示则超时失败。
// macOS/Linux 只能确认服务接受请求；回调可能在后台线程执行。
bool platform_notify(const std::string& title,const std::string& text,std::function<void(bool)> completion={});
const char* platform_notification_feedback();
const char* platform_notification_confirmation();
bool platform_open_notification_settings();
#ifdef _WIN32
// 仅 UI 隔离验证调用：合成自身消息回执，不更改系统通知设置。
bool platform_notification_regression_test();
#endif
bool platform_tray_available();
void platform_window_icon(Fl_Window* window);
bool platform_autostart_supported();
bool platform_autostart_enabled();
// 显式保存当前数据目录，便携/隔离模式登录后仍打开同一份数据。
bool platform_set_autostart(bool enabled,const std::string& data_directory);
bool platform_system_dark();
bool platform_opacity(Fl_Window* window,int percent);
bool platform_desktop_supported();
bool platform_desktop(Fl_Window* window,bool enabled);
bool platform_open_folder(const std::filesystem::path& folder);
bool platform_hotkey_supported();
bool platform_hotkey(bool enabled,unsigned int modifiers,int key);
void platform_poll();
void platform_shutdown();
const char* platform_tray_description();
