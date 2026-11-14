// LE MOT DE RENDU : ce que la file de rendu du client en fait, et ce que la
// table des patrons en porte.
#include "ro/render/effect_render_word.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_cross_spikes.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/map_effects.h"
#include "ro/render/str_effect_setup.h"

using ro::render::EffectLotIsDepthTested;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectRenderBucketOf;
using ro::render::EffectSpriteIsAdditive;
using ro::render::EffectWorldQuadBlend;

namespace {

constexpr std::int32_t kUn = 2;
constexpr std::int32_t kAlphaSource = 5;
constexpr std::int32_t kComplementAlpha = 6;

[[nodiscard]] bool AnyPatternCarries(std::uint32_t effect_id, std::uint32_t word) {
  for (const EffectNodePattern& node_pattern : EffectNodePatternsFor(effect_id)) {
    if (node_pattern.render_word == word) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("le mot du constructeur mélange à l'ordinaire et passe l'épreuve", "[render][word]") {
  // Un : le seau 396/420, vidé sous SRCBLEND 5 et DESTBLEND 6, ZENABLE posé.
  const auto seau = EffectRenderBucketOf(ro::render::kEffectRenderWordOfConstructor);
  CHECK(seau.depth_tested);
  REQUIRE(seau.imposed_blend.has_value());
  CHECK(seau.imposed_blend->source == kAlphaSource);
  CHECK(seau.imposed_blend->destination == kComplementAlpha);
  // ⚠ LE MÉLANGE DU QUAD NE COMPTE PLUS : le seau impose le sien.
  const auto blend = EffectWorldQuadBlend(kAlphaSource, kUn, 1U);
  CHECK(blend.destination == kComplementAlpha);
}

TEST_CASE("0x200 peint par-dessus tout, sans épreuve de profondeur", "[render][word]") {
  CHECK_FALSE(EffectLotIsDepthTested(0x201U));
  CHECK_FALSE(EffectLotIsDepthTested(0x203U));
  CHECK(EffectLotIsDepthTested(1U));
  CHECK(EffectLotIsDepthTested(5U));
  // Un lot sans mot garde l'épreuve : sa famille ne le porte pas encore.
  CHECK(EffectLotIsDepthTested(std::nullopt));
}

TEST_CASE("le bit deux impose l'addition, le bit quatre rend son mélange au quad",
          "[render][word]") {
  const auto additif = EffectRenderBucketOf(0x203U);
  REQUIRE(additif.imposed_blend.has_value());
  CHECK(additif.imposed_blend->source == kUn);
  CHECK(additif.imposed_blend->destination == kUn);
  // Cinq : le seau des primitives à mélange propre — celui du traceur choisi par
  // son seizième paramètre.
  CHECK_FALSE(EffectRenderBucketOf(5U).imposed_blend.has_value());
  const auto own = EffectWorldQuadBlend(kAlphaSource, kUn, 5U);
  CHECK(own.destination == kUn);
  // Et sans mot, le quad garde le sien.
  CHECK(EffectWorldQuadBlend(kAlphaSource, kUn, std::nullopt).destination == kUn);
}

TEST_CASE("un sprite ne s'ajoute que pour le mot cinq", "[render][word]") {
  // Le client pose la destination à `4 · (mot ≠ 5) + 2`.
  CHECK(EffectSpriteIsAdditive(5U));
  CHECK_FALSE(EffectSpriteIsAdditive(13U));
  CHECK_FALSE(EffectSpriteIsAdditive(1U));
  CHECK_FALSE(EffectSpriteIsAdditive(0x201U));
  CHECK(EffectSpriteIsAdditive(0x203U));
}

TEST_CASE("la table porte le mot que chaque bâtisseur écrit", "[render][word][table]") {
  // La couronne de Blessing lève 0x200 sur le un du constructeur ; le tronc de
  // la fabrique aussi ; l'effet 94 lève 2 puis 0x200 ; le type 61 de la
  // fabrique réécrit cinq.
  CHECK(AnyPatternCarries(42, 0x201U));
  CHECK(AnyPatternCarries(570, 0x201U));
  CHECK(AnyPatternCarries(94, 0x203U));
  CHECK(AnyPatternCarries(912, 5U));
}

TEST_CASE("le mot du patron va jusqu'au nœud semé", "[render][word]") {
  const std::span<const EffectNodePattern> node_patterns = EffectNodePatternsFor(570);
  REQUIRE_FALSE(node_patterns.empty());
  const ro::render::EffectNodeSeed seed =
      ro::render::EffectNodeSeedFromPattern(node_patterns.front(), 100, 0, 0);
  REQUIRE(seed.render_word.has_value());
  CHECK(*seed.render_word == node_patterns.front().render_word);
  std::uint32_t generator = 1U;
  const ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
  REQUIRE(node.render_word.has_value());
  CHECK(*node.render_word == node_patterns.front().render_word);
}

TEST_CASE("le brouillard prend les seaux vidés avant sa coupure", "[render][word][fog]") {
  using ro::render::EffectRenderWordIsFogged;
  // Les listes opaques du sol et du décor, répétées ou non.
  CHECK(EffectRenderWordIsFogged(0x0U));
  CHECK(EffectRenderWordIsFogged(0x20U));
  // Les seaux triés ordinaires, découpés, triés tôt, ou levés sans épreuve mais
  // gardés tôt.
  CHECK(EffectRenderWordIsFogged(0x1U));
  CHECK(EffectRenderWordIsFogged(0x21U));
  CHECK(EffectRenderWordIsFogged(0x11U));
  CHECK(EffectRenderWordIsFogged(0x101U));
  CHECK(EffectRenderWordIsFogged(0x103U));
  CHECK(EffectRenderWordIsFogged(0x209U));
  // Ce que la file peint brouillard coupé : tout mélange propre, l'addition
  // ordinaire, et ce qui passe par-dessus tout.
  CHECK_FALSE(EffectRenderWordIsFogged(0x5U));
  CHECK_FALSE(EffectRenderWordIsFogged(0xDU));
  CHECK_FALSE(EffectRenderWordIsFogged(0x205U));
  CHECK_FALSE(EffectRenderWordIsFogged(0x3U));
  CHECK_FALSE(EffectRenderWordIsFogged(0x203U));
  CHECK_FALSE(EffectRenderWordIsFogged(0x201U));
}

TEST_CASE("un lot sans mot ne s'embrume pas", "[render][word][fog]") {
  CHECK_FALSE(ro::render::EffectLotIsFogged(std::nullopt));
  CHECK(ro::render::EffectLotIsFogged(ro::render::kEffectRenderWordOfConstructor));
}

TEST_CASE("des effets de carte, seule la fumée des cheminées s'embrume", "[render][word][fog]") {
  CHECK_FALSE(ro::render::EffectRenderWordIsFogged(ro::render::kMapParticleRenderWord));
  CHECK(ro::render::EffectRenderWordIsFogged(ro::render::kMapSmokeRenderWord));
}

TEST_CASE("le seau des fichiers .str se vide après les seaux triés, avant ce qui passe dessus",
          "[render][word][order]") {
  using ro::render::EffectRenderWordPaintsBeforeStr;
  // Le fichier lui-même ne se peint pas sous lui-même, ni un sprite au mot cinq.
  CHECK_FALSE(EffectRenderWordPaintsBeforeStr(ro::render::kStrRenderWordOfCreation));
  // Les listes opaques, vidées les premières.
  CHECK(EffectRenderWordPaintsBeforeStr(0x0U));
  CHECK(EffectRenderWordPaintsBeforeStr(0x14U));
  // Les seaux triés : le mot du constructeur, l'addition, le découpé, le trié tôt.
  CHECK(EffectRenderWordPaintsBeforeStr(ro::render::kEffectRenderWordOfConstructor));
  CHECK(EffectRenderWordPaintsBeforeStr(0x3U));
  CHECK(EffectRenderWordPaintsBeforeStr(0x103U));
  CHECK(EffectRenderWordPaintsBeforeStr(0x11U));
  CHECK(EffectRenderWordPaintsBeforeStr(0x101U));
  // Le mélange propre tôt, et le « par-dessus tout » gardé sous le brouillard.
  CHECK(EffectRenderWordPaintsBeforeStr(0xDU));
  CHECK(EffectRenderWordPaintsBeforeStr(0x209U));
  // Ce qui se vide APRÈS les fichiers : sans épreuve de profondeur.
  CHECK_FALSE(EffectRenderWordPaintsBeforeStr(0x201U));
  CHECK_FALSE(EffectRenderWordPaintsBeforeStr(0x203U));
  CHECK_FALSE(EffectRenderWordPaintsBeforeStr(0x205U));
}

TEST_CASE("un lot sans mot garde sa place après les fichiers", "[render][word][order]") {
  CHECK_FALSE(ro::render::EffectLotPaintsBeforeStr(std::nullopt));
  CHECK(ro::render::EffectLotPaintsBeforeStr(ro::render::kEffectRenderWordOfConstructor));
}

TEST_CASE("la flèche et l'anneau de Fire Bolt se peignent sous l'éclat de feu",
          "[render][word][order]") {
  // Fire Bolt pose ses nœuds (effet 31) puis, à chaque frappe, l'éclat de feu
  // en fichier (effet 49). Le client montre l'éclat PAR-DESSUS la flèche.
  constexpr std::uint32_t kFireArrowEffect = 31;
  constexpr std::uint32_t kFireHitEffect = 49;
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(kFireArrowEffect);
  REQUIRE_FALSE(patterns.empty());
  for (const EffectNodePattern& pattern : patterns) {
    INFO("mot " << pattern.render_word);
    CHECK(ro::render::EffectRenderWordPaintsBeforeStr(pattern.render_word));
  }
  const ro::render::StrEffectSetup* fire_hit = ro::render::StrEffectSetupFor(kFireHitEffect);
  const std::uint32_t fire_hit_word =
      fire_hit != nullptr ? fire_hit->render_word : ro::render::kStrRenderWordOfCreation;
  CHECK_FALSE(ro::render::EffectRenderWordPaintsBeforeStr(fire_hit_word));
}

TEST_CASE("deux seaux se vident avant les corps", "[render][word][order]") {
  using ro::render::EffectRenderWordPaintsBeforeActors;
  // La file du sol, au mélange propre : l'ombre du serpent, la pénitence.
  CHECK(EffectRenderWordPaintsBeforeActors(0xDU));
  // Sans épreuve de profondeur mais gardé sous le brouillard : le quadrillage
  // de Digital Space, que le personnage recouvre.
  CHECK(EffectRenderWordPaintsBeforeActors(0x209U));
  // Par-dessus tout, sans 0x8 : après les corps.
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x201U));
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x200U));
  // L'addition et le mélange propre sans 0x8 ne passent jamais avant.
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x20BU));
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x205U));
  // Les seaux triés des corps et des fichiers.
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x101U));
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x5U));
  CHECK_FALSE(EffectRenderWordPaintsBeforeActors(0x1U));
}

