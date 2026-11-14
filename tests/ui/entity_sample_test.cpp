// LE CADRE D'ESSAI DE LA PAGE « ENTITÉS » : où l'entité tombe, et où son nom
// se pose.
//
// 🔴 LE CADRE DOIT POSER LE NOM COMME LE JEU : sous les pieds, de l'écart
// réglé, et SOUS les barres quand elles sont de ce côté-là. Un cadre qui les
// superpose ferait régler un écart faux, qu'on ne découvrirait qu'en jeu.

#include "ro/ui/entity_sample.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::NamePlateKind;
using ro::ui::EntityBarKind;
using ro::ui::EntitySample;
using ro::ui::EntitySampleFloor;
using ro::ui::EntitySampleHeadroom;
using ro::ui::EntitySampleHeight;
using ro::ui::EntitySampleNameLines;
using ro::ui::EntitySamplePlace;
using ro::ui::EntitySampleShown;
using ro::ui::kEntitySampleFloorPixels;
using ro::ui::kEntitySampleHeadroomPixels;
using ro::ui::kEntitySampleHeightPixels;
using ro::ui::PlaceEntitySample;

namespace {

[[nodiscard]] EntitySample Poring() {
  EntitySample sample;
  sample.sprite = ro::ui::SkinImage{.id = 1, .width = 320.0F, .height = 320.0F};
  sample.feet = ImVec2(160.0F, 310.0F);
  sample.head_y = 280.0F;
  sample.name = "Poring";
  sample.aside = "Lv. 1 | HP: 100%";
  return sample;
}

}  // namespace

TEST_CASE("Cadre d'essai : l'entité se pose pieds au centre, sol gardé dessous", "[ui][entites]") {
  const EntitySample sample = Poring();
  const EntitySamplePlace place =
      PlaceEntitySample(ImVec2(100.0F, 50.0F), ImVec2(500.0F, 290.0F), sample, 60.0F);
  CHECK(place.feet.x == 300.0F);
  CHECK(place.feet.y == 230.0F);
  // L'image se pose pour que SES pieds tombent sur ceux du cadre.
  CHECK(place.image_min.x + sample.feet.x == place.feet.x);
  CHECK(place.image_min.y + sample.feet.y == place.feet.y);
  // Le haut du sprite garde son écart aux pieds : trente pixels.
  CHECK(place.feet.y - place.head_y == 30.0F);
}

TEST_CASE("Cadre d'essai : sa hauteur suit la stature de l'entité", "[ui][entites]") {
  const EntitySample poring = Poring();
  EntitySample baphomet = poring;
  baphomet.head_y = 60.0F;
  CHECK(EntitySampleHeight(poring, kEntitySampleFloorPixels) == kEntitySampleHeightPixels);
  CHECK(EntitySampleHeight(baphomet, kEntitySampleFloorPixels) > kEntitySampleHeightPixels);
}

TEST_CASE("Cadre d'essai : une stature réservée garde la hauteur pendant la rotation",
          "[ui][entites]") {
  // 🔴 UN CADRE QUI CHANGE DE HAUTEUR À CHAQUE REPRÉSENTANT dérobe les réglages
  // qu'on visait dessous : la hauteur suit le plus grand, pas celui du moment.
  EntitySample small = Poring();
  EntitySample tall = small;
  tall.head_y = 60.0F;
  small.reserved_stature = tall.feet.y - tall.head_y;
  CHECK(EntitySampleHeight(small, kEntitySampleFloorPixels) ==
        EntitySampleHeight(tall, kEntitySampleFloorPixels));
}

