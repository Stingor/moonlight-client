// LES EFFETS D'UNE SEULE PARTICULE EN SPRITE : la table, la hauteur selon le
// métier du porteur, et ce que l'instance montre tic après tic.

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_class.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/sprite_effect_table.h"

using Catch::Approx;

namespace {

// Les identifiants que le client confie à la classe d'un seul sprite.
constexpr std::array<std::uint32_t, 37> kClassIds = {
    990,  1130, 1147, 1151, 1152, 1153, 1154, 1155, 1156, 1157, 1158, 1159, 1160,
    1192, 1211, 1212, 1213, 1231, 1232, 1233, 1234, 1235, 1236, 1237, 1242, 1377,
    1419, 1617, 1618, 1619, 2394, 2420, 2424, 2428, 2429, 2430, 2431};
// Le décor de Digital Space : un cas du répartiteur, de même figure.
constexpr std::uint32_t kDigitalSpace = 1240;

constexpr std::uint32_t kValkyrieWing = 1377;
constexpr std::uint32_t kSerpentShadow = 2394;
constexpr std::uint32_t kPenance = 2428;
constexpr std::uint32_t kPermafrost = 2429;
constexpr std::uint32_t kSubjectAuraThird = 1211;
constexpr std::uint32_t kSoulShadow = 1235;
constexpr std::uint32_t kLargeRing = 990;
constexpr std::uint32_t kAmdaraisCostume = 1419;

// Un chevalier sur son Peco, un bébé, un Doram, un novice.
constexpr std::uint32_t kPecoKnight = 13;
constexpr std::uint32_t kBabyPecoKnight = 4036;
constexpr std::uint32_t kBabyNovice = 4023;
constexpr std::uint32_t kDoramSummoner = 4218;
constexpr std::uint32_t kNovice = 0;

// Tout sprite a quatre images.
constexpr std::int32_t kFrames = 4;

struct Ticking {
  ro::render::EffectClassView view;
  ro::render::EffectClassTick tick;
  Ticking() {
    tick.view = &view;
    tick.owner_position = glm::vec3{10.0F, -3.0F, 20.0F};
    tick.sprite_frame_count = [](std::string_view, std::int32_t) { return kFrames; };
  }
};

std::unique_ptr<ro::render::EffectClassInstance> Spawn(std::uint32_t effect_id) {
  ro::render::EffectClassBirth birth;
  birth.effect_id = effect_id;
  birth.position = {10.0F, -3.0F, 20.0F};
  auto instance = ro::render::SpawnEffectClass(birth, ro::render::EffectClassView{});
  REQUIRE(instance != nullptr);
  return instance;
}

ro::render::EffectClassOutput AdvanceAndDraw(ro::render::EffectClassInstance& instance,
                                             Ticking& ticking, std::int32_t ticks = 1) {
  ro::render::EffectClassOutput events;
  for (std::int32_t tick_index = 0; tick_index < ticks; ++tick_index) {
    instance.Advance(ticking.tick, events);
  }
  ro::render::EffectClassOutput drawn;
  instance.Draw(ticking.view, drawn);
  return drawn;
}

}  // namespace

TEST_CASE("Sprite d'effet : la table est triée et couvre toute la classe",
          "[render][sprite_effect]") {
  const auto recipes = ro::render::SpriteEffectRecipes();
  CHECK(std::ranges::is_sorted(recipes, {}, &ro::render::SpriteEffectRecipe::effect_id));
  CHECK(recipes.size() == kClassIds.size() + 1);
  for (const std::uint32_t id : kClassIds) {
    const ro::render::SpriteEffectRecipe* recipe = ro::render::SpriteEffectRecipeFor(id);
    REQUIRE(recipe != nullptr);
    CHECK_FALSE(recipe->sprite.empty());
    CHECK(recipe->sprite.find(".spr") == std::string_view::npos);
    CHECK(ro::render::IsPortedEffectClass(id));
  }
  CHECK(ro::render::IsPortedEffectClass(kDigitalSpace));
  // Les auras qu'aucune classe ni aucun cas ne dessine restent sans effet.
  CHECK(ro::render::SpriteEffectRecipeFor(2301) == nullptr);
  CHECK_FALSE(ro::render::IsPortedEffectClass(2301));
}

TEST_CASE("Sprite d'effet : le métier relève ce qui se pose sur le porteur",
          "[render][sprite_effect]") {
  CHECK(ro::render::JobEffectLift(kNovice) == 0.0F);
  CHECK(ro::render::JobEffectLift(kPecoKnight) == 6.0F);
  CHECK(ro::render::JobEffectLift(kBabyPecoKnight) == 4.0F);
  CHECK(ro::render::JobEffectLift(kBabyNovice) == -1.0F);
  CHECK(ro::render::JobEffectLift(kDoramSummoner) == -2.0F);

  const ro::render::SpriteEffectRecipe* wing = ro::render::SpriteEffectRecipeFor(kValkyrieWing);
  REQUIRE(wing != nullptr);
  CHECK(ro::render::SpriteEffectLiftUnits(*wing, std::nullopt) == Approx(3.0F));
  CHECK(ro::render::SpriteEffectLiftUnits(*wing, kPecoKnight) == Approx(9.0F));
  // La pénitence a sa hauteur à elle pour un Doram, relevée du métier.
  const ro::render::SpriteEffectRecipe* penance = ro::render::SpriteEffectRecipeFor(kPenance);
  REQUIRE(penance != nullptr);
  CHECK(ro::render::SpriteEffectLiftUnits(*penance, kNovice) == Approx(17.0F));
  CHECK(ro::render::SpriteEffectLiftUnits(*penance, kDoramSummoner) == Approx(11.7F));
  // Le givre, lui, ne prend pas le relèvement du métier.
  const ro::render::SpriteEffectRecipe* frost = ro::render::SpriteEffectRecipeFor(kPermafrost);
  REQUIRE(frost != nullptr);
  CHECK(ro::render::SpriteEffectLiftUnits(*frost, kPecoKnight) == Approx(0.4F));
  CHECK(ro::render::SpriteEffectLiftUnits(*frost, kDoramSummoner) == Approx(2.2F));
}

