// CE QUE LE SERVEUR DE CARTE DIT DU PERSONNAGE : l'expérience gagnée, l'identifiant
// de compte, la classe d'un PNJ qui change, et ce qu'aucune fenêtre ne montre
// encore — jeu d'équipement de rechange, réputation, packs d'émotions.

#include "ro/net/character_ledger.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/net/world_update.h"
#include "ro/proto/ledger_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/character_ledger.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::core::Tr;
using ro::net::ApplyFrame;
using ro::net::ApplyLedgerFrame;
using ro::net::FrameOutcome;
using ro::sim::CharacterLedger;
using ro::sim::ChatKind;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kPlayerId = 2'000'001;
constexpr std::uint32_t kOtherId = 2'000'002;
constexpr std::uint32_t kPoringId = 110'001;
constexpr std::uint32_t kNowMs = 5000;
constexpr std::uint8_t kMonsterObject = 5;
constexpr std::uint16_t kPoringJob = 1002;
constexpr std::uint32_t kDropsJob = 1113;
constexpr std::uint16_t kBaseExpVar = 1;
constexpr std::uint16_t kJobExpVar = 2;
constexpr std::uint32_t kExpColor = 0xFFFF00;
constexpr std::uint32_t kQuestExpColor = 0xBE58BE;

[[nodiscard]] ro::sim::World WorldOf(std::uint32_t player_id) {
  ro::sim::World world;
  world.set_player_id(player_id);
  return world;
}

[[nodiscard]] FrameOutcome TellExp(ro::sim::World& world, ro::proto::ExpNotice notice) {
  return ApplyFrame(FrameOf(ro::proto::EncodeExpNotice(notice)), world, kNowMs);
}

[[nodiscard]] std::vector<ro::sim::ChatLine> LinesOf(const ro::sim::World& world, ChatKind kind) {
  std::vector<ro::sim::ChatLine> lines;
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    if (line.kind == kind) {
      lines.push_back(line);
    }
  }
  return lines;
}

}  // namespace

TEST_CASE("Expérience : un gain de base s'écrit sous son genre, en jaune", "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = 1200, .var_id = kBaseExpVar}) ==
          FrameOutcome::kApplied);
  const std::vector<ro::sim::ChatLine> lines = LinesOf(world, ChatKind::kExp);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].text.find("'1200'") != std::string::npos);
  CHECK(lines[0].color == kExpColor);
  CHECK(lines[0].speaker_id == 0);
}

TEST_CASE("Expérience : celle d'une quête est violette", "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  REQUIRE(TellExp(world,
                  {.account_id = kPlayerId, .amount = 50, .var_id = kJobExpVar, .exp_type = 1}) ==
          FrameOutcome::kApplied);
  const std::vector<ro::sim::ChatLine> lines = LinesOf(world, ChatKind::kExp);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].color == kQuestExpColor);
}

TEST_CASE("Expérience : une perte garde son signe, et sa phrase", "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = -300, .var_id = kBaseExpVar}) ==
          FrameOutcome::kApplied);
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = 300, .var_id = kBaseExpVar}) ==
          FrameOutcome::kApplied);
  const std::vector<ro::sim::ChatLine> lines = LinesOf(world, ChatKind::kExp);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].text.find("'-300'") != std::string::npos);
  // La perte et le gain ne disent pas la même phrase.
  CHECK(lines[0].text.substr(lines[0].text.find(' ')) !=
        lines[1].text.substr(lines[1].text.find(' ')));
}

TEST_CASE("Expérience : rien ne s'écrit d'un gain nul ni d'un autre personnage", "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  CHECK(TellExp(world, {.account_id = kPlayerId, .amount = 0, .var_id = kBaseExpVar}) ==
        FrameOutcome::kIgnored);
  CHECK(TellExp(world, {.account_id = kOtherId, .amount = 10, .var_id = kBaseExpVar}) ==
        FrameOutcome::kIgnored);
  CHECK(LinesOf(world, ChatKind::kExp).empty());
}

