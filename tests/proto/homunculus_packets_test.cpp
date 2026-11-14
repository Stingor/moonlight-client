// LES PAQUETS DE L'HOMONCULE : la fiche et le champ qui change tels qu'ils
// arrivent, l'ordre du menu et le renommage tels qu'ils partent.

#include "ro/proto/homunculus_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::DecodeHomunculusParameter;
using ro::proto::DecodeHomunculusProperties;
using ro::proto::HomunculusCommand;
using ro::proto::HomunculusCommandRequest;
using ro::proto::HomunculusParameter;
using ro::proto::HomunculusProperties;
using ro::proto::HomunculusRenameRequest;

namespace {

void PutU16(std::vector<std::uint8_t>& into, std::size_t at, std::uint16_t value) {
  into[at] = static_cast<std::uint8_t>(value & 0xFFU);
  into[at + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void PutU32(std::vector<std::uint8_t>& into, std::size_t at, std::uint32_t value) {
  PutU16(into, at, static_cast<std::uint16_t>(value & 0xFFFFU));
  PutU16(into, at + 2, static_cast<std::uint16_t>(value >> 16U));
}

void PutU64(std::vector<std::uint8_t>& into, std::size_t at, std::uint64_t value) {
  PutU32(into, at, static_cast<std::uint32_t>(value & 0xFFFFFFFFU));
  PutU32(into, at + 4, static_cast<std::uint32_t>(value >> 32U));
}

// La fiche d'un Amistr niveau un, écrite octet par octet aux décalages du
// serveur.
[[nodiscard]] std::vector<std::uint8_t> AmistrSheet() {
  std::vector<std::uint8_t> frame(HomunculusProperties::kWireSize, 0);
  PutU16(frame, 0, 0x0BA4);
  const char name[] = "Amistr";
  for (std::size_t at = 0; at + 1 < sizeof(name); ++at) {
    frame[2 + at] = static_cast<std::uint8_t>(name[at]);
  }
  frame[26] = 0x4;
  PutU16(frame, 27, 1);
  PutU16(frame, 29, 48);
  PutU16(frame, 31, 246);
  PutU16(frame, 33, 45);
  PutU16(frame, 35, 15);
  PutU16(frame, 37, 25);
  PutU16(frame, 39, 5);
  PutU16(frame, 41, 42);
  PutU16(frame, 43, 2);
  PutU16(frame, 45, 18);
  PutU16(frame, 47, 635);
  PutU32(frame, 49, 320);
  PutU32(frame, 53, 320);
  PutU32(frame, 57, 10);
  PutU32(frame, 61, 10);
  PutU64(frame, 65, 0x100000000ULL);
  PutU64(frame, 73, 50);
  PutU16(frame, 81, 49);
  PutU16(frame, 83, 2);
  return frame;
}

}  // namespace

TEST_CASE("Homoncule : la fiche se lit aux décalages du serveur", "[proto][homunculus]") {
  const auto sheet = DecodeHomunculusProperties(AmistrSheet());
  REQUIRE(sheet.has_value());
  CHECK(sheet->name == "Amistr");
  CHECK(sheet->flags == ro::proto::kHomunculusAliveFlag);
  CHECK(sheet->level == 1);
  CHECK(sheet->hunger == 48);
  CHECK(sheet->intimacy == 246);
  CHECK(sheet->atk == 45);
  CHECK(sheet->matk == 15);
  CHECK(sheet->hit == 25);
  CHECK(sheet->crit == 5);
  CHECK(sheet->def == 42);
  CHECK(sheet->mdef == 2);
  CHECK(sheet->flee == 18);
  CHECK(sheet->amotion_ms == 635);
  CHECK(sheet->hp == 320);
  CHECK(sheet->max_hp == 320);
  CHECK(sheet->sp == 10);
  CHECK(sheet->max_sp == 10);
  // ⚠ L'EXPÉRIENCE TIENT SUR SOIXANTE-QUATRE BITS.
  CHECK(sheet->exp == 0x100000000LL);
  CHECK(sheet->exp_next == 50);
  CHECK(sheet->skill_points == 49);
  CHECK(sheet->attack_range_cells == 2);
}

TEST_CASE("Homoncule : une statistique au-delà de 32767 se relit signée", "[proto][homunculus]") {
  std::vector<std::uint8_t> frame = AmistrSheet();
  PutU16(frame, 35, 0x8000);
  const auto sheet = DecodeHomunculusProperties(frame);
  REQUIRE(sheet.has_value());
  CHECK(sheet->matk == -32768);
}

TEST_CASE("Homoncule : la fiche refuse une longueur ou un opcode étranger", "[proto][homunculus]") {
  std::vector<std::uint8_t> frame = AmistrSheet();
  frame.pop_back();
  CHECK_FALSE(DecodeHomunculusProperties(frame).has_value());
  std::vector<std::uint8_t> other = AmistrSheet();
  PutU16(other, 0, 0x0B76);
  CHECK_FALSE(DecodeHomunculusProperties(other).has_value());
}

TEST_CASE("Homoncule : la fiche fait l'aller-retour", "[proto][homunculus]") {
  const auto sheet = DecodeHomunculusProperties(AmistrSheet());
  REQUIRE(sheet.has_value());
  CHECK(ro::proto::EncodeHomunculusProperties(*sheet) == AmistrSheet());
}

TEST_CASE("Homoncule : le champ qui change porte soixante-quatre bits", "[proto][homunculus]") {
  std::vector<std::uint8_t> frame(HomunculusParameter::kWireSize, 0);
  PutU16(frame, 0, 0x0BA5);
  PutU16(frame, 2, ro::proto::kHomunculusParameterExp);
  PutU64(frame, 4, 0x123456789ULL);
  const auto changed = DecodeHomunculusParameter(frame);
  REQUIRE(changed.has_value());
  CHECK(changed->type == 1);
  CHECK(changed->value == 0x123456789ULL);
  CHECK(ro::proto::EncodeHomunculusParameter(*changed) == frame);
  frame.pop_back();
  CHECK_FALSE(DecodeHomunculusParameter(frame).has_value());
}

TEST_CASE("Homoncule : nourrir part en 0x022D, type nul puis commande un", "[proto][homunculus]") {
  const auto frame =
      ro::proto::Encode(HomunculusCommandRequest{.command = HomunculusCommand::kFeed});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x2D, 0x02, 0x00, 0x00, 0x01});
  const auto deleted =
      ro::proto::Encode(HomunculusCommandRequest{.command = HomunculusCommand::kDelete});
  REQUIRE(deleted.has_value());
  CHECK(deleted->back() == 0x02);
}

TEST_CASE("Homoncule : le renommage part en 0x0231 sur vingt-quatre octets",
          "[proto][homunculus]") {
  const auto frame = ro::proto::Encode(HomunculusRenameRequest{.name = "Lif"});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == HomunculusRenameRequest::kWireSize);
  CHECK((*frame)[0] == 0x31);
  CHECK((*frame)[1] == 0x02);
  CHECK((*frame)[2] == 'L');
  CHECK((*frame)[4] == 'f');
  CHECK((*frame)[5] == 0);
  CHECK(frame->back() == 0);
}
