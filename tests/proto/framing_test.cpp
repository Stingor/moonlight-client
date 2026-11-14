#include "ro/proto/framing.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::FrameStatus;
using ro::proto::kOpcodeBytes;
using ro::proto::kVariableHeaderBytes;
using ro::proto::NextFrame;
using ro::proto::PacketLength;

namespace {

// 0x0064, la demande de connexion : 55 octets, longueur FIXE.
constexpr std::uint16_t kFixedOpcode = 0x0064;
constexpr std::size_t kFixedSize = 55;

// 0x0069, l'acceptation de connexion : longueur VARIABLE.
constexpr std::uint16_t kVariableOpcode = 0x0069;

// Un tampon commençant par l'opcode donné, puis des octets quelconques.
std::vector<std::uint8_t> Buffer(std::uint16_t opcode, std::size_t total) {
  std::vector<std::uint8_t> bytes(total, 0xAB);
  if (total >= 1) {
    bytes[0] = static_cast<std::uint8_t>(opcode & 0xFF);
  }
  if (total >= 2) {
    bytes[1] = static_cast<std::uint8_t>(opcode >> 8);
  }
  return bytes;
}

// Le même, dont les deux octets suivant l'opcode annoncent une longueur.
std::vector<std::uint8_t> VariableBuffer(std::uint16_t opcode, std::uint16_t announced,
                                         std::size_t total) {
  std::vector<std::uint8_t> bytes = Buffer(opcode, total);
  if (total >= kVariableHeaderBytes) {
    bytes[2] = static_cast<std::uint8_t>(announced & 0xFF);
    bytes[3] = static_cast<std::uint8_t>(announced >> 8);
  }
  return bytes;
}

}  // namespace

TEST_CASE("Découpage : les opcodes d'essai sont bien ceux qu'on croit", "[proto][framing]") {
  // Les autres cas reposent sur ces deux-là. Si la table changeait, mieux vaut
  // que ce test le dise que de voir échouer six cas sans comprendre pourquoi.
  const auto fixed = PacketLength(kFixedOpcode);
  REQUIRE(fixed.has_value());
  CHECK(*fixed == static_cast<std::int16_t>(kFixedSize));

  const auto variable = PacketLength(kVariableOpcode);
  REQUIRE(variable.has_value());
  CHECK(*variable == ro::proto::kVariableLength);
}

TEST_CASE("Découpage : une trame fixe complète est rendue entière", "[proto][framing]") {
  const auto result = NextFrame(Buffer(kFixedOpcode, kFixedSize));
  CHECK(result.status == FrameStatus::kComplete);
  CHECK(result.opcode == kFixedOpcode);
  CHECK(result.size == kFixedSize);
}

TEST_CASE("Découpage : ce qui SUIT la trame ne la rallonge pas", "[proto][framing]") {
  // Un flux arrive par morceaux qui n'ont aucun rapport avec les limites des
  // paquets : deux trames arrivent souvent dans le même bloc. Rendre la taille
  // du tampon au lieu de celle de la trame les collerait ensemble.
  const auto result = NextFrame(Buffer(kFixedOpcode, kFixedSize + 200));
  CHECK(result.status == FrameStatus::kComplete);
  CHECK(result.size == kFixedSize);
}

TEST_CASE("Découpage : une trame fixe incomplète attend", "[proto][framing]") {
  // Ce n'est pas une erreur, c'est le cas ordinaire d'un flux TCP.
  const auto result = NextFrame(Buffer(kFixedOpcode, kFixedSize - 1));
  CHECK(result.status == FrameStatus::kIncomplete);
  CHECK(result.opcode == kFixedOpcode);
  CHECK(result.size == 0);
}

TEST_CASE("Découpage : une trame variable lit sa longueur après l'opcode", "[proto][framing]") {
  const auto result = NextFrame(VariableBuffer(kVariableOpcode, 40, 40));
  CHECK(result.status == FrameStatus::kComplete);
  CHECK(result.opcode == kVariableOpcode);
  CHECK(result.size == 40);
}

