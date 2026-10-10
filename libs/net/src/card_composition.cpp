// Le sertissage des cartes : trois réponses, trois verbes. Voir l'en-tête pour
// ce que le serveur annonce — et surtout ce qu'il n'annonce pas.

#include "ro/net/card_composition.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Une place du sac, sur le fil.
constexpr std::size_t kSlotBytes = 2;

[[nodiscard]] FrameOutcome ApplyCandidates(const Frame& frame, sim::World& world) {
  core::ByteReader reader(frame.bytes);
  if (!reader.Seek(kCardCandidatesFixedSize)) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ LA LONGUEUR EST CELLE DE LA TRAME : le découpage l'a déjà lue dans
  // l'en-tête, et c'est elle qui borne la liste.
  std::vector<std::uint16_t> equip_slots;
  equip_slots.reserve(reader.remaining() / kSlotBytes);
  while (reader.remaining() >= kSlotBytes) {
    equip_slots.push_back(reader.U16());
  }
  world.composition().Offer(std::move(equip_slots));
  return FrameOutcome::kApplied;
}

[[nodiscard]] FrameOutcome ApplyCompatible(const Frame& frame, sim::World& world) {
  proto::ZcMoonlightCompatCards packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  reader.Seek(kCompatibleCardsFixedSize);
  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
  // annoncerait plus qu'elle n'en porte.
  const std::size_t fits = reader.remaining() / kSlotBytes;
  const std::size_t count =
      std::min(static_cast<std::size_t>(std::max<std::int16_t>(packet.count, 0)), fits);
  std::vector<std::uint16_t> card_slots;
  card_slots.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    card_slots.push_back(reader.U16());
  }
  return world.composition().TellCompatible(packet.index_equip, std::move(card_slots))
             ? FrameOutcome::kApplied
             : FrameOutcome::kIgnored;
}

}  // namespace

FrameOutcome ApplyCardCompositionFrame(const Frame& frame, sim::World& world) {
  switch (frame.opcode) {
    case kCardCandidatesOpcode:
      return ApplyCandidates(frame, world);

    case proto::ZcAckItemcomposition::kOpcode:
      return Decoded<proto::ZcAckItemcomposition>(frame, [&world](const auto& packet) {
        if (packet.result == kCardRefused) {
          world.composition().TellVerdict(sim::CardInsertVerdict::kRefused);
          return FrameOutcome::kApplied;
        }
        if (packet.result != kCardInserted) {
          return FrameOutcome::kIgnored;
        }
        // ⚠ LE VERDICT S'ÉCRIT MÊME SI L'UNE DES DEUX PLACES EST VIDE : le
        // client dit « réussi » avant de chercher les objets, et ne dit rien
        // de plus s'il ne les trouve pas.
        world.composition().TellVerdict(sim::CardInsertVerdict::kInserted);
        static_cast<void>(world.inventory().InsertCard(packet.equipIndex, packet.cardIndex));
        return FrameOutcome::kApplied;
      });

    case proto::ZcMoonlightCompatCards::kOpcode:
      return ApplyCompatible(frame, world);

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestCardCandidates(Connection& link, std::uint16_t card_slot) {
  proto::CzReqItemcompositionList request;
  request.index = card_slot;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestInsertCard(Connection& link, std::uint16_t card_slot,
                                     std::uint16_t equip_slot) {
  proto::CzReqItemcomposition request;
  request.index_card = card_slot;
  request.index_equip = equip_slot;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestCompatibleCards(Connection& link, std::uint16_t equip_slot) {
  proto::CzMoonlightReqCompatCards request;
  // ⚠ LE PAQUET EST FIXE MAIS PORTE UN CHAMP DE LONGUEUR, comme tous ceux du
  // serveur Moonlight : il se remplit à la main.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqCompatCards::kWireSize);
  request.index_equip = equip_slot;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
