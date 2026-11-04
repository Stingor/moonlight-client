#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/net/survivors_presence.h"
#include "ro/sim/signboard.h"

namespace moonclient {

// ── CE QUE L'INTERFACE ÉCRIT D'UN JOUEUR EN ARÈNE ───────────────────────────
//
// « En arène : Désert de Sograt, 07:42 » : l'arène et le temps passé, que le
// client compte lui-même depuis l'entrée que le serveur a dite (Q4). La même
// ligne sert à l'étiquette sous le nom, à la liste d'amis et à la guilde.

// UN AUTRE JOUEUR EN ARÈNE, tel que le serveur l'a dit.
struct SurvivorsPresenceSeen {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  ro::net::SurvivorsPresenceState state = ro::net::SurvivorsPresenceState::kPlaying;
  std::uint16_t arena_id = 0;
  std::uint16_t class_id = 0;
  // L'instant monotone de son entrée, tiré de la durée que le serveur a dite.
  std::int64_t entered_at_ms = 0;
  [[nodiscard]] bool operator==(const SurvivorsPresenceSeen&) const = default;
};

// Le temps passé en arène à `now_ms`, en secondes, nul avant l'entrée.
[[nodiscard]] std::uint32_t PresenceElapsedS(const SurvivorsPresenceSeen& seen,
                                             std::int64_t now_ms) noexcept;

// Le nom d'une arène, vide quand ce client ne la connaît pas.
using PresenceArenaNamer = std::function<std::string(std::uint16_t arena_id)>;

// La ligne d'un joueur à `now_ms`, déjà traduite.
[[nodiscard]] std::string PresenceLabel(const SurvivorsPresenceSeen& seen,
                                        std::string_view arena_name, std::int64_t now_ms);

// LE SALON QUE LE SERVEUR OUVRE pour le client natif, qui n'écrit pas la ligne
// sous le nom : une place, celle du joueur. moonclient écrit cette ligne, et la
// bulle ne ferait que la redire, en couvrant ce qui se tient derrière.
inline constexpr std::uint16_t kPresenceRoomLimit = 1;

// Vrai pour l'enseigne de ce salon : celle d'un joueur dont `arena_status` est
// la ligne de présence, non vide.
[[nodiscard]] bool IsPresenceRoom(const ro::sim::Signboard& board,
                                  std::string_view arena_status) noexcept;

// LES LIGNES DE TOUS LES JOUEURS EN ARÈNE, refaites à chaque image.
class SurvivorsPresenceLabels {
 public:
  void Compose(std::span<const SurvivorsPresenceSeen> seen, const PresenceArenaNamer& arena_name,
               std::int64_t now_ms);
  // La ligne d'un compte ; vide quand il n'est pas en arène.
  [[nodiscard]] std::string_view Of(std::uint32_t account_id) const noexcept;

 private:
  // Par `account_id` croissant, comme la table de la présence.
  std::vector<std::pair<std::uint32_t, std::string>> lines_;
};

}  // namespace moonclient
