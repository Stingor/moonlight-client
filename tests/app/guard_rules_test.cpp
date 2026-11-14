// Les règles du garde : les noms connus, la provenance des touches, l'accord
// des horloges, et la cadence du porte-parole.

#include "guard/guard_rules.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/cheat_report.h"

using moonclient::ClockAgreement;
using moonclient::GuardFinding;
using moonclient::GuardProbe;
using moonclient::GuardSignature;
using moonclient::GuardSpokesman;
using moonclient::KeyProvenance;
using moonclient::MatchSignature;

namespace {

constexpr std::uint64_t kSecondNs = 1'000'000'000;

void Press(KeyProvenance& keys, std::uint32_t count, bool raw, bool has_device, bool held) {
  for (std::uint32_t index = 0; index < count; ++index) {
    if (raw) {
      keys.NoteRawKey(has_device);
    }
    keys.NoteKeyMessage(held);
  }
}

}  // namespace

TEST_CASE("un nom connu est reconnu sans égard à la casse", "[app][guard]") {
  // Une table fictive : la vraie reste hors du dépôt.
  constexpr GuardSignature kTable[] = {
      {GuardProbe::kProcess, "Macro", "macro.exe"},
      {GuardProbe::kProcessSubstring, "Memoire", "memoire"},
      {GuardProbe::kWindowClass, "Macro", "macrowindow"},
  };

  const auto exact = MatchSignature(kTable, GuardProbe::kProcess, "Macro.EXE");
  REQUIRE(exact.has_value());
  CHECK(exact->tool == "Macro");
  CHECK(exact->detail == "Macro.EXE");

  // Un nom suffixé selon le processeur se reconnaît par ce qu'il contient.
  const auto suffixed = MatchSignature(kTable, GuardProbe::kProcess, "memoire-x86_64-AVX2.exe");
  REQUIRE(suffixed.has_value());
  CHECK(suffixed->tool == "Memoire");

  // Le nom exact d'un processus ne se cherche pas en sous-chaîne.
  CHECK_FALSE(MatchSignature(kTable, GuardProbe::kProcess, "notmacro.exe.bak").has_value());
  CHECK_FALSE(MatchSignature(kTable, GuardProbe::kProcess, "moonclient.exe").has_value());

  // Une sonde ne lit que ses propres signatures.
  CHECK(MatchSignature(kTable, GuardProbe::kWindowClass, "MacroWindowGUI").has_value());
  CHECK_FALSE(MatchSignature(kTable, GuardProbe::kModule, "MacroWindowGUI").has_value());
}

TEST_CASE("des touches du clavier ne disent rien", "[app][guard]") {
  KeyProvenance keys;
  Press(keys, moonclient::kKeysPerVerdict, true, true, true);
  CHECK_FALSE(keys.TakeVerdict().has_value());
}

TEST_CASE("un lot incomplet ne se juge pas", "[app][guard]") {
  KeyProvenance keys;
  Press(keys, moonclient::kKeysPerVerdict - 1, true, false, true);
  CHECK_FALSE(keys.TakeVerdict().has_value());
}

TEST_CASE("des touches injectées se signalent", "[app][guard]") {
  KeyProvenance keys;
  Press(keys, moonclient::kKeysPerVerdict, true, false, true);
  const auto verdict = keys.TakeVerdict();
  REQUIRE(verdict.has_value());
  CHECK(verdict->tool == "SendInput");
  // Le lot repart de zéro.
  CHECK_FALSE(keys.TakeVerdict().has_value());
}

TEST_CASE("des touches postées à la fenêtre se signalent", "[app][guard]") {
  KeyProvenance keys;
  // Postées : aucune entrée brute, et le système ignore que la touche est
  // enfoncée.
  Press(keys, moonclient::kKeysPerVerdict, false, false, false);
  const auto verdict = keys.TakeVerdict();
  REQUIRE(verdict.has_value());
  CHECK(verdict->tool == "PostMessage");
}

TEST_CASE("sans entrée brute, aucune part injectée ne se mesure", "[app][guard]") {
  KeyProvenance keys;
  Press(keys, moonclient::kKeysPerVerdict, false, false, true);
  CHECK_FALSE(keys.TakeVerdict().has_value());
}

