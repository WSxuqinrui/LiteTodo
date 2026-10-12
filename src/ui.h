#pragma once
#include "store.h"
#include <filesystem>
#include <memory>
class InstanceLock;
class Ui {
public:
    Ui(Store& store,const std::filesystem::path& data,bool smoke,const std::filesystem::path& anchor={},InstanceLock* instance=nullptr);
    ~Ui();
    int run();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
