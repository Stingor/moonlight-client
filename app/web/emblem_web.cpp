// Le service web des emblèmes : voir l'en-tête pour les deux chemins.

#include "web/emblem_web.h"

#include <algorithm>

#include "web/json_value.h"

namespace moonclient::web {
namespace {

constexpr int kHttpOk = 200;
// Ce que le service écrit dans `Type` quand il a rangé l'image.
constexpr std::int64_t kUploadStored = 1;
constexpr std::string_view kImageTypeBmp = "BMP";
constexpr std::string_view kBmpFileName = "emblem.bmp";
constexpr std::string_view kBmpMediaType = "image/bmp";

[[nodiscard]] WebPart Field(std::string_view name, std::string value) {
  return WebPart{.name = std::string(name), .content = std::move(value)};
}

}  // namespace

std::vector<WebPart> EmblemWeb::IdentityParts(std::int32_t guild_id) const {
  // ⚠ LES NOMS DES CHAMPS SONT CEUX QUE LE SERVICE LIT, casse comprise : il
  // cherche une clé exacte et refuse la requête entière s'il en manque une.
  std::vector<WebPart> parts;
  parts.push_back(Field("AID", std::to_string(identity_.account_id)));
  parts.push_back(Field("AuthToken", identity_.token));
  parts.push_back(Field("WorldName", identity_.world_name));
  parts.push_back(Field("GDID", std::to_string(guild_id)));
  return parts;
}

void EmblemWeb::Want(std::int32_t guild_id, std::int32_t version, std::uint32_t now_ms) {
  if (guild_id <= 0 || version <= 0) {
    return;
  }
  const Key key{guild_id, version};
  if (flight_ == Flight::kDownload && flying_ == key) {
    return;
  }
  if (std::ranges::find(queue_, key) != queue_.end()) {
    return;
  }
  const auto resting = resting_until_.find(key);
  if (resting != resting_until_.end() && static_cast<std::int32_t>(now_ms - resting->second) < 0) {
    return;
  }
  queue_.push_back(key);
}

bool EmblemWeb::Upload(std::int32_t guild_id, std::string bmp) {
  if (upload_.has_value() || guild_id <= 0 || bmp.empty()) {
    return false;
  }
  upload_ = PendingUpload{.guild_id = guild_id, .bmp = std::move(bmp)};
  return true;
}

void EmblemWeb::Advance(std::uint32_t now_ms) {
  if (flight_ != Flight::kNothing) {
    std::optional<WebReply> reply = transport_->Take();
    if (!reply.has_value()) {
      return;
    }
    Digest(*reply, now_ms);
    flight_ = Flight::kNothing;
  }
  if (!ready() || transport_->busy()) {
    return;
  }
  // 🔴 LE DÉPÔT D'ABORD : c'est un geste du joueur, et il attend sa réponse.
  if (upload_.has_value() && !upload_->sent) {
    WebRequest request;
    request.path = std::string(kEmblemUploadPath);
    request.parts = IdentityParts(upload_->guild_id);
    request.parts.push_back(Field("ImgType", std::string(kImageTypeBmp)));
    request.parts.push_back(WebPart{.name = "Img",
                                    .content = upload_->bmp,
                                    .file_name = std::string(kBmpFileName),
                                    .content_type = std::string(kBmpMediaType)});
    upload_->sent = true;
    flight_ = Flight::kUpload;
    transport_->Begin(request);
    return;
  }
  if (queue_.empty()) {
    return;
  }
  flying_ = queue_.front();
  queue_.pop_front();
  WebRequest request;
  request.path = std::string(kEmblemDownloadPath);
  request.parts = IdentityParts(flying_.first);
  request.parts.push_back(Field("Version", std::to_string(flying_.second)));
  flight_ = Flight::kDownload;
  transport_->Begin(request);
}

void EmblemWeb::Digest(const WebReply& reply, std::uint32_t now_ms) {
  if (flight_ == Flight::kDownload) {
    if (reply.arrived() && reply.status == kHttpOk && !reply.body.empty()) {
      downloads_.push_back(DownloadedEmblem{flying_.first, flying_.second, reply.body});
      resting_until_.erase(flying_);
      return;
    }
    // ⚠ UN ÉCHEC SE REPOSE, IL NE SE RELANCE PAS AUSSITÔT : un emblème absent
    // du service répondrait « introuvable » soixante fois par seconde.
    resting_until_[flying_] = now_ms + kEmblemRetryMs;
    return;
  }
  EmblemUploadOutcome outcome;
  outcome.guild_id = upload_.has_value() ? upload_->guild_id : 0;
  upload_.reset();
  if (!reply.arrived()) {
    outcome.refusal = reply.error.empty() ? "le service d'emblèmes est injoignable" : reply.error;
  } else if (reply.status != kHttpOk) {
    outcome.refusal =
        "le service d'emblèmes a refusé l'image (HTTP " + std::to_string(reply.status) + ")";
  } else {
    const auto body = ParseJson(reply.body);
    if (!body.has_value() || body->WholeAt("Type") != kUploadStored ||
        body->WholeAt("version") <= 0) {
      outcome.refusal = "le service d'emblèmes a répondu sans version";
    } else {
      outcome.version = static_cast<std::int32_t>(body->WholeAt("version"));
    }
  }
  outcome_ = std::move(outcome);
}

std::vector<DownloadedEmblem> EmblemWeb::TakeDownloads() {
  std::vector<DownloadedEmblem> taken;
  taken.swap(downloads_);
  return taken;
}

std::optional<EmblemUploadOutcome> EmblemWeb::TakeUploadOutcome() {
  std::optional<EmblemUploadOutcome> taken;
  taken.swap(outcome_);
  return taken;
}

}  // namespace moonclient::web
