// TROIS EFFETS QUE LE RÉPARTITEUR BÂTIT LUI-MÊME, sans bâtisseur à eux : l'eau
// bénite d'Aqua Benedicta (39), la fumée (44) et la bouffée de poison (53). Et la
// torche (47), qui est du même genre et dont la vie se lisait mal.
//
// Et deux autres cas en ligne : le venin qu'entretient une unité de Venom Dust
// (171), et la fumée du dragon (373), que bâtit le répartiteur des identifiants
// trois cents à trois cent soixante-treize.

#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_bearer_follow.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"

using Catch::Approx;
using ro::render::EffectNodePattern;

namespace {

constexpr std::uint32_t kAquaEffect = 39;
constexpr std::uint32_t kSmokeEffect = 44;
constexpr std::uint32_t kTorchEffect = 47;
constexpr std::uint32_t kPoisonHitEffect = 53;
constexpr std::uint32_t kVenomDust2Effect = 171;
constexpr std::uint32_t kDragonSmokeEffect = 373;

// Combien de tics un nœud semé avance avant qu'on lise son alpha.
constexpr int kAdvancedTicks = 3;
// L'écart toléré sur une composante que le cosinus d'un quart de tour rend
// presque nulle, en flottants.
constexpr float kAxisTolerance = 1.0e-5F;

const EffectNodePattern& OnlyPattern(std::uint32_t effect_id) {
  const std::span<const EffectNodePattern> patterns = ro::render::EffectNodePatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  REQUIRE(patterns.front().shape == ro::render::EffectNodeShape::kSprite);
  return patterns.front();
}

std::string_view SpriteOf(const EffectNodePattern& pattern) {
  const std::span<const std::string_view> couple = ro::render::EffectNodePatternTextures(pattern);
  REQUIRE(couple.size() == 1);
  return couple.front();
}

}  // namespace

TEST_CASE("les trois cas bâtis en ligne ont leur sprite", "[render][effect_inline]") {
  CHECK(SpriteOf(OnlyPattern(kAquaEffect)) == "data\\sprite\\이팩트\\성수뜨기");
  CHECK(SpriteOf(OnlyPattern(kSmokeEffect)) == "data\\sprite\\이팩트\\굴뚝연기");
  CHECK(SpriteOf(OnlyPattern(kPoisonHitEffect)) == "data\\sprite\\이팩트\\PoisonHit");
}

TEST_CASE("leur nœud vit la durée de l'effet", "[render][effect_inline]") {
  // 🔴 LE DÉFAUT DE DEUX CENTS ÉTAIT FAUX POUR LES QUATRE : chaque cas copie la
  // vie de l'instance dans son nœud, et cette vie est la durée que le client
  // déclare pour l'effet.
  for (const std::uint32_t each_effect_id :
       {kAquaEffect, kSmokeEffect, kTorchEffect, kPoisonHitEffect, kDragonSmokeEffect}) {
    INFO("effet " << each_effect_id);
    CHECK(OnlyPattern(each_effect_id).life_from_effect);
    CHECK(ro::render::EffectNodeLifeIsKnown(each_effect_id));
  }
  CHECK(ro::render::EffectNodeLifeFor(kAquaEffect) == 100);
  CHECK(ro::render::EffectNodeLifeFor(kSmokeEffect) == 50);
  CHECK(ro::render::EffectNodeLifeFor(kTorchEffect) == 250);
  CHECK(ro::render::EffectNodeLifeFor(kPoisonHitEffect) == 50);
  CHECK(ro::render::EffectNodeLifeFor(kDragonSmokeEffect) == 50);
}

TEST_CASE("l'eau bénite naît à vingt unités et meurt à sa dernière image",
          "[render][effect_inline]") {
  const EffectNodePattern& pattern = OnlyPattern(kAquaEffect);
  CHECK(pattern.launch_drop == Approx(-20.0F));
  CHECK(pattern.sprite_frame_hold_ticks == 2);
  CHECK(pattern.sprite_plays_once);
  // Sa teinte recouvre celle des calques : le cas ne pose pas le bit.
  CHECK((pattern.sprite_flags & ro::render::kEffectNodeKeepsLayerColor) == 0);
}

TEST_CASE("la bouffée de poison garde la couleur de ses calques", "[render][effect_inline]") {
  // 🔴 LE BIT QUE LE RELEVÉ NE VOIT PAS : ses bouffées sont à l'alpha 55 à 180,
  // et la teinte opaque du nœud les écraserait.
  const EffectNodePattern& pattern = OnlyPattern(kPoisonHitEffect);
  CHECK((pattern.sprite_flags & ro::render::kEffectNodeKeepsLayerColor) != 0);
  CHECK(pattern.sprite_scale == Approx(1.5F));
  CHECK(pattern.sprite_frame_hold_ticks == 2);
  CHECK(pattern.sprite_plays_once);
}

