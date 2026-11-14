#include "ro/render/effect_class.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skill_suite_table.h"
#include "ro/render/skill_unit_pose.h"
#include "ro/render/str_effect_setup.h"

using Catch::Approx;

TEST_CASE("Classe d'effet : le repère du client est le nôtre, hauteur et profondeur niées",
          "[render][effect_class]") {
  const glm::vec3 client{3.0F, -4.0F, 5.0F};
  const glm::vec3 nous = ro::render::EffectClassFromClient(client);
  CHECK(nous == glm::vec3(3.0F, 4.0F, -5.0F));
  CHECK(ro::render::EffectClassToClient(nous) == client);
}

TEST_CASE("Classe d'effet : le registre est trié et répond à ses identifiants",
          "[render][effect_class]") {
  const std::vector<std::uint32_t> ids = ro::render::PortedEffectClassIds();
  REQUIRE_FALSE(ids.empty());
  CHECK(std::ranges::is_sorted(ids));
  CHECK(std::ranges::adjacent_find(ids) == ids.end());
  // Le chant de Frigg, la flèche de Triangle Shot, le boulet de Cart Cannon.
  for (const std::uint32_t id : {1059U, 1324U, 1357U}) {
    CHECK(ro::render::IsPortedEffectClass(id));
  }
  CHECK_FALSE(ro::render::IsPortedEffectClass(1));
  CHECK(ro::render::EffectClassBornAtCaster(1357));
  CHECK_FALSE(ro::render::EffectClassBornAtCaster(1324));
}

TEST_CASE("Classe d'effet : l'hôte d'un .str attend son retard, puis finit sur sa dernière image",
          "[render][effect_class]") {
  ro::render::EffectClassTick tick_index;
  tick_index.str_last_frame = [](std::string_view) { return std::optional<std::int32_t>(2); };
  ro::render::EffectClassStrHost hote;
  hote.path = "essai.str";
  hote.delay_ms = 32;
  // Deux appels de seize millisecondes épuisent le retard sans rien montrer.
  CHECK(ro::render::AdvanceEffectClassStrHost(hote, tick_index));
  CHECK(ro::render::AdvanceEffectClassStrHost(hote, tick_index));
  CHECK_FALSE(hote.shown_frame.has_value());
  for (std::int32_t image = 0; image <= 2; ++image) {
    CHECK(ro::render::AdvanceEffectClassStrHost(hote, tick_index));
    CHECK(hote.shown_frame == image);
  }
  // Fini : la dernière image reste ce qu'il montre.
  CHECK_FALSE(ro::render::AdvanceEffectClassStrHost(hote, tick_index));
  CHECK(hote.shown_frame == 2);
}

TEST_CASE("Lecteur de .str : Strantum Tremor se met à l'échelle de son niveau et vit avec l'unité",
          "[render][effect_class]") {
  const ro::render::StrEffectSetup* reglage = ro::render::StrEffectSetupFor(1893);
  REQUIRE(reglage != nullptr);
  CHECK(ro::render::StrPaintsBehindActors(*reglage));
  CHECK_FALSE(ro::render::StrPaintsOverEverything(*reglage));
  CHECK(reglage->lives_with_owner);
  CHECK(reglage->loops);
  CHECK(reglage->unique_on_owner);
  CHECK(ro::render::StrEffectLevelScale(*reglage, 1) == Approx(0.7F));
  CHECK(ro::render::StrEffectLevelScale(*reglage, 3) == Approx(1.25F));
  CHECK(ro::render::StrEffectLevelScale(*reglage, 6) == Approx(1.0F));
  CHECK(ro::render::StrEffectLevelScale(*reglage, 0) == Approx(1.0F));
  // La fente d'Earth Drive fait trembler l'écran à son vingtième tic.
  const ro::render::StrEffectSetup* fente = ro::render::StrEffectSetupFor(1706);
  REQUIRE(fente != nullptr);
  constexpr std::uint16_t kQuakeTick = 20;
  REQUIRE(fente->quake_ticks.size() == 1);
  CHECK(fente->quake_ticks.front() == kQuakeTick);
  CHECK(ro::render::StrEffectSetupFor(1703) == nullptr);
}

