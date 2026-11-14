// LE SÉLECTEUR DE DESTINATION DE LA BARRE : ce qu'il montre, ce qu'il arme, où
// la ligne part, dans quelle teinte elle reviendra, et la place qui lui reste.

#include "chat/chat_line_preview.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/net/chat_room.h"
#include "ro/net/chat_target.h"
#include "ro/net/connection.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/chat_limits.h"
#include "ro/proto/manual_packets.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/chat_line.h"
#include "ro/ui/chat_input.h"

#include "chat/chat_item_link.h"
#include "chat/chat_line_color.h"
#include "chat/chat_mvp_link.h"
#include "chat/chat_navi_search_link.h"
#include "support/loopback_server.h"

namespace {

using moonclient::ChatLinePreviewMaker;
using ro::net::ChatTarget;
using ro::sim::ChatKind;
using ro::test::LoopbackServer;

constexpr std::string_view kSpeaker = "Moi";
// Le même, vu par la mesure : sans canal connu, sans groupe de staff.
constexpr moonclient::ChatWireSpeaker kMe{.name = kSpeaker};
// Ce que « Moi : » prend devant le texte d'une ligne diffusée.
constexpr std::size_t kNamedBytes = kSpeaker.size() + ro::proto::ChatRequest::kNameSeparator.size();

// L'opcode d'une trame émise, petit-boutiste comme tout le fil.
constexpr int kByteBits = 8;
[[nodiscard]] std::uint16_t OpcodeOf(const std::vector<std::uint8_t>& frame) {
  return static_cast<std::uint16_t>(frame[0] | (frame[1] << kByteBits));
}

// Une liaison dont on capte chaque trame émise.
struct Wire {
  LoopbackServer server;
  ro::net::Connection link;
  std::vector<std::vector<std::uint8_t>> sent;

  Wire() {
    REQUIRE(server.listening());
    auto opened = ro::net::Connection::Open("127.0.0.1", server.port());
    REQUIRE(opened.has_value());
    REQUIRE(server.Accept());
    link = std::move(*opened);
    link.set_send_observer([this](std::span<const std::uint8_t> frame) {
      sent.emplace_back(frame.begin(), frame.end());
    });
  }
};

// Ce que la boucle fait d'un rang rendu par le sélecteur : la liste de l'image
// en cours traduit, et le nom d'un canal se recopie.
void Arm(const ChatLinePreviewMaker& maker, int rank, ro::net::ChatArming* armed,
         std::string* channel) {
  const auto& chosen = maker.choices()[static_cast<std::size_t>(rank)];
  armed->mode = chosen.target;
  *channel = chosen.channel;
  armed->channel = *channel;
}

[[nodiscard]] int RankOf(const ChatLinePreviewMaker& maker, std::string_view label) {
  const auto modes = maker.preview().modes;
  for (std::size_t rank = 0; rank < modes.size(); ++rank) {
    if (modes[rank].label == label) {
      return static_cast<int>(rank);
    }
  }
  return -1;
}

}  // namespace

TEST_CASE("Sélecteur : « Groupe » part au groupe, « Guilde » à la guilde", "[app][chat]") {
  // 🔴 LE DÉFAUT REMONTÉ EN JEU : « choisir Groupe envoie dans la guilde, et
  // inversement ». Cet essai suit le chemin entier — le rang que la liste montre
  // sous ce libellé, l'armement, la lecture de la ligne, la trame émise — et
  // l'envoi est juste. Ce qui trompait, c'était la TEINTE des lignes revenues :
  // voir l'essai suivant.
  ChatLinePreviewMaker maker;
  ro::net::ChatArming armed;
  std::string channel;
  maker.Refresh("", kMe, armed);

  struct Expected {
    const char* label;
    ChatTarget target;
    std::uint16_t opcode;
  };
  const Expected expected[] = {
      {ro::core::Tr("Groupe"), ChatTarget::kParty, ro::proto::PartyChatRequest::kOpcode},
      {ro::core::Tr("Guilde"), ChatTarget::kGuild, ro::proto::GuildChatRequest::kOpcode},
      {ro::core::Tr("Clan"), ChatTarget::kClan, ro::proto::ClanChatRequest::kOpcode},
      {ro::core::Tr("Alentour"), ChatTarget::kPublic, ro::proto::ChatRequest::kOpcode},
  };
  for (const Expected& want : expected) {
    const int rank = RankOf(maker, want.label);
    REQUIRE(rank >= 0);
    Arm(maker, rank, &armed, &channel);
    maker.Refresh("", kMe, armed);
    // L'image suivante montre bien ce qu'on a choisi, et le rang tient.
    CHECK(maker.preview().mode == rank);
    CHECK(maker.preview().target == want.label);

    const ro::net::ChatIntent intent = ro::net::ReadChatIntent("on y va", armed);
    CHECK(intent.target == want.target);
    Wire wire;
    REQUIRE(ro::net::SendChat(wire.link, kSpeaker, intent).has_value());
    REQUIRE(wire.sent.size() == 1);
    CHECK(OpcodeOf(wire.sent[0]) == want.opcode);
  }
}

TEST_CASE("Sélecteur : chaque entrée prend la teinte des lignes de son genre", "[app][chat]") {
  // UNE SEULE TABLE pour le fil et pour la liste : la phrase de groupe revient
  // dans la couleur que l'entrée « Groupe » annonçait.
  ChatLinePreviewMaker maker;
  maker.Refresh("", kMe, ro::net::ChatArming{});
  const auto modes = maker.preview().modes;
  const auto color_of = [&](const char* label) {
    const int rank = RankOf(maker, label);
    REQUIRE(rank >= 0);
    return modes[static_cast<std::size_t>(rank)].color;
  };
  CHECK(color_of(ro::core::Tr("Groupe")) == moonclient::ColorOfChatKind(ChatKind::kParty));
  CHECK(color_of(ro::core::Tr("Guilde")) == moonclient::ColorOfChatKind(ChatKind::kGuild));
  CHECK(color_of(ro::core::Tr("Clan")) == moonclient::ColorOfChatKind(ChatKind::kClan));
  // « Alentour » garde le texte ordinaire : le teinter ferait croire à un mode.
  CHECK(color_of(ro::core::Tr("Alentour")) == 0);

  // ⚠ ET LES DEUX TEINTES QUI SE CONFONDAIENT ne se confondent plus : rose pour
  // le groupe, vert pour la guilde — vert est la guilde pour qui a joué.
  constexpr ImU32 kRose = IM_COL32(255, 200, 200, 255);
  constexpr ImU32 kGreen = IM_COL32(180, 255, 180, 255);
  CHECK(moonclient::ColorOfChatKind(ChatKind::kParty) == kRose);
  CHECK(moonclient::ColorOfChatKind(ChatKind::kGuild) == kGreen);
}