TEST_CASE("la fumée lâche une à quatre bouffées qui montent en tournant",
          "[render][effect_inline]") {
  const EffectNodePattern& pattern = OnlyPattern(kSmokeEffect);
  CHECK(pattern.copies == 1);
  CHECK(pattern.copies_high == 4);
  CHECK(pattern.sprite_spin_per_tick == Approx(0.75F));
  CHECK(pattern.launch_pitch_before_yaw);
  CHECK(pattern.launch_pitch_low == Approx(75.0F));
  CHECK(pattern.launch_pitch_high == Approx(104.0F));
  CHECK(pattern.launch_yaw_low == Approx(0.0F));
  CHECK(pattern.launch_yaw_high == Approx(359.0F));
  CHECK(pattern.step_low == Approx(0.3F));
  CHECK(pattern.step_high == Approx(0.5F));
  CHECK(pattern.launch_drop == Approx(-9.0F));
  CHECK(pattern.sprite_scale == Approx(1.5F));
  // LE FONDU COMMENCE À `vie − vie / 3` DE LA VRAIE VIE : trente-quatre sur
  // cinquante. Tiré de la vie par défaut, il tombait après la mort de la bouffée,
  // qui disparaissait alors d'un coup.
  CHECK(pattern.fade_out_tick == 34);
}

TEST_CASE("la fumée du dragon est une seule bouffée de la fumée", "[render][effect_inline]") {
  const EffectNodePattern& dragon = OnlyPattern(kDragonSmokeEffect);
  CHECK(SpriteOf(dragon) == SpriteOf(OnlyPattern(kSmokeEffect)));
  // UNE bouffée, au premier tic : le cas n'a ni boucle ni période.
  CHECK(dragon.copies == 1);
  CHECK(dragon.copies_high <= dragon.copies);
  CHECK(dragon.first_tick == 0);
  CHECK(dragon.last_tick == 0);
  CHECK(dragon.period_ticks == 0);
  // Tout le reste est la bouffée de la fumée : elle s'incline de soixante-quinze
  // à cent quatre degrés AVANT son cap, et monte donc quel que soit ce cap.
  CHECK(dragon.launch_pitch_before_yaw);
  CHECK(dragon.launch_pitch_low == Approx(75.0F));
  CHECK(dragon.launch_pitch_high == Approx(104.0F));
  CHECK(dragon.launch_yaw_low == Approx(0.0F));
  CHECK(dragon.launch_yaw_high == Approx(359.0F));
  CHECK(dragon.step_low == Approx(0.3F));
  CHECK(dragon.step_high == Approx(0.5F));
  CHECK(dragon.step_decay_share == Approx(0.0F));
  CHECK(dragon.launch_drop == Approx(-9.0F));
  CHECK(dragon.sprite_scale == Approx(1.5F));
  CHECK(dragon.sprite_spin_per_tick == Approx(0.75F));
}

TEST_CASE("la bouffée du dragon vit les cinquante tics de son effet et fond au trente-quatrième",
          "[render][effect_inline]") {
  const EffectNodePattern& dragon = OnlyPattern(kDragonSmokeEffect);
  CHECK(dragon.life_ticks == 50);
  CHECK(dragon.fade_out_tick == 34);
  // ET LA FIN DE L'EFFET LA BORNE : sans la vie de l'effet, ce qui reste de
  // l'instance à son semis serait sans limite.
  CHECK(ro::render::EffectNodePatternTicksLeft(dragon, 0) == 50);
}

TEST_CASE("le venin sème une bouffée tous les cinq tics tant que son unité tient sa case",
          "[render][effect_inline]") {
  const EffectNodePattern& venom = OnlyPattern(kVenomDust2Effect);
  CHECK(SpriteOf(venom) == "data\\sprite\\이팩트\\particle3");
  CHECK(venom.first_tick == 0);
  CHECK(venom.period_ticks == 5);
  CHECK(ro::render::EffectNodePatternSowsAt(venom, 0));
  CHECK(ro::render::EffectNodePatternSowsAt(venom, 5));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(venom, 7));
  // ⚠ LA BORNE EST LA DURÉE DÉCLARÉE, une sentinelle : ce qui arrête vraiment le
  // semis est le départ de l'unité, qui emporte l'horloge.
  CHECK(ro::render::EffectNodePatternLastSowTick(venom) == 9998);
  CHECK(ro::render::EffectNodePatternSowsAt(venom, 9995));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(venom, 10000));
  // Sa vie, elle, est posée : dix tics, dont deux de fondu.
  CHECK_FALSE(venom.life_from_effect);
  CHECK(venom.life_ticks == 10);
  CHECK(venom.fade_out_tick == 8);
  CHECK(venom.sprite_scale == Approx(0.8F));
}

