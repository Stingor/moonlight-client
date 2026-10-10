#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/ui/options_window.h"

#include "options/option_sections.h"
#include "options/option_targets.h"

// LES RÉGLAGES DE L'ÉCRAN DE CONNEXION : quatre pages du panneau du jeu, pour
// régler ce qu'on voit et ce qu'on entend avant d'entrer.
//
// 🔴 LES MÊMES RÉGLAGES, PAS UNE COPIE : chaque ligne est celle du panneau, et
// chaque geste passe par `GameOptions::Change`. Ce qu'on règle ici se retrouve
// en jeu, à la même place.
//
// ⚠ LES RÉTABLISSEMENTS SONT BORNÉS À CE QU'ON VOIT. « Rétablir les réglages
// d'origine » ne rend que ces pages-là, et rétablir le son épargne les sons des
// monstres et des pets, qui ne se règlent qu'en jeu.
namespace moonclient {

class GameSettings;

// LES PAGES MONTRÉES, dans l'ordre du panneau. ⚠ L'AFFICHAGE PORTE AUSSI
// L'ÉCHELLE DE L'INTERFACE, prise à la page « Interface » : c'est sur le
// formulaire qu'on voit si elle convient.
inline constexpr std::array kLoginSettingsSections = {
    OptionSection::kDisplay,
    OptionSection::kGraphics,
    OptionSection::kSound,
    OptionSection::kSkin,
};

class LoginSettings {
 public:
  void Open() noexcept { window_.open = true; }
  void Close() noexcept { window_.open = false; }
  [[nodiscard]] ro::ui::OptionsWindowState& window() noexcept { return window_; }

  // Compose les pages, fenêtre ouverte seulement, et pose le compte à rebours
  // du filet de l'affichage. ⚠ UNE VUE SUR CE QUE GARDE CETTE CLASSE : valable
  // jusqu'au prochain appel.
  [[nodiscard]] std::span<const ro::ui::OptionSectionView> Compose(GameSettings& settings,
                                                                   std::uint32_t now_ms);

  // Fait ce que le joueur a demandé.
  void Route(const ro::ui::OptionsWindowAction& asked, GameSettings& settings,
             const OptionTargets& to, std::uint32_t now_ms);

 private:
  // Rend à l'origine la page montrée à ce rang.
  void RestoreShown(std::size_t shown_rank, GameSettings& settings);

  ro::ui::OptionsWindowState window_;
  // ⚠ ICI ET NON SUR LA PILE : les sections n'en gardent qu'une vue.
  std::vector<ro::ui::OptionRowView> display_rows_;
  std::vector<ro::ui::OptionRowView> sound_rows_;
  std::array<ro::ui::OptionSectionView, kLoginSettingsSections.size()> sections_{};
};

}  // namespace moonclient
