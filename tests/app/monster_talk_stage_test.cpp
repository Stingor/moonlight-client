// L'ÉTAGE DE LA PAROLE DES MONSTRES, trame par trame : la réplique d'un coup,
// la cible mémorisée, ce qui ne fait pas parler, et `/notalkmsg` qui tait la
// ligne.

#include "game/monster_talk_stage.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/actor.h"
#include "ro/sim/talk_mute.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "support/net_frames.h"
#include "support/temp_directory.h"

using moonclient::MonsterTalkStage;
using ro::net::Frame;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kMonster = 110000001;
constexpr std::uint32_t kHero = 2000001;
constexpr std::uint32_t kSidekick = 2000002;
// Une classe que le catalogue vide nomme « Poring », comme tout inconnu.
constexpr std::uint32_t kPoringClass = 1002;
constexpr std::int32_t kSomeDamage = 12;
constexpr std::uint32_t kNowMs = 5000;
constexpr std::string_view kLine = "Poring : Boing !";

constexpr std::string_view kTable = "version: 1\nspecies:\n  poring:\n    attack: [Boing !]\n";

[[nodiscard]] Frame Strike(std::uint32_t source_id, std::uint32_t target_id,
                           std::int32_t damage = kSomeDamage) {
  ro::proto::ZcNotifyAct packet;
  packet.srcID = static_cast<std::int32_t>(source_id);
  packet.targetID = static_cast<std::int32_t>(target_id);
  packet.damage = damage;
  return FrameOf(Encode(packet));
}

[[nodiscard]] Frame Vanish(std::uint32_t actor_id) {
  ro::proto::ZcNotifyVanish packet;
  packet.gid = actor_id;
  return FrameOf(Encode(packet));
}

[[nodiscard]] World WorldWithMonster(std::string name = "Poring") {
  World world;
  ro::sim::Actor monster;
  monster.id = kMonster;
  monster.name = std::move(name);
  monster.look.job = kPoringClass;
  world.Spawn(monster);
  return world;
}

struct Bench {
  TempDirectory client;
  ro::render::SpriteCatalog catalog;
  MonsterTalkStage stage;
  bool resting = true;

  explicit Bench(bool with_table = true) {
    client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
    if (with_table) {
      client.WriteText("data/yaml_files/monster_talk.yml", std::string(kTable));
    }
    auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(vfs.has_value());
    stage.LoadTable(*vfs);
    stage.SeeCatalog(&catalog);
  }

  void Note(const Frame& frame, World& world) {
    stage.NoteBeforeWorld(frame, world, [this](const ro::sim::Actor&) { return resting; }, kNowMs);
  }
};

[[nodiscard]] std::size_t LinesOf(const World& world) { return world.chat_lines().size(); }

[[nodiscard]] std::string BubbleOf(const World& world) {
  const ro::sim::Actor* monster = world.Find(kMonster);
  return monster != nullptr ? monster->chat_text : std::string();
}

}  // namespace

TEST_CASE("Parole des monstres : un coup sur une cible neuve dit la réplique",
          "[app][monstertalk]") {
  Bench bench;
  World world = WorldWithMonster();
  bench.Note(Strike(kMonster, kHero), world);
  REQUIRE(LinesOf(world) == 1);
  CHECK(world.chat_lines().back().text == kLine);
  CHECK(world.chat_lines().back().speaker_id == kMonster);
  CHECK(BubbleOf(world) == kLine);
  // La même cible : rien de plus. Une autre : encore.
  bench.Note(Strike(kMonster, kHero), world);
  CHECK(LinesOf(world) == 1);
  bench.Note(Strike(kMonster, kSidekick), world);
  CHECK(LinesOf(world) == 2);
}

TEST_CASE("Parole des monstres : /notalkmsg tait la ligne, pas la bulle", "[app][monstertalk]") {
  Bench bench;
  ro::sim::TalkMute mute;
  mute.talk_hidden = true;
  bench.stage.SeeTalkMute(&mute);
  World world = WorldWithMonster();
  bench.Note(Strike(kMonster, kHero), world);
  CHECK(LinesOf(world) == 0);
  CHECK(BubbleOf(world) == kLine);
}

TEST_CASE("Parole des monstres : ce qui ne fait pas parler", "[app][monstertalk]") {
  SECTION("un acteur qui n'est pas au repos, mais mémorise sa cible") {
    Bench bench;
    World world = WorldWithMonster();
    bench.resting = false;
    bench.Note(Strike(kMonster, kHero), world);
    bench.resting = true;
    bench.Note(Strike(kMonster, kHero), world);
    CHECK(LinesOf(world) == 0);
  }
  SECTION("un coup annulé") {
    Bench bench;
    World world = WorldWithMonster();
    bench.Note(Strike(kMonster, kHero, ro::sim::kVoidStrikeDamage), world);
    CHECK(LinesOf(world) == 0);
  }
  SECTION("un acteur sans nom") {
    Bench bench;
    World world = WorldWithMonster("");
    bench.Note(Strike(kMonster, kHero), world);
    CHECK(LinesOf(world) == 0);
  }
  SECTION("sans table") {
    Bench bench(false);
    World world = WorldWithMonster();
    bench.Note(Strike(kMonster, kHero), world);
    CHECK(LinesOf(world) == 0);
  }
}

TEST_CASE("Parole des monstres : sorti de la vue, l'acteur oublie sa cible", "[app][monstertalk]") {
  Bench bench;
  World world = WorldWithMonster();
  bench.Note(Strike(kMonster, kHero), world);
  bench.Note(Vanish(kMonster), world);
  bench.Note(Strike(kMonster, kHero), world);
  CHECK(LinesOf(world) == 2);
}
