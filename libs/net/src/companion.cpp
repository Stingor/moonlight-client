// Le chariot, la monture, le faucon. Voir l'en-tête : le protocole d'origine
// n'a aucun paquet pour les appeler, et son bitmask d'option ment sur le
// chariot.

#include "ro/net/companion.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Les valeurs du serveur pour l'ESPÈCE de compagnon : chariot, monture, faucon,
// dans cet ordre.
[[nodiscard]] std::uint8_t WireKind(sim::CompanionKind kind) {
  switch (kind) {
    case sim::CompanionKind::kCart:
      return 0;
    case sim::CompanionKind::kMount:
      return 1;
    case sim::CompanionKind::kFalcon:
      return 2;
  }
  return 0;
}

// Les valeurs du serveur pour ce que le joueur demande à son compagnon.
[[nodiscard]] std::uint8_t WireWish(CompanionWish wish) {
  switch (wish) {
    case CompanionWish::kDismiss:
      return 0;
    case CompanionWish::kSummon:
      return 1;
    case CompanionWish::kDecorate:
      return 2;
  }
  return 0;
}

}  // namespace

FrameOutcome ApplyCompanionFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightCompanionState::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightCompanionState packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }

  sim::CompanionState told;
  // ⚠ LES NIVEAUX DE CHARIOT, DE MONTURE ET DE FAUCON NE SE GARDENT PAS : ce
  // paquet ne repart pas quand le métier change, et le grimoire les dit à jour.
  // Voir `ro/sim/companion.h`.
  told.change_cart_level = packet.changecart_lv;
  // ⚠ LE CHARIOT PORTE UN TYPE, PAS UN DRAPEAU : zéro est « aucun », et un à N
  // sont autant d'apparences. Le réduire à un booléen perdrait laquelle.
  told.cart_type = packet.cart_active;
  told.mounted = packet.riding_active != 0;
  told.falcon_out = packet.falcon_active != 0;
  told.cart_decoration_most = packet.cart_deco_max;
  told.push_cart_skill = packet.pushcart_id;
  told.mount_skill = packet.riding_id;
  told.falcon_skill = packet.falcon_id;

  world.TellCompanions(told);
  return FrameOutcome::kApplied;
}

core::Result<void> RequestCompanion(Connection& link, sim::CompanionKind kind, CompanionWish wish,
                                    std::uint8_t decoration) {
  proto::CzMoonlightCompanion request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT, et le générateur la recopie telle
  // quelle. La laisser à zéro ferait rejeter la trame sans un mot.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightCompanion::kWireSize);
  request.kind = WireKind(kind);
  request.action = WireWish(wish);
  request.arg = decoration;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