TEST_CASE("quelques touches lues en retard ne font pas un lot posté", "[app][guard]") {
  KeyProvenance keys;
  Press(keys, moonclient::kKeysPerVerdict - 4, true, true, true);
  Press(keys, 4, true, true, false);
  CHECK_FALSE(keys.TakeVerdict().has_value());
}

TEST_CASE("deux horloges d'accord ne disent rien", "[app][guard]") {
  ClockAgreement clocks;
  for (std::uint64_t second = 0; second <= 300; ++second) {
    CHECK_FALSE(clocks.Note(second * kSecondNs + 7, second * kSecondNs).has_value());
  }
}

TEST_CASE("un compteur accéléré se signale après deux fenêtres", "[app][guard]") {
  ClockAgreement clocks;
  int windows_until_verdict = 0;
  std::optional<GuardFinding> verdict;
  for (std::uint64_t second = 0; second <= 120 && !verdict; ++second) {
    verdict = clocks.Note(second * kSecondNs * 3 / 2, second * kSecondNs);
    if (second > 0 && second % 30 == 0) {
      ++windows_until_verdict;
    }
  }
  REQUIRE(verdict.has_value());
  CHECK(verdict->tool == "Speedhack");
  CHECK(verdict->detail == "compteur a x1.500 du noyau");
  CHECK(windows_until_verdict == static_cast<int>(moonclient::kClockStrikes));
}

TEST_CASE("une suspension ne compte pas comme un désaccord", "[app][guard]") {
  ClockAgreement clocks;
  CHECK_FALSE(clocks.Note(0, 0).has_value());
  // Le noyau a avancé de dix minutes pendant que le compteur, lui, dormait.
  CHECK_FALSE(clocks.Note(30 * kSecondNs, 600 * kSecondNs).has_value());
  CHECK_FALSE(clocks.Note(60 * kSecondNs, 630 * kSecondNs).has_value());
  CHECK_FALSE(clocks.Note(90 * kSecondNs, 660 * kSecondNs).has_value());
}

TEST_CASE("le porte-parole attend une session reconnue", "[app][guard]") {
  GuardSpokesman spokesman;
  CHECK(spokesman.Note({.tool = "A", .detail = "a"}));
  CHECK_FALSE(spokesman.Note({.tool = "A", .detail = "a"}));
  CHECK_FALSE(spokesman.Due(false, 0).has_value());
  const auto first = spokesman.Due(true, 0);
  REQUIRE(first.has_value());
  CHECK(first->tool == "A");
  CHECK_FALSE(spokesman.Due(true, 1).has_value());
}

TEST_CASE("le porte-parole tient la cadence du serveur", "[app][guard]") {
  GuardSpokesman spokesman;
  spokesman.Note({.tool = "A", .detail = "a"});
  spokesman.Note({.tool = "B", .detail = "b"});
  REQUIRE(spokesman.Due(true, 1000).has_value());
  CHECK_FALSE(spokesman.Due(true, 1000 + ro::net::kCheatReportIntervalMs).has_value());
  const auto second =
      spokesman.Due(true, 1000 + ro::net::kCheatReportIntervalMs + moonclient::kReportMarginMs);
  REQUIRE(second.has_value());
  CHECK(second->tool == "B");
}

TEST_CASE("une session neuve réentend tout", "[app][guard]") {
  GuardSpokesman spokesman;
  spokesman.Note({.tool = "A", .detail = "a"});
  REQUIRE(spokesman.Due(true, 0).has_value());
  // Une entrée en zone : la session tombe, puis se fait reconnaître à nouveau.
  CHECK_FALSE(spokesman.Due(false, 10).has_value());
  constexpr std::uint32_t kLater = 200'000;
  const auto again = spokesman.Due(true, kLater);
  REQUIRE(again.has_value());
  CHECK(again->tool == "A");
}

TEST_CASE("le porte-parole ne retient qu'un nombre borné de trouvailles", "[app][guard]") {
  GuardSpokesman spokesman;
  for (std::size_t index = 0; index < moonclient::kMaxFindings; ++index) {
    CHECK(spokesman.Note({.tool = "T", .detail = std::to_string(index)}));
  }
  CHECK_FALSE(spokesman.Note({.tool = "T", .detail = "de trop"}));
  CHECK(spokesman.known() == moonclient::kMaxFindings);
}
