// LA MISE EN PAGE DE LA TABLÉE : la lire, la graver, et nourrir le panneau
// « Personnaliser » — la galerie de décors, les mises en page enregistrées.
//
// ⚠ MÊME CLASSE, AUTRE UNITÉ DE TRADUCTION, le montage déjà employé pour la
// porte Moonlight et pour la tablée elle-même : tout ce qui suit regarde le
// disque du joueur, pas le serveur.

#include <string>
#include <system_error>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/ui/seat_layout.h"

#include "shell/lobby_screen.h"

namespace moonclient {
namespace {

// LES NOMS DES .BMP D'UN DOSSIER, ou rien s'il n'existe pas.
//
// ⚠ EN OCTETS UTF-8 : un nom accentué doit arriver tel quel au tri de la
// galerie, qui l'écarte — le convertir dans la page de code du système
// pourrait lever, et le perdre sans le dire.
[[nodiscard]] std::vector<std::string> BitmapsIn(const std::filesystem::path& folder) {
  std::vector<std::string> names;
  std::error_code code;
  for (std::filesystem::directory_iterator walk(folder, code);
       !code && walk != std::filesystem::directory_iterator(); walk.increment(code)) {
    if (!walk->is_regular_file(code)) {
      continue;
    }
    const std::u8string name = walk->path().filename().u8string();
    std::string bytes(reinterpret_cast<const char*>(name.data()), name.size());
    if (ro::core::AsciiLower(bytes).ends_with(kBackgroundSuffix)) {
      names.push_back(std::move(bytes));
    }
  }
  return names;
}

}  // namespace

void LobbyScreen::LoadLayout() {
  SeatLayoutBook book = LoadSeatLayout(shell_.client_root());
  seats_.layout = std::move(book.current);
  layout_presets_ = std::move(book.presets);
  seats_.layout_dirty = false;
}

void LobbyScreen::SaveLayoutIfDirty() {
  if (seats_.layout_dirty) {
    SaveLayout();
  }
}

void LobbyScreen::SaveLayout() {
  // ⚠ LA MARQUE TOMBE MÊME SI LE DISQUE REFUSE : réessayer à chaque sortie
  // n'y changerait rien, et le journal l'a déjà dit.
  seats_.layout_dirty = false;
  SaveSeatLayout(shell_.client_root(),
                 SeatLayoutBook{.current = seats_.layout, .presets = layout_presets_});
}

void LobbyScreen::ScanBackgrounds() {
  const std::vector<std::string> disk = BitmapsIn(UserBackgroundDirectory(shell_.client_root()));
  // ⚠ LE VFS LISTE AUSSI LE DISQUE, et c'est voulu : la galerie écarte le
  // doublon, et le fichier du joueur y garde son origine.
  const std::vector<std::string> packed =
      shell_.vfs().List(kUserBackgroundFolder, kBackgroundSuffix);
  gallery_ = ComposeGallery(disk, packed);
  // Compté ICI, et non à l'affichage : le panneau le lit à chaque image, et un
  // parcours de dossier par image est du disque pour rien.
  misplaced_backgrounds_ =
      static_cast<int>(BitmapsIn(MisplacedBackgroundDirectory(shell_.client_root())).size());
  // 🔴 UNE ABSENCE RETENUE NE SE REDEMANDE JAMAIS : sans cet oubli, un décor
  // choisi avant que son fichier ne soit déposé resterait introuvable jusqu'au
  // prochain lancement.
  if (shell_.pieces().game_images.has_value() && !shown_background_.empty()) {
    shell_.pieces().game_images->Forget(shown_background_);
  }
}

void LobbyScreen::OpenBackgroundFolder() {
  // ⚠ LE DOSSIER N'EXISTE PAS AU PREMIER USAGE : le créer AVANT de l'ouvrir,
  // sinon l'explorateur montre une erreur au lieu d'un dossier vide où déposer
  // ses images.
  const std::filesystem::path folder = UserBackgroundDirectory(shell_.client_root());
  std::error_code code;
  std::filesystem::create_directories(folder, code);
  const std::u8string where = folder.generic_u8string();
  const std::string address =
      "file:///" + std::string(reinterpret_cast<const char*>(where.data()), where.size());
  if (!SDL_OpenURL(address.c_str())) {
    ro::core::LogWarning("tablée : dossier des décors impossible à ouvrir ({})", SDL_GetError());
  }
}

void LobbyScreen::ComposeLayout(ro::ui::CharacterSelectView& view) {
  // LA SALLE, celle que la mise en page désigne.
  if (shell_.pieces().game_images.has_value()) {
    ro::render::GameImages& images = *shell_.pieces().game_images;
    const std::string wanted = ro::ui::BackgroundPath(seats_.layout);
    // ⚠ LE DÉCOR QUITTÉ S'OUBLIE : parcourir la galerie en garderait sinon une
    // texture plein écran par décor essayé. Il n'est plus dans aucune liste de
    // dessin — celle de l'image précédente est déjà rendue.
    if (!shown_background_.empty() && shown_background_ != wanted) {
      images.Forget(shown_background_);
    }
    shown_background_ = wanted;
    const ro::render::IconTexture& hall = images.GetOpaque(wanted);
    view.background_loaded = hall.usable();
    if (hall.usable()) {
      view.hall = ro::ui::SkinImage{.id = static_cast<ImTextureID>(hall.texture.name()),
                                    .width = static_cast<float>(shell_.width()),
                                    .height = static_cast<float>(shell_.height())};
    }
  }
  view.dialog_open = newcomer_.open() || doomed_.open();

  // ⚠ LE PANNEAU SEUL S'EN SERT : inutile de recopier la galerie à chaque
  // image quand il est fermé.
  if (!seats_.customizing) {
    return;
  }
  view.backgrounds = gallery_.entries;
  view.packed_backgrounds = gallery_.packed;
  view.misplaced_backgrounds = misplaced_backgrounds_;
  if (misplaced_backgrounds_ > 0) {
    const std::u8string folder = MisplacedBackgroundDirectory(shell_.client_root()).u8string();
    view.misplaced_folder.assign(reinterpret_cast<const char*>(folder.data()), folder.size());
  }
  view.presets.reserve(layout_presets_.size());
  for (const SeatLayoutPreset& preset : layout_presets_) {
    view.presets.push_back(preset.name);
  }
}

bool LobbyScreen::RouteLayout(const ro::ui::CharacterSelectAction& asked) {
  switch (asked.verb) {
    case ro::ui::CharacterSelectVerb::kBeginCustomizing:
      ScanBackgrounds();
      return true;
    case ro::ui::CharacterSelectVerb::kEndCustomizing:
      SaveLayoutIfDirty();
      return true;
    case ro::ui::CharacterSelectVerb::kRescanBackgrounds:
      ScanBackgrounds();
      return true;
    case ro::ui::CharacterSelectVerb::kOpenBackgroundFolder:
      OpenBackgroundFolder();
      return true;
    case ro::ui::CharacterSelectVerb::kSavePreset: {
      // ⚠ ÉCRIT TOUT DE SUITE, à la différence du glissement : le joueur
      // attend d'« Enregistrer » un effet immédiat sur le disque. Et l'entrée
      // écrite est désignée, nouvelle ou mise à jour.
      const int stored = StorePreset(layout_presets_, seats_.preset_name.data(), seats_.layout);
      if (stored >= 0) {
        seats_.preset_selected = stored;
        SaveLayout();
      }
      return true;
    }
    case ro::ui::CharacterSelectVerb::kApplyPreset:
      if (ApplyPreset(layout_presets_, asked.preset, seats_.layout)) {
        seats_.layout_dirty = true;
      }
      return true;
    case ro::ui::CharacterSelectVerb::kDeletePreset:
      if (DeletePreset(layout_presets_, asked.preset)) {
        SaveLayout();
      }
      return true;
    default:
      return false;
  }
}

}  // namespace moonclient
