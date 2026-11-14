#include "ro/render/ground_skill_visual.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/skill_impl.h"

using ro::render::BearerDeathTick;
using ro::render::EffectNodePool;
using ro::render::GroundSkillLauncherCarries;
using ro::render::GroundSkillVisualBearer;
using ro::render::GroundSkillVisualEntry;
using ro::render::GroundSkillVisualsToPlay;
using ro::render::kEffectTickMs;
using ro::render::PooledEffectNode;
using ro::render::SkillImplFor;

namespace {

constexpr std::uint16_t kIceWallSkill = 87;
constexpr std::uint16_t kIceWallEffect = 74;
constexpr std::uint16_t kGroundDriveSkill = 770;
constexpr std::uint16_t kGroundDriveEffect = 1399;
constexpr std::uint16_t kMaxPainSkill = 717;
constexpr std::uint16_t kMaxPainEffect = 1032;
constexpr std::int32_t kAnyLevel = 5;

// Ce que le chemin de la case pose pour une compétence, comme l'application le
// demande, sous la bascule donnée — éteinte par défaut, comme chez le joueur.
std::vector<GroundSkillVisualEntry> PlayedOnGround(std::uint16_t skill_id,
                                                   bool minimal_effects = false) {
  return GroundSkillVisualsToPlay(skill_id, SkillImplFor(skill_id).EffectId(), kAnyLevel,
                                  minimal_effects);
}

// L'effet unique qu'une compétence pose sous une bascule.
std::uint16_t OnlyEffectOf(std::uint16_t skill_id, bool minimal_effects) {
  const std::vector<GroundSkillVisualEntry> played = PlayedOnGround(skill_id, minimal_effects);
  REQUIRE(played.size() == 1);
  return played[0].effect_id;
}

// L'entrée d'une compétence qui pose cet effet, sous la bascule éteinte.
GroundSkillVisualEntry EntryOf(std::uint16_t skill_id, std::uint16_t effect_id) {
  const std::vector<GroundSkillVisualEntry> played = PlayedOnGround(skill_id);
  const auto found = std::ranges::find(played, effect_id, &GroundSkillVisualEntry::effect_id);
  REQUIRE(found != played.end());
  return *found;
}

}  // namespace

TEST_CASE("Ice Wall : la case reçoit 74, porté par un lanceur qui meurt à 6 240 ms",
          "[render][groundskill]") {
  // Les tables fondues ne connaissent pas Ice Wall, et le chemin de la case ne
  // posait rien. Le client pose 74 sur la case visée par un lanceur sans corps
  // qui vit (250 + 10) pas de vingt-quatre millisecondes.
  constexpr std::int32_t kLauncherLifeMs = (250 + 10) * 24;
  const std::vector<GroundSkillVisualEntry> played = PlayedOnGround(kIceWallSkill);
  REQUIRE(played.size() == 1);
  CHECK(played[0].effect_id == kIceWallEffect);
  CHECK(played[0].bearer == GroundSkillVisualBearer::kLauncher);
  CHECK(played[0].launcher_life_ms == kLauncherLifeMs);
  CHECK(GroundSkillLauncherCarries(played[0]));
  // ⚠ ET LA COUPE MORD : l'instance se déclare dix mille tics, ses pointes
  // aussi, et le lanceur meurt bien avant.
  CHECK(ro::render::EffectNodeLifeFor(kIceWallEffect) > BearerDeathTick(kLauncherLifeMs));
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(kIceWallEffect)) {
    CHECK(pattern.life_ticks > BearerDeathTick(kLauncherLifeMs));
  }
}

TEST_CASE("Ground Drive : la case reçoit 1399, et son lanceur ne le coupe pas",
          "[render][groundskill]") {
  // Les tables fondues ne le posaient pas sur la case. Le client, lui, le pose
  // par un lanceur de (75 + 10) × 24 = 2 040 ms — mais l'instance meurt à son
  // cent vingt-cinquième tic, 2 000 ms, et emporte ses pointes avant lui.
  constexpr std::int32_t kLauncherLifeMs = (75 + 10) * 24;
  const std::vector<GroundSkillVisualEntry> played = PlayedOnGround(kGroundDriveSkill);
  REQUIRE(played.size() == 1);
  CHECK(played[0].effect_id == kGroundDriveEffect);
  CHECK(played[0].bearer == GroundSkillVisualBearer::kLauncher);
  CHECK(played[0].launcher_life_ms == kLauncherLifeMs);
  CHECK(ro::render::EffectNodeLifeFor(kGroundDriveEffect) < BearerDeathTick(kLauncherLifeMs));
}

TEST_CASE("Max Pain : 1032 naît sur le lanceur du sort, pas sur la case", "[render][groundskill]") {
  // Le répartiteur de la case pose 1032 sur l'acteur qui reçoit le message :
  // celui qui a lancé. Le poser sur la case le déplaçait sous les pieds d'un
  // autre.
  const std::vector<GroundSkillVisualEntry> played = PlayedOnGround(kMaxPainSkill);
  REQUIRE(played.size() == 1);
  CHECK(played[0].effect_id == kMaxPainEffect);
  CHECK(played[0].bearer == GroundSkillVisualBearer::kCaster);
  CHECK_FALSE(GroundSkillLauncherCarries(played[0]));
}

