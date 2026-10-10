// La file d'inscription au champ de bataille. Voir l'en-tête.

#include "ro/sim/entry_queue.h"

#include <utility>

namespace ro::sim {
namespace {

// Les réponses du serveur à une inscription. Le code 0 refuse sans dire
// pourquoi : la raison arrive à part, dans la conversation.
constexpr std::uint8_t kApplyRefusedElsewhere = 0;
constexpr std::uint8_t kApplyAccepted = 1;
constexpr std::uint8_t kApplyQueueFinished = 2;
constexpr std::uint8_t kApplyUnknownName = 3;
constexpr std::uint8_t kApplyUnknownType = 4;
constexpr std::uint8_t kApplyTooMany = 5;
constexpr std::uint8_t kApplyLevel = 6;
constexpr std::uint8_t kApplyDuplicate = 7;
constexpr std::uint8_t kApplyReconnect = 8;
constexpr std::uint8_t kApplyLeaderOnly = 9;
constexpr std::uint8_t kApplyJobRefused = 10;

// La seule réponse à « j'entre » qui fasse entrer.
constexpr std::uint8_t kAdmissionGranted = 1;

// L'avis d'un refus d'inscription. Un code que cette version ne connaît pas se
// lit comme un refus sans raison donnée : la conversation en dira peut-être
// davantage.
[[nodiscard]] EntryQueueNoticeKind RefusalOf(std::uint8_t code) noexcept {
  switch (code) {
    case kApplyQueueFinished:
      return EntryQueueNoticeKind::kQueueFinished;
    case kApplyUnknownName:
      return EntryQueueNoticeKind::kUnknownName;
    case kApplyUnknownType:
      return EntryQueueNoticeKind::kUnknownType;
    case kApplyTooMany:
      return EntryQueueNoticeKind::kTooMany;
    case kApplyLevel:
      return EntryQueueNoticeKind::kLevel;
    case kApplyDuplicate:
      return EntryQueueNoticeKind::kDuplicate;
    case kApplyReconnect:
      return EntryQueueNoticeKind::kReconnect;
    case kApplyLeaderOnly:
      return EntryQueueNoticeKind::kLeaderOnly;
    case kApplyJobRefused:
      return EntryQueueNoticeKind::kJobRefused;
    case kApplyRefusedElsewhere:
    default:
      return EntryQueueNoticeKind::kRefusedElsewhere;
  }
}

// Le temps écoulé de `since_ms` à `now_ms`, nul si l'instant donné précède le
// départ : un appelant qui recule ne fait pas déborder la soustraction.
[[nodiscard]] std::uint32_t ElapsedMs(std::uint32_t since_ms, std::uint32_t now_ms) noexcept {
  return now_ms >= since_ms ? now_ms - since_ms : 0;
}

// Passe à `next`, et compte la transition quand la phase change vraiment.
void MoveTo(EntryQueuePhase& phase, std::uint32_t& serial, EntryQueuePhase next) noexcept {
  if (phase != next) {
    phase = next;
    ++serial;
  }
}

}  // namespace

void EntryQueue::NoteApplied(std::string field_name, EntryQueueApplyKind kind,
                             std::uint32_t now_ms) {
  // Déjà en file, une nouvelle demande ne change rien ici : le serveur la
  // refusera comme un doublon, et la place gardée reste la bonne.
  if (queued()) {
    return;
  }
  field_name_ = std::move(field_name);
  apply_kind_ = kind;
  count_ = kEntryQueueCountUnknown;
  // Une demande refaite pendant l'attente repart de zéro : c'est la dernière
  // partie qui attend sa réponse.
  applied_at_ms_ = now_ms;
  MoveTo(phase_, phase_serial_, EntryQueuePhase::kApplying);
}

void EntryQueue::TellApplyResult(std::uint8_t code, std::string field_name, std::uint32_t now_ms) {
  if (code == kApplyAccepted) {
    // ⚠ SANS DEMANDE DE NOTRE PART AUSSI : un chef qui inscrit son groupe ou sa
    // guilde y met chacun de ses membres, qui l'apprennent par cette réponse.
    if (!field_name.empty()) {
      field_name_ = std::move(field_name);
    }
    if (!queued()) {
      queued_since_ms_ = now_ms;
      count_ = kEntryQueueCountUnknown;
    }
    // Le serveur dit l'effectif juste après : la prochaine demande attend une
    // période entière.
    ranking_asked_ms_ = now_ms;
    sounds_.push_back(EntryQueueSound::kRequest);
    MoveTo(phase_, phase_serial_, EntryQueuePhase::kQueued);
    return;
  }

  notices_.push_back(EntryQueueNotice{.kind = RefusalOf(code), .field_name = field_name});
  // UN REFUS RÉPOND À LA DEMANDE EN COURS, pas à la place déjà tenue : un
  // doublon refusé laisse en file. Seule la fin de la file en fait sortir.
  if (code == kApplyQueueFinished || !queued()) {
    Reset();
  }
}

void EntryQueue::TellCount(std::string field_name, std::int32_t count, std::uint32_t /*now_ms*/) {
  if (phase_ == EntryQueuePhase::kIdle) {
    return;
  }
  if (!field_name.empty()) {
    field_name_ = std::move(field_name);
  }
  count_ = count;
}

void EntryQueue::NoteCancelSent() {
  if (phase_ == EntryQueuePhase::kQueued) {
    MoveTo(phase_, phase_serial_, EntryQueuePhase::kCancelling);
  }
}

void EntryQueue::TellCancelResult(bool success, std::string field_name) {
  if (success) {
    notices_.push_back(
        EntryQueueNotice{.kind = EntryQueueNoticeKind::kCancelled, .field_name = field_name});
    Reset();
    return;
  }
  notices_.push_back(
      EntryQueueNotice{.kind = EntryQueueNoticeKind::kCancelRefused, .field_name = field_name});
  if (phase_ == EntryQueuePhase::kCancelling) {
    MoveTo(phase_, phase_serial_, EntryQueuePhase::kQueued);
  }
}

void EntryQueue::TellInvitation(std::string field_name, std::string lobby, std::uint32_t now_ms) {
  if (!queued()) {
    queued_since_ms_ = now_ms;
  }
  if (!field_name.empty()) {
    field_name_ = std::move(field_name);
  }
  lobby_name_ = std::move(lobby);
  invited_at_ms_ = now_ms;
  sounds_.push_back(EntryQueueSound::kReady);
  MoveTo(phase_, phase_serial_, EntryQueuePhase::kInvited);
}

void EntryQueue::NoteInvitationAnswered(bool accept, std::uint32_t /*now_ms*/) {
  if (phase_ != EntryQueuePhase::kInvited) {
    return;
  }
  if (accept) {
    MoveTo(phase_, phase_serial_, EntryQueuePhase::kAccepted);
    return;
  }
  notices_.push_back(
      EntryQueueNotice{.kind = EntryQueueNoticeKind::kDeclined, .field_name = field_name_});
  Reset();
}

void EntryQueue::TellAdmission(std::uint8_t result, std::string map_name) {
  if (result != kAdmissionGranted) {
    return;
  }
  sounds_.push_back(EntryQueueSound::kForward);
  notices_.push_back(
      EntryQueueNotice{.kind = EntryQueueNoticeKind::kAdmitted, .field_name = std::move(map_name)});
}

void EntryQueue::Reset() {
  // ⚠ LES AVIS ET LES SONS EN ATTENTE RESTENT : la fin de file arrive avec son
  // avis, juste avant cette remise à zéro, et l'étage ne l'a pas encore lu. Le
  // compteur de transitions ne recule jamais.
  std::vector<EntryQueueNotice> notices = std::move(notices_);
  std::vector<EntryQueueSound> sounds = std::move(sounds_);
  std::uint32_t serial = phase_serial_;
  EntryQueuePhase phase = phase_;
  *this = EntryQueue();
  notices_ = std::move(notices);
  sounds_ = std::move(sounds);
  phase_serial_ = serial;
  phase_ = phase;
  MoveTo(phase_, phase_serial_, EntryQueuePhase::kIdle);
}

void EntryQueue::Advance(std::uint32_t now_ms) {
  if (phase_ != EntryQueuePhase::kApplying ||
      ElapsedMs(applied_at_ms_, now_ms) < kEntryQueueApplyTimeoutMs) {
    return;
  }
  notices_.push_back(
      EntryQueueNotice{.kind = EntryQueueNoticeKind::kNoAnswer, .field_name = field_name_});
  Reset();
}

bool EntryQueue::queued() const noexcept {
  switch (phase_) {
    case EntryQueuePhase::kQueued:
    case EntryQueuePhase::kCancelling:
    case EntryQueuePhase::kInvited:
    case EntryQueuePhase::kAccepted:
      return true;
    case EntryQueuePhase::kIdle:
    case EntryQueuePhase::kApplying:
      return false;
  }
  return false;
}

std::uint32_t EntryQueue::waited_ms(std::uint32_t now_ms) const noexcept {
  return queued() ? ElapsedMs(queued_since_ms_, now_ms) : 0;
}

std::uint32_t EntryQueue::invitation_ms_left(std::uint32_t now_ms) const noexcept {
  if (phase_ != EntryQueuePhase::kInvited) {
    return 0;
  }
  const std::uint32_t elapsed_ms = ElapsedMs(invited_at_ms_, now_ms);
  return elapsed_ms >= kEntryQueueInvitationMs ? 0 : kEntryQueueInvitationMs - elapsed_ms;
}

bool EntryQueue::RankingDue(std::uint32_t now_ms) const noexcept {
  return phase_ == EntryQueuePhase::kQueued &&
         ElapsedMs(ranking_asked_ms_, now_ms) >= kEntryQueueRankingPeriodMs;
}

void EntryQueue::NoteRankingAsked(std::uint32_t now_ms) noexcept { ranking_asked_ms_ = now_ms; }

std::vector<EntryQueueNotice> EntryQueue::TakeNotices() { return std::exchange(notices_, {}); }

std::vector<EntryQueueSound> EntryQueue::TakeSounds() { return std::exchange(sounds_, {}); }

}  // namespace ro::sim