TEST_CASE("Sélecteur : un canal prend la couleur que le serveur lui donne", "[app][chat]") {
  constexpr std::uint32_t kGlobalRgb = 0x33AAFF;
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .color = kGlobalRgb, .can_write = true},
      {.name = "ally", .alias = "[Ally]", .color = 0, .can_write = true}};
  ChatLinePreviewMaker maker;
  maker.Refresh("", moonclient::ChatWireSpeaker{.name = kSpeaker, .channels = channels},
                ro::net::ChatArming{});
  const auto modes = maker.preview().modes;
  const int global = RankOf(maker, "[Global]");
  REQUIRE(global >= 0);
  CHECK(modes[static_cast<std::size_t>(global)].color ==
        moonclient::ColorOfChatLine(ChatKind::kChannel, kGlobalRgb));
  // Sans couleur nommée, la teinte des canaux dans le fil — pas le texte ordinaire.
  const int ally = RankOf(maker, "[Ally]");
  REQUIRE(ally >= 0);
  CHECK(modes[static_cast<std::size_t>(ally)].color ==
        moonclient::ColorOfChatKind(ChatKind::kChannel));
}

TEST_CASE("Sélecteur : le libellé suit le choix avant la première lettre", "[app][chat]") {
  // 🔴 LE SECOND DÉFAUT REMONTÉ : le libellé restait « Alentour » tant que la
  // saisie était vide, et ne suivait le choix qu'une fois la frappe commencée.
  ChatLinePreviewMaker maker;
  maker.Refresh("", kMe, ro::net::ChatArming{.mode = ChatTarget::kGuild});
  CHECK(maker.preview().target == std::string_view(ro::core::Tr("Guilde")));
  // Une ligne vide ne part nulle part pour autant.
  CHECK(ro::net::ReadChatIntent("", ro::net::ChatArming{.mode = ChatTarget::kGuild}).text.empty());
  // Et un sigil tapé l'emporte toujours, le temps de sa ligne.
  maker.Refresh("%salut", kMe, ro::net::ChatArming{.mode = ChatTarget::kGuild});
  CHECK(maker.preview().target == std::string_view(ro::core::Tr("Groupe")));
}

TEST_CASE("Sélecteur : dans un salon, « Alentour » devient « Salon »", "[app][chat]") {
  // Le serveur remet la parole à voix haute aux seuls occupants du salon.
  ChatLinePreviewMaker maker;
  maker.Refresh("", kMe, ro::net::ChatArming{}, /*in_chat_room=*/true);
  CHECK(maker.preview().target == std::string_view(ro::core::Tr("Salon")));
  const int room = RankOf(maker, ro::core::Tr("Salon"));
  REQUIRE(room == 0);
  CHECK(RankOf(maker, ro::core::Tr("Alentour")) < 0);
  // Sa teinte est celle de son propre écho dans la fenêtre du salon.
  CHECK(maker.preview().modes[0].color ==
        moonclient::ColorOfChatLine(ChatKind::kSelf, ro::net::kChatRoomOwnSpeechColor));

  // Hors du salon, le mot revient.
  maker.Refresh("", kMe, ro::net::ChatArming{});
  CHECK(maker.preview().target == std::string_view(ro::core::Tr("Alentour")));
}

TEST_CASE("Compteur : un lien de MVP pèse sa balise, pas son libellé", "[app][chat]") {
  const moonclient::MvpLink link{.monster_id = 1039,
                                 .map = "prt_maze03",
                                 .kill_time_s = 1'790'000'000,
                                 .delay1_min = 120,
                                 .delay2_min = 10,
                                 .tomb_x = 101,
                                 .tomb_y = 47,
                                 .name = "Baphomet"};
  const std::string label = moonclient::MvpLinkLabel(link);
  const std::string wire = moonclient::BuildMvpLink(link);
  REQUIRE_FALSE(wire.empty());
  const std::vector<moonclient::PendingChatLink> pending{moonclient::PendingChatLink{
      .shown = label, .wire = wire, .kind = ro::ui::ChatLinkKind::kMvp, .item_id = 1039}};
  ChatLinePreviewMaker maker;
  maker.Refresh("vu ", kMe, ro::net::ChatArming{});
  const int without_link = maker.preview().bytes_left;

  // LA LIGNE QUI PART : le libellé y a repris sa balise.
  const std::string outgoing = moonclient::ResolveChatLinks("vu " + label, pending);
  CHECK(outgoing == "vu " + wire);
  maker.Refresh(outgoing.c_str(), kMe, ro::net::ChatArming{});
  const auto wire_bytes = static_cast<int>(ro::core::WireFromUtf8Text(wire).size());
  CHECK(maker.preview().bytes_left == without_link - wire_bytes);
  // LE LIBELLÉ N'A PAS LA TAILLE DE SA BALISE : le compter mentirait.
  const auto label_bytes = static_cast<int>(ro::core::WireFromUtf8Text(label).size());
  REQUIRE(label_bytes != wire_bytes);
  maker.Refresh(("vu " + label).c_str(), kMe, ro::net::ChatArming{});
  CHECK(maker.preview().bytes_left == without_link - label_bytes);

  // DEUX CENTS OCTETS ET UNE BALISE dépassent la ligne du serveur.
  const std::string long_text(200, 'a');
  const std::string long_line = moonclient::ResolveChatLinks(long_text + label, pending);
  maker.Refresh(long_line.c_str(), kMe, ro::net::ChatArming{});
  CHECK(maker.preview().budget_known);
  CHECK(maker.preview().bytes_left < 0);
}

