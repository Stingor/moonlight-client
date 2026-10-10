// Les coffres de ce personnage, et la demande d'ouverture. Voir l'en-tête pour
// ce que ce paquet défait de `ro/net/inventory.h`.

#include "ro/net/storage_list.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/storage_shelf.h"
#include "ro/sim/world.h"

namespace ro::net {

FrameOutcome ApplyStorageListFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightStorageList::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightStorageList packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }

  core::ByteReader reader(frame.bytes);
  reader.Seek(kStorageListFixedSize);

  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
  // annoncerait plus qu'elle n'en porte.
  const std::size_t fits = reader.remaining() / kStorageListEntrySize;
  const std::size_t count = std::min(static_cast<std::size_t>(packet.count), fits);

  std::vector<sim::StorageShelf> shelves;
  shelves.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    sim::StorageShelf shelf;
    shelf.id = reader.U8();
    // Le nom vient du JEU, donc de sa page de codes.
    shelf.name = core::Utf8FromCp1252(reader.FixedString(kStorageNameSize));
    shelves.push_back(std::move(shelf));
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }

  // ⚠ ON POSE MÊME UNE LISTE VIDE : « ce personnage n'a droit à aucun coffre »
  // est une réponse, et elle doit faire disparaître les onglets d'avant.
  world.TellStorages(std::move(shelves), packet.cur_id);
  return FrameOutcome::kApplied;
}

core::Result<void> RequestOpenStorage(Connection& link, std::uint8_t storage_id) {
  proto::CzMoonlightOpenStorage request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT, et le générateur la recopie telle
  // quelle. La laisser à zéro ferait rejeter la trame sans un mot.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightOpenStorage::kWireSize);
  request.stor_id = storage_id;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
