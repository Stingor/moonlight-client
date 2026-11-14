#include "effects/effect_str_look.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/mat4x4.hpp>

#include "ro/render/effect_catalog.h"
#include "ro/render/effect_duration.h"

using Catch::Approx;
using moonclient::EffectMapMemory;
using moonclient::LookStrPlaying;
using moonclient::ShapeStrPlaying;
using moonclient::StrFrameAt;
using moonclient::StrLook;
using moonclient::StrTurnDegrees;

namespace {

constexpr float kTickMs = 16.0F;

}  // namespace

TEST_CASE("Regard d'un .str : une rotation fixe au sol compense la vue", "[app][str_look]") {
  EffectMapMemory::Playing played;
  const StrLook regard{.spin_degrees = 90.0F, .spin_follows_view = true};
  LookStrPlaying(played, 1893, regard, 30.0F);
  CHECK(StrTurnDegrees(played, 30.0F) == Approx(90.0F));
  // La vue tourne de quarante degrés : l'image aussi, et reste où elle était au sol.
  CHECK(StrTurnDegrees(played, 70.0F) == Approx(130.0F));
  // Ramenée dans un tour, des deux côtés.
  CHECK(StrTurnDegrees(played, 330.0F) == Approx(30.0F));
  CHECK(StrTurnDegrees(played, -80.0F) == Approx(340.0F));
  // Sans ancrage, elle reste fixe à l'écran.
  played.turn_follows_view = false;
  CHECK(StrTurnDegrees(played, 70.0F) == Approx(90.0F));
}

TEST_CASE("Regard d'un .str : le lecteur ajoute son échelle, sa file et sa vie",
          "[app][str_look]") {
  EffectMapMemory::Playing played;
  constexpr std::uint32_t kUnit = 4'242;
  CHECK(ShapeStrPlaying(played, 1894, kUnit, std::nullopt).empty());
  LookStrPlaying(played, 1894, StrLook{.scale = 2.0F, .level = 1}, 0.0F);
  CHECK(played.scale == Approx(1.4F));
  CHECK(played.behind_actors);
  CHECK(played.loops_with_actor == kUnit);
  CHECK_FALSE(played.loops_endlessly);
  // SANS PORTEUR, la vie liée à lui tient pour toujours : le fichier reboucle.
  EffectMapMemory::Playing au_sol;
  static_cast<void>(ShapeStrPlaying(au_sol, 1894, 0, std::nullopt));
  CHECK(au_sol.loops_endlessly);
  CHECK(au_sol.loops_with_actor == 0);
  // Un fichier qui boucle sans vivre avec son porteur ne s'éteint pas avec lui.
  EffectMapMemory::Playing mark;
  static_cast<void>(ShapeStrPlaying(mark, 1090, kUnit, std::nullopt));
  CHECK(mark.loops_endlessly);
  CHECK(mark.loops_with_actor == 0);
}

