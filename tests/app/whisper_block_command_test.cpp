// `/ex`, `/in`, `/exall`, `/inall` : ce que la ligne tapée demande.

#include "chat/whisper_block_command.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::ReadWhisperBlockCommand;
using moonclient::WhisperBlockCommand;
using moonclient::WhisperBlockCommandKind;

TEST_CASE("Blocage : chaque commande et son nom", "[app][chat][whisper_block]") {
  const WhisperBlockCommand block = ReadWhisperBlockCommand("/ex Aurélie");
  CHECK(block.kind == WhisperBlockCommandKind::kBlock);
  CHECK(block.name == "Aurélie");
  const WhisperBlockCommand allow = ReadWhisperBlockCommand("/IN  Jean Paul ");
  CHECK(allow.kind == WhisperBlockCommandKind::kAllow);
  // Un nom peut porter une espace, jamais en tête ni en queue.
  CHECK(allow.name == "Jean Paul");
  CHECK(ReadWhisperBlockCommand("/exall").kind == WhisperBlockCommandKind::kBlockEveryone);
  CHECK(ReadWhisperBlockCommand("/inall").kind == WhisperBlockCommandKind::kAllowEveryone);
}

TEST_CASE("Blocage : sans nom, /ex redemande la liste et /in se dit",
          "[app][chat][whisper_block]") {
  CHECK(ReadWhisperBlockCommand("/ex").kind == WhisperBlockCommandKind::kList);
  CHECK(ReadWhisperBlockCommand("/ex   ").kind == WhisperBlockCommandKind::kList);
  CHECK(ReadWhisperBlockCommand("/in").kind == WhisperBlockCommandKind::kAllowWithoutName);
}

TEST_CASE("Blocage : la commande s'arrête à l'espace", "[app][chat][whisper_block]") {
  CHECK(ReadWhisperBlockCommand("/exit").kind == WhisperBlockCommandKind::kNone);
  CHECK(ReadWhisperBlockCommand("/invite Bob").kind == WhisperBlockCommandKind::kNone);
  CHECK(ReadWhisperBlockCommand("ex Bob").kind == WhisperBlockCommandKind::kNone);
}
