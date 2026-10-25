#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ro::net {

// ── LE CLASSEMENT MONDIAL DU MODE SURVIVORS SUR LE FIL ──────────────────────
//
// 🔴 LE CONTRAT EST CELUI DU SERVEUR. Chaque opcode et chaque taille ci-dessous
// est épinglé, dans le `.cpp`, contre la structure que `packetgen` engendre de
// ses en-têtes : un serveur qui change une trame arrête la bâtisse au lieu de
// désynchroniser le flux. AUCUNE fonction de ce fichier n'émet : elles rendent
// des octets.
//
// 🔴 MUET TANT QUE LE SERVEUR N'A RIEN ANNONCÉ. Un serveur qui ne connaît pas
// un CZ coupe la session : aucun octet du service ne part avant que
// `ZC 0x0F53` ait annoncé le service pour la version de règles du client.
//
//   CZ 0x0F4C BEGIN   [op][len:2][ruleset:2][arena:2][class:2][board:1]        11
//   ZC 0x0F4D TICKET  [op][len:2][result:1][ticket:4][seed:8][issued_at:8]     25
//   CZ 0x0F4E END     [op][len:2][ticket:4][survived_steps:4][kill_points:4]
//                     [kills:4][boss_kills:2][level:2][outcome:1]
//                     [journal_bytes:4]                                        29
//   ZC 0x0F4F RESULT  [op][len:2][ticket:4][verdict:1][score:8][rank:2][best:8] 27
//   CZ 0x0F50 BOARD   [op][len:2][ruleset:2][arena:2][class:2][board:1]        11
//   ZC 0x0F51 BOARD   [op][len:2][ruleset:2][arena:2][class:2][board:1]
//                     [own_rank:2][own_score:8][count:1]                       22
//                     puis `count` × [rank:2][score:8][survived_steps:4]
//                     [level:2][verified:1][name:24]                           41
//   CZ 0x0F52 JOURNAL réservé, jamais émis
//   ZC 0x0F53 SERVICE [op][len:2][ruleset_low:2][ruleset_high:2][server_utc:8] 16
//
// Tout est petit-boutiste ; `len` compte la trame entière, en-tête compris.
// Les heures sont des secondes UTC depuis 1970, à l'horloge du serveur. Un
// rang vaut 1 pour le premier et 0 hors tableau.

inline constexpr std::uint16_t kSurvivorsBeginOpcode = 0x0F4C;
inline constexpr std::uint16_t kSurvivorsTicketOpcode = 0x0F4D;
inline constexpr std::uint16_t kSurvivorsEndOpcode = 0x0F4E;
inline constexpr std::uint16_t kSurvivorsResultOpcode = 0x0F4F;
inline constexpr std::uint16_t kSurvivorsBoardAskOpcode = 0x0F50;
inline constexpr std::uint16_t kSurvivorsBoardOpcode = 0x0F51;
inline constexpr std::uint16_t kSurvivorsJournalOpcode = 0x0F52;
inline constexpr std::uint16_t kSurvivorsServiceOpcode = 0x0F53;

// Les ZC du service : la table des longueurs doit les connaître tous avant que
// le client n'annonce `kUiCapSurvivors`.
inline constexpr std::array<std::uint16_t, 4> kSurvivorsReplyOpcodes{
    kSurvivorsTicketOpcode, kSurvivorsResultOpcode, kSurvivorsBoardOpcode, kSurvivorsServiceOpcode};

inline constexpr std::size_t kSurvivorsBeginSize = 11;
inline constexpr std::size_t kSurvivorsTicketSize = 25;
inline constexpr std::size_t kSurvivorsEndSize = 29;
inline constexpr std::size_t kSurvivorsResultSize = 27;
inline constexpr std::size_t kSurvivorsBoardAskSize = 11;
inline constexpr std::size_t kSurvivorsBoardHeaderSize = 22;
// La plus petite entrée du tableau : un serveur plus récent peut l'allonger,
// et le client saute ce qu'il ne connaît pas.
inline constexpr std::size_t kSurvivorsBoardLineSize = 41;
inline constexpr std::size_t kSurvivorsServiceSize = 16;
inline constexpr std::size_t kSurvivorsBoardNameSize = 24;

// Le résultat d'une demande de jeton. Un numéro inconnu se lit `kUnknown` :
// un refus dont la raison ne promet rien.
enum class SurvivorsTicketResult : std::uint8_t {
  kGranted = 0,
  kPlaceRefused = 1,
  // Le compte n'a pas d'identifiant d'utilisateur.
  kUnlinkedAccount = 2,
  // Version de règles ou tableau que le serveur ne classe pas.
  kUnknownRules = 3,
  // Une demande par 10 s et par personnage.
  kTooSoon = 4,
  kUnknown = 255,
};

// Le verdict d'une fin de partie. Un numéro inconnu se lit `kUnknown`, comme
// un refus.
enum class SurvivorsVerdict : std::uint8_t {
  kAccepted = 0,
  kPersonalBest = 1,
  // Jeton inconnu, déjà consommé ou expiré.
  kUnknownTicket = 2,
  kImplausible = 3,
  kUnknown = 255,
};

