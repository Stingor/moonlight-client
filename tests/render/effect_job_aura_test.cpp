#include "ro/render/effect_job_aura.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_job_aura_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceJobAuraTracks;
using ro::render::ComposeLayeredAuraQuads;
using ro::render::ComposePlainAuraNodeQuads;
using ro::render::ComposePlainAuraQuads;
using ro::render::EffectFourParticleOffsetOf;
using ro::render::EffectJobAuraFlips;
using ro::render::EffectJobAuraPaletteOf;
using ro::render::EffectJobAuraPattern;
using ro::render::EffectJobAuraPatternsFor;
using ro::render::EffectJobAuraPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kJobAuraAlphaFall;
using ro::render::kJobAuraAlphaRise;
using ro::render::kJobAuraBlueMark;
using ro::render::kJobAuraBluePalette;
using ro::render::kJobAuraFrameCount;
using ro::render::kJobAuraLayers;
using ro::render::kJobAuraLayerScales;
using ro::render::kJobAuraMarkOfPalette;
using ro::render::kJobAuraOrbitRadius;
using ro::render::kJobAuraPaleMark;
using ro::render::kJobAuraPalePalette;
using ro::render::kJobAuraParticles;
using ro::render::kJobAuraRebirthHeightFloor;
using ro::render::kJobAuraRebirthHeightSpread;
using ro::render::kJobAuraRedPalette;
using ro::render::kJobAuraRisePhases;
using ro::render::kJobAuraStartPhase;
using ro::render::kJobAuraValueOfAlpha;
using ro::render::kJobAuraValueOfAngle;
using ro::render::kJobAuraValueOfFrame;
using ro::render::kJobAuraValueOfHeight;
using ro::render::kJobAuraValueOfPhase;
using ro::render::kJobAuraValuesPerParticle;
using ro::render::kLayeredAuraRadius;
using ro::render::kLayeredAuraStartAlphas;
using ro::render::kPlainAuraColor;
using ro::render::kPlainAuraRadius;
using ro::render::kPlainAuraRed;
using ro::render::kPlainAuraRedMark;
using ro::render::kPlainAuraStartAlphas;
using ro::render::SeedEffectJobAuraTracks;
using ro::render::SowEffectJobAuraNode;

namespace {

constexpr std::array<std::string_view, 5> kImages = {{"a.bmp", "b.bmp", "c.bmp", "d.bmp", "e.bmp"}};

[[nodiscard]] EffectJobAuraPattern TestPattern(std::uint8_t tracks, std::uint8_t mark,
                                               bool layers) {
  EffectJobAuraPattern pattern;
  pattern.effect_id = 1;
  pattern.client_function = "test";
  pattern.textures = kImages;
  pattern.live_tracks = tracks;
  pattern.palette_mark = mark;
  pattern.layered = layers;
  return pattern;
}

// UNE PISTE VIVE, posée à la main : quatre particules à un alpha franc, sur
// quatre angles distincts.
[[nodiscard]] EffectTrack LiveTrack() {
  EffectTrack track;
  track.active = true;
  track.ticks = 1000;
  track.radius_at_anchor = kLayeredAuraRadius;
  for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
    const std::size_t base = kJobAuraValuesPerParticle * particle;
    track.value[base + kJobAuraValueOfPhase] = kJobAuraStartPhase;
    track.value[base + kJobAuraValueOfAlpha] = 200.0F;
    track.value[base + kJobAuraValueOfFrame] = static_cast<float>(particle);
    track.value[base + kJobAuraValueOfAngle] = static_cast<float>(90 * particle);
  }
  return track;
}

// LE PLUS GRAND ÉCART entre deux coins d'un quad — la diagonale du carré.
[[nodiscard]] float DiagonaleDe(const EffectWorldQuad& quad) {
  float plus_grand = 0.0F;
  for (const glm::vec3& un : quad.corners) {
    for (const glm::vec3& other : quad.corners) {
      plus_grand = std::max(plus_grand, glm::length(un - other));
    }
  }
  return plus_grand;
}

}  // namespace

