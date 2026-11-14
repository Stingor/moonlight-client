// LA FICHE D'UN MONSTRE, vue de `app` : ce qu'on demande au serveur, quand, et
// ce que le relevé de Sense ouvre.
//
// 🔴 CE BANC TIENT QUATRE RÈGLES QUE LE JEU NE MONTRERAIT QU'À LA LONGUE :
//
//   · une demande en vol ne repart pas avant cinq secondes ;
//   · une fiche prête ne se redemande QUE si son EXP estimée a vieilli — un
//     niveau de plus — et la question se repose À L'IDENTIQUE : une classe de
//     sprite redemandée comme identifiant de base désignerait un autre monstre ;
//   · un monstre inconnu du serveur ne se redemande jamais ;
//   · le jeu refuse de montrer cinq Gardiens de forteresse, et le réglage lève
//     ce refus.

#include "world/monster_sheet.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/monster_sheet.h"
#include "ro/sim/player_status.h"
#include "ro/ui/monster_sheet_window.h"

using moonclient::MonsterSheet;

namespace {

constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kArcherGuardian = 1285;
constexpr std::uint32_t kPatienceMs = 5000;
constexpr std::uint32_t kStartMs = 1000;
// La fenêtre de la fiche est ouverte.
constexpr bool kOpen = true;
constexpr std::uint32_t kBaphomet = 1039;

[[nodiscard]] MonsterSheet Bare() {
  return MonsterSheet(nullptr, nullptr, nullptr, nullptr, nullptr);
}

[[nodiscard]] ro::sim::PlayerStatus AtLevel(std::int64_t base_level) {
  ro::sim::PlayerStatus status;
  status.base_level = base_level;
  status.job_level = 1;
  return status;
}

[[nodiscard]] ro::sim::MonsterSheet WithGain() {
  ro::sim::MonsterSheet sheet;
  sheet.name = "Poring";
  sheet.exp_gain = ro::sim::MonsterExpGain{.base_exp = 10, .next_base_exp = 100};
  return sheet;
}

}  // namespace

TEST_CASE("Fiche de monstre : une demande en vol attend sa réponse", "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/false);
  CHECK(state.open);
  CHECK(state.raise);

  const auto first = sheet.NextAsk(AtLevel(10), kOpen, kStartMs);
  REQUIRE(first.has_value());
  CHECK(first->monster_id == kPoring);
  CHECK_FALSE(first->by_view);
  CHECK_FALSE(sheet.NextAsk(AtLevel(10), kOpen, kStartMs + kPatienceMs - 1).has_value());
  // Sans réponse au bout de cinq secondes, elle repart.
  CHECK(sheet.NextAsk(AtLevel(10), kOpen, kStartMs + kPatienceMs).has_value());
}

TEST_CASE("Fiche de monstre : une fiche prête ne se redemande qu'au niveau suivant",
          "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/true);
  REQUIRE(sheet.NextAsk(AtLevel(10), kOpen, kStartMs).has_value());
  sheet.book().Receive(kPoring, WithGain());

  // La réponse est notée au niveau dix : rien à redemander tant qu'il dure.
  CHECK_FALSE(sheet.NextAsk(AtLevel(10), kOpen, kStartMs + kPatienceMs).has_value());
  // Un niveau de plus : l'EXP estimée a vieilli, et la MÊME question repart.
  const auto again = sheet.NextAsk(AtLevel(11), kOpen, kStartMs + kPatienceMs + 1);
  REQUIRE(again.has_value());
  CHECK(again->by_view);
}

TEST_CASE("Fiche de monstre : sans EXP estimée, une fiche prête est définitive",
          "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/false);
  REQUIRE(sheet.NextAsk(AtLevel(10), kOpen, kStartMs).has_value());
  sheet.book().Receive(kPoring, ro::sim::MonsterSheet{});
  CHECK_FALSE(sheet.NextAsk(AtLevel(99), kOpen, kStartMs + kPatienceMs).has_value());
}

