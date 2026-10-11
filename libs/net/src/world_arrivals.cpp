#include "world_arrivals.h"

#include <utility>

#include "ro/sim/death_rules.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Les deux départs qui laissent un effet à la place de l'acteur (clr_type,
// dans src/map/clif.hpp) : la déconnexion, puis la téléportation — que le
// client ne montre pas d'un invisible.
constexpr std::uint8_t kVanishOutOfSight = 0;
constexpr std::uint8_t kVanishLoggedOut = 2;
constexpr std::uint8_t kVanishTeleported = 3;
// La mort, et le rafraîchissement que le serveur fait suivre d'une réapparition.
constexpr std::uint8_t kVanishDead = 1;
constexpr std::uint8_t kVanishRefreshed = 4;
constexpr std::uint32_t kLoggedOutEffect = 314;   // EF_EXIT2
constexpr std::uint32_t kTeleportedEffect = 304;  // EF_TELEPORTATION2

// Ce qu'un joueur que le serveur fait apparaître reçoit : des anneaux bleus et
// le son du portail.
constexpr std::uint32_t kAppearedPlayerEffect = 344;  // EF_ENTRY2

[[nodiscard]] bool Invisible(const sim::Actor& actor) {
  return (actor.option & sim::kOptionInvisible) != 0;
}

void NoteVanishEffect(sim::World& world, std::uint32_t actor_id, std::uint8_t vanish_type,
                      std::uint32_t now_ms) {
  const sim::Actor* leaving = world.Find(actor_id);
  if (leaving == nullptr) {
    return;
  }
  // SORTI DE VUE, il s'efface au lieu de disparaître d'un coup : une copie se
  // dessine le temps du fondu, que plus rien ne désigne.
  if (vanish_type == kVanishOutOfSight) {
    world.departures().Fade(*leaving, now_ms);
    return;
  }
  // ⚠ LA DÉCONNEXION NE REGARDE PAS L'INVISIBILITÉ, la téléportation si : c'est
  // ce que fait le client.
  if (vanish_type == kVanishLoggedOut) {
    static_cast<void>(world.NoteDeparture(actor_id, kLoggedOutEffect, now_ms));
  } else if (vanish_type == kVanishTeleported && !Invisible(*leaving)) {
    static_cast<void>(world.NoteDeparture(actor_id, kTeleportedEffect, now_ms));
  }
}

}  // namespace

void SpawnAppearing(sim::World& world, sim::Actor actor, bool player, std::uint32_t now_ms) {
  // 🔴 SEULE UNE APPARITION NEUVE SE MONTRE : un acteur déjà en vue que le
  // serveur réannonce ne clignote pas et ne s'entoure de rien.
  const std::uint32_t id = actor.id;
  const bool fresh = world.Find(id) == nullptr;
  const bool shown = !Invisible(actor);
  if (fresh && !player) {
    actor.appeared_ms = now_ms;
  }
  world.Spawn(std::move(actor));
  if (fresh && player && shown) {
    static_cast<void>(world.RecordEffect(id, kAppearedPlayerEffect, now_ms));
  }
}

bool ApplyVanish(sim::World& world, std::uint32_t actor_id, std::uint8_t vanish_type,
                 std::uint32_t now_ms) {
  if (vanish_type == kVanishDead) {
    const sim::Actor* dying = world.Find(actor_id);
    if (dying == nullptr) {
      return false;
    }
    // UN PNJ NE MEURT PAS : il garde sa pose.
    if (sim::DeathAftermathOf(*dying) == sim::DeathAftermath::kStandsUp) {
      return true;
    }
    return world.SetPosture(actor_id, sim::Posture::kDead, now_ms);
  }
  if (vanish_type != kVanishRefreshed) {
    NoteVanishEffect(world, actor_id, vanish_type, now_ms);
  }
  // Retirer un inconnu n'est pas une erreur : il a pu partir avant d'arriver.
  world.Remove(actor_id);
  return true;
}

}  // namespace ro::net
