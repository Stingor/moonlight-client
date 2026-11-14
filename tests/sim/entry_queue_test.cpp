// LA FILE D'INSCRIPTION AU CHAMP DE BATAILLE : ses phases, ses avis, ses sons
// et ses délais, sans horloge.

#include "ro/sim/entry_queue.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::EntryQueue;
using ro::sim::EntryQueueApplyKind;
using ro::sim::EntryQueueNoticeKind;
using ro::sim::EntryQueuePhase;
using ro::sim::EntryQueueSound;
using ro::sim::kEntryQueueInvitationMs;
using ro::sim::kEntryQueueRankingPeriodMs;

namespace {

constexpr const char* kField = "Tierra Gorge";
constexpr const char* kMap = "bat_a01";
constexpr std::uint32_t kAppliedMs = 1'000;
constexpr std::uint32_t kQueuedMs = 1'200;
constexpr std::uint32_t kInvitedMs = 60'000;
constexpr std::int32_t kSixInQueue = 6;

// Les codes de réponse du serveur à une inscription.
constexpr std::uint8_t kRefusedElsewhere = 0;
constexpr std::uint8_t kAccepted = 1;
constexpr std::uint8_t kQueueFinished = 2;
constexpr std::uint8_t kLevel = 6;
constexpr std::uint8_t kDuplicate = 7;
constexpr std::uint8_t kJobRefused = 10;
constexpr std::uint8_t kUnknownToThisVersion = 42;

// Inscrit seul, et accepté en file.
[[nodiscard]] EntryQueue Queued() {
  EntryQueue queue;
  queue.NoteApplied(kField, EntryQueueApplyKind::kSolo, kAppliedMs);
  queue.TellApplyResult(kAccepted, kField, kQueuedMs);
  (void)queue.TakeSounds();
  (void)queue.TakeNotices();
  return queue;
}

// En file, puis invité.
[[nodiscard]] EntryQueue Invited() {
  EntryQueue queue = Queued();
  queue.TellInvitation(kField, kField, kInvitedMs);
  (void)queue.TakeSounds();
  return queue;
}

}  // namespace

TEST_CASE("File du champ : neuve, elle est hors file et sans effectif connu",
          "[sim][battleground]") {
  EntryQueue queue;
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
  CHECK_FALSE(queue.queued());
  CHECK(queue.count() == ro::sim::kEntryQueueCountUnknown);
  CHECK(queue.waited_ms(kQueuedMs) == 0);
  CHECK_FALSE(queue.RankingDue(kEntryQueueRankingPeriodMs * 3));
}

TEST_CASE("File du champ : la demande attend sa réponse, l'acceptation met en file",
          "[sim][battleground]") {
  EntryQueue queue;
  const std::uint32_t before = queue.phase_serial();
  queue.NoteApplied(kField, EntryQueueApplyKind::kGuild, kAppliedMs);
  CHECK(queue.phase() == EntryQueuePhase::kApplying);
  CHECK_FALSE(queue.queued());
  CHECK(queue.apply_kind() == EntryQueueApplyKind::kGuild);
  CHECK(queue.phase_serial() == before + 1);

  queue.TellApplyResult(kAccepted, kField, kQueuedMs);
  CHECK(queue.phase() == EntryQueuePhase::kQueued);
  CHECK(queue.queued());
  CHECK(queue.field_name() == kField);
  CHECK(queue.phase_serial() == before + 2);
  CHECK(queue.TakeSounds() == std::vector{EntryQueueSound::kRequest});
  // L'acceptation n'est pas un avis : la fenêtre d'attente suffit.
  CHECK(queue.TakeNotices().empty());

  queue.TellCount(kField, kSixInQueue, kQueuedMs);
  CHECK(queue.count() == kSixInQueue);
}

TEST_CASE("File du champ : un membre inscrit par son chef entre en file sans rien demander",
          "[sim][battleground]") {
  EntryQueue queue;
  queue.TellApplyResult(kAccepted, kField, kQueuedMs);
  CHECK(queue.phase() == EntryQueuePhase::kQueued);
  CHECK(queue.field_name() == kField);
  CHECK(queue.waited_ms(kQueuedMs + 5'000) == 5'000);
}

TEST_CASE("File du champ : chaque refus a son avis et fait sortir de la demande",
          "[sim][battleground]") {
  struct Case {
    std::uint8_t code;
    EntryQueueNoticeKind kind;
  };
  const std::vector<Case> cases = {
      {kRefusedElsewhere, EntryQueueNoticeKind::kRefusedElsewhere},
      {kQueueFinished, EntryQueueNoticeKind::kQueueFinished},
      {3, EntryQueueNoticeKind::kUnknownName},
      {4, EntryQueueNoticeKind::kUnknownType},
      {5, EntryQueueNoticeKind::kTooMany},
      {kLevel, EntryQueueNoticeKind::kLevel},
      {kDuplicate, EntryQueueNoticeKind::kDuplicate},
      {8, EntryQueueNoticeKind::kReconnect},
      {9, EntryQueueNoticeKind::kLeaderOnly},
      {kJobRefused, EntryQueueNoticeKind::kJobRefused},
      {kUnknownToThisVersion, EntryQueueNoticeKind::kRefusedElsewhere},
  };
  for (const Case& each : cases) {
    EntryQueue queue;
    queue.NoteApplied(kField, EntryQueueApplyKind::kParty, kAppliedMs);
    queue.TellApplyResult(each.code, kField, kQueuedMs);
    CHECK(queue.phase() == EntryQueuePhase::kIdle);
    const std::vector<ro::sim::EntryQueueNotice> notices = queue.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices.front().kind == each.kind);
    CHECK(notices.front().field_name == kField);
    CHECK(queue.TakeSounds().empty());
  }
}

