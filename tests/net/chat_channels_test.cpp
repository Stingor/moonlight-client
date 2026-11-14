// LES CANAUX DU SERVEUR, décodés — et ce qu'on peut leur dire.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE, et dont
// deux se voient à l'écran comme une erreur de CONFIGURATION du serveur :
//
//   · la couleur arrive en BGR et se remet à l'endroit ICI, une fois — un canal
//     bleu qui paraîtrait rouge se met sur le dos de `channels.conf` ;
//   · le nom voyage SANS son croisillon, et c'est le croisillon qui distingue
//     un canal d'un JOUEUR du même nom au moment de PARLER — mais il ne se tape
//     pas pour autant : il appartient aux charcommandes ;
//   · la longueur d'un nom de canal est VINGT, et non les vingt-quatre d'un
//     personnage — s'en tromper décale toute la liste dès la seconde entrée.

#include "ro/net/chat_channels.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/chat_target.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyChatChannelFrame;
using ro::net::ChatTarget;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::kChannelCanChat;
using ro::net::kChannelNameSize;
using ro::net::kChannelNeedsGuild;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

void PutName(std::vector<std::uint8_t>& into, std::string_view text) {
  for (std::size_t rank = 0; rank < kChannelNameSize; ++rank) {
    into.push_back(rank < text.size() ? static_cast<std::uint8_t>(text[rank]) : 0);
  }
}

struct Told {
  std::uint8_t flags = 0;
  std::uint32_t bgr = 0;
  std::string name;
  std::string alias;
};

[[nodiscard]] Frame ChannelFrame(const std::vector<Told>& channels) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightChannelList::kOpcode);
  PutU16(bytes, 0);  // la longueur, réécrite plus bas
  bytes.push_back(static_cast<std::uint8_t>(channels.size()));
  for (const Told& told : channels) {
    bytes.push_back(told.flags);
    PutU32(bytes, told.bgr);
    PutName(bytes, told.name);
    PutName(bytes, told.alias);
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return Frame{.opcode = ro::proto::ZcMoonlightChannelList::kOpcode, .bytes = std::move(bytes)};
}

}  // namespace

TEST_CASE("Canaux : la couleur arrive en BGR et se remet à l'endroit", "[net][channel]") {
  // 🔴 CE N'EST PAS UNE ÉTOURDERIE DU SERVEUR : `channel.cpp` convertit
  // « RGB to BGR » en lisant `channels.conf`, parce que c'est la forme que
  // Direct3D attend. Un décodeur qui recopierait l'entier peindrait « Global »
  // en bleu là où la configuration dit rouge, et rien ne le dirait.
  World world;
  const Frame frame = ChannelFrame({Told{.flags = kChannelCanChat,
                                         .bgr = 0x0000FFU,  // BGR : rouge
                                         .name = "global",
                                         .alias = "[Global]"}});
  CHECK(ApplyChatChannelFrame(frame, world) == FrameOutcome::kApplied);
  REQUIRE(world.chat_channels().size() == 1);
  CHECK(world.chat_channels()[0].color == 0xFF0000U);
}

TEST_CASE("Canaux : le nom voyage SANS son croisillon", "[net][channel]") {
  // Et c'est ainsi que le serveur les range : `channel_name2channel` compare
  // toujours à partir du caractère suivant. Le croisillon se remet au moment
  // d'écrire sur le fil, et nulle part ailleurs.
  World world;
  const Frame frame =
      ChannelFrame({Told{.flags = kChannelCanChat, .name = "map", .alias = ""},
                    Told{.flags = static_cast<std::uint8_t>(kChannelCanChat | kChannelNeedsGuild),
                         .name = "ally",
                         .alias = "[Alliance]"}});
  CHECK(ApplyChatChannelFrame(frame, world) == FrameOutcome::kApplied);
  REQUIRE(world.chat_channels().size() == 2);
  CHECK(world.chat_channels()[0].name == "map");
  CHECK(world.chat_channels()[0].alias.empty());
  CHECK_FALSE(world.chat_channels()[0].guild_only);
  CHECK(world.chat_channels()[1].name == "ally");
  CHECK(world.chat_channels()[1].guild_only);
}

TEST_CASE("Canaux : un canal en lecture seule se distingue", "[net][channel]") {
  // Le serveur le pousse pour qu'on sache le RECONNAÎTRE, pas pour qu'on y
  // écrive : l'armer enverrait une ligne qu'il refuserait sans un mot.
  World world;
  const Frame frame = ChannelFrame({Told{.flags = 0, .name = "annonces", .alias = "[Annonces]"}});
  CHECK(ApplyChatChannelFrame(frame, world) == FrameOutcome::kApplied);
  REQUIRE(world.chat_channels().size() == 1);
  CHECK_FALSE(world.chat_channels()[0].can_write);
}