TEST_CASE("Cadre d'essai : une barre de cast relevée agrandit la place au-dessus de la tête",
          "[ui][entites]") {
  EntitySample sample = Poring();
  // Le haut de ce qui est peint, cinq pixels au-dessus du corps.
  sample.body_min = ImVec2(140.0F, 275.0F);
  ro::ui::CastBarsLook cast;
  const EntitySampleShown shown{.bars = EntityBarKind::kMonster};
  constexpr float kFontPixels = 13.0F;
  CHECK(EntitySampleHeadroom(sample, cast, shown, kFontPixels) == kEntitySampleHeadroomPixels);
  cast.kinds[static_cast<std::size_t>(EntityBarKind::kMonster)].offset_pixels =
      -ro::ui::kLargestCastBarOffset;
  const float raised = EntitySampleHeadroom(sample, cast, shown, kFontPixels);
  CHECK(raised > kEntitySampleHeadroomPixels);
  // ⚠ SANS REMPLACEMENT, C'EST LA BARRE DU CLIENT, collée au haut du sprite.
  cast.replaced = false;
  CHECK(EntitySampleHeadroom(sample, cast, shown, kFontPixels) == kEntitySampleHeadroomPixels);
}

TEST_CASE("Cadre d'essai : le nom se compose comme en jeu, filtré par le réglage",
          "[ui][entites]") {
  const EntitySample sample = Poring();
  ro::render::EntityNameLook look;
  CHECK(EntitySampleNameLines(sample, look) ==
        std::vector<std::string>{"Poring (Lv. 1 | HP: 100%) "});
  look.aside = false;
  CHECK(EntitySampleNameLines(sample, look).size() == 1);
  CHECK(EntitySampleNameLines(sample, look).front().find("Lv.") == std::string::npos);
}

TEST_CASE("Cadre d'essai : la seconde ligne porte la race et l'élément", "[ui][entites]") {
  // @showmobinfo : la race à la place de la guilde, l'élément à celle du rang.
  EntitySample sample = Poring();
  sample.banner = "Plant";
  sample.badge = "Water 1";
  ro::render::EntityNameLook look;
  const std::vector<std::string> lines = EntitySampleNameLines(sample, look);
  REQUIRE(lines.size() == 2);
  CHECK(lines[1].find("Plant") != std::string::npos);
  // ⚠ ENTRE PARENTHÈSES ET NON ENTRE CROCHETS : la composition coréenne, celle
  // que l'installation déclare, les met partout.
  CHECK(lines[1].find("(Water 1)") != std::string::npos);
  // La case éteinte fait tomber la ligne entière, insigne compris.
  look.banner = false;
  CHECK(EntitySampleNameLines(sample, look).size() == 1);
}

TEST_CASE("Cadre d'essai : une barre chevauchée repousse le nom", "[ui][entites]") {
  const EntitySample sample = Poring();
  ro::ui::EntityBarsLook bars;
  ro::render::EntityNamesLook names;
  const EntitySampleShown shown{.bars = EntityBarKind::kMonster, .name = NamePlateKind::kMonster};
  // Un nom rapproché des pieds, qui tombe sur la barre quand elle y est.
  names.kinds[static_cast<std::size_t>(NamePlateKind::kMonster)].offset_pixels = 30.0F;
  bars.kinds[static_cast<std::size_t>(EntityBarKind::kMonster)].offset_pixels = 30.0F;
  bars.kinds[static_cast<std::size_t>(EntityBarKind::kMonster)].under_feet = true;
  const float below = EntitySampleFloor(sample, bars, names, shown);
  bars.kinds[static_cast<std::size_t>(EntityBarKind::kMonster)].under_feet = false;
  const float above = EntitySampleFloor(sample, bars, names, shown);
  CHECK(below > above);
  // Sans nom ni barre, le sol minimal.
  CHECK(EntitySampleFloor(sample, bars, names, EntitySampleShown{}) == kEntitySampleFloorPixels);
}

TEST_CASE("Précision du ciblage : la zone s'élargit au plancher autour de son centre",
          "[ui][ciblage]") {
  // Un œuf de vingt-cinq pixels, un plancher de cent : il se défend sur cent,
  // il lui en manque soixante-quinze, partagés des deux côtés.
  const ro::ui::ScreenBand widened = ro::ui::PickZoneSpan(100.0F, 125.0F, 100.0F);
  CHECK(widened.top == 62.5F);
  CHECK(widened.bottom == 162.5F);
  // Plus grand que le plancher, il garde son dessin.
  const ro::ui::ScreenBand kept = ro::ui::PickZoneSpan(100.0F, 250.0F, 100.0F);
  CHECK(kept.top == 100.0F);
  CHECK(kept.bottom == 250.0F);
}