TEST_CASE("Compteur : une recherche posée pèse sa balise, et retenue elle revient", "[app][chat]") {
  const moonclient::NaviSearchLink search{
      .family = moonclient::NaviSearchFamily::kNpc, .map = "prontera", .term = "Kafra"};
  const std::string tag = moonclient::BuildNaviSearchLink(search);
  REQUIRE_FALSE(tag.empty());
  const std::string label = moonclient::NaviSearchLinkLabel(search, nullptr);
  std::vector<moonclient::PendingChatLink> links = {
      {.shown = label, .wire = tag, .kind = ro::ui::ChatLinkKind::kNaviSearch}};
  const ro::net::ChatArming armed{};

  // LE COMPTEUR MESURE LA BALISE <NAVS>, pas le libellé que la saisie montre.
  const std::string typed = "ici " + label;
  const std::string outgoing = moonclient::ResolveChatLinks(typed, links);
  REQUIRE(outgoing == "ici " + tag);
  ChatLinePreviewMaker maker;
  maker.Refresh(outgoing.c_str(), kMe, armed);
  CHECK(maker.preview().bytes_left == moonclient::ChatWireBytesLeft(outgoing, kMe, armed));
  CHECK(maker.preview().bytes_left != moonclient::ChatWireBytesLeft(typed, kMe, armed));

  // UNE LIGNE QUI NE TIENT QU'AVANT RÉSOLUTION est retenue, et son lien survit.
  const int typed_left = moonclient::ChatWireBytesLeft(typed, kMe, armed);
  REQUIRE(typed_left > 0);
  const std::string padded = std::string(static_cast<std::size_t>(typed_left), 'a') + typed;
  const moonclient::ChatWireRoom room =
      moonclient::ChatWireRoomOf(moonclient::ResolveChatLinks(padded, links), kMe, armed);
  REQUIRE(room.bytes_left < 0);
  ro::ui::ChatInputState input;
  REQUIRE(moonclient::HoldIfOverlong(input, padded, room, true, &links).has_value());
  CHECK(input.pending_insert == padded);
  moonclient::PruneChatLinks(std::string(input.text.data()) + input.pending_insert, &links);
  CHECK(links.size() == 1);
}

TEST_CASE("Compteur : la ligne qui part se mesure balises comprises", "[app][chat]") {
  // UN RESPAWN POSÉ : son libellé dans la saisie, sa balise sur le fil.
  const moonclient::MvpLink respawn{.monster_id = 1039,
                                    .map = "prt_maze03",
                                    .kill_time_s = 1'790'000'000,
                                    .delay1_min = 120,
                                    .delay2_min = 10,
                                    .tomb_x = 101,
                                    .tomb_y = 47,
                                    .name = "Baphomet"};
  const std::string tag = moonclient::BuildMvpLink(respawn);
  const std::string label = moonclient::MvpLinkLabel(respawn);
  const std::vector<moonclient::PendingChatLink> pending = {
      {.shown = label, .wire = tag, .kind = ro::ui::ChatLinkKind::kMvp}};
  const ro::net::ChatArming armed{};
  const std::string typed = "vu " + label;
  const std::string outgoing = moonclient::ResolveChatLinks(typed, pending);
  REQUIRE(outgoing == "vu " + tag);
  // LA BORNE DE « Nom : texte », moins le nom et son séparateur, moins le texte
  // qui part.
  const int left = moonclient::ChatWireBytesLeft(outgoing, kMe, armed);
  CHECK(left ==
        moonclient::kWireNamedBodyMax -
            static_cast<int>(kSpeaker.size() + ro::proto::ChatRequest::kNameSeparator.size() +
                             outgoing.size()));
  // Le compteur dit la même chose de la même ligne.
  ChatLinePreviewMaker maker;
  maker.Refresh(outgoing.c_str(), kMe, armed);
  CHECK(maker.preview().bytes_left == left);

  // UNE PHRASE QUI REMPLIT LA SAISIE TAPÉE se mesure, une fois partie, à la
  // taille de sa balise et non à celle de son libellé.
  const int typed_left = moonclient::ChatWireBytesLeft(typed, kMe, armed);
  REQUIRE(typed_left > 0);
  const std::string padded = std::string(static_cast<std::size_t>(typed_left), 'a') + typed;
  CHECK(moonclient::ChatWireBytesLeft(padded, kMe, armed) == 0);
  CHECK(moonclient::ChatWireBytesLeft(moonclient::ResolveChatLinks(padded, pending), kMe, armed) ==
        left - typed_left);

  // 🔴 ET ELLE PEUT DÉBORDER UNE FOIS PARTIE quand la balise pèse plus que son
  // libellé, comme celle d'un objet.
  const std::string item_label = moonclient::ChatLinkLabel("Pomme");
  const std::vector<moonclient::PendingChatLink> item = {
      {.shown = item_label, .wire = "<ITEML>000001</ITEML>", .kind = ro::ui::ChatLinkKind::kItem}};
  const std::string item_typed = "vu " + item_label;
  const int item_left = moonclient::ChatWireBytesLeft(item_typed, kMe, armed);
  REQUIRE(item_left > 0);
  const std::string item_padded =
      std::string(static_cast<std::size_t>(item_left), 'a') + item_typed;
  CHECK(moonclient::ChatWireBytesLeft(item_padded, kMe, armed) == 0);
  CHECK(moonclient::ChatWireBytesLeft(moonclient::ResolveChatLinks(item_padded, item), kMe, armed) <
        0);
}