TEST_CASE("Canaux : la liste REMPLACE, elle ne s'ajoute pas", "[net][channel]") {
  // 🔴 Le serveur envoie l'ensemble, déjà filtré par les droits du joueur.
  // L'accumuler garderait un canal qu'un changement de groupe vient de retirer.
  World world;
  CHECK(ApplyChatChannelFrame(ChannelFrame({Told{.name = "un"}, Told{.name = "deux"}}), world) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_channels().size() == 2);
  CHECK(ApplyChatChannelFrame(ChannelFrame({Told{.name = "trois"}}), world) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_channels().size() == 1);
  CHECK(world.chat_channels()[0].name == "trois");
}

TEST_CASE("Canaux : un compte plus grand que la trame ne la croit pas", "[net][channel]") {
  // Une trame tronquée annoncerait plus d'entrées qu'elle n'en porte. On prend
  // le plus petit des deux, et le lecteur dit le reste.
  World world;
  Frame frame = ChannelFrame({Told{.flags = kChannelCanChat, .name = "global"}});
  frame.bytes[4] = 9;  // le compte, menteur
  CHECK(ApplyChatChannelFrame(frame, world) == FrameOutcome::kApplied);
  CHECK(world.chat_channels().size() == 1);
}

TEST_CASE("Canaux : un autre opcode ne lui appartient pas", "[net][channel]") {
  World world;
  const Frame frame{.opcode = 0x0000, .bytes = {}};
  CHECK(ApplyChatChannelFrame(frame, world) == FrameOutcome::kUnhandled);
}

TEST_CASE("Destination : le croisillon reste aux CHARCOMMANDES", "[net][chat][channel]") {
  // 🔴 UN CANAL NE SE TAPE PAS, IL S'ARME. Le croisillon appartient aux
  // charcommandes de rAthena — « #pseudo @commande » —, et ce serveur-ci le
  // leur donne (`charcommand_symbol: #`) ; c'est même la raison pour laquelle
  // l'alliance est sur « ^ », le correctif de Moonlight réécrivant l'immédiat
  // du client pour rendre le croisillon.
  //
  // ⚠ ET LA CASSE SERAIT SILENCIEUSE : la ligne partirait en chuchotement vers
  // un canal nommé d'après le pseudo visé, que `channel_name2channel` ne
  // trouverait pas — au lieu de la parole publique, où `is_atcommand`
  // l'intercepte. Le joueur verrait sa commande ne rien faire, sans un mot.
  const ro::net::ChatIntent dit = ro::net::ReadChatIntent("#Bob @heal");
  CHECK(dit.target == ChatTarget::kPublic);
  CHECK(dit.text == "#Bob @heal");
}

TEST_CASE("Destination : un canal ARMÉ sans nom retombe à voix haute", "[net][chat][channel]") {
  // ⚠ MÊME RAISON QUE LE CHUCHOTEMENT : un mode armé qui ne porte pas sa
  // destination enverrait la ligne dans le vide. Le canal est la seule
  // destination armable qui porte un nom.
  ro::net::ChatArming armed;
  armed.mode = ChatTarget::kChannel;
  CHECK(ro::net::ReadChatIntent("bonsoir", armed).target == ChatTarget::kPublic);

  armed.channel = "global";
  const ro::net::ChatIntent dit = ro::net::ReadChatIntent("bonsoir", armed);
  CHECK(dit.target == ChatTarget::kChannel);
  CHECK(dit.recipient == "global");
}

TEST_CASE("Destination : un sigil tapé l'emporte sur le canal armé", "[net][chat][channel]") {
  // C'est la règle de toute la barre, et elle ne souffre pas d'exception : une
  // ligne qui porte un sigil a déjà dit où elle va.
  ro::net::ChatArming armed;
  armed.mode = ChatTarget::kChannel;
  armed.channel = "global";
  CHECK(ro::net::ReadChatIntent("%on y va", armed).target == ChatTarget::kParty);

  // ⚠ ET LA BOX « PSEUDO » AUSSI, comme pour tous les autres modes : un nom
  // écrit là EST une destination, et il est plus précis qu'un mode armé.
  armed.whisper_to = "Bob";
  CHECK(ro::net::ReadChatIntent("tu es la ?", armed).target == ChatTarget::kWhisper);
}
