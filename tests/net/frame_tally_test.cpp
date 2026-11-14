#include "ro/net/frame_tally.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::net::DescribedFrameTally;
using ro::net::FrameOutcome;
using ro::net::FrameTally;
using ro::net::HexOpcode;

namespace {

// Range `count` trames du même opcode et du même verdict.
void RecordMany(FrameTally& tally, std::uint16_t opcode, FrameOutcome outcome, int count) {
  for (int index = 0; index < count; ++index) {
    tally.Record(opcode, outcome);
  }
}

}  // namespace

TEST_CASE("un opcode s'écrit sur quatre chiffres, minuscules et complétés à gauche") {
  CHECK(HexOpcode(0x0284) == "0x0284");
  CHECK(HexOpcode(0x000A) == "0x000a");
  CHECK(HexOpcode(0xFFFF) == "0xffff");
}

TEST_CASE("les quatre verdicts partitionnent les trames d'un opcode") {
  FrameTally tally;
  tally.Record(0x01DE, FrameOutcome::kApplied);
  tally.Record(0x01DE, FrameOutcome::kIgnored);
  tally.Record(0x01DE, FrameOutcome::kUnhandled);
  tally.Record(0x01DE, FrameOutcome::kUndecodable);

  const std::vector<FrameTally::Entry> all = tally.All();
  REQUIRE(all.size() == 1);
  CHECK(all[0].opcode == 0x01DE);
  CHECK(all[0].applied == 1);
  CHECK(all[0].ignored == 1);
  CHECK(all[0].unhandled == 1);
  CHECK(all[0].undecodable == 1);
  CHECK(all[0].total() == 4);
  // Ce qui n'a RIEN donné, et rien d'autre : une trame ignorée est comprise.
  CHECK(all[0].missing() == 2);
  CHECK(tally.frames() == 4);
  CHECK(tally.missing_frames() == 2);
}

TEST_CASE("un opcode entièrement traduit ne figure pas dans ce qui manque") {
  FrameTally tally;
  RecordMany(tally, 0x0B1A, FrameOutcome::kApplied, 3);
  RecordMany(tally, 0x0088, FrameOutcome::kIgnored, 2);

  CHECK(tally.All().size() == 2);
  CHECK(tally.Missing().empty());
  CHECK(tally.missing_frames() == 0);
}

TEST_CASE("ce qui manque vient du plus fréquent au moins fréquent") {
  FrameTally tally;
  RecordMany(tally, 0x043D, FrameOutcome::kUnhandled, 2);
  RecordMany(tally, 0x0284, FrameOutcome::kUnhandled, 9);
  RecordMany(tally, 0x0A41, FrameOutcome::kUndecodable, 5);

  const std::vector<FrameTally::Entry> missing = tally.Missing();
  REQUIRE(missing.size() == 3);
  CHECK(missing[0].opcode == 0x0284);
  CHECK(missing[1].opcode == 0x0A41);
  CHECK(missing[2].opcode == 0x043D);
}

TEST_CASE("à égalité, l'opcode croissant tranche") {
  FrameTally tally;
  tally.Record(0x0B0D, FrameOutcome::kUnhandled);
  tally.Record(0x0284, FrameOutcome::kUnhandled);
  tally.Record(0x01FF, FrameOutcome::kUnhandled);

  const std::vector<FrameTally::Entry> missing = tally.Missing();
  REQUIRE(missing.size() == 3);
  CHECK(missing[0].opcode == 0x01FF);
  CHECK(missing[1].opcode == 0x0284);
  CHECK(missing[2].opcode == 0x0B0D);
}

TEST_CASE("un relevé vide le dit plutôt que de ne rien rendre") {
  const FrameTally tally;
  const std::vector<std::string> lines = DescribedFrameTally(tally, 8);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "trames : aucune reçue");
}

TEST_CASE("un relevé sans perte tient en une ligne") {
  FrameTally tally;
  RecordMany(tally, 0x0B1A, FrameOutcome::kApplied, 4);

  const std::vector<std::string> lines = DescribedFrameTally(tally, 8);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "trames : 4 reçues, toutes traduites");
}

