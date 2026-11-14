// LE CARNET DE CHASSE MVP, côté état : horloge, fenêtres de retour, créneaux,
// observations, groupe, tombes et signalement.

#include "ro/sim/mvp_tracker.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::MvpBadgeLit;
using ro::sim::MvpGroup;
using ro::sim::MvpMember;
using ro::sim::MvpObservation;
using ro::sim::MvpSlot;
using ro::sim::MvpTomb;
using ro::sim::MvpTombsOn;
using ro::sim::MvpTrackerBook;
using ro::sim::MvpWindow;

namespace {

// Le retour au plus tôt de Baphomet : deux heures et une minute ; l'amplitude
// du tirage : neuf minutes.
constexpr std::uint32_t kDelay1Ms = 7260000;
constexpr std::uint32_t kDelay2Ms = 540000;
constexpr std::int64_t kDelay1S = 7260;
constexpr std::int64_t kDelay2S = 540;
constexpr std::int64_t kKillS = 1700000000;
constexpr std::int64_t kLocalS = 1699999000;

[[nodiscard]] MvpSlot Slot(std::uint16_t slot_id, std::uint16_t mob_id, std::string map,
                           std::uint32_t delay1_ms = kDelay1Ms,
                           std::uint32_t delay2_ms = kDelay2Ms) {
  MvpSlot slot;
  slot.slot_id = slot_id;
  slot.mob_id = mob_id;
  slot.delay1_ms = delay1_ms;
  slot.delay2_ms = delay2_ms;
  slot.map = std::move(map);
  return slot;
}

[[nodiscard]] MvpObservation Killed(std::int64_t kill_time_s, std::int16_t tomb_x = -1,
                                    std::int16_t tomb_y = -1) {
  MvpObservation observation;
  observation.kill_time_s = kill_time_s;
  observation.tomb_x = tomb_x;
  observation.tomb_y = tomb_y;
  return observation;
}

[[nodiscard]] MvpObservation Exact(std::int64_t exact_respawn_s, std::int16_t tomb_x = -1,
                                   std::int16_t tomb_y = -1) {
  MvpObservation observation = Killed(0, tomb_x, tomb_y);
  observation.exact_respawn_s = exact_respawn_s;
  return observation;
}

void Observe(MvpTrackerBook& book, std::uint16_t slot_id, MvpObservation observation) {
  std::vector<std::pair<std::uint16_t, MvpObservation>> entries;
  entries.emplace_back(slot_id, std::move(observation));
  book.ReceiveObservations(std::move(entries), false);
}

}  // namespace

TEST_CASE("Carnet MVP : l'horloge suit le dernier paquet du serveur", "[sim][mvp]") {
  MvpTrackerBook book;
  // Avant tout paquet : l'heure locale.
  CHECK_FALSE(book.clock_known());
  CHECK(book.ServerNow(kLocalS) == kLocalS);

  book.NoteServerTime(kLocalS + 120, kLocalS);
  CHECK(book.clock_known());
  CHECK(book.ServerNow(kLocalS) == kLocalS + 120);
  CHECK(book.ServerNow(kLocalS + 5) == kLocalS + 125);

  // Recalée à CHAQUE paquet : le second écart remplace le premier.
  book.NoteServerTime(kLocalS - 30, kLocalS);
  CHECK(book.ServerNow(kLocalS) == kLocalS - 30);

  // Une heure nulle ou négative n'en est pas une : l'écart connu reste.
  book.NoteServerTime(0, kLocalS + 1000);
  book.NoteServerTime(-5, kLocalS + 1000);
  CHECK(book.ServerNow(kLocalS) == kLocalS - 30);
}

TEST_CASE("Carnet MVP : une heure nulle ne fait pas connaître l'horloge", "[sim][mvp]") {
  MvpTrackerBook book;
  book.NoteServerTime(0, kLocalS);
  CHECK_FALSE(book.clock_known());
  CHECK(book.ServerNow(kLocalS) == kLocalS);
}

