#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::sim {

// ── LA RECHERCHE DE GROUPE ──────────────────────────────────────────────────
//
// Une ANNONCE dit qu'un joueur cherche des compagnons : à partir de quel
// niveau, pour quel lieu, et quels métiers ou rôles. Chaque personnage en
// publie une au plus ; le serveur la garde tant qu'il est connecté, et la
// pousse à tous les clients à chaque publication, modification ou retrait.
//
// Ce modèle tient la dernière page de recherche, l'annonce du joueur et les
// avis à écrire dans la conversation (case « Recherche de groupe »). Il ne
// parle pas au réseau : `net` lui dit ce que le serveur a répondu.

// LES CASES DE MÉTIER D'UNE ANNONCE, et la marque d'une case vide.
inline constexpr std::size_t kPartyBookingJobSlots = 6;
inline constexpr std::int16_t kPartyBookingNoJob = -1;
using PartyBookingJobs = std::array<std::int16_t, kPartyBookingJobSlots>;

// LES CODES D'UNE CASE. Les quatre premiers sont des RÔLES ; les suivants, de
// `kPartyBookingFirstJobCode` à `kPartyBookingLastJobCode`, des métiers, dans
// l'ordre de `PartyBookingJobName`. Ce ne sont pas les numéros de classe du
// serveur : le serveur ne les lit pas, il les recopie.
inline constexpr std::int16_t kPartyBookingRoleAttack = 1;
inline constexpr std::int16_t kPartyBookingRoleDefense = 2;
inline constexpr std::int16_t kPartyBookingRoleHealer = 3;
inline constexpr std::int16_t kPartyBookingRoleSupport = 4;
inline constexpr std::int16_t kPartyBookingFirstJobCode = 5;
inline constexpr std::int16_t kPartyBookingLastJobCode = 63;

// LE LIEU : un code à deux octets, la RÉGION en octet haut et le lieu dans la
// région en octet bas, tous deux comptés depuis zéro dans la table des lieux.
// Zéro dit « tous les lieux ».
inline constexpr std::int16_t kPartyBookingAnyMap = 0;
inline constexpr unsigned kPartyBookingRegionShift = 8;
inline constexpr unsigned kPartyBookingPlaceMask = 0xFFU;
[[nodiscard]] constexpr std::int16_t PartyBookingMapCode(std::size_t region, std::size_t place) {
  return static_cast<std::int16_t>((region << kPartyBookingRegionShift) |
                                   (place & kPartyBookingPlaceMask));
}
[[nodiscard]] constexpr std::size_t PartyBookingRegionOf(std::int16_t map_code) {
  return (static_cast<std::uint16_t>(map_code) >> kPartyBookingRegionShift) &
         kPartyBookingPlaceMask;
}
[[nodiscard]] constexpr std::size_t PartyBookingPlaceOf(std::int16_t map_code) {
  return static_cast<std::uint16_t>(map_code) & kPartyBookingPlaceMask;
}

// LE NIVEAU D'UNE ANNONCE est le niveau MINIMAL recherché ; elle vaut pour les
// personnages jusqu'à quinze niveaux au-dessus. Le plus haut est celui que ce
// serveur accorde.
inline constexpr std::int16_t kPartyBookingLevelSpan = 15;
inline constexpr std::int16_t kPartyBookingLeastLevel = 1;
inline constexpr std::int16_t kPartyBookingMostLevel = 999;

// Le serveur rend au plus dix annonces par page.
inline constexpr std::int16_t kPartyBookingPageSize = 10;

// Le nom d'un métier de `kPartyBookingFirstJobCode` à `kPartyBookingLastJobCode`
// — un nom propre du jeu, non traduit —, vide pour tout autre code.
[[nodiscard]] std::string_view PartyBookingJobName(std::int16_t code);
// Vrai pour un rôle, de `kPartyBookingRoleAttack` à `kPartyBookingRoleSupport`.
[[nodiscard]] bool IsPartyBookingRole(std::int16_t code);

// LES FAMILLES DE MÉTIERS, telles que la fenêtre les propose.
enum class PartyBookingJobFamily : std::uint8_t {
  kRoles,
  kFirst,
  kSecond,
  kThirdOne,
  kThirdTwo,
  kFirstHigh,
  kSecondHigh,
  kOther,
};
inline constexpr std::size_t kPartyBookingJobFamilyCount = 8;
// Les codes d'une famille, dans l'ordre où la fenêtre les montre.
[[nodiscard]] std::span<const std::int16_t> PartyBookingJobsOf(PartyBookingJobFamily family);

// UNE ANNONCE. ⚠ LE NOM EST EN UTF-8.
struct PartyBookingAd {
  // Son rang, unique et croissant sur ce serveur de carte.
  std::uint32_t index = 0;
  std::string leader_name;
  // La dernière publication ou modification, en secondes Unix.
  std::uint32_t posted_unix = 0;
  std::int16_t level = 0;
  std::int16_t map_code = kPartyBookingAnyMap;
  PartyBookingJobs jobs = {kPartyBookingNoJob, kPartyBookingNoJob, kPartyBookingNoJob,
                           kPartyBookingNoJob, kPartyBookingNoJob, kPartyBookingNoJob};
};

// CE QU'UNE RECHERCHE DEMANDE. Zéro au niveau et au lieu, `kPartyBookingNoJob`
// au métier : pas de filtre de ce côté.
struct PartyBookingCriteria {
  std::int16_t level = 0;
  std::int16_t map_code = kPartyBookingAnyMap;
  std::int16_t job = kPartyBookingNoJob;
};

