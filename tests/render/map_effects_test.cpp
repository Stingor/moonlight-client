#include "ro/render/map_effects.h"

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_smoke.h"

using Catch::Approx;
using ro::render::MapEffectLot;
using ro::render::MapEffects;
using ro::render::MapEffectTable;
using ro::render::MapEmitterSetting;

namespace {

ro::formats::RswEffect Object(std::int32_t number, float x) {
  ro::formats::RswEffect object;
  object.number = number;
  object.position = {x, 0.0F, 0.0F};
  return object;
}

MapEmitterSetting Textured(std::string texture) {
  MapEmitterSetting setting;
  setting.texture = std::move(texture);
  return setting;
}

}  // namespace

TEST_CASE("chaque objet 974 reçoit l'entrée de son rang, et seulement elle") {
  MapEffectTable table;
  table.emitters.emplace(0, Textured("rang0.bmp"));
  table.emitters.emplace(1, Textured("rang1.bmp"));
  // Un vent et une cheminée s'intercalent : ils ne décalent pas le rang des 974.
  const std::vector<ro::formats::RswEffect> objects = {
      Object(974, 1.0F), Object(ro::render::kMapWindEffect, 2.0F),
      Object(ro::render::kMapChimneySmokeEffect, 3.0F), Object(974, 4.0F), Object(974, 5.0F)};
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);

  REQUIRE(effects.emitters().size() == 3);
  CHECK(effects.emitters()[0].setting().texture == "rang0.bmp");
  CHECK(effects.emitters()[0].native_position().x == 1.0F);
  CHECK(effects.emitters()[1].setting().texture == "rang1.bmp");
  CHECK(effects.emitters()[1].native_position().x == 4.0F);
  // Sans entrée : les défauts de la classe.
  CHECK(effects.emitters()[2].setting().texture == "smoke2.bmp");
  REQUIRE(effects.winds().size() == 1);
  CHECK(effects.winds()[0].setting().texture == "effect\\cloud11.tga");
  // La cheminée ne lit aucune table : elle prend seulement sa place.
  REQUIRE(effects.chimneys().size() == 1);
  CHECK(effects.chimneys()[0].native_position().x == 3.0F);

  const std::set<std::string> textures = ro::render::MapEffectTextures(table, objects);
  CHECK(textures ==
        std::set<std::string>{"rang0.bmp", "rang1.bmp", "smoke2.bmp", "effect\\cloud11.tga"});
}

TEST_CASE("les effets de carte ne vivent qu'au pas de seize millisecondes") {
  MapEffectTable table;
  const std::vector<ro::formats::RswEffect> objects = {Object(974, 0.0F)};
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);
  const glm::vec3 player{1000.0F, 0.0F, 1010.0F};

  effects.Advance(1000, player);
  effects.Advance(1015, player);
  CHECK_FALSE(effects.emitters()[0].drawn());
  effects.Advance(1016, player);
  CHECK(effects.emitters()[0].drawn());
}

namespace {

constexpr ro::render::MapEffectFrame kFrame{200, 200};
// Le porteur d'une cheminée du RSW : aucun.
constexpr std::uint32_t kNoOwner = 0;
// Deux places assez éloignées pour qu'un joueur posé sur l'une soit hors de
// portée de l'autre.
constexpr glm::vec3 kHere{0.0F, 0.0F, 0.0F};
constexpr float kThereX = 1000.0F;

// Un émetteur dont toute la réserve se peint dès le premier pas : autant de
// quads que de particules possibles, nées ou non.
MapEmitterSetting Reserve(std::string texture, std::int32_t count, float life_seconds) {
  MapEmitterSetting setting = Textured(std::move(texture));
  setting.max_count = count;
  setting.rate = {60.0F, 60.0F};
  setting.life = {life_seconds, life_seconds};
  return setting;
}

}  // namespace