TEST_CASE("Fiche de monstre : un monstre inconnu ne se redemande jamais", "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/false);
  REQUIRE(sheet.NextAsk(AtLevel(10), kOpen, kStartMs).has_value());
  sheet.book().NoteUnknown(kPoring);
  CHECK_FALSE(sheet.NextAsk(AtLevel(10), kOpen, kStartMs + kPatienceMs * 2).has_value());

  const ro::ui::MonsterSheetView& view = sheet.Refresh(state, AtLevel(10), true, false, kStartMs);
  CHECK(view.phase == ro::ui::MonsterSheetPhase::kUnknown);
}

TEST_CASE("Fiche de monstre : Sense ouvre la fiche, sauf sur un Gardien", "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  sheet.book().ReceiveSense(ro::sim::SenseReading{.sprite_class = kArcherGuardian});
  CHECK_FALSE(sheet.TakeSense(/*show_guardians=*/false).has_value());
  // Un relevé ne se lit qu'une fois, qu'il ait été retenu ou non.
  CHECK_FALSE(sheet.TakeSense(/*show_guardians=*/true).has_value());

  sheet.book().ReceiveSense(ro::sim::SenseReading{.sprite_class = kArcherGuardian});
  const auto opened = sheet.TakeSense(/*show_guardians=*/true);
  REQUIRE(opened.has_value());
  CHECK(*opened == kArcherGuardian);

  sheet.book().ReceiveSense(ro::sim::SenseReading{.sprite_class = kPoring});
  CHECK(sheet.TakeSense(/*show_guardians=*/false) == kPoring);
}

TEST_CASE("Fiche de monstre : la carte où on l'a vu suit le monstre montré",
          "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/true);
  sheet.NoteSeenOn("prt_fild08");
  sheet.book().Receive(kPoring, WithGain());
  // Le rapport de bug lit la vue de la dernière image : son nom y est.
  CHECK(sheet.Refresh(state, AtLevel(10), true, false, kStartMs).name == "Poring");
  CHECK(sheet.view().name == "Poring");
  // Le même monstre, rouvert ou repris par la base, garde sa carte…
  sheet.Show(kPoring);
  sheet.Open(state, kPoring, /*by_view=*/false);
  CHECK(sheet.seen_map() == "prt_fild08");
  // …un autre monstre ne l'hérite pas…
  sheet.Open(state, kBaphomet, /*by_view=*/false);
  CHECK(sheet.seen_map().empty());
  // …et un changement de carte l'oublie.
  sheet.Open(state, kPoring, /*by_view=*/true);
  sheet.NoteSeenOn("prt_fild08");
  sheet.Forget();
  CHECK(sheet.seen_map().empty());
}

TEST_CASE("Fiche de monstre : le qualificatif ne dit que ce qui est sûr", "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/false);
  const auto qualifier = [&](const char* aegis, bool summoned) {
    ro::sim::MonsterSheet told;
    told.aegis_name = aegis;
    told.summoned = summoned;
    sheet.book().Receive(kPoring, told);
    return sheet.Refresh(state, AtLevel(10), true, false, kStartMs).qualifier;
  };
  CHECK(qualifier("EVENT_PORING", false) != nullptr);
  CHECK(qualifier("G_PORING", true) != nullptr);
  CHECK(qualifier("E_PORING", false) != nullptr);
  // ⚠ UN PRÉFIXE SANS SENS ÉTABLI NE DIT RIEN : certains sont de vrais boss.
  CHECK(qualifier("B_SEYREN", false) == nullptr);
  CHECK(qualifier("PORING", false) == nullptr);
  // Le préfixe seul n'est pas un nom : « EVENT_ » ne se qualifie pas lui-même.
  CHECK(qualifier("EVENT_", false) == nullptr);
}