TEST_CASE("Expérience : la phrase suit le paramètre, et un autre n'écrit que sa fin",
          "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  constexpr std::uint16_t kOtherVar = 3;
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = 1200, .var_id = kBaseExpVar}) ==
          FrameOutcome::kApplied);
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = 40, .var_id = kJobExpVar}) ==
          FrameOutcome::kApplied);
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = 10, .var_id = kOtherVar}) ==
          FrameOutcome::kApplied);
  REQUIRE(TellExp(world, {.account_id = kPlayerId, .amount = -10, .var_id = kOtherVar}) ==
          FrameOutcome::kApplied);
  const std::vector<ro::sim::ChatLine> lines = LinesOf(world, ChatKind::kExp);
  REQUIRE(lines.size() == 4);
  CHECK(lines[0].text.starts_with("'1200' "));
  CHECK(lines[1].text.starts_with("'40' "));
  CHECK(lines[0].text.substr(lines[0].text.find(' ')) !=
        lines[1].text.substr(lines[1].text.find(' ')));
  CHECK(lines[2].text == " " + std::string(Tr("obtenus.")));
  CHECK(lines[3].text == " " + std::string(Tr("perdus.")));
}

TEST_CASE("Expérience : une trame tronquée est indéchiffrable", "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  std::vector<std::uint8_t> bytes =
      ro::proto::EncodeExpNotice({.account_id = kPlayerId, .amount = 10, .var_id = kBaseExpVar});
  REQUIRE(bytes.size() == ro::proto::ExpNotice::kWireSize);
  bytes.pop_back();
  CHECK(ApplyFrame(FrameOf(bytes), world, kNowMs) == FrameOutcome::kUndecodable);
}

TEST_CASE("Identifiant de compte : il confirme celui de la connexion", "[net][exp]") {
  ro::sim::World world = WorldOf(kPlayerId);
  CHECK(ApplyFrame(FrameOf(ro::proto::EncodeAccountIdNotice({.account_id = kPlayerId})), world,
                   kNowMs) == FrameOutcome::kApplied);
  // Un désaccord ne change pas le joueur : c'est lui que tout le reste désigne.
  CHECK(ApplyFrame(FrameOf(ro::proto::EncodeAccountIdNotice({.account_id = kOtherId})), world,
                   kNowMs) == FrameOutcome::kIgnored);
  CHECK(world.player_id() == kPlayerId);
}

TEST_CASE("Classe d'un PNJ : un monstre prend la nouvelle, et son sexe revient à zéro",
          "[net][class_change]") {
  ro::sim::World world = WorldOf(kPlayerId);
  ro::proto::PacketIdleUnit poring = IdleUnit(kPoringId, {100, 100});
  poring.objecttype = kMonsterObject;
  poring.job = kPoringJob;
  poring.sex = 1;
  REQUIRE(ApplyFrame(FrameOf(Encode(poring)), world, kNowMs) == FrameOutcome::kApplied);

  ro::proto::ZcNpcspriteChange change;
  change.GID = kPoringId;
  change.class_ = kDropsJob;
  REQUIRE(ApplyFrame(FrameOf(Encode(change)), world, kNowMs) == FrameOutcome::kApplied);
  const ro::sim::Actor* actor = world.Find(kPoringId);
  REQUIRE(actor != nullptr);
  CHECK(actor->look.job == kDropsJob);
  CHECK(actor->look.sex == 0);
  CHECK(actor->position.x == 100);
}

TEST_CASE("Classe d'un PNJ : un joueur ou un absent n'est pas touché", "[net][class_change]") {
  ro::sim::World world = WorldOf(kPlayerId);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kOtherId, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
  const std::uint16_t job_before = world.Find(kOtherId)->look.job;

  ro::proto::ZcNpcspriteChange change;
  change.GID = kOtherId;
  change.class_ = kDropsJob;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, kNowMs) == FrameOutcome::kIgnored);
  CHECK(world.Find(kOtherId)->look.job == job_before);

  change.GID = kPoringId;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, kNowMs) == FrameOutcome::kIgnored);
}

