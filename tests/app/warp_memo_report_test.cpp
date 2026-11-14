#include "items/warp_memo_report.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/warp_memo_words.h"

namespace {

using moonclient::WarpMemoLineOfAck;
using moonclient::WarpMemoLineOfMapInfo;
using ro::vfs::WarpMemoMessage;

// Les deux couleurs du client, réécrites en toutes lettres : un essai qui
// relirait les constantes qu'il vérifie suivrait la faute au lieu de
// l'attraper.
constexpr std::uint32_t kCyan = 0x00FFFF;
constexpr std::uint32_t kRed = 0xFF0000;

}  // namespace

TEST_CASE("Mémo : la commande répond à ses trois noms, et à eux seuls") {
  using moonclient::IsWarpMemoCommand;
  CHECK(IsWarpMemoCommand("/memo"));
  // Le mot coréen tapé sur un clavier latin.
  CHECK(IsWarpMemoCommand("/rldjr"));
  CHECK(IsWarpMemoCommand("/기억"));
  // La casse latine ne compte pas, et ce qui suit le premier mot est ignoré.
  CHECK(IsWarpMemoCommand("/MEMO"));
  CHECK(IsWarpMemoCommand("/memo prontera"));
  // Ni un préfixe, ni un mot plus long, ni une parole sans barre oblique.
  CHECK_FALSE(IsWarpMemoCommand("/mem"));
  CHECK_FALSE(IsWarpMemoCommand("/memory"));
  CHECK_FALSE(IsWarpMemoCommand("memo"));
  CHECK_FALSE(IsWarpMemoCommand(""));
}

TEST_CASE("Mémo : chaque code de la réponse trouve SA phrase, et sa couleur") {
  // ⚠ LES NUMÉROS SE RÉÉCRIVENT EN TOUTES LETTRES : ils viennent de
  // `e_ack_remember_warppoint_result`, moonlight src/map/clif.hpp.
  const auto saved = WarpMemoLineOfAck(0);
  REQUIRE(saved.has_value());
  CHECK(saved->message == WarpMemoMessage::kSaved);
  CHECK(saved->color == kCyan);

  const auto too_low = WarpMemoLineOfAck(1);
  REQUIRE(too_low.has_value());
  CHECK(too_low->message == WarpMemoMessage::kSkillTooLow);
  CHECK(too_low->color == kRed);

  const auto not_learned = WarpMemoLineOfAck(2);
  REQUIRE(not_learned.has_value());
  CHECK(not_learned->message == WarpMemoMessage::kSkillNotLearned);
  CHECK(not_learned->color == kRed);

  const auto here = WarpMemoLineOfAck(3);
  REQUIRE(here.has_value());
  CHECK(here->message == WarpMemoMessage::kCannotSaveHere);
  CHECK(here->color == kRed);
}

TEST_CASE("Mémo : un code de réponse inconnu se tait") {
  // Comme le client : aucune phrase vaut mieux qu'une raison inventée.
  CHECK_FALSE(WarpMemoLineOfAck(4).has_value());
  CHECK_FALSE(WarpMemoLineOfAck(255).has_value());
}

TEST_CASE("Mémo : le refus de la carte trouve SA phrase, et sa couleur") {
  // `e_notify_mapinfo_result`, moonlight src/map/clif.hpp — puis la couleur que
  // le client donne à chacune : rouge pour la téléportation seule.
  const auto teleport = WarpMemoLineOfMapInfo(0);
  REQUIRE(teleport.has_value());
  CHECK(teleport->message == WarpMemoMessage::kTeleportForbidden);
  CHECK(teleport->color == kRed);

  // C'EST CELUI QUE LE SERVEUR ENVOIE quand la carte interdit de mémoriser.
  const auto memo = WarpMemoLineOfMapInfo(1);
  REQUIRE(memo.has_value());
  CHECK(memo->message == WarpMemoMessage::kMemoForbidden);
  CHECK(memo->color == kCyan);

  const auto skill = WarpMemoLineOfMapInfo(2);
  REQUIRE(skill.has_value());
  CHECK(skill->message == WarpMemoMessage::kSkillForbiddenHere);
  CHECK(skill->color == kCyan);

  const auto item = WarpMemoLineOfMapInfo(3);
  REQUIRE(item.has_value());
  CHECK(item->message == WarpMemoMessage::kItemForbiddenHere);
  CHECK(item->color == kCyan);
}

TEST_CASE("Mémo : un code de refus inconnu ou négatif se tait") {
  CHECK_FALSE(WarpMemoLineOfMapInfo(4).has_value());
  CHECK_FALSE(WarpMemoLineOfMapInfo(-1).has_value());
}
