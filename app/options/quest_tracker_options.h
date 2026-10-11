#pragma once

#include <span>
#include <utility>
#include <vector>

#include "ro/ui/options_window.h"

#include "options/chat_web_options.h"

namespace moonclient {

// ── LA SECTION « JOURNAL DES QUÊTES ET SUIVI », sous l'Interface ────────────
//
// La case qui montre le panneau de suivi, puis les mêmes réglages que son clic
// droit, par le même dessin (`ro::ui::DrawTrackerSettingsItems`). Ils ne sont
// pas dans `settings.yml` mais dans `suivi.yml`, que tient la partie : c'est
// elle qui CONFIE le dessin à l'entrée en jeu, et le reprend à la sortie. Hors
// partie, la section le dit.
//
// ⚠ C'EST LE DESSIN QUI ENREGISTRE : il pose ses demandes là où le journal pose
// les siennes, et l'étage du suivi les écrit. La ligne n'a rien à écrire.
inline constexpr int kQuestTrackerRowBase = 8800;

enum class QuestTrackerRow : int {
  kPanel = 0,
  kAbsent,
  kCount,
};

// ⚠ LA CENTAINE D'ENTRE DEUX est celle du relai Discord.
static_assert(kChatOpenHostRowBase + static_cast<int>(ro::core::kMostUserHosts) <=
                  kQuestTrackerRowBase,
              "la plage du suivi des quêtes chevauche celle des sites ouverts");

class QuestTrackerOptions {
 public:
  // Le dessin des réglages, confié par la partie ; vide hors partie.
  void Entrust(ro::ui::OptionPanelDraw draw) { draw_ = std::move(draw); }

  // Refait les lignes de la section : le panneau, ou la note qui dit qu'il
  // s'ouvre en jeu.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked) const noexcept;

 private:
  ro::ui::OptionPanelDraw draw_;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
