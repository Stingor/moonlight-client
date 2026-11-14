// LA RECHERCHE DE GROUPE, côté modèle : la page, l'annonce du joueur, les avis
// poussés par le serveur, et ce qui s'écrit dans la conversation.

#include "ro/sim/party_booking.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::PartyBooking;
using ro::sim::PartyBookingAd;
using ro::sim::PartyBookingCriteria;
using ro::sim::PartyBookingFault;
using ro::sim::PartyBookingJobFamily;
using ro::sim::PartyBookingJobs;
using ro::sim::PartyBookingNotice;

namespace {

constexpr std::int16_t kKnight = 11;
constexpr std::int16_t kPriest = 12;
constexpr std::int16_t kLevel = 50;
constexpr std::int16_t kPronteraCulvert = ro::sim::PartyBookingMapCode(1, 1);
constexpr PartyBookingJobs kKnightOnly = {kKnight, -1, -1, -1, -1, -1};
constexpr PartyBookingJobs kNoJobs = {-1, -1, -1, -1, -1, -1};

PartyBookingAd Ad(std::uint32_t index, std::string leader, std::int16_t level,
                  std::int16_t map_code, PartyBookingJobs jobs) {
  PartyBookingAd ad;
  ad.index = index;
  ad.leader_name = std::move(leader);
  ad.level = level;
  ad.map_code = map_code;
  ad.jobs = jobs;
  return ad;
}

}  // namespace

TEST_CASE("Recherche de groupe : les codes de métier et de lieu", "[sim][party_booking]") {
  CHECK(ro::sim::PartyBookingJobName(ro::sim::kPartyBookingFirstJobCode) == "Swordman");
  CHECK(ro::sim::PartyBookingJobName(kKnight) == "Knight");
  CHECK(ro::sim::PartyBookingJobName(ro::sim::kPartyBookingLastJobCode) == "Soul Linker");
  CHECK(ro::sim::PartyBookingJobName(ro::sim::kPartyBookingRoleHealer).empty());
  CHECK(ro::sim::IsPartyBookingRole(ro::sim::kPartyBookingRoleSupport));
  CHECK_FALSE(ro::sim::IsPartyBookingRole(kKnight));

  // Les familles couvrent chaque métier une fois, dans l'ordre des codes, sauf
  // les troisièmes jobs : aucune ne les propose, mais ils gardent leur nom.
  constexpr std::int16_t kFirstThirdJob = 24;
  constexpr std::int16_t kFirstJobAfterThird = 37;
  CHECK(ro::sim::PartyBookingJobName(kFirstThirdJob) == "Rune Knight");
  CHECK(ro::sim::PartyBookingJobName(kFirstJobAfterThird - 1) == "Shadow Chaser");
  std::int16_t expected = ro::sim::kPartyBookingFirstJobCode;
  for (std::size_t family = 1; family < ro::sim::kPartyBookingJobFamilyCount; ++family) {
    for (const std::int16_t code :
         ro::sim::PartyBookingJobsOf(static_cast<PartyBookingJobFamily>(family))) {
      if (expected == kFirstThirdJob) {
        expected = kFirstJobAfterThird;
      }
      CHECK(code == expected);
      ++expected;
    }
  }
  CHECK(expected == ro::sim::kPartyBookingLastJobCode + 1);
  CHECK(ro::sim::PartyBookingJobsOf(PartyBookingJobFamily::kRoles).size() == 4);

  CHECK(kPronteraCulvert == 0x0101);
  CHECK(ro::sim::PartyBookingRegionOf(kPronteraCulvert) == 1);
  CHECK(ro::sim::PartyBookingPlaceOf(kPronteraCulvert) == 1);
}

TEST_CASE("Recherche de groupe : ce qui ne peut pas partir", "[sim][party_booking]") {
  CHECK(ro::sim::CheckPartyBookingRegister(kLevel, kKnightOnly) == PartyBookingFault::kNone);
  CHECK(ro::sim::CheckPartyBookingRegister(0, kKnightOnly) == PartyBookingFault::kLevelOutOfRange);
  CHECK(ro::sim::CheckPartyBookingRegister(kLevel, kNoJobs) == PartyBookingFault::kNoJob);
  CHECK(ro::sim::CheckPartyBookingUpdate(kNoJobs) == PartyBookingFault::kNoJob);
  // Le serveur filtre par lieu OU par métier.
  CHECK(ro::sim::CheckPartyBookingSearch(PartyBookingCriteria{
            .map_code = kPronteraCulvert, .job = kKnight}) == PartyBookingFault::kMapAndJob);
  CHECK(ro::sim::CheckPartyBookingSearch(PartyBookingCriteria{.level = 0}) ==
        PartyBookingFault::kNone);

  const std::vector<std::int16_t> scattered = {-1, kPriest, -1, kKnight};
  const PartyBookingJobs packed = ro::sim::PackPartyBookingJobs(scattered);
  CHECK(packed == PartyBookingJobs{kPriest, kKnight, -1, -1, -1, -1});
}

