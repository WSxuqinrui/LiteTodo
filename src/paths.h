#pragma once
#include <filesystem>
// 使用系统用户目录；--data-dir 为实验检查提供独立数据目录。
std::filesystem::path default_data_directory();
std::filesystem::path resolve_data_directory(const std::filesystem::path& anchor);
void remember_data_directory(const std::filesystem::path& anchor,const std::filesystem::path& destination);
