#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/target_hud.h"

#include "game/settings_file.h"
#include "options/game_option_ids.h"
#include "world/targeting.h"

namespace moonclient {

// ── LA PAGE « FENÊTRE DE CIBLE » ────────────────────────────────────────────
//
// Le mode Ciblage et son HUD : ce qu'ils gardent, ce que la page montre, ce
// qu'une ligne change.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption` : elles sont
// une quarantaine, et l'énumération tient sur huit bits.
inline constexpr int kTargetRowBase = 5000;
inline constexpr int kTargetPartRowBase = 5100;
inline constexpr int kTargetFieldsPerPart = 3;

enum class TargetRow : int {
  kEnabled = 0,
  kLocked,
  kLayoutMode,
  kBorder,
  kSticky,
  kRounding,
  kTextScale,
  kGaugeText,
  kPortraitDirection,
  kPortraitAnimation,
  kPortraitAnimated,
  kNativeMarker,
  kClickNoAttack,
  kCastOnTarget,
  kProxyClick,
  kProxyUnlockedNote,
  kKeyboardNote,
  kCycleWrap,
  kEscapeClears,
  kCycleResetMs,
  kResetLayout,
  kStatusPreview,
  kStatusPartOffNote,
  kStatusIconPixels,
  kStatusGapPixels,
  kStatusMost,
  kStatusRows,
  kStatusSort,
  kStatusIconBackground,
  kStatusCountdown,
  kStatusCountdownPixels,
  kStatusSweep,
  kStatusSweepColor,
  kCount,
};

enum class TargetPartField : int { kShown = 0, kBackground, kForeground, kCount };
static_assert(static_cast<int>(TargetPartField::kCount) == kTargetFieldsPerPart);
static_assert(kTargetRowBase + static_cast<int>(TargetRow::kCount) < kTargetPartRowBase,
              "les lignes du mode Ciblage déborderaient sur celles de ses cadres");
static_assert(kEntityNameRowBase + kEntityKindsPerFamily * kEntityNameFieldsPerKind <
                  kTargetRowBase,
              "les lignes des étiquettes déborderaient sur celles du mode Ciblage");

[[nodiscard]] constexpr int TargetRowId(TargetRow row) {
  return kTargetRowBase + static_cast<int>(row);
}
[[nodiscard]] constexpr int TargetPartRowId(ro::ui::TargetPart part, TargetPartField field) {
  return kTargetPartRowBase + static_cast<int>(part) * kTargetFieldsPerPart +
         static_cast<int>(field);
}

class TargetOptions {
 public:
  [[nodiscard]] const TargetingSettings& settings() const noexcept { return settings_; }
  // En écriture : le joueur pose les cadres à la souris.
  [[nodiscard]] ro::ui::TargetHudLook& look() noexcept { return look_; }
  [[nodiscard]] const ro::ui::TargetHudLook& look() const noexcept { return look_; }
  // L'aperçu des états : de faux états, le temps de régler. Jamais persisté.
  [[nodiscard]] bool status_preview() const noexcept { return status_preview_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image de panneau ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  TargetingSettings settings_;
  ro::ui::TargetHudLook look_{.locked = true};
  bool status_preview_ = false;
  std::vector<ro::ui::OptionRowView> rows_;
  // Refaits à chaque composition : le pointeur d'un texte traduit meurt au
  // changement de langue.
  std::vector<const char*> gauge_text_names_;
  std::vector<const char*> sort_names_;
  std::vector<const char*> sweep_names_;
};

}  // namespace moonclient