TEST_CASE("Recherche de groupe : une annonce répond aux critères", "[sim][party_booking]") {
  const PartyBookingAd ad = Ad(1, "Alice", kLevel, kPronteraCulvert, kKnightOnly);
  CHECK(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{}));
  // Le niveau d'une annonce est le plancher ; elle vaut quinze niveaux
  // au-dessus. Le critère est le niveau du PERSONNAGE qui cherche : un
  // personnage plus faible que le plancher n'est pas concerné, un plus fort de
  // seize niveaux non plus.
  CHECK(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.level = kLevel}));
  CHECK(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.level = kLevel + 15}));
  CHECK_FALSE(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.level = kLevel + 16}));
  CHECK_FALSE(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.level = kLevel - 1}));
  CHECK(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.map_code = kPronteraCulvert}));
  CHECK_FALSE(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.map_code = 0x0102}));
  CHECK(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.job = kKnight}));
  CHECK_FALSE(ro::sim::PartyBookingAdMatches(ad, PartyBookingCriteria{.job = kPriest}));
}

TEST_CASE("Recherche de groupe : une page, puis la suivante", "[sim][party_booking]") {
  PartyBooking booking;
  booking.NoteSearchSent(PartyBookingCriteria{.level = kLevel});
  CHECK(booking.searching());
  booking.TellSearchResults(
      true, {Ad(4, "Alice", kLevel, 0, kKnightOnly), Ad(9, "Bob", kLevel, 0, kKnightOnly)});
  CHECK_FALSE(booking.searching());
  CHECK(booking.more_on_server());
  REQUIRE(booking.results().size() == 2);
  // La page suivante part du rang qui suit la DERNIÈRE annonce de la page.
  CHECK(booking.next_index() == 10);
  CHECK(booking.TakeNotices().empty());

  // Une page vide le dit dans la conversation.
  booking.NoteSearchSent(PartyBookingCriteria{});
  booking.TellSearchResults(false, {});
  CHECK(booking.results().empty());
  CHECK(booking.TakeNotices() == std::vector<PartyBookingNotice>{PartyBookingNotice::kNoResult});
}

TEST_CASE("Recherche de groupe : mon annonce, de la publication au retrait",
          "[sim][party_booking]") {
  PartyBooking booking;
  booking.SetOwnName("Moi");
  booking.NoteRegisterSent(Ad(0, "Moi", kLevel, kPronteraCulvert, kKnightOnly));
  CHECK(booking.registering());
  booking.TellRegisterResult(ro::sim::kPartyBookingRegistered);
  CHECK_FALSE(booking.registering());
  REQUIRE(booking.mine().has_value());
  CHECK(booking.mine()->index == 0);

  // L'avis poussé à tous lui donne son rang.
  booking.TellInserted(Ad(12, "Moi", kLevel, kPronteraCulvert, kKnightOnly), false);
  REQUIRE(booking.mine().has_value());
  CHECK(booking.mine()->index == 12);

  booking.TellUpdated(12, PartyBookingJobs{kPriest, -1, -1, -1, -1, -1});
  CHECK(booking.mine()->jobs[0] == kPriest);

  booking.TellDeleteResult(ro::sim::kPartyBookingDeleted);
  CHECK_FALSE(booking.mine().has_value());
  static_cast<void>(booking.TakeNotices());

  // Refusée, ou déjà publiée : un avis, et pas d'annonce.
  booking.NoteRegisterSent(Ad(0, "Moi", kLevel, 0, kKnightOnly));
  booking.TellRegisterResult(ro::sim::kPartyBookingAlreadyRegistered);
  CHECK_FALSE(booking.mine().has_value());
  booking.TellDeleteResult(ro::sim::kPartyBookingNothingRegistered);
  CHECK(booking.TakeNotices() ==
        std::vector<PartyBookingNotice>{PartyBookingNotice::kAlreadyRegistered,
                                        PartyBookingNotice::kNothingToDelete});
}

TEST_CASE("Recherche de groupe : une annonce poussée entre dans la liste ou se signale",
          "[sim][party_booking]") {
  PartyBooking booking;
  // Avant toute recherche, toute annonce se signale quand la liste est fermée.
  booking.TellInserted(Ad(1, "Alice", kLevel, 0, kKnightOnly), false);
  CHECK(booking.TakeNotices() == std::vector<PartyBookingNotice>{PartyBookingNotice::kAdPosted});
  CHECK(booking.results().empty());

  // Après une recherche de chevaliers, une annonce de prêtres se tait.
  booking.NoteSearchSent(PartyBookingCriteria{.job = kKnight});
  booking.TellSearchResults(false, {Ad(1, "Alice", kLevel, 0, kKnightOnly)});
  booking.TellInserted(Ad(2, "Bob", kLevel, 0, PartyBookingJobs{kPriest, -1, -1, -1, -1, -1}),
                       false);
  CHECK(booking.TakeNotices().empty());

  // La liste ouverte : l'annonce qui répond y entre, sans avis.
  booking.TellInserted(Ad(3, "Carl", kLevel, 0, kKnightOnly), true);
  CHECK(booking.TakeNotices().empty());
  REQUIRE(booking.results().size() == 2);
  CHECK(booking.results()[1].leader_name == "Carl");

  // Retirée par son auteur : elle quitte la liste.
  booking.TellDeleted(1);
  REQUIRE(booking.results().size() == 1);
  CHECK(booking.results()[0].index == 3);

  // Une nouvelle session oublie tout, sauf le nom du joueur.
  booking.Reset();
  CHECK(booking.results().empty());
  CHECK_FALSE(booking.criteria().has_value());
}
