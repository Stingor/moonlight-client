#pragma once

#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LE REFUS D'OFFICE DES ÉCHANGES, DANS « GAMEPLAY » ───────────────────────
//
// Une case : toute demande d'échange reçue se refuse d'elle-même, sans que la
// boîte ne paraisse. C'est le `/notrade` (`/nt`) du client du joueur, décoché
// par défaut, et retenu d'une partie à l'autre comme chez lui.
inline constexpr int kDealRefusalRowBase = 9300;

enum class DealRefusalRow : int {
  kRefuseDeals = 0,
  kCount,
};

[[nodiscard]] constexpr int DealRefusalRowId(DealRefusalRow row) {
  return kDealRefusalRowBase + static_cast<int>(row);
}

class DealRefusalOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  // Ajoute sa ligne à celles de Gameplay, à chaque image où le panneau est
  // ouvert.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;
  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);
  [[nodiscard]] bool refuses_deals() const noexcept { return refuses_deals_; }

 private:
  bool refuses_deals_ = false;
};

}  // namespace moonclient
