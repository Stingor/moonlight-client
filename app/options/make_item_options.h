#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "items/make_item_flow.h"

namespace moonclient {

// ── LA PAGE « FABRICATION » ─────────────────────────────────────────────────
//
// Ses réglages et leurs défauts. ⚠ LA RELANCE PAR OBJET CONSOMME L'OBJET : elle
// a son propre réglage, et un plafond offert — jamais imposé.
inline constexpr int kMakeItemRowBase = 6700;

enum class MakeItemRow : int {
  kIntro = 0,
  kGestures,
  kSorting,
  kShowOwned,
  kShowFilter,
  kHoverPreview,
  kHistory,
  kLogTime,
  kEnterKey,
  kAutoRecast,
  kAutoReuseItem,
  kUnlimited,
  kReuseCap,
  kCount,
};

[[nodiscard]] constexpr int MakeItemRowId(MakeItemRow row) {
  return kMakeItemRowBase + static_cast<int>(row);
}

class MakeItemOptions {
 public:
  [[nodiscard]] const MakeItemSettings& settings() const noexcept { return settings_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  MakeItemSettings settings_;
  // Le dernier plafond CHOISI, gardé pendant que « sans limite » est coché :
  // décocher puis recocher ne doit pas effacer le réglage. Non retenu d'une
  // partie à l'autre — c'est une commodité d'édition.
  int reuse_cap_ = kDefaultReuseCap;
  static constexpr int kDefaultReuseCap = 10;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
