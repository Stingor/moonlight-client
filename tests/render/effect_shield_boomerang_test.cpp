#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_shield_boomerang_seed.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::EffectShieldBoomerangPattern;
using ro::render::EffectShieldBoomerangPatternsFor;
using ro::render::EffectShieldBoomerangPatternTable;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectShieldBoomerangTracks;
using ro::render::SowEffectShieldBoomerangNode;

namespace {

constexpr std::size_t kSlotOfSpeed = 1;
constexpr std::size_t kSlotOfRise = 2;
constexpr std::size_t kMarkOfKind = 2;
constexpr std::uint32_t kShieldBoomerang = 249;
// EF_TANJI, Finger Offensive : le frère qui éclate.
constexpr std::uint32_t kBlueIvy = 265;
// Le paramètre de l'instance sur son chemin nu.
constexpr std::int32_t kRawParam = 1;

const EffectShieldBoomerangPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectShieldBoomerangPattern> own_entries =
      EffectShieldBoomerangPatternsFor(effect_id);
  REQUIRE(own_entries.size() == 1);
  return own_entries.front();
}

std::array<ro::render::EffectTrack, kEffectTracksPerNode> BoomerangTracks(glm::vec3 anchor,
                                                                          glm::vec3 target) {
  return SeedEffectShieldBoomerangTracks(PatternOf(kShieldBoomerang), anchor, target, kRawParam);
}

}  // namespace

TEST_CASE("Boumerang : quatre sites en table, deux bâtisseurs, et l'un n'est pas sous effect\\",
          "[render][boomerang]") {
  const std::span<const EffectShieldBoomerangPattern> table = EffectShieldBoomerangPatternTable();
  REQUIRE(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectShieldBoomerangPattern::effect_id));
  for (const EffectShieldBoomerangPattern& pattern : table) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.texture.starts_with("data\\texture\\"));
    CHECK((pattern.client_function == "Effect_Build_ShieldBoomerang" ||
           pattern.client_function == "Effect_Build_BlueIvyBeamWithSound"));
  }
  // ⚠ L'UNE DES TROIS EST UNE ICÔNE D'OBJET, prise sous l'interface et non sous
  // `effect\`. Un crible qui exigerait ce préfixe l'écarterait, et ce cas de
  // test dit que son absence est voulue plutôt que tolérée.
  CHECK(std::ranges::any_of(table, [](const EffectShieldBoomerangPattern& pattern) {
    return pattern.texture.find("\\effect\\") == std::string_view::npos;
  }));
}

TEST_CASE("Boumerang : sans cible, il ne sème RIEN", "[render][boomerang]") {
  // ⚠ C'EST LA RÈGLE DU CRIBLE PORTÉE JUSQU'AU SEMIS. Sans victime il n'y a ni
  // distance à parcourir ni altitude à rattraper : le semer donnerait un carré
  // immobile là où le client ne montre rien, et une division par zéro pour le
  // calculer.
  for (const std::uint32_t each_effect_id : {kShieldBoomerang, kBlueIvy}) {
    INFO("effet " << each_effect_id);
    CHECK_FALSE(SowEffectShieldBoomerangNode(PatternOf(each_effect_id), {0.0F, 0.0F, 0.0F},
                                             std::nullopt, kRawParam)
                    .has_value());
    CHECK(SowEffectShieldBoomerangNode(PatternOf(each_effect_id), {0.0F, 0.0F, 0.0F},
                                       glm::vec3{10.0F, 0.0F, 0.0F}, kRawParam)
              .has_value());
  }
}

TEST_CASE("Boumerang : il part du lanceur, HUIT au-dessus de ses pieds", "[render][boomerang]") {
  // Le client RETRANCHE huit dans un repère qui descend : la soustraction élève.
  const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      BoomerangTracks({3.0F, 2.0F, 5.0F}, {13.0F, 2.0F, 5.0F});
  CHECK(tracks.front().anchor.x == Catch::Approx(3.0F));
  CHECK(tracks.front().anchor.y == Catch::Approx(10.0F));
  CHECK(tracks.front().anchor.z == Catch::Approx(5.0F));
}