TEST_CASE("le résumé sépare les non traitées des indéchiffrables") {
  FrameTally tally;
  RecordMany(tally, 0x0B1A, FrameOutcome::kApplied, 10);
  RecordMany(tally, 0x0284, FrameOutcome::kUnhandled, 3);
  tally.Record(0x0A41, FrameOutcome::kUndecodable);

  const std::vector<std::string> lines = DescribedFrameTally(tally, 8);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "trames : 14 reçues, 4 perdues sur 2 opcodes (dont 1 indéchiffrable)");
  CHECK(lines[1] == "0x0284 ×3  0x0a41 ×1");
}

TEST_CASE("une seule trame reçue se dit au singulier") {
  FrameTally tally;
  tally.Record(0x0284, FrameOutcome::kUnhandled);

  const std::vector<std::string> lines = DescribedFrameTally(tally, 8);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "trames : 1 reçue, 1 perdue sur 1 opcode");
}

TEST_CASE("les opcodes se groupent par ligne") {
  FrameTally tally;
  // Quatre opcodes, de fréquence décroissante pour que l'ordre soit prévisible.
  RecordMany(tally, 0x0001, FrameOutcome::kUnhandled, 4);
  RecordMany(tally, 0x0002, FrameOutcome::kUnhandled, 3);
  RecordMany(tally, 0x0003, FrameOutcome::kUnhandled, 2);
  RecordMany(tally, 0x0004, FrameOutcome::kUnhandled, 1);

  const std::vector<std::string> lines = DescribedFrameTally(tally, 8);
  REQUIRE(lines.size() == 3);
  CHECK(lines[1] == "0x0001 ×4  0x0002 ×3  0x0003 ×2");
  CHECK(lines[2] == "0x0004 ×1");
}

TEST_CASE("ce qui ne tient pas dans la place se compte au lieu de se taire") {
  FrameTally tally;
  // Cinq opcodes pour deux lignes de place : la seconde n'en nomme que trois,
  // et doit avouer les deux qu'elle laisse.
  for (std::uint16_t opcode = 1; opcode <= 5; ++opcode) {
    RecordMany(tally, opcode, FrameOutcome::kUnhandled, 6 - opcode);
  }

  const std::vector<std::string> lines = DescribedFrameTally(tally, 2);
  REQUIRE(lines.size() == 2);
  CHECK(lines[1] == "0x0001 ×5  0x0002 ×4  0x0003 ×3  … +2");
}

TEST_CASE("le résumé passe même quand il est seul à tenir") {
  FrameTally tally;
  RecordMany(tally, 0x0284, FrameOutcome::kUnhandled, 2);

  const std::vector<std::string> lines = DescribedFrameTally(tally, 1);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "trames : 2 reçues, 2 perdues sur 1 opcode");
}

TEST_CASE("vider le relevé le ramène à son état de départ") {
  FrameTally tally;
  RecordMany(tally, 0x0284, FrameOutcome::kUnhandled, 3);
  tally.Clear();

  CHECK(tally.frames() == 0);
  CHECK(tally.missing_frames() == 0);
  CHECK(tally.All().empty());
  CHECK(tally.Missing().empty());
}

TEST_CASE("l'auteur attaché à une trame compte, appliqué, sous son opcode") {
  constexpr std::uint16_t kGuildChat = 0x017F;
  constexpr std::uint16_t kChatAuthor = 0x0F3A;
  FrameTally tally;
  ro::net::Frame frame{kGuildChat, {0x7F, 0x01}};
  frame.preamble = {0x3A, 0x0F};
  tally.Record(frame, FrameOutcome::kApplied);

  const std::vector<FrameTally::Entry> all = tally.All();
  REQUIRE(all.size() == 2);
  CHECK(all[0].opcode == kGuildChat);
  CHECK(all[1].opcode == kChatAuthor);
  CHECK(all[1].applied == 1);
  CHECK(tally.missing_frames() == 0);
}
