#include "capture/screenshot.h"

#include <chrono>
#include <ctime>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/i18n.h"
#include "ro/core/local_time.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/core/utf8_path.h"
#include "ro/formats/png.h"
#include "ro/render/screen_grab.h"

#include "capture/screenshot_folder.h"
#include "game/braces_filled.h"

namespace moonclient {
namespace {

constexpr std::string_view kScreenshotPrefix = "screen_";
constexpr std::string_view kScreenshotExtension = ".png";
// Au-delà, la seconde est saturée : on écrase plutôt que de chercher sans fin.
constexpr int kMostSameSecondShots = 100;
constexpr int kFirstYear = 1900;

// L'heure locale, telle qu'elle nomme le fichier. Vide si le système ne la
// donne pas : le suffixe suffit alors à distinguer les captures.
[[nodiscard]] std::string LocalStamp() {
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
  if (!ro::core::ToLocalTime(now, local)) {
    return {};
  }
  return ro::core::Printed("%04d-%02d-%02d_%02d-%02d-%02d", local.tm_year + kFirstYear,
                           local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
                           local.tm_sec);
}

// Le chemin montré au joueur : relatif à l'installation, comme il le cherchera.
[[nodiscard]] std::string ShownPath(const std::filesystem::path& written) {
  std::filesystem::path shown = std::filesystem::path(kSavedPicturesFolder) / written.filename();
  return ro::core::Utf8FromPath(shown.make_preferred());
}

}  // namespace

std::filesystem::path FreeScreenshotPath(const std::filesystem::path& folder,
                                         std::string_view stamp) {
  const std::string base = std::string(kScreenshotPrefix) + std::string(stamp);
  std::filesystem::path wanted = folder / (base + std::string(kScreenshotExtension));
  for (int rank = 2; rank <= kMostSameSecondShots; ++rank) {
    std::error_code failed;
    if (!std::filesystem::exists(wanted, failed) || failed) {
      return wanted;
    }
    wanted = folder / (base + "_" + std::to_string(rank) + std::string(kScreenshotExtension));
  }
  return wanted;
}

ScreenShooter::ScreenShooter(const std::filesystem::path& client_root)
    : folder_(client_root / kSavedPicturesFolder) {}

ScreenShooter::~ScreenShooter() {
  // ⚠ ON ATTEND LE FIL : il écrit dans `outcome_`, que ce destructeur libère.
  if (writer_.joinable()) {
    writer_.join();
  }
}

void ScreenShooter::GrabIfDue(int drawable_width, int drawable_height) {
  if (!due_) {
    return;
  }
  due_ = false;
  if (writer_.joinable()) {
    refusal_ = ro::core::Tr("Capture d'écran refusée : la précédente s'enregistre encore.");
    return;
  }
  const ro::render::ScreenRegion whole{.width = drawable_width, .height = drawable_height};
  auto grab = ro::render::ScreenGrab::Create(drawable_width, drawable_height);
  if (!grab) {
    ro::core::LogWarning("capture d'écran : {}", ro::core::Describe(grab.error()));
    refusal_ = ro::core::Tr("Capture d'écran impossible.");
    return;
  }
  auto picture = grab->Take(whole, drawable_height);
  if (!picture) {
    ro::core::LogWarning("capture d'écran : {}", ro::core::Describe(picture.error()));
    refusal_ = ro::core::Tr("Capture d'écran impossible.");
    return;
  }
  const std::string stamp = LocalStamp();
  written_.store(false);
  // ⚠ L'IMAGE EST CÉDÉE AU FIL, pas partagée : il en est seul lecteur.
  writer_ = std::thread([this, image = std::move(*picture), stamp] {
    auto encoded = ro::formats::EncodePng(image);
    if (!encoded) {
      ro::core::LogWarning("capture d'écran : {}", ro::core::Describe(encoded.error()));
      outcome_ = ro::core::Tr("Capture d'écran impossible.");
      written_.store(true);
      return;
    }
    const std::filesystem::path path = FreeScreenshotPath(folder_, stamp);
    const std::string_view bytes(reinterpret_cast<const char*>(encoded->data()), encoded->size());
    if (const ro::core::Result<void> saved = ro::core::WriteFileAtomically(path, bytes); !saved) {
      ro::core::LogWarning("capture d'écran : {} non écrite — {}", ro::core::Utf8FromPath(path),
                           ro::core::Describe(saved.error()));
      outcome_ = ro::core::Tr("Capture d'écran impossible : le fichier n'a pas pu s'écrire.");
    } else {
      outcome_ = BracesFilled(ro::core::Tr("Capture d'écran enregistrée : {}"), {ShownPath(path)});
    }
    written_.store(true);
  });
}

std::optional<std::string> ScreenShooter::TakeReport() {
  if (refusal_.has_value()) {
    std::optional<std::string> said = std::move(refusal_);
    refusal_.reset();
    return said;
  }
  if (!writer_.joinable() || !written_.load()) {
    return std::nullopt;
  }
  writer_.join();
  return std::move(outcome_);
}

}  // namespace moonclient
