// LES TRAJECTOIRES PAR TYPE DE DÉGÂT : laquelle chaque nombre suit, ce que ses
// ancres montrent, et ce qu'une ancre tirée change.

#include "options/damage_path_anchors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/path_canvas.h"

#include "world/hit_popup_rules.h"
#include "world/number_paths.h"

namespace {

using moonclient::DamagePath;
using moonclient::GameOption;
using moonclient::HitPopupLook;
using moonclient::NumberPath;

[[nodiscard]] NumberPath& Ordinary(HitPopupLook& look) {
  return look.paths[static_cast<std::size_t>(DamagePath::kOnMonster)];
}

[[nodiscard]] const ro::ui::PathAnchor* AnchorOf(const std::vector<ro::ui::PathAnchor>& anchors,
                                                 GameOption id) {
  for (const ro::ui::PathAnchor& anchor : anchors) {
    if (anchor.id == static_cast<int>(id)) {
      return &anchor;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("Dégâts : chaque type a sa vie et son chemin, ceux du client par défaut",
          "[app][degats]") {
  const HitPopupLook look;
  // Le coup ordinaire jaillit : sa cloche monte puis retombe sous son point de
  // naissance, et il dérive de côté.
  const NumberPath& ordinary = moonclient::PathOf(look, DamagePath::kOnMonster);
  CHECK(ordinary.arrival_y_pixels < 0.0F);
  CHECK(ordinary.bend_one_y_pixels > 0.0F);
  CHECK(ordinary.bend_one_y_pixels == ordinary.bend_two_y_pixels);
  CHECK(ordinary.facing_drift);
  // Le total monte droit, sans dériver.
  const NumberPath& total = moonclient::PathOf(look, DamagePath::kTotal);
  CHECK(total.arrival_y_pixels > 0.0F);
  CHECK(total.bend_one_y_pixels == 0.0F);
  CHECK_FALSE(total.facing_drift);

  // Leur vie est celle du client : le coup jaillit grand et s'éteint plus tôt
  // que le total, qui naît petit.
  const std::optional<ro::render::NumberLife> blow =
      moonclient::PathLifeOf(look, DamagePath::kOnMonster);
  const std::optional<ro::render::NumberLife> sum =
      moonclient::PathLifeOf(look, DamagePath::kTotal);
  REQUIRE(blow.has_value());
  REQUIRE(sum.has_value());
  CHECK(blow->birth_scale > 1.0F);
  CHECK(sum->birth_scale < 1.0F);
  CHECK(blow->life_ms < sum->life_ms);
  // Les mots n'en ont pas : leur animation tient la leur.
  CHECK_FALSE(moonclient::PathLifeOf(look, DamagePath::kWords).has_value());

  // Un type se règle seul : les autres ne bougent pas.
  HitPopupLook tuned;
  moonclient::PathOf(tuned, DamagePath::kCritical).lift_pixels = -10.0F;
  CHECK(moonclient::PathOf(tuned, DamagePath::kOnMonster).lift_pixels == 0.0F);
}

TEST_CASE("Dégâts : le chemin d'un type se plie dans l'allure, écart au hasard compris",
          "[app][degats]") {
  ro::render::NumberLook motion;
  motion.scale = 1.5F;
  NumberPath path;
  path.start_x_pixels = 4.0F;
  path.lift_pixels = 8.0F;
  path.arrival_x_pixels = 20.0F;
  path.arrival_y_pixels = -6.0F;
  path.bend_one_y_pixels = 12.0F;
  path.bend_two_x_pixels = -4.0F;
  const ro::render::NumberLife life{.birth_scale = 2.0F, .life_ms = 900.0F};
  const ro::render::NumberLook placed = moonclient::OnPath(motion, path, life, {3.0F, -1.0F});
  CHECK(placed.scale == 1.5F);
  CHECK(placed.start_x_pixels == 4.0F);
  CHECK(placed.lift_pixels == 8.0F);
  CHECK(placed.arrival_x_pixels == 23.0F);
  CHECK(placed.arrival_y_pixels == -7.0F);
  CHECK(placed.bend_one_y_pixels == 12.0F);
  CHECK(placed.bend_two_x_pixels == -4.0F);
  REQUIRE(placed.life.has_value());
  CHECK(placed.life->life_ms == 900.0F);
  // Sans vie — les mots —, celle du client.
  CHECK_FALSE(moonclient::OnPath(motion, path, std::nullopt, {0.0F, 0.0F}).life.has_value());
}

TEST_CASE("Dégâts : chaque coup de la boucle d'essai montre le type qu'on lui demande",
          "[app][degats]") {
  const HitPopupLook look;
  for (const DamagePath path : {DamagePath::kOnMonster, DamagePath::kSkillOnMonster,
                                DamagePath::kOnPlayer, DamagePath::kCritical, DamagePath::kWords}) {
    const moonclient::Rehearsal shown = moonclient::PathRehearsal(path);
    const moonclient::StrikePlan plan = moonclient::PlanStrike(shown.strike, shown.sides, look);
    CHECK(plan.shown);
    CHECK(plan.path == path);
  }
  // Le coup multiple montre ses parts ET le total, qui a sa trajectoire.
  const moonclient::Rehearsal multiple = moonclient::PathRehearsal(DamagePath::kTotal);
  const moonclient::StrikePlan plan = moonclient::PlanStrike(multiple.strike, multiple.sides, look);
  CHECK(plan.total.has_value());
  // Et le soin est une compétence sans dégâts.
  CHECK(moonclient::PathRehearsal(DamagePath::kHeal).support_skill_id != 0);
}

TEST_CASE("Dégâts : les ancres se posent sur la trajectoire et la rendent en la lâchant",
          "[app][degats]") {
  NumberPath path;
  path.start_x_pixels = 10.0F;
  path.lift_pixels = 20.0F;
  path.arrival_x_pixels = 40.0F;
  path.arrival_y_pixels = 10.0F;
  path.bend_one_y_pixels = 6.0F;
  const std::vector<ro::ui::PathAnchor> anchors = moonclient::PathAnchorsOf(path);
  // Sans dispersion, quatre ancres ; le départ est la dernière, et se saisit
  // donc la première là où elles se confondent.
  REQUIRE(anchors.size() == 4);
  CHECK(anchors.back().id == static_cast<int>(GameOption::kDamageStartX));

  const ro::ui::PathAnchor* arrival = AnchorOf(anchors, GameOption::kDamageArrivalX);
  REQUIRE(arrival != nullptr);
  CHECK(arrival->at.x == 50.0F);
  CHECK(arrival->at.y == 30.0F);
  // La première poignée s'accroche au tiers du chemin droit, écartée de son
  // réglage, et se relie au départ ; la seconde, aux deux tiers, à l'arrivée.
  const ro::ui::PathAnchor* one = AnchorOf(anchors, GameOption::kDamageBendOne);
  REQUIRE(one != nullptr);
  CHECK(one->at.x == Catch::Approx(10.0F + 40.0F / 3.0F));
  CHECK(one->at.y == Catch::Approx(20.0F + 10.0F / 3.0F + 6.0F));
  REQUIRE(one->tied.has_value());
  CHECK(one->tied->x == 10.0F);
  const ro::ui::PathAnchor* two = AnchorOf(anchors, GameOption::kDamageBendTwo);
  REQUIRE(two != nullptr);
  REQUIRE(two->tied.has_value());
  CHECK(two->tied->x == 50.0F);
  // Elle se tire dans tous les sens.
  CHECK(one->lowest.x < one->highest.x);
  CHECK(one->lowest.y < one->highest.y);

  // Lâcher l'arrivée ailleurs change l'arrivée, par rapport au départ.
  CHECK(moonclient::MoveAnchor(path, GameOption::kDamageArrivalX, ImVec2(0.0F, 50.0F)));
  CHECK(path.arrival_x_pixels == -10.0F);
  CHECK(path.arrival_y_pixels == 30.0F);
  // Lâcher le départ le déplace SEUL : l'arrivée reste où elle est — sans quoi
  // deux points superposés ne se sépareraient plus.
  CHECK(moonclient::MoveAnchor(path, GameOption::kDamageStartX, ImVec2(-5.0F, 0.0F)));
  CHECK(path.start_x_pixels == -5.0F);
  CHECK(path.lift_pixels == 0.0F);
  CHECK(path.arrival_x_pixels == 5.0F);
  CHECK(path.arrival_y_pixels == 50.0F);
  // Une poignée se lit depuis le point du chemin droit qu'elle tire.
  CHECK(moonclient::MoveAnchor(path, GameOption::kDamageBendOne, ImVec2(0.0F, 5.0F)));
  CHECK(path.bend_one_x_pixels == Catch::Approx(5.0F - 5.0F / 3.0F));
  CHECK(path.bend_one_y_pixels == Catch::Approx(5.0F - 50.0F / 3.0F));
  // Une case n'est pas une ancre.
  CHECK_FALSE(moonclient::MoveAnchor(path, GameOption::kDamageScatter, ImVec2(0.0F, 0.0F)));

  // La dispersion allumée ajoute ses deux rayons, posés autour de l'arrivée.
  path.scatter = true;
  const std::vector<ro::ui::PathAnchor> scattered = moonclient::PathAnchorsOf(path);
  CHECK(scattered.size() == 6);
  CHECK(moonclient::MoveAnchor(path, GameOption::kDamageScatterX, ImVec2(8.0F, 50.0F)));
  CHECK(path.scatter_x_pixels == Catch::Approx(8.0F));
}

TEST_CASE("Dégâts : les coups d'essai suivent l'intervalle réglé, jamais sous le plancher",
          "[app][degats]") {
  HitPopupLook look;
  CHECK(moonclient::RehearsalStepMs(look) == 800U);
  ro::ui::OptionsWindowAction asked;
  asked.value = 250.0F;
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamageRehearsalStep, asked));
  CHECK(moonclient::RehearsalStepMs(look) == 250U);
  // Un fichier retouché à zéro ne fait pas naître un coup à chaque image.
  look.rehearsal_interval_ms = 0.0F;
  CHECK(moonclient::RehearsalStepMs(look) ==
        static_cast<std::uint32_t>(moonclient::kShortestRehearsalIntervalMs));
}

TEST_CASE("Dégâts : le type montré se règle seul — ancres, vie, et retour au client",
          "[app][degats]") {
  HitPopupLook look;
  ro::ui::OptionsWindowAction asked;

  // Le type montré se choisit dans la liste.
  asked.chosen = static_cast<int>(DamagePath::kTotal);
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamagePathShown, asked));
  CHECK(look.path_shown == static_cast<int>(DamagePath::kTotal));

