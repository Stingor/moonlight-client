// Les messages par numéro : chaque opcode écrit sa ligne, dans sa couleur, et
// un numéro inconnu n'écrit rien.

#include "ro/net/server_message.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::core::Tr;
using ro::net::ApplyServerMessageFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 4242;
constexpr std::uint32_t kWhite = 0xFFFFFF;
constexpr std::uint32_t kRed = 0xFF0000;
constexpr std::uint32_t kYellow = 0xFFFF00;
constexpr std::uint32_t kLavender = 0xCDCDFF;
// Un cyan pâle, tel que le serveur le range (BGR), puis à l'endroit (RGB).
constexpr std::uint32_t kPaleCyanBgr = 0xFFCC33;
constexpr std::uint32_t kPaleCyanRgb = 0x33CCFF;

constexpr std::uint16_t kOverweight = 52;
constexpr std::uint16_t kOverWeightLimit = 220;
constexpr std::uint16_t kCannotEquip = 372;
constexpr std::uint16_t kCannotPartyCall = 1221;
constexpr std::uint16_t kReuseLimit = 1862;
constexpr std::uint16_t kItemBannedOnMap = 3278;
constexpr std::uint16_t kMvpItem = 143;
constexpr std::uint16_t kSkillSuccess = 1574;
constexpr std::uint16_t kUnknownMessage = 1;
constexpr std::uint16_t kBash = 5;

ro::net::Frame Msg(std::uint16_t message_id) {
  ro::proto::ZcMsg packet;
  packet.msgId = message_id;
  return FrameOf(ro::proto::Encode(packet));
}

ro::net::Frame MsgColor(std::uint16_t message_id, std::uint32_t bgr) {
  ro::proto::ZcMsgColor packet;
  packet.MessageId = message_id;
  packet.MessageColor = bgr;
  return FrameOf(ro::proto::Encode(packet));
}

ro::net::Frame MsgValue(std::uint16_t message_id, std::int32_t value) {
  ro::proto::ZcMsgValue packet;
  packet.message = message_id;
  packet.value = value;
  return FrameOf(ro::proto::Encode(packet));
}

ro::net::Frame MsgSkill(std::uint16_t skill_id, std::int32_t message_id) {
  ro::proto::ZcMsgSkill packet;
  packet.skillId = skill_id;
  packet.msgId = message_id;
  return FrameOf(ro::proto::Encode(packet));
}

// La seule ligne écrite.
const ro::sim::ChatLine& OnlyLine(const ro::sim::World& world) {
  REQUIRE(world.chat_lines().size() == 1);
  return world.chat_lines().front();
}

[[nodiscard]] ro::sim::SkillTree BashTree() {
  return ro::sim::SkillTree::FromTables(
      {ro::sim::SkillNode{.id = kBash, .idname = "SM_BASH", .display_name = "Bash"}}, {});
}

// Le nombre de trous de ce genre dans un texte.
std::size_t Holes(std::string_view text, std::string_view hole) {
  std::size_t holes = 0;
  for (std::size_t at = text.find(hole); at != std::string_view::npos;
       at = text.find(hole, at + 1)) {
    ++holes;
  }
  return holes;
}

}  // namespace

