#include "ro/render/effect_fading_streak.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_fading_streak_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceFadingStreakTracks;
using ro::render::ComposeFadingStreakQuads;
using ro::render::EffectFadingStreakPattern;
using ro::render::EffectFadingStreakPatternsFor;
using ro::render::EffectFadingStreakPatternTable;
using ro::render::EffectFadingStreakTintFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kFadingStreakCopies;
using ro::render::kFadingStreakImage;
using ro::render::kFadingStreakRise;
using ro::render::kMarkOfFadingStreakImage;
using ro::render::kMarkOfFadingStreakSpeedKey;
using ro::render::kMarkOfFadingStreakTint;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectFadingStreakTracks;
using ro::render::SowEffectFadingStreakNode;

namespace {

constexpr std::uint32_t kColdThrow = 791;
constexpr glm::vec3 kSource{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kTarget{60.0F, 0.0F, 0.0F};

// UNE PISTE POSÉE À LA MAIN : la cible à soixante unités, une clé de vingt tics.
[[nodiscard]] EffectTrack Flocon() {
  EffectTrack track;
  track.active = true;
  track.alpha = 255;
  track.ticks = 10;
  track.radius_at_anchor = 3.0F;
  track.anchor = kTarget;
  track.tilt_degrees = 0;
  track.mark[kMarkOfFadingStreakImage] = kFadingStreakImage;
  return track;
}

}  // namespace

TEST_CASE("la traînée : quatre teintes, et rien au-delà", "[render][fading_streak]") {
  std::array<std::uint8_t, 3> tint{};
  CHECK(EffectFadingStreakTintFor(0, tint));
  CHECK(tint == std::array<std::uint8_t, 3>{55, 55, 255});
  CHECK(EffectFadingStreakTintFor(3, tint));
  CHECK(tint == std::array<std::uint8_t, 3>{85, 85, 255});
  // ⚠ PAS DE CAS PAR DÉFAUT : une cinquième marque n'émet rien du tout.
  CHECK_FALSE(EffectFadingStreakTintFor(4, tint));
  CHECK(ComposeFadingStreakQuads(Flocon(), 0, 0, kSource, 4, 20).empty());
}

TEST_CASE("la traînée : cinq copies, la tête franche et la queue éteinte",
          "[render][fading_streak]") {
  const EffectTrack track = Flocon();
  const std::vector<EffectWorldQuad> trail = ComposeFadingStreakQuads(track, 0, 0, kSource, 0, 20);
  REQUIRE(trail.size() == kFadingStreakCopies);

  // ⚠ LA PREMIÈRE GARDE TOUT SON ALPHA, la deuxième en perd CENT VINGT d'un coup,
  // puis vingt de plus chacune. Ce n'est pas un dégradé régulier.
  CHECK(trail[0].tint[3] == 255);
  CHECK(trail[1].tint[3] == 255 - 120);
  CHECK(trail[2].tint[3] == 255 - 140);
  CHECK(trail[3].tint[3] == 255 - 160);
  CHECK(trail[4].tint[3] == 255 - 180);

  // Toutes prennent la quatrième image, celle que la marque désigne.
  for (const EffectWorldQuad& quad : trail) {
    CHECK(quad.texture == kFadingStreakImage);
  }
}

TEST_CASE("la traînée : le flocon avance avec le TEMPS, pas d'un pas", "[render][fading_streak]") {
  // La cible est à soixante unités, la clé vaut vingt tics : au dixième tic, le
  // flocon a fait la moitié du chemin.
  EffectTrack track = Flocon();
  const std::vector<EffectWorldQuad> a_dix = ComposeFadingStreakQuads(track, 0, 0, kSource, 0, 20);
  glm::vec3 head{0.0F, 0.0F, 0.0F};
  for (const glm::vec3& corner : a_dix.front().corners) {
    head += corner * 0.25F;
  }
  CHECK(head.x == Approx(30.0F).margin(0.001));
  // ⚠ ET LE QUAD S'ÉLÈVE DE DOUZE AU-DESSUS DU NŒUD : le client retranche dans
  // son repère qui descend, ce qui élève.
  CHECK(head.y == Approx(kSource.y + kFadingStreakRise).margin(0.001));

  // ⚠ LA CLÉ DIVISE : la moitié de la clé fait deux fois le chemin dans le même
  // temps. C'est ce qui fait arriver tous les flocons ensemble quelle que soit
  // la portée.
  const std::vector<EffectWorldQuad> twice_as_fast =
      ComposeFadingStreakQuads(track, 0, 0, kSource, 0, 10);
  glm::vec3 fast{0.0F, 0.0F, 0.0F};
  for (const glm::vec3& corner : twice_as_fast.front().corners) {
    fast += corner * 0.25F;
  }
  CHECK(fast.x == Approx(60.0F).margin(0.001));

  // ⚠ ET UNE CLÉ NULLE NE DIVISE PAS : le client s'y diviserait par zéro.
  track.ticks = 5;
  CHECK(ComposeFadingStreakQuads(track, 0, 0, kSource, 0, 0).empty());
}

TEST_CASE("la traînée : le flocon tourne, s'allume puis s'éteint", "[render][fading_streak]") {
  std::uint32_t generator = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingStreakTracks(kSource, kTarget, generator);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.anchor.x == Approx(kTarget.x));
    CHECK(track.mark[kMarkOfFadingStreakSpeedKey] >= 40);
    CHECK(track.mark[kMarkOfFadingStreakSpeedKey] <= 60);
  }
  const std::int16_t start_angle = tracks.front().degrees;

  for (std::int32_t tick = 0; tick < 9; ++tick) {
    AdvanceFadingStreakTracks(tracks, 0, 0);
  }
  // ⚠ NEUF TICS DE MONTÉE, pas dix : la borne du client est stricte.
  CHECK(tracks.front().alpha == 9 * 25);
  // ⚠ ET L'ANGLE TOURNE D'UN DEGRÉ PAR TIC.
  CHECK(tracks.front().degrees == static_cast<std::int16_t>(start_angle + 9) % 360);

  // Du dixième au quarantième, l'alpha ne bouge plus.
  for (std::int32_t tick = 0; tick < 31; ++tick) {
    AdvanceFadingStreakTracks(tracks, 0, 0);
  }
  CHECK(tracks.front().ticks == 40);
  CHECK(tracks.front().alpha == 9 * 25);

  // Puis il tombe de cinq par tic, jusqu'à zéro — sans que la piste s'éteigne.
  for (std::int32_t tick = 0; tick < 60; ++tick) {
    AdvanceFadingStreakTracks(tracks, 0, 0);
  }
  CHECK(tracks.front().alpha == 0);
  CHECK(tracks.front().active);
}