TEST_CASE("un lot repris à l'image suivante garde la mémoire de ses quads") {
  // ⚠ LES LOTS SE REPRENNENT PAR LEUR PLACE, pas par leur émetteur : le premier
  // lot d'une image reprend le premier de la précédente, quel que soit l'effet
  // qui l'occupait. Sa mémoire doit suivre, sans quoi chaque image rallouerait
  // les quads de chaque émetteur.
  MapEffectTable table;
  // Le premier s'éteint vite une fois quitté ; le second est plus petit.
  table.emitters.emplace(0, Reserve("grand.bmp", 10, 0.1F));
  table.emitters.emplace(1, Reserve("petit.bmp", 2, 10.0F));
  const std::vector<ro::formats::RswEffect> objects = {Object(974, kHere.x), Object(974, kThereX)};
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);
  const ro::render::Camera camera;

  effects.Advance(0, kFrame.ToWorld(kHere));
  effects.Advance(160, kFrame.ToWorld(kHere));
  const std::span<const MapEffectLot> first = effects.Lots(camera);
  REQUIRE(first.size() == 1);
  REQUIRE(*first[0].texture == "grand.bmp");
  REQUIRE(first[0].quads.size() == 10);

  // Le joueur passe à l'autre émetteur : le grand se tait, le petit s'allume
  // et prend la première place.
  effects.Advance(1160, kFrame.ToWorld(glm::vec3{kThereX, 0.0F, 0.0F}));
  const std::span<const MapEffectLot> second = effects.Lots(camera);
  REQUIRE(second.size() == 1);
  REQUIRE(*second[0].texture == "petit.bmp");
  CHECK(second[0].quads.size() == 2);
  CHECK(second[0].quads.capacity() >= 10);
}

TEST_CASE("un lot repris ne garde rien de l'effet qui l'occupait") {
  // L'image d'une cheminée est propre à son lot ; l'émetteur qui reprend sa
  // place n'en a pas, et doit chercher la sienne dans la scène. Hériter de
  // celle de la cheminée peindrait de la fumée à sa place.
  MapEffectTable table;
  table.emitters.emplace(0, Textured("a.bmp"));
  const std::vector<ro::formats::RswEffect> objects = {
      Object(ro::render::kMapChimneySmokeEffect, kHere.x), Object(974, kThereX)};
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);
  const ro::render::Camera camera;
  ro::render::ChimneySmokeLook look;
  look.image.width = 1;
  look.image.height = 1;
  look.image.pixels.assign(4, 255);

  effects.Advance(0, kFrame.ToWorld(kHere));
  effects.Advance(160, kFrame.ToWorld(kHere));
  const std::span<const MapEffectLot> first = effects.Lots(camera, &look);
  REQUIRE(first.size() == 1);
  REQUIRE(first[0].image == &look.image);

  effects.Advance(320, kFrame.ToWorld(glm::vec3{kThereX, 0.0F, 0.0F}));
  const std::span<const MapEffectLot> second = effects.Lots(camera, &look);
  REQUIRE(second.size() == 1);
  CHECK(second[0].effect_id == ro::render::kMapEmitterEffect);
  CHECK(*second[0].texture == "a.bmp");
  CHECK(second[0].image == nullptr);
}

TEST_CASE("une cheminée posée en cours de partie vit comme celles du RSW, et part avec la carte") {
  // Une carte sans objet d'effet : la cheminée d'un effet annoncé la peuple
  // seule.
  MapEffectTable table;
  MapEffects effects;
  effects.Raise(table, {}, 200, 200);
  effects.AddChimney(kFrame.ToWorld(kHere), kNoOwner);
  REQUIRE(effects.chimneys().size() == 1);
  CHECK(effects.chimneys()[0].native_position().x == kHere.x);
  CHECK(effects.chimneys()[0].native_position().z == kHere.z);
  CHECK_FALSE(effects.empty());

  const ro::render::Camera camera;
  ro::render::ChimneySmokeLook look;
  look.image.width = 1;
  look.image.height = 1;
  look.image.pixels.assign(4, 255);
  // RIEN AVANT SA PREMIÈRE RENAISSANCE : ses bouffées naissent sans sprite, et
  // ne le reçoivent qu'au cinquante et unième pas.
  constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  constexpr std::uint32_t kStepsBeforeRebirth = 50;
  effects.Advance(0, kFrame.ToWorld(kHere));
  effects.Advance(kStepsBeforeRebirth * kTickMs, kFrame.ToWorld(kHere));
  const std::span<const MapEffectLot> before = effects.Lots(camera, &look);
  REQUIRE(before.size() == 1);
  CHECK(before[0].quads.empty());
  effects.Advance((kStepsBeforeRebirth + 1) * kTickMs, kFrame.ToWorld(kHere));
  const std::span<const MapEffectLot> after = effects.Lots(camera, &look);
  REQUIRE(after.size() == 1);
  CHECK_FALSE(after[0].quads.empty());

  // Et la carte suivante l'efface, comme les siennes.
  effects.Raise(table, {}, 200, 200);
  CHECK(effects.chimneys().empty());
}

