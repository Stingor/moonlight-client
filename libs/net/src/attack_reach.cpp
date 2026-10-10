// Le coup refusé faute de portée. Voir l'en-tête pour ce que le serveur ne
// fait PAS après ce paquet, et qui revient donc au client.

#include "ro/net/attack_reach.h"

#include <cstdint>
#include <optional>

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Recale un acteur sur la case que le serveur nomme, sans chemin ni motion — le
// message 61 du client. Voir `sim::World::Reposition`.
void PlaceWhereToldTo(sim::World& world, std::uint32_t id, std::int16_t x, std::int16_t y,
                      std::uint32_t now_ms) {
  static_cast<void>(
      world.Reposition(id, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), now_ms));
}

// Le refus, quand cette trame en est un. ⚠ UN SEUL ENDROIT POUR LES DEUX
// LECTEURS : la trame est lue deux fois — par `app` pour la poursuite, par la
// chaîne de relais pour les positions —, et deux tests d'opcode côte à côte
// finiraient par diverger.
std::optional<proto::ZcAttackFailureForDistance> Refusal(const Frame& frame) {
  if (frame.opcode != proto::ZcAttackFailureForDistance::kOpcode) {
    return std::nullopt;
  }
  proto::ZcAttackFailureForDistance packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return packet;
}

}  // namespace

std::optional<std::uint32_t> TargetTooFar(const Frame& frame) {
  const std::optional<proto::ZcAttackFailureForDistance> told = Refusal(frame);
  if (!told.has_value()) {
    return std::nullopt;
  }
  return told->targetAID;
}

FrameOutcome ApplyAttackReachFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcAttackFailureForDistance::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const std::optional<proto::ZcAttackFailureForDistance> told = Refusal(frame);
  if (!told.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  const proto::ZcAttackFailureForDistance& packet = *told;

  // La portée de l'arme, que le client natif range au même endroit que celle de
  // `ZC_ATTACK_RANGE` : les deux paquets la disent, et c'est elle que la
  // poursuite consulte.
  world.SetPlayerAttackRange(packet.currentAttRange);

  PlaceWhereToldTo(world, packet.targetAID, packet.targetXPos, packet.targetYPos, now_ms);
  if (world.player_id() != 0) {
    PlaceWhereToldTo(world, world.player_id(), packet.xPos, packet.yPos, now_ms);
  }

  // Et rien de plus : la poursuite à reprendre passe par `TargetTooFar`.
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