TEST_CASE("Messages du serveur : 0x0291 écrit le texte de la table, en blanc",
          "[net][server_message]") {
  ro::sim::World world;
  CHECK(ApplyServerMessageFrame(Msg(kCannotEquip), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  const ro::sim::ChatLine& line = OnlyLine(world);
  CHECK(line.text == Tr("Vous ne pouvez pas équiper cet item."));
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  CHECK(line.type == ro::sim::ChatType::kPublic);
  CHECK(line.color == kWhite);
  CHECK(line.said_ms == kNowMs);
}

TEST_CASE("Messages du serveur : un refus rouge le reste, par 0x0291 comme par 0x09CD",
          "[net][server_message]") {
  ro::sim::World plain;
  REQUIRE(ApplyServerMessageFrame(Msg(kCannotPartyCall), plain, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(plain).color == kRed);

  ro::sim::World colored;
  REQUIRE(ApplyServerMessageFrame(MsgColor(kCannotPartyCall, kPaleCyanBgr), colored, nullptr,
                                  kNowMs) == FrameOutcome::kApplied);
  CHECK(OnlyLine(colored).color == kRed);
  CHECK(OnlyLine(colored).text ==
        Tr("Le membre du groupe n'a pas été invoqué car vous n'êtes pas le chef du groupe."));
}

TEST_CASE("Messages du serveur : les refus de surcharge restent blancs, et 0x07E2 ne rougit pas",
          "[net][server_message]") {
  for (const std::uint16_t message_id : {kOverweight, kOverWeightLimit}) {
    ro::sim::World world;
    REQUIRE(ApplyServerMessageFrame(Msg(message_id), world, nullptr, kNowMs) ==
            FrameOutcome::kApplied);
    CHECK(OnlyLine(world).color == kWhite);
  }

  ro::sim::World valued;
  REQUIRE(ApplyServerMessageFrame(MsgValue(kCannotPartyCall, 0), valued, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(valued).color == kYellow);
}

TEST_CASE("Messages du serveur : 0x09CD remet la couleur du fil à l'endroit",
          "[net][server_message]") {
  ro::sim::World world;
  REQUIRE(ApplyServerMessageFrame(MsgColor(kCannotEquip, kPaleCyanBgr), world, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  const ro::sim::ChatLine& line = OnlyLine(world);
  CHECK(line.color == kPaleCyanRgb);
  CHECK(line.text == Tr("Vous ne pouvez pas équiper cet item."));
  CHECK(line.kind == ro::sim::ChatKind::kServer);
}

TEST_CASE("Messages du serveur : 0x07E2 remplit le gabarit, en jaune", "[net][server_message]") {
  ro::sim::World world;
  REQUIRE(ApplyServerMessageFrame(MsgValue(kReuseLimit, 5), world, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyServerMessageFrame(MsgValue(kReuseLimit, 99), world, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text == "Encore 5 secondes avant de pouvoir réutiliser l'item.");
  CHECK(world.chat_lines()[1].text == "Encore 99 secondes avant de pouvoir réutiliser l'item.");
  CHECK(world.chat_lines()[0].color == kYellow);
  CHECK(world.chat_lines()[1].kind == ro::sim::ChatKind::kServer);
}

TEST_CASE("Messages du serveur : 0x07E2 nomme l'item que sa valeur désigne, pour le message 3278",
          "[net][server_message]") {
  constexpr std::int32_t kApple = 512;
  const ro::net::ItemNameLookup names = [](std::uint32_t item_id) {
    return item_id == static_cast<std::uint32_t>(kApple) ? std::string("Apple") : std::string{};
  };
  ro::sim::World world;
  REQUIRE(ApplyServerMessageFrame(MsgValue(kItemBannedOnMap, kApple), world, nullptr, kNowMs,
                                  names) == FrameOutcome::kApplied);
  const ro::sim::ChatLine& line = OnlyLine(world);
  CHECK(line.text == "Apple ne peut pas être utilisé sur cette carte.");
  CHECK(line.color == kYellow);
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  CHECK(line.type == ro::sim::ChatType::kPublic);

  // ⚠ UN ITEM INCONNU, OU AUCUNE TABLE, S'ÉCRIT PAR UNE ESPACE, et non par son
  // numéro.
  constexpr std::int32_t kUnknownItem = 7;
  ro::sim::World unknown;
  REQUIRE(ApplyServerMessageFrame(MsgValue(kItemBannedOnMap, kUnknownItem), unknown, nullptr,
                                  kNowMs, names) == FrameOutcome::kApplied);
  CHECK(OnlyLine(unknown).text == "  ne peut pas être utilisé sur cette carte.");
  ro::sim::World tableless;
  REQUIRE(ApplyServerMessageFrame(MsgValue(kItemBannedOnMap, kApple), tableless, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(tableless).text == "  ne peut pas être utilisé sur cette carte.");
}

TEST_CASE("Messages du serveur : 0x07E6 nomme la compétence par son nom affiché",
          "[net][server_message]") {
  const ro::sim::SkillTree tree = BashTree();

  ro::sim::World world;
  REQUIRE(ApplyServerMessageFrame(MsgSkill(kBash, kSkillSuccess), world, &tree, kNowMs) ==
          FrameOutcome::kApplied);
  const ro::sim::ChatLine& line = OnlyLine(world);
  CHECK(line.text == std::string("[Bash] ") + Tr("Réussite"));
  CHECK(line.color == kLavender);
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  // ⚠ SA CASE EST CELLE DES ÉCHECS DE COMPÉTENCE, et non la case générale.
  CHECK(line.type == ro::sim::ChatType::kSkillFailure);
}

TEST_CASE("Messages du serveur : sans arbre, le nom interne puis le numéro",
          "[net][server_message]") {
  ro::sim::World unnamed;
  REQUIRE(ApplyServerMessageFrame(MsgSkill(kBash, kSkillSuccess), unnamed, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(unnamed).text == std::string("[5] ") + Tr("Réussite"));

  ro::sim::World learned;
  ro::sim::KnownSkill bash;
  bash.id = kBash;
  bash.level = 1;
  bash.name = "SM_BASH";
  learned.skill_book().Learn(std::move(bash));
  REQUIRE(ApplyServerMessageFrame(MsgSkill(kBash, kSkillSuccess), learned, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(learned).text == std::string("[SM_BASH] ") + Tr("Réussite"));
}

TEST_CASE("Messages du serveur : un plafond du tribut à la guilde va sous sa case, par 0x0291",
          "[net][server_message]") {
  constexpr std::uint16_t kGuildExpFull = 3475;
  constexpr std::uint16_t kCharacterExpFull = 3476;
  ro::sim::World world;
  REQUIRE(ApplyServerMessageFrame(Msg(kGuildExpFull), world, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyServerMessageFrame(Msg(kCharacterExpFull), world, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text ==
        Tr("L'expérience de la guilde est au maximum : vous ne pouvez plus accumuler "
           "d'expérience de guilde."));
  CHECK(world.chat_lines()[1].text ==
        Tr("L'expérience du personnage est au maximum : vous ne pouvez plus accumuler "
           "d'expérience de guilde."));
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    CHECK(line.type == ro::sim::ChatType::kRepaymentExp);
    CHECK(line.color == kWhite);
  }

  // ⚠ LE TRI NE VAUT QUE POUR 0x0291 : par 0x09CD, le même numéro reste sous la
  // case générale, et un autre numéro par 0x0291 aussi.
  ro::sim::World colored;
  REQUIRE(ApplyServerMessageFrame(MsgColor(kGuildExpFull, kPaleCyanBgr), colored, nullptr,
                                  kNowMs) == FrameOutcome::kApplied);
  CHECK(OnlyLine(colored).type == ro::sim::ChatType::kPublic);
  ro::sim::World plain;
  REQUIRE(ApplyServerMessageFrame(Msg(kCannotEquip), plain, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(plain).type == ro::sim::ChatType::kPublic);
}

TEST_CASE("Messages du serveur : un numéro inconnu n'écrit rien", "[net][server_message]") {
  ro::sim::World world;
  CHECK(ApplyServerMessageFrame(Msg(kUnknownMessage), world, nullptr, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(ApplyServerMessageFrame(MsgColor(kUnknownMessage, kPaleCyanBgr), world, nullptr, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(ApplyServerMessageFrame(MsgValue(kUnknownMessage, 3), world, nullptr, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(ApplyServerMessageFrame(MsgSkill(kBash, -1), world, nullptr, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE(
    "Messages du serveur : une trame tronquée est indécodable, et d'autres opcodes ne "
    "le regardent pas",
    "[net][server_message]") {
  ro::sim::World world;
  ro::net::Frame cut = MsgColor(kCannotEquip, kPaleCyanBgr);
  cut.bytes.pop_back();
  CHECK(ApplyServerMessageFrame(cut, world, nullptr, kNowMs) == FrameOutcome::kUndecodable);
  ro::net::Frame other = Msg(kCannotEquip);
  other.opcode = ro::proto::ZcNotifyVanish::kOpcode;
  CHECK(ApplyServerMessageFrame(other, world, nullptr, kNowMs) == FrameOutcome::kUnhandled);
  CHECK(world.chat_lines().empty());
}

TEST_CASE(
    "Messages du serveur : la table est triée sans doublon, et seuls 143, 1862 et 3278 ont un "
    "trou",
    "[net][server_message]") {
  const auto table = ro::net::ServerMessageTable();
  REQUIRE_FALSE(table.empty());
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].message_id < table[index].message_id);
  }
  for (const ro::net::ServerMessageEntry& entry : table) {
    REQUIRE(entry.french != nullptr);
    CHECK(Holes(entry.french, "%d") == (entry.message_id == kReuseLimit ? 1U : 0U));
    const bool names_an_item = entry.message_id == kItemBannedOnMap || entry.message_id == kMvpItem;
    CHECK(Holes(entry.french, "%s") == (names_an_item ? 1U : 0U));
  }
}
