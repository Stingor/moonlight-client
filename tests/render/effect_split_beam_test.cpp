#include "ro/render/effect_split_beam.h"

#include <algorithm>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_screen_node.h"

using Catch::Approx;
using ro::render::ComposeScreenSplitBeamQuads;
using ro::render::EffectScreenNode;
using ro::render::EffectScreenQuad;
using ro::render::EffectScreenShape;
using ro::render::EffectSplitBeamPattern;
using ro::render::EffectSplitBeamPatternsFor;
using ro::render::SowEffectSplitBeamNodes;

namespace {

// UN TRAIT DROIT, sans rotation : c'est la pose la plus lisible, et celle où
// chaque coin se calcule de tête.
EffectScreenNode StraightLine() {
  EffectScreenNode node;
  node.shape = EffectScreenShape::kSplitBeam;
  node.half_height = 100.0F;
  node.half_width = 10.0F;
  node.turn_degrees = 0.0F;
  node.alpha = 200.0F;
  return node;
}

const EffectSplitBeamPattern& EtoileDeFrostDiver() {
  const auto patterns = EffectSplitBeamPatternsFor(51);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("Trait fendu : DEUX triangles qui partagent leur arête", "[render][effect]") {
  const std::vector<EffectScreenQuad> wings =
      ComposeScreenSplitBeamQuads(StraightLine(), {0.0F, 0.0F});
  REQUIRE(wings.size() == 2);

  // ⚠ CHACUN EST UN TRIANGLE, rendu comme un quad dont le dernier coin REPREND
  // le premier : le second triangle du quad est alors d'aire nulle. C'est ce que
  // fait déjà l'éclat d'écran.
  for (const EffectScreenQuad& aile : wings) {
    CHECK(aile.corners[3].x == Approx(aile.corners[0].x));
    CHECK(aile.corners[3].y == Approx(aile.corners[0].y));
  }

  // L'ARÊTE EST LA MÊME POUR LES DEUX : ce sont les coins un et deux. Deux
  // arêtes différentes feraient deux pointes séparées au lieu d'une fendue.
  CHECK(wings[0].corners[1].x == Approx(wings[1].corners[1].x));
  CHECK(wings[0].corners[1].y == Approx(wings[1].corners[1].y));
  CHECK(wings[0].corners[2].x == Approx(wings[1].corners[2].x));
  CHECK(wings[0].corners[2].y == Approx(wings[1].corners[2].y));
}

TEST_CASE("Trait fendu : la demi-taille se coupe ENCORE en deux", "[render][effect]") {
  const std::vector<EffectScreenQuad> wings =
      ComposeScreenSplitBeamQuads(StraightLine(), {0.0F, 0.0F});
  REQUIRE(wings.size() == 2);
  // 🔴 CENT DE DEMI-HAUTEUR DONNE UN TRAIT DE CENT PIXELS, pas de deux cents :
  // le dessinateur multiplie par un demi avant tout le reste. Sans ce facteur la
  // figure serait deux fois trop longue, ce qui se voit d'un écran de distance.
  //
  // ⚠ LES NOMBRES SONT ÉCRITS EN CLAIR, ET NON RECALCULÉS DEPUIS LES CONSTANTES
  // DU MODULE : un essai qui refait le calcul de ce qu'il contrôle passe quelle
  // que soit la valeur de la constante, et ne tient donc rien.
  // ⚠ À L'ANGLE ZÉRO, L'ARÊTE EST VERTICALE : la rotation est la rotation
  // habituelle, l'identité à zéro. Lire les deux tables du client sous leur nom
  // la coucherait à l'horizontale.
  CHECK(wings[0].corners[1].x == Approx(0.0F).margin(1e-4));
  CHECK(wings[0].corners[1].y == Approx(50.0F));
  CHECK(wings[0].corners[2].x == Approx(0.0F).margin(1e-4));
  CHECK(wings[0].corners[2].y == Approx(-50.0F));

  // LES DEUX POINTES, de part et d'autre, à une demi-largeur du milieu — cinq
  // pixels pour une demi-largeur de dix — et DÉCALÉES le long de l'arête,
  // chacune vers un bout : cinquante moins ses cinq sixièmes, soit huit et un
  // tiers.
  CHECK(wings[0].corners[0].x == Approx(-5.0F));
  CHECK(wings[0].corners[0].y == Approx((50.0F / 1.2F) - 50.0F));
  CHECK(wings[1].corners[0].x == Approx(5.0F));
  CHECK(wings[1].corners[0].y == Approx(50.0F - (50.0F / 1.2F)));
  // ⚠ ET LES DEUX DÉCALAGES SONT OPPOSÉS : c'est ce qui donne au trait sa forme
  // de pointe. Les mettre du même côté en ferait un cerf-volant.
  CHECK(wings[0].corners[0].y == Approx(-wings[1].corners[0].y));
}

TEST_CASE("Trait fendu : les deux ailes montrent l'image en MIROIR", "[render][effect]") {
  const std::vector<EffectScreenQuad> wings =
      ComposeScreenSplitBeamQuads(StraightLine(), {0.0F, 0.0F});
  REQUIRE(wings.size() == 2);
  // L'arête porte le milieu de l'image, et chaque pointe un bord : l'aile gauche
  // prend le zéro, la droite le un.
  CHECK(wings[0].texture_uv[0].x == Approx(0.0F));
  CHECK(wings[1].texture_uv[0].x == Approx(1.0F));
  for (const EffectScreenQuad& aile : wings) {
    CHECK(aile.texture_uv[1].x == Approx(0.5F));
    CHECK(aile.texture_uv[2].x == Approx(0.5F));
    CHECK(aile.texture_uv[0].y == Approx(0.5F));
    CHECK(aile.texture_uv[1].y == Approx(1.0F));
    CHECK(aile.texture_uv[2].y == Approx(0.0F));
    // UN, et le nœud n'a pas son mot à dire.
    CHECK(aile.blend_mode == 1);
    // L'alpha du nœud passe dans la teinte des deux.
    CHECK(aile.tint[3] == 200);
  }
}

TEST_CASE("Trait fendu : il SUIT son porteur et ce qu'il a parcouru", "[render][effect]") {
  EffectScreenNode node = StraightLine();
  node.screen_offset = {5.0F, -20.0F};
  node.travel = {30.0F, -8.0F};
  const std::vector<EffectScreenQuad> wings = ComposeScreenSplitBeamQuads(node, {200.0F, 300.0F});
  REQUIRE(wings.size() == 2);
  // 🔴 LE DÉPLACEMENT CUMULÉ EN EST, et c'est ce qui sépare ce trait de l'éclat :
  // il partage le MOUVEMENT du quad, donc il avance. Le composer comme l'éclat
  // — porteur plus décalage, sans le parcours — clouerait les neuf pointes de
  // Frost Diver sur leur victime au lieu de les faire jaillir.
  const float centre_x = 200.0F + 5.0F + 30.0F;
  const float centre_y = 300.0F - 20.0F - 8.0F;
  CHECK(wings[0].corners[1].x == Approx(centre_x));
  CHECK(wings[0].corners[1].y == Approx(centre_y + 50.0F));
}

TEST_CASE("Trait fendu : sans aire, aucun triangle", "[render][effect]") {
  EffectScreenNode without_width = StraightLine();
  without_width.half_width = 0.0F;
  CHECK(ComposeScreenSplitBeamQuads(without_width, {0.0F, 0.0F}).empty());
  EffectScreenNode without_height = StraightLine();
  without_height.half_height = 0.0F;
  CHECK(ComposeScreenSplitBeamQuads(without_height, {0.0F, 0.0F}).empty());
}

TEST_CASE("Étoile de glace : neuf pointes, une sur trois deux fois plus longue",
          "[render][effect]") {
  const EffectSplitBeamPattern& pattern = EtoileDeFrostDiver();
  CHECK(pattern.count == 9);
  CHECK(pattern.base_step_degrees == Approx(40.0F));

  std::uint32_t generator = 7U;
  const std::vector<EffectScreenNode> etoile = SowEffectSplitBeamNodes(pattern, generator);
  REQUIRE(etoile.size() == 9);

  for (std::size_t rang = 0; rang < etoile.size(); ++rang) {
    const EffectScreenNode& shard = etoile[rang];
    CHECK(shard.shape == EffectScreenShape::kSplitBeam);
    CHECK(shard.life_ticks == 15);
    CHECK(shard.fade_out_tick == 5);
    CHECK(shard.alpha == Approx(200.0F));
    // ⚠ LE PLAFOND RESTE CELUI DU CONSTRUCTEUR : le bâtisseur n'y touche pas.
    CHECK(shard.alpha_max == Approx(254.0F));

    // 🔴 UN SUR TROIS EST LONG, et c'est le RANG qui décide — pas un tirage.
    // N'en garder qu'un jeu donnerait une étoile régulière là où le client en
    // fait une qui alterne.
    if (rang % 3 == 0) {
      CHECK(shard.half_height >= 100.0F);
      CHECK(shard.half_height <= 109.0F);
      CHECK(shard.half_width == Approx(10.0F));
    } else {
      CHECK(shard.half_height >= 45.0F);
      CHECK(shard.half_height <= 49.0F);
      CHECK(shard.half_width == Approx(4.0F));
    }

    // LE CAP ET L'ANGLE DE LA FIGURE SONT LE MÊME NOMBRE : un éclat file dans la
    // direction où il pointe.
    CHECK(shard.heading_degrees == Approx(shard.turn_degrees));
    const float base = 40.0F * static_cast<float>(rang);
    CHECK(shard.heading_degrees >= base - 15.0F);
    CHECK(shard.heading_degrees <= base + 44.0F);

    // ⚠ LE FREINAGE SE CALCULE : moins le pas sur la vie, le tout sur un et demi.
    CHECK(shard.step_change == Approx(-(shard.step / 15.0F) / 1.5F));
  }
}

TEST_CASE("Étoile de glace : le pas se tire par PALIERS, pas dans une fourchette",
          "[render][effect]") {
  const EffectSplitBeamPattern& pattern = EtoileDeFrostDiver();
  // 🔴 CINQ VALEURS, PAS UNE PLAGE CONTINUE : le client divise `rand() % 45` par
  // dix EN ENTIERS. Une fourchette rendrait des vitesses que le client ne produit
  // jamais, et l'écart ne se retrouverait plus une fois écrit.
  std::uint32_t generator = 1U;
  for (int attempt = 0; attempt < 12; ++attempt) {
    for (const EffectScreenNode& shard : SowEffectSplitBeamNodes(pattern, generator)) {
      const float plancher = shard.half_width > 5.0F ? 12.8F : 4.8F;
      const float palier = shard.step - plancher;
      CHECK(palier >= -0.001F);
      CHECK(palier <= 4.001F);
      // Le palier est ENTIER : sa distance au plus proche entier est nulle.
      CHECK(std::abs(palier - std::round(palier)) < 0.001F);
    }
  }
}

TEST_CASE("Étoile de glace : elle naît vingt pixels PLUS HAUT que sa victime", "[render][effect]") {
  const EffectSplitBeamPattern& pattern = EtoileDeFrostDiver();
  CHECK(pattern.screen_offset_y == Approx(-20.0F));

  std::uint32_t generator = 3U;
  const std::vector<EffectScreenNode> etoile = SowEffectSplitBeamNodes(pattern, generator);
  REQUIRE(etoile.size() == 9);
  // ⚠ L'ORDONNÉE D'ÉCRAN DESCEND : un décalage négatif ÉLÈVE. Le rayon de
  // naissance, tiré de zéro à quatre, l'écarte de part et d'autre.
  for (const EffectScreenNode& shard : etoile) {
    CHECK(shard.screen_offset.y <= -20.0F + 4.001F);
    CHECK(shard.screen_offset.y >= -20.0F - 4.001F);
    CHECK(std::abs(shard.screen_offset.x) <= 4.001F);
  }
  // Le premier éclat a l'angle de base zéro : son sinus est nul, il naît donc
  // droit AU-DESSUS du décalage, de son rayon tiré. C'est le seul dont on
  // connaisse l'abscisse sans connaître son tirage.
  CHECK(etoile.front().screen_offset.x == Approx(0.0F).margin(1e-4));
  CHECK(etoile.front().screen_offset.y <= -20.0F + 1e-4F);
  CHECK(etoile.front().screen_offset.y >= -24.0F - 1e-4F);
}

TEST_CASE("Étoile de glace : un seul effet en porte une", "[render][effect]") {
  // ⚠ LE SOUS-TYPE 2 N'A QU'UN SEUL BÂTISSEUR DANS TOUT LE RELEVÉ. Si un second
  // s'ajoutait, cet essai tomberait — et ce serait le bon moment pour séparer la
  // figure de son semis, comme ailleurs.
  CHECK(EffectSplitBeamPatternsFor(51).size() == 1);
  CHECK(EffectSplitBeamPatternsFor(28).empty());
  CHECK(EffectSplitBeamPatternsFor(27).empty());
  CHECK(EffectSplitBeamPatternsFor(0).empty());
}
