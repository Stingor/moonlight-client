#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "ro/ui/options_window.h"
#include "ro/ui/window_places.h"

#include "game/minimap_layout_slot.h"
#include "game/profile_keeper.h"
#include "game/ui_layout_file.h"

namespace moonclient {

class UiStage;

// LA DISPOSITION D'USINE : la place de chaque fenêtre et ce qui s'ouvre au
// premier lancement. Ce que « Réinitialiser » repose. Une fenêtre qu'elle ne
// nomme pas reprend la place que son code lui donne à la première ouverture.
// La minimap y a ses défauts d'usine.
[[nodiscard]] ro::ui::LayoutProfile FactoryLayout();

// LA DISPOSITION DES FENÊTRES, vue comme une famille de profils : ce que
// `ProfileKeeper` relève, pose et range.
class LayoutPort final : public ProfilePort<ro::ui::LayoutProfile> {
 public:
  explicit LayoutPort(std::filesystem::path client_root)
      : client_root_(client_root), minimap_(std::move(client_root)) {}

  // L'interface dont on range la disposition. ⚠ AVANT TOUT AUTRE APPEL.
  void Attach(UiStage& ui) noexcept { ui_ = &ui; }

  [[nodiscard]] std::uint64_t Revision() override;
  [[nodiscard]] ro::ui::LayoutProfile Capture() override;
  void Pose(const ro::ui::LayoutProfile& profile) override;
  [[nodiscard]] ro::ui::LayoutProfile Defaults() override { return FactoryLayout(); }
  // ⚠ SANS FICHIER DE PROFILS, « Default » EST LA DISPOSITION D'USINE — sa
  // minimap exceptée, qu'il hérite de `navigation.yml`.
  //
  // ⚠ LE FICHIER D'IMGUI N'Y COMPTE PAS : il ne porte qu'un cache des places,
  // et la vérité est dans le profil.
  [[nodiscard]] bool Load(UiLayoutBook& into) override;
  void Save(const UiLayoutBook& from) override;

  // La présentation de la minimap que la partie dessine.
  [[nodiscard]] ro::ui::MinimapLayout& minimap() noexcept { return minimap_.minimap(); }

 private:
  std::filesystem::path client_root_;
  UiStage* ui_ = nullptr;
  MinimapLayoutSlot minimap_;
  // LES INTERRUPTEURS N'ONT PAS DE RÉVISION À EUX : on compare, et l'on compte.
  std::map<std::string, bool> seen_toggles_;
  std::uint64_t toggle_revision_ = 0;
};

// LES PROFILS DE DISPOSITION, VIVANTS. ⚠ À LA COQUILLE ET NON À LA PARTIE : la
// disposition traverse les parties, et le personnage suivant pose la sienne en
// entrant.
class UiLayouts {
 public:
  explicit UiLayouts(std::filesystem::path client_root)
      : port_(std::move(client_root)), keeper_(port_, &Text) {}

  // Au démarrage, l'interface ouverte : lit le fichier, et pose « Default ».
  void Start(UiStage& ui) {
    port_.Attach(ui);
    keeper_.Start(true);
  }
  void EnterCharacter(std::string_view character) { keeper_.EnterCharacter(character); }
  void Tick(std::uint32_t now_ms) { keeper_.Tick(now_ms); }
  void Flush() { keeper_.Flush(); }
  [[nodiscard]] ro::ui::OptionSectionBody Body() { return keeper_.Body(); }
  // La présentation de la minimap, rangée avec la disposition : la partie la
  // dessine et la règle, le gardien l'enregistre dans le profil posé.
  [[nodiscard]] ro::ui::MinimapLayout& minimap() noexcept { return port_.minimap(); }

 private:
  [[nodiscard]] static ProfileFamilyText Text();

  LayoutPort port_;
  ProfileKeeper<ro::ui::LayoutProfile> keeper_;
};

}  // namespace moonclient