TEST_CASE("Fiche de monstre : l'aperçu ne vit que l'image où on le survole",
          "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  const ro::ui::MonsterSheetPreviewState box;
  CHECK(sheet.Preview(box, true, kStartMs) == nullptr);

  sheet.NoteHovered(kBaphomet, /*by_view=*/false);
  const ro::ui::MonsterSheetPreview* asking = sheet.Preview(box, true, kStartMs);
  REQUIRE(asking != nullptr);
  CHECK(asking->phase == ro::ui::MonsterSheetPhase::kAsking);
  // ⚠ LA FENÊTRE FERMÉE, L'APERÇU DEMANDE QUAND MÊME ce qu'il survole.
  const auto ask = sheet.NextAsk(AtLevel(10), /*sheet_open=*/false, kStartMs);
  REQUIRE(ask.has_value());
  CHECK(ask->monster_id == kBaphomet);
  CHECK_FALSE(ask->by_view);

  ro::sim::MonsterSheet told;
  told.name = "Baphomet";
  told.boss_rank = 2;
  told.hp = 668000;
  sheet.book().Receive(kBaphomet, told);
  sheet.NoteHovered(kBaphomet, /*by_view=*/false);
  const ro::ui::MonsterSheetPreview* ready = sheet.Preview(box, true, kStartMs + 1);
  REQUIRE(ready != nullptr);
  CHECK(ready->phase == ro::ui::MonsterSheetPhase::kReady);
  CHECK(ready->name == "Baphomet");
  CHECK(ready->boss_rank == 2);
  CHECK(ready->hp == 668000);

  // La souris partie, rien ne se montre ni ne se demande plus.
  CHECK(sheet.Preview(box, true, kStartMs + 2) == nullptr);
  CHECK_FALSE(
      sheet.NextAsk(AtLevel(10), /*sheet_open=*/false, kStartMs + kPatienceMs * 2).has_value());
}

TEST_CASE("Fiche de monstre : la fenêtre ouverte passe avant l'aperçu", "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/false);
  sheet.NoteHovered(kBaphomet, /*by_view=*/false);
  static_cast<void>(sheet.Preview(ro::ui::MonsterSheetPreviewState{}, true, kStartMs));
  const auto first = sheet.NextAsk(AtLevel(10), kOpen, kStartMs);
  REQUIRE(first.has_value());
  CHECK(first->monster_id == kPoring);
  // Celle de la fenêtre partie, l'aperçu a son tour à l'image suivante.
  const auto second = sheet.NextAsk(AtLevel(10), kOpen, kStartMs + 1);
  REQUIRE(second.has_value());
  CHECK(second->monster_id == kBaphomet);
}

TEST_CASE("Fiche de monstre : la ligne de pied vit et meurt avec son survol",
          "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  const ro::ui::MonsterSheetPreviewState box;
  sheet.NoteHovered(kBaphomet, /*by_view=*/false, "Maj+clic : lien dans le chat");
  const ro::ui::MonsterSheetPreview* hinted = sheet.Preview(box, true, kStartMs);
  REQUIRE(hinted != nullptr);
  CHECK(hinted->hint == "Maj+clic : lien dans le chat");
  // UN SURVOL SANS LIGNE n'hérite pas de celle du précédent.
  sheet.NoteHovered(kBaphomet, /*by_view=*/false);
  const ro::ui::MonsterSheetPreview* bare = sheet.Preview(box, true, kStartMs + 1);
  REQUIRE(bare != nullptr);
  CHECK(bare->hint.empty());
  // UN SURVOL NUL ne pose ni monstre ni ligne.
  sheet.NoteHovered(0, /*by_view=*/false, "Maj+clic : lien dans le chat");
  CHECK(sheet.Preview(box, true, kStartMs + 2) == nullptr);
  sheet.NoteHovered(kBaphomet, /*by_view=*/false);
  const ro::ui::MonsterSheetPreview* again = sheet.Preview(box, true, kStartMs + 3);
  REQUIRE(again != nullptr);
  CHECK(again->hint.empty());
}