TEST_CASE("Jeu d'équipement de rechange : la liste se lit et se remplace", "[net][ledger]") {
  ro::proto::EquipSwitchList list;
  list.entries = {{.client_slot = 12, .location = 0x0002}, {.client_slot = 15, .location = 0x0020}};
  CharacterLedger ledger;
  REQUIRE(ApplyLedgerFrame(FrameOf(ro::proto::EncodeEquipSwitchList(list)), ledger) ==
          FrameOutcome::kApplied);
  REQUIRE(ledger.equip_switch().size() == 2);
  CHECK(ledger.equip_switch()[1].client_slot == 15);
  CHECK(ledger.equip_switch()[1].location == 0x0020);

  REQUIRE(ApplyLedgerFrame(FrameOf(ro::proto::EncodeEquipSwitchList({})), ledger) ==
          FrameOutcome::kApplied);
  CHECK(ledger.equip_switch().empty());

  std::vector<std::uint8_t> ragged = ro::proto::EncodeEquipSwitchList(list);
  ragged.pop_back();
  ragged[2] = static_cast<std::uint8_t>(ragged.size());
  CHECK(ApplyLedgerFrame(FrameOf(ragged), ledger) == FrameOutcome::kUndecodable);
}

TEST_CASE("Jeu d'équipement de rechange : les accusés le tiennent à jour", "[net][ledger]") {
  constexpr std::uint16_t kSlot = 12;
  constexpr std::uint32_t kArmor = 0x0010;
  CharacterLedger ledger;
  REQUIRE(ApplyLedgerFrame(FrameOf(ro::proto::EncodeEquipSwitchAddAck(
                               {.client_slot = kSlot, .location = kArmor, .result = 0})),
                           ledger) == FrameOutcome::kApplied);
  REQUIRE(ledger.equip_switch().size() == 1);
  CHECK(ledger.equip_switch()[0].location == kArmor);

  // Un refus de rangement ne touche à rien.
  CHECK(ApplyLedgerFrame(FrameOf(ro::proto::EncodeEquipSwitchAddAck(
                             {.client_slot = kSlot + 1, .location = kArmor, .result = 2})),
                         ledger) == FrameOutcome::kIgnored);
  CHECK(ledger.equip_switch().size() == 1);

  // ⚠ UN REFUS DE SORTIE SORT QUAND MÊME LA PIÈCE, comme le client d'origine.
  REQUIRE(ApplyLedgerFrame(FrameOf(ro::proto::EncodeEquipSwitchRemoveAck(
                               {.client_slot = kSlot, .location = kArmor, .failure = 1})),
                           ledger) == FrameOutcome::kApplied);
  CHECK(ledger.equip_switch().empty());

  std::vector<std::uint8_t> ragged = ro::proto::EncodeEquipSwitchRunAck({.failure = 1});
  ragged.pop_back();
  CHECK(ApplyLedgerFrame(FrameOf(ragged), ledger) == FrameOutcome::kUndecodable);
}

