#include <cmath>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_thrown_item_seed.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceThrownItemTracks;
using ro::render::EffectThrownItemBirthTick;
using ro::render::EffectThrownItemPattern;
using ro::render::EffectThrownItemPatternsFor;
using ro::render::EffectThrownItemPatternTable;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::kMarkOfThrownKindSeed;
using ro::render::kMarkOfThrownSteps;
using ro::render::kSlotOfThrownSpeed;
using ro::render::kTailSlotOfThrownBaseHeight;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectThrownItemTracks;
using ro::render::SowEffectThrownItemNode;

namespace {

// LE FLACON VIDE, celui qui part au tic zéro et dont le pas se calcule.
constexpr std::uint32_t kEmptyFlask = 298;
// L'IDENTIFIANT QUI EN LANCE DEUX, l'un au cinquième tic et l'autre au dixième.
constexpr std::uint32_t kTwoFlasks = 539;
// LA LAME, le seul style qui parte d'un angle et tourne vite.
constexpr std::uint8_t kStyleDeLaLame = 3;

constexpr glm::vec3 kThrower{0.0F, 10.0F, 0.0F};
constexpr glm::vec3 kTarget{50.0F, 10.0F, 0.0F};
// LE PARAMÈTRE D'UNE INSTANCE qui ne vient pas d'un coup multi-coups.
constexpr std::int32_t kPlainParameter = 1;

[[nodiscard]] EffectThrownItemPattern PatternOfStyle(std::uint8_t style) {
  return {kEmptyFlask, "EffectInst_InitBeamTrailParticle", "data\\texture\\test.bmp", style};
}

// LES DEUX ANGLES DE CAMÉRA, que le mouvement range dans les deux premières
// pistes avant toute autre chose.
constexpr std::int16_t kTourDeCamera = 30;
constexpr std::int16_t kCameraTilt = 45;

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t count) {
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceThrownItemTracks(tracks, kTarget, kTourDeCamera, kCameraTilt);
  }
}

}  // namespace

TEST_CASE("la table des objets lances porte une rangee par appel", "[render][thrown_item]") {
  const std::span<const EffectThrownItemPattern> table = EffectThrownItemPatternTable();
  REQUIRE_FALSE(table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }

  // UN IDENTIFIANT EN LANCE DEUX, et leurs styles décident de tics de naissance
  // différents : c'est la preuve qu'une rangée vaut un APPEL.
  const std::span<const EffectThrownItemPattern> two = EffectThrownItemPatternsFor(kTwoFlasks);
  REQUIRE(two.size() == 2);
  CHECK(EffectThrownItemBirthTick(two[0].builder_argument) == 5);
  CHECK(EffectThrownItemBirthTick(two[1].builder_argument) == 10);
  CHECK(EffectThrownItemBirthTick(0) == 0);
  CHECK(EffectThrownItemPatternsFor(kEmptyFlask).size() == 1);
}

TEST_CASE("le semis tire tout du couple lanceur-cible", "[render][thrown_item]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter);
  const EffectTrack& head = tracks.front();

  // LA CLÉ PORTE LA DISTANCE DU JET, et non une hauteur (ADR 0017).
  CHECK(head.lift_key == Approx(50.0F));
  // VINGT-CINQ PAS POUR LE VOYAGE, quelle que soit la distance.
  CHECK(head.value[kSlotOfThrownSpeed] == Approx(2.0F));
  // ⚠ L'ANCRE PART AU SOL et la vraie hauteur vit dans la queue : huit unités
  // AU-DESSUS du lanceur. Le client les retranche dans son repère qui descend ;
  // les retrancher dans le nôtre lançait l'objet sous le sol.
  CHECK(head.anchor.y == Approx(0.0F));
  CHECK(head.tail[kTailSlotOfThrownBaseHeight] == Approx(18.0F));
  CHECK(head.alpha == 255);

  // ⚠ LES TROIS CRANS SONT ALLUMÉS ET VIDES : éteints, ils ne pourraient rien
  // recevoir de la cascade.
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
  }
}