  // Tirer son départ le déplace, et l'arrivée reste où elle est. Le coup
  // ordinaire ne bouge pas.
  const NumberPath client = moonclient::ClientPathOf(DamagePath::kTotal);
  asked = {};
  asked.dragged = true;
  asked.point = ImVec2(-20.0F, 12.0F);
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamageStartX, asked));
  const NumberPath& total = moonclient::PathOf(look, DamagePath::kTotal);
  CHECK(total.start_x_pixels == -20.0F);
  CHECK(total.lift_pixels == 12.0F);
  CHECK(total.arrival_x_pixels == Catch::Approx(20.0F));
  CHECK(total.arrival_y_pixels == Catch::Approx(client.arrival_y_pixels - 12.0F));
  CHECK(Ordinary(look).start_x_pixels == 0.0F);

  // Une ancre n'a pas de curseur : sans geste, rien ne bouge.
  asked = {};
  asked.value = 7.0F;
  CHECK_FALSE(moonclient::ChangeDamagePath(look, GameOption::kDamageStartX, asked));
  CHECK(total.start_x_pixels == -20.0F);

  // Sa vie : la taille se montre en pour cent, les temps en millisecondes.
  asked = {};
  asked.value = 250.0F;
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamageBirthScale, asked));
  asked.value = 1500.0F;
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamageLifeSpan, asked));
  const ro::render::NumberLife& life = moonclient::PathLifeOf(look, DamagePath::kTotal);
  CHECK(life.birth_scale == Catch::Approx(2.5F));
  CHECK(life.life_ms == 1500.0F);
  asked = {};
  asked.on = true;
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamageFacingDrift, asked));
  CHECK(total.facing_drift);

  // Le retour au client ne rend que ce type.
  Ordinary(look).lift_pixels = 5.0F;
  CHECK(moonclient::ChangeDamagePath(look, GameOption::kDamagePathClient, asked));
  CHECK(total.start_x_pixels == 0.0F);
  CHECK_FALSE(total.facing_drift);
  CHECK(life.life_ms == ro::render::ClientLifeOf(ro::render::NumberStyle::kRising).life_ms);
  CHECK(Ordinary(look).lift_pixels == 5.0F);

  // Une autre ligne ne le concerne pas.
  CHECK_FALSE(moonclient::ChangeDamagePath(look, GameOption::kDamageScale, asked));
}

