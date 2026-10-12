#pragma once

#include <vector>

#include "ro/sim/talk_mute.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/quest_tracker_options.h"

namespace moonclient {

// ── LA PAROLE QU'ON NE VEUT PLUS LIRE DANS LA CONVERSATION ──────────────────
//
// Les deux bascules du jeu, sous leurs commandes `/notalkmsg` (`/nm`) et
// `/notalkmsg2` (`/nm2`) : ce qu'elles taisent est dit dans `ro::sim::TalkMute`.
// Le client d'origine les range dans ses réglages, chacune sous son titre de
// commande : elles y sont des cases comme ici.
//
// ⚠ ÉTEINTES TOUTES DEUX PAR DÉFAUT, comme au client d'origine.
inline constexpr int kTalkFilterRowBase = 8900;

enum class TalkFilterRow : int {
  kTalkHidden = 0,
  kTalkAndGuildHidden,
  kCount,
};

static_assert(kQuestTrackerRowBase + static_cast<int>(QuestTrackerRow::kCount) <=
                  kTalkFilterRowBase,
              "la plage de la parole tue chevauche celle du suivi des quêtes");

class TalkFilterOptions {
 public:
  // ⚠ UNE ADRESSE STABLE : les étages qui taisent la parole la gardent.
  [[nodiscard]] const ro::sim::TalkMute& mute() const noexcept { return mute_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute ses lignes à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // L'identifiant de la ligne d'une bascule, celui qu'une commande renvoie à
  // `GameOptions::Change` pour suivre le chemin de la fenêtre.
  [[nodiscard]] static int IdOf(TalkFilterRow row) noexcept {
    return kTalkFilterRowBase + static_cast<int>(row);
  }
  // L'état présent d'une bascule.
  [[nodiscard]] bool IsOn(TalkFilterRow row) const noexcept;

 private:
  ro::sim::TalkMute mute_;
};

}  // namespace moonclient
