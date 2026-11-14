// LES OBJETS LANCÉS PAR LES COMPÉTENCES DE L'ALCHIMISTE : flacon d'acide (298),
// potion (299) et potion Berserk (541).

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/skill_cast_extra.h"

using Catch::Approx;

namespace {

constexpr std::int32_t kInverseSourceAlphaBlend = 6;
constexpr std::int32_t kOneBlend = 2;
// LE LANCEUR À L'ORIGINE, la cible à cinquante unités sur l'axe des abscisses,
// dans le repère du client.
constexpr glm::vec3 kThrower{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kTarget{50.0F, 0.0F, 0.0F};

std::unique_ptr<ro::render::EffectClassInstance> Throw(std::uint32_t effect_id,
                                                       std::int32_t level = -1) {
  ro::render::EffectClassBirth birth;
  birth.effect_id = effect_id;
  birth.position = kThrower;
  birth.bound_position = kTarget;
  birth.level = level;
  return ro::render::SpawnEffectClass(birth, ro::render::EffectClassView{});
}

struct Ticking {
  ro::render::EffectClassView view;
  ro::render::EffectClassTick tick;
  Ticking() {
    tick.view = &view;
    tick.owner_position = kThrower;
  }
};

ro::render::EffectClassOutput Step(ro::render::EffectClassInstance& instance, Ticking& ticking) {
  ro::render::EffectClassOutput events;
  instance.Advance(ticking.tick, events);
  ro::render::EffectClassOutput drawn;
  instance.Draw(ticking.view, drawn);
  return drawn;
}

// LE CENTRE D'UN QUAD, dans notre repère.
glm::vec3 Centre(const ro::render::EffectWorldQuad& quad) {
  return (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) / 4.0F;
}

}  // namespace

TEST_CASE("Objet lancé : les trois objets de l'alchimiste sont des classes",
          "[render][effect_class]") {
  CHECK(ro::render::IsPortedEffectClass(298));
  CHECK(ro::render::IsPortedEffectClass(299));
  CHECK(ro::render::IsPortedEffectClass(541));
}

TEST_CASE("Objet lancé : une tête et trois copies pâlissantes", "[render][effect_class]") {
  auto instance = Throw(298);
  REQUIRE(instance != nullptr);
  Ticking ticking;
  const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
  REQUIRE(drawn.quads.size() == 4);
  CHECK(drawn.quads[0].quad.tint[3] == 255);
  CHECK(drawn.quads[1].quad.tint[3] == 105);
  CHECK(drawn.quads[2].quad.tint[3] == 80);
  CHECK(drawn.quads[3].quad.tint[3] == 55);
  // Le flacon d'acide est vert pâle et se mélange par son alpha.
  CHECK(drawn.quads[0].quad.tint[0] == 135);
  CHECK(drawn.quads[0].quad.tint[1] == 255);
  CHECK(drawn.quads[0].quad.blend_destination == kInverseSourceAlphaBlend);
}

TEST_CASE("Objet lancé : vingt-six pas jusqu'à la cible, en cloche", "[render][effect_class]") {
  auto instance = Throw(298);
  Ticking ticking;
  // À mi-vol, la tête a fait treize pas de deux unités et monte au sommet de sa
  // cloche : trente unités, plus les huit de départ.
  ro::render::EffectClassOutput drawn;
  for (int tick = 0; tick <= 13; ++tick) {
    drawn = Step(*instance, ticking);
  }
  REQUIRE_FALSE(drawn.quads.empty());
  const glm::vec3 head = Centre(drawn.quads[0].quad);
  // Le quad se tient une unité devant sa place, que sa rotation propre déplace.
  CHECK(head.x == Approx(28.0F).margin(1.01F));
  CHECK(head.y == Approx(8.0F + 30.0F * std::sin(90.0F * 3.141592F / 180.0F)).margin(0.5F));
  // Arrivé, l'objet pâlit de vingt par tic et finit par disparaître.
  for (int tick = 14; tick < 60; ++tick) {
    drawn = Step(*instance, ticking);
  }
  CHECK(drawn.quads.empty());
}

TEST_CASE("Objet lancé : la potion prend la teinte de son niveau", "[render][effect_class]") {
  struct Expected {
    std::int32_t level;
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::int32_t blend;
  };
  for (const Expected& expected : {Expected{1, 255, 135, 135, kInverseSourceAlphaBlend},
                                   Expected{2, 255, 205, 135, kInverseSourceAlphaBlend},
                                   Expected{3, 255, 255, 135, kInverseSourceAlphaBlend},
                                   Expected{4, 255, 255, 255, kInverseSourceAlphaBlend},
                                   Expected{5, 135, 135, 255, kInverseSourceAlphaBlend},
                                   Expected{6, 255, 255, 255, kOneBlend}}) {
    auto instance = Throw(299, expected.level);
    Ticking ticking;
    const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
    REQUIRE_FALSE(drawn.quads.empty());
    CHECK(drawn.quads[0].quad.tint[0] == expected.red);
    CHECK(drawn.quads[0].quad.tint[1] == expected.green);
    CHECK(drawn.quads[0].quad.tint[2] == expected.blue);
    CHECK(drawn.quads[0].quad.blend_destination == expected.blend);
  }
}

TEST_CASE("Objet lancé : le paramètre que chaque compétence passe", "[render][effect_class]") {
  CHECK(ro::render::EffectClassThrownParam(299, 3) == 3);
  CHECK(ro::render::EffectClassThrownParam(299, 0) == 1);
  CHECK(ro::render::EffectClassThrownParam(299, 12) == 10);
  CHECK(ro::render::EffectClassThrownParam(541, 1) == 4);
  CHECK_FALSE(ro::render::EffectClassThrownParam(298, 5).has_value());
}

TEST_CASE("Objet lancé : les flacons d'Acid Demonstration partent au cinquième et au dixième tic",
          "[render][effect_class]") {
  CHECK(ro::render::IsPortedEffectClass(539));
  CHECK(ro::render::IsPortedEffectClass(980));
  for (const auto& [effect_id, start_tick] : {std::pair{539U, 5}, std::pair{980U, 10}}) {
    auto instance = Throw(effect_id);
    Ticking ticking;
    for (int tick = 0; tick < start_tick; ++tick) {
      CHECK(Step(*instance, ticking).quads.empty());
    }
    const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
    REQUIRE(drawn.quads.size() == 4);
    // Blanc, mélangé par son alpha, et une cloche d'une seule unité.
    CHECK(drawn.quads[0].quad.tint[0] == 255);
    CHECK(drawn.quads[0].quad.tint[1] == 255);
    CHECK(drawn.quads[0].quad.blend_destination == kInverseSourceAlphaBlend);
  }
}

TEST_CASE("Objet lancé : Acid Demonstration lance ses deux flacons en incantant",
          "[render][effect_class]") {
  const std::span<const ro::render::SkillCastExtraEntry> flacons =
      ro::render::SkillCastExtrasFor(490);
  REQUIRE(flacons.size() == 2);
  CHECK(flacons[0].effect_id == 539);
  CHECK(flacons[1].effect_id == 980);
  CHECK(flacons[0].thrown_at_target);
  CHECK(flacons[1].thrown_at_target);
  // Les anneaux de Soul Collect, eux, naissent sur le moine sans rien lancer.
  CHECK_FALSE(ro::render::SkillCastExtrasFor(401)[0].thrown_at_target);
}

TEST_CASE("Objet lancé : la pierre et les lames volent à pas fixe", "[render][effect_class]") {
  for (const std::uint32_t effect_id : {308U, 600U, 613U, 614U, 615U, 616U, 914U}) {
    CHECK(ro::render::IsPortedEffectClass(effect_id));
  }
  // LA PIERRE avance de deux unités par tic : un tic suffit pour le voir.
  {
    auto instance = Throw(308);
    Ticking ticking;
    const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
    REQUIRE(drawn.quads.size() == 4);
    CHECK(drawn.quads[0].quad.tint[0] == 255);
    CHECK(drawn.quads[0].quad.tint[1] == 255);
  }
  // LE SHURIKEN est rose et vit deux cents tics.
  {
    auto instance = Throw(613);
    Ticking ticking;
    const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
    REQUIRE(drawn.quads.size() == 4);
    CHECK(drawn.quads[0].quad.tint[0] == 255);
    CHECK(drawn.quads[0].quad.tint[1] == 200);
    CHECK(drawn.quads[0].quad.tint[2] == 255);
    for (int tick = 1; tick < 200; ++tick) {
      static_cast<void>(Step(*instance, ticking));
    }
    CHECK_FALSE(instance->Alive(ticking.tick));
  }
  // LA SPORE reçoit quatre de son site : blanche.
  CHECK(ro::render::EffectClassThrownParam(914, 7) == 4);
}

TEST_CASE("Objet lancé : huit kunai en étoile qui font tourner leur lanceur",
          "[render][effect_class]") {
  // SANS ACTEUR LIÉ, chaque groupe part de la place de son lanceur, décalé de
  // quarante-cinq degrés du précédent.
  ro::render::EffectClassBirth birth;
  birth.effect_id = 981;
  birth.position = kThrower;
  auto instance = ro::render::SpawnEffectClass(birth, ro::render::EffectClassView{});
  REQUIRE(instance != nullptr);
  Ticking ticking;
  const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
  REQUIRE(drawn.quads.size() == 32);
  // Les têtes des huit groupes partent sur huit caps : huit places distinctes.
  std::set<std::pair<long, long>> heads;
  for (std::size_t head = 0; head < drawn.quads.size(); head += 4) {
    const glm::vec3 centre = Centre(drawn.quads[head].quad);
    heads.emplace(std::lround(centre.x * 10.0F), std::lround(centre.z * 10.0F));
  }
  CHECK(heads.size() == 8);
  // Le lanceur pivote de quarante-cinq degrés par tic pendant dix-neuf tics.
  const std::array<ro::render::ActorEffectPlay, 1> early = {{{981, 0}}};
  const std::array<ro::render::ActorEffectPlay, 1> late = {{{981, 100}}};
  CHECK(ro::render::SpinDegreesAfterEffects(early) == Approx(45.0F));
  CHECK(ro::render::SpinDegreesAfterEffects(late) == Approx(45.0F * 19.0F));
}

TEST_CASE("Objet lancé : une orbe par sphère libérée, de l'image de leur élément",
          "[render][effect_class]") {
  ro::render::EffectClassBirth birth;
  birth.effect_id = 989;
  birth.position = kThrower;
  birth.bound_position = kTarget;
  birth.owner_charm_element = 3;
  birth.owner_charm_count = 3;
  auto instance = ro::render::SpawnEffectClass(birth, ro::render::EffectClassView{});
  Ticking ticking;
  const ro::render::EffectClassOutput drawn = Step(*instance, ticking);
  REQUIRE(drawn.quads.size() == 3);
  CHECK(drawn.quads[0].texture == "effect\\FireOrb.bmp");
  // Elles s'ajoutent, sans se mélanger par leur alpha.
  CHECK(drawn.quads[0].quad.blend_destination == kOneBlend);
  // Sans sphère, rien.
  birth.owner_charm_count = 0;
  auto none = ro::render::SpawnEffectClass(birth, ro::render::EffectClassView{});
  CHECK(Step(*none, ticking).quads.empty());
}

TEST_CASE("Objet lancé : il meurt avec son lanceur", "[render][effect_class]") {
  auto instance = Throw(541, 4);
  Ticking ticking;
  CHECK(instance->Alive(ticking.tick));
  ticking.tick.owner_present = false;
  CHECK_FALSE(instance->Alive(ticking.tick));
}