TEST_CASE("Jeu d'équipement de rechange : ce que chaque accusé dit", "[net][ledger]") {
  using ro::net::EquipSwitchNewsOf;
  using ro::sim::EquipSwitchEvent;
  using ro::sim::EquipSwitchRefusal;
  constexpr std::uint16_t kSlot = 7;
  constexpr std::uint32_t kAmmo = 0x8000;

  const auto placed = EquipSwitchNewsOf(FrameOf(
      ro::proto::EncodeEquipSwitchAddAck({.client_slot = kSlot, .location = kAmmo, .result = 0})));
  REQUIRE(placed.has_value());
  CHECK(placed->event == EquipSwitchEvent::kPlaced);
  CHECK(placed->client_slot == kSlot);
  CHECK(placed->location == kAmmo);

  // Le rangement : trois dit « rien à échanger », tout autre refus « impossible ».
  const auto nothing = EquipSwitchNewsOf(
      FrameOf(ro::proto::EncodeEquipSwitchAddAck({.client_slot = kSlot, .result = 3})));
  REQUIRE(nothing.has_value());
  CHECK(nothing->event == EquipSwitchEvent::kPlaceRefused);
  CHECK(nothing->refusal == EquipSwitchRefusal::kNothingToReplace);
  const auto refused = EquipSwitchNewsOf(
      FrameOf(ro::proto::EncodeEquipSwitchAddAck({.client_slot = kSlot, .result = 2})));
  REQUIRE(refused.has_value());
  CHECK(refused->refusal == EquipSwitchRefusal::kCannotRegister);

  const auto removed = EquipSwitchNewsOf(
      FrameOf(ro::proto::EncodeEquipSwitchRemoveAck({.client_slot = kSlot, .failure = 0})));
  REQUIRE(removed.has_value());
  CHECK(removed->event == EquipSwitchEvent::kRemoved);
  // Un code de sortie autre qu'un se tait.
  CHECK_FALSE(EquipSwitchNewsOf(FrameOf(ro::proto::EncodeEquipSwitchRemoveAck(
                                    {.client_slot = kSlot, .failure = 2})))
                  .has_value());

  // L'échange accordé ne s'écrit pas ; refusé, un dit « rien à échanger ».
  CHECK_FALSE(
      EquipSwitchNewsOf(FrameOf(ro::proto::EncodeEquipSwitchRunAck({.failure = 0}))).has_value());
  const auto swap_one =
      EquipSwitchNewsOf(FrameOf(ro::proto::EncodeEquipSwitchRunAck({.failure = 1})));
  REQUIRE(swap_one.has_value());
  CHECK(swap_one->event == EquipSwitchEvent::kSwapRefused);
  CHECK(swap_one->refusal == EquipSwitchRefusal::kNothingToReplace);
  const auto swap_two =
      EquipSwitchNewsOf(FrameOf(ro::proto::EncodeEquipSwitchRunAck({.failure = 2})));
  REQUIRE(swap_two.has_value());
  CHECK(swap_two->refusal == EquipSwitchRefusal::kCannotSwap);
}

TEST_CASE("Réputation : la liste puis une seule faction, fusionnées", "[net][ledger]") {
  ro::proto::ZcReputeInfo info;
  info.success = 1;
  info.list = {{.type = 1, .points = 100}, {.type = 2, .points = -5}};
  CharacterLedger ledger;
  REQUIRE(ApplyLedgerFrame(FrameOf(Encode(info)), ledger) == FrameOutcome::kApplied);
  info.list = {{.type = 2, .points = 40}};
  REQUIRE(ApplyLedgerFrame(FrameOf(Encode(info)), ledger) == FrameOutcome::kApplied);
  REQUIRE(ledger.reputation().size() == 2);
  CHECK(ledger.reputation().at(1) == 100);
  CHECK(ledger.reputation().at(2) == 40);

  info.success = 0;
  info.list = {{.type = 1, .points = 0}};
  CHECK(ApplyLedgerFrame(FrameOf(Encode(info)), ledger) == FrameOutcome::kIgnored);
  CHECK(ledger.reputation().at(1) == 100);
}

TEST_CASE("Packs d'émotions : avec ou sans compte d'utilisations", "[net][ledger]") {
  CharacterLedger ledger;
  ro::proto::ZcAckBuyCashEmotion bought;
  bought.pack_id = 3;
  bought.has_count = 0;
  // Sans le drapeau, le compte ne vaut rien, même non nul.
  bought.count = 7;
  REQUIRE(ApplyLedgerFrame(FrameOf(Encode(bought)), ledger) == FrameOutcome::kApplied);
  bought.pack_id = 4;
  bought.has_count = 1;
  bought.count = 20;
  REQUIRE(ApplyLedgerFrame(FrameOf(Encode(bought)), ledger) == FrameOutcome::kApplied);
  REQUIRE(ledger.emotion_packs().size() == 2);
  CHECK_FALSE(ledger.emotion_packs().at(3).has_value());
  CHECK(ledger.emotion_packs().at(4) == 20);
}

TEST_CASE("Registre : ce qui n'est pas à lui n'est pas traité", "[net][ledger]") {
  CharacterLedger ledger;
  CHECK(ApplyLedgerFrame(FrameOf(ro::proto::EncodeExpNotice({})), ledger) ==
        FrameOutcome::kUnhandled);
}