TEST_CASE("le rang du seau suit la file, fichiers compris", "[render][word][order]") {
  using ro::render::EffectBucketRank;
  using ro::render::EffectRenderWordBucketRank;
  // Sous les fichiers, exactement ce que la file vide avant eux.
  constexpr std::uint32_t kEveryWordBit = 0x400;
  for (std::uint32_t word = 0; word < kEveryWordBit; ++word) {
    CHECK((EffectRenderWordBucketRank(word) < EffectBucketRank::kStr) ==
          ro::render::EffectRenderWordPaintsBeforeStr(word));
  }
  // Le mot du constructeur et son voisin trié, puis l'addition.
  CHECK(EffectRenderWordBucketRank(0x1U) == EffectBucketRank::kSorted);
  CHECK(EffectRenderWordBucketRank(0x21U) == EffectBucketRank::kSortedNeighbour);
  CHECK(EffectRenderWordBucketRank(0x3U) == EffectBucketRank::kAdditive);
  CHECK(EffectRenderWordBucketRank(0x5U) == EffectBucketRank::kStr);
  CHECK(ro::render::EffectLotBucketRank(std::nullopt) == EffectBucketRank::kStr);
}

TEST_CASE("les pointes de deux effets voisins s'entremêlent par la profondeur",
          "[render][word][order]") {
  // La grille de Heaven's Drive pose une pointe derrière la glace et une
  // devant ; la pointe de glace est entre les deux. Peints lot par lot, à la
  // profondeur de leur point d'appui, les deux pointes de la grille passaient
  // ensemble, toutes deux dessous ou toutes deux dessus.
  using ro::render::EffectBucketRank;
  using ro::render::EffectPaintedQuad;
  using ro::render::EffectWorldQuad;
  const auto quad_at = [](float depth) {
    EffectWorldQuad quad;
    quad.corners.fill(glm::vec3{0.0F, 0.0F, depth});
    return quad;
  };
  const EffectWorldQuad grid_far = quad_at(-30.0F);
  const EffectWorldQuad grid_near = quad_at(30.0F);
  const EffectWorldQuad ice = quad_at(0.0F);
  const EffectWorldQuad glow_far = quad_at(-50.0F);
  constexpr std::size_t kGridLot = 0;
  constexpr std::size_t kIceLot = 1;
  constexpr std::size_t kGlowLot = 2;
  std::vector<EffectPaintedQuad> painted = {
      {.quad = &grid_far, .lot = kGridLot, .bucket = EffectBucketRank::kSorted},
      {.quad = &grid_near, .lot = kGridLot, .bucket = EffectBucketRank::kSorted},
      {.quad = &glow_far, .lot = kGlowLot, .bucket = EffectBucketRank::kAdditive},
      {.quad = &ice, .lot = kIceLot, .bucket = EffectBucketRank::kSorted},
  };
  ro::render::OrderEffectPaintedQuads(painted, glm::vec3{0.0F, 0.0F, 100.0F},
                                      glm::vec3{0.0F, 0.0F, -1.0F});
  REQUIRE(painted.size() == 4);
  CHECK(painted[0].quad == &grid_far);
  CHECK(painted[1].quad == &ice);
  CHECK(painted[2].quad == &grid_near);
  // L'addition se vide APRÈS le seau trié, même la plus lointaine.
  CHECK(painted[3].quad == &glow_far);
}