TEST_CASE("l'aura a trois couches rend douze carres par piste vive", "[render][job_aura]") {
  const EffectTrack track = LiveTrack();
  const std::vector<EffectWorldQuad> quads =
      ComposeLayeredAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(quads.size() == kJobAuraLayers * kJobAuraParticles);
}

TEST_CASE("l'aura a une couche n'en rend que quatre", "[render][job_aura]") {
  EffectTrack track = LiveTrack();
  track.radius_at_anchor = kPlainAuraRadius;
  const std::vector<EffectWorldQuad> quads =
      ComposePlainAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  REQUIRE(quads.size() == kJobAuraParticles);
}

TEST_CASE("une piste eteinte ne rend rien", "[render][job_aura]") {
  EffectTrack track = LiveTrack();
  track.active = false;
  REQUIRE(ComposeLayeredAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
  REQUIRE(ComposePlainAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor).empty());
}

TEST_CASE("les trois couches grandissent et s'assombrissent", "[render][job_aura]") {
  const EffectTrack track = LiveTrack();
  const std::vector<EffectWorldQuad> quads =
      ComposeLayeredAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F});

  // Les quads sortent couche par couche : les quatre premiers sont la plus
  // serrée, les quatre derniers la plus large.
  for (std::size_t layer = 1; layer < kJobAuraLayers; ++layer) {
    const float before = DiagonaleDe(quads[(layer - 1) * kJobAuraParticles]);
    const float after = DiagonaleDe(quads[layer * kJobAuraParticles]);
    REQUIRE(after > before);
  }
  // ⚠ ET LA PLUS LARGE EST LA PLUS SOMBRE : c'est ce qui fait un halo plutôt
  // qu'un bloc, puisque les trois s'ajoutent.
  const EffectWorldQuad& tight = quads[0];
  const EffectWorldQuad& large = quads[(kJobAuraLayers - 1) * kJobAuraParticles];
  REQUIRE(large.tint[2] < tight.tint[2]);
}

TEST_CASE("l'echelle de la couche multiplie le rayon a l'ancre", "[render][job_aura]") {
  const EffectTrack track = LiveTrack();
  const std::vector<EffectWorldQuad> quads =
      ComposeLayeredAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F});
  const float tight = DiagonaleDe(quads[0]);
  const float large = DiagonaleDe(quads[(kJobAuraLayers - 1) * kJobAuraParticles]);
  REQUIRE(large / tight ==
          Approx(kJobAuraLayerScales[kJobAuraLayers - 1] / kJobAuraLayerScales[0]).epsilon(1e-4));
}

TEST_CASE("chaque particule tourne sur un cercle de rayon quatre", "[render][job_aura]") {
  EffectTrack track = LiveTrack();
  // Un seul angle change d'une particule à l'autre ; le centre du quad doit
  // donc parcourir un cercle de rayon fixe autour de l'ancre.
  const std::vector<EffectWorldQuad> quads =
      ComposePlainAuraQuads(track, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
    glm::vec3 centre{0.0F, 0.0F, 0.0F};
    for (const glm::vec3& corner : quads[particle].corners) {
      centre += corner;
    }
    centre /= 4.0F;
    const float radius = std::sqrt(centre.x * centre.x + centre.z * centre.z);
    REQUIRE(radius == Approx(kJobAuraOrbitRadius).epsilon(1e-3));
  }
}