// UNE ANNONCE RÉPOND-ELLE À CES CRITÈRES ? Un niveau non nul garde les
// annonces dont le niveau va de lui à quinze au-dessus ; un lieu, l'égalité ;
// un métier, sa présence dans une case. C'est la règle des annonces poussées
// pendant la partie — qui entrent dans la liste ou se signalent —, et non celle
// du serveur pour une page.
[[nodiscard]] bool PartyBookingAdMatches(const PartyBookingAd& ad,
                                         const PartyBookingCriteria& criteria);

// POURQUOI UNE ANNONCE NE PEUT PAS PARTIR, avant tout envoi.
enum class PartyBookingFault : std::uint8_t {
  kNone,
  kLevelOutOfRange,
  kNoJob,
  // Le serveur filtre par lieu OU par métier : les deux ensemble ne rendent rien.
  kMapAndJob,
};

// Une publication : niveau dans les bornes, au moins un métier.
[[nodiscard]] PartyBookingFault CheckPartyBookingRegister(std::int16_t level,
                                                          const PartyBookingJobs& jobs);
// Une modification : au moins un métier.
[[nodiscard]] PartyBookingFault CheckPartyBookingUpdate(const PartyBookingJobs& jobs);
// Une recherche : pas de lieu ET de métier à la fois, un niveau nul ou dans les
// bornes.
[[nodiscard]] PartyBookingFault CheckPartyBookingSearch(const PartyBookingCriteria& criteria);

// LES CASES REMPLIES, tassées au début, les vides à la fin.
[[nodiscard]] PartyBookingJobs PackPartyBookingJobs(std::span<const std::int16_t> codes);

// CE QUI S'ÉCRIT DANS LA CONVERSATION, case « Recherche de groupe ».
enum class PartyBookingNotice : std::uint8_t {
  // Une annonce a été publiée ; la liste n'est pas ouverte pour la montrer.
  kAdPosted,
  kRegisterRefused,
  kAlreadyRegistered,
  kDeleteRefused,
  kNothingToDelete,
  kNoResult,
};

// LES RÉPONSES DU SERVEUR À UNE PUBLICATION ET À UN RETRAIT.
inline constexpr std::uint16_t kPartyBookingRegistered = 0;
inline constexpr std::uint16_t kPartyBookingAlreadyRegistered = 2;
inline constexpr std::uint16_t kPartyBookingDeleted = 0;
inline constexpr std::uint16_t kPartyBookingDeletedExpired = 1;
inline constexpr std::uint16_t kPartyBookingDeleteRefused = 2;
inline constexpr std::uint16_t kPartyBookingNothingRegistered = 3;

class PartyBooking {
 public:
  // UNE NOUVELLE SESSION DE CARTE : le serveur oublie l'annonce d'un personnage
  // qui se déconnecte, et la liste ne vaut plus.
  void Reset();

  // Le joueur, dont le nom reconnaît son annonce parmi celles poussées.
  void SetOwnName(std::string_view own_name) { own_name_ = own_name; }

  // ── LES DEMANDES, notées à l'envoi ──
  //
  // Une recherche, ou sa page suivante, qui redonne les mêmes critères. La page
  // reçue remplace la liste.
  void NoteSearchSent(const PartyBookingCriteria& criteria);
  // La publication envoyée : ce qu'elle porte devient l'annonce du joueur à
  // l'accusé.
  void NoteRegisterSent(const PartyBookingAd& draft);

  // ── LES RÉPONSES ET LES AVIS DU SERVEUR ──
  void TellSearchResults(bool more_results, std::vector<PartyBookingAd> page);
  void TellRegisterResult(std::uint16_t result);
  void TellDeleteResult(std::uint16_t result);
  // Une annonce publiée, par n'importe qui. `list_shown` dit si la liste est
  // ouverte : elle y entre alors si elle répond aux critères, et sinon elle se
  // signale dans la conversation si elle répond à ceux de la dernière recherche.
  void TellInserted(const PartyBookingAd& ad, bool list_shown);
  void TellUpdated(std::uint32_t index, const PartyBookingJobs& jobs);
  void TellDeleted(std::uint32_t index);

  // La dernière page reçue, dans l'ordre du serveur, avec les annonces poussées
  // depuis.
  [[nodiscard]] std::span<const PartyBookingAd> results() const noexcept { return results_; }
  // Le serveur a d'autres annonces après cette page.
  [[nodiscard]] bool more_on_server() const noexcept { return more_on_server_; }
  // Le rang d'où part la page suivante.
  [[nodiscard]] std::uint32_t next_index() const noexcept { return next_index_; }
  // Une recherche est partie, et sa page n'est pas encore là.
  [[nodiscard]] bool searching() const noexcept { return searching_; }
  // Les critères de la dernière recherche ; rien avant la première.
  [[nodiscard]] const std::optional<PartyBookingCriteria>& criteria() const noexcept {
    return criteria_;
  }
  // L'annonce du joueur ; rien s'il n'en a pas.
  [[nodiscard]] const std::optional<PartyBookingAd>& mine() const noexcept { return mine_; }
  // Une publication est partie, et son accusé n'est pas encore là.
  [[nodiscard]] bool registering() const noexcept { return draft_.has_value(); }

  // Les avis à écrire depuis la dernière fois.
  [[nodiscard]] std::vector<PartyBookingNotice> TakeNotices();

 private:
  [[nodiscard]] PartyBookingAd* Find(std::uint32_t index);

  std::string own_name_;
  std::vector<PartyBookingAd> results_;
  bool more_on_server_ = false;
  std::uint32_t next_index_ = 0;
  bool searching_ = false;
  std::optional<PartyBookingCriteria> criteria_;
  std::optional<PartyBookingAd> mine_;
  std::optional<PartyBookingAd> draft_;
  std::vector<PartyBookingNotice> notices_;
};

}  // namespace ro::sim