TEST_CASE("Sprite d'effet : les ailes de Valkyrie bouclent sur leur porteur, devant lui",
          "[render][sprite_effect]") {
  Ticking ticking;
  ticking.tick.owner_job = kPecoKnight;
  auto wing = Spawn(kValkyrieWing);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*wing, ticking);
  REQUIRE(drawn.sprites.size() == 1);
  const ro::render::EffectClassSpriteDraw& sprite = drawn.sprites.front();
  CHECK(sprite.sprite == "이팩트\\valkyrie_wing\\valkyrie_wing");
  CHECK(sprite.keeps_layer_color);
  // Trois unités au-dessus des pieds, six de plus pour un corps monté.
  CHECK(sprite.ground_position.y == Approx(3.0F + 9.0F));
  CHECK(sprite.depth_bias == Approx(0.001F));
  REQUIRE(sprite.render_word.has_value());
  CHECK_FALSE(ro::render::EffectSpriteIsAdditive(*sprite.render_word));
  // Six tics par image, et la boucle : après ses quatre images, la première.
  CHECK(AdvanceAndDraw(*wing, ticking, 5).sprites.front().frame == 1);
  CHECK(AdvanceAndDraw(*wing, ticking, 18).sprites.front().frame == 0);
  CHECK(wing->Alive(ticking.tick));
  ticking.tick.owner_present = false;
  CHECK_FALSE(wing->Alive(ticking.tick));
}

TEST_CASE("Sprite d'effet : l'ombre du serpent passe sous le corps", "[render][sprite_effect]") {
  Ticking ticking;
  auto shadow = Spawn(kSerpentShadow);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*shadow, ticking);
  REQUIRE(drawn.sprites.size() == 1);
  REQUIRE(drawn.sprites.front().render_word.has_value());
  CHECK(ro::render::EffectRenderWordPaintsBeforeActors(*drawn.sprites.front().render_word));
  CHECK(drawn.sprites.front().ground_position.y == Approx(3.0F));
  // Les ailes, elles, se peignent après les corps.
  auto wing = Spawn(kValkyrieWing);
  const ro::render::EffectClassOutput wing_drawn = AdvanceAndDraw(*wing, ticking);
  REQUIRE(wing_drawn.sprites.front().render_word.has_value());
  CHECK_FALSE(
      ro::render::EffectRenderWordPaintsBeforeActors(*wing_drawn.sprites.front().render_word));
}

TEST_CASE("Sprite d'effet : l'aura d'un sujet choisit son action, et rapetisse avec le corps",
          "[render][sprite_effect]") {
  Ticking ticking;
  ticking.tick.owner_shrink_scale = 0.5F;
  auto aura = Spawn(kSubjectAuraThird);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*aura, ticking);
  REQUIRE(drawn.sprites.size() == 1);
  CHECK(drawn.sprites.front().sprite == "이팩트\\subject_aura\\subject_aura");
  CHECK(drawn.sprites.front().action == 2);
  CHECK(drawn.sprites.front().scale == Approx(0.5F));
  CHECK(drawn.sprites.front().ground_position.y == Approx(3.0F + 8.0F));
}

TEST_CASE("Sprite d'effet : une âme joue une fois et s'en va", "[render][sprite_effect]") {
  Ticking ticking;
  auto soul = Spawn(kSoulShadow);
  CHECK(AdvanceAndDraw(*soul, ticking).sprites.size() == 1);
  CHECK(soul->Alive(ticking.tick));
  // Quatre images de quatre tics : passée la dernière, rien.
  static_cast<void>(AdvanceAndDraw(*soul, ticking, kFrames * 4));
  CHECK_FALSE(soul->Alive(ticking.tick));
  CHECK(AdvanceAndDraw(*soul, ticking).sprites.empty());
}

TEST_CASE("Sprite d'effet : celui qui vit tant que son porteur est là ne s'empile pas",
          "[render][sprite_effect]") {
  // Redemandé sur le même porteur, il rend celui qui boucle déjà : sinon chaque
  // annonce en ajoute un, jusqu'à masquer le porteur.
  CHECK(Spawn(kAmdaraisCostume)->reused_on_same_owner());
  CHECK(Spawn(kValkyrieWing)->reused_on_same_owner());
  // Ce qui s'en va de lui-même peut se rejouer par-dessus.
  CHECK_FALSE(Spawn(kSoulShadow)->reused_on_same_owner());
  CHECK_FALSE(Spawn(kLargeRing)->reused_on_same_owner());
}

TEST_CASE("Sprite d'effet : le grand anneau vit cent tics, une fois et demie plus grand",
          "[render][sprite_effect]") {
  Ticking ticking;
  auto ring = Spawn(kLargeRing);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*ring, ticking);
  REQUIRE(drawn.sprites.size() == 1);
  CHECK(drawn.sprites.front().scale == Approx(1.5F));
  static_cast<void>(AdvanceAndDraw(*ring, ticking, ro::render::kSpriteEffectFixedLifeTicks - 2));
  CHECK(ring->Alive(ticking.tick));
  static_cast<void>(AdvanceAndDraw(*ring, ticking));
  CHECK_FALSE(ring->Alive(ticking.tick));
}