TEST_CASE("la hauteur de l'aura ELEVE la figure", "[render][job_aura]") {
  EffectTrack basse = LiveTrack();
  EffectTrack haute = LiveTrack();
  haute.value[kJobAuraValueOfHeight] = 20.0F;
  const std::vector<EffectWorldQuad> au_sol =
      ComposePlainAuraQuads(basse, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  const std::vector<EffectWorldQuad> on_top =
      ComposePlainAuraQuads(haute, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  // ⚠ LE CLIENT RETRANCHE, ET SON REPÈRE DESCEND : retrancher ÉLÈVE. Recopier
  // le signe au lieu du sens enfoncerait l'aura sous les pieds.
  REQUIRE(on_top[0].corners[0].y > au_sol[0].corners[0].y);
  REQUIRE(on_top[0].corners[0].y - au_sol[0].corners[0].y == Approx(20.0F).epsilon(1e-4));
}

TEST_CASE("l'alpha du sous-type 99 est plafonne par les tics", "[render][job_aura]") {
  EffectTrack jeune = LiveTrack();
  jeune.ticks = 3;
  const std::vector<EffectWorldQuad> quads =
      ComposeLayeredAuraQuads(jeune, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F});
  for (const EffectWorldQuad& quad : quads) {
    REQUIRE(quad.tint[3] <= 3);
  }
}

TEST_CASE("l'alpha du sous-type 35 ne l'est PAS", "[render][job_aura]") {
  EffectTrack jeune = LiveTrack();
  jeune.ticks = 3;
  const std::vector<EffectWorldQuad> quads =
      ComposePlainAuraQuads(jeune, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  // L'aura à une couche paraît à plein dès son premier tic.
  REQUIRE(quads[0].tint[3] == 200);
}

TEST_CASE("l'image ZERO et l'image DEUX inversent le retournement", "[render][job_aura]") {
  // Un écart qui met la particule « loin » de la caméra.
  constexpr float kFarAngle = 100.0F;
  REQUIRE(EffectJobAuraFlips(kFarAngle, 0, 0));
  REQUIRE(EffectJobAuraFlips(kFarAngle, 0, 2));
  REQUIRE_FALSE(EffectJobAuraFlips(kFarAngle, 0, 1));
  REQUIRE_FALSE(EffectJobAuraFlips(kFarAngle, 0, 3));

  // Et un écart qui la met « près » : la règle s'inverse tout entière.
  constexpr float kAnglePres = 10.0F;
  REQUIRE_FALSE(EffectJobAuraFlips(kAnglePres, 0, 0));
  REQUIRE(EffectJobAuraFlips(kAnglePres, 0, 1));
}

TEST_CASE("les deux seuils du retournement ne sont pas symetriques", "[render][job_aura]") {
  // ⚠ SOIXANTE-DIX D'UN CÔTÉ, QUATRE-VINGTS DE L'AUTRE. Les arrondir au même
  // nombre décalerait le retournement de dix degrés sur un côté.
  REQUIRE(EffectJobAuraFlips(70.0F, 0, 0));
  REQUIRE_FALSE(EffectJobAuraFlips(-70.0F, 0, 0));
  REQUIRE(EffectJobAuraFlips(-80.0F, 0, 0));
}

TEST_CASE("le retournement echange les coins deux a deux", "[render][job_aura]") {
  EffectTrack right = LiveTrack();
  EffectTrack retournee = LiveTrack();
  // Une seule particule vive suffit : on compare la même à deux angles dont un
  // seul retourne.
  right.value[kJobAuraValueOfFrame] = 1.0F;
  right.value[kJobAuraValueOfAngle] = 10.0F;
  retournee.value[kJobAuraValueOfFrame] = 0.0F;
  retournee.value[kJobAuraValueOfAngle] = 10.0F;
  const std::vector<EffectWorldQuad> a =
      ComposePlainAuraQuads(right, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  const std::vector<EffectWorldQuad> b =
      ComposePlainAuraQuads(retournee, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F}, kPlainAuraColor);
  // Même carré, coins permutés : A avec B, C avec D.
  REQUIRE(a[0].corners[0].x == Approx(b[0].corners[1].x));
  REQUIRE(a[0].corners[1].x == Approx(b[0].corners[0].x));
  REQUIRE(a[0].corners[2].x == Approx(b[0].corners[3].x));
  REQUIRE(a[0].corners[3].x == Approx(b[0].corners[2].x));
}

TEST_CASE("la marque choisit la palette des trois couches", "[render][job_aura]") {
  REQUIRE(EffectJobAuraPaletteOf(kJobAuraBlueMark) == kJobAuraBluePalette);
  REQUIRE(EffectJobAuraPaletteOf(kJobAuraPaleMark) == kJobAuraPalePalette);
  // ⚠ TOUTE AUTRE MARQUE retombe sur le rouge, et le client n'en connaît que
  // trois : c'est un `else`, pas un troisième cas.
  REQUIRE(EffectJobAuraPaletteOf(2) == kJobAuraRedPalette);
  REQUIRE(EffectJobAuraPaletteOf(99) == kJobAuraRedPalette);
}

TEST_CASE("le ROUGE du sous-type 35 DEBORDE sur les pistes suivantes", "[render][job_aura]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track = LiveTrack();
    track.radius_at_anchor = kPlainAuraRadius;
  }
  // Seule la SECONDE porte la marque rouge.
  tracks[1].mark[kJobAuraMarkOfPalette] = kPlainAuraRedMark;

  const std::vector<EffectWorldQuad> quads =
      ComposePlainAuraNodeQuads(tracks, 0, 0, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(quads.size() == kEffectTracksPerNode * kJobAuraParticles);

  // La première reste bleue ; les trois autres sont rouges, y compris celles
  // qui ne portent pas la marque. C'est l'effet de bord du client, transcrit.
  REQUIRE(quads[0].tint[0] == kPlainAuraColor[0]);
  for (std::size_t rang = kJobAuraParticles; rang < quads.size(); ++rang) {
    REQUIRE(quads[rang].tint[0] == kPlainAuraRed[0]);
    REQUIRE(quads[rang].tint[1] == kPlainAuraRed[1]);
  }
}

TEST_CASE("le mouvement range les deux angles de camera dans les styles", "[render][job_aura]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  std::uint32_t generator = 1U;
  AdvanceJobAuraTracks(tracks, 137, 42, 0.0F, generator);
  REQUIRE(tracks[0].style == 137);
  REQUIRE(tracks[1].style == 42);
}

TEST_CASE("le compteur n'avance que sur une piste vive", "[render][job_aura]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  std::uint32_t generator = 1U;
  AdvanceJobAuraTracks(tracks, 0, 0, 0.0F, generator);
  REQUIRE(tracks[0].ticks == 1);
  // ⚠ AU CONTRAIRE DES LUCIOLES : le client teste avant d'incrémenter, et c'est
  // ce compteur qui plafonne l'alpha du sous-type 99.
  REQUIRE(tracks[1].ticks == 0);
}

TEST_CASE("une particule monte de vingt-cinq puis descend de douze", "[render][job_aura]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].value[kJobAuraValueOfPhase] = 0.0F;
  tracks[0].value[kJobAuraValueOfAlpha] = 0.0F;
  std::uint32_t generator = 1U;

  // Les neuf premiers tics montent.
  for (std::int32_t tick = 1; tick < static_cast<std::int32_t>(kJobAuraRisePhases); ++tick) {
    AdvanceJobAuraTracks(tracks, 0, 0, 0.0F, generator);
    REQUIRE(tracks[0].value[kJobAuraValueOfAlpha] ==
            Approx(kJobAuraAlphaRise * static_cast<float>(tick)));
  }
  const float vertex = tracks[0].value[kJobAuraValueOfAlpha];
  // ⚠ ET LE DIXIÈME DESCEND DÉJÀ : le client compare la phase APRÈS l'avoir
  // incrémentée. Tester avant ferait monter un tic de trop.
  AdvanceJobAuraTracks(tracks, 0, 0, 0.0F, generator);
  REQUIRE(tracks[0].value[kJobAuraValueOfAlpha] == Approx(vertex - kJobAuraAlphaFall));
}

TEST_CASE("une particule epuisee RENAIT plus haut", "[render][job_aura]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  // Déjà en descente, et à un cran de sa fin.
  tracks[0].value[kJobAuraValueOfPhase] = kJobAuraStartPhase;
  tracks[0].value[kJobAuraValueOfAlpha] = 15.0F;
  std::uint32_t generator = 7U;

  AdvanceJobAuraTracks(tracks, 0, 0, 0.0F, generator);

  REQUIRE(tracks[0].value[kJobAuraValueOfPhase] == Approx(0.0F));
  REQUIRE(tracks[0].value[kJobAuraValueOfAlpha] == Approx(0.0F));
  const float angle = tracks[0].value[kJobAuraValueOfAngle];
  REQUIRE(angle >= 0.0F);
  REQUIRE(angle < 360.0F);
  const float image = tracks[0].value[kJobAuraValueOfFrame];
  REQUIRE(image >= 0.0F);
  REQUIRE(image < static_cast<float>(kJobAuraFrameCount));
  // ⚠ LA HAUTEUR EST DANS LE REPÈRE DU CLIENT, qui DESCEND : de moins trois à
  // moins dix, c'est-à-dire de trois à dix AU-DESSUS du nœud.
  const float height = EffectFourParticleOffsetOf(tracks[0], 0).y;
  REQUIRE(height <= static_cast<float>(kJobAuraRebirthHeightFloor));
  REQUIRE(height >=
          static_cast<float>(kJobAuraRebirthHeightFloor - kJobAuraRebirthHeightSpread + 1));
}

TEST_CASE("les quatre particules d'une piste ne renaissent pas ensemble", "[render][job_aura]") {
  EffectJobAuraPattern pattern = TestPattern(1, 0, true);
  std::uint32_t generator = 3U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectJobAuraTracks(pattern, generator);
  // ⚠ C'EST TOUT CE QUI LES SÉPARE : même phase, quatre alphas. Leur donner le
  // même les ferait clignoter à l'unisson pour toute la vie de l'aura.
  for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
    const std::size_t base = kJobAuraValuesPerParticle * particle;
    REQUIRE(tracks[0].value[base + kJobAuraValueOfPhase] == Approx(kJobAuraStartPhase));
    REQUIRE(tracks[0].value[base + kJobAuraValueOfAlpha] ==
            Approx(kLayeredAuraStartAlphas[particle]));
  }
}

TEST_CASE("le semis n'allume que les pistes que le site demande", "[render][job_aura]") {
  for (const std::uint8_t count : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{4}}) {
    EffectJobAuraPattern pattern = TestPattern(count, 0, false);
    std::uint32_t generator = 11U;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectJobAuraTracks(pattern, generator);
    std::size_t living = 0;
    for (const EffectTrack& track : tracks) {
      if (track.active) {
        ++living;
      }
    }
    REQUIRE(living == count);
  }
}

