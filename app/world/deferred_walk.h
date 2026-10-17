#pragma once

#include <cstdint>
#include <optional>

#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/long_walk.h"
#include "ro/sim/movement.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// CE QUE LE PERSONNAGE DU JOUEUR JOUE à une image : sa motion, et l'état
// d'action du coup qu'il montre, quand c'en est un.
struct PlayerGesture {
  ro::render::Motion motion = ro::render::Motion::kIdle;
  std::optional<std::int8_t> action_state;

  friend bool operator==(const PlayerGesture&, const PlayerGesture&) = default;
};

// LA MARCHE QUE LE GESTE DU PERSONNAGE RETIENT.
//
// 🔴 UN CLIC AU SOL PENDANT L'ATTAQUE, UN RAMASSAGE OU UNE ASSISE NE PART PAS :
// la case se GARDE, et la marche ne se demande qu'au changement de motion
// suivant — la fin du coup, le coup d'après, le relevé. Le maintien du bouton
// ne fait que remplacer la case gardée. Rien d'autre ne l'efface, pas même un
// clic sur un monstre : seul un clic au sol hors de ces trois gestes la
// retire, en partant lui-même.
//
// ⚠ LA VARIANTE D'ATTAQUE NE RETIENT RIEN : seul l'état deux, l'attaque
// ordinaire, retient la marche ; l'état neuf la laisse partir.
//
// ⚠ LA MARCHE LIBÉRÉE PASSE LE PLANCHER comme les autres, et tombe comme elles
// s'il la retient : voir `ro::sim::AdmitWalkRequest`.
struct DeferredWalk {
  std::optional<ro::sim::CellPosition> destination;
  // Le geste qui la retient, et l'instant où il a commencé : un autre geste, ou
  // le même recommencé, la libère.
  PlayerGesture held_gesture;
  std::uint32_t held_since_ms = 0;
  // L'entrée sur carte où elle a été gardée : changer de carte l'oublie.
  std::uint32_t map_entry = 0;
  // LE CLIC LOINTAIN EN COURS DE ROUTE, une fois sa première étape partie :
  // voir `ro::sim::LongWalk`.
  std::optional<ro::sim::LongWalk> legs;
};

// Vrai pour les trois gestes qui retiennent une marche : l'attaque ordinaire,
// le ramassage et l'assise.
[[nodiscard]] bool GestureHoldsWalk(const PlayerGesture& gesture) noexcept;

// L'instant où le geste `motion` du personnage a commencé, tel que le monde le
// connaît.
[[nodiscard]] std::uint32_t MotionOriginMs(const ro::sim::Actor& self, ro::render::Motion motion);

// Vrai quand `cell` est la case où le personnage se tient, ou celle où sa
// marche en cours l'emmène.
//
// ⚠ ELLE NE RETIENT QUE LA REPRISE D'UN CLIC MAINTENU, jamais l'appui : un clic
// sur ses propres pieds est le seul moyen d'arrêter une attaque continue ou une
// barre de progression, et le serveur l'accepte. Le maintien, lui, répéterait
// la même demande toutes les cent millisecondes pour rien.
[[nodiscard]] bool StandsOn(const ro::sim::World& world, ro::sim::CellPosition cell);

// UN CLIC AU SOL VERS `destination` : la case à demander MAINTENANT, ou rien.
//
// Rien quand le geste du personnage la retient (`player_gesture`, ce que la
// scène montre de lui) — la case est alors gardée dans `deferred` — ou quand
// le plancher retient la marche, qui est alors perdue. `deferred` peut être
// nul : rien ne se garde alors.
[[nodiscard]] std::optional<ro::sim::CellPosition> GroundWalkDue(
    const ro::sim::World& world, ro::sim::ApproachWalkClock& clock, DeferredWalk* deferred,
    std::optional<PlayerGesture> player_gesture, ro::sim::CellPosition destination,
    std::uint32_t now_ms);

// La case gardée, si le geste qui la retenait a changé et que le plancher la
// laisse partir. La case gardée s'efface dès que le geste a changé, qu'elle
// parte ou non.
[[nodiscard]] std::optional<ro::sim::CellPosition> ReleasedWalk(
    const ro::sim::World& world, ro::sim::ApproachWalkClock& clock, DeferredWalk& deferred,
    std::optional<PlayerGesture> player_gesture, std::uint32_t now_ms);

}  // namespace moonclient