TEST_CASE("Carnet MVP : la fenêtre de retour suit la loi du créneau", "[sim][mvp]") {
  MvpTrackerBook book;
  book.ReceiveCatalogue({Slot(4, 1039, "prt_maze03"), Slot(5, 1046, "gef_dun02", 1999, 2999)});

  SECTION("une mort connue donne une plage") {
    Observe(book, 4, Killed(kKillS));
    const std::optional<MvpWindow> window = book.WindowOf(4);
    REQUIRE(window.has_value());
    CHECK(window->from_s == kKillS + kDelay1S);
    CHECK(window->to_s == kKillS + kDelay1S + kDelay2S);
    CHECK_FALSE(window->exact);
  }
  SECTION("les millisecondes tombent en secondes ENTIÈRES") {
    Observe(book, 5, Killed(kKillS));
    const std::optional<MvpWindow> window = book.WindowOf(5);
    REQUIRE(window.has_value());
    CHECK(window->from_s == kKillS + 1);
    CHECK(window->to_s == kKillS + 1 + 2);
  }
  SECTION("un instant mérité vaut même sans créneau au catalogue") {
    Observe(book, 99, Exact(kKillS + 500));
    const std::optional<MvpWindow> window = book.WindowOf(99);
    REQUIRE(window.has_value());
    CHECK(window->from_s == kKillS + 500);
    CHECK(window->to_s == kKillS + 500);
    CHECK(window->exact);
  }
  SECTION("l'instant mérité passe avant la mort") {
    MvpObservation both = Killed(kKillS);
    both.exact_respawn_s = kKillS + 42;
    Observe(book, 4, both);
    const std::optional<MvpWindow> window = book.WindowOf(4);
    REQUIRE(window.has_value());
    CHECK(window->exact);
    CHECK(window->from_s == kKillS + 42);
  }
  SECTION("une mort inconnue ne donne rien") {
    Observe(book, 4, Killed(0));
    CHECK_FALSE(book.WindowOf(4).has_value());
  }
  SECTION("un créneau absent du catalogue ne donne rien") {
    Observe(book, 77, Killed(kKillS));
    CHECK_FALSE(book.WindowOf(77).has_value());
  }
  SECTION("sans observation, rien") { CHECK_FALSE(book.WindowOf(4).has_value()); }
}

TEST_CASE("Carnet MVP : un créneau se retrouve par son monstre et sa carte", "[sim][mvp]") {
  MvpTrackerBook book;
  // Le créneau scripté d'abord : le créneau du monstre doit passer devant lui.
  book.ReceiveCatalogue({Slot(0, 0, "lhz_dun03"), Slot(1, 1039, "prt_maze03"),
                         Slot(2, 1646, "lhz_dun03"), Slot(3, 1039, "gef_dun02")});

  SECTION("le monstre et sa carte") {
    const MvpSlot* slot = book.FindSlotFor(1039, "gef_dun02");
    REQUIRE(slot != nullptr);
    CHECK(slot->slot_id == 3);
  }
  SECTION("la carte se compare sans la casse") {
    const MvpSlot* slot = book.FindSlotFor(1039, "PRT_Maze03");
    REQUIRE(slot != nullptr);
    CHECK(slot->slot_id == 1);
  }
  SECTION("le créneau du monstre passe avant le créneau scripté") {
    const MvpSlot* slot = book.FindSlotFor(1646, "lhz_dun03");
    REQUIRE(slot != nullptr);
    CHECK(slot->slot_id == 2);
  }
  SECTION("un monstre sans créneau retombe sur le créneau scripté de la carte") {
    const MvpSlot* slot = book.FindSlotFor(1647, "LHZ_DUN03");
    REQUIRE(slot != nullptr);
    CHECK(slot->slot_id == 0);
  }
  SECTION("une carte vide ne trouve rien") {
    CHECK(book.FindSlotFor(0, "") == nullptr);
    CHECK(book.FindSlotFor(1039, "") == nullptr);
  }
  SECTION("une carte inconnue ne trouve rien") {
    CHECK(book.FindSlotFor(1039, "prontera") == nullptr);
    // Sans créneau scripté sur la carte, pas de repli.
    CHECK(book.FindSlotFor(1646, "prt_maze03") == nullptr);
  }
  SECTION("un préfixe de carte n'est pas la carte") {
    CHECK(book.FindSlotFor(1039, "prt_maze0") == nullptr);
    CHECK(book.FindSlotFor(1039, "prt_maze033") == nullptr);
  }
  SECTION("par numéro de créneau") {
    REQUIRE(book.FindSlot(2) != nullptr);
    CHECK(book.FindSlot(2)->mob_id == 1646);
    CHECK(book.FindSlot(42) == nullptr);
  }
}

