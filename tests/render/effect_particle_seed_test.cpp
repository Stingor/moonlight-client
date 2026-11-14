#include "ro/render/effect_particle_seed.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_figures.h"

using ro::render::ComposeRisingParticleQuad;
using ro::render::EffectNodePool;
using ro::render::EffectRisingParticlePattern;
using ro::render::EffectRisingParticlePatternsFor;
using ro::render::EffectRisingParticlePatternTable;
using ro::render::EffectTrack;
using ro::render::kEffectRisingParticleLifeTicks;
using ro::render::kEffectTicksPerSecond;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectRisingParticleTracks;
using ro::render::SowEffectRisingParticleNode;

namespace {

// Le retard le plus long que le bâtisseur tire, et la demi-diagonale la plus
// employée.
constexpr std::int32_t kLongestDelayTicks = 47;
constexpr float kUsualHalfDiagonal = 5.0F;
// Une marque qui dessine, et qui monte vite.
constexpr std::uint8_t kFastMark = 2;

float AreaOf(const ro::render::EffectWorldQuad& quad) {
  return glm::length(
      glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
}

}  // namespace

TEST_CASE("Particules : la table est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectRisingParticlePattern> table = EffectRisingParticlePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Particules : chaque rangée a de quoi se montrer", "[render][table]") {
  // Une demi-diagonale nulle donnerait un quad sans aire, et une marque hors
  // table ne dessinerait rien : les deux ressemblent à une figure ratée.
  for (const EffectRisingParticlePattern& pattern : EffectRisingParticlePatternTable()) {
    CHECK(!pattern.texture.empty());
    CHECK(pattern.half_diagonal > 0.0F);
    CHECK(ro::render::EffectRisingParticleIsDrawn(pattern.color_case));
  }
}

TEST_CASE("Particules : un effet en sème PLUSIEURS, avec des textures différentes",
          "[render][table]") {
  // Le 770 en sème huit. N'en poser qu'un montrerait une arme là où le client en
  // fait tourner huit.
  const std::span<const EffectRisingParticlePattern> huit = EffectRisingParticlePatternsFor(770);
  REQUIRE(huit.size() == 8);
  CHECK(huit[0].texture != huit[1].texture);

  // Et le 495 en sème deux, à deux tailles : le relevé par push ne lisait ni
  // l'une ni l'autre.
  const std::span<const EffectRisingParticlePattern> two = EffectRisingParticlePatternsFor(495);
  REQUIRE(two.size() == 2);
  CHECK(two[0].half_diagonal == Catch::Approx(5.0F));
  CHECK(two[1].half_diagonal == Catch::Approx(9.0F));
}

TEST_CASE("Particules : la MARQUE est évaluée, pas recopiée", "[render][table]") {
  // Le bâtisseur traduit le nombre du site en marque par cinq branches, dont la
  // dernière est un choix sans branchement. Le C++ ne voit jamais ce nombre.
  CHECK(EffectRisingParticlePatternsFor(495).front().color_case == 2);
  CHECK(EffectRisingParticlePatternsFor(820).front().color_case == 6);
  CHECK(EffectRisingParticlePatternsFor(907).front().color_case == 7);
  CHECK(EffectRisingParticlePatternsFor(2436).front().color_case == 8);
  CHECK(EffectRisingParticlePatternsFor(2437).front().color_case == 9);
}

TEST_CASE("Particules : les pièces lancées en l'air, trente nœuds qui s'écartent plus vite",
          "[render][table]") {
  // LE SECOND BÂTISSEUR DU SOUS-TYPE : une image seule, et tout le reste écrit
  // en dur — une demi-diagonale de trois et demi, la marque un, un rayon qui
  // croît d'un et demi par tic, et un site qui l'appelle trente fois.
  const std::span<const EffectRisingParticlePattern> pieces = EffectRisingParticlePatternsFor(274);
  REQUIRE(pieces.size() == 1);
  CHECK(pieces[0].client_function == "Effect_Build_RisingScatter2b");
  CHECK(pieces[0].texture == "data\\texture\\effect\\coin_a.bmp");
  CHECK(pieces[0].half_diagonal == Catch::Approx(3.5F));
  CHECK(pieces[0].color_case == 1);
  CHECK(pieces[0].radius_growth_per_tick == Catch::Approx(1.5F));
  CHECK(pieces[0].copies == 30);

  // L'AÎNÉ GARDE SON PAS D'UN, et un seul nœud par appel.
  CHECK(EffectRisingParticlePatternsFor(495).front().radius_growth_per_tick == Catch::Approx(1.0F));
  CHECK(EffectRisingParticlePatternsFor(495).front().copies == 1);

  // LE PAS VA JUSQU'AU MOUVEMENT : rangé en première case de chaque piste, il
  // fait grandir la sphère d'un et demi par tic.
  std::uint32_t generator = 5;
  const ro::render::PooledEffectNode node = SowEffectRisingParticleNode(pieces[0], {}, generator);
  for (const EffectTrack& track : node.tracks) {
    CHECK(track.value[0] == Catch::Approx(1.5F));
    CHECK(track.mark[0] == 1);
  }
  EffectTrack partie = node.tracks[0];
  partie.ticks = 0;
  ro::render::AdvanceRisingParticleTrack(partie);
  CHECK(partie.radius_at_anchor == Catch::Approx(13.5F));
}

TEST_CASE("Particules : un effet ABSENT rend un span vide", "[render][table]") {
  CHECK(EffectRisingParticlePatternsFor(0).empty());
  CHECK(EffectRisingParticlePatternsFor(12).empty());
}

TEST_CASE("Particules : les quatre pistes se TIRENT AU SORT", "[render][effecttrack]") {
  // C'est la figure même de l'effet : quatre particules qui partent dans quatre
  // directions. Les semer identiques les superposerait, et l'on n'en verrait
  // qu'une.
  std::uint32_t generator = 7;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRisingParticleTracks(kUsualHalfDiagonal, kFastMark, {}, generator);

  bool all_same = true;
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    all_same = all_same && tracks[rang].degrees == tracks[0].degrees &&
               tracks[rang].azimuth_degrees == tracks[0].azimuth_degrees &&
               tracks[rang].tilt_degrees == tracks[0].tilt_degrees;
  }
  CHECK_FALSE(all_same);

  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.radius_at_anchor == Catch::Approx(12.0F));
    CHECK(track.value[1] == Catch::Approx(kUsualHalfDiagonal));
    CHECK(track.mark[0] == kFastMark);
    CHECK(track.ticks <= -18);
    CHECK(track.ticks >= -kLongestDelayTicks);
    CHECK(track.degrees >= 0);
    CHECK(track.degrees < 360);
  }
}

