#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/memorial_dungeon.h"
#include "ro/ui/memorial_dungeon_window.h"

namespace moonclient {

// ── L'ÉTAGE DE L'INSTANCE (« MEMORIAL DUNGEON ») ────────────────────────────
//
// L'instance que le réseau décrit, la vue que sa fenêtre dessine, et la
// destruction qu'elle demande.
//
// 🔴 LA FENÊTRE S'OUVRE D'ELLE-MÊME quand une instance paraît, s'ouvre ou en
// remplace une autre, et pas à chaque rappel du serveur : il redit l'instance à
// chaque changement de carte, et une fenêtre fermée par le joueur le reste.
// Alt+B la rouvre.

// LES PIÈCES DE L'ÉTAGE dans les fenêtres de l'interface.
struct MemorialWindows {
  ro::ui::MemorialDungeonWindowState window;
  // Composée à chaque avancée ; ses textes vivent dans l'étage.
  ro::ui::MemorialDungeonView view;
  // Ce que la fenêtre a demandé, lu à l'avancée suivante.
  ro::ui::MemorialDungeonWindowAction answer;
};

class MemorialStage {
 public:
  // UNE NOUVELLE PARTIE : plus d'instance, fenêtre fermée. Le serveur redira
  // la sienne à l'entrée en carte.
  void StartSession(MemorialWindows& windows);

  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 std::int64_t now_unix_s);

  // Alt+B : joué à l'avancée suivante.
  void Toggle() noexcept { toggle_asked_ = true; }

  // À CHAQUE IMAGE : la destruction demandée, l'ouverture d'elle-même, la vue.
  void Advance(MemorialWindows& windows, ro::net::Connection& link, bool link_ready,
               std::int64_t now_unix_s);

  // LES LIGNES À ÉCRIRE DANS LA CONVERSATION, déjà traduites. Consommées.
  [[nodiscard]] std::vector<std::string> TakeNotices();

  [[nodiscard]] const ro::sim::MemorialDungeon& dungeon() const noexcept { return dungeon_; }

 private:
  void Compose(ro::ui::MemorialDungeonView& view, std::int64_t now_unix_s);

  ro::sim::MemorialDungeon dungeon_;
  std::uint32_t seen_arrival_ = 0;
  ro::sim::MemorialPhase seen_phase_ = ro::sim::MemorialPhase::kNone;
  bool toggle_asked_ = false;
  // Les textes que la vue désigne.
  std::string end_clock_;
};

// La porte du bouton de destruction, d'après l'instance et le détail du
// serveur. Exposée pour les essais.
[[nodiscard]] ro::ui::MemorialDestroyGate MemorialDestroyGateOf(
    const ro::sim::MemorialDungeon& dungeon, std::int64_t now_unix_s);

}  // namespace moonclient