TEST_CASE("Découpage : la longueur annoncée COMPTE l'en-tête", "[proto][framing]") {
  // Une trame qui n'annonce que ses quatre octets d'en-tête est légale. La
  // compter en plus de l'en-tête ferait attendre quatre octets qui ne
  // viendront jamais.
  const auto result =
      NextFrame(VariableBuffer(kVariableOpcode, kVariableHeaderBytes, kVariableHeaderBytes));
  CHECK(result.status == FrameStatus::kComplete);
  CHECK(result.size == kVariableHeaderBytes);
}

TEST_CASE("Découpage : une trame variable dont la longueur manque attend", "[proto][framing]") {
  // Deux octets suffisent pour l'opcode, pas pour savoir combien lire.
  const auto result = NextFrame(Buffer(kVariableOpcode, kOpcodeBytes));
  CHECK(result.status == FrameStatus::kIncomplete);
  CHECK(result.opcode == kVariableOpcode);
}

TEST_CASE("Découpage : une trame variable annoncée trop courte est CORROMPUE", "[proto][framing]") {
  // Une trame ne peut pas être plus courte que ce qui porte sa propre
  // longueur. L'accepter ferait avancer de moins que l'en-tête, et le lecteur
  // tournerait en boucle sur les mêmes octets.
  for (const std::uint16_t announced : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{3}}) {
    const auto result = NextFrame(VariableBuffer(kVariableOpcode, announced, 100));
    CHECK(result.status == FrameStatus::kCorrupt);
  }
}

TEST_CASE("Découpage : un opcode inconnu ne se saute PAS", "[proto][framing]") {
  // Sans sa longueur on ne sait pas de combien avancer. Sauter « au suivant »
  // reviendrait à chercher un opcode au milieu d'un corps de paquet, et à
  // inventer des trames à partir de données.
  const auto result = NextFrame(Buffer(0xFFFF, 200));
  CHECK(result.status == FrameStatus::kUnknownOpcode);
  CHECK(result.opcode == 0xFFFF);
  CHECK(result.size == 0);
}

TEST_CASE("Découpage : un tampon trop court pour l'opcode n'en invente pas", "[proto][framing]") {
  // Laisser l'opcode à zéro tout en le disant lu affirmerait le paquet 0x0000.
  for (const std::size_t size : {std::size_t{0}, std::size_t{1}}) {
    const auto result = NextFrame(std::vector<std::uint8_t>(size, 0x64));
    CHECK(result.status == FrameStatus::kIncomplete);
    CHECK(result.size == 0);
  }
}

TEST_CASE("Découpage : deux trames collées se séparent", "[proto][framing]") {
  // Le cas qui arrive à chaque tour de boucle réseau, et celui où une erreur de
  // taille se paie immédiatement par une désynchronisation.
  std::vector<std::uint8_t> stream = Buffer(kFixedOpcode, kFixedSize);
  const std::vector<std::uint8_t> second = VariableBuffer(kVariableOpcode, 30, 30);
  stream.insert(stream.end(), second.begin(), second.end());

  const auto first = NextFrame(stream);
  REQUIRE(first.status == FrameStatus::kComplete);
  CHECK(first.size == kFixedSize);

  const auto next = NextFrame(std::span(stream).subspan(first.size));
  CHECK(next.status == FrameStatus::kComplete);
  CHECK(next.opcode == kVariableOpcode);
  CHECK(next.size == 30);
}

TEST_CASE("Découpage : un flux livré octet par octet finit par se découper", "[proto][framing]") {
  // La preuve que « incomplet » est bien un état d'attente et non un échec :
  // en ajoutant un octet à la fois, la trame doit apparaître exactement quand
  // le dernier arrive, et pas avant.
  const std::vector<std::uint8_t> complete = Buffer(kFixedOpcode, kFixedSize);
  for (std::size_t available = 0; available < kFixedSize; ++available) {
    const auto partial = NextFrame(std::span(complete).first(available));
    CHECK(partial.status == FrameStatus::kIncomplete);
  }
  CHECK(NextFrame(complete).status == FrameStatus::kComplete);
}
