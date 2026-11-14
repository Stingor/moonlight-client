#include "ro/render/effect_cone_wedge.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_cone_wedge_seed.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::ComposeConeWedgeQuads;
using ro::render::EffectConeWedgePattern;
using ro::render::EffectConeWedgePatternsFor;
using ro::render::EffectConeWedgePatternTable;
using ro::render::EffectConeWedgeStyleFor;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kSlotOfWedgeApexHeight;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectConeWedgeTracks;
using ro::render::SowEffectConeWedgeNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

// Les pistes d'un nœud, avancées par le mouvement — elles naissent invisibles.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::uint8_t variant,
                                                                     std::uint8_t radius,
                                                                     std::int32_t ticks) {
  const EffectConeWedgePattern pattern{0, "essai", "image.tga", variant, 8};
  std::uint32_t seed = 31U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectConeWedgeTracks(pattern, radius, seed);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    ro::render::AdvanceConeWedgeTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("l'éventail naît invisible", "[render][cone_wedge]") {
  const EffectConeWedgePattern& pattern = EffectConeWedgePatternsFor(366).front();
  std::uint32_t seed = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> neuves =
      SeedEffectConeWedgeTracks(pattern, 0, seed);
  for (const EffectTrack& track : neuves) {
    // ⚠ LES QUATRE PISTES SONT VIVES, à la différence de la plupart des familles.
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(ComposeConeWedgeQuads(track, kAtTheOrigin).empty());
  }
}

TEST_CASE("chaque piste sort DEUX triangles déguisés en quads", "[render][cone_wedge]") {
  // ⚠ TRENTE TICS, PAS CINQ : la variante quatre part à moins vingt-quatre, et
  // ne commence donc à s'allumer qu'après. Ce cas de test l'avait d'abord oublié.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(4, 0, 30);
  const std::vector<EffectWorldQuad> quads = ComposeConeWedgeQuads(tracks.front(), kAtTheOrigin);
  REQUIRE(quads.size() == 2);

  // ⚠ LES DEUX DERNIERS COINS SONT LE MÊME POINT : la voie A ne sait émettre que
  // des quads, et le client s'en sert pour poser des triangles.
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[2] == quad.corners[3]);
    // Et cet apex est à la hauteur que porte la case zéro, ramenée dans notre
    // repère : le client la compte vers le bas.
    CHECK(quad.corners[2].y == Approx(-tracks.front().value[kSlotOfWedgeApexHeight]));
  }

  // Le second coin est PLUS LARGE que le premier : ses deux points sont plus
  // écartés l'un de l'autre.
  const auto offset = [](const EffectWorldQuad& quad) {
    return glm::length(quad.corners[1] - quad.corners[0]);
  };
  CHECK(offset(quads[1]) > offset(quads[0]));
}

TEST_CASE("les éventails de Head Crush et Joint Beat naissent au-dessus du sol",
          "[render][cone_wedge]") {
  // Le semis range l'apex tel que le client l'écrit, douze unités NÉGATIVES :
  // au-dessus de l'ancre dans un repère qui descend. Recopié sans le ramener au
  // nôtre, l'éventail entier naissait douze unités sous le sol.
  for (const std::uint32_t effect_id : {399U, 400U}) {
    INFO("effet " << effect_id);
    const EffectConeWedgePattern& pattern = EffectConeWedgePatternsFor(effect_id).front();
    for (std::uint8_t radius = 0; radius < pattern.node_count; ++radius) {
      // Le premier tic où l'éventail s'allume, rayon encore petit.
      const std::array<EffectTrack, kEffectTracksPerNode> tracks =
          Advanced(pattern.variant, radius, 25);
      for (const EffectTrack& track : tracks) {
        const std::vector<EffectWorldQuad> quads = ComposeConeWedgeQuads(track, kAtTheOrigin);
        REQUIRE(quads.size() == 2);
        for (const EffectWorldQuad& quad : quads) {
          CHECK(quad.corners[2].y == Approx(12.0F));
          for (const glm::vec3& corner : quad.corners) {
            CHECK(corner.y > 0.0F);
          }
        }
      }
    }
  }
}

