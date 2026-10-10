#include "chat/chat_image_cache.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/formats/web_picture.h"

#include "web/preview_get.h"
#include "web/webp_picture.h"

namespace moonclient {
namespace {

// LA VISIONNEUSE : deux images gardées — celle qu'on regarde, et la précédente
// pour un aller-retour —, parce qu'une image en grand pèse jusqu'à seize
// mégaoctets de texture. Elle n'entre pas dans les vingt-quatre aperçus.
constexpr std::size_t kMostLargePictures = 2;
constexpr int kLargeSideStill = 2048;
constexpr int kLargeSideAnimated = 512;
constexpr std::size_t kLargeAnimatedBytes = 32U * 1024U * 1024U;

// LE NOM D'UNE IMAGE ENREGISTRÉE : « chat_AAAAMMJJ_HHMMSS », puis « _2 »,
// « _3 »… quand la seconde a déjà servi.
constexpr std::string_view kSavedStem = "chat_";
constexpr const char* kSavedStampFormat = "%Y%m%d_%H%M%S";
constexpr std::size_t kSavedStampCapacity = 32;

// Ce qu'un fil rend : l'image prête à téléverser, ou le motif de l'échec.
struct Prepared {
  bool ok = false;
  ro::render::ChatPicture picture;
  std::string failure;
};

[[nodiscard]] std::span<const std::uint8_t> BytesOf(const std::string& body) {
  return {reinterpret_cast<const std::uint8_t*>(body.data()), body.size()};
}

// LE TRAVAIL D'UN FIL : télécharger, décoder, réduire. Rien d'OpenGL ici.
// Le téléchargement se fait en parallèle ; le décodage, lui, attend `decoding`.
[[nodiscard]] Prepared FetchAndPrepare(const std::string& url,
                                       const ro::core::PreviewAllowList& allow,
                                       const ro::render::ChatPictureLimits& limits,
                                       std::mutex& decoding) {
  const ro::core::PreviewFetched fetched = ro::core::FetchPreviewBytes(url, allow, &PreviewGet);
  if (!fetched.ok) {
    return Prepared{.failure = fetched.failure};
  }
  const std::span<const std::uint8_t> bytes = BytesOf(fetched.body);
  const std::lock_guard<std::mutex> one_at_a_time(decoding);
  auto picture = ro::render::PrepareChatPicture(bytes, limits);
  // Le WebP se MESURE et se garde comme les autres, mais `stb_image` ne le
  // décode pas : un décodeur dédié prend le relais, pour lui seul.
  if (!picture && picture.error().code == ro::core::ErrorCode::kUnsupportedVersion &&
      ro::formats::SniffWebPicture(bytes) == ro::formats::WebPictureFormat::kWebp) {
    auto decoded = DecodeWebpPicture(bytes);
    if (decoded) {
      picture = ro::render::FitChatPicture(std::move(*decoded), limits);
    } else {
      picture = std::unexpected(decoded.error());
    }
  }
  if (!picture) {
    return Prepared{.failure = ro::core::Describe(picture.error())};
  }
  return Prepared{.ok = true, .picture = std::move(*picture)};
}

// Le téléversement, sur le fil qui tient le contexte.
[[nodiscard]] ro::core::Result<ChatPictureTextures> Upload(const ro::render::ChatPicture& picture) {
  ChatPictureTextures uploaded;
  uploaded.textures.reserve(picture.frames.size());
  for (std::size_t index = 0; index < picture.frames.size(); ++index) {
    auto texture = ro::render::GlTexture::Upload(picture.frames[index],
                                                 ro::render::GlTexture::Filtering::kSprite);
    if (!texture) {
      return std::unexpected(texture.error());
    }
    uploaded.frames.push_back(ro::ui::ChatPictureFrame{
        .texture = static_cast<ImTextureID>(texture->name()),
        .delay_ms = index < picture.delays_ms.size() ? picture.delays_ms[index] : 0});
    uploaded.textures.push_back(std::move(*texture));
  }
  if (!picture.frames.empty()) {
    uploaded.width_px = static_cast<float>(picture.frames.front().width);
    uploaded.height_px = static_cast<float>(picture.frames.front().height);
  }
  return uploaded;
}

[[nodiscard]] ro::ui::ChatPictureState StateOf(ro::core::PreviewState state) {
  switch (state) {
    case ro::core::PreviewState::kUnknown:
      return ro::ui::ChatPictureState::kUnknown;
    case ro::core::PreviewState::kPending:
      return ro::ui::ChatPictureState::kPending;
    case ro::core::PreviewState::kReady:
      return ro::ui::ChatPictureState::kReady;
    case ro::core::PreviewState::kFailed:
      return ro::ui::ChatPictureState::kFailed;
  }
  return ro::ui::ChatPictureState::kUnknown;
}

// L'heure locale de la demande, en « AAAAMMJJ_HHMMSS ». Une heure illisible
// donne les secondes depuis l'époque : un nom unique vaut mieux qu'un échec.
[[nodiscard]] std::string SavedStamp() {
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local = {};
#ifdef _WIN32
  const bool readable = localtime_s(&local, &now) == 0;
#else
  const bool readable = localtime_r(&now, &local) != nullptr;
#endif
  std::array<char, kSavedStampCapacity> stamp = {};
  if (!readable || std::strftime(stamp.data(), stamp.size(), kSavedStampFormat, &local) == 0) {
    return std::to_string(static_cast<long long>(now));
  }
  return stamp.data();
}

// L'extension que les octets eux-mêmes annoncent : l'adresse, elle, ne prouve
// rien, et bien des liens d'image n'en ont pas.
[[nodiscard]] std::string_view ExtensionOf(ro::formats::WebPictureFormat format) {
  switch (format) {
    case ro::formats::WebPictureFormat::kPng:
      return "png";
    case ro::formats::WebPictureFormat::kJpeg:
      return "jpg";
    case ro::formats::WebPictureFormat::kGif:
      return "gif";
    case ro::formats::WebPictureFormat::kBmp:
      return "bmp";
    case ro::formats::WebPictureFormat::kWebp:
      return "webp";
    case ro::formats::WebPictureFormat::kUnknown:
      break;
  }
  return {};
}

struct Saved {
  ChatSaveOutcome outcome;
  std::string failure;
};

// LE TRAVAIL D'UN FIL D'ENREGISTREMENT : les octets d'origine, tels que le
// serveur les a rendus, sans décodage ni réduction. Le nom est fixé d'avance ;
// seule l'extension vient des octets.
[[nodiscard]] Saved FetchAndSave(const std::string& url, const ro::core::PreviewAllowList& allow,
                                 const std::filesystem::path& stem_path) {
  Saved saved;
  const ro::core::PreviewFetched fetched = ro::core::FetchPreviewBytes(url, allow, &PreviewGet);
  if (!fetched.ok) {
    saved.failure = fetched.failure;
    return saved;
  }
  const std::string_view extension =
      ExtensionOf(ro::formats::SniffWebPicture(BytesOf(fetched.body)));
  if (extension.empty()) {
    saved.failure = "ce n'est pas une image connue : " + url;
    return saved;
  }
  std::filesystem::path path = stem_path;
  path += "." + std::string(extension);
  if (auto written = ro::core::WriteFileAtomically(path, fetched.body); !written) {
    saved.failure = ro::core::Describe(written.error());
    return saved;
  }
  saved.outcome.ok = true;
  saved.outcome.written = path;
  return saved;
}

}  // namespace

ChatImageCache::ChatImageCache() {
  large_.book = ro::core::PreviewBook(kMostLargePictures);
  large_.limits = ro::render::ChatPictureLimits{.most_side_still = kLargeSideStill,
                                                .most_side_animated = kLargeSideAnimated,
                                                .most_animated_bytes = kLargeAnimatedBytes};
}

ChatImageCache::Shelf& ChatImageCache::ShelfOf(ChatPictureSize size) noexcept {
  return size == ChatPictureSize::kLarge ? large_ : preview_;
}

const ChatImageCache::Shelf& ChatImageCache::ShelfOf(ChatPictureSize size) const noexcept {
  return size == ChatPictureSize::kLarge ? large_ : preview_;
}

void ChatImageCache::SetAllowList(ro::core::PreviewAllowList allow) {
  // Un échec que l'accord NEUF peut lever — une page admise dont l'image vit
  // sur un autre CDN — s'oublie : sans cela, le geste du joueur ne relancerait
  // rien, et il croirait qu'il n'a servi à rien.
  for (Shelf* shelf : {&preview_, &large_}) {
    for (auto it = shelf->failed.begin(); it != shelf->failed.end();) {
      if (allow.AllowedOnce(*it) && !allow_.AllowedOnce(*it)) {
        shelf->book.Forget(*it);
        it = shelf->failed.erase(it);
      } else {
        ++it;
      }
    }
  }
  allow_ = std::move(allow);
  ++generation_;
}

void ChatImageCache::Request(std::string_view url, ChatPictureSize size) {
  if (url.empty() || !allow_.Previewable(url)) {
    return;
  }
  Shelf& shelf = ShelfOf(size);
  std::string key(url);
  if (!shelf.book.Start(key)) {
    return;
  }
  ++generation_;
  auto slot = std::make_shared<PictureSlot>();
  shelf.pending.emplace(key, slot);
  // ⚠ LE FIL PREND SA COPIE DE TOUT : le casier par pointeur compté, l'adresse,
  // la liste et les budgets par valeur. Rien de ce qu'il touche n'appartient
  // au cache, qui peut disparaître pendant qu'il travaille.
  std::thread([slot, key = std::move(key), allow = allow_, limits = shelf.limits,
               decoding = decoding_]() {
    Prepared prepared = FetchAndPrepare(key, allow, limits, *decoding);
    {
      const std::lock_guard<std::mutex> held(slot->lock);
      slot->ok = prepared.ok;
      slot->picture = std::move(prepared.picture);
      slot->failure = std::move(prepared.failure);
    }
    slot->done.store(true, std::memory_order_release);
  }).detach();
}

void ChatImageCache::PollShelf(Shelf& shelf) {
  for (auto it = shelf.pending.begin(); it != shelf.pending.end();) {
    if (!it->second->done.load(std::memory_order_acquire)) {
      ++it;
      continue;
    }
    bool ok = false;
    ro::render::ChatPicture picture;
    std::string failure;
    {
      const std::lock_guard<std::mutex> held(it->second->lock);
      ok = it->second->ok;
      picture = std::move(it->second->picture);
      failure = std::move(it->second->failure);
    }
    const std::string key = it->first;
    it = shelf.pending.erase(it);
    ++generation_;
    if (ok) {
      auto uploaded = Upload(picture);
      if (uploaded) {
        shelf.ready.insert_or_assign(key, std::move(*uploaded));
        shelf.book.Ready(key);
        continue;
      }
      failure = ro::core::Describe(uploaded.error());
    }
    ro::core::LogInfo("aperçu d'image indisponible : {}", failure);
    shelf.book.Failed(key);
    shelf.failed.insert(key);
  }

  // Une adresse que la liste ne permet plus perd son image tout de suite :
  // retirer un site doit AGIR, pas attendre l'éviction.
  for (auto it = shelf.ready.begin(); it != shelf.ready.end();) {
    if (allow_.Previewable(it->first)) {
      ++it;
      continue;
    }
    shelf.book.Forget(it->first);
    it = shelf.ready.erase(it);
    ++generation_;
  }
  for (const std::string& evicted : shelf.book.Evict()) {
    shelf.ready.erase(evicted);
    shelf.failed.erase(evicted);
    ++generation_;
  }
}

void ChatImageCache::Poll() {
  PollShelf(preview_);
  PollShelf(large_);
  PumpSaves();
}

ro::ui::ChatPictureState ChatImageCache::State(std::string_view url, ChatPictureSize size) const {
  const Shelf& shelf = ShelfOf(size);
  if (shelf.ready.find(url) != shelf.ready.end()) {
    return ro::ui::ChatPictureState::kReady;
  }
  const ro::core::PreviewState state = shelf.book.State(url);
  // Prête au registre mais retirée de l'étagère : elle part au `Poll` suivant.
  return state == ro::core::PreviewState::kReady ? ro::ui::ChatPictureState::kUnknown
                                                 : StateOf(state);
}

const ChatPictureTextures* ChatImageCache::Ready(std::string_view url, ChatPictureSize size) const {
  const Shelf& shelf = ShelfOf(size);
  const auto found = shelf.ready.find(url);
  return found == shelf.ready.end() ? nullptr : &found->second;
}

void ChatImageCache::RequestSave(std::string_view url, const std::filesystem::path& folder) {
  if (url.empty()) {
    return;
  }
  const bool queued = std::any_of(save_queue_.begin(), save_queue_.end(),
                                  [&](const SaveJob& job) { return job.url == url; });
  if (queued || (save_running_ != nullptr && save_running_->url == url)) {
    return;
  }
  // 🔴 LE NOM SE FIXE ICI, sur le fil principal, à l'heure de la demande : deux
  // enregistrements de la même seconde prennent `_2`, `_3`…, et aucun fil n'a
  // à deviner ce qu'un autre est en train d'écrire.
  const std::string second = SavedStamp();
  if (second == save_second_) {
    ++save_rank_;
  } else {
    save_second_ = second;
    save_rank_ = 1;
  }
  std::string stem = std::string(kSavedStem) + second;
  if (save_rank_ > 1) {
    stem += "_" + std::to_string(save_rank_);
  }
  save_queue_.push_back(SaveJob{.url = std::string(url), .stem_path = folder / stem});
  PumpSaves();
}

void ChatImageCache::PumpSaves() {
  if (save_running_ != nullptr && save_running_->done.load(std::memory_order_acquire)) {
    ChatSaveOutcome outcome;
    std::string failure;
    {
      const std::lock_guard<std::mutex> held(save_running_->lock);
      outcome = std::move(save_running_->outcome);
      failure = std::move(save_running_->failure);
    }
    save_running_.reset();
    if (!outcome.ok) {
      ro::core::LogWarning("image du chat non enregistrée : {}", failure);
    }
    saves_done_.push_back(std::move(outcome));
  }
  if (save_running_ != nullptr || save_queue_.empty()) {
    return;
  }
  // UN ENREGISTREMENT À LA FOIS, dans l'ordre des demandes.
  SaveJob job = std::move(save_queue_.front());
  save_queue_.pop_front();
  save_running_ = std::make_shared<SaveSlot>();
  save_running_->url = job.url;
  std::thread([slot = save_running_, job = std::move(job), allow = allow_]() {
    Saved saved = FetchAndSave(job.url, allow, job.stem_path);
    {
      const std::lock_guard<std::mutex> held(slot->lock);
      slot->outcome = std::move(saved.outcome);
      slot->failure = std::move(saved.failure);
    }
    slot->done.store(true, std::memory_order_release);
  }).detach();
}

std::optional<ChatSaveOutcome> ChatImageCache::TakeSaved() {
  PumpSaves();
  if (saves_done_.empty()) {
    return std::nullopt;
  }
  ChatSaveOutcome outcome = std::move(saves_done_.front());
  saves_done_.pop_front();
  return outcome;
}

}  // namespace moonclient