TEST_CASE("Compteur : « reste 0 » part et tient, « 1 de trop » est retenu", "[app][chat]") {
  const ro::net::ChatArming armed{};
  // LES BORNES DU SERVEUR, en chiffres : le relais public garde 255 octets de
  // « Nom : texte », celui de la guilde 251, celui d'un canal 243 de ligne.
  CHECK(moonclient::kWireTextMax == 253);
  CHECK(moonclient::kWireNamedBodyMax == 255);
  CHECK(moonclient::kWireGuildBodyMax == 251);
  CHECK(moonclient::kWireChannelLineMax == 243);

  // UNE DESTINATION NOMMÉE : c'est « Nom : texte » qui borne.
  const int named_max =
      moonclient::kWireNamedBodyMax -
      static_cast<int>(kSpeaker.size() + ro::proto::ChatRequest::kNameSeparator.size());
  const std::string said(static_cast<std::size_t>(named_max), 'a');
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(said, kMe, armed);
  CHECK(room.bytes_left == 0);
  CHECK(room.text_max == named_max);
  CHECK(room.overflow == moonclient::ChatOverflow::kTruncated);
  CHECK(Encode(ro::proto::ChatRequest{.speaker = std::string(kSpeaker), .text = said}).has_value());
  const std::string said_over = said + 'a';
  CHECK(moonclient::ChatWireBytesLeft(said_over, kMe, armed) == -1);
  CHECK_FALSE(Encode(ro::proto::ChatRequest{.speaker = std::string(kSpeaker), .text = said_over})
                  .has_value());

  // 🔴 UN CHUCHOTEMENT NE PORTE PAS LE NOM, mais le serveur compte le zéro et un
  // octet de plus : 254 octets de texte se jettent.
  const std::string to_bob = std::string(ro::net::kWhisperCommand) + "Bob ";
  const std::string whispered(static_cast<std::size_t>(moonclient::kWireTextMax), 'a');
  const moonclient::ChatWireRoom whisper_room =
      moonclient::ChatWireRoomOf(to_bob + whispered, kMe, armed);
  CHECK(whisper_room.bytes_left == 0);
  CHECK(whisper_room.text_max == moonclient::kWireTextMax);
  CHECK(whisper_room.overflow == moonclient::ChatOverflow::kDropped);
  CHECK(Encode(ro::proto::WhisperRequest{.recipient = "Bob", .text = whispered}).has_value());
  const std::string whispered_over = whispered + 'a';
  CHECK(moonclient::ChatWireBytesLeft(to_bob + whispered_over, kMe, armed) == -1);
  CHECK_FALSE(
      Encode(ro::proto::WhisperRequest{.recipient = "Bob", .text = whispered_over}).has_value());
}

TEST_CASE("Compteur : 255 octets de « Nom : texte » passent à voix haute et au groupe",
          "[app][chat]") {
  // LE RELAIS PUBLIC ET CELUI DU GROUPE gardent la ligne entière jusqu'à 255
  // octets, zéro non compris : la 255e ne se retient pas.
  const std::string prefix =
      std::string(kSpeaker) + std::string(ro::proto::ChatRequest::kNameSeparator);
  const std::string text(255 - prefix.size(), 'a');
  CHECK(moonclient::ChatWireBytesLeft(text, kMe, ro::net::ChatArming{}) == 0);
  CHECK(Encode(ro::proto::ChatRequest{.speaker = std::string(kSpeaker), .text = text}).has_value());
  CHECK(Encode(ro::proto::PartyChatRequest{.speaker = std::string(kSpeaker), .text = text})
            .has_value());
  CHECK(moonclient::ChatWireBytesLeft(std::string(1, ro::net::kPartySigil) + text, kMe,
                                      ro::net::ChatArming{}) == 0);
  // La 256e, si.
  const std::string over = text + 'a';
  CHECK(moonclient::ChatWireBytesLeft(over, kMe, ro::net::ChatArming{}) == -1);
  CHECK_FALSE(Encode(ro::proto::PartyChatRequest{.speaker = std::string(kSpeaker), .text = over})
                  .has_value());
}

TEST_CASE("Compteur : la guilde se borne à 251 octets de « Nom : texte »", "[app][chat]") {
  // 🔴 SON RELAIS PREND AUSSI L'EN-TÊTE DE SON PAQUET sur le tampon : une ligne
  // de 252 octets y arriverait coupée, quand la parole publique la garderait.
  const ro::net::ChatArming armed{};
  const std::string guild(1, ro::net::kGuildSigil);
  const int guild_max =
      251 - static_cast<int>(kSpeaker.size() + ro::proto::ChatRequest::kNameSeparator.size());
  const std::string text(static_cast<std::size_t>(guild_max), 'a');
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(guild + text, kMe, armed);
  CHECK(room.bytes_left == 0);
  CHECK(room.text_max == guild_max);
  CHECK(room.overflow == moonclient::ChatOverflow::kTruncated);
  CHECK(Encode(ro::proto::GuildChatRequest{.speaker = std::string(kSpeaker), .text = text})
            .has_value());

  const std::string over = text + 'a';
  const moonclient::ChatWireRoom over_room = moonclient::ChatWireRoomOf(guild + over, kMe, armed);
  CHECK(over_room.bytes_left == -1);
  CHECK(over_room.overflow == moonclient::ChatOverflow::kTruncated);
  CHECK_FALSE(Encode(ro::proto::GuildChatRequest{.speaker = std::string(kSpeaker), .text = over})
                  .has_value());
  // La même ligne passe à voix haute : la borne est bien celle de la guilde.
  CHECK(moonclient::ChatWireBytesLeft(over, kMe, armed) > 0);
}