TEST_CASE("les deux coins n'ont jamais la même teinte", "[render][cone_wedge]") {
  // ⚠ LE LARGE N'EST PAS UNE COPIE ÉLARGIE DE L'ÉTROIT : il change de couleur, et
  // parfois de mélange. Les peindre pareil perdrait le dégradé du cœur vers le
  // bord, qui est tout l'effet.
  for (const float variant : {2.0F, 4.0F, 5.0F, 6.0F, 0.0F}) {
    INFO("variante " << variant);
    const auto style = EffectConeWedgeStyleFor(variant);
    CHECK(style.narrow_color != style.wide_color);
  }
  // Une seule variante donne la même couleur aux deux, et elle change alors de
  // MÉLANGE — c'est ainsi qu'elle garde un dégradé.
  const auto three = EffectConeWedgeStyleFor(3.0F);
  CHECK(three.narrow_color == three.wide_color);
  CHECK(three.narrow_blend != three.wide_blend);

  // ⚠ ET LA BRANCHE PAR DÉFAUT N'EST PAS UNE ABSENCE : c'est la variante la plus
  // contrastée de toutes, un cœur cyan dans un bord rouge.
  const auto defaut = EffectConeWedgeStyleFor(99.0F);
  CHECK(defaut.narrow_color == std::array<std::uint8_t, 3>{0, 250, 250});
  CHECK(defaut.wide_color == std::array<std::uint8_t, 3>{250, 0, 0});

  // Une seule aplatit sa profondeur, pour ses deux coins.
  CHECK(EffectConeWedgeStyleFor(6.0F).flattens_depth);
  CHECK_FALSE(EffectConeWedgeStyleFor(5.0F).flattens_depth);
}

TEST_CASE("l'inclinaison porte les DEUX écarts à la fois", "[render][cone_wedge]") {
  // ⚠ CELUI DE LA PISTE DANS LE NŒUD, et celui du nœud dans l'éventail. C'est
  // cette somme qui déploie les rayons ; n'en garder qu'un replierait l'éventail.
  const std::array<EffectTrack, kEffectTracksPerNode> first = Advanced(4, 0, 0);
  const std::array<EffectTrack, kEffectTracksPerNode> second = Advanced(4, 1, 0);

  // Les quatre pistes d'un même nœud s'écartent d'un quart de tour.
  CHECK(first[1].tilt_degrees - first[0].tilt_degrees == 90);
  CHECK(first[2].tilt_degrees - first[1].tilt_degrees == 90);
  // Et deux nœuds voisins de l'éventail s'écartent de vingt-deux degrés.
  CHECK(second[0].tilt_degrees - first[0].tilt_degrees == 22);

  // ⚠ SAUF POUR UNE VARIANTE, qui les resserre à SEPT — son éventail est trois
  // fois plus fin, ce qui va de pair avec ses coins.
  CHECK(Advanced(3, 1, 0)[0].tilt_degrees - Advanced(3, 0, 0)[0].tilt_degrees == 7);
}

TEST_CASE("trois variantes partent tout de suite, les autres attendent", "[render][cone_wedge]") {
  for (const std::uint8_t variant : std::array<std::uint8_t, 3>{2, 3, 6}) {
    INFO("variante " << static_cast<int>(variant));
    CHECK(Advanced(variant, 0, 0).front().ticks == 0);
  }
  for (const std::uint8_t variant : std::array<std::uint8_t, 3>{0, 4, 5}) {
    INFO("variante " << static_cast<int>(variant));
    CHECK(Advanced(variant, 0, 0).front().ticks == -24);
    // ⚠ ET TANT QU'ELLE ATTEND, RIEN NE BOUGE : ni l'alpha, ni le rayon.
    CHECK(Advanced(variant, 0, 10).front().alpha == 0);
  }
}