TEST_CASE("le pas d'un objet lance loin est plus grand", "[render][thrown_item]") {
  const glm::vec3 far_point{100.0F, 10.0F, 0.0F};
  const auto court =
      SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter);
  const auto longue =
      SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, far_point, kPlainParameter);
  CHECK(longue.front().value[kSlotOfThrownSpeed] > court.front().value[kSlotOfThrownSpeed]);
  // ⚠ ET LE NOMBRE DE PAS NE CHANGE PAS : c'est ce qui rend la durée du vol à
  // peu près constante, quelle que soit la portée.
  CHECK(longue.front().mark[kMarkOfThrownSteps] == court.front().mark[kMarkOfThrownSteps]);

  // LA PIERRE ET LA LAME, ELLES, ONT UN PAS FIXE : un jet lointain leur prend
  // donc plus de tics.
  const auto pierre_pres =
      SeedEffectThrownItemTracks(PatternOfStyle(2), kThrower, kTarget, kPlainParameter);
  const auto far_stone =
      SeedEffectThrownItemTracks(PatternOfStyle(2), kThrower, far_point, kPlainParameter);
  CHECK(pierre_pres.front().value[kSlotOfThrownSpeed] == Approx(2.0F));
  CHECK(far_stone.front().value[kSlotOfThrownSpeed] == Approx(2.0F));
  CHECK(far_stone.front().mark[kMarkOfThrownSteps] > pierre_pres.front().mark[kMarkOfThrownSteps]);
}

TEST_CASE("le genre et l'angle de depart ne se deduisent pas du style", "[render][thrown_item]") {
  // LES QUATRE GENRES SONT CEUX DU CLIENT, et ils ne suivent pas l'ordre des
  // styles : huit, neuf, zéro, quinze.
  CHECK(SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter)
            .front()
            .mark[kMarkOfThrownKindSeed] == 8);
  CHECK(SeedEffectThrownItemTracks(PatternOfStyle(1), kThrower, kTarget, kPlainParameter)
            .front()
            .mark[kMarkOfThrownKindSeed] == 9);
  CHECK(SeedEffectThrownItemTracks(PatternOfStyle(2), kThrower, kTarget, kPlainParameter)
            .front()
            .mark[kMarkOfThrownKindSeed] == 0);
  CHECK(
      SeedEffectThrownItemTracks(PatternOfStyle(kStyleDeLaLame), kThrower, kTarget, kPlainParameter)
          .front()
          .mark[kMarkOfThrownKindSeed] == 15);

  // 🔴 LE FLACON DU STYLE UN PREND SA SOUS-MARQUE DU PARAMÈTRE DE L'INSTANCE :
  // un le teinte de rouge, et zéro — que rien ne dessine — le rendait
  // invisible. Les autres styles ne le lisent pas.
  constexpr std::int32_t kThreeHits = 3;
  const auto flacon = SeedEffectThrownItemTracks(PatternOfStyle(1), kThrower, kTarget, kThreeHits);
  CHECK(flacon.front().mark[ro::render::kMarkOfThrownExtra] == kThreeHits);
  CHECK(ro::render::EffectTintedQuadStyleFor(flacon.front().mark[kMarkOfThrownKindSeed],
                                             flacon.front().mark[ro::render::kMarkOfThrownExtra])
            .draws);
  const auto ordinary =
      SeedEffectThrownItemTracks(PatternOfStyle(1), kThrower, kTarget, kPlainParameter);
  CHECK(ro::render::EffectTintedQuadStyleFor(ordinary.front().mark[kMarkOfThrownKindSeed],
                                             ordinary.front().mark[ro::render::kMarkOfThrownExtra])
            .color == std::array<std::uint8_t, 3>{255, 135, 135});
  CHECK(SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kThreeHits)
            .front()
            .mark[ro::render::kMarkOfThrownExtra] == 0);

  // LA LAME EST LA SEULE À NE PAS PARTIR DE ZÉRO.
  CHECK(
      SeedEffectThrownItemTracks(PatternOfStyle(kStyleDeLaLame), kThrower, kTarget, kPlainParameter)
          .front()
          .degrees == 330);
  CHECK(SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter)
            .front()
            .degrees == 0);
}

