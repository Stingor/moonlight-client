#pragma once

#include <array>
#include <cstdint>

#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/world_fwd.h"

#include "world/mouse_engagement.h"

namespace moonclient {

// ── LA FLÈCHE DE SÉLECTION ──────────────────────────────────────────────────
//
// Le petit triangle blanc qui flotte au-dessus de l'entité qu'on a cliquée pour
// agir. Il existe hors de tout mode : c'est le geste du jeu, et le mode Ciblage
// ne fait que le détourner vers sa cible quand il n'aurait rien montré.
//
// Une décision par image, prise sur l'engagement de la souris : cacher, montrer
// au-dessus d'une entité, ou ne RIEN toucher. Ce troisième ordre est ce qui fige
// la flèche en l'air quand l'entité quitte la vue : sa dernière pose reste, et
// son animation continue.

enum class MarkerOrder : std::uint8_t {
  kKeep,
  kHide,
  kShow,
};

struct MarkerCall {
  MarkerOrder order = MarkerOrder::kKeep;
  // L'entité à surmonter : non nulle pour `kShow` seulement.
  const ro::sim::Actor* on = nullptr;
};

// L'entité engagée, si la flèche peut encore la trouver : présente dans le
// monde, et jamais le personnage lui-même, que la recherche ne voit pas.
[[nodiscard]] const ro::sim::Actor* EngagedActor(const ro::sim::World& world, std::uint32_t id);

// Ce qu'une entité trouvée fait de la flèche. Un portail la refuse, quelle que
// soit son espèce. Ce qui n'a pas un sprite de joueur la reçoit sans autre
// examen, caché ou non ; ce qui en a un la refuse s'il est caché.
//
// ⚠ AUCUN DROIT NE LA RAMÈNE SUR UN JOUEUR CACHÉ, staff compris. Le serveur
// annonce l'option `@hide` à tous de la même façon, sans permission qui voie à
// travers, et le client d'origine ne garde sa flèche pour aucun compte : sa
// liste d'administrateurs ne joue que sur l'AFFICHAGE d'un joueur sous Hiding
// ou Cloaking — jamais sous `@hide`, jamais sur la flèche.
[[nodiscard]] MarkerCall MarkerOver(const ro::sim::Actor& actor);

// La décision de l'image. Non engagé : cacher. L'entité introuvable : garder.
// Sinon, ce qu'elle en fait.
//
// 🔴 UN SUIVI EN COURS (`followed_id`, zéro pour aucun) PASSE AVANT
// L'ENGAGEMENT, qu'il ne regarde pas : le joueur suivi hors de vue, garder ;
// en vue, ce qu'il en fait — voir `world/follow_rules.h`.
[[nodiscard]] MarkerCall NativeMarkerCall(const MouseEngagement& engagement,
                                          const ro::sim::World& world,
                                          std::uint32_t followed_id = 0);

// Sous le mode Ciblage, CACHER devient MONTRER au-dessus de la cible du HUD
// quand il y en a une. Montrer et garder ne changent pas : une entité engagée
// garde la flèche même quand le HUD en suit une autre.
[[nodiscard]] MarkerCall UnderTargetingMode(MarkerCall native, const ro::sim::Actor* hud_marked);

// La cellule où la flèche se pose : celle de l'entité, tronquée vers zéro dans
// un repère dont l'origine est au CENTRE de la carte, en unités du monde. Elle
// avance donc par pas d'une unité, un cinquième de cellule, et chaque pas la
// rapproche du centre.
//
// ⚠ UNE ENTITÉ ARRÊTÉE N'Y ÉCHAPPE PAS. Elle se tient au centre de sa cellule,
// à une demi-unité d'une unité ronde quand la grille est paire : la flèche se
// pose alors une demi-unité plus près du centre de la carte que l'entité.
//
// `map_width_cells` et `map_height_cells` sont les dimensions de la grille de
// franchissement ; `cell_units` la taille d'une cellule en unités du monde.
[[nodiscard]] ro::sim::FractionalCell MarkerCell(ro::sim::FractionalCell actor_cell,
                                                 int map_width_cells, int map_height_cells,
                                                 float cell_units);

// La flèche telle qu'elle est posée. Elle naît cachée.
struct MarkerPose {
  bool shown = false;
  std::array<float, 3> ground{};
};

// Applique l'ordre de l'image. `ground` n'est lu que pour `kShow`.
void ApplyMarker(MarkerPose& pose, MarkerOrder order, const std::array<float, 3>& ground);

}  // namespace moonclient