TEST_CASE("Case visée : /mineffect choisit la variante, les pleins par défaut",
          "[render][groundskill]") {
  // Meteor Storm, Lord of Vermilion et Quagmire posaient leur variante allégée
  // bascule éteinte, parce que c'était celle des tables fondues.
  CHECK(OnlyEffectOf(83, false) == 92);
  CHECK(OnlyEffectOf(83, true) == 1163);
  CHECK(OnlyEffectOf(85, false) == 90);
  CHECK(OnlyEffectOf(85, true) == 1162);
  CHECK(OnlyEffectOf(92, false) == 95);
  CHECK(OnlyEffectOf(92, true) == 1161);
  // Storm Gust garde les deux siennes.
  CHECK(OnlyEffectOf(89, false) == 89);
  CHECK(OnlyEffectOf(89, true) == 708);
  // Et les tables fondues retiennent les pleins, le réglage par défaut.
  CHECK(SkillImplFor(83).EffectId() == 92);
  CHECK(SkillImplFor(85).EffectId() == 90);
  CHECK(SkillImplFor(92).EffectId() == 95);
}

TEST_CASE("Case visée : l'effet que les tables choisissent reste celui qui se joue",
          "[render][groundskill]") {
  // Bascule éteinte, l'effet des tables fondues fait partie de ce qui se pose :
  // le relevé du répartiteur ne fait que dire comment il se pose.
  constexpr std::array<std::uint16_t, 12> kChosenByTables = {21,  83,  85,  89,  91,  92,
                                                             111, 459, 694, 723, 737, 769};
  for (const std::uint16_t skill_id : kChosenByTables) {
    CAPTURE(skill_id);
    const std::uint16_t from_tables = SkillImplFor(skill_id).EffectId();
    REQUIRE(from_tables != 0);
    const std::vector<GroundSkillVisualEntry> played = PlayedOnGround(skill_id);
    CHECK(std::ranges::any_of(played, [from_tables](const GroundSkillVisualEntry& entry) {
      return entry.effect_id == from_tables;
    }));
  }
}

TEST_CASE("Case visée : la vie d'un lanceur se compte sur l'horloge de l'effet",
          "[render][groundskill]") {
  // Le lanceur vit tant que son âge n'excède pas sa vie : l'effet qu'il porte
  // le perd au premier tic qui la dépasse.
  CHECK(BearerDeathTick(0) == 0);
  CHECK(BearerDeathTick(6240) == 391);
  CHECK(BearerDeathTick(2040) == 128);
  // ⚠ ET CE TIC EST UN TIC D'EFFET, non une image : aucun effet porté par un
  // lanceur ne vieillit à chaque image.
  for (const GroundSkillVisualEntry& entry : ro::render::GroundSkillVisualTable()) {
    if (GroundSkillLauncherCarries(entry)) {
      CAPTURE(entry.skill_id, entry.effect_id);
      CHECK_FALSE(ro::render::EffectInstanceBeatsEveryFrame(entry.effect_id));
    }
  }
}

TEST_CASE("Case visée : les commandes du répartiteur se rangent sur leur effet",
          "[render][groundskill]") {
  // NC_COLDSLOWER : la case devient le point visé de 791.
  CHECK(EntryOf(2260, 791).aimed_at_cell);
  CHECK(EntryOf(2260, 791).bearer == GroundSkillVisualBearer::kCaster);
  // ABC_ABYSS_STRIKE : 2091 et 2092 attendent 880 ms, 2087 et 2088 non.
  CHECK(EntryOf(5319, 2091).str_start_delay_ms == 880);
  CHECK(EntryOf(5319, 2092).str_start_delay_ms == 880);
  CHECK(EntryOf(5319, 2087).str_start_delay_ms == 0);
  CHECK(EntryOf(5319, 2088).str_start_delay_ms == 0);
  // 774 : 1062 saute 0,4 de son fichier — la part est un flottant, et le
  // produit se tronque.
  CHECK(EntryOf(774, 1062).str_skipped_share == 0.4F);
  CHECK(ro::render::StrSkippedFrames(100, 0.4F) == 40);
  CHECK(ro::render::StrSkippedFrames(9, 0.4F) == 3);
  // HT_DETECTING envoie le faucon ; HT_SPRINGTRAP ne passe pas par ici.
  CHECK(ro::render::GroundSkillSendsFalcon(130));
  CHECK_FALSE(ro::render::GroundSkillSendsFalcon(131));
}