TEST_CASE("Carnet MVP : le catalogue se remplace et se numérote", "[sim][mvp]") {
  MvpTrackerBook book;
  CHECK_FALSE(book.catalog_known());
  const std::uint32_t before = book.catalog_serial();
  book.ReceiveCatalogue({Slot(1, 1039, "prt_maze03"), Slot(2, 1046, "gef_dun02")});
  CHECK(book.catalog_known());
  CHECK(book.catalog_serial() > before);
  const std::uint32_t first = book.catalog_serial();
  book.ReceiveCatalogue({Slot(7, 1059, "mjolnir_04")});
  CHECK(book.catalog_serial() > first);
  REQUIRE(book.slots().size() == 1);
  CHECK(book.FindSlot(1) == nullptr);
  CHECK(book.FindSlot(7) != nullptr);
}

TEST_CASE("Carnet MVP : l'instantané remplace, le delta complète", "[sim][mvp]") {
  MvpTrackerBook book;
  Observe(book, 1, Killed(kKillS));
  Observe(book, 2, Killed(kKillS + 10));
  REQUIRE(book.observations().size() == 2);

  // Un delta REMPLACE sans condition : l'arbitrage est au serveur, même une
  // source moins précise que la précédente fait autorité.
  MvpObservation mirror = Exact(kKillS + 99);
  mirror.source = ro::sim::MvpSource::kMirror;
  Observe(book, 1, mirror);
  MvpObservation manual = Killed(kKillS + 5);
  manual.source = ro::sim::MvpSource::kManual;
  Observe(book, 1, manual);
  REQUIRE(book.FindObservation(1) != nullptr);
  CHECK(book.FindObservation(1)->kill_time_s == kKillS + 5);
  CHECK(book.FindObservation(1)->source == ro::sim::MvpSource::kManual);

  std::vector<std::pair<std::uint16_t, MvpObservation>> snapshot;
  snapshot.emplace_back(std::uint16_t{3}, Killed(kKillS + 20));
  book.ReceiveObservations(std::move(snapshot), true);
  CHECK(book.observations().size() == 1);
  CHECK(book.FindObservation(1) == nullptr);
  CHECK(book.FindObservation(3) != nullptr);

  // L'instantané vide : la réponse « sans groupe ».
  book.ReceiveObservations({}, true);
  CHECK(book.observations().empty());
}

TEST_CASE("Carnet MVP : quitter un groupe vide ce qu'il savait", "[sim][mvp]") {
  MvpTrackerBook book;
  book.ReceiveInvitation(12, "Chasseurs");
  Observe(book, 1, Killed(kKillS));

  MvpGroup group;
  group.group_id = 5;
  group.owner_user_id = 2000001;
  group.name = "Chasse";
  group.members.push_back(MvpMember{.user_id = 2000001, .level = 99, .online = true, .name = "A"});
  book.ReceiveGroup(group, true);
  CHECK(book.group().group_id == 5);
  REQUIRE(book.group().members.size() == 1);
  // Un groupe non nul ne vide rien.
  CHECK(book.observations().size() == 1);

  SECTION("une réponse tronquée ne vide pas") {
    book.ReceiveGroup(MvpGroup(), false);
    CHECK(book.group().group_id == 0);
    CHECK(book.group().members.empty());
    CHECK(book.observations().size() == 1);
  }
  SECTION("« dans aucun groupe » vide") {
    book.ReceiveGroup(MvpGroup(), true);
    CHECK(book.group().group_id == 0);
    CHECK(book.observations().empty());
  }
  // L'invitation survit à tout paquet de groupe.
  CHECK(book.pending_invite() == 12);
  CHECK(book.pending_invite_name() == "Chasseurs");
}