TEST_CASE("Compteur : un canal se mesure à la ligne que son relais diffuse", "[app][chat]") {
  // « [Global] Moi : texte » : alias, espace, nom, séparateur, texte — sous 243.
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .can_write = true},
      {.name = "ally", .alias = "[Ally]", .can_write = true}};
  const moonclient::ChatWireSpeaker me{.name = kSpeaker, .channels = channels};
  const auto max_under = [](std::string_view alias) {
    return 243 - static_cast<int>(alias.size() + 1 + kSpeaker.size() +
                                  ro::proto::ChatRequest::kNameSeparator.size());
  };

  const ro::net::ChatArming on_global{.mode = ChatTarget::kChannel, .channel = "global"};
  const std::string text(static_cast<std::size_t>(max_under("[Global]")), 'a');
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(text, me, on_global);
  CHECK(room.bytes_left == 0);
  CHECK(room.text_max == max_under("[Global]"));
  CHECK(room.overflow == moonclient::ChatOverflow::kTruncated);
  CHECK(moonclient::ChatWireBytesLeft(text + 'a', me, on_global) == -1);
  // « /w #global » y mène aussi, et se mesure de même.
  CHECK(moonclient::ChatWireBytesLeft(std::string(ro::net::kWhisperCommand) + "#global " + text, me,
                                      ro::net::ChatArming{}) == 0);
  // Le compteur de la saisie dit la même chose.
  ChatLinePreviewMaker maker;
  maker.Refresh((text + 'a').c_str(), me, on_global);
  CHECK(maker.preview().bytes_left == -1);

  // LES ALLIÉS SONT UN CANAL : « ^ » se mesure sous l'alias de `#ally`.
  const std::string ally(1, ro::net::kAllySigil);
  const std::string ally_text(static_cast<std::size_t>(max_under("[Ally]")), 'a');
  CHECK(moonclient::ChatWireBytesLeft(ally + ally_text, me, ro::net::ChatArming{}) == 0);
  CHECK(moonclient::ChatWireBytesLeft(ally + ally_text + 'a', me, ro::net::ChatArming{}) == -1);

  // ⚠ UN CANAL DONT L'ALIAS EST INCONNU compte le plus long que le serveur range.
  const std::string unknown_alias(19, 'x');
  const std::string unknown_text(static_cast<std::size_t>(max_under(unknown_alias)), 'a');
  CHECK(moonclient::ChatWireBytesLeft(unknown_text, kMe, on_global) == 0);
}

TEST_CASE("Compteur : le clan ne garde que le rejet du texte", "[app][chat]") {
  // SON RELAIS NE COUPE PAS À CETTE TAILLE : 253 octets de texte partent, même
  // sous un nom long, et le 254e se jetterait.
  const std::string long_name(23, 'N');
  const moonclient::ChatWireSpeaker named{.name = long_name};
  const ro::net::ChatArming on_clan{.mode = ChatTarget::kClan};
  const std::string text(static_cast<std::size_t>(moonclient::kWireTextMax), 'a');
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(text, named, on_clan);
  CHECK(room.bytes_left == 0);
  CHECK(room.overflow == moonclient::ChatOverflow::kDropped);
  CHECK(Encode(ro::proto::ClanChatRequest{.speaker = long_name, .text = text}).has_value());
  const moonclient::ChatWireRoom over = moonclient::ChatWireRoomOf(text + 'a', named, on_clan);
  CHECK(over.bytes_left == -1);
  CHECK(over.overflow == moonclient::ChatOverflow::kDropped);
  CHECK_FALSE(
      Encode(ro::proto::ClanChatRequest{.speaker = long_name, .text = text + 'a'}).has_value());
}

TEST_CASE("Compteur : le motif dit ce qui arriverait vraiment", "[app][chat]") {
  // 🔴 UN TEXTE QUI PASSE LE REJET mais pas la diffusion arrive coupé ; un
  // texte qui dépasse le rejet est jeté entier — même à une destination nommée,
  // où la diffusion borne plus tôt.
  const ro::net::ChatArming armed{};
  const moonclient::ChatWireRoom cut =
      moonclient::ChatWireRoomOf(std::string(250, 'a'), kMe, armed);
  CHECK(cut.bytes_left < 0);
  CHECK(cut.overflow == moonclient::ChatOverflow::kTruncated);
  const moonclient::ChatWireRoom dropped =
      moonclient::ChatWireRoomOf(std::string(254, 'a'), kMe, armed);
  CHECK(dropped.bytes_left < 0);
  CHECK(dropped.overflow == moonclient::ChatOverflow::kDropped);
}

TEST_CASE("Compteur : une commande garde la borne de la parole qui la porte", "[app][chat]") {
  // 🔴 LE SERVEUR LA DIFFUSE quand le groupe n'y a pas droit, sur une carte
  // `nocommand` ou face à un PNJ : la compter comme exécutée laisserait couper.
  const ro::net::ChatArming armed{};
  const int named_max = moonclient::kWireNamedBodyMax - static_cast<int>(kNamedBytes);
  const std::string command = "@" + std::string(static_cast<std::size_t>(named_max - 1), 'a');
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(command, kMe, armed);
  CHECK(room.bytes_left == 0);
  CHECK(room.overflow == moonclient::ChatOverflow::kTruncated);
  CHECK(moonclient::ChatWireBytesLeft(command + 'a', kMe, armed) == -1);
  CHECK(moonclient::ChatWireBytesLeft("#" + command.substr(1) + 'a', kMe, armed) == -1);
  // L'encodeur tient la même borne, commande du client comprise.
  Wire wire;
  REQUIRE(ro::net::SendServerCommand(wire.link, kSpeaker, command).has_value());
  REQUIRE(wire.sent.size() == 1);
  CHECK(OpcodeOf(wire.sent[0]) == ro::proto::ChatRequest::kOpcode);
  CHECK_FALSE(ro::net::SendServerCommand(wire.link, kSpeaker, command + 'a').has_value());
  // EN GUILDE, sous la borne de la guilde, comme toute parole de guilde.
  const int guild_max = moonclient::kWireGuildBodyMax - static_cast<int>(kNamedBytes);
  const std::string guild_command = std::string(1, ro::net::kGuildSigil) + "@" +
                                    std::string(static_cast<std::size_t>(guild_max - 1), 'a');
  CHECK(moonclient::ChatWireBytesLeft(guild_command, kMe, armed) == 0);
  CHECK(moonclient::ChatWireBytesLeft(guild_command + 'a', kMe, armed) == -1);
}

