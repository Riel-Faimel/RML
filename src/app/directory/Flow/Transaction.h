// 版本安装事务：开始写 installing，成功提交 committed，失败时新版本整目录回滚。
#pragma once

#include <string>

namespace directory {
namespace flow {

class Transaction {
public:
    Transaction(std::string game_dir, std::string id, std::string role, std::string loader,
                std::string loader_version, std::string minecraft, std::string base,
                bool user_installed);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool Ok() const { return ok_; }
    const std::string& Error() const { return error_; }
    bool Commit(const std::string& name = std::string());

private:
    std::string game_dir_;
    std::string id_;
    std::string name_;
    std::string role_;
    std::string loader_;
    std::string loader_version_;
    std::string minecraft_;
    std::string base_;
    std::string error_;
    bool user_installed_ = false;
    bool ok_ = false;
    bool committed_ = false;
};

}  // namespace flow
}  // namespace directory