TEST_CASE("les deux angles de camera se posent meme quand le noeud dort", "[render][thrown_item]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  REQUIRE_FALSE(tracks.front().active);
  Advance(tracks, 1);
  // ⚠ ILS ARRIVENT DÉJÀ COMPLÉTÉS : la troupe passe `EffectBillboardTurnDegrees`
  // et `…TiltDegrees`, et le mouvement les range tels quels. Cet essai attendait
  // qu'il les complète une seconde fois — la figure recevait alors l'angle brut
  // de la caméra. Voir `effect_billboard_facing_test.cpp`.
  CHECK(tracks[0].style == kTourDeCamera);
  CHECK(tracks[1].style == kCameraTilt);
}

TEST_CASE("le cap se prend au premier tic et ne bouge plus", "[render][thrown_item]") {
  auto tracks = SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter);
  REQUIRE(tracks.front().azimuth_degrees == 0);
  Advance(tracks, 1);
  const std::int16_t cap = tracks.front().azimuth_degrees;
  // LA CIBLE EST PLEIN EST : le cap vaut zéro, et c'est bien ce qu'il vaut déjà.
  CHECK(cap == 0);
  // L'ANCRE A AVANCÉ D'UN PAS le long de ce cap.
  CHECK(tracks.front().anchor.x == Approx(2.0F));
  CHECK(tracks.front().anchor.z == Approx(0.0F));

  // ⚠ UNE CIBLE AU NORD DONNE UN AUTRE CAP, et c'est ce qui prouve que le cap
  // est mesuré et non posé. C'est le cap du CLIENT, dont la profondeur est
  // l'opposée de la nôtre : notre profondeur positive est son trois quarts de
  // tour, et l'objet part bien vers la cible.
  const glm::vec3 au_nord{0.0F, 10.0F, 50.0F};
  auto vers_le_nord =
      SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, au_nord, kPlainParameter);
  const glm::vec3 start = vers_le_nord.front().anchor;
  AdvanceThrownItemTracks(vers_le_nord, au_nord, 0, 0);
  CHECK(vers_le_nord.front().azimuth_degrees == 270);
  CHECK(vers_le_nord.front().anchor.z > start.z);
  CHECK(vers_le_nord.front().anchor.x == Approx(start.x).margin(1e-4));
}

TEST_CASE("le cap de l'objet lance s'arrondit comme celui du client", "[render][thrown_item]") {
  // ⚠ LE CLIENT BALAIE SA TABLE DE COSINUS ET S'ARRÊTE AU PREMIER DEGRÉ qui passe
  // sous le rapport : un cap de 30,4 degrés devient 31, là où un `atan2` tronqué
  // rendait 30.
  constexpr float kDegreesOff = 30.4F;
  const float radians = kDegreesOff * std::numbers::pi_v<float> / 180.0F;
  // Vers une profondeur NÉGATIVE chez nous : positive chez le client.
  const glm::vec3 aim_point =
      kThrower + glm::vec3{100.0F * std::cos(radians), 0.0F, -100.0F * std::sin(radians)};
  auto tracks = SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, aim_point, kPlainParameter);
  AdvanceThrownItemTracks(tracks, aim_point, 0, 0);
  CHECK(tracks.front().azimuth_degrees == 31);
}

