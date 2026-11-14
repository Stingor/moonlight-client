#include "ro/render/effect_pie_wedge.h"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pie_wedge_seed.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceLiveTrackTicks;
using ro::render::ComposePieWedgeQuads;
using ro::render::EffectPieWedgePattern;
using ro::render::EffectPieWedgePatternsFor;
using ro::render::EffectPieWedgePatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackFullTurnDegrees;
using ro::render::kEffectTracksPerNode;
using ro::render::kPieWedgeDegreesPerStep;
using ro::render::kPieWedgeSowingTicks;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectPieWedgeTracks;
using ro::render::SowEffectPieWedgeNode;

namespace {

constexpr std::uint32_t kCharge = 895;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

}  // namespace

TEST_CASE("douze quartiers ferment le tour en trois tics", "[render][pie_wedge]") {
  // ⚠ QUATRE PISTES FOIS TROIS TICS FONT DOUZE, et douze fois trente degrés font
  // le tour : c'est le compte qui donne son sens au semis.
  CHECK(kEffectTracksPerNode * kPieWedgeSowingTicks * kPieWedgeDegreesPerStep ==
        kEffectTrackFullTurnDegrees);

  std::set<std::int16_t> inclinaisons;
  for (std::int32_t tick = 0; tick < kPieWedgeSowingTicks; ++tick) {
    for (const EffectTrack& track : SeedEffectPieWedgeTracks(tick)) {
      CHECK(track.active);
      inclinaisons.insert(track.tilt_degrees);
    }
  }
  // AUCUNE INCLINAISON N'EST RÉPÉTÉE : c'est la preuve que le tic entre dans le
  // compte. S'il n'y entrait pas, les trois nœuds se superposeraient.
  CHECK(inclinaisons.size() == 12);
  CHECK(*inclinaisons.begin() == 0);
  CHECK(*inclinaisons.rbegin() == kEffectTrackFullTurnDegrees - kPieWedgeDegreesPerStep);

  // LE DEUXIÈME NŒUD REPREND OÙ LE PREMIER S'ARRÊTE.
  CHECK(SeedEffectPieWedgeTracks(1).front().tilt_degrees == 120);
  CHECK(SeedEffectPieWedgeTracks(2).back().tilt_degrees == 330);
}

TEST_CASE("le quartier est un triangle, pointe au centre", "[render][pie_wedge]") {
  const EffectTrack track = SeedEffectPieWedgeTracks(0).front();
  const std::vector<EffectWorldQuad> quads = ComposePieWedgeQuads(track, kAtOrigin);
  REQUIRE(quads.size() == 1);
  const EffectWorldQuad& quartier = quads.front();
  // ⚠ LES DEUX DERNIERS COINS VALENT LE MÊME POINT : la voie A ne sait émettre
  // que des quads, et le client s'en sert pour poser des triangles.
  CHECK(quartier.corners[2] == quartier.corners[3]);
  // LA POINTE EST AU CENTRE, en x et en z.
  CHECK(quartier.corners[2].x == Approx(0.0F));
  CHECK(quartier.corners[2].z == Approx(0.0F));
  // ET LES DEUX COINS DU BORD SONT À CENT UNITÉS DU CENTRE, l'un de chaque côté.
  CHECK(quartier.corners[0] != quartier.corners[1]);
}

TEST_CASE("l'ouverture vaut dix degres, et elle reboucle", "[render][pie_wedge]") {
  // ⚠ LE QUARTIER POSÉ À ZÉRO S'OUVRE DE PART ET D'AUTRE DE ZÉRO, donc un de ses
  // bords passe par trois cent cinquante-cinq. C'est le cas de bord du client, et
  // son rebouclage asymétrique le couvre.
  EffectTrack a_zero = SeedEffectPieWedgeTracks(0).front();
  REQUIRE(a_zero.tilt_degrees == 0);
  const EffectWorldQuad first = ComposePieWedgeQuads(a_zero, kAtOrigin).front();

  // SES DEUX BORDS SONT SUR LE CERCLE, l'un à trois cent cinquante-cinq degrés et
  // l'autre à cinq : un rebouclage manqué enverrait le premier à moins cinq, donc
  // au même endroit que le second par symétrie du cosinus — et le quartier
  // n'aurait plus d'aire. C'est ce que ce test empêche. La profondeur s'inverse
  // en passant du repère du client au nôtre : le bord de trois cent
  // cinquante-cinq, négatif chez lui, est positif ici.
  CHECK(first.corners[0].z > 0.0F);
  CHECK(first.corners[1].z < 0.0F);
  // Et la pointe se dresse au-dessus du sol : l'apex du semis est négatif dans
  // le repère du client, qui descend.
  CHECK(first.corners[2].y > 0.0F);

  // ⚠ ET LE QUARTIER DE TROIS CENT TRENTE NE SORT PAS DU TOUR NON PLUS : son bord
  // lointain est à trois cent trente-cinq, pas à trois cent soixante-quinze.
  EffectTrack at_end = a_zero;
  at_end.tilt_degrees = 330;
  const std::vector<EffectWorldQuad> last = ComposePieWedgeQuads(at_end, kAtOrigin);
  REQUIRE(last.size() == 1);
  // Les deux coins du bord restent sur le cercle de cent unités.
  for (std::size_t corner = 0; corner < 2; ++corner) {
    const glm::vec3 point = last.front().corners[corner];
    CHECK(std::sqrt(point.x * point.x + point.z * point.z) == Approx(100.0F).margin(0.01F));
  }
}

TEST_CASE("une piste eteinte ou pale ne rend rien", "[render][pie_wedge]") {
  EffectTrack faded = SeedEffectPieWedgeTracks(0).front();
  faded.active = false;
  CHECK(ComposePieWedgeQuads(faded, kAtOrigin).empty());

  EffectTrack pale = SeedEffectPieWedgeTracks(0).front();
  pale.alpha = 0;
  CHECK(ComposePieWedgeQuads(pale, kAtOrigin).empty());
}

TEST_CASE("le mouvement n'avance que les compteurs des pistes vives", "[render][pie_wedge]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectPieWedgeTracks(0);
  tracks[2].active = false;
  const std::int16_t tilt = tracks[0].tilt_degrees;
  const float radius = tracks[0].radius_at_anchor;

  AdvanceLiveTrackTicks(tracks);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[1].ticks == 1);
  CHECK(tracks[2].ticks == 0);
  CHECK(tracks[3].ticks == 1);
  // ⚠ ET RIEN D'AUTRE NE BOUGE : le quartier est immobile, et c'est son SEMIS qui
  // le disperse. Lui prêter un mouvement ferait tourner ce que le client laisse
  // en place.
  CHECK(tracks[0].tilt_degrees == tilt);
  CHECK(tracks[0].radius_at_anchor == Approx(radius));
  CHECK(tracks[0].alpha == 255);
}

TEST_CASE("la table des eclats nomme son effet", "[render][pie_wedge]") {
  const std::span<const EffectPieWedgePattern> table = EffectPieWedgePatternTable();
  REQUIRE_FALSE(table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
  CHECK(EffectPieWedgePatternsFor(kCharge).size() == 1);
  CHECK(EffectPieWedgePatternsFor(1U).empty());

  const ro::render::PooledEffectNode node =
      SowEffectPieWedgeNode(table.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 2, 60);
  CHECK(node.shape == PooledNodeShape::kPieWedge);
  CHECK(node.tracks.back().tilt_degrees == 330);
  REQUIRE(node.resource_paths.size() == 1);
}
