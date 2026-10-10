#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/pending_action.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// Ramasser un objet au sol : y marcher, puis le prendre en arrivant.
//
// Le serveur ne fait pas ce trajet pour nous. Ramasser exige d'être à portée,
// et il ne rapproche personne — pas plus ici que pour l'attaque, où nous
// avions cru le contraire : voir `ro/sim/pending_action.h`. C'est donc le
// client qui demande la marche jusqu'à l'objet, puis le ramassage une fois
// arrêté à côté. Cette intention n'est PAS un fait du monde : le serveur n'en
// sait rien, et elle vit à part de lui.

// De combien de cases un objet se laisse prendre : la plus grande des deux
// différences de coordonnées, deux au plus (pc_takeitem, dans src/map/pc.cpp,
// par check_distance_bl). Plus loin, le serveur refuse — sans rien dire
// d'exploitable.
inline constexpr std::int32_t kPickupReachCells = 2;

// Le temps laissé au serveur pour répondre à la marche demandée, en
// millisecondes. Une marche refusée — case bloquée, trop loin — ne reçoit
// AUCUNE réponse : c'est l'absence de déplacement passé ce délai qui la dit.
inline constexpr std::uint32_t kWalkAnswerGraceMs = 1000;

[[nodiscard]] bool WithinPickupReach(CellPosition actor, CellPosition item) noexcept;

struct PickupIntent {
  std::uint32_t item_id = 0;
  // L'instant du clic, dans l'horloge de l'APPELANT — puis celui de la marche,
  // quand le plancher l'a fait attendre.
  std::uint32_t since_ms = 0;
  // LA MARCHE QUE LE PLANCHER A RETENUE, et qui reste à demander : la case de
  // l'objet. Vide quand elle est partie, ou quand l'objet était à portée.
  std::optional<CellPosition> walk_toward;
};

// La marche vers l'objet que le plancher avait retenue, si elle peut partir
// maintenant : rend sa case, et l'intention la tient pour partie.
//
// 🔴 ELLE SE REDEMANDE À CHAQUE TIC tant que le plancher la retient, comme la
// marche d'approche du client — sans quoi un ramassage cliqué juste après une
// autre marche ne mènerait nulle part.
[[nodiscard]] std::optional<CellPosition> DuePickupWalk(const World& world, PickupIntent& intent,
                                                        ApproachWalkClock& clock,
                                                        std::uint32_t now_ms);

// Ce que l'intention demande à cet instant.
enum class PickupStep : std::uint8_t {
  // Rien encore : la marche n'a pas fini, ou pas commencé.
  kWait,
  // Le joueur est arrêté à portée : demander l'objet. L'intention est alors
  // CONSOMMÉE — la redemander harcèlerait le serveur.
  kPickUp,
  // L'objet a disparu, le joueur s'est arrêté trop loin, ou le serveur n'a
  // jamais répondu : abandonner.
  kGiveUp,
};

// `player_path` est le chemin du joueur, celui que le planificateur lui donne ;
// `now_ms` vient de l'appelant, jamais d'une horloge.
[[nodiscard]] PickupStep AdvancePickup(const World& world, const WalkPath& player_path,
                                       const PickupIntent& intent, std::uint32_t now_ms);

}  // namespace ro::sim