TEST_CASE("Boumerang : la clé est la distance AU SOL", "[render][boomerang]") {
  // ⚠ L'ALTITUDE N'Y ENTRE PAS : le client ne compte que l'abscisse et la cote.
  // L'y mettre allongerait le vol vers une cible en hauteur, et le bouclier
  // reviendrait trop tard.
  const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      BoomerangTracks({0.0F, 0.0F, 0.0F}, {3.0F, 100.0F, 4.0F});
  CHECK(tracks.front().lift_key == Catch::Approx(5.0F));
}

TEST_CASE("Boumerang : la montée le fait arriver SUR sa cible", "[render][boomerang]") {
  // Distance dix, vitesse deux : le trajet dure cinq tics. Pour rattraper vingt
  // unités d'altitude il faut donc quatre par tic.
  //
  // ⚠ ET LE SIGNE EST RETOURNÉ, parce que le mouvement l'est aussi : il
  // RETRANCHE cette valeur à l'aller. Une cible plus HAUTE demande donc une
  // valeur NÉGATIVE, faute de quoi le bouclier plongerait pour l'atteindre.
  const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      BoomerangTracks({0.0F, 0.0F, 0.0F}, {10.0F, 20.0F, 0.0F});
  CHECK(tracks.front().value[kSlotOfSpeed] == Catch::Approx(2.0F));
  CHECK(tracks.front().value[kSlotOfRise] == Catch::Approx(-4.0F));

  // Et cinq tics de vol rattrapent bien les vingt unités : la tête part à huit
  // au-dessus du lanceur, et le mouvement retranche la montée à chaque tic.
  const float after_five_ticks = tracks.front().anchor.y - 5.0F * tracks.front().value[kSlotOfRise];
  CHECK(after_five_ticks == Catch::Approx(8.0F + 20.0F));
}

TEST_CASE("Boumerang : les quatre pistes sont allumées, trois à alpha NUL", "[render][boomerang]") {
  // Elles ne reçoivent que par la cascade ; les éteindre les priverait de la
  // traînée, leur donner l'alpha de la tête montrerait quatre boucliers.
  const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      BoomerangTracks({0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F});
  for (const ro::render::EffectTrack& track : tracks) {
    CHECK(track.active);
  }
  CHECK(tracks[0].alpha == 255);
  CHECK(tracks[1].alpha == 0);
  CHECK(tracks[2].alpha == 0);
  CHECK(tracks[3].alpha == 0);
  // Le boumerang part aussitôt, à rayon fixe, et revient : genre zéro.
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(7.0F));
  CHECK(tracks[0].mark[kMarkOfKind] == 0);
}

TEST_CASE("Boumerang : le nœud porte SES DEUX BOUTS", "[render][boomerang]") {
  // C'est ce qui le distingue de toute la famille : il va d'un point à un autre.
  const EffectShieldBoomerangPattern& pattern = PatternOf(kShieldBoomerang);
  const std::optional<ro::render::PooledEffectNode> node = SowEffectShieldBoomerangNode(
      pattern, {1.0F, 0.0F, 2.0F}, glm::vec3{11.0F, 0.0F, 2.0F}, kRawParam);
  REQUIRE(node.has_value());
  CHECK(node->shape == ro::render::PooledNodeShape::kHomingTrail);
  CHECK(node->owner_position.x == Catch::Approx(1.0F));
  REQUIRE(node->target_position.has_value());
  CHECK(node->target_position->x == Catch::Approx(11.0F));
  REQUIRE(node->resource_paths.size() == 1);
  CHECK(node->resource_paths.front() == pattern.texture);
  // Le mot de rendu vaut cinq : le mélange de chaque primitive, et la
  // profondeur.
  CHECK(node->node.render_word == ro::render::kShieldBoomerangRenderWord);
}

TEST_CASE("Finger Offensive : l'image est écrite EN DUR par le bâtisseur", "[render][boomerang]") {
  // Son site ne passe rien ; le bâtisseur charge `effect\blue_ivy.bmp`.
  const EffectShieldBoomerangPattern& pattern = PatternOf(kBlueIvy);
  CHECK(pattern.client_function == "Effect_Build_BlueIvyBeamWithSound");
  CHECK(pattern.texture == "data\\texture\\effect\\blue_ivy.bmp");
}

