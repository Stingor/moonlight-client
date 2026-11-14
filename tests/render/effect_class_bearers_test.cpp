// LES CLASSES QU'UN ÉTAT POSE SUR SON PORTEUR : elles naissent, suivent leur
// porteur et montrent ce que le client montre.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_class.h"

using Catch::Approx;

namespace {

constexpr std::int32_t kStrLastFrame = 2;

ro::render::EffectClassBirth Birth(std::uint32_t effect_id, std::int32_t level = -1) {
  ro::render::EffectClassBirth birth;
  birth.effect_id = effect_id;
  birth.position = {10.0F, -3.0F, 20.0F};
  birth.level = level;
  return birth;
}

// Un tic où tout `.str` a trois images et tout sprite quatre, vu de face.
struct Ticking {
  ro::render::EffectClassView view;
  ro::render::EffectClassTick tick;
  Ticking() {
    tick.view = &view;
    tick.owner_position = glm::vec3{10.0F, -3.0F, 20.0F};
    tick.str_last_frame = [](std::string_view) {
      return std::optional<std::int32_t>(kStrLastFrame);
    };
    tick.sprite_frame_count = [](std::string_view, std::int32_t) { return 4; };
  }
};

std::unique_ptr<ro::render::EffectClassInstance> Spawn(std::uint32_t effect_id,
                                                       std::int32_t level = -1) {
  const ro::render::EffectClassView view;
  auto instance = ro::render::SpawnEffectClass(Birth(effect_id, level), view);
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
  drawn.sounds = events.sounds;
  return drawn;
}

std::vector<std::string> Paths(const ro::render::EffectClassOutput& output) {
  std::vector<std::string> paths;
  for (const ro::render::EffectClassStrDraw& str : output.strs) {
    paths.push_back(str.path);
  }
  return paths;
}

}  // namespace

TEST_CASE("Classe d'état : les classes que les états posent sont portées",
          "[render][effect_class]") {
  for (const std::uint32_t id : {1096U, 1097U, 1110U, 1151U, 1155U, 1160U, 1214U, 1217U, 1222U,
                                 1800U, 1839U, 1913U, 2305U, 2306U}) {
    CHECK(ro::render::IsPortedEffectClass(id));
  }
}

TEST_CASE("Classe d'état : l'Univers du Star Emperor montre un soleil, une lune et une étoile",
          "[render][effect_class]") {
  Ticking ticking;
  auto universe = Spawn(1217);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*universe, ticking);
  REQUIRE(drawn.quads.size() == 3);
  CHECK(drawn.quads[0].texture == "effect\\sun_stance\\sun_stance.tga");
  CHECK(drawn.quads[1].texture == "effect\\sun_stance\\lunar_stance.tga");
  CHECK(drawn.quads[2].texture == "effect\\sun_stance\\star_stance.tga");
  // Le Soleil seul : trois soleils.
  auto sun = Spawn(1214);
  for (const ro::render::EffectClassQuadDraw& quad : AdvanceAndDraw(*sun, ticking).quads) {
    CHECK(quad.texture == "effect\\sun_stance\\sun_stance.tga");
  }
  ticking.tick.owner_present = false;
  CHECK_FALSE(sun->Alive(ticking.tick));
}

TEST_CASE("Classe d'état : Climax joue son fond selon le niveau, et sonne à la naissance",
          "[render][effect_class]") {
  Ticking ticking;
  auto climax = Spawn(1800, 3);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*climax, ticking);
  const std::vector<std::string> paths = Paths(drawn);
  CHECK(std::ranges::find(paths, "climax\\climax\\climax.str") != paths.end());
  CHECK(std::ranges::find(paths, "climax\\climax_bottom\\climax_bottom_3.str") != paths.end());
  REQUIRE(drawn.sounds.size() == 1);
  CHECK(drawn.sounds.front().file == "effect\\AG_CLIMAX.wav");
  CHECK(climax->reused_on_same_owner());
  // Les deux bouclent : après bien plus de tics que leurs trois images, ils sont
  // toujours là.
  CHECK(Paths(AdvanceAndDraw(*climax, ticking, 10)).size() == 2);
  // Hors de un à cinq, pas de fond.
  auto without_bottom = Spawn(1800, 7);
  CHECK(Paths(AdvanceAndDraw(*without_bottom, ticking)).size() == 1);
}