TEST_CASE("File du champ : un doublon refusé laisse en file, la fin de file en sort",
          "[sim][battleground]") {
  EntryQueue queue = Queued();
  queue.TellApplyResult(kDuplicate, kField, kInvitedMs);
  CHECK(queue.phase() == EntryQueuePhase::kQueued);
  CHECK(queue.TakeNotices().size() == 1);

  // L'invitation expirée : « file terminée », puis la remise à zéro.
  queue.TellInvitation(kField, kField, kInvitedMs);
  queue.TellApplyResult(kQueueFinished, kField, kInvitedMs + kEntryQueueInvitationMs);
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
  queue.Reset();
  // L'avis de fin survit à la remise à zéro qui le suit.
  const auto notices = queue.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices.front().kind == EntryQueueNoticeKind::kQueueFinished);
}

TEST_CASE("File du champ : l'annulation réussie fait sortir, refusée elle laisse en file",
          "[sim][battleground]") {
  SECTION("réussie") {
    EntryQueue queue = Queued();
    queue.NoteCancelSent();
    CHECK(queue.phase() == EntryQueuePhase::kCancelling);
    CHECK(queue.queued());
    queue.TellCancelResult(true, kField);
    CHECK(queue.phase() == EntryQueuePhase::kIdle);
    CHECK(queue.field_name().empty());
    CHECK(queue.count() == ro::sim::kEntryQueueCountUnknown);
    const auto notices = queue.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices.front().kind == EntryQueueNoticeKind::kCancelled);
  }
  SECTION("refusée") {
    EntryQueue queue = Queued();
    queue.NoteCancelSent();
    queue.TellCancelResult(false, kField);
    CHECK(queue.phase() == EntryQueuePhase::kQueued);
    const auto notices = queue.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices.front().kind == EntryQueueNoticeKind::kCancelRefused);
  }
}