TEST_CASE("Dégâts : le cadre des trajectoires pose et tire ses ancres à l'écran", "[ui][degats]") {
  ro::ui::PathSample sample;
  sample.sprite_scale = 2.0F;
  sample.birth = ImVec2(0.0F, 50.0F);
  // Les pieds en (100, 200) : un point de trajectoire monte au-dessus de la
  // naissance, à l'échelle du jeu.
  const ImVec2 at = ro::ui::PathCanvasPoint(ImVec2(100.0F, 200.0F), sample, ImVec2(10.0F, 5.0F));
  CHECK(at.x == 120.0F);
  CHECK(at.y == 90.0F);

  ro::ui::PathAnchor anchor;
  anchor.at = ImVec2(0.0F, 0.0F);
  anchor.lowest = ImVec2(-10.0F, 0.0F);
  anchor.highest = ImVec2(10.0F, 0.0F);
  // Huit pixels d'écran à droite font quatre pixels de sprite ; vers le haut,
  // rien : l'axe est verrouillé.
  const ImVec2 dragged = ro::ui::DraggedAnchor(anchor, ImVec2(8.0F, -8.0F), 2.0F, false);
  CHECK(dragged.x == 4.0F);
  CHECK(dragged.y == 0.0F);
  // Sous Maj, un quart.
  CHECK(ro::ui::DraggedAnchor(anchor, ImVec2(8.0F, 0.0F), 2.0F, true).x == 1.0F);
  // Et jamais hors de sa boîte.
  CHECK(ro::ui::DraggedAnchor(anchor, ImVec2(400.0F, 0.0F), 2.0F, false).x == 10.0F);

  // Deux ancres au même point : c'est la dernière qu'on saisit.
  const std::array<ro::ui::PathAnchor, 2> stacked = {anchor, anchor};
  const std::optional<std::size_t> under =
      ro::ui::AnchorUnder(ImVec2(100.0F, 200.0F), sample, stacked,
                          ro::ui::PathCanvasPoint(ImVec2(100.0F, 200.0F), sample, anchor.at));
  REQUIRE(under.has_value());
  CHECK(*under == 1);
  // Loin de tout, rien.
  CHECK_FALSE(
      ro::ui::AnchorUnder(ImVec2(100.0F, 200.0F), sample, stacked, ImVec2(0.0F, 0.0F)).has_value());
}

