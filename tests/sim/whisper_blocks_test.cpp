// LA LISTE DE CEUX QUI NE PEUVENT PLUS NOUS CHUCHOTER : ce que chaque réponse
// du serveur y change, telle que le serveur lui-même la tient.

#include "ro/sim/whisper_blocks.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::WhisperBlockAnswer;
using ro::sim::WhisperBlockList;
using ro::sim::WhisperBlockResult;
using ro::sim::WhisperBlockWay;

namespace {

constexpr auto kDone = static_cast<std::uint8_t>(WhisperBlockResult::kDone);
constexpr auto kFailed = static_cast<std::uint8_t>(WhisperBlockResult::kFailed);
constexpr auto kListFull = static_cast<std::uint8_t>(WhisperBlockResult::kListFull);

}  // namespace

TEST_CASE("Blocage : un blocage réussi fait entrer le nom de la demande", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  blocks.Expect("Aurélie", WhisperBlockWay::kBlock);
  const WhisperBlockAnswer answer = blocks.Settle(WhisperBlockWay::kBlock, kDone);
  CHECK(answer.name == "Aurélie");
  CHECK(blocks.Blocks("Aurélie"));
  // ⚠ LA CASSE COMPTE, comme chez le serveur.
  CHECK_FALSE(blocks.Blocks("aurélie"));
  CHECK(blocks.pending() == 0);
  // Le serveur dit « réussi » pour un nom déjà présent : il n'entre pas deux fois.
  blocks.Expect("Aurélie", WhisperBlockWay::kBlock);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kBlock, kDone));
  CHECK(blocks.names().size() == 1);
}

TEST_CASE("Blocage : un refus de blocage ne change rien", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  blocks.Expect("Bob", WhisperBlockWay::kBlock);
  CHECK(blocks.Settle(WhisperBlockWay::kBlock, kFailed).name == "Bob");
  blocks.Expect("Carl", WhisperBlockWay::kBlock);
  CHECK(blocks.Settle(WhisperBlockWay::kBlock, kListFull).name == "Carl");
  CHECK(blocks.names().empty());
}

TEST_CASE("Blocage : autoriser fait sortir le nom, réussi ou refusé", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  blocks.Replace({"Bob", "Carl"});
  blocks.Expect("Bob", WhisperBlockWay::kAllow);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kAllow, kDone));
  CHECK_FALSE(blocks.Blocks("Bob"));
  // Le refus dit que le serveur ne l'avait pas : notre reflet s'aligne.
  blocks.Expect("Carl", WhisperBlockWay::kAllow);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kAllow, kFailed));
  CHECK(blocks.names().empty());
}

TEST_CASE("Blocage : les réponses s'apparient dans l'ordre des demandes", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  blocks.Expect("Bob", WhisperBlockWay::kBlock);
  blocks.Expect("Carl", WhisperBlockWay::kBlock);
  CHECK(blocks.Settle(WhisperBlockWay::kBlock, kFailed).name == "Bob");
  CHECK(blocks.Settle(WhisperBlockWay::kBlock, kDone).name == "Carl");
  CHECK_FALSE(blocks.Blocks("Bob"));
  CHECK(blocks.Blocks("Carl"));
}

TEST_CASE("Blocage : une réponse qui ne s'apparie pas ne touche à rien", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  // SANS DEMANDE : personne à nommer.
  CHECK(blocks.Settle(WhisperBlockWay::kBlock, kDone).name.empty());
  CHECK(blocks.names().empty());
  // DANS L'AUTRE SENS : l'ordre est perdu, la file s'oublie.
  blocks.Expect("Bob", WhisperBlockWay::kBlock);
  blocks.Expect("Carl", WhisperBlockWay::kBlock);
  CHECK(blocks.Settle(WhisperBlockWay::kAllow, kDone).name.empty());
  CHECK(blocks.pending() == 0);
  CHECK(blocks.names().empty());
}

