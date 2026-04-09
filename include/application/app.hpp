#pragma once

// 应用模块

#include "domain/entity/document.hpp"
#include "domain/service/language.hpp"
#include "domain/service/lifecycle.hpp"
#include "protocol/lsp/basic.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace application {
class App : public std::enable_shared_from_this<application::App> {
public:
  App() = default;
  ~App() = default;

  auto Initialize(const nlohmann::json &params) -> nlohmann::json {
    auto input = lsp::InitializeParams();
    input.Decode(params);
    return lifecycle_->Initialize(input).Encode();
  }

  auto Initialized(const nlohmann::json &params) -> nlohmann::json {
    return nlohmann::json::object();
  }

  auto DidOpen(const nlohmann::json &params) -> nlohmann::json {
    auto input = lsp::DidOpenTextDocumentParams();
    input.Decode(params);
    return document_->DidOpen(input).Encode();
  }

  auto DidSave(const nlohmann::json &params) -> nlohmann::json {
    return nlohmann::json::object();
  }

  auto DidChange(const nlohmann::json &params) -> nlohmann::json {
    return nlohmann::json::object();
  }

  auto DidClose(const nlohmann::json &params) -> nlohmann::json {
    return nlohmann::json::object();
  }

  auto Rename(const nlohmann::json &params) -> nlohmann::json {
    return nlohmann::json::object();
  }

  auto Completion(const nlohmann::json &params) -> nlohmann::json {
    auto input = lsp::CompletionParams();
    return language_->Completion(input).Encode();
  }

  auto Resolve(const nlohmann::json &params) -> nlohmann::json {
    return nlohmann::json::object();
  }

private:
  std::unique_ptr<domain::service::Lifecycle> lifecycle_{
      std::make_unique<domain::service::Lifecycle>()};
  std::unique_ptr<domain::service::Language> language_{
      std::make_unique<domain::service::Language>()};
  std::unique_ptr<domain::entity::Document> document_{
      std::make_unique<domain::entity::Document>()};
};
} // namespace application