TEST_CASE("la bouffée du venin monte à la verticale en freinant, quelle que soit la face",
          "[render][effect_inline]") {
  const EffectNodePattern& venom = OnlyPattern(kVenomDust2Effect);
  CHECK(venom.step_low == Approx(0.25F));
  CHECK(venom.step_high == Approx(0.4875F));
  CHECK(venom.step_decay_share == Approx(0.5F));
  const ro::render::EffectNodeSeed style_seed =
      ro::render::EffectNodeSeedFromPattern(venom, venom.life_ticks, 0, 0);
  // 🔴 LE CLIENT N'Y MET AUCUN CAP, pas même la face du porteur : sa matrice est
  // le seul quart de tour autour de l'abscisse.
  for (const float face : {0.0F, 90.0F, 180.0F, 270.0F}) {
    INFO("face " << face);
    std::uint32_t generator = 1;
    const ro::render::EffectLaunchDraw lance = ro::render::DrawEffectLaunch(
        style_seed, face, style_seed.life_ticks, venom.step_low, generator);
    CHECK(lance.direction.y == Approx(1.0F));
    CHECK(lance.direction.x == Approx(0.0F).margin(kAxisTolerance));
    CHECK(lance.direction.z == Approx(0.0F).margin(kAxisTolerance));
    // Le pas perd la moitié de lui-même sur la vie du nœud.
    CHECK(lance.step_decay ==
          Approx(-(venom.step_low / static_cast<float>(venom.life_ticks)) * 0.5F));
  }
}

TEST_CASE("l'alpha du venin part de soixante-dix et monte d'un dixième du plafond par tic",
          "[render][effect_inline]") {
  const EffectNodePattern& venom = OnlyPattern(kVenomDust2Effect);
  CHECK(venom.alpha_start == Approx(70.0F));
  CHECK(venom.alpha_max == Approx(254.0F));
  CHECK(venom.alpha_step == Approx(25.4F));
  std::uint32_t generator = 1;
  ro::render::EffectNode effect_node = ro::render::SowEffectNode(
      ro::render::EffectNodeSeedFromPattern(venom, venom.life_ticks, 0, 0), generator);
  for (int tick = 0; tick < kAdvancedTicks; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(effect_node));
  }
  CHECK(effect_node.alpha == Approx(70.0F + static_cast<float>(kAdvancedTicks) * 25.4F));
}

TEST_CASE("la fumée du dragon suit son porteur à chaque tic, le venin reste où il est né",
          "[render][effect_inline]") {
  // Le cas du dragon recopie la place de son porteur à chaque appel, avant sa
  // garde d'âge ; celui du venin ne la recopie jamais.
  const ro::render::EffectBearerFollow dragon =
      ro::render::EffectBearerFollowOf(kDragonSmokeEffect);
  const std::int32_t last_tick = OnlyPattern(kDragonSmokeEffect).life_ticks - 1;
  CHECK(ro::render::EffectFollowsBearerStepByStep(dragon, 0));
  CHECK(ro::render::EffectFollowsBearerStepByStep(dragon, last_tick));
  CHECK(ro::render::EffectBearerFollowFor(kVenomDust2Effect) == nullptr);
}

TEST_CASE("l'inclinaison composée avant le cap fait monter la particule quel que soit le cap",
          "[render][effect_inline]") {
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = 10;
  seed.step_length = ro::render::EffectNodeRange{1.0F, 1.0F};
  seed.launch_pitch_degrees = ro::render::EffectNodeRange{90.0F, 90.0F};
  seed.launch_pitch_before_yaw = true;
  for (const float cap : {0.0F, 90.0F, 180.0F, 270.0F}) {
    INFO("cap " << cap);
    seed.launch_yaw_degrees = ro::render::EffectNodeRange{cap, cap};
    std::uint32_t generator = 1;
    const ro::render::EffectLaunchDraw lance =
        ro::render::DrawEffectLaunch(seed, 0.0F, seed.life_ticks, 1.0F, generator);
    CHECK(lance.direction.y == Approx(1.0F));
  }
  // ⚠ ET L'ORDRE ORDINAIRE, lui, retourne la particule à cent quatre-vingts
  // degrés : c'est ce qui rend le drapeau nécessaire, et non un détail.
  seed.launch_pitch_before_yaw = false;
  seed.launch_yaw_degrees = ro::render::EffectNodeRange{180.0F, 180.0F};
  std::uint32_t generator = 1;
  const ro::render::EffectLaunchDraw retourne =
      ro::render::DrawEffectLaunch(seed, 0.0F, seed.life_ticks, 1.0F, generator);
  CHECK(retourne.direction.y == Approx(-1.0F));
}

TEST_CASE("un sprite semé avec une rotation propre tourne à chaque tic",
          "[render][effect_inline]") {
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = 10;
  seed.spin_speed_per_tick = 0.75F;
  std::uint32_t generator = 1;
  ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
  CHECK(node.draws_rotated);
  const float start = node.spin_degrees;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.spin_degrees - start == Approx(1.5F));
}