TEST_CASE("Fiche de monstre : la base la remplit en attendant le serveur, qui la remplace",
          "[app][monster_sheet][database]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  // HÉBERGÉE PAR LA BASE DE DONNÉES : la fenêtre à elle reste fermée.
  sheet.Show(kPoring);
  CHECK_FALSE(state.open);
  CHECK_FALSE(sheet.shown_by_view());
  CHECK(sheet.Refresh(state, AtLevel(10), true, false, kStartMs).phase ==
        ro::ui::MonsterSheetPhase::kAsking);

  ro::sim::MonsterSheet base;
  base.name = "Poring";
  base.written_exp = ro::sim::MonsterWrittenExp{.base_exp = 2, .base_rate_percent = 100000};
  base.drops = {ro::sim::MonsterDrop{.item_id = 909, .rate_hundredths = 7000}};
  sheet.NoteBase(kPoring, base);
  CHECK(sheet.HasBase(kPoring));
  const ro::ui::MonsterSheetView& from_base =
      sheet.Refresh(state, AtLevel(10), true, false, kStartMs);
  CHECK(from_base.phase == ro::ui::MonsterSheetPhase::kReady);
  CHECK(from_base.name == "Poring");
  // La base ne porte pas les résistances : leur onglet n'existe pas.
  CHECK_FALSE(from_base.resist_known);
  REQUIRE(from_base.drops.size() == 1);
  // Le serveur est toujours interrogé : sa fiche est plus complète.
  CHECK(sheet.NextAsk(AtLevel(10), kOpen, kStartMs).has_value());

  ro::sim::MonsterSheet answered = WithGain();
  answered.drops = {ro::sim::MonsterDrop{.item_id = 909, .rate_hundredths = 7000},
                    ro::sim::MonsterDrop{.item_id = 501, .rate_hundredths = 100}};
  sheet.book().Receive(kPoring, answered);
  const ro::ui::MonsterSheetView& from_server =
      sheet.Refresh(state, AtLevel(10), true, false, kStartMs);
  CHECK(from_server.resist_known);
  CHECK(from_server.exp_gain.has_value());
  CHECK(from_server.drops.size() == 2);
  // L'EXP écrite reste celle de la base : le serveur ne l'envoie pas.
  REQUIRE(from_server.written_exp.has_value());
  CHECK(from_server.written_exp->base_rate_percent == 100000);
  sheet.NoteDatabaseLink(true);
  CHECK(from_server.database_link);
  // Une fiche ouverte par Sense ne se range pas dans la base.
  sheet.Open(state, kBaphomet, /*by_view=*/true);
  CHECK(sheet.shown_by_view());
}

TEST_CASE("Fiche de monstre : les map drops s'ajoutent à la fiche du serveur",
          "[app][monster_sheet]") {
  MonsterSheet sheet = Bare();
  ro::ui::MonsterSheetWindowState state;
  sheet.Open(state, kPoring, /*by_view=*/false);
  sheet.book().Receive(kPoring, WithGain());
  CHECK(sheet.Refresh(state, AtLevel(10), true, false, kStartMs).map_drops.empty());
  CHECK_FALSE(sheet.HasMapDrops(kPoring));

  // ⚠ NOTÉS APRÈS LA COMPOSITION DES LISTES : elles se refont.
  constexpr std::uint32_t kJellopy = 909;
  sheet.NoteMapDrops(kPoring, {ro::sim::MonsterMapDrop{.item_id = kJellopy,
                                                       .rate_thousandths = 25,
                                                       .random_options = true,
                                                       .map_name = "gef_d01_i",
                                                       .name = "Jellopy"}});
  CHECK(sheet.HasMapDrops(kPoring));
  const ro::ui::MonsterSheetView& view = sheet.Refresh(state, AtLevel(10), true, false, kStartMs);
  REQUIRE(view.map_drops.size() == 1);
  CHECK(view.map_drops[0].item_id == kJellopy);
  CHECK(view.map_drops[0].map_name == "gef_d01_i");
  CHECK(view.map_drops[0].map_label == "gef_d01_i");
  CHECK(view.map_drops[0].rate_thousandths == 25);
  CHECK(view.map_drops[0].random_options);

  // Un autre monstre n'en hérite pas.
  sheet.Open(state, kBaphomet, /*by_view=*/false);
  sheet.book().Receive(kBaphomet, WithGain());
  CHECK(sheet.Refresh(state, AtLevel(10), true, false, kStartMs).map_drops.empty());
}
