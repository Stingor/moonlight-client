// LE RELEVÉ PAR LE CAS : ce qu'un cas de répartiteur écrit dans le nœud qu'il
// bâtit lui-même, et à qui ce nœud appartient.

#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"

using Catch::Approx;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeShape;

namespace {

// Le bit du mot de rendu que ces cas lèvent par-dessus celui du constructeur.
constexpr std::uint32_t kRenderWordRaisedBit = 0x200;

const EffectNodePattern* FirstOfShape(std::uint32_t effect_id, EffectNodeShape shape) {
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
    if (pattern.shape == shape) {
      return &pattern;
    }
  }
  return nullptr;
}

bool NamesTexture(const EffectNodePattern& pattern, std::string_view fragment) {
  for (std::string_view texture : ro::render::EffectNodePatternTextures(pattern)) {
    if (texture.find(fragment) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("un cas qui bâtit son sprite y pose ses drapeaux et son mot de rendu",
          "[render][per_case]") {
  // 165 garde les couleurs de ses calques ; 212 lève le bit du mot de rendu ;
  // 622 fait les deux.
  const EffectNodePattern* garde = FirstOfShape(165, EffectNodeShape::kSprite);
  REQUIRE(garde != nullptr);
  CHECK((garde->sprite_flags & ro::render::kEffectNodeKeepsLayerColor) != 0U);

  const EffectNodePattern* leve = FirstOfShape(212, EffectNodeShape::kSprite);
  REQUIRE(leve != nullptr);
  CHECK((leve->render_word & kRenderWordRaisedBit) != 0U);

  const EffectNodePattern* both = FirstOfShape(622, EffectNodeShape::kSprite);
  REQUIRE(both != nullptr);
  CHECK((both->sprite_flags & ro::render::kEffectNodeKeepsLayerColor) != 0U);
  CHECK((both->render_word & kRenderWordRaisedBit) != 0U);
}

TEST_CASE("le sprite du cas 601 rétrécit d'un dixième par tic", "[render][per_case]") {
  const EffectNodePattern* sprite = FirstOfShape(601, EffectNodeShape::kSprite);
  REQUIRE(sprite != nullptr);
  CHECK(sprite->sprite_scale == Approx(2.5F));
  CHECK(sprite->sprite_scale_step == Approx(-0.1F));
}

TEST_CASE("le nœud posé dans le corps du dernier cas n'appartient qu'à lui", "[render][per_case]") {
  // Le rune de Jack Frost se bâtit dans le cas de 1448, APRÈS l'adresse de ce
  // cas et avant celle du suivant : aucun autre effet du même répartiteur ne le
  // reçoit.
  const EffectNodePattern* rune = FirstOfShape(1448, EffectNodeShape::kOrientedQuad);
  REQUIRE(rune != nullptr);
  CHECK(NamesTexture(*rune, "jack_magic_rune"));
  CHECK(rune->life_from_effect);
  for (std::uint32_t neighbor : {601U, 622U, 1051U}) {
    CAPTURE(neighbor);
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(neighbor)) {
      CHECK_FALSE(NamesTexture(pattern, "jack_magic_rune"));
    }
  }
}