TEST_CASE("Carnet MVP : l'invitation, le résultat, et ce que Clear garde", "[sim][mvp]") {
  MvpTrackerBook book;
  book.ReceiveInvitation(9, "Groupe");
  CHECK(book.pending_invite() == 9);
  book.ClearInvitation();
  CHECK(book.pending_invite() == 0);
  CHECK(book.pending_invite_name().empty());

  // Zéro efface de fait.
  book.ReceiveInvitation(9, "Groupe");
  book.ReceiveInvitation(0, "");
  CHECK(book.pending_invite() == 0);

  book.ReceiveResult(static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull), 4242);
  CHECK(book.last_result() == static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull));
  CHECK(book.last_result_ms() == 4242);
  // DEUX RÉSULTATS IDENTIQUES se comptent deux fois : chacun répond à sa
  // commande.
  const std::uint32_t results = book.result_serial();
  book.ReceiveResult(static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull), 4243);
  CHECK(book.result_serial() == results + 1);

  book.ReceiveCatalogue({Slot(1, 1039, "prt_maze03")});
  book.ReceiveFavorites({1});
  Observe(book, 1, Killed(kKillS));
  book.NoteServerTime(kLocalS + 60, kLocalS);
  book.ReceiveInvitation(3, "Autre");
  MvpGroup group;
  group.group_id = 8;
  book.ReceiveGroup(group, true);
  const std::uint32_t serial = book.catalog_serial();

  book.Clear();
  CHECK_FALSE(book.catalog_known());
  CHECK(book.slots().empty());
  CHECK(book.observations().empty());
  CHECK(book.favorites().empty());
  CHECK(book.group().group_id == 0);
  CHECK(book.pending_invite() == 0);
  CHECK_FALSE(book.clock_known());
  CHECK(book.ServerNow(kLocalS) == kLocalS);
  CHECK(book.catalog_serial() != serial);
  CHECK(book.last_result() == static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull));
  // Le dernier résultat, son heure et le compte des résultats restent.
  CHECK(book.last_result_ms() == 4243);
  CHECK(book.result_serial() == results + 1);
}

TEST_CASE("Carnet MVP : les queues lues prouvent la forme étendue, jusqu'à Clear", "[sim][mvp]") {
  // Les codes des résultats étendus sont ceux du fil.
  STATIC_CHECK(static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kObservationKept) == 14);
  STATIC_CHECK(static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kNameTooLong) == 17);

  MvpTrackerBook book;
  book.ReceiveInvitation(9, "Groupe");
  book.ReceiveResult(0, 1);
  MvpGroup plain;
  plain.group_id = 8;
  book.ReceiveGroup(plain, true);
  CHECK_FALSE(book.extensions_seen());
  CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kUnknown);
  CHECK_FALSE(book.last_result_tag().has_value());

  SECTION("le groupe qui dit notre compte") {
    MvpGroup told = plain;
    told.self_user_id = 0;
    book.ReceiveGroup(told, true);
    CHECK(book.extensions_seen());
  }
  SECTION("l'invitation qui dit son origine, gardée jusqu'à la suivante") {
    book.ReceiveInvitation(9, "Groupe", ro::sim::MvpInviteOrigin::kSnapshot);
    CHECK(book.extensions_seen());
    CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kSnapshot);
    // Une trame sans queue n'hérite pas de l'origine de la précédente.
    book.ReceiveInvitation(9, "Groupe");
    CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kUnknown);
  }
  SECTION(
      "le résultat étiqueté, qui ne prouve pas la forme étendue, et dont l'étiquette "
      "survit à Clear avec lui") {
    // Le serveur étiquette toute commande identifiée, capacité annoncée ou non.
    book.ReceiveResult(static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kObservationKept), 2,
                       ro::sim::MvpResultTag{.request_id = 41, .command = 10});
    CHECK_FALSE(book.extensions_seen());
    book.Clear();
    REQUIRE(book.last_result_tag().has_value());
    CHECK(book.last_result_tag()->request_id == 41);
    CHECK(book.last_result_tag()->command == 10);
  }
  // 🔴 UNE NOUVELLE SESSION SE PROUVE À NOUVEAU.
  book.Clear();
  CHECK_FALSE(book.extensions_seen());
  CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kUnknown);
}

