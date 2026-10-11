#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/actor.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {
class WalkMap;
}  // namespace ro::sim

namespace moonclient {

// ── LE SUIVI D'UN JOUEUR ────────────────────────────────────────────────────
//
// Maj + appui du bouton droit sur un autre joueur : le personnage marche vers
// lui, une fois par seconde, jusqu'à ce que le bouton gauche s'enfonce ou que
// le joueur reste hors de vue dix secondes. S'il reparaît avant — de l'autre
// côté d'un portail —, le suivi reprend.
//
// Ce fichier tient l'état et la décision de chaque image, sans rien envoyer :
// `world/follow_action.h` les joue.

// Perdu de vue plus longtemps que ce délai, en millisecondes, le joueur suivi
// n'est plus suivi.
inline constexpr std::uint32_t kFollowLostMs = 10000;

// Le pas vers le joueur suivi se redemande au plus une fois par ce délai, en
// millisecondes — STRICTEMENT au-delà.
inline constexpr std::uint32_t kFollowStepMs = 1000;

// La case du joueur suivi que la grille du client tient pour infranchissable —
// un bord, une grille en retard sur le serveur — se remplace par la plus proche
// qui se franchit, cherchée jusqu'à ce nombre d'anneaux autour d'elle.
inline constexpr std::int32_t kFollowNearestRing = 4;

// CE QUE LE SUIVI RETIENT, d'une image à l'autre. Un état d'ENTRÉE, comme
// l'engagement de la souris : le serveur n'en sait rien.
struct FollowState {
  // Le joueur suivi ; zéro : aucun suivi.
  std::uint32_t followed_id = 0;
  // Son nom au départ, pour dire l'arrêt même quand il n'est plus là.
  std::string followed_name;
  // Le dernier pas demandé ; rien avant le premier, qui part aussitôt.
  std::optional<std::uint32_t> last_step_ms;
  // Depuis quand il est hors de vue ; rien tant qu'on le voit.
  std::optional<std::uint32_t> lost_since_ms;
  // Un appui du bouton gauche est tombé depuis l'image précédente, fenêtres
  // comprises. Posé par le routage des événements, lu et effacé par le tic.
  bool left_pressed = false;

  [[nodiscard]] bool active() const noexcept { return followed_id != 0; }
};

// Seul un AUTRE JOUEUR se suit : ni soi, ni un monstre, un PNJ ou un familier.
[[nodiscard]] bool CanFollow(const ro::sim::Actor& actor, std::uint32_t own_id);

// Retient `target` comme joueur suivi, horloges remises à zéro.
void BeginFollow(FollowState& state, const ro::sim::Actor& target);

// Plus aucun suivi.
void EndFollow(FollowState& state);

// Le joueur suivi, s'il est en vue : un acteur JOUEUR de cet identifiant.
// Zéro ne désigne personne.
[[nodiscard]] const ro::sim::Actor* FollowedPlayer(const ro::sim::World& world,
                                                   std::uint32_t followed_id);

// Le joueur en vue qui porte EXACTEMENT ce nom, soi excepté.
[[nodiscard]] const ro::sim::Actor* PlayerInViewNamed(const ro::sim::World& world,
                                                      std::string_view name);

enum class FollowStep : std::uint8_t {
  kNothing,
  // L'appelant met fin au suivi, et dit pourquoi.
  kStop,
  // Se tourner vers la case, sans marcher : assis, ou dans un salon.
  kTurn,
  // Marcher jusqu'à la case.
  kWalk,
};

enum class FollowEnd : std::uint8_t {
  // Le joueur l'a lâché : le bouton gauche, le menu, la commande.
  kLetGo,
  // Hors de vue depuis plus de `kFollowLostMs`.
  kOutOfSight,
};

// Ce que l'image apporte au tic.
struct FollowTick {
  // Le bouton gauche est tenu en ce moment.
  bool left_held = false;
  // Le personnage est dans un salon de discussion.
  bool in_chat_room = false;
  std::uint32_t now_ms = 0;
};

struct FollowDecision {
  FollowStep step = FollowStep::kNothing;
  // Lu pour `kStop` seulement.
  FollowEnd end = FollowEnd::kLetGo;
  // Lue pour `kTurn` et `kWalk` seulement.
  ro::sim::CellPosition cell;
};

// LA DÉCISION DE L'IMAGE.
//
// Un appui gauche, ou le bouton tenu, arrête. Sinon on cherche le joueur suivi
// parmi les joueurs en vue : introuvable, l'horloge de perte part, et arrête
// au-delà de `kFollowLostMs` ; trouvé, elle s'efface, et un pas part si le
// précédent date de plus de `kFollowStepMs`. Le pas vise la case où le joueur
// se trouve à cet instant : assis ou en salon, le personnage se tourne vers
// elle ; sinon il y marche, ou à la case franchissable la plus proche — et
// sans case franchissable, il ne fait rien. Sans grille, la case part telle
// quelle, et le serveur tranche.
//
// ⚠ L'ÉTAT N'EST PAS VIDÉ à l'arrêt : l'appelant y met fin, après avoir lu le
// nom qu'il annonce.
[[nodiscard]] FollowDecision AdvanceFollow(FollowState& state, const ro::sim::World& world,
                                           const ro::sim::WalkMap* walk, const FollowTick& tick);

// ── `/follow` ───────────────────────────────────────────────────────────────
//
//     /follow <nom>   suit le joueur en vue de ce nom, exactement
//     /follow         arrête le suivi en cours
//
// Le mot se compare sans tenir compte de la casse ; `/followme` n'est pas
// `/follow`. Rend le nom, vide pour `/follow` seul ; rien pour une autre ligne.
[[nodiscard]] std::optional<std::string> ReadFollowCommand(std::string_view line);

}  // namespace moonclient