TEST_CASE("Regard d'un .str : les tremblements partent chacun à son tic", "[app][str_look]") {
  EffectMapMemory::Playing played;
  played.started_ms = 1'000;
  // La fente d'Earth Drive, à son vingtième tic.
  CHECK(ShapeStrPlaying(played, 1706, 0, std::nullopt) ==
        std::vector<std::uint32_t>{1'000 + 20 * 16});
  // La pluie de météores en fait trembler cinq, de cinquante en cinquante tics.
  const std::vector<std::uint32_t> pluie = ShapeStrPlaying(played, 1162, 0, std::nullopt);
  REQUIRE(pluie.size() == 5);
  CHECK(pluie.front() == 1'000 + 50 * 16);
  CHECK(pluie.back() == 1'000 + 250 * 16);
  CHECK(ShapeStrPlaying(played, 1703, 0, std::nullopt).empty());
}

TEST_CASE("Regard d'un .str : relu, borné, ou tenu sur sa dernière image", "[app][str_look]") {
  ro::render::LoadedEffect file;
  file.prepared.last_frame = 9;
  EffectMapMemory::Playing played;
  played.effect = &file;
  const float un_tour_ms = 10.0F * kTickMs;
  CHECK(StrFrameAt(played, un_tour_ms - 1.0F) == std::optional<std::int32_t>(9));
  CHECK_FALSE(StrFrameAt(played, un_tour_ms).has_value());
  // RELU DEUX FOIS : trois tours, puis éteint.
  static_cast<void>(ShapeStrPlaying(played, 1992, 0, std::nullopt));
  CHECK(played.replays == 2);
  CHECK(StrFrameAt(played, un_tour_ms) == std::optional<std::int32_t>(0));
  CHECK(StrFrameAt(played, 3.0F * un_tour_ms - 1.0F) == std::optional<std::int32_t>(9));
  CHECK_FALSE(StrFrameAt(played, 3.0F * un_tour_ms).has_value());
  // BORNÉ PAR LA TABLE DES DURÉES, même quand le fichier boucle.
  EffectMapMemory::Playing borne;
  borne.effect = &file;
  borne.loops_endlessly = true;
  static_cast<void>(ShapeStrPlaying(borne, 2027, 0, std::nullopt));
  REQUIRE(borne.life_ticks == ro::render::EffectDurationTicks(2027));
  CHECK(StrFrameAt(borne, static_cast<float>(borne.life_ticks) * kTickMs - 1.0F).has_value());
  CHECK_FALSE(StrFrameAt(borne, static_cast<float>(borne.life_ticks) * kTickMs).has_value());
  // Tenu sur sa dernière image.
  EffectMapMemory::Playing held;
  held.effect = &file;
  held.holds_last_frame = true;
  CHECK(StrFrameAt(held, 100.0F * un_tour_ms) == std::optional<std::int32_t>(9));
}

TEST_CASE("Regard d'un .str : une seule instance par porteur pour certains", "[app][str_look]") {
  constexpr std::uint32_t kBearer = 77;
  std::vector<EffectMapMemory::Playing> en_jeu(1);
  en_jeu.front().actor_id = kBearer;
  en_jeu.front().from_effect_id = 1893;
  CHECK(moonclient::StrAlreadyOnOwner(en_jeu, 1893, kBearer));
  CHECK_FALSE(moonclient::StrAlreadyOnOwner(en_jeu, 1893, kBearer + 1));
  // 141 vit avec son porteur, mais s'y joue autant de fois qu'on le demande.
  en_jeu.front().from_effect_id = 141;
  CHECK_FALSE(moonclient::StrAlreadyOnOwner(en_jeu, 141, kBearer));
}

TEST_CASE("Regard d'un .str : l'angle d'écran tourne dans le sens des aiguilles d'une montre",
          "[app][str_look]") {
  const ro::render::Camera camera;
  const glm::mat4 view = camera.ViewMatrix();
  const glm::vec3 right{view[0][0], view[1][0], view[2][0]};
  const glm::vec3 top{view[0][1], view[1][1], view[2][1]};
  const glm::vec3 start{0.0F, 0.0F, 0.0F};
  constexpr float kStep = 10.0F;
  CHECK(*moonclient::ScreenBearingDegrees(camera, start, start + kStep * right) ==
        Approx(0.0F).margin(0.01));
  CHECK(*moonclient::ScreenBearingDegrees(camera, start, start - kStep * top) == Approx(90.0F));
  CHECK(*moonclient::ScreenBearingDegrees(camera, start, start - kStep * right) == Approx(180.0F));
  CHECK(*moonclient::ScreenBearingDegrees(camera, start, start + kStep * top) == Approx(270.0F));
  CHECK_FALSE(moonclient::ScreenBearingDegrees(camera, start, start).has_value());
  // L'IMAGE REGARDE D'OÙ ELLE VIENT : 2250 vise à droite et se tourne vers la
  // gauche.
  CHECK(*moonclient::StrTowardTargetDegrees(2250, &camera, start, start + kStep * right) ==
        Approx(180.0F));
  CHECK_FALSE(
      moonclient::StrTowardTargetDegrees(1893, &camera, start, start + kStep * right).has_value());
}

TEST_CASE("Regard d'un .str : l'éclair d'un Shockwave Trap reste sur son piège",
          "[app][str_look]") {
  // Il ne jouait son fichier qu'une fois : le piège restait nu bien avant la
  // fin. Le client le fait repartir à sa fin jusqu'au bout de sa vie de table,
  // et l'unité qui le porte l'emporte en s'en allant.
  constexpr std::uint32_t kShockwaveEffect = 145;
  constexpr std::uint32_t kTrap = 7'001;
  EffectMapMemory::Playing on_trap;
  static_cast<void>(ShapeStrPlaying(on_trap, kShockwaveEffect, kTrap, std::nullopt));
  CHECK(on_trap.loops_with_actor == kTrap);
  CHECK(on_trap.life_ticks == ro::render::EffectDurationTicks(kShockwaveEffect));
  CHECK(on_trap.life_ticks > 0);
  // Sans porteur, il repart jusqu'au bout de sa vie.
  EffectMapMemory::Playing alone;
  static_cast<void>(ShapeStrPlaying(alone, kShockwaveEffect, 0, std::nullopt));
  CHECK(alone.loops_endlessly);
  CHECK(alone.life_ticks == on_trap.life_ticks);
  // Son voisin, le coup du même piège, ne repart pas.
  EffectMapMemory::Playing hit;
  static_cast<void>(ShapeStrPlaying(hit, kShockwaveEffect + 1, kTrap, std::nullopt));
  CHECK(hit.loops_with_actor == 0);
  CHECK_FALSE(hit.loops_endlessly);
}
