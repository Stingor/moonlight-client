#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ro::sim {

// ── LA FILE D'INSCRIPTION AU CHAMP DE BATAILLE ──────────────────────────────
//
// Ce que le serveur a dit de notre place dans la file : inscrits, combien en
// file, invités à entrer, entrés. HORS DU MONDE : elle survit aux changements
// de carte, et c'est l'étage de l'application qui la tient.
//
// ⚠ ELLE NE LIT AUCUNE HORLOGE : chaque fait arrive daté par l'appelant, et
// les durées se calculent contre l'instant qu'il donne. Une file rejouée donne
// donc les mêmes délais.
//
// ⚠ LE SERVEUR JUGE TOUT : effectif, niveau, délai après abandon. La file ne
// refuse rien d'elle-même ; elle retient ce qu'on a demandé et ce qui a été
// répondu.

// Le type d'inscription, tel qu'il part sur le fil.
enum class EntryQueueApplyKind : std::uint16_t { kSolo = 1, kParty = 2, kGuild = 4 };

enum class EntryQueuePhase : std::uint8_t {
  // Hors file.
  kIdle,
  // La demande est partie, la réponse n'est pas venue.
  kApplying,
  // En file.
  kQueued,
  // L'annulation est partie, la réponse n'est pas venue : toujours en file.
  kCancelling,
  // Le terrain est prêt : le serveur attend « j'entre » ou « je refuse ».
  kInvited,
  // « J'entre » est parti.
  kAccepted,
};

// CE QUE LE JOUEUR DOIT APPRENDRE, une fois. Chaque code de refus du serveur a
// le sien ; le code 0 dit un refus dont la raison arrive à part, dans la
// conversation.
enum class EntryQueueNoticeKind : std::uint8_t {
  kQueued,
  kRefusedElsewhere,
  kQueueFinished,
  kUnknownName,
  kUnknownType,
  kTooMany,
  kLevel,
  kDuplicate,
  kReconnect,
  kLeaderOnly,
  kJobRefused,
  kCancelled,
  kCancelRefused,
  kDeclined,
  kAdmitted,
  // La demande d'inscription est restée sans réponse : la file en est
  // ressortie d'elle-même. Voir `kEntryQueueApplyTimeoutMs`.
  kNoAnswer,
};

// LES SONS QUE LA FILE DEMANDE : inscription acceptée, terrain prêt, entrée
// accordée.
enum class EntryQueueSound : std::uint8_t { kRequest, kReady, kForward };

// L'invitation expire côté serveur au bout de ce délai.
inline constexpr std::uint32_t kEntryQueueInvitationMs = 20000;
// Le rythme auquel l'effectif de la file se redemande, en file.
inline constexpr std::uint32_t kEntryQueueRankingPeriodMs = 10000;
// LE DÉLAI LAISSÉ AU SERVEUR POUR RÉPONDRE À UNE INSCRIPTION. ⚠ IL PEUT SE
// TAIRE : un nom de terrain qu'il ne connaît pas, la fonction coupée chez lui,
// un chef laissé hors d'une équipe déjà pleine — aucun de ces cas ne renvoie de
// réponse. Sans échéance, la file attendrait pour toujours, et les gestes
// d'inscription resteraient éteints.
inline constexpr std::uint32_t kEntryQueueApplyTimeoutMs = 10000;
// L'effectif tant que le serveur ne l'a pas dit.
inline constexpr std::int32_t kEntryQueueCountUnknown = -1;

struct EntryQueueNotice {
  EntryQueueNoticeKind kind = EntryQueueNoticeKind::kQueued;
  // Le terrain dont il s'agit, tel que le serveur le nomme ; vide s'il ne l'a
  // pas dit.
  std::string field_name;
};

class EntryQueue {
 public:
  // LA DEMANDE EST PARTIE : la file attend sa réponse.
  void NoteApplied(std::string field_name, EntryQueueApplyKind kind, std::uint32_t now_ms);
  // `ZC 0x08D8`. Code 1 : en file. Tout autre : hors file, et un avis.
  void TellApplyResult(std::uint8_t code, std::string field_name, std::uint32_t now_ms);
  // `ZC 0x08D9` : l'effectif TOTAL en file, pas un rang.
  void TellCount(std::string field_name, std::int32_t count, std::uint32_t now_ms);
  void NoteCancelSent();
  // `ZC 0x08DB`. Réussie : hors file. Refusée : toujours en file, et un avis.
  void TellCancelResult(bool success, std::string field_name);
  // `ZC 0x08DF` : le terrain est prêt, et l'invitation court.
  void TellInvitation(std::string field_name, std::string lobby, std::uint32_t now_ms);
  void NoteInvitationAnswered(bool accept, std::uint32_t now_ms);
  // `ZC 0x08E1`. `map_name` est la carte où le serveur nous envoie.
  void TellAdmission(std::uint8_t result, std::string map_name);
  // `ZC 0x090E` : tout revient à zéro, sans avis.
  void Reset();
  // LE TEMPS PASSE. Une demande restée sans réponse `kEntryQueueApplyTimeoutMs`
  // après son départ ramène hors file, avec l'avis `kNoAnswer`. À appeler à
  // chaque image.
  void Advance(std::uint32_t now_ms);

  [[nodiscard]] EntryQueuePhase phase() const noexcept { return phase_; }
  // De `kQueued` à `kAccepted` : le serveur nous tient en file.
  [[nodiscard]] bool queued() const noexcept;
  [[nodiscard]] EntryQueueApplyKind apply_kind() const noexcept { return apply_kind_; }
  [[nodiscard]] const std::string& field_name() const noexcept { return field_name_; }
  [[nodiscard]] const std::string& lobby_name() const noexcept { return lobby_name_; }
  // `kEntryQueueCountUnknown` tant que le serveur ne l'a pas dit.
  [[nodiscard]] std::int32_t count() const noexcept { return count_; }
  // Depuis l'entrée en file ; zéro hors file.
  [[nodiscard]] std::uint32_t waited_ms(std::uint32_t now_ms) const noexcept;
  // Ce qui reste de l'invitation, borné à zéro ; zéro hors invitation.
  [[nodiscard]] std::uint32_t invitation_ms_left(std::uint32_t now_ms) const noexcept;
  // En file, hors invitation, et `kEntryQueueRankingPeriodMs` écoulées depuis
  // la dernière demande d'effectif.
  [[nodiscard]] bool RankingDue(std::uint32_t now_ms) const noexcept;
  void NoteRankingAsked(std::uint32_t now_ms) noexcept;
  // AUGMENTE À CHAQUE CHANGEMENT DE PHASE : l'étage y lit les transitions sans
  // comparer d'états.
  [[nodiscard]] std::uint32_t phase_serial() const noexcept { return phase_serial_; }
  // Consommés.
  [[nodiscard]] std::vector<EntryQueueNotice> TakeNotices();
  [[nodiscard]] std::vector<EntryQueueSound> TakeSounds();

 private:
  EntryQueuePhase phase_ = EntryQueuePhase::kIdle;
  EntryQueueApplyKind apply_kind_ = EntryQueueApplyKind::kSolo;
  std::string field_name_;
  std::string lobby_name_;
  std::int32_t count_ = kEntryQueueCountUnknown;
  std::uint32_t applied_at_ms_ = 0;
  std::uint32_t queued_since_ms_ = 0;
  std::uint32_t invited_at_ms_ = 0;
  std::uint32_t ranking_asked_ms_ = 0;
  std::uint32_t phase_serial_ = 0;
  std::vector<EntryQueueNotice> notices_;
  std::vector<EntryQueueSound> sounds_;
};

}  // namespace ro::sim