TEST_CASE("Particules : la même graine rejoue la MÊME figure", "[render][effecttrack]") {
  // C'est ce qui rend un effet vérifiable au banc : deux semis d'une même graine
  // ne diffèrent pas.
  std::uint32_t one = 42;
  std::uint32_t other = 42;
  const std::array<EffectTrack, kEffectTracksPerNode> left =
      SeedEffectRisingParticleTracks(kUsualHalfDiagonal, kFastMark, {}, one);
  const std::array<EffectTrack, kEffectTracksPerNode> right =
      SeedEffectRisingParticleTracks(kUsualHalfDiagonal, kFastMark, {}, other);

  for (std::size_t rang = 0; rang < left.size(); ++rang) {
    CHECK(left[rang].ticks == right[rang].ticks);
    CHECK(left[rang].degrees == right[rang].degrees);
    CHECK(left[rang].azimuth_degrees == right[rang].azimuth_degrees);
  }
}

TEST_CASE("Particules : l'ANCRE se pose sur les quatre pistes", "[render][effecttrack]") {
  // Le bâtisseur copie la position de l'effet dans chaque piste. Sans elle, les
  // quatre particules partiraient de l'origine du monde.
  std::uint32_t generator = 3;
  const glm::vec3 ou{12.0F, 3.0F, -5.0F};
  for (const EffectTrack& track :
       SeedEffectRisingParticleTracks(kUsualHalfDiagonal, kFastMark, ou, generator)) {
    CHECK(track.anchor.x == Catch::Approx(ou.x));
    CHECK(track.anchor.y == Catch::Approx(ou.y));
    CHECK(track.anchor.z == Catch::Approx(ou.z));
  }
}

TEST_CASE("Particules : la VIE du nœud couvre la plus lente de ses pistes",
          "[render][effecttrack]") {
  // Ce nombre est DÉDUIT du mouvement, faute d'avoir trouvé celui du client, et
  // ce test est ce qui le tient : à ce tic la piste la plus retardée est
  // éteinte, et un tic plus tôt elle ne l'est pas.
  EffectTrack tardive;
  tardive.active = true;
  tardive.ticks = -kLongestDelayTicks;
  tardive.mark[0] = kFastMark;
  tardive.value[0] = 1.0F;

  EffectTrack dormant = tardive;
  for (std::int32_t tick = 0; tick < kEffectRisingParticleLifeTicks - 1; ++tick) {
    ro::render::AdvanceRisingParticleTrack(dormant);
  }
  CHECK(dormant.alpha > 0);

  ro::render::AdvanceRisingParticleTrack(dormant);
  CHECK(dormant.alpha == 0);
}

TEST_CASE("Particules : semées puis battues, elles MONTRENT quelque chose",
          "[render][effecttrack]") {
  // Le seul test qui relie les trois mains sur cette famille : le semis, le
  // mouvement et la figure. Chacune passe ses épreuves sans que l'ensemble
  // dessine, et c'est ce qui est arrivé au lot 5.
  std::size_t montrent = 0;
  for (const EffectRisingParticlePattern& pattern : EffectRisingParticlePatternTable()) {
    std::uint32_t generator = pattern.effect_id + 1U;
    EffectNodePool pool;
    pool.Add(SowEffectRisingParticleNode(pattern, glm::vec3{0.0F, 0.0F, 0.0F}, generator));
    // Assez pour que la piste la plus retardée soit partie et bien visible.
    pool.Advance(4U * static_cast<std::uint32_t>(kEffectTicksPerSecond) * 16U);
    REQUIRE(pool.size() == 1);

    const ro::render::PooledEffectNode& node = pool.nodes().front();
    CHECK(node.shape == PooledNodeShape::kRisingParticles);
    float area = 0.0F;
    for (const EffectTrack& track : node.tracks) {
      if (const std::optional<ro::render::EffectWorldQuad> world_quad =
              ComposeRisingParticleQuad(track, 0, 0);
          world_quad.has_value()) {
        area += AreaOf(*world_quad);
      }
    }
    if (area > 0.0F) {
      ++montrent;
    }
  }
  CHECK(montrent == EffectRisingParticlePatternTable().size());
}
