// LES RÈGLES DU JOURNAL DES QUÊTES : ce qu'une chasse écrit, le temps qui
// reste, ce qu'une recherche trouve, l'ordre des onglets et celui du suivi.

#include "quest/quest_rules.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::QuestCatalog;
using moonclient::QuestQuery;
using moonclient::QuestSortFacts;
using ro::sim::QuestHunt;
using ro::sim::QuestRecord;

namespace {

constexpr const char* kCatalog = R"(version: 1
language: en
categories:
  - { key: all, label: "All" }
  - { key: etc, label: "Other" }
  - { key: daily, label: "Daily", icons: [dq] }
quests:
  10:
    title: "Zebra hunt"
    icon: dq
    summary: "Talk to ^0000FFthe guard^000000"
    npc: { sprite: X, map: prontera, x: 150, y: 180 }
  20:
    title: "apple picking"
    description: "Go to <NAVI>the orchard<INFO>payon,10,20,0,101,0,</INFO></NAVI> now."
    npc: { sprite: Y, map: payon, x: 1, y: 2 }
  30:
    title: "Middle"
  50:
    title: "Come back tomorrow"
    cooldown: true
)";

[[nodiscard]] QuestCatalog Catalog() {
  auto catalog = QuestCatalog::Parse(kCatalog, "en");
  REQUIRE(catalog.has_value());
  return std::move(*catalog);
}

[[nodiscard]] QuestHunt Hunt(std::int16_t killed, std::int16_t total) {
  return QuestHunt{.mob_id = 1002, .killed = killed, .total = total, .mob_name = "Poring"};
}

}  // namespace

TEST_CASE("Quêtes : le libellé d'une chasse borne ses niveaux", "[app][quetes]") {
  QuestHunt hunt = Hunt(0, 10);
  CHECK(moonclient::HuntLabel(hunt) == "Poring");
  hunt.level_min = 5;
  CHECK(moonclient::HuntLabel(hunt) == "Poring Lv.5");
  hunt.level_max = 12;
  CHECK(moonclient::HuntLabel(hunt) == "Poring Lv.5~Lv.12");
  hunt.level_min = 0;
  CHECK(moonclient::HuntLabel(hunt) == "Poring Lv.12");
}

TEST_CASE("Quêtes : une chasse générique se nomme par ses critères", "[app][quetes]") {
  // Sans monstre : la description du serveur, en anglais, se relit.
  QuestHunt hunt{.mob_id = 0, .mob_name = "prt_fild08, Brute, Small, Fire Element"};
  CHECK(moonclient::HuntTargetName(hunt) ==
        "Monstres : race Bête, taille Petite, élément Feu, sur prt_fild08");
  hunt.level_min = 10;
  CHECK(moonclient::HuntLabel(hunt) ==
        "Monstres : race Bête, taille Petite, élément Feu, sur prt_fild08 Lv.10");

  // Le champ tronque la description : le dernier critère coupé se reconnaît.
  hunt = QuestHunt{.mob_id = 0, .mob_name = "Demihuman, Medium, Neutr"};
  CHECK(moonclient::HuntTargetName(hunt) ==
        "Monstres : race Demi-humain, taille Moyenne, élément Neutre");
  hunt.mob_name = "Demihuman, Medium,";
  CHECK(moonclient::HuntTargetName(hunt) == "Monstres : race Demi-humain, taille Moyenne");

  // Mort-vivant : la race précède l'élément, l'élément porte « Element ».
  hunt.mob_name = "Undead, Undead Element";
  CHECK(moonclient::HuntTargetName(hunt) == "Monstres : race Mort-vivant, élément Mort-vivant");

  // Tous critères ouverts : rien à dire du monstre.
  hunt.mob_name = "";
  CHECK(moonclient::HuntTargetName(hunt) == "Monstres de toute sorte");

  // Une description qui ne se lit pas paraît telle quelle.
  hunt.mob_name = "Small, Brute";
  CHECK(moonclient::HuntTargetName(hunt) == "Small, Brute");

  // Un monstre précis garde son nom, même s'il ressemble à un critère.
  hunt = QuestHunt{.mob_id = 1002, .mob_name = "Brute"};
  CHECK(moonclient::HuntTargetName(hunt) == "Brute");
}

TEST_CASE("Quêtes : les lignes de la conversation sont celles du client d'origine",
          "[app][quetes]") {
  // Une quête reçue ne se nomme pas.
  CHECK(moonclient::QuestReceivedLine() == "Vous avez reçu une nouvelle quête.");
  // Un retrait se dit sous son titre, même vide.
  CHECK(moonclient::QuestDeletedLine("Zebra hunt") == "La quête [Zebra hunt] a été supprimée.");
  CHECK(moonclient::QuestDeletedLine("") == "La quête [] a été supprimée.");
  // Une chasse : le titre, la cible, le compte puis le total.
  CHECK(moonclient::QuestHuntLine("Zebra hunt", Hunt(4, 10)) ==
        "Quête [Zebra hunt] : vaincre [Poring], progression (4/10).");
  // ⚠ SANS TITRE, RIEN : le client d'origine se tait.
  CHECK(moonclient::QuestHuntLine("", Hunt(4, 10)).empty());
}