TEST_CASE("Carnet MVP : les favoris", "[sim][mvp]") {
  MvpTrackerBook book;
  book.ReceiveFavorites({4, 9});
  CHECK(book.IsFavorite(4));
  CHECK(book.IsFavorite(9));
  CHECK_FALSE(book.IsFavorite(5));
  book.ReceiveFavorites({5});
  CHECK_FALSE(book.IsFavorite(4));
  CHECK(book.IsFavorite(5));
}

TEST_CASE("Carnet MVP : les tombes d'une carte", "[sim][mvp]") {
  MvpTrackerBook book;
  book.ReceiveCatalogue({Slot(1, 1039, "prt_maze03"), Slot(2, 1046, "prt_maze03"),
                         Slot(3, 1059, "gef_dun02"), Slot(4, 1086, "prt_maze03"),
                         Slot(5, 1087, "prt_maze03")});
  Observe(book, 1, Killed(kKillS, 100, 120));  // gardée
  Observe(book, 2, Killed(kKillS, -1, 50));    // tombe inconnue
  Observe(book, 3, Killed(kKillS, 10, 10));    // autre carte
  Observe(book, 4, Killed(0, 30, 40));         // sans fenêtre
  Observe(book, 5, Exact(kKillS + 60, 0, 0));  // exacte, la cellule (0, 0) est valable
  const std::int64_t to_s = kKillS + kDelay1S + kDelay2S;

  SECTION("une tombe connue, sur la carte, encore fraîche") {
    const std::vector<MvpTomb> tombs = MvpTombsOn(book, "PRT_MAZE03", kKillS);
    REQUIRE(tombs.size() == 2);
    CHECK(tombs[0].cell_x == 100);
    CHECK(tombs[0].cell_y == 120);
    CHECK_FALSE(tombs[0].exact);
    // Chaque tombe nomme son créneau : la minimap ouvre le carnet sur lui.
    CHECK(tombs[0].slot_id == 1);
    CHECK(tombs[1].cell_x == 0);
    CHECK(tombs[1].cell_y == 0);
    CHECK(tombs[1].exact);
    CHECK(tombs[1].slot_id == 5);
  }
  SECTION("une heure après la fin, elle tient encore ; une seconde de plus, non") {
    const std::vector<MvpTomb> last = MvpTombsOn(book, "prt_maze03", to_s + 3600);
    REQUIRE(last.size() == 1);
    CHECK(last[0].cell_x == 100);
    CHECK(MvpTombsOn(book, "prt_maze03", to_s + 3601).empty());
  }
  SECTION("l'instant exact vieillit comme les autres") {
    const std::int64_t exact_s = kKillS + 60;
    const std::vector<MvpTomb> fresh = MvpTombsOn(book, "prt_maze03", exact_s + 3600);
    // La tombe exacte est encore là ; l'autre aussi (sa fenêtre finit plus tard).
    CHECK(fresh.size() == 2);
    const std::vector<MvpTomb> aged = MvpTombsOn(book, "prt_maze03", exact_s + 3601);
    REQUIRE(aged.size() == 1);
    CHECK_FALSE(aged[0].exact);
  }
  SECTION("une autre carte") {
    const std::vector<MvpTomb> tombs = MvpTombsOn(book, "gef_dun02", kKillS);
    REQUIRE(tombs.size() == 1);
    CHECK(tombs[0].cell_x == 10);
    CHECK(MvpTombsOn(book, "prontera", kKillS).empty());
  }
}