TEST_CASE("File du champ : l'invitation sonne, court vingt secondes et s'arrête à zéro",
          "[sim][battleground]") {
  EntryQueue queue = Queued();
  queue.TellInvitation(kField, kField, kInvitedMs);
  CHECK(queue.phase() == EntryQueuePhase::kInvited);
  CHECK(queue.lobby_name() == kField);
  CHECK(queue.TakeSounds() == std::vector{EntryQueueSound::kReady});
  CHECK(queue.invitation_ms_left(kInvitedMs) == kEntryQueueInvitationMs);
  CHECK(queue.invitation_ms_left(kInvitedMs + 5'000) == kEntryQueueInvitationMs - 5'000);
  CHECK(queue.invitation_ms_left(kInvitedMs + kEntryQueueInvitationMs) == 0);
  CHECK(queue.invitation_ms_left(kInvitedMs + kEntryQueueInvitationMs * 2) == 0);
  // Un instant antérieur ne fait pas déborder le décompte.
  CHECK(queue.invitation_ms_left(kInvitedMs - 1) == kEntryQueueInvitationMs);
  // Hors invitation, rien ne court.
  CHECK(Queued().invitation_ms_left(kInvitedMs) == 0);
}

TEST_CASE("File du champ : accepter fait attendre l'entrée, refuser fait sortir",
          "[sim][battleground]") {
  SECTION("accepter") {
    EntryQueue queue = Invited();
    queue.NoteInvitationAnswered(true, kInvitedMs + 1'000);
    CHECK(queue.phase() == EntryQueuePhase::kAccepted);
    CHECK(queue.queued());
    CHECK(queue.invitation_ms_left(kInvitedMs + 1'000) == 0);
    queue.TellAdmission(1, kMap);
    CHECK(queue.TakeSounds() == std::vector{EntryQueueSound::kForward});
    const auto notices = queue.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices.front().kind == EntryQueueNoticeKind::kAdmitted);
    CHECK(notices.front().field_name == kMap);
  }
  SECTION("refuser") {
    EntryQueue queue = Invited();
    queue.NoteInvitationAnswered(false, kInvitedMs + 1'000);
    CHECK(queue.phase() == EntryQueuePhase::kIdle);
    const auto notices = queue.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices.front().kind == EntryQueueNoticeKind::kDeclined);
  }
  SECTION("répondre sans invitation ne fait rien") {
    EntryQueue queue = Queued();
    queue.NoteInvitationAnswered(true, kInvitedMs);
    CHECK(queue.phase() == EntryQueuePhase::kQueued);
  }
  SECTION("une entrée refusée ne sonne pas") {
    EntryQueue queue = Invited();
    queue.TellAdmission(0, kMap);
    CHECK(queue.TakeSounds().empty());
    CHECK(queue.TakeNotices().empty());
  }
}

TEST_CASE("File du champ : la remise à zéro vide tout, sans avis, et compte la transition",
          "[sim][battleground]") {
  EntryQueue queue = Invited();
  queue.TellCount(kField, kSixInQueue, kInvitedMs);
  const std::uint32_t before = queue.phase_serial();
  queue.Reset();
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
  CHECK(queue.phase_serial() == before + 1);
  CHECK(queue.field_name().empty());
  CHECK(queue.lobby_name().empty());
  CHECK(queue.count() == ro::sim::kEntryQueueCountUnknown);
  CHECK(queue.waited_ms(kInvitedMs) == 0);
  CHECK(queue.TakeNotices().empty());
  // Hors file, une seconde remise à zéro n'est pas une transition.
  queue.Reset();
  CHECK(queue.phase_serial() == before + 1);
}

TEST_CASE("File du champ : l'effectif se redemande toutes les dix secondes, en file seulement",
          "[sim][battleground]") {
  EntryQueue queue = Queued();
  CHECK_FALSE(queue.RankingDue(kQueuedMs));
  CHECK_FALSE(queue.RankingDue(kQueuedMs + kEntryQueueRankingPeriodMs - 1));
  CHECK(queue.RankingDue(kQueuedMs + kEntryQueueRankingPeriodMs));
  queue.NoteRankingAsked(kQueuedMs + kEntryQueueRankingPeriodMs);
  CHECK_FALSE(queue.RankingDue(kQueuedMs + kEntryQueueRankingPeriodMs + 1));
  // Pendant l'invitation, on ne redemande rien.
  queue.TellInvitation(kField, kField, kInvitedMs);
  CHECK_FALSE(queue.RankingDue(kInvitedMs + kEntryQueueRankingPeriodMs * 3));
}

TEST_CASE("File du champ : l'attente se compte depuis l'entrée en file", "[sim][battleground]") {
  EntryQueue queue = Queued();
  CHECK(queue.waited_ms(kQueuedMs + 61'000) == 61'000);
  // Un instant antérieur ne fait pas déborder la soustraction.
  CHECK(queue.waited_ms(kQueuedMs - 1) == 0);
  // L'invitation ne remet pas l'attente à zéro.
  queue.TellInvitation(kField, kField, kInvitedMs);
  CHECK(queue.waited_ms(kInvitedMs) == kInvitedMs - kQueuedMs);
}

TEST_CASE("File du champ : une demande restée sans réponse ressort d'elle-même, avec un avis",
          "[sim][battleground]") {
  using ro::sim::kEntryQueueApplyTimeoutMs;
  EntryQueue queue;
  queue.NoteApplied(kField, EntryQueueApplyKind::kSolo, kAppliedMs);
  const std::uint32_t applying_serial = queue.phase_serial();

  // Juste avant l'échéance, on attend encore.
  queue.Advance(kAppliedMs + kEntryQueueApplyTimeoutMs - 1);
  CHECK(queue.phase() == EntryQueuePhase::kApplying);
  CHECK(queue.TakeNotices().empty());
  // Un instant antérieur à la demande ne la fait pas échoir.
  queue.Advance(kAppliedMs - 1);
  CHECK(queue.phase() == EntryQueuePhase::kApplying);

  queue.Advance(kAppliedMs + kEntryQueueApplyTimeoutMs);
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
  CHECK(queue.phase_serial() == applying_serial + 1);
  const std::vector<ro::sim::EntryQueueNotice> notices = queue.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices.front().kind == EntryQueueNoticeKind::kNoAnswer);
  CHECK(notices.front().field_name == kField);
  CHECK(queue.TakeSounds().empty());
  // Une fois sortie, le temps qui passe ne redit rien.
  queue.Advance(kAppliedMs + kEntryQueueApplyTimeoutMs * 2);
  CHECK(queue.TakeNotices().empty());
}

TEST_CASE("File du champ : l'échéance ne touche qu'une demande en cours", "[sim][battleground]") {
  using ro::sim::kEntryQueueApplyTimeoutMs;
  // En file, l'attente n'a pas de borne.
  EntryQueue queue = Queued();
  queue.Advance(kQueuedMs + kEntryQueueApplyTimeoutMs * 100);
  CHECK(queue.phase() == EntryQueuePhase::kQueued);
  CHECK(queue.TakeNotices().empty());

  // Une demande refaite repart de zéro.
  EntryQueue again;
  again.NoteApplied(kField, EntryQueueApplyKind::kSolo, kAppliedMs);
  again.NoteApplied(kField, EntryQueueApplyKind::kSolo, kQueuedMs);
  again.Advance(kAppliedMs + kEntryQueueApplyTimeoutMs);
  CHECK(again.phase() == EntryQueuePhase::kApplying);
  again.Advance(kQueuedMs + kEntryQueueApplyTimeoutMs);
  CHECK(again.phase() == EntryQueuePhase::kIdle);
}