TEST_CASE("Case visée : les sons propres du répartiteur", "[render][groundskill]") {
  const auto file_of = [](std::uint16_t skill_id, std::int32_t level, bool minimal) {
    const std::vector<ro::render::GroundSkillSound> sounds =
        ro::render::GroundSkillSoundsToPlay(skill_id, level, minimal);
    REQUIRE(sounds.size() == 1);
    CHECK(sounds[0].hearing_range == 250.0F);
    CHECK(sounds[0].near_distance == 40.0F);
    CHECK(sounds[0].volume == 1.0F);
    return sounds[0];
  };
  CHECK(file_of(483, kAnyLevel, false).file == "effect\\EF_FireWall.wav");
  CHECK(file_of(484, kAnyLevel, false).file == "effect\\wizard_earthspike.wav");
  CHECK(file_of(710, kAnyLevel, false).file == "effect\\ef_frostdiver.wav");
  CHECK(file_of(769, kAnyLevel, false).file == "effect\\wizard_meteor.wav");
  CHECK(file_of(737, kAnyLevel, false).file == "effect\\LG_RAYOFGENESIS.wav");
  CHECK(file_of(2321, kAnyLevel, false).file == "effect\\LG_RAYOFGENESIS.wav");
  // Diamond Dust sonne sous les deux bascules.
  CHECK(file_of(2447, kAnyLevel, false).file == "effect\\wl_jackfrost.wav");
  CHECK(file_of(2447, kAnyLevel, true).file == "effect\\wl_jackfrost.wav");
  // Aid Condensed Potion : cent cinquante unités AU-DESSUS de l'origine, l'axe
  // vertical du client descendant.
  const ro::render::GroundSkillSound potion = file_of(478, kAnyLevel, false);
  CHECK(potion.file == "assulter_attack.wav");
  CHECK(potion.client_position == std::array<float, 3>{0.0F, -150.0F, 0.0F});
  CHECK(file_of(521, kAnyLevel, false).client_position == std::array<float, 3>{});
  CHECK(ro::render::GroundSkillSoundsToPlay(87, kAnyLevel, false).empty());
}

TEST_CASE("Case visée : les poses que le répartiteur mène", "[render][groundskill]") {
  // AG_STRANTUM_TREMOR, AG_TORNADO_STORM, AG_FLORAL_FLARE_ROAD : images 1 à 3
  // du rang 12, puis la 3 tenue, 500 ms.
  for (const std::uint16_t skill_id : std::array<std::uint16_t, 3>{5221, 5227, 5229}) {
    const ro::render::GroundSkillPose* pose = ro::render::GroundSkillPoseOf(skill_id, kAnyLevel);
    REQUIRE(pose != nullptr);
    CHECK(pose->play_rank == 12);
    CHECK(pose->first_frame == 1);
    CHECK(pose->last_frame == 3);
    CHECK(pose->hold_rank == 12);
    CHECK(pose->hold_frame == 3);
    CHECK(pose->hold_ms == 500);
  }
  // ABC_ABYSS_SQUARE : l'image 0 du rang 12.
  const ro::render::GroundSkillPose* square = ro::render::GroundSkillPoseOf(5321, kAnyLevel);
  REQUIRE(square != nullptr);
  CHECK(square->first_frame == 0);
  CHECK(square->hold_frame == 0);
  // Les pièges du Windhawk : images 0 à 2 du rang 3, puis l'image 2 du rang 12.
  for (const std::uint16_t skill_id : std::array<std::uint16_t, 4>{5331, 5332, 5333, 5335}) {
    const ro::render::GroundSkillPose* pose = ro::render::GroundSkillPoseOf(skill_id, kAnyLevel);
    REQUIRE(pose != nullptr);
    CHECK(pose->play_rank == 3);
    CHECK(pose->last_frame == 2);
    CHECK(pose->hold_rank == 12);
    CHECK(pose->hold_frame == 2);
  }
  CHECK(ro::render::GroundSkillPoseOf(5334, kAnyLevel) == nullptr);
}

TEST_CASE("Troupe : un porteur qui meurt emporte l'instance avant sa vie",
          "[render][groundskill][effectnode]") {
  // Les pointes d'Ice Wall vivent dix mille tics ; semées par un lanceur qui
  // meurt au tic 391, elles partent avec lui.
  constexpr std::int32_t kDeathTick = 391;
  constexpr std::int32_t kSliceTicks = 30;
  EffectNodePool pool;
  {
    const EffectNodePool::AnchoredSowing sowing{pool, 0, glm::vec3{0.0F, 0.0F, 0.0F},
                                                kIceWallEffect};
    pool.DieWithBearerAt(kDeathTick);
    PooledEffectNode carried;
    carried.node.life_ticks = ro::render::EffectNodeLifeFor(kIceWallEffect);
    pool.Add(carried);
    pool.DieWithBearerAt(0);
    PooledEffectNode free_node;
    free_node.node.life_ticks = ro::render::EffectNodeLifeFor(kIceWallEffect);
    pool.Add(free_node);
  }
  REQUIRE(pool.size() == 2);
  std::int32_t elapsed_ticks = 0;
  while (elapsed_ticks + kSliceTicks <= kDeathTick - 1) {
    pool.Advance(static_cast<std::uint32_t>(kEffectTickMs * kSliceTicks));
    elapsed_ticks += kSliceTicks;
  }
  pool.Advance(static_cast<std::uint32_t>(kEffectTickMs * (kDeathTick - 1 - elapsed_ticks)));
  CHECK(pool.size() == 2);
  pool.Advance(static_cast<std::uint32_t>(kEffectTickMs));
  CHECK(pool.size() == 1);
}
