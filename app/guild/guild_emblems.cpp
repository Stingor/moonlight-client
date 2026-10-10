// Les emblèmes des guildes : voir l'en-tête pour les deux chemins d'arrivée.

#include "guild/guild_emblems.h"

#include <fstream>
#include <iterator>
#include <utility>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/formats/emblem.h"

namespace moonclient {
namespace {

constexpr std::string_view kCacheSuffix = ".emb";

// UN NOM DE MONDE EN NOM DE DOSSIER : lettres, chiffres, tirets et soulignés
// seulement. Le reste devient un souligné, plutôt qu'un chemin que le système
// refuserait.
[[nodiscard]] std::string FolderName(std::string_view world) {
  std::string folder;
  for (const char letter : world) {
    const bool plain = (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') ||
                       (letter >= '0' && letter <= '9') || letter == '-' || letter == '_';
    folder.push_back(plain ? letter : '_');
  }
  return folder.empty() ? std::string("monde") : folder;
}

[[nodiscard]] std::optional<std::string> ReadWhole(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    return std::nullopt;
  }
  return bytes;
}

[[nodiscard]] std::span<const std::uint8_t> Bytes(const std::string& text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

}  // namespace

GuildEmblems::GuildEmblems(std::string origin, std::filesystem::path cache_folder,
                           ro::render::OverlayRenderer* overlay)
    : cache_folder_(std::move(cache_folder)), overlay_(overlay) {
  if (origin.empty()) {
    return;
  }
  // 🔴 LE CLAIR, ET SEULEMENT POUR CE TRANSPORT-CI : voir `web::WebScheme`.
  transport_ =
      std::make_unique<web::HttpTransport>(std::move(origin), web::WebScheme::kPlainAllowed);
  web_ = std::make_unique<web::EmblemWeb>(*transport_);
}

GuildEmblems::~GuildEmblems() {
  // 🔴 LA COUCHE D'ÉCRAN SURVIT À LA PARTIE, ET RECONNAÎT UNE IMAGE À SON
  // ADRESSE. Une image d'étiquette détruite sans être oubliée laisserait sa
  // texture sous une adresse qu'une autre image pourra reprendre — et celle-ci
  // se montrerait avec l'emblème d'une autre guilde.
  if (overlay_ == nullptr) {
    return;
  }
  for (const auto& [key, image] : plates_.images()) {
    overlay_->ForgetAtlas(image);
  }
}

void GuildEmblems::SetIdentity(web::EmblemIdentity identity) {
  world_name_ = identity.world_name;
  if (web_ != nullptr) {
    web_->SetIdentity(std::move(identity));
  }
}

std::filesystem::path GuildEmblems::CachePath(Key key) const {
  return cache_folder_ / FolderName(world_name_) /
         (std::to_string(key.first) + "_" + std::to_string(key.second) + std::string(kCacheSuffix));
}

bool GuildEmblems::Keep(Key key, std::string image, bool to_disk) {
  auto decoded = ro::formats::DecodeEmblemImage(Bytes(image));
  if (!decoded) {
    ro::core::LogWarning("emblème de la guilde {} illisible : {}", key.first,
                         decoded.error().detail);
    return false;
  }
  if (to_disk) {
    const auto written = ro::core::WriteFileAtomically(CachePath(key), image);
    if (!written) {
      ro::core::LogWarning("emblème de la guilde {} : cache disque impossible ({})", key.first,
                           written.error().detail);
    }
  }
  plates_.Store(key.first, key.second, std::move(*decoded));
  served_.insert_or_assign(key, std::move(image));
  return true;
}

void GuildEmblems::Fetch(const ro::sim::GuildBook& book, Key key, std::uint32_t now_ms) {
  if (plates_.Settled(key.first, key.second)) {
    return;
  }
  if (const ro::sim::PushedEmblem* pushed = book.PushedEmblemOf(key.first);
      pushed != nullptr && pushed->version == key.second) {
    auto bmp = ro::formats::InflateEmblem(pushed->compressed);
    if (bmp && Keep(key, std::string(bmp->begin(), bmp->end()), /*to_disk=*/true)) {
      return;
    }
  }
  if (looked_on_disk_.insert(key).second) {
    if (auto cached = ReadWhole(CachePath(key))) {
      if (Keep(key, std::move(*cached), /*to_disk=*/false)) {
        return;
      }
      // UN FICHIER DE CACHE ILLISIBLE S'EFFACE, et le service se réinterroge :
      // le garder le relirait à chaque session, sans jamais rien redemander.
      std::error_code ignored;
      std::filesystem::remove(CachePath(key), ignored);
    }
  }
  if (web_ != nullptr) {
    web_->Want(key.first, key.second, now_ms);
  }
}

void GuildEmblems::ShowOwn(Key key) {
  const ro::formats::Image* image = plates_.Find(key.first, key.second);
  // DÉJÀ LÀ, OU TOUJOURS ABSENTE : rien à refaire. ⚠ UNE CLÉ QUI CHANGE SANS
  // IMAGE vide l'onglet — l'ancien emblème ne vaut plus.
  if (key == own_.key && (own_.texture.has_value() || image == nullptr)) {
    return;
  }
  own_ = Shown{.key = key};
  if (image == nullptr) {
    return;
  }
  auto texture = ro::render::GlTexture::Upload(*image, ro::render::GlTexture::Filtering::kPixelArt);
  if (texture) {
    own_.texture = std::move(*texture);
    own_.width = static_cast<float>(image->width);
    own_.height = static_cast<float>(image->height);
  }
}

void GuildEmblems::Advance(const ro::sim::GuildBook& book, std::uint32_t now_ms) {
  if (web_ != nullptr) {
    web_->Advance(now_ms);
    for (web::DownloadedEmblem& arrived : web_->TakeDownloads()) {
      // ⚠ UNE IMAGE SERVIE ILLISIBLE ne se redemande plus de la partie : le
      // service la resservirait telle quelle.
      const Key key{arrived.guild_id, arrived.version};
      if (!Keep(key, std::move(arrived.image), /*to_disk=*/true)) {
        plates_.Reject(key.first, key.second);
      }
    }
  }
  // ⚠ UNE VERSION NULLE, C'EST « PAS D'EMBLÈME » : rien à demander, et ce qu'on
  // montrait ne vaut plus.
  Key own{0, 0};
  if (book.in_guild()) {
    own = Key{book.belonging()->guild_id, book.EmblemVersionOf(book.belonging()->guild_id)};
    if (own.second > 0) {
      Fetch(book, own, now_ms);
    }
  }
  ShowOwn(own);
  for (const Key& wanted : plates_.TakeWanted()) {
    Fetch(book, wanted, now_ms);
  }
}

ro::ui::SkinImage GuildEmblems::Of(std::int32_t guild_id) const {
  if (guild_id != own_.key.first || !own_.texture.has_value()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(own_.texture->name()),
                           .width = own_.width,
                           .height = own_.height};
}

const std::string* GuildEmblems::ImageOf(std::int32_t guild_id) const {
  if (guild_id != own_.key.first) {
    return nullptr;
  }
  const auto served = served_.find(own_.key);
  return served == served_.end() || served->second.empty() ? nullptr : &served->second;
}

bool GuildEmblems::Upload(std::int32_t guild_id, std::string bmp) {
  return web_ != nullptr && web_->Upload(guild_id, std::move(bmp));
}

std::optional<web::EmblemUploadOutcome> GuildEmblems::TakeUploadOutcome() {
  return web_ != nullptr ? web_->TakeUploadOutcome() : std::nullopt;
}

}  // namespace moonclient
