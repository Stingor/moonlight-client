#pragma once

#include <optional>
#include <vector>

#include "ro/ui/options_window.h"

namespace moonclient {

// ── LE RELAI DU CANAL DISCORD, DANS LA SECTION CONVERSATION ─────────────────
//
// Une case : ce qui se dit sur le canal Discord du serveur paraît-il dans le
// fil, à Gonryun ?
//
// 🔴 LE SERVEUR TIENT CE RÉGLAGE, PAS LE FICHIER, comme l'appel d'urgence : la
// case montre ce qu'il a dit, et un clic DEMANDE. Elle ne change que ce que ce
// client montre — la parole publique de Gonryun part vers Discord, cochée ou
// non.
inline constexpr int kDiscordRelayRowBase = 8700;

enum class DiscordRelayRow : int {
  kRelay = 0,
  kCount,
};

class DiscordRelayOptions {
 public:
  // Ce que le serveur a dit, relu à chaque image.
  void NoteRelay(const std::optional<bool>& relay) noexcept { relay_ = relay; }

  // Ajoute sa ligne à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était la sienne. ⚠ RIEN À ÉCRIRE DANS LE FICHIER :
  // le geste devient une demande, que la partie envoie.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // La demande en attente, rendue une seule fois.
  [[nodiscard]] std::optional<bool> TakeRequest() noexcept;

 private:
  std::optional<bool> relay_;
  std::optional<bool> requested_;
};

}  // namespace moonclient