TEST_CASE("Dégâts : les ancres se posent sur le chemin que l'allure fait déjà faire",
          "[app][degats]") {
  // Une allure qui dérive de cent vingt pixels à gauche en montant de soixante,
  // relevée en trois points.
  ro::ui::PathSample sample;
  sample.allure = {ImVec2(0.0F, 0.0F), ImVec2(-60.0F, 40.0F), ImVec2(-120.0F, 60.0F)};
  CHECK(ro::ui::AllureAt(sample, 0.25F).x == Catch::Approx(-30.0F));
  CHECK(ro::ui::AllureAt(sample, 1.0F).y == Catch::Approx(60.0F));
  CHECK(ro::ui::AllureAt(ro::ui::PathSample{}, 0.5F).x == 0.0F);

  // Une arrivée tirée de cent vingt pixels à droite : la croix tombe là où le
  // nombre finit vraiment — l'allure l'a ramenée au-dessus de la cible.
  NumberPath path;
  path.arrival_x_pixels = 120.0F;
  const moonclient::AllureShift allure{.one = ro::ui::AllureAt(sample, 0.25F),
                                       .two = ro::ui::AllureAt(sample, 0.5F),
                                       .end = ro::ui::AllureAt(sample, 1.0F)};
  const std::vector<ro::ui::PathAnchor> anchors = moonclient::PathAnchorsOf(path, allure);
  const ro::ui::PathAnchor* arrival = AnchorOf(anchors, GameOption::kDamageArrivalX);
  REQUIRE(arrival != nullptr);
  CHECK(arrival->at.x == Catch::Approx(0.0F));
  CHECK(arrival->at.y == Catch::Approx(60.0F));
  // Le départ, lui, ne bouge pas : l'allure n'a encore rien fait à la naissance.
  CHECK(AnchorOf(anchors, GameOption::kDamageStartX)->at.x == 0.0F);

  // Lâchée ailleurs, elle rend la trajectoire seule, sans ce qu'ajoute l'allure.
  const ImVec2 dropped(30.0F, 60.0F);
  const ImVec2 shift = moonclient::AllureShiftOf(GameOption::kDamageArrivalX, allure);
  CHECK(moonclient::MoveAnchor(path, GameOption::kDamageArrivalX,
                               ImVec2(dropped.x - shift.x, dropped.y - shift.y)));
  CHECK(path.arrival_x_pixels == Catch::Approx(150.0F));
  CHECK(path.arrival_y_pixels == Catch::Approx(0.0F));
}