// Le tableau mondial : la partie classée (graine tirée par le serveur) ou le
// défi de la semaine (graine de la semaine). Le client n'émet jamais
// `kUnknown`.
enum class SurvivorsBoardKind : std::uint8_t {
  kRanked = 0,
  kChallenge = 1,
  kUnknown = 255,
};

// Ce qui désigne un tableau : la demande de jeton, la demande de tableau et
// l'en-tête d'une page en portent la même forme.
struct SurvivorsBoardKey {
  std::uint16_t ruleset = 0;
  std::uint16_t arena_id = 0;
  // La classe du mode (`ClassRule::class_id`), pas le job d'apparence.
  std::uint16_t class_id = 0;
  SurvivorsBoardKind board = SurvivorsBoardKind::kRanked;
  [[nodiscard]] bool operator==(const SurvivorsBoardKey&) const = default;
};
using SurvivorsTicketAsk = SurvivorsBoardKey;
using SurvivorsBoardAsk = SurvivorsBoardKey;

struct SurvivorsTicket {
  SurvivorsTicketResult result = SurvivorsTicketResult::kUnknown;
  std::uint32_t ticket = 0;
  // En défi : `SeedOfWeek` de la semaine ISO de `issued_at_utc_s`.
  std::uint64_t seed = 0;
  std::int64_t issued_at_utc_s = 0;
  [[nodiscard]] bool operator==(const SurvivorsTicket&) const = default;
};

// Les composantes d'une partie finie, que le serveur borne et dont il refait
// le score.
struct SurvivorsEndReport {
  std::uint32_t ticket = 0;
  std::uint32_t survived_steps = 0;  // pas de 20 ms
  std::uint32_t kill_points = 0;
  std::uint32_t kills = 0;
  std::uint16_t boss_kills = 0;
  std::uint16_t level = 0;
  // 1 tombé, 2 survécu, 3 victoire : rien d'autre ne s'envoie.
  std::uint8_t outcome = 0;
  std::uint32_t journal_bytes = 0;
  [[nodiscard]] bool operator==(const SurvivorsEndReport&) const = default;
};

// Les issues qu'`END` peut porter.
inline constexpr std::uint8_t kSurvivorsWireOutcomeFell = 1;
inline constexpr std::uint8_t kSurvivorsWireOutcomeVictory = 3;

struct SurvivorsResult {
  // Le jeton d'`END`, renvoyé en écho : c'est lui qui apparie la réponse.
  std::uint32_t ticket = 0;
  SurvivorsVerdict verdict = SurvivorsVerdict::kUnknown;
  std::uint64_t score = 0;  // recalculé par le serveur
  std::uint16_t rank = 0;
  std::uint64_t best = 0;  // le meilleur score du compte sur ce tableau, 0 sans
  [[nodiscard]] bool operator==(const SurvivorsResult&) const = default;
};

struct SurvivorsBoardLine {
  std::uint16_t rank = 0;
  std::uint64_t score = 0;
  std::uint32_t survived_steps = 0;
  std::uint16_t level = 0;
  bool verified = false;  // toujours faux tant que le serveur ne rejoue pas
  std::string name;       // en UTF-8
  [[nodiscard]] bool operator==(const SurvivorsBoardLine&) const = default;
};

struct SurvivorsBoardPage {
  SurvivorsBoardKey key;
  std::uint16_t own_rank = 0;   // 0 : aucune partie acceptée
  std::uint64_t own_score = 0;  // 0 : aucune partie acceptée
  std::vector<SurvivorsBoardLine> lines;
  [[nodiscard]] bool operator==(const SurvivorsBoardPage&) const = default;
};

// L'annonce du service : les versions de règles que le serveur classe, bornes
// comprises, et son heure.
struct SurvivorsService {
  std::uint16_t ruleset_low = 0;
  std::uint16_t ruleset_high = 0;
  std::int64_t server_utc_s = 0;
  [[nodiscard]] bool operator==(const SurvivorsService&) const = default;
};

// LES OCTETS D'UN CZ, rien quand la demande ne peut pas partir : un tableau
// `kUnknown`, une issue qu'`END` ne porte pas.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeSurvivorsBegin(
    const SurvivorsTicketAsk& ask);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeSurvivorsEnd(
    const SurvivorsEndReport& report);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeSurvivorsBoardAsk(
    const SurvivorsBoardAsk& ask);

// LES ZC, depuis la trame entière : rien quand l'opcode n'est pas le sien,
// que `len` ne dit pas la taille de la trame, ou que la taille n'est pas la
// sienne.
[[nodiscard]] std::optional<SurvivorsTicket> DecodeSurvivorsTicket(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<SurvivorsResult> DecodeSurvivorsResult(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<SurvivorsService> DecodeSurvivorsService(
    std::span<const std::uint8_t> frame);
// La taille d'une entrée se déduit de la trame, `(len − 22) / count`, qui doit
// tomber juste et valoir au moins 41 ; un `count` nul exige `len = 22`. Une
// trame dont le compte ne tombe pas juste se rejette entière.
[[nodiscard]] std::optional<SurvivorsBoardPage> DecodeSurvivorsBoard(
    std::span<const std::uint8_t> frame);

}  // namespace ro::net