TEST_CASE("Quêtes : le temps qui reste, sa hâte et son échéance", "[app][quetes]") {
  constexpr std::int64_t kNow = 1'700'000'000;
  CHECK(moonclient::TimeLeftOf(0, kNow).text.empty());
  const auto days = moonclient::TimeLeftOf(kNow + 2 * 86400 + 3 * 3600, kNow);
  CHECK(days.text == "2 j 3 h");
  CHECK_FALSE(days.urgent);
  const auto hours = moonclient::TimeLeftOf(kNow + 2 * 3600 + 13 * 60, kNow);
  CHECK(hours.text == "2 h 13 min");
  const auto minutes = moonclient::TimeLeftOf(kNow + 12 * 60 + 5, kNow);
  CHECK(minutes.text == "12 min");
  CHECK(minutes.urgent);
  CHECK(moonclient::TimeLeftOf(kNow + 30, kNow).text == "moins d'une minute");
  const auto expired = moonclient::TimeLeftOf(kNow - 1, kNow);
  CHECK(expired.expired);
  CHECK(expired.text == "expirée");
}

TEST_CASE("Quêtes : l'avancée ne compte que les totaux connus", "[app][quetes]") {
  QuestRecord quest{.quest_id = 1};
  CHECK(moonclient::QuestProgress(quest) < 0.0F);
  CHECK_FALSE(moonclient::ObjectivesMet(quest));
  quest.hunts = {Hunt(5, 10), Hunt(3, 0)};
  CHECK(moonclient::QuestProgress(quest) == 0.5F);
  CHECK_FALSE(moonclient::ObjectivesMet(quest));
  quest.hunts = {Hunt(10, 10), Hunt(15, 10)};
  CHECK(moonclient::QuestProgress(quest) == 1.0F);
  CHECK(moonclient::ObjectivesMet(quest));
}

TEST_CASE("Quêtes : le texte lisible perd couleurs et charges de liens", "[app][quetes]") {
  CHECK(moonclient::PlainQuestText("Talk to ^0000FFthe guard^000000.") == "Talk to the guard.");
  CHECK(moonclient::PlainQuestText("Go <NAVI>here<INFO>payon,1,2</INFO></NAVI>!") == "Go here!");
}

TEST_CASE("Quêtes : la recherche lit titre, résumé, description et chasses", "[app][quetes]") {
  const QuestCatalog catalog = Catalog();
  const QuestRecord zebra{.quest_id = 10, .hunts = {Hunt(0, 5)}};
  const QuestRecord apple{.quest_id = 20};
  CHECK(moonclient::QuestMatches(zebra, catalog.Find(10), "ZEBRA"));
  CHECK(moonclient::QuestMatches(zebra, catalog.Find(10), "the guard"));
  CHECK(moonclient::QuestMatches(zebra, catalog.Find(10), "poring"));
  CHECK(moonclient::QuestMatches(zebra, catalog.Find(10), "prontera"));
  CHECK(moonclient::QuestMatches(apple, catalog.Find(20), "orchard"));
  // La charge d'un lien ne se cherche pas : elle ne se lit pas.
  CHECK_FALSE(moonclient::QuestMatches(apple, catalog.Find(20), "101"));
  CHECK(moonclient::QuestMatches(apple, nullptr, ""));
}

TEST_CASE("Quêtes : un onglet filtre, cherche et trie", "[app][quetes]") {
  const QuestCatalog catalog = Catalog();
  constexpr std::int64_t kNow = 1'700'000'000;
  const std::vector<QuestRecord> quests = {
      QuestRecord{.quest_id = 10, .active = true, .expires_unix_s = kNow + 500},
      QuestRecord{.quest_id = 20, .active = true, .expires_unix_s = 0, .hunts = {Hunt(9, 10)}},
      QuestRecord{.quest_id = 30, .active = false},
      QuestRecord{.quest_id = 40, .active = true, .expires_unix_s = kNow + 100}};
  QuestSortFacts facts;
  facts.received_unix_s = {{10, 3}, {20, 1}, {40, 2}};

  const auto ids = [](const std::vector<const QuestRecord*>& rows) {
    std::vector<std::uint32_t> out;
    for (const QuestRecord* row : rows) {
      out.push_back(row->quest_id);
    }
    return out;
  };
  QuestQuery query;
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)) ==
        std::vector<std::uint32_t>{10, 40, 20});
  query.sort = ro::ui::QuestSort::kTimeLeft;
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)) ==
        std::vector<std::uint32_t>{40, 10, 20});
  query.sort = ro::ui::QuestSort::kName;
  // « apple » avant « Zebra » : la casse ne compte pas ; une quête inconnue du
  // catalogue n'a pas de titre et vient d'abord.
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)) ==
        std::vector<std::uint32_t>{40, 20, 10});
  query.sort = ro::ui::QuestSort::kProgress;
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)).front() == 20);
  query.category = "daily";
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)) ==
        std::vector<std::uint32_t>{10});
  query.category = "etc";
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)) ==
        std::vector<std::uint32_t>{20, 40});
  query = QuestQuery{.tab = ro::ui::QuestTab::kSuspended};
  CHECK(ids(moonclient::QuestsOfTab(quests, &catalog, query, facts, kNow)) ==
        std::vector<std::uint32_t>{30});
}

