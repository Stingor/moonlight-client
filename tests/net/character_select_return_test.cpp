// LE RETOUR À LA SÉLECTION DE PERSONNAGE : ce qui part, et ce que le serveur
// peut répondre.

#include "ro/net/character_select_return.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"

using ro::net::CharacterSelectAnswer;
using ro::net::CharacterSelectAnswerOf;
using ro::net::Frame;

namespace {

// `ZC_RESTART_ACK` 0x00B3, puis son type.
[[nodiscard]] Frame RestartAck(std::uint8_t type) {
  return Frame{.opcode = 0x00B3, .bytes = {0xB3, 0x00, type}};
}

// `ZC_ACK_REQ_DISCONNECT` 0x018B, puis un résultat sur deux octets.
[[nodiscard]] Frame DisconnectAck(std::uint8_t result) {
  return Frame{.opcode = 0x018B, .bytes = {0x8B, 0x01, result, 0x00}};
}

}  // namespace

TEST_CASE("Retour à la sélection : la demande est CZ_RESTART de type 1", "[net][restart]") {
  // ⚠ LE TYPE ZÉRO EST LA RÉAPPARITION D'UN MORT : se tromper de type ferait
  // ressusciter au point de sauvegarde au lieu de revenir à la tablée.
  ro::proto::RestartRequest request;
  request.what = ro::proto::RestartRequest::kCharacterSelect;
  const auto frame = ro::proto::Encode(request);
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xB2, 0x00, 0x01});
}

TEST_CASE("Retour à la sélection : l'accord et ses refus", "[net][restart]") {
  CHECK(CharacterSelectAnswerOf(RestartAck(1)) == CharacterSelectAnswer::kGranted);
  // Zéro : le serveur de personnages ne répondait pas.
  CHECK(CharacterSelectAnswerOf(RestartAck(0)) == CharacterSelectAnswer::kRefused);
  // Un combat récent, un personnage caché.
  CHECK(CharacterSelectAnswerOf(DisconnectAck(1)) == CharacterSelectAnswer::kRefused);
  // ⚠ LE ZÉRO DE CE PAQUET-LÀ ACCORDE UNE DÉCONNEXION : ce n'est pas une réponse
  // à notre demande.
  CHECK(CharacterSelectAnswerOf(DisconnectAck(0)) == CharacterSelectAnswer::kNone);
}

TEST_CASE("Retour à la sélection : le reste ne répond à rien", "[net][restart]") {
  CHECK(CharacterSelectAnswerOf(Frame{.opcode = 0x0080, .bytes = {0x80, 0x00}}) ==
        CharacterSelectAnswer::kNone);
  // Une trame tronquée ne se lit pas comme un refus.
  CHECK(CharacterSelectAnswerOf(Frame{.opcode = 0x018B, .bytes = {0x8B, 0x01}}) ==
        CharacterSelectAnswer::kNone);
}