TEST_CASE("Lecteur de .str : la table est triée, et chaque mot de rendu a un sens",
          "[render][effect_class]") {
  const std::span<const ro::render::StrEffectSetup> table = ro::render::StrEffectSetupTable();
  REQUIRE_FALSE(table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id < table[rang].effect_id);
  }
  for (const ro::render::StrEffectSetup& reglage : table) {
    // Le mot de la création, la file du sol, ou par-dessus tout : rien d'autre.
    const bool sens = reglage.render_word == ro::render::kStrRenderWordOfCreation ||
                      ro::render::StrPaintsBehindActors(reglage) ||
                      ro::render::StrPaintsOverEverything(reglage);
    CHECK(sens);
  }
  // Le mot 517 peint par-dessus tout.
  const ro::render::StrEffectSetup* dessus = ro::render::StrEffectSetupFor(1607);
  REQUIRE(dessus != nullptr);
  CHECK(ro::render::StrPaintsOverEverything(*dessus));
  // Les listes nomment 1262 : un identifiant d'une autre classe, jamais lu ici.
  CHECK(ro::render::StrEffectSetupFor(1262) == nullptr);
}

TEST_CASE("Suite : l'échelle d'Earth Drive et d'Axe Stomp suit le niveau annoncé",
          "[render][effect_class]") {
  const auto scale = [](std::uint16_t skill, std::uint32_t effect, std::int32_t niveau) {
    for (const ro::render::SkillSuiteStep& step :
         ro::render::SkillSuiteOf(skill, ro::render::SkillSuiteMoment::kAnnounced)) {
      if (step.effect_id == effect && ro::render::SkillSuiteStepPlays(step, niveau)) {
        return std::optional<float>(step.scale);
      }
    }
    return std::optional<float>();
  };
  // Earth Drive borne le niveau à [1, 10] : zéro compte comme un.
  CHECK(scale(2323, 1706, 0) == Approx(0.5F));
  CHECK(scale(2323, 1707, 2) == Approx(0.5F));
  CHECK(scale(2323, 1706, 4) == Approx(0.7F));
  CHECK(scale(2323, 1706, 5) == Approx(0.9F));
  CHECK(scale(2323, 1706, 8) == Approx(1.0F));
  // Axe Stomp lit le niveau brut.
  CHECK(scale(5295, 1940, 0) == Approx(1.0F));
  CHECK(scale(5295, 1941, 1) == Approx(0.7F));
  CHECK(scale(5295, 1940, 3) == Approx(1.0F));
  CHECK(scale(5295, 1941, 5) == Approx(1.2F));
}

TEST_CASE("Suite : le trou de Vulcan Arm tire son angle par dizaines de degrés",
          "[render][effect_class]") {
  bool draws = false;
  for (const ro::render::SkillSuiteStep& step :
       ro::render::SkillSuiteOf(2258, ro::render::SkillSuiteMoment::kImpact)) {
    draws = draws || (step.effect_id == 1705 && step.spin_drawn);
  }
  CHECK(draws);
  CHECK(ro::render::SkillSuiteDrawSpinDegrees(0) == Approx(0.0F));
  CHECK(ro::render::SkillSuiteDrawSpinDegrees(7) == Approx(70.0F));
  CHECK(ro::render::SkillSuiteDrawSpinDegrees(35) == Approx(350.0F));
  CHECK(ro::render::SkillSuiteDrawSpinDegrees(36) == Approx(0.0F));
}

TEST_CASE("Unité : Strantum Tremor pose ses deux couches tournées d'un quart, fixes au sol",
          "[render][effect_class]") {
  constexpr std::uint16_t kStrantum = 269;
  std::size_t tournees = 0;
  for (const ro::render::SkillUnitPoseStep& step : ro::render::SkillUnitPoseOf(kStrantum)) {
    if (step.kind == ro::render::SkillUnitPoseKind::kEffect) {
      CHECK(step.spin_degrees == Approx(90.0F));
      CHECK(step.spin_anchored_to_ground);
      ++tournees;
    }
  }
  CHECK(tournees == 2);
}

