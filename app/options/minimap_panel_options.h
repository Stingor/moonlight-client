#pragma once

#include <span>
#include <utility>
#include <vector>

#include "ro/ui/options_window.h"

#include "options/window_title_options.h"

namespace moonclient {

// ── LA SECTION « MINIMAP », sous l'Interface ───────────────────────────────
//
// Les mêmes réglages que le sous-menu de la minimap, par le même dessin
// (`ro::ui::DrawMinimapSettings`). Ils ne sont pas dans `settings.yaml` mais dans
// `navigation.yaml`, que tient la partie : c'est elle qui CONFIE le dessin à
// l'entrée en jeu, et le reprend à la sortie. Hors partie, la section le dit.
inline constexpr int kMinimapPanelRowBase = 8100;

enum class MinimapPanelRow : int {
  kPanel = 0,
  kAbsent,
  kCount,
};

static_assert(kWindowTitleRowBase + static_cast<int>(WindowTitleRow::kCount) <=
                  kMinimapPanelRowBase,
              "la plage de la minimap chevauche celle du titre de la fenêtre");

class MinimapPanelOptions {
 public:
  // Le dessin des réglages, confié par la partie ; vide hors partie.
  void Entrust(ro::ui::OptionPanelDraw draw) { draw_ = std::move(draw); }

  // Refait les lignes de la section : le panneau, ou la note qui dit qu'il
  // s'ouvre en jeu.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était la sienne : l'enregistrement est alors dû.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // VRAI UNE FOIS par geste fini : `gesture_held` dit qu'un widget est encore
  // tenu — un curseur qu'on glisse change à chaque cran, et le fichier ne
  // s'écrit qu'au relâchement.
  [[nodiscard]] bool TakeSave(bool gesture_held) noexcept;

 private:
  ro::ui::OptionPanelDraw draw_;
  bool save_pending_ = false;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