TEST_CASE("une cheminée portée suit son porteur et meurt avec lui, celle du RSW ne bouge pas") {
  MapEffectTable table;
  const std::vector<ro::formats::RswEffect> objects = {
      Object(ro::render::kMapChimneySmokeEffect, kHere.x)};
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);
  constexpr std::uint32_t kOwner = 110'044;
  effects.AddChimney(kFrame.ToWorld(kHere), kOwner);
  REQUIRE(effects.chimneys().size() == 2);

  // Le porteur a marché : sa cheminée le rejoint, celle du RSW reste où elle est.
  constexpr glm::vec3 kWalkedTo{3.0F, 0.0F, 0.0F};
  effects.FollowOwners([&](std::uint32_t bearer) -> std::optional<glm::vec3> {
    CHECK(bearer == kOwner);
    return kFrame.ToWorld(kWalkedTo);
  });
  CHECK(effects.chimneys()[0].native_position().x == kHere.x);
  CHECK(effects.chimneys()[1].native_position().x == Approx(kWalkedTo.x));

  // Parti, il emporte la sienne ; celle du RSW demeure.
  effects.FollowOwners([](std::uint32_t) -> std::optional<glm::vec3> { return std::nullopt; });
  REQUIRE(effects.chimneys().size() == 1);
  CHECK(effects.chimneys()[0].owner_id() == kNoOwner);
}

TEST_CASE("le lot d'une cheminée portée se distingue de celui d'un objet de la carte") {
  MapEffectTable table;
  const std::vector<ro::formats::RswEffect> objects = {
      Object(ro::render::kMapChimneySmokeEffect, kHere.x)};
  MapEffects effects;
  effects.Raise(table, objects, 200, 200);
  constexpr std::uint32_t kOwner = 110'044;
  effects.AddChimney(kFrame.ToWorld(kHere), kOwner);

  const ro::render::Camera camera;
  ro::render::ChimneySmokeLook look;
  look.image.width = 1;
  look.image.height = 1;
  look.image.pixels.assign(4, 255);
  effects.Advance(0, kFrame.ToWorld(kHere));
  effects.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs), kFrame.ToWorld(kHere));
  const std::span<const MapEffectLot> lots = effects.Lots(camera, &look);
  REQUIRE(lots.size() == 2);
  CHECK_FALSE(lots[0].carried);
  CHECK(lots[1].carried);
}

TEST_CASE("la relève d'une même carte garde les cheminées portées, pas celles de la liste") {
  MapEffectTable table;
  const std::vector<ro::formats::RswEffect> day = {
      Object(ro::render::kMapChimneySmokeEffect, kHere.x)};
  MapEffects effects;
  effects.Raise(table, day, 200, 200);
  constexpr std::uint32_t kOwner = 110'044;
  effects.AddChimney(kFrame.ToWorld(kHere), kOwner);
  REQUIRE(effects.chimneys().size() == 2);

  // 🔴 L'EFFET QUI L'A DEMANDÉE NE SE REJOUE PAS : effacée ici, elle ne reviendrait
  // jamais tant que son porteur reste en vue.
  constexpr float kNightOnlyX = 9.0F;
  const std::vector<ro::formats::RswEffect> night = {
      Object(ro::render::kMapChimneySmokeEffect, kNightOnlyX), Object(974, kHere.x)};
  effects.RaiseKeepingCarried(table, night, 200, 200);
  REQUIRE(effects.chimneys().size() == 2);
  CHECK(effects.chimneys()[0].owner_id() == kNoOwner);
  CHECK(effects.chimneys()[0].native_position().x == kNightOnlyX);
  CHECK(effects.chimneys()[1].owner_id() == kOwner);
  CHECK(effects.chimneys()[1].native_position().x == Approx(kHere.x));
  CHECK(effects.emitters().size() == 1);

  // ⚠ ELLE SUIT TOUJOURS SON PORTEUR, et part avec lui.
  effects.FollowOwners([](std::uint32_t) -> std::optional<glm::vec3> { return std::nullopt; });
  REQUIRE(effects.chimneys().size() == 1);
  CHECK(effects.chimneys()[0].owner_id() == kNoOwner);

  // ET LA LEVÉE D'UNE CARTE NEUVE PART DE RIEN.
  effects.AddChimney(kFrame.ToWorld(kHere), kOwner);
  effects.Raise(table, {}, 200, 200);
  CHECK(effects.chimneys().empty());
}
