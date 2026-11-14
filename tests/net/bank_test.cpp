// LA BANQUE DE ZENY, décodée — et les trois demandes, sur le fil.
//
// 🔴 CE BANC PORTE QUATRE CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · `ZC_BANKING_CHECK` BASCULE la fenêtre. Deux réponses de suite l'ouvrent
//     puis la referment, et c'est pour cela qu'il n'y a pas de « rafraîchir » ;
//   · sa disposition est l'INVERSE de celle des deux accusés — solde d'abord,
//     raison ensuite —, et confondre les deux lit le solde dans la raison ;
//   · les raisons ne se numérotent pas pareil d'un paquet à l'autre : la valeur
//     2 veut dire « le système travaille », « pas assez de zeny » ou « erreur
//     inconnue » selon le paquet qui la porte ;
//   · un accusé REFUSÉ ne touche à aucun solde. Le reprendre quand même
//     écraserait la banque avec ce que le serveur renvoie d'un refus.

#include "ro/net/bank.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyBankFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::RequestBankCheck;
using ro::net::RequestBankDeposit;
using ro::net::RequestBankWithdraw;
using ro::sim::BankVerdict;
using ro::sim::World;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::PutU64;
using ro::test_support::ReadU32;

namespace {

// ⚠ LE SOLDE EST EN PREMIER, LA RAISON ENSUITE : c'est l'ordre de ce paquet-là,
// et de lui seul.
[[nodiscard]] Frame CheckFrame(std::int64_t money, std::int16_t reason) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcBankingCheck::kOpcode);
  PutU64(bytes, static_cast<std::uint64_t>(money));
  PutU16(bytes, static_cast<std::uint16_t>(reason));
  return Frame{.opcode = ro::proto::ZcBankingCheck::kOpcode, .bytes = bytes};
}

// ⚠ ET LÀ, LA RAISON D'ABORD : les deux accusés rangent leurs champs dans
// l'autre sens.
[[nodiscard]] Frame AckFrame(std::uint16_t opcode, std::int16_t reason, std::int64_t money,
                             std::int32_t zeny) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, opcode);
  PutU16(bytes, static_cast<std::uint16_t>(reason));
  PutU64(bytes, static_cast<std::uint64_t>(money));
  PutU32(bytes, static_cast<std::uint32_t>(zeny));
  return Frame{.opcode = opcode, .bytes = bytes};
}

[[nodiscard]] Frame DepositAck(std::int16_t reason, std::int64_t money, std::int32_t zeny) {
  return AckFrame(ro::proto::ZcAckBankingDeposit::kOpcode, reason, money, zeny);
}

[[nodiscard]] Frame WithdrawAck(std::int16_t reason, std::int64_t money, std::int32_t zeny) {
  return AckFrame(ro::proto::ZcAckBankingWithdraw::kOpcode, reason, money, zeny);
}

}  // namespace

TEST_CASE("Banque : la réponse d'ouverture BASCULE la fenêtre", "[net][bank]") {
  World world;
  CHECK_FALSE(world.bank().open());

  CHECK(ApplyBankFrame(CheckFrame(1000, 0), world) == FrameOutcome::kApplied);
  CHECK(world.bank().open());
  CHECK(world.bank().vault() == 1000);

  // 🔴 LA SECONDE REFERME. C'est le comportement du client d'origine, et c'est
  // ce qui interdit un bouton « rafraîchir » : redemander le solde ferait
  // disparaître la fenêtre.
  CHECK(ApplyBankFrame(CheckFrame(1000, 0), world) == FrameOutcome::kApplied);
  CHECK_FALSE(world.bank().open());
}

TEST_CASE("Banque : un refus d'ouverture pose le solde et n'ouvre rien", "[net][bank]") {
  World world;

  // ⚠ LE SOLDE SE POSE MÊME SUR UN REFUS : d'autres écrans le lisent, et il ne
  // dépend pas de ce qu'on décide d'afficher.
  CHECK(ApplyBankFrame(CheckFrame(4242, 2), world) == FrameOutcome::kApplied);
  CHECK(world.bank().vault() == 4242);
  CHECK_FALSE(world.bank().open());
  CHECK(world.bank().verdict() == BankVerdict::kSystemBusy);

  // Toute autre raison est l'erreur de système, comme chez le client d'origine.
  CHECK(ApplyBankFrame(CheckFrame(4242, 7), world) == FrameOutcome::kApplied);
  CHECK(world.bank().verdict() == BankVerdict::kSystemError);
  CHECK_FALSE(world.bank().open());
}

TEST_CASE("Banque : un dépôt réussi pose les deux soldes", "[net][bank]") {
  World world;
  CHECK(ApplyBankFrame(DepositAck(0, 5000, 1200), world) == FrameOutcome::kApplied);
  CHECK(world.bank().vault() == 5000);
  CHECK(world.player_status().zeny == 1200);
  CHECK(world.bank().verdict() == BankVerdict::kDone);
}

