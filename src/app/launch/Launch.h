// 启动模块：组合版本管理、环境管理、启动；安装/补齐交给 Download。
#pragma once

#include <string>

#include "app/download/Download.h"
#include "app/launch/Environment/Environment.h"
#include "app/launch/Starter/Starter.h"
#include "app/launch/Version/Version.h"

class Launch {
public:
    explicit Launch(std::string game_dir)
        : versions_(std::move(game_dir)),
          starter_(versions_, environment_),
          download_(versions_) {}

    launch::Version& Versions() { return versions_; }
    launch::Environment& Tools() { return environment_; }
    launch::Starter& GameStarter() { return starter_; }
    Download& Downloader() { return download_; }

private:
    launch::Version versions_;
    launch::Environment environment_;
    launch::Starter starter_;
    Download download_;
};
