// LA LIGNE VALIDÉE : envoyée balises reprises, ou retenue pour sa longueur avec
// ses liens, et ce qu'un lien posé dans la même image en fait.

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/chat_target.h"
#include "ro/ui/chat_input.h"

#include "chat/chat_item_link.h"
#include "chat/chat_line_preview.h"

namespace {

constexpr std::string_view kSpeaker = "Moi";
constexpr moonclient::ChatWireSpeaker kMe{.name = kSpeaker};

}  // namespace

TEST_CASE("Retenue : la ligne retenue passe devant ce qui a été posé dans la même image",
          "[app][chat]") {
  const ro::net::ChatArming armed{};
  const std::string typed = std::string(250, 'a') + moonclient::ChatLinkLabel("Pomme");
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(typed, kMe, armed);
  REQUIRE(room.bytes_left < 0);

  // Un lien et un emoji attendaient déjà dans la saisie : ils suivent la ligne,
  // et leurs liens restent posés, dans l'ordre.
  ro::ui::ChatInputState input;
  const std::string posed = "<Knife>";
  std::ranges::copy(posed, input.text.begin());
  input.text[posed.size()] = '\0';
  input.pending_insert = ":)";
  std::vector<moonclient::PendingChatLink> links = {{.shown = moonclient::ChatLinkLabel("Pomme")},
                                                    {.shown = posed}};
  REQUIRE(moonclient::HoldIfOverlong(input, typed, room, true, &links).has_value());
  CHECK(input.text[0] == '\0');
  CHECK(input.pending_insert == typed + " <Knife>:)");
  CHECK(links.size() == 2);
}

TEST_CASE("Retenue : ce qui ne tient plus derrière la ligne retenue se perd, liens compris",
          "[app][chat]") {
  const ro::net::ChatArming armed{};
  const std::string pomme = moonclient::ChatLinkLabel("Pomme");
  const std::string typed = std::string(250, 'a') + pomme;
  const moonclient::ChatWireRoom room = moonclient::ChatWireRoomOf(typed, kMe, armed);
  REQUIRE(room.bytes_left < 0);

  // Le dépôt déjà plein : la ligne retenue et lui ne tiennent pas ensemble.
  ro::ui::ChatInputState input;
  const std::string knife = "<Knife>";
  input.pending_insert = std::string(ro::ui::kChatInputSize - typed.size() - 1, 'b') + knife;
  std::vector<moonclient::PendingChatLink> links = {{.shown = pomme}, {.shown = knife}};
  REQUIRE(moonclient::HoldIfOverlong(input, typed, room, true, &links).has_value());
  CHECK(input.pending_insert == typed);
  REQUIRE(links.size() == 1);
  CHECK(links[0].shown == pomme);
}

TEST_CASE("Envoi : la ligne validée se règle AVANT qu'un lien se pose dans la même image",
          "[app][chat]") {
  const ro::net::ChatArming armed{};
  const std::string pomme = moonclient::ChatLinkLabel("Pomme");
  const std::string pomme_wire = "<ITEML>000001</ITEML>";
  const std::string knife = "<Knife>";

  SECTION("la ligne part avec sa balise, et seul le lien posé ensuite reste") {
    // La validation a vidé la saisie : la ligne n'est plus que `said`.
    ro::ui::ChatInputState input;
    std::vector<moonclient::PendingChatLink> links = {{.shown = pomme, .wire = pomme_wire}};
    const std::string said = "voici " + pomme;
    const moonclient::SettledChatLine settled =
        moonclient::SettleChatLine(said, input, &links, kMe, armed);
    REQUIRE(settled.outgoing.has_value());
    CHECK(*settled.outgoing == "voici " + pomme_wire);
    CHECK_FALSE(settled.held_notice.has_value());
    CHECK(links.empty());

    REQUIRE(moonclient::PlaceChatLinkLabel(knife, input, &links));
    links.push_back(moonclient::PendingChatLink{.shown = knife});
    CHECK(std::string(input.text.data()) == knife);
    REQUIRE(links.size() == 1);
    CHECK(links[0].shown == knife);
  }

  SECTION("la ligne retenue garde ses liens, et le lien posé ensuite la suit") {
    ro::ui::ChatInputState input;
    std::vector<moonclient::PendingChatLink> links = {{.shown = pomme, .wire = pomme_wire}};
    const std::string said = std::string(250, 'a') + pomme;
    const moonclient::SettledChatLine settled =
        moonclient::SettleChatLine(said, input, &links, kMe, armed);
    CHECK_FALSE(settled.outgoing.has_value());
    REQUIRE(settled.held_notice.has_value());
    CHECK(input.pending_insert == said);
    REQUIRE(links.size() == 1);

    REQUIRE(moonclient::PlaceChatLinkLabel(knife, input, &links));
    links.push_back(moonclient::PendingChatLink{.shown = knife});
    CHECK(input.pending_insert == said + " " + knife);
    moonclient::PruneChatLinks(std::string(input.text.data()) + input.pending_insert, &links);
    REQUIRE(links.size() == 2);
    CHECK(links[0].shown == pomme);
    CHECK(links[1].shown == knife);
  }
}