TEST_CASE("la haute pointe d'Ice Wall passe sous les pointes de pierre plantées devant elle",
          "[render][word][order]") {
  // Heaven's Drive (142) plante ses pointes de pierre autour d'un éclat d'Ice
  // Wall (74). Les deux figures sont la même croix, au même mot : elles tombent
  // dans le même seau trié, et la file range chaque triangle par son PREMIER
  // SOMMET, un coin de sa base au sol. Rangée par le centre de ses coins,
  // l'éclat — trois fois plus haut — passait devant toutes les pointes, même
  // celles plantées entre lui et l'œil : son sommet est plus près de la caméra.
  using ro::render::EffectBucketRank;
  using ro::render::EffectCrossSpikeRequest;
  using ro::render::EffectPaintedQuad;
  using ro::render::EffectRenderWordBucketRank;
  using ro::render::EffectWorldQuad;
  constexpr std::uint32_t kIceWall = 74;
  constexpr std::uint32_t kHeavensDrive = 142;
  const std::span<const EffectNodePattern> ice_patterns = EffectNodePatternsFor(kIceWall);
  const std::span<const EffectNodePattern> stone_patterns = EffectNodePatternsFor(kHeavensDrive);
  REQUIRE(ice_patterns.size() == 1);
  REQUIRE(stone_patterns.size() == 1);
  const EffectBucketRank bucket = EffectRenderWordBucketRank(ice_patterns[0].render_word);
  REQUIRE(bucket == EffectRenderWordBucketRank(stone_patterns[0].render_word));
  REQUIRE(bucket == EffectBucketRank::kSorted);

  // Les tailles relevées : demi-côté 2,5 et sommet 30 à 39,9 pour la glace,
  // demi-côté 3 à 3,49 et une dizaine de hauteur pour la pierre.
  constexpr float kIceApex = 35.0F;
  constexpr float kStoneHalfSide = 3.2F;
  constexpr float kStoneApex = 12.0F;
  // La pierre s'écarte de la glace de deux pointes, vers l'œil ou à l'opposé.
  constexpr float kStoneOffset = 10.0F;
  const EffectCrossSpikeRequest ice_request{.half_side = ice_patterns[0].sprite_scale,
                                            .apex_height = kIceApex};
  const EffectCrossSpikeRequest stone_request{.half_side = kStoneHalfSide,
                                              .apex_height = kStoneApex};
  const ro::render::Camera camera;
  const glm::vec3 eye = camera.Position();
  const glm::vec3 toward_eye =
      glm::normalize(glm::vec3{eye.x - camera.look_at.x, 0.0F, eye.z - camera.look_at.z});
  const std::vector<EffectWorldQuad> ice =
      ro::render::ComposeCrossSpikeQuads(ice_request, camera.look_at);
  const std::vector<EffectWorldQuad> stone_front =
      ro::render::ComposeCrossSpikeQuads(stone_request, camera.look_at + toward_eye * kStoneOffset);
  const std::vector<EffectWorldQuad> stone_behind =
      ro::render::ComposeCrossSpikeQuads(stone_request, camera.look_at - toward_eye * kStoneOffset);

  // Les lots entrent dans l'ordre de leur point d'appui, la glace en dernier :
  // c'est l'ordre qu'elle gardait quand elle se peignait d'un bloc.
  constexpr std::size_t kStoneLot = 0;
  constexpr std::size_t kIceLot = 1;
  std::vector<EffectPaintedQuad> painted;
  for (const auto* stones : {&stone_behind, &stone_front}) {
    for (const EffectWorldQuad& quad : *stones) {
      painted.push_back({.quad = &quad, .lot = kStoneLot, .bucket = bucket});
    }
  }
  for (const EffectWorldQuad& quad : ice) {
    painted.push_back({.quad = &quad, .lot = kIceLot, .bucket = bucket});
  }
  const glm::mat4 view = camera.ViewMatrix();
  ro::render::OrderEffectPaintedQuads(painted, eye, -glm::vec3(view[0][2], view[1][2], view[2][2]));

  const auto is_in = [](const std::vector<EffectWorldQuad>& figure, const EffectWorldQuad* quad) {
    return quad >= figure.data() && quad < figure.data() + figure.size();
  };
  REQUIRE(painted.size() == ice.size() + stone_front.size() + stone_behind.size());
  // Derrière d'abord, puis la glace, puis ce qui la cache.
  for (std::size_t rank = 0; rank < painted.size(); ++rank) {
    const EffectWorldQuad* quad = painted[rank].quad;
    if (rank < stone_behind.size()) {
      CHECK(is_in(stone_behind, quad));
    } else if (rank < stone_behind.size() + ice.size()) {
      CHECK(is_in(ice, quad));
    } else {
      CHECK(is_in(stone_front, quad));
    }
  }
}

TEST_CASE("Réglages de .str : la réticule de Bull's Eye se relève moins que les autres",
          "[render][effects]") {
  // Un écart voulu : voir `StrAnchorLiftFor`.
  CHECK(ro::render::StrAnchorLiftFor(ro::render::kBullseyeReticleEffect) ==
        ro::render::kBullseyeReticleLiftPixels);
  // Les autres gardent le relèvement de tous.
  CHECK_FALSE(ro::render::StrAnchorLiftFor(648).has_value());
}