TEST_CASE("Compteur : un faux nom possible se compte au plus long", "[app][chat]") {
  // UN MEMBRE DU STAFF PEUT PORTER UN FAUX NOM que le client ne voit pas, et les
  // relais de la parole le diffusent à la place du sien : 23 octets au plus.
  const ro::net::ChatArming armed{};
  const moonclient::ChatWireSpeaker staff{.name = kSpeaker, .may_wear_fake_name = true};
  const int fake_named = static_cast<int>(ro::proto::kChatShownNameMax +
                                          ro::proto::ChatRequest::kNameSeparator.size());
  const std::string spoken(static_cast<std::size_t>(moonclient::kWireNamedBodyMax - fake_named),
                           'a');
  CHECK(moonclient::ChatWireBytesLeft(spoken, staff, armed) == 0);
  CHECK(moonclient::ChatWireBytesLeft(spoken + 'a', staff, armed) == -1);
  CHECK(moonclient::ChatWireBytesLeft(std::string(1, ro::net::kPartySigil) + spoken + 'a', staff,
                                      armed) == -1);
  // Sans groupe, le sien.
  CHECK(moonclient::ChatWireBytesLeft(spoken + 'a', kMe, armed) > 0);
  // ⚠ UN CANAL GARDE LE VRAI NOM : le faux n'y change rien.
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .can_write = true}};
  const ro::net::ChatArming on_global{.mode = ChatTarget::kChannel, .channel = "global"};
  const moonclient::ChatWireSpeaker staff_in{
      .name = kSpeaker, .channels = channels, .may_wear_fake_name = true};
  const moonclient::ChatWireSpeaker me_in{.name = kSpeaker, .channels = channels};
  CHECK(moonclient::ChatWireBytesLeft("a", staff_in, on_global) ==
        moonclient::ChatWireBytesLeft("a", me_in, on_global));
}

TEST_CASE("Compteur : la guilde en champ de bataille garde 255 octets", "[app][chat]") {
  // SON RELAIS EST CELUI DU CHAMP, qui garde la ligne entière : 252 à 255 octets
  // de « Nom : texte » passent sur une carte de champ de bataille, pas ailleurs.
  const ro::net::ChatArming on_guild{.mode = ChatTarget::kGuild};
  const moonclient::ChatWireSpeaker fighting{.name = kSpeaker, .battlefield = true};
  for (const std::size_t body : {std::size_t{252}, std::size_t{254}}) {
    const std::string text(body - kNamedBytes, 'a');
    CHECK(moonclient::ChatWireBytesLeft(text, fighting, on_guild) ==
          moonclient::kWireNamedBodyMax - static_cast<int>(body));
    CHECK(Encode(ro::proto::GuildChatRequest{
                     .speaker = std::string(kSpeaker), .text = text, .battlefield = true})
              .has_value());
    CHECK(moonclient::ChatWireBytesLeft(text, kMe, on_guild) < 0);
    CHECK_FALSE(Encode(ro::proto::GuildChatRequest{.speaker = std::string(kSpeaker), .text = text})
                    .has_value());
  }
  // La 256e, même là.
  const std::string over(256 - kNamedBytes, 'a');
  CHECK(moonclient::ChatWireBytesLeft(over, fighting, on_guild) == -1);
  CHECK_FALSE(Encode(ro::proto::GuildChatRequest{
                         .speaker = std::string(kSpeaker), .text = over, .battlefield = true})
                  .has_value());
  // L'envoi pose le champ de bataille sur la requête.
  const std::string full(255 - kNamedBytes, 'a');
  const ro::net::ChatIntent intent{.target = ChatTarget::kGuild, .text = full};
  Wire wire;
  CHECK(ro::net::SendChat(wire.link, kSpeaker, intent, /*on_battlefield=*/true).has_value());
  CHECK_FALSE(ro::net::SendChat(wire.link, kSpeaker, intent).has_value());
}

TEST_CASE("Compteur : la parole liée à un canal se mesure à son relais", "[app][chat]") {
  // « @channel bindto #nom » : la parole à voix haute part au relais du canal,
  // salon compris, sous « alias nom : texte ».
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .can_write = true},
      {.name = "annonce", .alias = "[Annonce]", .can_write = false}};
  ChatLinePreviewMaker maker;
  maker.FollowSentCommand("@Channel BINDTO #Global", channels);
  REQUIRE(maker.bound_channel() == "global");
  const moonclient::ChatWireSpeaker bound{
      .name = kSpeaker, .channels = channels, .bound_channel = maker.bound_channel()};
  const int channel_max = moonclient::kWireChannelLineMax -
                          static_cast<int>(std::string_view("[Global]").size() + 1 + kNamedBytes);
  const std::string text(static_cast<std::size_t>(channel_max), 'a');
  CHECK(moonclient::ChatWireBytesLeft(text, bound, ro::net::ChatArming{}) == 0);
  CHECK(moonclient::ChatWireBytesLeft(text + 'a', bound, ro::net::ChatArming{}) == -1);
  CHECK(moonclient::ChatWireBytesLeft(text + 'a', kMe, ro::net::ChatArming{}) > 0);
  // Le libellé suit, salon compris.
  maker.Refresh(text.c_str(), bound, ro::net::ChatArming{}, /*in_chat_room=*/true);
  CHECK(maker.preview().target == std::string_view("[Global]"));
  CHECK(maker.preview().modes[0].label == std::string_view("[Global]"));
  CHECK(maker.preview().bytes_left == 0);

  // « unbind » le lève ; l'alias de la commande se compare à l'identique.
  maker.FollowSentCommand("@channel unbind", channels);
  CHECK(maker.bound_channel().empty());
  maker.FollowSentCommand("@MAIN bindto #global", channels);
  CHECK(maker.bound_channel().empty());
  maker.FollowSentCommand("@main bindto global", channels);
  CHECK(maker.bound_channel().empty());
  maker.FollowSentCommand("@main bindto #global", channels);
  CHECK(maker.bound_channel() == "global");

  // ⚠ UN CANAL OÙ L'ON NE PEUT PAS ÉCRIRE ne prend pas la parole : le libellé
  // reste, la mesure garde la plus serrée.
  maker.FollowSentCommand("@channel bindto #annonce", channels);
  const moonclient::ChatWireSpeaker mute{
      .name = kSpeaker, .channels = channels, .bound_channel = maker.bound_channel()};
  maker.Refresh("", mute, ro::net::ChatArming{});
  CHECK(maker.preview().target == std::string_view(ro::core::Tr("Alentour")));
}

