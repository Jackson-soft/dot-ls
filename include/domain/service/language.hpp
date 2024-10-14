#pragma once

#include "domain/model/language.hpp"

// 语言相关服务
namespace domain::service {
class Language {
public:
    Language()  = default;
    ~Language() = default;

    auto Completion(const domain::model::CompletionParams &param) -> domain::model::CompletionList {
        domain::model::CompletionList result;
        return result;
    }
};
}  // namespace domain::service