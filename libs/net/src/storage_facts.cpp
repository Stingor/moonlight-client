// Ce que le serveur dit des objets du coffre : prix, sous-type, emplacement.
// Voir l'en-tête pour ce que ce paquet est seul à porter.

#include "ro/net/storage_facts.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/storage_facts.h"
#include "ro/sim/world.h"

namespace ro::net {

FrameOutcome ApplyStorageFactsFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightStoragePrices::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightStoragePrices packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }

  core::ByteReader reader(frame.bytes);
  reader.Seek(kStorageFactsFixedSize);

  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
  // annoncerait plus qu'elle n'en porte.
  const std::size_t fits = reader.remaining() / kStorageFactsEntrySize;
  const std::size_t count = std::min(static_cast<std::size_t>(packet.count), fits);

  std::vector<sim::StorageItemFacts> told;
  told.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    sim::StorageItemFacts facts;
    facts.item_id = reader.U32();
    facts.sell_price = reader.U32();
    facts.subtype = reader.U8();
    facts.equip_mask = reader.U32();
    facts.card_slots = reader.U16();
    told.push_back(facts);
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }

  // ⚠ ON POSE MÊME UNE TABLE VIDE : un coffre vide en envoie une, et elle doit
  // faire disparaître les prix du coffre d'avant.
  world.TellStorageFacts(std::move(told));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