TEST_CASE("Classe d'état : le compte d'Eternal Slash montre son chiffre, puis s'en va",
          "[render][effect_class]") {
  Ticking ticking;
  auto count = Spawn(1913, 2);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*count, ticking);
  REQUIRE(drawn.strs.size() == 1);
  CHECK(drawn.strs.front().path ==
        "eternal_slash\\eternal_slash_count\\eternal_slash_count_02.str");
  // Vingt-cinq unités au-dessus du porteur, dans notre repère.
  CHECK(drawn.strs.front().ground_position.y == Approx(3.0F + 25.0F));
  CHECK(count->Alive(ticking.tick));
  static_cast<void>(AdvanceAndDraw(*count, ticking, kStrLastFrame + 1));
  CHECK_FALSE(count->Alive(ticking.tick));
  // Un compte hors de un à cinq ne montre rien.
  CHECK_FALSE(Spawn(1913, 0)->Alive(ticking.tick));
}

TEST_CASE("Classe d'état : les cubes maudits apparaissent, puis bouclent",
          "[render][effect_class]") {
  Ticking ticking;
  auto cube = Spawn(2305);
  CHECK(Paths(AdvanceAndDraw(*cube, ticking)) ==
        std::vector<std::string>{
            "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_bottom_start.str",
            "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_start.str"});
  // Trois images, puis l'apparition finie s'en va et la boucle la remplace.
  static_cast<void>(AdvanceAndDraw(*cube, ticking, kStrLastFrame + 2));
  const std::vector<std::string> loop = Paths(AdvanceAndDraw(*cube, ticking, 2));
  CHECK(loop == std::vector<std::string>{
                    "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_bottom_loop.str",
                    "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_loop.str"});
  CHECK(cube->Alive(ticking.tick));
}

TEST_CASE("Classe d'état : la machine d'attaque attend une fois, puis attend en boucle",
          "[render][effect_class]") {
  Ticking ticking;
  auto machine = Spawn(1839);
  CHECK(Paths(AdvanceAndDraw(*machine, ticking)) ==
        std::vector<std::string>{"a_machine\\a_machine\\a_machine_attack_waiting.str"});
  static_cast<void>(AdvanceAndDraw(*machine, ticking, kStrLastFrame + 2));
  CHECK(Paths(AdvanceAndDraw(*machine, ticking)) ==
        std::vector<std::string>{"a_machine\\a_machine\\a_machine_waiting.str"});
}

TEST_CASE("Classe d'état : les billes d'âme choisissent leur image par leur direction",
          "[render][effect_class]") {
  Ticking ticking;
  auto beads = Spawn(1096);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*beads, ticking);
  REQUIRE(drawn.sprites.size() == 3);
  CHECK(drawn.sprites[0].sprite == "이팩트\\su_spritemable\\su_spritemable_green");
  // La première bille est à un degré, caméra de face : 1 − 75 = −74, divisé
  // par trente en tronquant vers zéro, −2, soit le secteur 10 sur 12.
  CHECK(drawn.sprites[0].frame == 10);
  // La deuxième est à 121 degrés : (121 − 75) / 30 = 1.
  CHECK(drawn.sprites[1].frame == 1);
}

TEST_CASE("Classe d'état : l'immunité à un élément pose son symbole au-dessus du porteur",
          "[render][effect_class]") {
  Ticking ticking;
  auto fire = Spawn(1154);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*fire, ticking);
  REQUIRE(drawn.sprites.size() == 1);
  CHECK(drawn.sprites.front().sprite == "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_FIRE");
  CHECK(drawn.sprites.front().ground_position.y == Approx(3.0F + 25.0F));
  // Les racines de Root Twist, elles, sont trois unités sous les pieds.
  auto roots = Spawn(1110);
  CHECK(AdvanceAndDraw(*roots, ticking).sprites.front().ground_position.y == Approx(3.0F - 3.0F));
}