TEST_CASE("le rayon croît de deux façons qui ne se ressemblent pas", "[render][cone_wedge]") {
  // ⚠ TROIS VARIANTES AJOUTENT TROIS UNITÉS par tic — une ouverture en ligne
  // droite — et toutes les autres MULTIPLIENT par un et quinze centièmes, ce qui
  // est une exponentielle. Les confondre donnerait un éventail deux fois trop
  // large ou deux fois trop étroit selon la variante.
  const float start = Advanced(2, 0, 0).front().radius_at_anchor;
  CHECK(Advanced(2, 0, 5).front().radius_at_anchor == Approx(start + 15.0F));

  // ⚠ ON COMPARE À NOMBRE DE TICS ACTIFS ÉGAL, et il faut compenser le retard :
  // la variante quatre attend vingt-quatre tics avant de commencer. Comparer à
  // nombre de tics ÉCOULÉS égal donnerait l'exponentielle pour perdante, et on
  // conclurait à l'envers.
  constexpr std::int32_t kDelay = 24;
  const float exponentiel = Advanced(4, 0, kDelay + 30).front().radius_at_anchor;
  const float lineaire = Advanced(2, 0, 30).front().radius_at_anchor;
  CHECK(exponentiel > lineaire);
}

TEST_CASE("l'alpha monte puis retombe, à trois rythmes", "[render][cone_wedge]") {
  CHECK(Advanced(2, 0, 10).front().alpha == 100);
  CHECK(Advanced(3, 0, 10).front().alpha == 60);
  // ⚠ ET LE RAYON CROÎT MÊME PENDANT LE PALIER, entre le onzième et le treizième
  // tic, où l'alpha ne bouge plus : le client le pose hors des branches d'alpha.
  CHECK(Advanced(2, 0, 11).front().alpha == 100);
  CHECK(Advanced(2, 0, 11).front().radius_at_anchor > Advanced(2, 0, 10).front().radius_at_anchor);
  CHECK(Advanced(2, 0, 20).front().alpha < 100);
  // Et il ne descend jamais sous zéro.
  CHECK(Advanced(2, 0, 200).front().alpha == 0);
}

TEST_CASE("les neuf éventails sont en table, avec leurs tours de boucle", "[render][cone_wedge]") {
  const std::span<const EffectConeWedgePattern> table = EffectConeWedgePatternTable();
  // ⚠ NEUF : 618 appelle ce bâtisseur en SECOND dans son cas, et seul un relevé
  // qui lit TOUS les bâtisseurs d'un cas le voit.
  CHECK(table.size() == 9);
  CHECK(std::ranges::is_sorted(table, {}, &EffectConeWedgePattern::effect_id));

  std::int32_t nodes = 0;
  for (const EffectConeWedgePattern& pattern : table) {
    INFO("effet " << pattern.effect_id);
    CHECK_FALSE(pattern.texture.empty());
    // ⚠ CHACUN POSE PLUSIEURS NŒUDS, et c'est là tout l'éventail : un seul
    // donnerait un rayon au lieu de cinq à douze.
    CHECK(pattern.node_count > 1);
    nodes += pattern.node_count;
    CHECK(EffectConeWedgePatternsFor(pattern.effect_id).size() == 1);

    // Et tout rayon avancé a de l'aire.
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        Advanced(pattern.variant, static_cast<std::uint8_t>(pattern.node_count - 1), 40);
    for (const EffectTrack& track : tracks) {
      const std::vector<EffectWorldQuad> quads = ComposeConeWedgeQuads(track, kAtTheOrigin);
      REQUIRE(quads.size() == 2);
    }
  }
  CHECK(nodes == 65);
  CHECK(EffectConeWedgePatternsFor(1).empty());
}

TEST_CASE("un éventail entier rejoint la troupe", "[render][cone_wedge]") {
  const EffectConeWedgePattern& pattern = EffectConeWedgePatternsFor(386).front();
  CHECK(pattern.node_count == 12);

  EffectNodePool pool;
  std::uint32_t seed = 9U;
  for (std::uint8_t radius = 0; radius < pattern.node_count; ++radius) {
    pool.Add(SowEffectConeWedgeNode(pattern, radius, glm::vec3{1.0F, 0.0F, 2.0F},
                                    ro::render::kEffectNodeDefaultLifeTicks, seed));
  }
  REQUIRE(pool.size() == pattern.node_count);
  for (int tick = 0; tick < 8; ++tick) {
    pool.Advance(16);
  }
  // Les douze rayons pointent dans douze directions distinctes.
  std::set<std::int16_t> caps;
  for (const ro::render::PooledEffectNode& node : pool.nodes()) {
    CHECK(node.shape == PooledNodeShape::kConeWedge);
    caps.insert(node.tracks[0].tilt_degrees);
  }
  CHECK(caps.size() == pattern.node_count);
}
