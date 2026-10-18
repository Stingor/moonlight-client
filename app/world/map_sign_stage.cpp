#include "world/map_sign_stage.h"

#include "ro/render/skin_pieces.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

constexpr std::string_view kSignFolder = "display_mapname\\";
constexpr std::string_view kSignExtension = ".png";
constexpr std::string_view kMapFileExtension = ".gat";

}  // namespace

std::string MapSignImagePath(std::string_view background) {
  std::string path(ro::render::kSkinRoot);
  path += kSignFolder;
  path += background;
  path += kSignExtension;
  return path;
}

std::string MapSignFileOf(std::string_view map_name) {
  std::string file(map_name);
  file += kMapFileExtension;
  return file;
}

bool MapSignWanted(const ro::vfs::MapSign& sign, MapSignMode mode) noexcept {
  switch (mode) {
    case MapSignMode::kEveryTitled:
      return sign.announced || sign.titled();
    case MapSignMode::kLikeGame:
      return sign.announced;
    case MapSignMode::kNever:
    case MapSignMode::kCount:
      return false;
  }
  return false;
}

void MapSignStage::Enter(const ro::vfs::MapSign* sign, MapSignMode mode, std::uint32_t now_ms) {
  if (sign == nullptr || !MapSignWanted(*sign, mode)) {
    shown_.reset();
    return;
  }
  shown_ = *sign;
  image_path_ = MapSignImagePath(shown_->background);
  entered_ms_ = now_ms;
}

std::optional<ro::ui::MapSignView> MapSignStage::Stage(ro::render::GameImages* images,
                                                       std::uint32_t now_ms) {
  if (!shown_.has_value()) {
    return std::nullopt;
  }
  // ⚠ UNE HEURE PLUS ANCIENNE QUE L'ARRIVÉE N'A RIEN ÉCOULÉ : le salon prend
  // l'heure de son image avant de monter la carte, et lève le bandeau après.
  // Soustraite telle quelle, elle déborderait en un bandeau déjà fini.
  const std::uint32_t elapsed_ms = now_ms >= entered_ms_ ? now_ms - entered_ms_ : 0;
  if (!ro::ui::MapSignAlpha(elapsed_ms).has_value()) {
    shown_.reset();
    return std::nullopt;
  }
  if (images == nullptr) {
    return std::nullopt;
  }
  // ⚠ `GetOurs` : le chemin est écrit en UTF-8, et le dossier d'interface est
  // en coréen.
  const ro::render::IconTexture& image = images->GetOurs(image_path_);
  if (!image.usable()) {
    return std::nullopt;
  }
  return ro::ui::MapSignView{.background = SkinImageOf(image),
                             .subtitle = shown_->subtitle,
                             .title = shown_->title,
                             .elapsed_ms = elapsed_ms};
}

}  // namespace moonclient
