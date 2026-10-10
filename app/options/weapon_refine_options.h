#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "items/weapon_refine_flow.h"

namespace moonclient {

// ── LA PAGE « REFINE » ──────────────────────────────────────────────────────
//
// Ses réglages et leurs défauts. ⚠ L'AFFINAGE AUTOMATIQUE JOUE SANS
// CONFIRMATION des armes qu'un échec détruit : une ligne le rappelle sous sa
// case.
inline constexpr int kWeaponRefineRowBase = 6800;

enum class WeaponRefineRow : int {
  kIntro = 0,
  kGestures,
  kSorting,
  kConfirm,
  kShowCards,
  kShowFilter,
  kHoverPreview,
  kEnterKey,
  kAutoRecast,
  kAutoRefine,
  kAutoRefineWarning,
  kHistory,
  kLogTime,
  kCount,
};

[[nodiscard]] constexpr int WeaponRefineRowId(WeaponRefineRow row) {
  return kWeaponRefineRowBase + static_cast<int>(row);
}

class WeaponRefineOptions {
 public:
  [[nodiscard]] const WeaponRefineSettings& settings() const noexcept { return settings_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  WeaponRefineSettings settings_;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
