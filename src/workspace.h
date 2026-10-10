#pragma once
#include "store.h"
std::vector<Task> organize_text(const std::string& text);
std::vector<std::filesystem::path> drop_files(const std::string& text);
std::filesystem::path daily_backup(Store& store,const std::filesystem::path& directory);
std::vector<Attachment> copy_attachments(const std::vector<Attachment>& existing,const std::vector<std::filesystem::path>& incoming,const std::filesystem::path& folder,std::vector<std::filesystem::path>& created);