TEST_CASE(
    "Classe d'effet : Shadow Exceed double son porteur, de dos ou de face, puis lui laisse son "
    "aura",
    "[render][effect_class]") {
  constexpr std::uint32_t kShadowExceed = 1867;
  constexpr std::uint32_t kShadowExceedAura = 1004;
  static constexpr std::int32_t kLastFrame = 3;
  constexpr float kMargin = 1e-3F;
  // Les directions dessinées : face à la caméra, ouest, nord, est.
  constexpr std::uint8_t kDrawnSouth = 0;
  constexpr std::uint8_t kDrawnWest = 2;
  constexpr std::uint8_t kDrawnNorth = 4;
  constexpr std::uint8_t kDrawnEast = 6;
  CHECK(ro::render::IsPortedEffectClass(kShadowExceed));
  const ro::render::EffectClassView view;
  ro::render::EffectClassBirth birth;
  birth.effect_id = kShadowExceed;
  birth.position = {10.0F, -3.0F, 20.0F};
  auto shadow = ro::render::SpawnEffectClass(birth, view);
  REQUIRE(shadow != nullptr);
  CHECK(shadow->reused_on_same_owner());
  ro::render::EffectClassTick tick;
  tick.view = &view;
  tick.owner_position = glm::vec3{10.0F, -3.0F, 20.0F};
  tick.str_last_frame = [](std::string_view) { return std::optional<std::int32_t>(kLastFrame); };
  // Le porteur regarde le nord, dos à la caméra : son image montre la direction
  // quatre, son corps fait cent quatre-vingts degrés.
  tick.owner_sprite_direction = kDrawnNorth;
  tick.owner_body_degrees = 180.0F;
  ro::render::EffectClassOutput events;
  shadow->Advance(tick, events);
  REQUIRE(events.sounds.size() == 1);
  CHECK(events.sounds.front().file == "effect\\SHC_SHADOW_EXCEED.wav");
  ro::render::EffectClassOutput drawn;
  shadow->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 2);
  // De dos : la paire arrière, sous les corps, retournée, aux quatorze
  // centièmes, cinq unités derrière lui — au sud, dans notre repère z croît
  // vers le sud —, douze puis dix unités au-dessus du sol.
  CHECK(drawn.strs[0].path ==
        "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_b_shadow_bottom.str");
  CHECK(drawn.strs[1].path ==
        "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_b_shadow.str");
  for (const ro::render::EffectClassStrDraw& str : drawn.strs) {
    CHECK(str.behind_actors);
    CHECK(str.flip_x);
    CHECK(str.scale == Approx(0.14F));
    CHECK(str.ground_position.x == Approx(10.0F).margin(kMargin));
    CHECK(str.ground_position.z == Approx(-15.0F).margin(kMargin));
  }
  CHECK(drawn.strs[0].ground_position.y == Approx(15.0F));
  CHECK(drawn.strs[1].ground_position.y == Approx(13.0F));
  // Vers l'ouest (deux) : la paire arrière encore, sans retournement.
  tick.owner_sprite_direction = kDrawnWest;
  tick.owner_body_degrees = 270.0F;
  shadow->Advance(tick, events);
  drawn = {};
  shadow->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 2);
  CHECK(drawn.strs[0].behind_actors);
  CHECK_FALSE(drawn.strs[0].flip_x);
  // Derrière un corps tourné vers l'ouest : cinq unités à l'est.
  CHECK(drawn.strs[0].ground_position.x == Approx(15.0F).margin(kMargin));
  CHECK(drawn.strs[0].ground_position.z == Approx(-20.0F).margin(kMargin));
  // Face à la caméra (zéro), le corps au sud : la paire de face, devant les
  // corps, retournée, cinq unités au nord.
  tick.owner_sprite_direction = kDrawnSouth;
  tick.owner_body_degrees = 360.0F;
  shadow->Advance(tick, events);
  drawn = {};
  shadow->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 2);
  CHECK(drawn.strs[0].path ==
        "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_f_shadow_bottom.str");
  CHECK_FALSE(drawn.strs[0].behind_actors);
  CHECK(drawn.strs[0].flip_x);
  CHECK(drawn.strs[0].ground_position.z == Approx(-25.0F).margin(kMargin));
  // De profil vers l'est (six) : la paire de face, sans retournement.
  tick.owner_sprite_direction = kDrawnEast;
  tick.owner_body_degrees = 90.0F;
  shadow->Advance(tick, events);
  drawn = {};
  shadow->Draw(view, drawn);
  REQUIRE(drawn.strs.size() == 2);
  CHECK_FALSE(drawn.strs[0].behind_actors);
  CHECK_FALSE(drawn.strs[0].flip_x);
  // Quatre images jouées : la suivante finit les paires, l'instance meurt et
  // demande l'aura sur son porteur — une fois, et plus rien ne se peint.
  CHECK(shadow->Alive(tick));
  CHECK(events.spawns_on_owner.empty());
  shadow->Advance(tick, events);
  CHECK_FALSE(shadow->Alive(tick));
  REQUIRE(events.spawns_on_owner.size() == 1);
  CHECK(events.spawns_on_owner.front() == kShadowExceedAura);
  drawn = {};
  shadow->Draw(view, drawn);
  CHECK(drawn.strs.empty());
  // Sans porteur, rien ne bouge, et l'instance s'en va sans rien demander.
  auto orphan = ro::render::SpawnEffectClass(birth, view);
  REQUIRE(orphan != nullptr);
  ro::render::EffectClassTick gone = tick;
  gone.owner_present = false;
  gone.owner_position.reset();
  CHECK_FALSE(orphan->Alive(gone));
  ro::render::EffectClassOutput nothing;
  orphan->Advance(gone, nothing);
  CHECK(nothing.spawns_on_owner.empty());
}
