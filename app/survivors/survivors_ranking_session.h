#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/net/survivors_ranking.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "survivors/survivors_pending.h"

namespace moonclient {

// ── LA SESSION DE CLASSEMENT, sans liaison ni horloge propre ────────────────
//
// Elle reçoit ce que le serveur dit (l'annonce du service, les quatre ZC
// décodés), les instants du jeu et les intentions du joueur, et rend les trames
// à émettre, les changements des résultats en attente et les lignes à dire.
// `app` émet les trames, écrit les changements au carnet et dit les lignes.
//
// 🔴 MUETTE TANT QUE LE SERVEUR N'A RIEN ANNONCÉ. Un CZ que le serveur ne
// connaît pas coupe la session : `TakeOutgoing` reste vide tant que
// `ZC 0x0F53` n'a pas annoncé le service pour la version de règles de la
// session, et chaque poignée de main vérifiée, comme sa perte, le referme
// jusqu'à la prochaine annonce.
//
// 🔴 UN RÉSULTAT EN ATTENTE APPARTIENT À UN PERSONNAGE D'UN SERVEUR : il ne se
// redit qu'avec eux, à chaque poignée de main vérifiée et à chaque entrée en
// carte, un par image au plus.

// U23 : le jeton se reçoit dans les 3 s qui suivent la demande, sinon la partie
// part libre.
inline constexpr std::int64_t kSurvivorsTicketWaitMs = 3000;
// Une demande de tableau toutes les 2 s au plus.
inline constexpr std::int64_t kSurvivorsBoardCadenceMs = 2000;
// A5 : un jeton vaut 24 h après son heure ; au-delà, l'entrée s'oublie.
inline constexpr std::int64_t kSurvivorsTicketLifetimeS = 86400;

// Les instants que l'appelant donne à chaque image.
struct SurvivorsRankingNow {
  std::int64_t steady_ms = 0;  // monotone : les échéances et les cadences
  std::int64_t utc_s = 0;      // à l'horloge de la machine
};

// Où en est la partie en cours au classement.
enum class SurvivorsRunRanking : std::uint8_t {
  kNotAsked,  // aucune demande : partie libre ou pas encore partie
  kAwaiting,  // jeton demandé, échéance pas encore passée
  kRanked,    // jeton reçu à temps : la partie se classe
  kFree,      // refus, silence, départ chargé, service fermé : partie libre
};

// Ce que la session a à dire, au journal ou en conversation.
enum class SurvivorsRankingNoticeKind : std::uint8_t {
  kServiceAbsent,          // une fois : le serveur n'a jamais annoncé le service
  kRulesetNotServed,       // l'annonce ne couvre pas la version de la session
  kTicketRefused,          // `ticket_result` dit la raison
  kTicketSilent,           // l'échéance est passée sans jeton
  kChallengeSeedMismatch,  // la graine du défi n'est pas celle de sa semaine
  kVerdict,                // `result` : accepté, record, ou refusé
  kResultLost,             // jeton inconnu au serveur : déjà comptée, ou perdue
  kResultExpired,          // le jeton a dépassé sa durée de vie : oubliée
  kScoreMismatch,          // le score du serveur diffère du score local
};
struct SurvivorsRankingNotice {
  SurvivorsRankingNoticeKind kind = SurvivorsRankingNoticeKind::kServiceAbsent;
  ro::net::SurvivorsTicketResult ticket_result = ro::net::SurvivorsTicketResult::kUnknown;
  ro::net::SurvivorsResult result;
  std::uint64_t local_score = 0;
  std::int64_t ended_at_utc_s = 0;  // la fin de la partie dont on parle, 0 sans
  // Le tableau demandé, pour un refus de jeton : un défi refusé reste un défi local.
  ro::net::SurvivorsBoardKind board = ro::net::SurvivorsBoardKind::kRanked;
  [[nodiscard]] bool operator==(const SurvivorsRankingNotice&) const = default;
};

// Un changement des résultats en attente, que `app` porte au carnet par
// `AddPendingResult` ou `RemovePendingResult`.
enum class SurvivorsPendingChangeKind : std::uint8_t { kAdd, kRemove };
struct SurvivorsPendingChange {
  SurvivorsPendingChangeKind kind = SurvivorsPendingChangeKind::kAdd;
  SurvivorsPendingResult entry;
  [[nodiscard]] bool operator==(const SurvivorsPendingChange&) const = default;
};

// LA DEMANDE DE JETON d'une partie : le tableau, les clés du catalogue pour
// l'entrée en attente, et si le départ est neutre — un départ chargé ne se
// classe jamais. La version des règles est celle de la session.
struct SurvivorsTicketRequest {
  std::uint16_t arena_id = 0;
  std::uint16_t class_id = 0;
  ro::net::SurvivorsBoardKind board = ro::net::SurvivorsBoardKind::kRanked;
  std::string arena_key;
  std::string class_key;
  bool neutral_start = false;
};

// LA FIN D'UNE PARTIE. Une partie figée par la perte de liaison ne s'envoie
// pas, ni une partie abandonnée.
struct SurvivorsRunEnd {
  ro::sim::SurvivorsTally tally;
  std::uint32_t journal_bytes = 0;
  bool frozen_by_link_loss = false;
};

// La dernière page reçue d'un tableau, avec son instant.
struct SurvivorsHeldPage {
  ro::net::SurvivorsBoardPage page;
  std::int64_t received_at_ms = 0;
};

// Vrai quand le joueur a un rang sur la page ; un rang 0 dit « hors tableau ».
[[nodiscard]] inline bool IsRankedOnPage(const ro::net::SurvivorsBoardPage& page) noexcept {
  return page.own_rank != 0;
}

class SurvivorsRankingSession {
 public:
  explicit SurvivorsRankingSession(std::uint16_t ruleset = ro::sim::kSurvivorsRulesetVersion);