TEST_CASE("Blocage : tout bloquer, puis tout autoriser", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  blocks.Replace({"Bob"});
  blocks.SettleEveryone(WhisperBlockWay::kBlock, false);
  CHECK(blocks.blocks_everyone());
  // Un refus dit qu'il l'était déjà.
  blocks.SettleEveryone(WhisperBlockWay::kBlock, true);
  CHECK(blocks.blocks_everyone());
  // L'interrupteur allumé, « tout autoriser » l'éteint SANS toucher la liste.
  blocks.SettleEveryone(WhisperBlockWay::kAllow, false);
  CHECK_FALSE(blocks.blocks_everyone());
  CHECK(blocks.Blocks("Bob"));
  // Éteint, il vide la liste.
  blocks.SettleEveryone(WhisperBlockWay::kAllow, false);
  CHECK(blocks.names().empty());
  // Et un refus dit qu'il n'y avait ni l'un ni l'autre.
  blocks.Replace({"Carl"});
  blocks.SettleEveryone(WhisperBlockWay::kAllow, true);
  CHECK(blocks.names().empty());
  CHECK_FALSE(blocks.blocks_everyone());
}

TEST_CASE("Blocage : la liste redite remplace la nôtre, et une partie neuve repart de rien",
          "[sim][whisper_block]") {
  WhisperBlockList blocks;
  blocks.Replace({"Bob"});
  blocks.Replace({"Carl", "", "Carl", "Dan"});
  CHECK(blocks.names().size() == 2);
  CHECK_FALSE(blocks.Blocks("Bob"));
  CHECK(blocks.Blocks("Dan"));
  blocks.SettleEveryone(WhisperBlockWay::kBlock, false);
  blocks.Expect("Eve", WhisperBlockWay::kBlock);
  blocks.Clear();
  CHECK(blocks.names().empty());
  CHECK_FALSE(blocks.blocks_everyone());
  CHECK(blocks.pending() == 0);
}

TEST_CASE("Blocage : la file des demandes reste bornée", "[sim][whisper_block]") {
  WhisperBlockList blocks;
  for (std::size_t sent = 0; sent <= ro::sim::kMostPendingWhisperBlocks; ++sent) {
    blocks.Expect("N" + std::to_string(sent), WhisperBlockWay::kBlock);
  }
  CHECK(blocks.pending() == ro::sim::kMostPendingWhisperBlocks);
  // La plus ancienne est tombée.
  CHECK(blocks.Settle(WhisperBlockWay::kBlock, kDone).name == "N1");
}

TEST_CASE("Blocage : chaque réponse qui change la liste se note, et se prend une fois",
          "[sim][whisper_block]") {
  using ro::sim::WhisperBlockChange;
  using ro::sim::WhisperBlockChangeKind;
  WhisperBlockList blocks;
  // La liste redite constate, elle ne change rien.
  blocks.Replace({"Bob"});
  CHECK(blocks.TakeChanges().empty());

  // Un blocage réussi se note même pour un nom déjà là ; un refus, non.
  blocks.Expect("Bob", WhisperBlockWay::kBlock);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kBlock, kDone));
  blocks.Expect("Carl", WhisperBlockWay::kBlock);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kBlock, kListFull));
  blocks.Expect("Dan", WhisperBlockWay::kAllow);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kAllow, kFailed));
  std::vector<WhisperBlockChange> changes = blocks.TakeChanges();
  REQUIRE(changes.size() == 2);
  CHECK(changes[0].kind == WhisperBlockChangeKind::kAdded);
  CHECK(changes[0].name == "Bob");
  CHECK(changes[1].kind == WhisperBlockChangeKind::kRemoved);
  CHECK(changes[1].name == "Dan");
  CHECK(blocks.TakeChanges().empty());

  // « Tout autoriser » vide la liste quand l'interrupteur était éteint, pas
  // quand il l'éteint.
  blocks.SettleEveryone(WhisperBlockWay::kBlock, false);
  blocks.SettleEveryone(WhisperBlockWay::kAllow, false);
  CHECK(blocks.TakeChanges().empty());
  blocks.SettleEveryone(WhisperBlockWay::kAllow, false);
  changes = blocks.TakeChanges();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].kind == WhisperBlockChangeKind::kEmptied);

  // Une partie neuve n'est pas un changement.
  blocks.Expect("Eve", WhisperBlockWay::kBlock);
  static_cast<void>(blocks.Settle(WhisperBlockWay::kBlock, kDone));
  blocks.Clear();
  CHECK(blocks.TakeChanges().empty());
}