TEST_CASE("Quêtes : une attente quitte « En cours » pour « En attente »", "[app][quetes]") {
  const QuestCatalog catalog = Catalog();
  constexpr std::int64_t kNow = 1'700'000'000;
  const std::vector<QuestRecord> quests = {
      QuestRecord{.quest_id = 10, .active = true},
      QuestRecord{.quest_id = 50, .active = true, .expires_unix_s = kNow + 60},
      QuestRecord{.quest_id = 30, .active = false}};
  const auto ids = [&](ro::ui::QuestTab tab) {
    std::vector<std::uint32_t> out;
    for (const QuestRecord* row :
         moonclient::QuestsOfTab(quests, &catalog, QuestQuery{.tab = tab}, {}, kNow)) {
      out.push_back(row->quest_id);
    }
    return out;
  };
  CHECK(ids(ro::ui::QuestTab::kActive) == std::vector<std::uint32_t>{10});
  CHECK(ids(ro::ui::QuestTab::kWaiting) == std::vector<std::uint32_t>{50});
  CHECK(ids(ro::ui::QuestTab::kSuspended) == std::vector<std::uint32_t>{30});
  CHECK(ids(ro::ui::QuestTab::kRecommended).empty());
  // UNE ATTENTE SUSPENDUE va aux suspendues : le cadenas est un choix.
  const QuestRecord locked{.quest_id = 50, .active = false};
  CHECK(moonclient::QuestInTab(locked, catalog.Find(50), ro::ui::QuestTab::kSuspended));
  CHECK_FALSE(moonclient::QuestInTab(locked, catalog.Find(50), ro::ui::QuestTab::kWaiting));
  // Une quête inconnue du catalogue n'est pas une attente.
  CHECK(moonclient::QuestInTab(QuestRecord{.quest_id = 99, .active = true}, nullptr,
                               ro::ui::QuestTab::kActive));
  // Il n'y a rien à faire d'une attente : elle ne paraît pas au suivi.
  const std::vector<std::uint32_t> tracked = {10, 50};
  const auto shown = moonclient::TrackedQuests(quests, tracked, &catalog, "", false);
  REQUIRE(shown.size() == 1);
  CHECK(shown.front()->quest_id == 10);
}

TEST_CASE("Quêtes : une attente compte vers « prête », sans jamais presser", "[app][quetes]") {
  constexpr std::int64_t kNow = 1'700'000'000;
  const moonclient::QuestTimeLeft soon = moonclient::WaitLeftOf(kNow + 120, kNow);
  CHECK_FALSE(soon.urgent);
  CHECK_FALSE(soon.expired);
  CHECK(soon.text == moonclient::TimeLeftOf(kNow + 120, kNow).text);
  const moonclient::QuestTimeLeft over = moonclient::WaitLeftOf(kNow - 1, kNow);
  CHECK(over.expired);
  CHECK(over.text == "prête");
  CHECK(moonclient::WaitLeftOf(0, kNow).text.empty());
}

TEST_CASE("Quêtes : le suivi met la carte courante d'abord", "[app][quetes]") {
  const QuestCatalog catalog = Catalog();
  const std::vector<QuestRecord> quests = {QuestRecord{.quest_id = 10, .active = true},
                                           QuestRecord{.quest_id = 20, .active = true},
                                           QuestRecord{.quest_id = 30, .active = false}};
  const std::vector<std::uint32_t> tracked = {10, 20, 30};
  const auto first = [&](bool current_map_first) {
    return moonclient::TrackedQuests(quests, tracked, &catalog, "payon", current_map_first);
  };
  // Une quête suspendue ne se suit pas à l'écran.
  REQUIRE(first(false).size() == 2);
  CHECK(first(false).front()->quest_id == 10);
  CHECK(first(true).front()->quest_id == 20);
}

TEST_CASE("Quêtes : la bulle de suivi compte les tués, puis dit « terminé »", "[app][quest]") {
  CHECK(moonclient::HuntBalloonLine(Hunt(3, 10)) == "Poring [ 3 / 10 ]");
  CHECK(moonclient::HuntBalloonLine(Hunt(10, 10)) == "Poring [ Terminé ]");
  // ⚠ « PLEIN » SE LIT À L'ÉGALITÉ : un compte qui dépasse garde ses chiffres.
  CHECK(moonclient::HuntBalloonLine(Hunt(11, 10)) == "Poring [ 11 / 10 ]");
  // La tranche de niveaux suit le nom, comme dans le suivi.
  QuestHunt ranged = Hunt(1, 5);
  ranged.level_min = 10;
  ranged.level_max = 20;
  CHECK(moonclient::HuntBalloonLine(ranged) == "Poring Lv.10~Lv.20 [ 1 / 5 ]");
}
