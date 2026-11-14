// L'INTERRUPTEUR DU CARNET DE CHASSE MVP, contre un serveur en boucle locale :
// éteint, aucune porte n'ouvre, rien ne part, rien ne se montre, et seule la
// case le rallume.

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/mvp_tracker_window.h"

#include "mvp/mvp_tracker_stage.h"
#include "mvp_tracker_stage_bench.h"

using namespace mvp_tracker_stage_bench;

TEST_CASE("Carnet MVP, étage : éteint, aucune porte n'ouvre ni ne rallume", "[app][mvp]") {
  Bench bench;
  bench.Apply(Catalogue(kSlots));
  // TOUTES LES PORTES : la bascule de la touche et de l'icône, l'ouverture des
  // options, le lien, l'import, la tombe et l'alerte.
  bench.stage.RequestToggle();
  bench.stage.RequestOpen();
  bench.stage.RequestOpenOn(1);
  CHECK_FALSE(bench.Advance(true));
  CHECK_FALSE(bench.options.settings().enabled);
  CHECK_FALSE(bench.windows.window.open);
  CHECK_FALSE(bench.windows.enabled);
  CHECK(bench.Silent());
  // 🔴 UNE PORTE DEMANDÉE ÉTEINT ne s'ouvre pas à l'allumage. Rallumé, le carnet
  // se réabonne : l'instantané se redemande.
  bench.SwitchOn();
  bench.Advance(true);
  CHECK(bench.windows.enabled);
  CHECK_FALSE(bench.windows.window.open);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  CHECK(bench.Silent());
  // Allumé, la porte ouvre, et ne change aucun réglage.
  bench.stage.RequestOpen();
  CHECK_FALSE(bench.Advance(true));
  CHECK(bench.windows.window.open);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  // ÉTEINT, LA FENÊTRE OUVERTE SE FERME, et ce qu'elle demandait se perd.
  bench.windows.answer.leave_group = true;
  bench.windows.answer.alert_slot = 1;
  bench.Switch(false);
  bench.Advance(true);
  CHECK_FALSE(bench.windows.window.open);
  CHECK_FALSE(bench.windows.enabled);
  CHECK(bench.Silent());
  bench.Advance(true);
  CHECK_FALSE(bench.windows.window.open);
  // RALLUMÉ SANS QUITTER LA ZONE : réabonné, et toujours fermé.
  bench.Switch(true);
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  CHECK_FALSE(bench.windows.window.open);
}

TEST_CASE("Carnet MVP, étage : éteint, rien ne part, rien ne se montre, le menu le dit",
          "[app][mvp]") {
  Bench bench;
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Group(0, 9, "Chasse"));
  bench.Apply(Observations(1, {{.slot_id = 1, .source = 2, .kill_s = kLocalS - 3480}}));
  bench.Apply(Favorites({1}));
  bench.Apply(Group(1, 12, "Les Chasseurs"));
  const MvpLink known{.monster_id = 1039,
                      .map = "gef_dun03",
                      .kill_time_s = kLocalS - 600,
                      .tomb_x = 10,
                      .tomb_y = 20,
                      .name = "Baphomet"};
  bench.Advance(true);
  CHECK(bench.Silent());
  // RIEN NE SE MONTRE : ni alerte, ni son, ni signal de l'icône ; la vue le dit
  // à `ui`, qui ne dessine ni invitation ni ligne détachée.
  CHECK_FALSE(bench.stage.TakeAlertSound());
  CHECK_FALSE(bench.windows.badge_lit);
  REQUIRE(bench.windows.view != nullptr);
  CHECK_FALSE(bench.windows.view->enabled);
  CHECK(bench.windows.view->alerts.empty());
  // LE MENU D'UN <MVPL> : ce qui passe par le carnet se grise, avec la même
  // raison ; le guidage, qui ne lit que le lien, reste.
  const ro::ui::MvpLinkMenuFacts* facts = bench.stage.FactsFor(known);
  CHECK(Said(facts->import_refusal) == Said(ro::ui::MvpTrackerOffText()));
  CHECK(Said(facts->open_refusal) == Said(ro::ui::MvpTrackerOffText()));
  CHECK(facts->tomb_known);
  // LES GESTES QUI ENVOIENT : rien ne part, et le chat dit pourquoi.
  bench.stage.Import(known, "Sting", bench.link);
  bench.stage.Invite("Bob", bench.link);
  bench.windows.answer.accept_invite = true;
  bench.windows.answer.create_group = "Chasse";
  bench.Advance(true);
  CHECK(bench.Silent());
  CHECK(bench.stage.TakeNotices() ==
        std::vector<std::string>{
            "Carnet MVP : saisie de Baphomet non envoyée — Le carnet de chasse est éteint.",
            "Carnet MVP : invitation à Bob non envoyée — Le carnet de chasse est éteint."});
  CHECK_FALSE(bench.windows.window.open);

  // RALLUMÉ, tout revient : l'invitation gardée, l'alerte, le menu offert.
  bench.SwitchOn();
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  CHECK(bench.windows.view->enabled);
  CHECK(bench.windows.view->pending_invite == 12);
  CHECK(bench.stage.TakeAlertSound());
  CHECK(bench.stage.FactsFor(known)->open_refusal == nullptr);
  CHECK(Said(bench.stage.FactsFor(known)->import_refusal) != Said(ro::ui::MvpTrackerOffText()));
}
