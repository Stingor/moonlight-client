#include "ro/render/effect_grand_cross.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <span>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_grand_cross_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceGrandCrossFrameTracks;
using ro::render::AdvanceGrandCrossRibbonTracks;
using ro::render::ComposeGrandCrossFrameQuads;
using ro::render::ComposeGrandCrossRibbonQuads;
using ro::render::EffectGrandCrossPattern;
using ro::render::EffectGrandCrossPatternsFor;
using ro::render::EffectGrandCrossPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGrandCrossFrameHalfDepth;
using ro::render::kGrandCrossFrameHalfWidth;
using ro::render::kGrandCrossFrameLivingTracks;
using ro::render::kGrandCrossFrameQuadsPerTrack;
using ro::render::kGrandCrossLastTick;
using ro::render::kGrandCrossRibbonAlphaRise;
using ro::render::kGrandCrossRibbonRadius;
using ro::render::kGrandCrossRibbonRadiusReset;
using ro::render::kGrandCrossRiseTicks;
using ro::render::SeedGrandCrossFrameTracks;
using ro::render::SeedGrandCrossRibbonTracks;
using ro::render::SowGrandCrossFrameNode;
using ro::render::SowGrandCrossRibbonNode;

namespace {

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> RibbonAfter(std::int32_t ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedGrandCrossRibbonTracks();
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceGrandCrossRibbonTracks(tracks);
  }
  return tracks;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> FrameAfter(std::int32_t ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedGrandCrossFrameTracks();
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceGrandCrossFrameTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("le ruban allume ses quatre bras, le cadre deux", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> ribbon = SeedGrandCrossRibbonTracks();
  for (const EffectTrack& track : ribbon) {
    CHECK(track.active);
    // ⚠ IL NAÎT INVISIBLE : c'est le mouvement qui monte l'alpha.
    CHECK(track.alpha == 0);
  }
  // ⚠ ET LE CADRE N'EN ALLUME QUE DEUX. Le bâtisseur couche les deux dernières
  // explicitement ; en allumer quatre doublerait ses panneaux.
  const std::array<EffectTrack, kEffectTracksPerNode> frame = SeedGrandCrossFrameTracks();
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    CHECK(frame[rang].active == (rang < kGrandCrossFrameLivingTracks));
  }
}

TEST_CASE("les quatre bras sont decales aux quatre coins", "[render][effect][grandcross]") {
  // ⚠ CE SONT DES OCTETS SIGNÉS, et c'est ce qui fait la croix. Lus non signés,
  // deux des quatre bras partiraient à deux cent trente-trois unités.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedGrandCrossRibbonTracks();
  std::set<std::pair<int, int>> corners;
  for (const EffectTrack& track : tracks) {
    corners.insert(
        {static_cast<std::int8_t>(track.mark[0]), static_cast<std::int8_t>(track.mark[1])});
  }
  CHECK(corners.size() == kEffectTracksPerNode);
  for (const auto& [x, z] : corners) {
    CHECK(std::abs(x) == 23);
    CHECK(std::abs(z) == 23);
  }
  // Et leurs azimuts couvrent les quatre quarts, sans doublon.
  std::set<std::int16_t> azimuts;
  for (const EffectTrack& track : tracks) {
    azimuts.insert(track.azimuth_degrees);
  }
  CHECK(azimuts == std::set<std::int16_t>{0, 90, 180, 270});
}

TEST_CASE("l'alpha monte dix tics puis redescend", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> montant =
      RibbonAfter(kGrandCrossRiseTicks - 1);
  CHECK(montant[0].alpha == kGrandCrossRibbonAlphaRise * (kGrandCrossRiseTicks - 1));
  // Au tic suivant il redescend d'un cran, et non de dix.
  const std::array<EffectTrack, kEffectTracksPerNode> bascule = RibbonAfter(kGrandCrossRiseTicks);
  CHECK(bascule[0].alpha == montant[0].alpha - 1);
}

TEST_CASE("le ruban REPART quand son alpha s'annule", "[render][effect][grandcross]") {
  // ⚠ LA PISTE NE MEURT PAS À L'EXTINCTION DE SON ALPHA : il faut aussi avoir
  // passé le trentième tic. Entre les deux, le rayon repart de vingt — c'est
  // ainsi que la croix pulse sans qu'aucune boucle ne soit écrite.
  // ⚠ ET LE TRENTIÈME TIC N'EST PAS UNE DURÉE DE VIE. L'alpha monte de dix
  // pendant neuf tics — quatre-vingt-dix — puis ne redescend que d'un par tic :
  // il lui faut quatre-vingt-dix tics de plus pour s'annuler. La piste vit donc
  // quatre-vingt-dix-neuf tics là où trente semblaient écrits, et c'est le
  // premier tic où les DEUX conditions se réunissent qui la tue.
  // L'alpha vaut son sommet au neuvième tic, et descend d'un par tic à partir
  // du dixième : il s'annule au tic « sommet plus neuf ».
  constexpr std::int32_t kPeak = kGrandCrossRibbonAlphaRise * (kGrandCrossRiseTicks - 1);
  constexpr std::int32_t kMort = kPeak + kGrandCrossRiseTicks - 1;
  static_assert(kMort > kGrandCrossLastTick);

  const std::array<EffectTrack, kEffectTracksPerNode> before = RibbonAfter(kMort - 1);
  CHECK(before[0].active);
  CHECK(before[0].alpha == 1);
  CHECK(before[0].radius_at_anchor < kGrandCrossRibbonRadius);

  const std::array<EffectTrack, kEffectTracksPerNode> after = RibbonAfter(kMort);
  CHECK_FALSE(after[0].active);
  CHECK(after[0].alpha == 0);
  // Son rayon repart au MÊME instant : la remise à vingt est liée à
  // l'annulation de l'alpha, non au trentième tic.
  CHECK(after[0].radius_at_anchor == Approx(kGrandCrossRibbonRadiusReset));
}

TEST_CASE("le rayon du ruban decroit avant de repartir", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = RibbonAfter(3);
  CHECK(tracks[0].radius_at_anchor < kGrandCrossRibbonRadius);
  CHECK(tracks[0].radius_at_anchor == Approx(kGrandCrossRibbonRadius - 0.03F));
}

TEST_CASE("le cadre s'elargit et se souleve", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = FrameAfter(4);
  CHECK(tracks[0].value[0] == Approx(kGrandCrossFrameHalfWidth + 0.02F));
  CHECK(tracks[0].value[1] == Approx(kGrandCrossFrameHalfDepth + 0.02F));
  // La hauteur d'extrusion suit le sinus du tic : au quatrième, elle est petite
  // mais non nulle.
  CHECK(tracks[0].value[2] > 0.0F);
  // ⚠ ET LES DEUX PISTES ÉTEINTES N'AVANCENT PAS.
  CHECK(tracks[3].ticks == 0);
  CHECK(tracks[3].value[0] == Approx(0.0F));
}

TEST_CASE("le cadre rend six panneaux, dont deux a mi-alpha", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = FrameAfter(5);
  const std::vector<EffectWorldQuad> quads = ComposeGrandCrossFrameQuads(tracks[0], {});
  REQUIRE(quads.size() == kGrandCrossFrameQuadsPerTrack);
  const std::uint8_t full = quads.front().tint[3];
  CHECK(full > 0);
  CHECK(quads[4].tint[3] == full / 2);
  CHECK(quads[5].tint[3] == full / 2);
  // ⚠ LE CADRE EST AU-DESSUS DU SOL : un cran au-dessus pour sa base, plus haut
  // pour son rebord. Le client écrit ces hauteurs négatives, dans son repère qui
  // descend ; recopiées, elles enterraient le cadre.
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y >= 1.0F);
    }
  }
  CHECK(quads.front().corners[2].y > quads.front().corners[1].y);

  // Une piste éteinte ne rend rien, et une piste à alpha nul non plus.
  EffectTrack faded = tracks[0];
  faded.active = false;
  CHECK(ComposeGrandCrossFrameQuads(faded, {}).empty());
}

