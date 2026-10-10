#include "ro/net/area_damage.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/world.h"

namespace ro::net {

FrameOutcome ApplyAreaDamageFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcMoonlightSkillDmg::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightSkillDmg packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ LE SERVEUR NOMME QUAND MÊME LE LANCEUR, et on le vérifie : il n'envoie
  // ce paquet qu'à nous, mais un compteur qui compterait les dégâts d'autrui
  // sur la foi d'un champ qu'il n'a pas lu serait faux sans jamais le dire.
  if (packet.src_aid != world.player_id()) {
    return FrameOutcome::kIgnored;
  }
  world.damage_meter().SetSelf(world.player_id());
  world.damage_meter().Record(sim::MeterBlow{.mode = sim::MeterMode::kDamageDone,
                                             .who = world.player_id(),
                                             .amount = packet.damage,
                                             .skill_id = sim::kAreaCycleKey},
                              now_ms);
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
