// 接口：账号。启动模块依赖它，但不关心它怎么拿到凭据。
// 约定：Acquire 返回的凭据必须可直接用于启动（刷新由实现负责）。
#pragma once

#include <string>

class AccountProvider {
public:
    struct Credentials {
        std::string username;
        std::string uuid;
        std::string access_token;
        std::string user_type;  // msa / legacy
    };

    virtual ~AccountProvider() = default;
    virtual bool Acquire(Credentials& out, std::string& error) = 0;
};