TEST_CASE("le ruban lit une bande de son image par segment", "[render][effect][grandcross]") {
  // 🔴 SANS COORDONNÉES DE TEXTURE, LE RUBAN ÉTAIT INVISIBLE. Chaque sommet
  // lisait le texel du coin de `ring_white.tga`, qui est transparent : les
  // quatre bras se peignaient en entier sans qu'un pixel paraisse, et Grand
  // Cross ne montrait rien du tout, quoique semé.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = RibbonAfter(5);
  const std::vector<EffectWorldQuad> quads = ComposeGrandCrossRibbonQuads(tracks[0], 0, 1, {});
  REQUIRE(quads.size() == 8);
  for (std::size_t index = 0; index < quads.size(); ++index) {
    const float band_start = static_cast<float>(index) / ro::render::kGrandCrossRibbonTextureBands;
    const float band_end =
        static_cast<float>(index + 1) / ro::render::kGrandCrossRibbonTextureBands;
    const std::array<glm::vec2, 4>& uv = quads[index].texture_uv;
    // L'arc — les deux premiers coins — porte l'ordonnée un, le bout du bras zéro.
    CHECK(uv[0].x == Approx(band_start));
    CHECK(uv[0].y == Approx(1.0F));
    CHECK(uv[1].x == Approx(band_end));
    CHECK(uv[1].y == Approx(1.0F));
    CHECK(uv[2].x == Approx(band_end));
    CHECK(uv[2].y == Approx(0.0F));
    CHECK(uv[3].x == Approx(band_start));
    CHECK(uv[3].y == Approx(0.0F));
  }
  // Les bandes se suivent : le second bord d'un segment est le premier du suivant.
  for (std::size_t index = 1; index < quads.size(); ++index) {
    CHECK(quads[index].texture_uv[0].x == Approx(quads[index - 1].texture_uv[1].x));
  }
}