TEST_CASE("Banque : un accusé refusé ne touche à aucun solde", "[net][bank]") {
  World world;
  REQUIRE(ApplyBankFrame(DepositAck(0, 5000, 1200), world) == FrameOutcome::kApplied);

  // `BDA_NO_MONEY`. ⚠ LES SOLDES RESTENT CEUX D'AVANT : le client d'origine ne
  // les reprend que sur un succès.
  CHECK(ApplyBankFrame(DepositAck(2, 9, 9), world) == FrameOutcome::kApplied);
  CHECK(world.bank().vault() == 5000);
  CHECK(world.player_status().zeny == 1200);
  CHECK(world.bank().verdict() == BankVerdict::kDepositNoMoney);
}

TEST_CASE("Banque : la même raison ne dit pas la même chose selon le paquet", "[net][bank]") {
  World world;

  // 🔴 DEUX veut dire « pas assez de zeny » sur un dépôt…
  CHECK(ApplyBankFrame(DepositAck(2, 0, 0), world) == FrameOutcome::kApplied);
  CHECK(world.bank().verdict() == BankVerdict::kDepositNoMoney);
  // … et « erreur inconnue » sur un retrait, qui tombe dans le défaut du client.
  CHECK(ApplyBankFrame(WithdrawAck(2, 0, 0), world) == FrameOutcome::kApplied);
  CHECK(world.bank().verdict() == BankVerdict::kSystemError);
  // UN veut dire « la banque ne suffit pas » sur un retrait…
  CHECK(ApplyBankFrame(WithdrawAck(1, 0, 0), world) == FrameOutcome::kApplied);
  CHECK(world.bank().verdict() == BankVerdict::kWithdrawNoMoney);
  // … et « erreur » sur un dépôt, qui tombe dans le même défaut.
  CHECK(ApplyBankFrame(DepositAck(1, 0, 0), world) == FrameOutcome::kApplied);
  CHECK(world.bank().verdict() == BankVerdict::kSystemError);
}

TEST_CASE("Banque : chaque verdict porte un numéro, même répété", "[net][bank]") {
  World world;
  CHECK(world.bank().verdict_seq() == 0);

  REQUIRE(ApplyBankFrame(DepositAck(2, 0, 0), world) == FrameOutcome::kApplied);
  const std::uint64_t first = world.bank().verdict_seq();
  CHECK(first == 1);

  // 🔴 LE MÊME REFUS, UNE SECONDE FOIS, EST UN SECOND REFUS. Sans ce numéro, le
  // joueur ne saurait pas que son deuxième clic a été refusé lui aussi.
  REQUIRE(ApplyBankFrame(DepositAck(2, 0, 0), world) == FrameOutcome::kApplied);
  CHECK(world.bank().verdict_seq() == first + 1);
  CHECK(world.bank().verdict() == BankVerdict::kDepositNoMoney);
}

TEST_CASE("Banque : une trame étrangère n'est pas à elle", "[net][bank]") {
  World world;
  const Frame other{.opcode = 0x0001, .bytes = {0x01, 0x00}};
  CHECK(ApplyBankFrame(other, world) == FrameOutcome::kUnhandled);
}

TEST_CASE("Banque : une trame tronquée se refuse", "[net][bank]") {
  World world;
  Frame cut = CheckFrame(1, 0);
  cut.bytes.pop_back();
  CHECK(ApplyBankFrame(cut, world) == FrameOutcome::kUndecodable);
  CHECK_FALSE(world.bank().open());
}

TEST_CASE("Banque : les trois demandes portent l'identifiant de compte", "[net][bank]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(RequestBankCheck(link, 0x11223344).has_value());
  const std::vector<std::uint8_t> check = server.Receive(ro::proto::CzReqBankingCheck::kWireSize);
  REQUIRE(check.size() == ro::proto::CzReqBankingCheck::kWireSize);
  CHECK((check[0] | (check[1] << 8)) == ro::proto::CzReqBankingCheck::kOpcode);
  CHECK(ReadU32(check, 2) == 0x11223344);

  REQUIRE(RequestBankDeposit(link, 0x11223344, 7500).has_value());
  const std::vector<std::uint8_t> deposit =
      server.Receive(ro::proto::CzReqBankingDeposit::kWireSize);
  REQUIRE(deposit.size() == ro::proto::CzReqBankingDeposit::kWireSize);
  CHECK((deposit[0] | (deposit[1] << 8)) == ro::proto::CzReqBankingDeposit::kOpcode);
  CHECK(ReadU32(deposit, 2) == 0x11223344);
  CHECK(ReadU32(deposit, 6) == 7500);

  REQUIRE(RequestBankWithdraw(link, 0x11223344, 250).has_value());
  const std::vector<std::uint8_t> withdraw =
      server.Receive(ro::proto::CzReqBankingWithdraw::kWireSize);
  REQUIRE(withdraw.size() == ro::proto::CzReqBankingWithdraw::kWireSize);
  CHECK((withdraw[0] | (withdraw[1] << 8)) == ro::proto::CzReqBankingWithdraw::kOpcode);
  CHECK(ReadU32(withdraw, 6) == 250);
}
