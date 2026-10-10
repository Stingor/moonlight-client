#pragma once

#include <vector>

#include "ro/ui/options_window.h"

#include "chat/whisper_desk.h"
#include "game/settings_file.h"
#include "options/shop_options.h"

namespace moonclient {

// ── LE RÉGLAGE DES AMIS DU CLIENT, dans la section Conversation ─────────────
//
// Ses trois cases : ouvrir une conversation 1:1 quand un AMI chuchote, quand un
// INCONNU chuchote, et sonner à l'ouverture. Ils se montrent dans les réglages
// de la conversation : la fenêtre du client qui les portait n'existe pas ici,
// et une conversation qui ne s'ouvre jamais sans qu'on sache pourquoi est
// pire qu'un réglage de plus.
//
// LES LIBELLÉS SONT CEUX DU CLIENT, devenus des textes de l'interface
// (ADR 0090).
//
// 🔴 MÉMORISÉES, À REBOURS DU CLIENT, qui les remet toutes à zéro à chaque
// lancement. Un joueur qui les coche veut les retrouver.
inline constexpr int kWhisperRowBase = 7300;

enum class WhisperRow : int {
  kOpenFromFriend = 0,
  kOpenFromStranger,
  kAlarm,
  kCount,
};

static_assert(kShopRowBase + static_cast<int>(ShopRow::kCount) <= kWhisperRowBase,
              "la plage des conversations 1:1 chevauche celle des échoppes");

class WhisperOptions {
 public:
  [[nodiscard]] const FriendSetup& setup() const noexcept { return setup_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute ses lignes à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  FriendSetup setup_;
};

}  // namespace moonclient