TEST_CASE("une piste eteinte ne consomme aucun tirage au semis", "[render][job_aura]") {
  // ⚠ AU CONTRAIRE DES LUCIOLES, dont les pistes éteintes tirent quand même :
  // ce bâtisseur-ci ne boucle que sur celles qu'il allume. Les faire tirer
  // décalerait la suite du générateur.
  EffectJobAuraPattern one = TestPattern(1, 0, true);
  EffectJobAuraPattern four = TestPattern(4, 0, true);
  std::uint32_t after_one = 5U;
  std::uint32_t after_four = 5U;
  static_cast<void>(SeedEffectJobAuraTracks(one, after_one));
  static_cast<void>(SeedEffectJobAuraTracks(four, after_four));
  REQUIRE(after_one != after_four);

  // Et la première piste des deux est la MÊME : le générateur part du même
  // état et les tirages d'une piste ne dépendent que d'elle.
  std::uint32_t a = 5U;
  std::uint32_t b = 5U;
  const std::array<EffectTrack, kEffectTracksPerNode> single = SeedEffectJobAuraTracks(one, a);
  const std::array<EffectTrack, kEffectTracksPerNode> all = SeedEffectJobAuraTracks(four, b);
  REQUIRE(single[0].value[kJobAuraValueOfAngle] == Approx(all[0].value[kJobAuraValueOfAngle]));
}