TEST_CASE("Compteur : un canal se retrouve sans tenir compte de la casse", "[app][chat]") {
  // Le serveur range ses canaux sans casse : « /w #GLOBAL » compte l'alias de
  // « #global », et non le plus long possible.
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .can_write = true}};
  const moonclient::ChatWireSpeaker me{.name = kSpeaker, .channels = channels};
  const std::string to_global = std::string(ro::net::kWhisperCommand) + "#global a";
  const std::string to_upper = std::string(ro::net::kWhisperCommand) + "#GLOBAL a";
  CHECK(moonclient::ChatWireBytesLeft(to_upper, me, ro::net::ChatArming{}) ==
        moonclient::ChatWireBytesLeft(to_global, me, ro::net::ChatArming{}));
}

TEST_CASE("Compteur : nom et texte se comptent en octets du fil", "[app][chat]") {
  // 🔴 UN ACCENT PÈSE UN OCTET EN 1252, deux en UTF-8 : le compte suit la page
  // de codes qui part, pour le nom comme pour le texte.
  const std::string name = "\xC3\x89lo\xC3\xAFse";  // « Éloïse », 6 octets en 1252.
  const std::string accent = "\xC3\xA9";            // « é »
  const auto repeat = [](const std::string& piece, std::size_t times) {
    std::string out;
    for (std::size_t rank = 0; rank < times; ++rank) {
      out += piece;
    }
    return out;
  };
  const moonclient::ChatWireSpeaker eloise{.name = name};
  const ro::net::ChatArming armed{};
  const std::string wire_name = ro::core::Cp1252FromUtf8(name);
  REQUIRE(wire_name.size() == 6);
  const auto encodes = [&](const std::string& text) {
    return Encode(ro::proto::ChatRequest{.speaker = wire_name,
                                         .text = ro::core::WireFromUtf8Text(text)})
        .has_value();
  };
  const std::string said = repeat(accent, 240);
  CHECK(moonclient::ChatWireBytesLeft(said, eloise, armed) == 255 - 9 - 240);
  CHECK(encodes(said + repeat(accent, 6)));
  CHECK(moonclient::ChatWireBytesLeft(said + repeat(accent, 7), eloise, armed) == -1);
  CHECK_FALSE(encodes(said + repeat(accent, 7)));

  // UN EMOJI FAIT BASCULER LA PHRASE EN UTF-8 : chaque accent y pèse deux octets.
  const std::string emoji = "\xF0\x9F\x99\x82";
  const std::string mixed = repeat(accent, 100) + emoji;
  const int mixed_left = 255 - 9 - (100 * 2 + 4);
  CHECK(moonclient::ChatWireBytesLeft(mixed, eloise, armed) == mixed_left);
  const std::string full = mixed + std::string(static_cast<std::size_t>(mixed_left), 'a');
  CHECK(moonclient::ChatWireBytesLeft(full, eloise, armed) == 0);
  CHECK(encodes(full));
  CHECK_FALSE(encodes(full + 'a'));

  // Et sur un canal, le nom diffusé se compte de même.
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .can_write = true}};
  const moonclient::ChatWireSpeaker in_global{.name = name, .channels = channels};
  const ro::net::ChatArming on_global{.mode = ChatTarget::kChannel, .channel = "global"};
  CHECK(moonclient::ChatWireBytesLeft("a", in_global, on_global) == 243 - (8 + 1 + 6 + 3) - 1);
}

TEST_CASE("Compteur : une annonce se mesure à la commande que le serveur en fait", "[app][chat]") {
  // « @kami » et son espace devant, dans un tampon de 255 octets : 249 restent
  // à l'annonce, 248 à l'annonce de carte (« @lkami »).
  const ro::net::ChatArming armed{};
  const std::string message(249, 'a');
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf("/nb " + message, kMe, armed);
  CHECK(room.bytes_left == 0);
  CHECK(room.text_max == 249);
  CHECK(room.overflow == moonclient::ChatOverflow::kTruncated);
  CHECK(moonclient::ChatWireBytesLeft("/nb " + message + 'a', kMe, armed) == -1);
  CHECK(moonclient::ChatWireBytesLeft("/nlb " + std::string(248, 'a'), kMe, armed) == 0);
  CHECK(moonclient::ChatWireBytesLeft("/nlb " + message, kMe, armed) == -1);

  // `/b` porte le nom, compris dans ce qui part.
  const int named_max =
      249 - static_cast<int>(kSpeaker.size() + ro::proto::ChatRequest::kNameSeparator.size());
  const std::string named(static_cast<std::size_t>(named_max), 'a');
  const moonclient::ChatWireRoom named_room = moonclient::ChatWireRoomOf("/b " + named, kMe, armed);
  CHECK(named_room.bytes_left == 0);
  CHECK(named_room.text_max == named_max);
  CHECK(moonclient::ChatWireBytesLeft("/b " + named + 'a', kMe, armed) == -1);
  // Le compteur de la saisie dit la même chose.
  ChatLinePreviewMaker maker;
  maker.Refresh(("/nb " + message).c_str(), kMe, armed);
  CHECK(maker.preview().bytes_left == 0);
}

