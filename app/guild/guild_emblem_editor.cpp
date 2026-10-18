// L'éditeur d'emblème, vu de la partie : le dossier `emblem\` de
// l'installation, le sac pour la galerie, et les dépôts.
//
// ⚠ LE DOSSIER EST CELUI DU CLIENT D'ORIGINE, pour que les deux voient les
// mêmes fichiers — et ce que l'éditeur y enregistre, la fenêtre du client le
// verra aussi.

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "ro/core/ascii.h"
#include "ro/core/file_write.h"
#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/formats/bmp.h"
#include "ro/formats/emblem.h"
#include "ro/formats/image.h"
#include "ro/sim/world.h"

#include "guild/guild_panel.h"

namespace moonclient {
namespace {

using ro::core::Tr;

constexpr std::string_view kEmblemFolder = "emblem";
constexpr std::string_view kBmpExtension = ".bmp";
// Un fichier du dossier plus lourd que cela n'est pas un emblème : il ne se lit
// pas en entier pour le découvrir.
constexpr std::uintmax_t kFileMostBytes = 64U * 1024U;
// Le sac se relit une fois par seconde pour la galerie : il bouge au fil des
// ramassages, pas à la fréquence d'affichage.
constexpr std::uint32_t kGalleryEveryMs = 1000;

[[nodiscard]] std::string Utf8Of(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return {reinterpret_cast<const char*>(text.data()), text.size()};
}

[[nodiscard]] std::filesystem::path PathOf(std::string_view utf8) {
  return {std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size())};
}

[[nodiscard]] std::vector<std::uint8_t> ReadSmallFile(const std::filesystem::path& path) {
  std::error_code failed;
  const std::uintmax_t size = std::filesystem::file_size(path, failed);
  if (failed || size == 0 || size > kFileMostBytes) {
    return {};
  }
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Le motif d'un refus, en clair — un emblème rejeté en silence par le serveur
// serait indiscernable d'un serveur muet.
[[nodiscard]] std::string FaultText(const ro::formats::EmblemVerdict& verdict) {
  switch (verdict.fault) {
    case ro::formats::EmblemFault::kNone:
      return {};
    case ro::formats::EmblemFault::kTooShort:
      return Tr("Fichier trop court pour un BMP.");
    case ro::formats::EmblemFault::kNotBmp:
      return Tr("Ce n'est pas un BMP (signature « BM » absente).");
    case ro::formats::EmblemFault::kDeclaredSize:
      return Tr("En-tête BMP incohérent (taille déclarée != taille du fichier).");
    case ro::formats::EmblemFault::kPixelsOutside:
      return Tr("En-tête BMP incohérent (offset des pixels hors fichier).");
    case ro::formats::EmblemFault::kWrongSize:
      return ro::core::Printed(Tr("Dimensions %ldx%ld : le jeu n'affiche que du %dx%d."),
                               static_cast<long>(verdict.width), static_cast<long>(verdict.height),
                               ro::formats::kEmblemSide, ro::formats::kEmblemSide);
    case ro::formats::EmblemFault::kDepth:
      return Tr("Profondeur non gérée : utilise du 24 bits ou du 256 couleurs.");
    case ro::formats::EmblemFault::kCompressed:
      return Tr("BMP compressé (RLE) : enregistre-le sans compression.");
    case ro::formats::EmblemFault::kTooHeavy:
      return ro::core::Printed(Tr("Fichier de %zu octets : le serveur en accepte %zu au plus."),
                               verdict.bytes, ro::formats::kEmblemMostBytes);
  }
  return {};
}

// UN NOM DE FICHIER, ET RIEN DE PLUS : le joueur le tape, et un « ..\ » ou une
// barre l'enverrait écrire hors du dossier des emblèmes.
[[nodiscard]] bool PlainFileName(std::string_view name) {
  return !name.empty() && name.find('/') == std::string_view::npos &&
         name.find('\\') == std::string_view::npos && name.find(':') == std::string_view::npos &&
         name.find("..") == std::string_view::npos;
}

[[nodiscard]] ro::ui::SkinImage ImageOf(const ro::render::IconTexture& icon) {
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

}  // namespace

std::filesystem::path GuildPanel::EmblemFolder() const {
  return pieces_.client_root / std::filesystem::path(kEmblemFolder);
}

void GuildPanel::ScanEmblemFolder() {
  files_.clear();
  const std::filesystem::path folder = EmblemFolder();
  // ⚠ AVEC DES « / » : la barre inverse ne dit rien de plus au joueur, et le
  // chemin se lit mieux ainsi.
  const std::u8string generic = folder.generic_u8string();
  folder_shown_ = std::string(reinterpret_cast<const char*>(generic.data()), generic.size()) + "/";
  std::error_code failed;
  for (auto walk = std::filesystem::directory_iterator(folder, failed);
       !failed && walk != std::filesystem::directory_iterator(); walk.increment(failed)) {
    if (!walk->is_regular_file() ||
        ro::core::AsciiLower(Utf8Of(walk->path().extension())) != kBmpExtension) {
      continue;
    }
    EmblemFile file;
    file.name = Utf8Of(walk->path().filename());
    file.bytes = ReadSmallFile(walk->path());
    if (file.bytes.empty()) {
      file.why = Tr("Fichier illisible ou vide.");
    } else {
      const ro::formats::EmblemVerdict verdict = ro::formats::CheckEmblemBmp(file.bytes);
      file.usable = verdict.usable();
      file.why = FaultText(verdict);
      file.transparency = ro::formats::EmblemTransparencyPercent(file.bytes);
      // L'APERÇU, gardé par nom : une nouvelle lecture ne refait pas la texture.
      if (!previews_.contains(file.name)) {
        if (auto image = ro::formats::DecodeEmblemImage(file.bytes)) {
          if (auto texture = ro::render::GlTexture::Upload(
                  *image, ro::render::GlTexture::Filtering::kPixelArt)) {
            previews_.emplace(file.name, std::move(*texture));
          }
        }
      }
    }
    files_.push_back(std::move(file));
  }
  std::ranges::sort(files_, [](const EmblemFile& a, const EmblemFile& b) {
    return ro::core::AsciiLower(a.name) < ro::core::AsciiLower(b.name);
  });
  file_views_.clear();
  for (const EmblemFile& file : files_) {
    ro::ui::EmblemFileView view;
    view.name = file.name;
    view.usable = file.usable;
    view.why = file.why;
    view.bytes = file.bytes.size();
    view.transparency = file.transparency;
    if (const auto preview = previews_.find(file.name); preview != previews_.end()) {
      view.preview = ro::ui::SkinImage{.id = static_cast<ImTextureID>(preview->second.name()),
                                       .width = static_cast<float>(ro::formats::kEmblemSide),
                                       .height = static_cast<float>(ro::formats::kEmblemSide)};
    }
    file_views_.push_back(view);
  }
}

void GuildPanel::RefreshEmblemEditor(const ro::sim::World& world, ro::ui::GuildTabState& state,
                                     std::uint32_t now_ms) {
  view_.emblem_files = file_views_;
  view_.emblem_folder = folder_shown_;
  view_.transparency_limit = ro::formats::kEmblemTransparencyLimit;
  if (state.emblem.item_id > 0 && pieces_.item_icons != nullptr) {
    view_.typed_item_icon =
        ImageOf(pieces_.item_icons->Get(static_cast<std::uint32_t>(state.emblem.item_id)));
  }
  // LA GALERIE : une icône par objet distinct du sac, rangées par numéro — un
  // ordre qui ne bouge pas sous le curseur au fil des ramassages.
  if (state.emblem.gallery_open &&
      (!gallery_scanned_ || now_ms - gallery_scanned_ms_ >= kGalleryEveryMs)) {
    gallery_scanned_ = true;
    gallery_scanned_ms_ = now_ms;
    gallery_ids_.clear();
    for (const ro::sim::InventoryItem& item : world.inventory().items()) {
      if (item.item_id != 0) {
        gallery_ids_.push_back(item.item_id);
      }
    }
    std::ranges::sort(gallery_ids_);
    gallery_ids_.erase(std::ranges::unique(gallery_ids_).begin(), gallery_ids_.end());
  }
  gallery_.clear();
  if (state.emblem.gallery_open) {
    for (const std::uint32_t id : gallery_ids_) {
      ro::ui::EmblemItemView item;
      item.item_id = id;
      if (pieces_.item_icons != nullptr) {
        item.icon = ImageOf(pieces_.item_icons->Get(id));
      }
      if (pieces_.items != nullptr) {
        if (const ro::vfs::ItemEntry* entry = pieces_.items->Find(id); entry != nullptr) {
          item.name = entry->name;
        }
      }
      gallery_.push_back(item);
    }
  }
  view_.gallery = gallery_;
  // LE POIDS ET LA TRANSPARENCE DU DESSIN, au sens du serveur.
  const ro::ui::EmblemCanvas& canvas = state.emblem.canvas;
  if (canvas.revision != canvas_revision_) {
    canvas_revision_ = canvas.revision;
    const std::vector<std::uint8_t> bmp = ro::formats::EncodeEmblemBmp(canvas.pixel);
    canvas_bytes_ = bmp.size();
    canvas_transparency_ = ro::formats::EmblemTransparencyPercent(bmp);
  }
  view_.canvas_bytes = canvas_bytes_;
  view_.canvas_transparency = canvas_transparency_;
}

bool GuildPanel::LoadCurrentEmblem(ro::ui::GuildTabState& state, const GuildEmblems& emblems,
                                   std::int32_t guild_id) {
  const std::string* image = emblems.ImageOf(guild_id);
  if (image == nullptr) {
    return false;
  }
  const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(image->data()),
                                            image->size());
  if (auto pixels = ro::formats::EmblemPixelsFromBmp(bytes)) {
    ro::ui::LoadEmblemCanvas(state.emblem.canvas, *pixels);
    return true;
  }
  // ⚠ UN EMBLÈME PEUT ÊTRE UN GIF, déposé par le client d'origine : sa première
  // image se reprend comme une icône, centrée.
  auto decoded = ro::formats::DecodeEmblemImage(bytes);
  if (!decoded) {
    return false;
  }
  ro::ui::LoadEmblemCanvas(state.emblem.canvas, ro::formats::EmblemPixelsFromImage(*decoded));
  return true;
}

void GuildPanel::OpenEmblemEditor(ro::ui::GuildTabState& state, const GuildEmblems& emblems,
                                  std::int32_t guild_id, bool from_current) {
  // ⚠ LE DOSSIER EST RELU À CHAQUE OUVERTURE : on y dépose justement un fichier
  // juste avant de venir le choisir.
  ScanEmblemFolder();
  if (!from_current) {
    return;
  }
  // Ouvert par un clic sur l'emblème : on repart de celui qui est en place. Un
  // échec garde le dessin en cours, et le dit.
  if (LoadCurrentEmblem(state, emblems, guild_id)) {
    state.emblem.goto_paint = true;
    state.emblem.report = Tr("Emblème actuel de la guilde chargé dans l'éditeur.");
  } else {
    state.emblem.report =
        Tr("Emblème actuel illisible (pas encore téléchargé ?) : le dessin en cours est gardé.");
  }
}

bool GuildPanel::WriteEmblemFile(const std::string& file_name,
                                 const std::vector<std::uint8_t>& bmp) {
  if (!PlainFileName(file_name) || bmp.empty()) {
    return false;
  }
  const auto written = ro::core::WriteFileAtomically(
      EmblemFolder() / PathOf(file_name),
      std::string_view(reinterpret_cast<const char*>(bmp.data()), bmp.size()));
  if (!written) {
    return false;
  }
  // La vignette de ce nom a changé : celle en cache montrerait l'ancien dessin.
  previews_.erase(file_name);
  return true;
}

void GuildPanel::UploadFile(ro::ui::GuildTabState& state, GuildEmblems& emblems,
                            std::int32_t guild_id, int file) {
  if (file < 0 || file >= static_cast<int>(files_.size()) ||
      !files_[static_cast<std::size_t>(file)].usable) {
    return;
  }
  const EmblemFile& chosen = files_[static_cast<std::size_t>(file)];
  const bool started =
      emblems.Upload(guild_id, std::string(chosen.bytes.begin(), chosen.bytes.end()));
  std::string line;
  if (started) {
    line = ro::core::Printed(Tr("Envoi au service web lancé (%s)."), chosen.name.c_str());
    state.status = std::string(Tr("Emblème envoyé : ")) + chosen.name;
  } else {
    line = ro::core::Printed("%s",
                             Tr("Refusé : un envoi est déjà en cours, ou service indisponible."));
  }
  state.emblem.report = line;
}

void GuildPanel::UploadCanvas(ro::ui::GuildTabState& state, GuildEmblems& emblems,
                              std::int32_t guild_id, const std::string& file_name) {
  const std::vector<std::uint8_t> bmp = ro::formats::EncodeEmblemBmp(state.emblem.canvas.pixel);
  // 🔴 LE DESSIN S'ÉCRIT D'ABORD DANS LE DOSSIER, comme le fait le client
  // d'origine : il y reste, et la liste des fichiers le montrera.
  if (!WriteEmblemFile(file_name, bmp)) {
    state.emblem.report = std::string(Tr("Écriture impossible : emblem/")) + file_name;
    return;
  }
  const bool started = emblems.Upload(guild_id, std::string(bmp.begin(), bmp.end()));
  if (started) {
    state.emblem.report =
        ro::core::Printed(Tr("Envoi au service web lancé (%s)."), file_name.c_str());
    state.status = ro::core::Printed(Tr("Emblème dessiné envoyé (%s)."), file_name.c_str());
  } else {
    state.emblem.report = Tr("Refusé : un envoi est déjà en cours, ou service indisponible.");
  }
}

void GuildPanel::SaveCanvas(ro::ui::GuildTabState& state, const std::string& file_name) {
  const std::vector<std::uint8_t> bmp = ro::formats::EncodeEmblemBmp(state.emblem.canvas.pixel);
  if (!WriteEmblemFile(file_name, bmp)) {
    state.emblem.error = std::string(Tr("Écriture impossible : emblem/")) + file_name;
    return;
  }
  state.status = std::string(Tr("Emblème enregistré : emblem/")) + file_name;
  ScanEmblemFolder();
}

void GuildPanel::LoadFileIntoCanvas(ro::ui::GuildTabState& state, int file) {
  if (file < 0 || file >= static_cast<int>(files_.size())) {
    return;
  }
  const EmblemFile& chosen = files_[static_cast<std::size_t>(file)];
  auto pixels = ro::formats::EmblemPixelsFromBmp(chosen.bytes);
  if (!pixels) {
    state.emblem.error = Tr("Ce fichier n'est pas reprenable (dimensions ou profondeur).");
    return;
  }
  ro::ui::LoadEmblemCanvas(state.emblem.canvas, *pixels);
  // L'onglet suit, sinon le clic n'a aucun effet visible.
  state.emblem.goto_paint = true;
  state.emblem.error.clear();
  state.status = std::string(Tr("Dessin repris de ")) + chosen.name;
}

void GuildPanel::ImportItemIcon(ro::ui::GuildTabState& state, std::uint32_t item_id) {
  if (pieces_.vfs == nullptr || pieces_.items == nullptr) {
    state.emblem.report = Tr("Icône introuvable pour cet item.");
    return;
  }
  const std::string path = ro::render::ItemIconPath(*pieces_.items, item_id);
  auto bytes = path.empty() ? ro::core::Result<std::vector<std::uint8_t>>(
                                  ro::core::Fail(ro::core::ErrorCode::kNotFound, "icône"))
                            : pieces_.vfs->ReadRaw(path);
  auto image = bytes ? ro::formats::DecodeBmp(*bytes)
                     : ro::core::Result<ro::formats::Image>(std::unexpected(bytes.error()));
  if (!image) {
    state.emblem.report = Tr("Icône introuvable pour cet item.");
    return;
  }
  // ⚠ LA TRANSPARENCE DE L'ICÔNE DEVIENT DU VIDE, pas du noir.
  ro::formats::ApplyMagentaColorKey(*image);
  ro::ui::LoadEmblemCanvas(state.emblem.canvas, ro::formats::EmblemPixelsFromImage(*image));
  state.emblem.item_id = static_cast<int>(item_id);
  state.emblem.report.clear();
}

}  // namespace moonclient