TEST_CASE("le cadre lit l'image entiere retournee, cotes a l'ordinaire et sol en addition",
          "[render][effect][grandcross]") {
  // Le quad projeté du client, pour les six panneaux : l'image entière, un en
  // ordonnée aux deux premiers coins et zéro aux deux derniers ; le mode un pour
  // les côtés — mélange ordinaire —, zéro pour les deux panneaux du sol —
  // addition. Le cadre ne posait ni image ni mélange.
  constexpr std::int32_t kSourceAlpha = 5;
  constexpr std::int32_t kInverseSourceAlpha = 6;
  constexpr std::int32_t kOne = 2;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = FrameAfter(5);
  const std::vector<EffectWorldQuad> quads = ComposeGrandCrossFrameQuads(tracks[0], {});
  REQUIRE(quads.size() == kGrandCrossFrameQuadsPerTrack);
  for (std::size_t index = 0; index < quads.size(); ++index) {
    const std::array<glm::vec2, 4>& uv = quads[index].texture_uv;
    CHECK(uv[0] == glm::vec2{0.0F, 1.0F});
    CHECK(uv[1] == glm::vec2{1.0F, 1.0F});
    CHECK(uv[2] == glm::vec2{1.0F, 0.0F});
    CHECK(uv[3] == glm::vec2{0.0F, 0.0F});
    CHECK(quads[index].blend_source == kSourceAlpha);
    const bool floor_panel = index >= ro::render::kGrandCrossFrameSideQuads;
    CHECK(quads[index].blend_destination == (floor_panel ? kOne : kInverseSourceAlpha));
    CHECK(quads[index].tint[0] == 255);
    CHECK(quads[index].tint[1] == 255);
    CHECK(quads[index].tint[2] == 255);
  }
  // ⚠ LA PLACE DU NŒUD S'AJOUTE UNE FOIS, pas deux : le quad projeté la pose
  // sur des coins relatifs.
  const glm::vec3 anchor{100.0F, 0.0F, 200.0F};
  const std::vector<EffectWorldQuad> placed = ComposeGrandCrossFrameQuads(tracks[0], anchor);
  REQUIRE(placed.size() == quads.size());
  CHECK(placed.front().corners[0].x == Approx(quads.front().corners[0].x + anchor.x));
  CHECK(placed.front().corners[0].z == Approx(quads.front().corners[0].z + anchor.z));
}