TEST_CASE("Sélecteur : « Champ de bataille » paraît sur sa carte, et part en 0x02DB",
          "[app][chat]") {
  const char* label = ro::core::Tr("Champ de bataille");
  ChatLinePreviewMaker maker;
  ro::net::ChatArming armed;
  std::string channel;
  maker.Refresh("", kMe, armed);
  CHECK(RankOf(maker, label) < 0);
  const moonclient::ChatWireSpeaker fighting{.name = kSpeaker, .battlefield = true};
  maker.Refresh("", fighting, armed);
  const int rank = RankOf(maker, label);
  REQUIRE(rank >= 0);
  CHECK(maker.preview().modes[static_cast<std::size_t>(rank)].color ==
        moonclient::ColorOfChatKind(ChatKind::kBattlefield));
  CHECK(moonclient::ColorOfChatKind(ChatKind::kBattlefield) == IM_COL32(255, 0, 155, 255));
  Arm(maker, rank, &armed, &channel);
  maker.Refresh("", fighting, armed);
  CHECK(maker.preview().mode == rank);
  CHECK(maker.preview().target == std::string_view(label));
  // « & » se lit le temps de sa ligne, même sans mode armé ; sa borne est
  // celle de la parole publique.
  const ro::net::ChatArming on_field{.on_battlefield = true};
  maker.Refresh("&on y va", fighting, on_field);
  CHECK(maker.preview().target == std::string_view(label));
  CHECK(moonclient::ChatWireBytesLeft(std::string(255 - kNamedBytes, 'a'), fighting, armed) == 0);
  // Hors de la carte, « & » n'est plus un sigil : la ligne part à voix haute.
  maker.Refresh("&lt;3", kMe, ro::net::ChatArming{});
  CHECK(maker.preview().target != std::string_view(label));
  CHECK(ro::net::ReadChatIntent("&lt;3").target == ChatTarget::kPublic);

  // L'envoi : « Moi : texte » sous l'opcode du champ, comme la parole publique.
  REQUIRE(Encode(ro::proto::BattlefieldChatRequest{.speaker = "Moi", .text = "x"}).has_value());
  Wire wire;
  REQUIRE(ro::net::SendChat(wire.link, kSpeaker, ro::net::ReadChatIntent("on y va", armed))
              .has_value());
  REQUIRE(wire.sent.size() == 1);
  CHECK(OpcodeOf(wire.sent[0]) == ro::proto::BattlefieldChatRequest::kOpcode);
  const std::string body(wire.sent[0].begin() + static_cast<std::ptrdiff_t>(
                                                    ro::proto::BattlefieldChatRequest::kHeaderSize),
                         wire.sent[0].end());
  CHECK(body.starts_with("Moi : on y va"));
}

TEST_CASE("Discord : la marque suit la voix haute de Gonryun, et elle seule", "[app][chat]") {
  const moonclient::ChatWireSpeaker at_gonryun{.name = kSpeaker, .relayed_to_discord = true};
  ChatLinePreviewMaker maker;
  ro::net::ChatArming armed;
  maker.Refresh("", at_gonryun, armed);
  CHECK(maker.preview().relayed);
  CHECK_FALSE(maker.preview().relay_tip.empty());
  // Dans la liste, seule « Alentour » porte la marque.
  const auto modes = maker.preview().modes;
  CHECK(modes[static_cast<std::size_t>(RankOf(maker, ro::core::Tr("Alentour")))].relayed);
  CHECK_FALSE(modes[static_cast<std::size_t>(RankOf(maker, ro::core::Tr("Groupe")))].relayed);
  // Un salon n'y change rien : le serveur relaie aussi ce qui s'y dit.
  maker.Refresh("salut", at_gonryun, armed, /*in_chat_room=*/true);
  CHECK(maker.preview().relayed);

  // 🔴 LA DESTINATION RÉSOLUE DÉCIDE : un sigil, la case « Pseudo », une
  // commande du client et la parole liée à un canal ne partent pas sur Discord.
  maker.Refresh((std::string(1, ro::net::kPartySigil) + "salut").c_str(), at_gonryun, armed);
  CHECK_FALSE(maker.preview().relayed);
  CHECK(maker.preview().relay_tip.empty());
  ro::net::ChatArming boxed;
  boxed.whisper_to = "Bob";
  maker.Refresh("salut", at_gonryun, boxed);
  CHECK_FALSE(maker.preview().relayed);
  maker.Refresh("/where", at_gonryun, armed);
  CHECK_FALSE(maker.preview().relayed);
  // Une commande du serveur s'exécute avant le relai.
  maker.Refresh("@autoloot", at_gonryun, armed);
  CHECK_FALSE(maker.preview().relayed);
  CHECK(maker.preview().relay_tip.empty());
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "global", .alias = "[Global]", .can_write = true}};
  const moonclient::ChatWireSpeaker bound{.name = kSpeaker,
                                          .channels = channels,
                                          .bound_channel = "global",
                                          .relayed_to_discord = true};
  maker.Refresh("salut", bound, armed);
  CHECK_FALSE(maker.preview().relayed);
  // Liée à un canal, la voix haute ne porte pas non plus la marque dans la
  // liste des modes.
  for (const ro::ui::ChatSendMode& mode : maker.preview().modes) {
    CHECK_FALSE(mode.relayed);
  }
  // Le groupe armé, une ligne sans sigil part au groupe : pas de marque.
  const ro::net::ChatArming on_party{.mode = ChatTarget::kParty};
  maker.Refresh("salut", at_gonryun, on_party);
  CHECK_FALSE(maker.preview().relayed);
}

TEST_CASE("Discord : hors de Gonryun, rien ne porte la marque", "[app][chat]") {
  ChatLinePreviewMaker maker;
  maker.Refresh("salut", kMe, ro::net::ChatArming{});
  CHECK_FALSE(maker.preview().relayed);
  CHECK(maker.preview().relay_tip.empty());
  for (const ro::ui::ChatSendMode& mode : maker.preview().modes) {
    CHECK_FALSE(mode.relayed);
  }
}
