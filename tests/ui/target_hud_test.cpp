// Les règles du HUD de cible qu'un banc peut prouver : ce qu'une jauge écrit, ce
// qu'elle remplit, et quels cadres se montrent pour quelle cible.
//
// 🔴 L'UNE D'ELLES EST UNE RÈGLE DE CONFIDENTIALITÉ. « Les PV d'un adversaire ne
// se chiffrent pas » se relâche sans bruit : la jauge continue de se dessiner,
// personne ne voit qu'un chiffre est apparu avec.

#include "ro/ui/target_hud.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::ui::GaugeCaption;
using ro::ui::GaugeFill;
using ro::ui::GaugeNumbersAllowed;
using ro::ui::GaugeText;
using ro::ui::GaugeTextFor;
using ro::ui::GaugeTextOf;
using ro::ui::TargetPart;
using ro::ui::TargetPartApplies;
using ro::ui::TargetView;

TEST_CASE("Jauge de cible : les quatre modes de texte", "[ui][cible]") {
  CHECK(GaugeCaption(true, 300, 1200, GaugeText::kNothing, true, false).empty());
  CHECK(GaugeCaption(true, 300, 1200, GaugeText::kPercent, true, false) == "25 %");
  CHECK(GaugeCaption(true, 300, 1200, GaugeText::kValues, true, false) == "300 / 1200");
  CHECK(GaugeCaption(true, 300, 1200, GaugeText::kBoth, true, false) == "300 / 1200 (25 %)");
}

TEST_CASE("Jauge de cible : un adversaire ne se chiffre pas", "[ui][cible]") {
  for (const GaugeText mode :
       {GaugeText::kNothing, GaugeText::kPercent, GaugeText::kValues, GaugeText::kBoth}) {
    CHECK(GaugeCaption(true, 300, 1200, mode, /*numbers_allowed=*/false, false).empty());
  }
  TargetView enemy;
  enemy.is_player = true;
  CHECK_FALSE(GaugeNumbersAllowed(enemy, /*is_magic=*/false, /*known=*/true));
  // Inconnus, ses PV se disent « inconnus » : sans ce mot, une jauge vide se
  // lirait « il est mort ».
  CHECK(GaugeNumbersAllowed(enemy, false, false));
  TargetView ally = enemy;
  ally.ally = true;
  CHECK(GaugeNumbersAllowed(ally, false, true));
  // Un monstre se chiffre toujours.
  CHECK(GaugeNumbersAllowed(TargetView{}, false, true));
}

TEST_CASE("Jauge de cible : des PV en pourcentage s'écrivent en pour cent", "[ui][cible]") {
  TargetView monster;
  monster.health_in_percent = true;
  for (const GaugeText mode : {GaugeText::kPercent, GaugeText::kValues, GaugeText::kBoth}) {
    const GaugeText written = GaugeTextFor(monster, /*is_magic=*/false, mode);
    CHECK(written == GaugeText::kPercent);
    CHECK(GaugeCaption(true, 37, 100, written, GaugeNumbersAllowed(monster, false, true), false) ==
          "37 %");
  }
  CHECK(GaugeTextFor(monster, false, GaugeText::kNothing) == GaugeText::kNothing);
  // La magie ne change pas de règle, ni des PV exacts.
  CHECK(GaugeTextFor(monster, /*is_magic=*/true, GaugeText::kValues) == GaugeText::kValues);
  CHECK(GaugeTextFor(TargetView{}, false, GaugeText::kBoth) == GaugeText::kBoth);
  // Un adversaire reste muet, pourcentage ou non.
  TargetView enemy = monster;
  enemy.is_player = true;
  CHECK(GaugeCaption(true, 37, 100, GaugeTextFor(enemy, false, GaugeText::kValues),
                     GaugeNumbersAllowed(enemy, false, true), false)
            .empty());
}

TEST_CASE("Jauge de cible : inconnu n'est pas zéro", "[ui][cible]") {
  const std::string unknown = GaugeCaption(false, 0, 0, GaugeText::kBoth, true, false);
  CHECK_FALSE(unknown.empty());
  CHECK(unknown.find('0') == std::string::npos);
  CHECK(GaugeFill(false, 0, 0, /*is_magic=*/false) == 0.0F);
}

TEST_CASE("Jauge de cible : un monstre n'a pas de réserve de magie", "[ui][cible]") {
  // Le serveur ne peut pas écrire zéro : ni barre pleine, ni « 1 / 1 ».
  CHECK(GaugeFill(true, 1, 1, /*is_magic=*/true) == 0.0F);
  for (const GaugeText mode : {GaugeText::kPercent, GaugeText::kValues, GaugeText::kBoth}) {
    CHECK(GaugeCaption(true, 1, 1, mode, true, /*is_magic=*/true) == "No SP");
  }
  // La règle ne vaut que pour la magie : un point de vie, c'est une jauge pleine.
  CHECK(GaugeFill(true, 1, 1, /*is_magic=*/false) == 1.0F);
}

TEST_CASE("Jauge de cible : le remplissage et le pourcentage sont bornés", "[ui][cible]") {
  CHECK(GaugeFill(true, 600, 1200, false) == 0.5F);
  CHECK(GaugeFill(true, 0, 1200, false) == 0.0F);
  CHECK(GaugeFill(true, 5000, 1200, false) == 1.0F);
  CHECK(GaugeFill(true, 300, 0, false) == 0.0F);
  CHECK(GaugeCaption(true, 5000, 1200, GaugeText::kPercent, true, false) == "100 %");
}

TEST_CASE("HUD de cible : quels cadres pour quelle cible", "[ui][cible]") {
  TargetView npc;
  npc.is_npc = true;
  CHECK_FALSE(TargetPartApplies(TargetPart::kHealth, npc));
  CHECK_FALSE(TargetPartApplies(TargetPart::kMagic, npc));
  CHECK(TargetPartApplies(TargetPart::kKind, npc));

  TargetView enemy;
  enemy.is_player = true;
  CHECK_FALSE(TargetPartApplies(TargetPart::kKind, enemy));
  CHECK_FALSE(TargetPartApplies(TargetPart::kMagic, enemy));
  CHECK(TargetPartApplies(TargetPart::kHealth, enemy));
  TargetView ally = enemy;
  ally.ally = true;
  CHECK(TargetPartApplies(TargetPart::kMagic, ally));
}

TEST_CASE("HUD de cible : la disposition d'origine", "[ui][cible]") {
  const auto parts = ro::ui::DefaultTargetParts();
  const ro::ui::HudRect portrait = parts[static_cast<std::size_t>(TargetPart::kPortrait)].rect;
  CHECK(portrait.x == 20.0F);
  CHECK(portrait.width == 72.0F);
  // Les états sont éteints d'usine, les cinq autres allumés.
  CHECK_FALSE(parts[static_cast<std::size_t>(TargetPart::kStatuses)].shown);
  CHECK(parts[static_cast<std::size_t>(TargetPart::kName)].shown);
  // Un texte de jauge relu hors bornes retombe sur les valeurs.
  CHECK(GaugeTextOf(9) == GaugeText::kValues);
  CHECK(GaugeTextOf(1) == GaugeText::kPercent);
}
