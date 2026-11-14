#include "ro/render/effect_aura_column.h"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_aura_column_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceRingExpansionTracks;
using ro::render::ComposeAuraColumnQuads;
using ro::render::EffectAuraColumnPattern;
using ro::render::EffectAuraColumnPatternsFor;
using ro::render::EffectAuraColumnPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kAuraColumnInnerTint;
using ro::render::kAuraColumnLayers;
using ro::render::kAuraColumnOuterTint;
using ro::render::kAuraColumnRadius;
using ro::render::kAuraColumnWideStep;
using ro::render::kEffectTracksPerNode;
using ro::render::kMarkOfAuraColumnShape;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectAuraColumnTracks;
using ro::render::SowEffectAuraColumnNode;

namespace {

constexpr std::uint32_t kFrostMysty = 726;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advance(std::int32_t count) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectAuraColumnTracks();
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceRingExpansionTracks(tracks, 0, 0);
  }
  return tracks;
}

}  // namespace

TEST_CASE("la colonne naît eteinte et ne rend rien", "[render][aura_column]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectAuraColumnTracks();
  CHECK(tracks.front().active);
  CHECK(tracks.front().radius_at_anchor == Approx(kAuraColumnRadius));
  for (std::size_t layer = 1; layer <= kAuraColumnLayers; ++layer) {
    CHECK(tracks.front().value[layer] == Approx(0.0F));
  }
  // ⚠ UNE COUCHE D'ALPHA NUL NE S'EMET PAS : au premier tic, la colonne est
  // entierement invisible. C'est le mouvement qui l'ouvre.
  CHECK(ComposeAuraColumnQuads(tracks.front(), 0, 0, kAtOrigin).empty());
}

TEST_CASE("elle s'ouvre couche par couche", "[render][aura_column]") {
  // ⚠ LES DIX COUCHES MONTENT DU MEME PAS et s'arretent a des plafonds
  // differents : au bout de quelques tics, les plus etroites sont deja au
  // plafond et les plus larges montent encore.
  const auto tot = Advance(6);
  CHECK(ComposeAuraColumnQuads(tot.front(), 0, 0, kAtOrigin).size() == kAuraColumnLayers);
  // La couche un plafonne a soixante-dix, la dix a deux cent cinquante.
  CHECK(tot.front().value[1] == Approx(70.0F));
  CHECK(tot.front().value[kAuraColumnLayers] == Approx(6 * 13.0F));

  // ⚠ ET LA PLUS LARGE N ATTEINT JAMAIS SON PLAFOND. Elle monte de treize par tic
  // et la montee cesse au vingtieme : dix-neuf montees font deux cent
  // quarante-sept, et son plafond est a deux cent cinquante. Les neuf autres y
  // arrivent toutes. Ecrire deux cent cinquante dans le test aurait masque cela.
  const auto later = Advance(19);
  for (std::size_t layer = 1; layer < kAuraColumnLayers; ++layer) {
    CHECK(later.front().value[layer] == Approx(50.0F + 20.0F * static_cast<float>(layer)));
  }
  CHECK(later.front().value[kAuraColumnLayers] == Approx(19 * 13.0F));
}

TEST_CASE("neuf couches se retirent, la plus large attend", "[render][aura_column]") {
  // ⚠ LA BOUCLE DE RETRAIT N'EN COUVRE QUE NEUF. La dixieme attend le centieme
  // tic, et c'est ce qui laisse un bord a la colonne quand son coeur s'est
  // eteint.
  // ⚠ ET LE RETRAIT SE VOIT DANS L ORDRE INVERSE DE L OUVERTURE : les plus
  // etroites, qui plafonnaient le plus bas, s eteignent les premieres. A soixante
  // tics il ne reste que les trois plus larges.
  const auto a_soixante = Advance(60);
  CHECK(a_soixante.front().value[1] == Approx(0.0F));
  CHECK(a_soixante.front().value[7] == Approx(0.0F));
  CHECK(a_soixante.front().value[kAuraColumnLayers] == Approx(19 * 13.0F));
  const std::vector<EffectWorldQuad> remainder =
      ComposeAuraColumnQuads(a_soixante.front(), 0, 0, kAtOrigin);
  REQUIRE(remainder.size() == 3);
  CHECK(remainder.front().tint[0] == kAuraColumnOuterTint[0]);

  // ET ELLE FINIT PAR PARTIR, une fois le centieme tic passe.
  const auto a_la_fin = Advance(160);
  CHECK(a_la_fin.front().value[kAuraColumnLayers] == Approx(0.0F));
  CHECK(ComposeAuraColumnQuads(a_la_fin.front(), 0, 0, kAtOrigin).empty());
}

