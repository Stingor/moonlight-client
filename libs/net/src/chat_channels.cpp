// Les canaux du serveur. Voir l'en-tête pour ce que le générateur ne porte pas,
// et pour la raison d'être de ce paquet.

#include "ro/net/chat_channels.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/world.h"

#include "wire_color.h"

namespace ro::net {

FrameOutcome ApplyChatChannelFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightChannelList::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightChannelList packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }

  core::ByteReader reader(frame.bytes);
  reader.Seek(kChannelListFixedSize);

  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE. Une trame tronquée en
  // annoncerait plus qu'elle n'en porte ; on prend le plus petit des deux, et
  // le lecteur dit le reste.
  const std::size_t fits = reader.remaining() / kChannelListEntrySize;
  const std::size_t count = std::min(static_cast<std::size_t>(packet.count), fits);

  std::vector<sim::ChatChannel> channels;
  channels.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    const std::uint8_t flags = reader.U8();
    const std::uint32_t color = reader.U32();
    // Les deux noms viennent du JEU, donc de sa page de codes.
    const std::string name = core::Utf8FromCp1252(reader.FixedString(kChannelNameSize));
    const std::string alias = core::Utf8FromCp1252(reader.FixedString(kChannelNameSize));
    // ⚠ UN CANAL SANS NOM N'EN EST PAS UN, et le serveur n'en envoie pas : mais
    // une entrée vide armerait un chuchotement vers « # », que le serveur
    // refuserait sans rien dire.
    if (name.empty()) {
      continue;
    }
    sim::ChatChannel channel;
    channel.name = name;
    channel.alias = alias;
    channel.color = RgbFromBgr(color);
    channel.can_write = (flags & kChannelCanChat) != 0;
    channel.guild_only = (flags & kChannelNeedsGuild) != 0;
    channels.push_back(std::move(channel));
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }

  // ⚠ ON POSE MÊME UNE LISTE VIDE : « ce serveur n'offre aucun canal » est une
  // réponse, et la première liste arrive avant qu'on ait pu en accumuler une
  // autre.
  world.TellChatChannels(std::move(channels));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