TEST_CASE("les deux sous-types n'ont ni le meme rayon ni les memes alphas", "[render][job_aura]") {
  std::uint32_t a = 2U;
  std::uint32_t b = 2U;
  EffectJobAuraPattern layers = TestPattern(1, 0, true);
  EffectJobAuraPattern plate = TestPattern(1, 0, false);
  const std::array<EffectTrack, kEffectTracksPerNode> with = SeedEffectJobAuraTracks(layers, a);
  const std::array<EffectTrack, kEffectTracksPerNode> sans = SeedEffectJobAuraTracks(plate, b);
  REQUIRE(with[0].radius_at_anchor == Approx(kLayeredAuraRadius));
  REQUIRE(sans[0].radius_at_anchor == Approx(kPlainAuraRadius));
  REQUIRE(with[0].value[kJobAuraValueOfAlpha] == Approx(kLayeredAuraStartAlphas[0]));
  REQUIRE(sans[0].value[kJobAuraValueOfAlpha] == Approx(kPlainAuraStartAlphas[0]));
}

TEST_CASE("le semis pose la marque de teinte sur chaque piste vive", "[render][job_aura]") {
  EffectJobAuraPattern pattern = TestPattern(2, kJobAuraPaleMark, true);
  std::uint32_t generator = 13U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectJobAuraTracks(pattern, generator);
  REQUIRE(tracks[0].mark[kJobAuraMarkOfPalette] == kJobAuraPaleMark);
  REQUIRE(tracks[1].mark[kJobAuraMarkOfPalette] == kJobAuraPaleMark);
}