TEST_CASE("la plus large est la plus claire", "[render][aura_column]") {
  const auto opened = Advance(6);
  const std::vector<EffectWorldQuad> column =
      ComposeAuraColumnQuads(opened.front(), 0, 0, kAtOrigin);
  REQUIRE(column.size() == kAuraColumnLayers);
  // ⚠ LE CLIENT DESCEND DE DIX A UN : le premier quad soumis est le plus large,
  // et c'est le seul a porter la teinte claire.
  CHECK(column.front().tint[0] == kAuraColumnOuterTint[0]);
  CHECK(column.back().tint[0] == kAuraColumnInnerTint[0]);

  // ET IL EST AUSSI LE PLUS PETIT EN RAYON : le rayon DECROIT avec le rang de la
  // couche, puisque le client ajoute « dix moins la couche » fois le pas.
  const float large = std::abs(column.front().corners[0].x);
  const float narrow = std::abs(column.back().corners[0].x);
  CHECK(narrow > large);
}

TEST_CASE("le rayon enfle plus vite quand le dernier bord vit", "[render][aura_column]") {
  const auto before = Advance(99);
  const auto after = Advance(101);
  // Deux tics plus tard, le rayon a ete multiplie TROIS fois et non deux : le
  // client multiplie une seconde fois tant que la derniere couche vit.
  const float expected = before.front().radius_at_anchor * 1.01F * 1.01F * 1.01F;
  CHECK(after.front().radius_at_anchor == Approx(expected).epsilon(0.001));
}

TEST_CASE("les quatre coins sont un carre", "[render][aura_column]") {
  const auto opened = Advance(6);
  const EffectWorldQuad quad = ComposeAuraColumnQuads(opened.front(), 0, 0, kAtOrigin).front();
  // ⚠ QUARANTE-CINQ, CENT TRENTE-CINQ, DEUX CENT VINGT-CINQ, TROIS CENT QUINZE :
  // les quatre coins sont a la meme distance du centre, et leurs diagonales se
  // croisent en leur milieu.
  const glm::vec3 middle_a = (quad.corners[0] + quad.corners[2]) * 0.5F;
  const glm::vec3 middle_b = (quad.corners[1] + quad.corners[3]) * 0.5F;
  CHECK(middle_a.x == Approx(middle_b.x).margin(0.001F));
  CHECK(middle_a.z == Approx(middle_b.z).margin(0.001F));
  // ET LES QUATRE MONTENT ENSEMBLE : la colonne est droite, non penchee.
  for (std::size_t corner = 1; corner < 4; ++corner) {
    CHECK(quad.corners[corner].y == Approx(quad.corners[0].y));
  }
  // ⚠ ET ELLES MONTENT AU-DESSUS DU PORTEUR : le client retranche la montee
  // dans son repere qui descend. La retrancher dans le notre enfoncait la
  // colonne de trente unites.
  CHECK(quad.corners[0].y > 0.0F);
}

TEST_CASE("le noeud seme porte sa forme et sa marque", "[render][aura_column]") {
  const std::span<const EffectAuraColumnPattern> table = EffectAuraColumnPatternTable();
  REQUIRE_FALSE(table.empty());
  CHECK(EffectAuraColumnPatternsFor(kFrostMysty).size() == 1);

  const ro::render::PooledEffectNode node =
      SowEffectAuraColumnNode(table.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 60);
  CHECK(node.shape == PooledNodeShape::kAuraColumn);
  // ⚠ LA MARQUE DE FORME VAUT UN, NON DEUX : a deux la colonne se serrerait et
  // le mouvement cesserait de suivre son porteur.
  CHECK(node.tracks.front().mark[kMarkOfAuraColumnShape] == 1);
  CHECK(kAuraColumnWideStep > 0.0F);
  REQUIRE(node.resource_paths.size() == 1);
}