  // LES ENTRÉES.
  // Les entrées en attente du carnet, à l'ouverture ; elles remplacent celles
  // que la session tenait.
  void LoadPending(std::vector<SurvivorsPendingResult> entries);
  void NoteCharacter(std::string server_key, std::uint32_t character_id,
                     std::string character_name);
  void NoteHandshakeVerified();
  // La poignée de main n'est plus reconnue (changement de zone, liaison
  // perdue) : le service se referme et rien de ce qui attendait ne partira.
  void NoteHandshakeLost();
  void NoteMapEntered();
  // L'instant de réception des trames qui suivent, avant elles : l'échéance du
  // jeton se juge à son arrivée, pas à l'image suivante.
  void NoteReceivedAt(std::int64_t steady_ms) noexcept { now_.steady_ms = steady_ms; }
  void NoteService(const ro::net::SurvivorsService& service);
  void NoteFrame(const ro::net::SurvivorsTicket& ticket);
  void NoteFrame(const ro::net::SurvivorsResult& result);
  void NoteFrame(const ro::net::SurvivorsBoardPage& page);
  // À chaque image : l'échéance du jeton et un renvoi au plus.
  void Tick(const SurvivorsRankingNow& now);

  // LES INTENTIONS.
  // Demande le jeton de la partie qui part, et rend son état : kAwaiting, ou
  // kFree tout de suite quand rien ne peut partir.
  SurvivorsRunRanking AskTicket(const SurvivorsTicketRequest& request);
  // Une partie part sans demande, ou son départ n'a pas abouti : l'état de la
  // partie précédente, son jeton, son envoi et son verdict s'oublient, la
  // partie ne se classe pas.
  void ForgetRun() noexcept;
  // La partie classée finie produit son `END` et son entrée en attente.
  void ReportEnd(const SurvivorsRunEnd& end);
  // Vrai quand la demande est partie : service ouvert et cadence passée.
  bool AskBoard(const ro::net::SurvivorsBoardAsk& ask);

  // LES SORTIES, vidées à la lecture.
  [[nodiscard]] std::vector<std::vector<std::uint8_t>> TakeOutgoing();
  [[nodiscard]] std::vector<SurvivorsPendingChange> TakePendingChanges();
  [[nodiscard]] std::vector<SurvivorsRankingNotice> TakeNotices();

