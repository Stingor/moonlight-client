// Les données de compte du service web : voir l'en-tête pour les deux chemins.

#include "web/user_config_web.h"

#include <algorithm>
#include <format>

#include "web/json_value.h"

namespace moonclient::web {
namespace {

constexpr int kHttpOk = 200;
// Ce que le service écrit dans `Type` : servi avec ses données, ou servi sans.
constexpr std::int64_t kTypeServed = 1;
constexpr std::int64_t kTypeServedEmpty = 2;
constexpr std::string_view kTypeKey = "Type";
constexpr std::string_view kDataKey = "data";
// Le premier caractère qu'une chaîne JSON porte tel quel ; ceux d'avant
// s'échappent en `\uXXXX`.
constexpr unsigned char kFirstPlainCharacter = 0x20;

[[nodiscard]] WebPart Field(std::string_view name, std::string value) {
  return WebPart{.name = std::string(name), .content = std::move(value)};
}

// UNE CHAÎNE JSON, guillemets compris.
[[nodiscard]] std::string Quoted(std::string_view text) {
  std::string out = "\"";
  for (const char letter : text) {
    const auto code = static_cast<unsigned char>(letter);
    if (letter == '"' || letter == '\\') {
      out.push_back('\\');
      out.push_back(letter);
    } else if (code < kFirstPlainCharacter) {
      out += std::format("\\u{:04x}", code);
    } else {
      out.push_back(letter);
    }
  }
  out.push_back('"');
  return out;
}

[[nodiscard]] UserConfigReply ReplyOf(const WebReply& reply, std::string_view what) {
  if (!reply.arrived()) {
    return UserConfigReply{.refusal = reply.error.empty()
                                          ? std::format("{} : service injoignable", what)
                                          : std::format("{} : {}", what, reply.error)};
  }
  if (reply.status != kHttpOk) {
    return UserConfigReply{.refusal = std::format("{} : refusé (HTTP {})", what, reply.status)};
  }
  return UserConfigReply{.body = reply.body};
}

}  // namespace

ro::core::Result<std::vector<std::string>> WhisperBlocksFromUserConfig(std::string_view body) {
  ro::core::Result<JsonValue> document = ParseJson(body);
  if (!document) {
    return std::unexpected(document.error());
  }
  const std::int64_t type = document->WholeAt(kTypeKey);
  if (!document->is_object() || (type != kTypeServed && type != kTypeServedEmpty)) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          "données de compte : réponse sans « Type » servi");
  }
  std::vector<std::string> names;
  const JsonValue* data = document->Field(kDataKey);
  const JsonValue* listed = data == nullptr ? nullptr : data->Field(kWhisperBlockListKey);
  if (listed == nullptr) {
    return names;
  }
  for (const JsonValue& item : listed->Items()) {
    const std::string_view name = item.Text();
    if (item.kind() == JsonValue::Kind::kString && !name.empty() &&
        std::ranges::find(names, name) == names.end()) {
      names.emplace_back(name);
    }
  }
  return names;
}

std::string UserConfigOfWhisperBlocks(std::span<const std::string> names) {
  std::string listed;
  for (const std::string& name : names) {
    if (!listed.empty()) {
      listed.push_back(',');
    }
    listed += Quoted(name);
  }
  return std::format(R"({{"{}":{{"{}":[{}]}}}})", kDataKey, kWhisperBlockListKey, listed);
}

void UserConfigWeb::Advance() {
  if (flight_ != Flight::kNothing) {
    std::optional<WebReply> reply = transport_->Take();
    if (!reply.has_value()) {
      return;
    }
    if (flight_ == Flight::kLoad) {
      loaded_ = ReplyOf(*reply, "relecture des données de compte");
    } else {
      saved_ = ReplyOf(*reply, "sauvegarde des données de compte");
    }
    flight_ = Flight::kNothing;
  }
  if (identity_.account_id == 0 || transport_->busy()) {
    return;
  }
  // ⚠ LES NOMS DES CHAMPS SONT CEUX QUE LE SERVICE LIT, casse comprise : il
  // cherche une clé exacte et refuse la requête entière s'il en manque une.
  WebRequest request;
  request.parts.push_back(Field("AID", std::to_string(identity_.account_id)));
  if (load_wanted_) {
    load_wanted_ = false;
    request.path = std::string(kUserConfigLoadPath);
    if (!identity_.token.empty()) {
      request.parts.push_back(Field("AuthToken", identity_.token));
    }
    request.parts.push_back(Field("WorldName", identity_.world_name));
    flight_ = Flight::kLoad;
  } else if (save_document_.has_value() && ready()) {
    request.path = std::string(kUserConfigSavePath);
    request.parts.push_back(Field("AuthToken", identity_.token));
    request.parts.push_back(Field("WorldName", identity_.world_name));
    request.parts.push_back(Field(kDataKey, std::move(*save_document_)));
    save_document_.reset();
    flight_ = Flight::kSave;
  } else {
    return;
  }
  transport_->Begin(request);
}

std::optional<UserConfigReply> UserConfigWeb::TakeLoaded() {
  std::optional<UserConfigReply> taken;
  taken.swap(loaded_);
  return taken;
}

std::optional<UserConfigReply> UserConfigWeb::TakeSaved() {
  std::optional<UserConfigReply> taken;
  taken.swap(saved_);
  return taken;
}

}  // namespace moonclient::web