TEST_CASE("le ruban joint ses segments et suit son arc", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = RibbonAfter(5);
  const std::vector<EffectWorldQuad> quads = ComposeGrandCrossRibbonQuads(tracks[0], 0, 0, {});
  // Neuf pas de dix degrés sur un balayage de quatre-vingt-dix, et le premier
  // segment ne joint rien : huit panneaux.
  REQUIRE(quads.size() == 8);
  // ⚠ CHAQUE PANNEAU PARTAGE DEUX COINS AVEC LE SUIVANT : c'est un ruban, pas
  // une suite de carrés indépendants. Les séparer laisserait une fente entre
  // chaque segment.
  for (std::size_t rang = 1; rang < quads.size(); ++rang) {
    CHECK(quads[rang].corners[0] == quads[rang - 1].corners[1]);
    CHECK(quads[rang].corners[3] == quads[rang - 1].corners[2]);
  }
  // Le bras S'ÉLÈVE : son extrémité est au-dessus du plan de l'arc. Le client
  // l'écrit à une ordonnée négative, dans son repère qui descend.
  CHECK(quads.front().corners[2].y > quads.front().corners[1].y);
}

namespace {

// L'extrémité du premier segment, au centième près.
[[nodiscard]] std::pair<int, int> ArmEnd(const EffectTrack& track, std::size_t rang) {
  const std::vector<EffectWorldQuad> quads = ComposeGrandCrossRibbonQuads(track, rang, 0, {});
  if (quads.empty()) {
    return {0, 0};
  }
  const glm::vec3 tip = quads.front().corners[2];
  return {static_cast<int>(std::lround(tip.x * 100.0F)),
          static_cast<int>(std::lround(tip.z * 100.0F))};
}

}  // namespace

TEST_CASE("l'inclinaison de quatre-vingt-dix rend le rang MUET", "[render][effect][grandcross]") {
  // ⚠ CE QUI SÉPARE LES QUATRE BRAS N'EST PAS L'ANGLE DU BRAS, et le croire
  // aurait fait chercher un défaut là où il n'y en a pas. Le bâtisseur sème une
  // inclinaison de quatre-vingt-dix degrés, dont le cosinus est NUL : la part
  // horizontale du bras s'annule, le ruban pend tout droit, et la chaîne de
  // comparaisons sur le rang ne déplace plus rien. Ce sont les deux marques
  // signées, et elles seules, qui posent les quatre bras aux quatre coins.
  EffectTrack track = RibbonAfter(5)[0];
  track.mark[0] = 0;
  track.mark[1] = 0;
  std::set<std::pair<int, int>> extremites;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    extremites.insert(ArmEnd(track, rang));
  }
  CHECK(extremites.size() == 1);

  // ⚠ ET LA CHAÎNE À QUATRE CAS N'EN DONNE QUE DEUX. Quatre cent cinquante et
  // quatre-vingt-dix diffèrent d'un TOUR, six cent trente et deux cent soixante-
  // dix aussi : le rang zéro vise comme le deux, le un comme le trois. La chaîne
  // du client a l'air de distinguer quatre bras, elle n'en distingue que deux.
  // Cela ne se voit qu'en couchant l'inclinaison, puisque droite elle annule
  // tout — et c'est pourquoi ce test la couche.
  track.tilt_degrees = 45;
  std::set<std::pair<int, int>> couchees;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    couchees.insert(ArmEnd(track, rang));
  }
  CHECK(couchees.size() == 2);
  CHECK(ArmEnd(track, 0) == ArmEnd(track, 2));
  CHECK(ArmEnd(track, 1) == ArmEnd(track, 3));
}