TEST_CASE("le noeud seme porte les cinq images et la bonne forme", "[render][job_aura]") {
  std::uint32_t generator = 17U;
  EffectJobAuraPattern layers = TestPattern(2, 0, true);
  const ro::render::PooledEffectNode node =
      SowEffectJobAuraNode(layers, glm::vec3{1.0F, 2.0F, 3.0F}, 100, generator);
  REQUIRE(node.resource_paths.size() == kImages.size());
  REQUIRE(node.shape == ro::render::PooledNodeShape::kLayeredAura);
  // ⚠ LE NŒUD SUIT SON PORTEUR : son mouvement recopie la position à chaque
  // tic, et le semis part de la même.
  REQUIRE(node.owner_position.x == Approx(1.0F));

  std::uint32_t other = 17U;
  EffectJobAuraPattern plate = TestPattern(2, 0, false);
  REQUIRE(SowEffectJobAuraNode(plate, glm::vec3{0.0F, 0.0F, 0.0F}, 100, other).shape ==
          ro::render::PooledNodeShape::kPlainAura);
}

TEST_CASE("la table des auras porte les six identifiants, triee", "[render][job_aura]") {
  const std::span<const EffectJobAuraPattern> table = EffectJobAuraPatternTable();
  REQUIRE(table.size() == 6);
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectJobAuraPattern::effect_id));
  for (const std::uint32_t effect_id : {203U, 254U, 418U, 455U, 485U, 486U}) {
    REQUIRE(EffectJobAuraPatternsFor(effect_id).size() == 1);
  }
  REQUIRE(EffectJobAuraPatternsFor(1U).empty());
}

TEST_CASE("les trois identifiants du sous-type 99 ont trois teintes", "[render][job_aura]") {
  // 486 est bleu, 485 blanc doré, 418 rouge — les trois auras de niveau.
  REQUIRE(EffectJobAuraPatternsFor(486U)[0].palette_mark == kJobAuraBlueMark);
  REQUIRE(EffectJobAuraPatternsFor(485U)[0].palette_mark == kJobAuraPaleMark);
  REQUIRE(EffectJobAuraPatternsFor(418U)[0].palette_mark == 2);
  for (const std::uint32_t effect_id : {418U, 485U, 486U}) {
    REQUIRE(EffectJobAuraPatternsFor(effect_id)[0].layered);
    REQUIRE(EffectJobAuraPatternsFor(effect_id)[0].live_tracks == 2);
  }
}

TEST_CASE("les trois identifiants du sous-type 35 n'ont pas le meme nombre de pistes",
          "[render][job_aura]") {
  REQUIRE(EffectJobAuraPatternsFor(203U)[0].live_tracks == 1);
  REQUIRE(EffectJobAuraPatternsFor(455U)[0].live_tracks == 2);
  REQUIRE(EffectJobAuraPatternsFor(254U)[0].live_tracks == 4);
  for (const std::uint32_t effect_id : {203U, 254U, 455U}) {
    REQUIRE_FALSE(EffectJobAuraPatternsFor(effect_id)[0].layered);
  }
  // ⚠ ET CELUI-CI EST ROUGE : sa marque vaut un, ce qui pour le sous-type 35
  // ne choisit pas une palette mais force le rouge.
  REQUIRE(EffectJobAuraPatternsFor(455U)[0].palette_mark == kPlainAuraRedMark);
}