TEST_CASE("Carnet MVP : l'icône s'allume pour un favori proche", "[sim][mvp]") {
  MvpTrackerBook book;
  book.ReceiveCatalogue({Slot(1, 1039, "prt_maze03"), Slot(2, 1046, "gef_dun02")});
  Observe(book, 1, Killed(kKillS));
  Observe(book, 2, Exact(kKillS));
  const std::int64_t from_s = kKillS + kDelay1S;
  const std::int64_t to_s = from_s + kDelay2S;
  constexpr int kLeadMin = 5;
  constexpr std::int64_t kLeadS = 300;

  SECTION("un non-favori n'allume rien") { CHECK_FALSE(MvpBadgeLit(book, from_s, kLeadMin)); }
  SECTION("une fenêtre : du préavis jusqu'à la fin plus une demi-heure") {
    book.ReceiveFavorites({1});
    CHECK_FALSE(MvpBadgeLit(book, from_s - kLeadS - 1, kLeadMin));
    CHECK(MvpBadgeLit(book, from_s - kLeadS, kLeadMin));
    CHECK(MvpBadgeLit(book, from_s, kLeadMin));
    CHECK(MvpBadgeLit(book, to_s + 1800, kLeadMin));
    CHECK_FALSE(MvpBadgeLit(book, to_s + 1801, kLeadMin));
    // Sans préavis : pas avant l'ouverture.
    CHECK_FALSE(MvpBadgeLit(book, from_s - 1, 0));
    CHECK(MvpBadgeLit(book, from_s, 0));
  }
  SECTION("un instant exact : un quart d'heure, pas plus") {
    book.ReceiveFavorites({2});
    CHECK(MvpBadgeLit(book, kKillS + 900, kLeadMin));
    CHECK_FALSE(MvpBadgeLit(book, kKillS + 901, kLeadMin));
    CHECK_FALSE(MvpBadgeLit(book, kKillS - kLeadS - 1, kLeadMin));
    CHECK(MvpBadgeLit(book, kKillS - kLeadS, kLeadMin));
  }
  SECTION("un favori sans fenêtre n'allume rien") {
    book.ReceiveFavorites({42});
    CHECK_FALSE(MvpBadgeLit(book, from_s, kLeadMin));
  }
}

TEST_CASE("Carnet MVP : une heure aberrante du fil s'arrête à la limite", "[sim][mvp]") {
  constexpr std::int64_t kLatestS = std::numeric_limits<std::int64_t>::max();
  constexpr std::int64_t kEarliestS = std::numeric_limits<std::int64_t>::min();
  constexpr int kLeadMin = 5;
  constexpr std::int64_t kLeadS = 300;
  MvpTrackerBook book;
  book.ReceiveCatalogue({Slot(1, 1039, "prt_maze03"), Slot(2, 1046, "prt_maze03")});
  Observe(book, 1, Exact(kLatestS, 10, 20));
  Observe(book, 2, Killed(kLatestS - 1, 30, 40));

  SECTION("la fenêtre d'une mort tout au bout reste au bout") {
    const std::optional<MvpWindow> window = book.WindowOf(2);
    REQUIRE(window.has_value());
    CHECK(window->from_s == kLatestS);
    CHECK(window->to_s == kLatestS);
  }
  SECTION("une tombe tout au bout n'a pas vieilli") {
    CHECK(MvpTombsOn(book, "prt_maze03", kLatestS).size() == 2);
  }
  SECTION("l'icône d'un instant tout au bout s'allume à son heure") {
    book.ReceiveFavorites({1});
    CHECK(MvpBadgeLit(book, kLatestS, kLeadMin));
    CHECK_FALSE(MvpBadgeLit(book, kLatestS - kLeadS - 1, kLeadMin));
  }
  SECTION("l'écart d'horloge s'arrête aux deux bouts") {
    book.NoteServerTime(kLatestS, -1);
    CHECK(book.ServerNow(kLocalS) == kLatestS);
    book.NoteServerTime(1, kLatestS);
    CHECK(book.ServerNow(kEarliestS) == kEarliestS);
  }
}