TEST_CASE("Dégâts : un point ou toute la trajectoire se copie vers un autre type",
          "[app][degats]") {
  HitPopupLook look;
  look.path_shown = static_cast<int>(DamagePath::kCritical);
  NumberPath& critical = moonclient::PathOf(look, DamagePath::kCritical);
  critical.arrival_x_pixels = 80.0F;
  critical.arrival_y_pixels = -20.0F;
  critical.start_x_pixels = 15.0F;

  // L'arrivée seule : le soin prend sa trajectoire, et rien d'autre ne change.
  CHECK(moonclient::CopyPathPart(look, GameOption::kDamageArrivalX,
                                 static_cast<int>(DamagePath::kHeal)));
  const NumberPath& heal = moonclient::PathOf(look, DamagePath::kHeal);
  CHECK(heal.arrival_x_pixels == 80.0F);
  CHECK(heal.arrival_y_pixels == -20.0F);
  CHECK(heal.start_x_pixels == 0.0F);
  CHECK(Ordinary(look).arrival_x_pixels == 0.0F);

  // Toute la trajectoire, vers tous les autres : le coup ordinaire compris.
  CHECK(moonclient::CopyPathPart(look, GameOption::kDamagePathCopy,
                                 static_cast<int>(moonclient::kDamagePathCount)));
  CHECK(Ordinary(look).start_x_pixels == 15.0F);
  CHECK(moonclient::PathOf(look, DamagePath::kWords).start_x_pixels == 15.0F);
  CHECK(moonclient::PathOf(look, DamagePath::kHeal).start_x_pixels == 15.0F);

  // Ni une cible hors de la liste, ni ce qui n'est pas une ancre.
  CHECK_FALSE(moonclient::CopyPathPart(look, GameOption::kDamageArrivalX, -1));
  CHECK_FALSE(moonclient::CopyPathPart(look, GameOption::kDamageScatter,
                                       static_cast<int>(DamagePath::kHeal)));
}

TEST_CASE("Dégâts : une ancre s'aimante à la grille, et pas sans elle", "[ui][degats]") {
  CHECK(ro::ui::SnappedToGrid(ImVec2(14.0F, -6.0F), 10.0F).x == 10.0F);
  CHECK(ro::ui::SnappedToGrid(ImVec2(14.0F, -6.0F), 10.0F).y == -10.0F);
  CHECK(ro::ui::SnappedToGrid(ImVec2(14.0F, -6.0F), 0.0F).x == 14.0F);
}