  // LA VUE.
  [[nodiscard]] bool service_open() const noexcept { return service_open_; }
  [[nodiscard]] SurvivorsRunRanking run_state() const noexcept { return run_state_; }
  // La graine que le serveur a rendue pour la partie classée en cours.
  [[nodiscard]] std::optional<std::uint64_t> ranked_seed() const noexcept;
  // Le jeton de la partie classée en cours ; nul hors d'elle.
  [[nodiscard]] const ro::net::SurvivorsTicket* ranked_ticket() const noexcept;
  // Le verdict de la dernière partie classée envoyée ; rien tant qu'il n'est
  // pas venu.
  [[nodiscard]] const std::optional<ro::net::SurvivorsResult>& run_result() const noexcept {
    return run_result_;
  }
  [[nodiscard]] bool run_submitted() const noexcept { return submitted_ticket_.has_value(); }
  // Le jeton de la dernière partie classée envoyée, que son verdict porte.
  [[nodiscard]] const std::optional<std::uint32_t>& submitted_ticket() const noexcept {
    return submitted_ticket_;
  }
  [[nodiscard]] std::uint16_t ruleset() const noexcept { return ruleset_; }
  // Le serveur du personnage connecté : ses résultats en attente sont les siens.
  [[nodiscard]] const std::string& server_key() const noexcept { return server_key_; }
  [[nodiscard]] const SurvivorsHeldPage* page(ro::net::SurvivorsBoardKind board) const noexcept;
  // L'instant d'où une demande de tableau peut repartir.
  [[nodiscard]] std::int64_t board_ready_at_ms() const noexcept;
  [[nodiscard]] std::span<const SurvivorsPendingResult> pending() const noexcept {
    return pending_;
  }
  // L'écart de l'horloge du serveur sur celle de la machine, en secondes.
  [[nodiscard]] std::int64_t server_offset_s() const noexcept { return server_offset_s_; }
  // Les instants du dernier `Tick`, ou de la dernière réception.
  [[nodiscard]] const SurvivorsRankingNow& now() const noexcept { return now_; }

 private:
  [[nodiscard]] bool IsOurs(const SurvivorsPendingResult& entry) const;
  // Vrai quand la trame est mise à partir.
  bool Push(std::optional<std::vector<std::uint8_t>> frame);
  // Ce qui attendait de partir s'oublie, et avec lui les demandes de jeton.
  void DropOutgoing() noexcept;
  void Say(SurvivorsRankingNotice notice);
  void FreeRun(SurvivorsRankingNotice notice);
  void ArmResend();
  // Émet l'`END` de l'entrée et compte un envoi de plus.
  void Send(SurvivorsPendingResult& entry);
  void Remove(std::size_t index);

  std::uint16_t ruleset_;
  std::string server_key_;
  std::uint32_t character_id_ = 0;
  std::string character_name_;
  SurvivorsRankingNow now_;

  bool service_open_ = false;
  bool service_seen_ = false;
  bool told_absent_ = false;
  bool told_unserved_ = false;
  std::int64_t server_offset_s_ = 0;

  SurvivorsRunRanking run_state_ = SurvivorsRunRanking::kNotAsked;
  SurvivorsTicketRequest run_request_;
  std::int64_t run_deadline_ms_ = 0;
  std::optional<ro::net::SurvivorsTicket> run_ticket_;
  std::optional<std::uint32_t> submitted_ticket_;
  std::optional<ro::net::SurvivorsResult> run_result_;
  // LES JETONS ARRIVENT DANS L'ORDRE DES DEMANDES, une réponse par demande sur
  // une même liaison. Le jeton d'une demande déjà échue se reconnaît donc au
  // compte des demandes parties sans réponse : tant qu'il en reste une plus
  // ancienne, le jeton reçu est le sien et s'ignore. Une poignée de main
  // remet les comptes à zéro.
  std::uint32_t begins_queued_ = 0;  // demandes mises à partir, pas encore remises
  std::uint32_t tickets_owed_ = 0;   // demandes remises à la liaison, sans réponse

  std::vector<SurvivorsPendingResult> pending_;
  // Les jetons à redire, dans l'ordre du carnet.
  std::vector<std::uint32_t> resend_;

  std::optional<std::int64_t> last_board_ask_ms_;
  std::optional<SurvivorsHeldPage> ranked_page_;
  std::optional<SurvivorsHeldPage> challenge_page_;

  std::vector<std::vector<std::uint8_t>> outgoing_;
  std::vector<SurvivorsPendingChange> changes_;
  std::vector<SurvivorsRankingNotice> notices_;
};

}  // namespace moonclient