TEST_CASE("Finger Offensive : la tête naît PÂLE, en RETARD, et du genre qui ÉCLATE",
          "[render][boomerang]") {
  // Alpha cinq, compteur à moins quinze, genre un.
  const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectShieldBoomerangTracks(PatternOf(kBlueIvy), {0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F},
                                      kRawParam);
  CHECK(tracks[0].alpha == 5);
  CHECK(tracks[0].ticks == -15);
  CHECK(tracks[0].mark[kMarkOfKind] == 1);
  // Le reste est celui du boumerang : vitesse deux, huit au-dessus du lanceur,
  // trois pistes allumées à alpha nul.
  CHECK(tracks[0].value[kSlotOfSpeed] == Catch::Approx(2.0F));
  CHECK(tracks[0].anchor.y == Catch::Approx(8.0F));
  for (std::size_t rang = 1; rang < kEffectTracksPerNode; ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
  }
}

TEST_CASE("Finger Offensive : le rayon suit le PARAMÈTRE de l'instance", "[render][boomerang]") {
  // Rayon = paramètre de l'instance × 0,4 + 2,0. Le paramètre est
  // le nombre de coups : une sphère au chemin nu, cinq au plus fort.
  const EffectShieldBoomerangPattern& pattern = PatternOf(kBlueIvy);
  const glm::vec3 caster{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{10.0F, 0.0F, 0.0F};
  constexpr std::int32_t kFiveHits = 5;
  CHECK(SeedEffectShieldBoomerangTracks(pattern, caster, aimed, kRawParam)[0].radius_at_anchor ==
        Catch::Approx(2.4F));
  CHECK(SeedEffectShieldBoomerangTracks(pattern, caster, aimed, kFiveHits)[0].radius_at_anchor ==
        Catch::Approx(4.0F));
  // ⚠ ET LE BOUMERANG NE LE SUIT PAS : son rayon est écrit tout net.
  CHECK(SeedEffectShieldBoomerangTracks(PatternOf(kShieldBoomerang), caster, aimed, kFiveHits)[0]
            .radius_at_anchor == Catch::Approx(7.0F));
}

TEST_CASE("Finger Offensive : la tête attend quinze tics, puis s'allume et part",
          "[render][boomerang]") {
  // Le mouvement ne touche pas une tête dont le compte n'est pas remonté à un ;
  // passé ce cap, celle du genre qui éclate gagne vingt-cinq d'alpha par tic.
  const glm::vec3 caster{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{100.0F, 0.0F, 0.0F};
  std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectShieldBoomerangTracks(PatternOf(kBlueIvy), caster, aimed, kRawParam);
  std::uint32_t seed = 1;
  constexpr std::int32_t kWaitTicks = 15;
  for (std::int32_t tick = 0; tick < kWaitTicks; ++tick) {
    ro::render::AdvanceHomingTrailTracks(tracks, caster, aimed, 0, 0, seed);
  }
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 5);
  CHECK(tracks[0].anchor.x == Catch::Approx(0.0F));

  ro::render::AdvanceHomingTrailTracks(tracks, caster, aimed, 0, 0, seed);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].alpha == 30);
  CHECK(tracks[0].anchor.x == Catch::Approx(2.0F));
}

TEST_CASE("Finger Offensive : le carré est BLEUTÉ, ajouté, et garde ses coins",
          "[render][boomerang]") {
  // ⚠ LE DESSINATEUR RELIT LE GENRE COMME SA MARQUE : la marque un,
  // (150, 150, 250), mélange additif (seizième paramètre à zéro), coins dans
  // l'ordre.
  const std::optional<ro::render::PooledEffectNode> node = SowEffectShieldBoomerangNode(
      PatternOf(kBlueIvy), {0.0F, 0.0F, 0.0F}, glm::vec3{10.0F, 0.0F, 0.0F}, kRawParam);
  REQUIRE(node.has_value());
  const ro::render::EffectTrack& head = node->tracks.front();
  const ro::render::EffectTintedQuadStyle style =
      ro::render::EffectTintedQuadStyleFor(head.mark[kMarkOfKind], 0);
  REQUIRE(style.draws);
  CHECK(style.color == std::array<std::uint8_t, 3>{150, 150, 250});
  CHECK_FALSE(style.ordinary_blend);
  CHECK_FALSE(style.swaps_corners);
  // Et il se voit dès sa naissance, à l'alpha cinq : le dessinateur ne demande
  // qu'une piste allumée d'alpha positif.
  CHECK(ro::render::ComposeTintedRotatedQuad(head, head.mark[kMarkOfKind], 0, 0, 0).has_value());
}