TEST_CASE("l'objet monte, culmine a mi-chemin et redescend", "[render][thrown_item]") {
  auto tracks = SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter);
  const std::int32_t step = tracks.front().mark[kMarkOfThrownSteps];
  REQUIRE(step > 4);

  // ⚠ NOTRE REPÈRE MONTE : culminer, c'est avoir la hauteur la PLUS HAUTE.
  float at_highest = 0.0F;
  std::int32_t apex_tick = 0;
  for (std::int32_t tick = 1; tick <= step; ++tick) {
    Advance(tracks, 1);
    if (tracks.front().anchor.y > at_highest) {
      at_highest = tracks.front().anchor.y;
      apex_tick = tick;
    }
  }
  // LE SOMMET EST À MI-VOL, à un tic près.
  CHECK(apex_tick * 2 >= step - 1);
  CHECK(apex_tick * 2 <= step + 1);
  // Et au-dessus du lanceur, qui se tient à dix unités.
  CHECK(at_highest > kThrower.y);
}

TEST_CASE("la cascade se deplie un cran sur deux tics", "[render][thrown_item]") {
  auto tracks = SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter);
  // AU PREMIER TIC, RIEN N'A ENCORE ÉTÉ DONNÉ : chaque cran attend son rang.
  Advance(tracks, 1);
  CHECK(tracks[1].alpha == 0);
  // ⚠ LE PREMIER SAUT COÛTE CENT CINQUANTE, les suivants vingt-cinq : la tête
  // passe donc de deux cent cinquante-cinq à cent cinq.
  Advance(tracks, 2);
  CHECK(tracks[1].alpha == 255 - 150);
  CHECK(tracks[2].alpha == 0);
  Advance(tracks, 2);
  CHECK(tracks[2].alpha == 255 - 150 - 25);
}

TEST_CASE("passe son but, l'objet ne fait plus que s'eteindre", "[render][thrown_item]") {
  auto tracks = SeedEffectThrownItemTracks(PatternOfStyle(0), kThrower, kTarget, kPlainParameter);
  const std::int32_t step = tracks.front().mark[kMarkOfThrownSteps];

  // ⚠ LE VOL SE JUGE FINI À 1,05 ET NON À 1, et ce cinquième en plus se COMPTE :
  // à vingt-six pas de deux unités pour cinquante de portée, l'objet est à
  // cinquante-deux quand il a fait ses pas, puis va jusqu a cinquante-quatre — un
  // tic de rab — avant de s'arrêter. Un vol borné à un s'arrêterait à
  // cinquante-deux, et l'objet n'atteindrait jamais tout à fait sa cible.
  Advance(tracks, step);
  CHECK(tracks.front().anchor.x == Approx(52.0F));
  Advance(tracks, 1);
  const float at_end = tracks.front().anchor.x;
  CHECK(at_end == Approx(54.0F));

  // Une fois passé là, l'ancre ne bouge plus et l'alpha tombe de vingt par tic.
  Advance(tracks, 3);
  const std::int16_t remainder = tracks.front().alpha;
  CHECK(tracks.front().anchor.x == Approx(at_end));
  Advance(tracks, 1);
  CHECK(tracks.front().alpha == remainder - 20);
}

TEST_CASE("un vol sans pas s'eteint au premier tic", "[render][thrown_item]") {
  // ⚠ UN BÂTISSEUR QUI N'A PAS SU COMPTER laisse la marque à zéro, et le client
  // divise alors par zéro : son vol se juge fini d'emblée. C'est sa mesure.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks.front().active = true;
  tracks.front().alpha = 255;
  tracks.front().mark[kMarkOfThrownSteps] = 0;
  Advance(tracks, 1);
  CHECK(tracks.front().alpha == 235);
  CHECK(tracks.front().anchor.x == Approx(0.0F));
}

TEST_CASE("le noeud seme porte sa forme et sa cible", "[render][thrown_item]") {
  const ro::render::PooledEffectNode node =
      SowEffectThrownItemNode(PatternOfStyle(0), kThrower, kTarget, 60, kPlainParameter);
  CHECK(node.shape == PooledNodeShape::kThrownItem);
  REQUIRE(node.target_position.has_value());
  CHECK(node.target_position->x == Approx(50.0F));
  CHECK(node.node.life_ticks == 60);
  REQUIRE(node.resource_paths.size() == 1);
}