TEST_CASE("la traînée : le cap part de la source vers la cible", "[render][fading_streak]") {
  // ⚠ LE CLIENT MESURE L'ANGLE DE LA CIBLE VERS LA SOURCE, puis lui ajoute cent
  // soixante à deux cents degrés : le résultat pointe donc VERS la cible, à
  // vingt degrés près. Mesurer dans l'autre sens ferait partir les flocons à
  // l'opposé.
  std::uint32_t generator = 9U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFadingStreakTracks(kSource, kTarget, generator);
  for (const EffectTrack& track : tracks) {
    // La cible est plein est : le cap doit rester dans les vingt degrés de zéro.
    const std::int32_t cap = track.tilt_degrees;
    const bool vers_l_est = cap <= 20 || cap >= 340;
    INFO("cap " << cap);
    CHECK(vers_l_est);
  }
}

TEST_CASE("la table des traînées porte ses deux effets et leurs trois nœuds",
          "[render][fading_streak]") {
  const std::span<const EffectFadingStreakPattern> table = EffectFadingStreakPatternTable();
  REQUIRE(table.size() == 2);
  CHECK(EffectFadingStreakPatternsFor(kColdThrow).size() == 1);
  CHECK(EffectFadingStreakPatternsFor(1U).empty());

  const EffectFadingStreakPattern& pattern = table.front();
  REQUIRE(pattern.textures.size() == 4);
  CHECK(pattern.textures.back().ends_with("E9_snow03.tga"));
  // ⚠ TROIS NŒUDS PAR EFFET : le cas appelle son bâtisseur trois fois, et rien
  // d'autre que le répartiteur ne le dit.
  CHECK(pattern.node_count == 3);

  std::uint32_t generator = 13U;
  const ro::render::PooledEffectNode node =
      SowEffectFadingStreakNode(pattern, kSource, kTarget, 60, generator);
  CHECK(node.shape == PooledNodeShape::kFadingStreak);
  CHECK(node.resource_paths.size() == 4);
  REQUIRE(node.target_position.has_value());
  CHECK(node.target_position->x == Approx(kTarget.x));
  CHECK(node.tracks.front().mark[kMarkOfFadingStreakTint] < 4);
}