TEST_CASE("Retenue : la ligne trop longue revient dans la saisie, liens compris", "[app][chat]") {
  const ro::net::ChatArming armed{};
  const std::string label = moonclient::ChatLinkLabel("Pomme");
  std::vector<moonclient::PendingChatLink> links = {
      {.shown = label, .wire = "<ITEML>000001</ITEML>", .kind = ro::ui::ChatLinkKind::kItem}};
  const std::string typed = std::string(250, 'a') + label;
  const moonclient::ChatWireRoom room =
      moonclient::ChatWireRoomOf(moonclient::ResolveChatLinks(typed, links), kMe, armed);
  REQUIRE(room.bytes_left < 0);

  ro::ui::ChatInputState input;
  const std::optional<std::string> held =
      moonclient::HoldIfOverlong(input, typed, room, true, &links);
  REQUIRE(held.has_value());
  CHECK(input.pending_insert == typed);
  CHECK(held->find(std::to_string(-room.bytes_left)) != std::string::npos);
  CHECK(held->find(std::to_string(room.text_max)) != std::string::npos);
  CHECK(*held == moonclient::OverlongChatNotice(room));
  // Les deux motifs se disent différemment.
  CHECK(room.overflow == moonclient::ChatOverflow::kDropped);
  moonclient::ChatWireRoom truncated = room;
  truncated.overflow = moonclient::ChatOverflow::kTruncated;
  CHECK(moonclient::OverlongChatNotice(truncated) != *held);

  // 🔴 LES LIENS SURVIVENT : l'élagage lit la saisie DÉPÔT COMPRIS. Sur la seule
  // saisie, vidée par l'envoi, ils disparaîtraient.
  moonclient::PruneChatLinks(std::string(input.text.data()) + input.pending_insert, &links);
  CHECK(links.size() == 1);
  std::vector<moonclient::PendingChatLink> lost = links;
  moonclient::PruneChatLinks(std::string(input.text.data()), &lost);
  CHECK(lost.empty());

  // Rien ne se retient quand la ligne tient, ni sans personnage.
  ro::ui::ChatInputState untouched;
  CHECK_FALSE(moonclient::HoldIfOverlong(untouched, "court",
                                         moonclient::ChatWireRoomOf("court", kMe, armed), true,
                                         nullptr)
                  .has_value());
  CHECK_FALSE(moonclient::HoldIfOverlong(untouched, typed, room, false, nullptr).has_value());
  CHECK(untouched.pending_insert.empty());
}

TEST_CASE("Retenue : une conversation 1:1 se mesure à sa cible figée", "[app][chat]") {
  // LA CIBLE NE SE RELIT PAS : un « % » en tête reste du texte chuchoté.
  const std::string text = "%" + std::string(252, 'a');
  const ro::net::ChatIntent to_bob{
      .target = ro::net::ChatTarget::kWhisper, .recipient = "Bob", .text = text};
  CHECK(moonclient::ChatIntentRoomOf(to_bob, kMe).bytes_left == 0);
  ro::net::ChatIntent over = to_bob;
  over.text += 'a';
  const moonclient::ChatWireRoom room = moonclient::ChatIntentRoomOf(over, kMe);
  CHECK(room.bytes_left == -1);
  CHECK(room.overflow == moonclient::ChatOverflow::kDropped);
}