TEST_CASE("les marques signees ecartent les quatre bras", "[render][effect][grandcross]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = RibbonAfter(5);
  std::set<std::pair<int, int>> ancres;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    const std::vector<EffectWorldQuad> quads =
        ComposeGrandCrossRibbonQuads(tracks[rang], rang, 0, {});
    REQUIRE_FALSE(quads.empty());
    const glm::vec3 corner_index = quads.front().corners[0];
    ancres.insert({static_cast<int>(std::lround(corner_index.x)),
                   static_cast<int>(std::lround(corner_index.z))});
  }
  CHECK(ancres.size() == kEffectTracksPerNode);
}

TEST_CASE("la table de Grand Cross porte ses deux identifiants", "[render][effect][grandcross]") {
  const std::span<const EffectGrandCrossPattern> table = EffectGrandCrossPatternTable();
  REQUIRE(table.size() == 2);
  CHECK(std::ranges::is_sorted(table, {}, &EffectGrandCrossPattern::effect_id));

  REQUIRE_FALSE(EffectGrandCrossPatternsFor(226).empty());
  const EffectGrandCrossPattern& white = EffectGrandCrossPatternsFor(226).front();
  // ⚠ LE CADRE DE 226 EST JAUNE, et cette image n'apparaissait dans AUCUN relevé :
  // la fonction a deux branches, et c'est le site qui tranche (ADR 0019).
  CHECK(white.ribbon_texture == "data\\texture\\effect\\ring_white.tga");
  CHECK(white.frame_texture == "data\\texture\\effect\\ring_yellow.tga");
  CHECK(white.ribbon_node_scale == Approx(1.0F));

  const EffectGrandCrossPattern& noir = EffectGrandCrossPatternsFor(450).front();
  CHECK(noir.ribbon_texture == "data\\texture\\effect\\ring_black.tga");
  CHECK(noir.frame_texture == noir.ribbon_texture);
  CHECK(noir.ribbon_node_scale == Approx(12.0F));

  CHECK(EffectGrandCrossPatternsFor(1).empty());
}

TEST_CASE("les deux noeuds se sement separement", "[render][effect][grandcross]") {
  const EffectGrandCrossPattern& pattern = EffectGrandCrossPatternsFor(226).front();
  const ro::render::PooledEffectNode ribbon = SowGrandCrossRibbonNode(pattern, {}, 60);
  const ro::render::PooledEffectNode frame = SowGrandCrossFrameNode(pattern, {}, 60);
  CHECK(ribbon.shape == ro::render::PooledNodeShape::kGrandCrossRibbon);
  CHECK(frame.shape == ro::render::PooledNodeShape::kGrandCrossFrame);
  REQUIRE(ribbon.resource_paths.size() == 1);
  REQUIRE(frame.resource_paths.size() == 1);
  CHECK(ribbon.resource_paths.front() != frame.resource_paths.front());
  CHECK(ribbon.node.life_ticks == 60);
  // Le mot de rendu que les deux bâtisseurs laissent : translucide, au mélange
  // propre de chaque panneau — le seau des fichiers.
  CHECK(ribbon.node.render_word == ro::render::kGrandCrossRenderWord);
  CHECK(frame.node.render_word == ro::render::kGrandCrossRenderWord);
  CHECK(ro::render::kGrandCrossRenderWord == 5U);
  CHECK(ro::render::EffectLotBucketRank(ro::render::kGrandCrossRenderWord) ==
        ro::render::EffectBucketRank::kStr);
}
