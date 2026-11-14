// LA CONSOLE DE MISE AU POINT : l'écrivain JSON, la lecture d'une ligne de
// requête, et ce que les requêtes répondent sur un monde construit ici.

#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/frame_tally.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "debug/debug_json.h"
#include "debug/debug_queries.h"
#include "debug/debug_request.h"

using moonclient::AnswerDebugRequest;
using moonclient::DebugRequest;
using moonclient::DebugSources;
using moonclient::JsonWriter;
using moonclient::ParseDebugRequest;

namespace {

constexpr std::uint32_t kPlayerId = 100;
constexpr std::uint32_t kPupaId = 3032616;
constexpr std::uint32_t kFarId = 300;
constexpr std::uint16_t kPupaJob = 1008;
constexpr std::uint16_t kCenter = 100;
constexpr std::uint16_t kVanishOpcode = 0x0080;
constexpr std::uint16_t kOtherOpcode = 0x0087;

void Place(ro::sim::World& world, std::uint32_t id, ro::sim::ActorKind kind, std::uint16_t x) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.look.job = kPupaJob;
  actor.position = ro::sim::CellPosition{x, kCenter, 0};
  world.Spawn(actor);
}

[[nodiscard]] std::string Ask(const DebugSources& sources, const char* line) {
  const std::optional<DebugRequest> request = ParseDebugRequest(line);
  REQUIRE(request.has_value());
  return AnswerDebugRequest(*request, sources);
}

[[nodiscard]] bool Has(const std::string& text, const char* piece) {
  return text.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Console : l'écrivain JSON pose virgules et échappements", "[app][debug]") {
  JsonWriter out;
  out.BeginObject()
      .Key("a")
      .Int(1)
      .Key("b")
      .BeginArray()
      .Bool(true)
      .Null()
      .String("x\"y\n")
      .EndArray()
      .Key("c")
      .String("é")
      .EndObject();
  CHECK(out.text() == "{\"a\":1,\"b\":[true,null,\"x\\\"y\\n\"],\"c\":\"é\"}");
}

TEST_CASE("Console : une ligne de requête se lit, une ligne fausse se refuse", "[app][debug]") {
  const std::optional<DebugRequest> request =
      ParseDebugRequest("7 actors radius=20 kind=monster\r");
  REQUIRE(request.has_value());
  CHECK(request->id == 7);
  CHECK(request->method == "actors");
  CHECK(request->IntParam("radius", 0) == 20);
  CHECK(request->TextParam("kind") == "monster");
  CHECK(request->IntParam("absent", 5) == 5);
  CHECK_FALSE(ParseDebugRequest("actors").has_value());
  CHECK_FALSE(ParseDebugRequest("x actors").has_value());
}

TEST_CASE("Console : état, acteurs filtrés, acteur, trames par AID", "[app][debug]") {
  ro::sim::World world;
  Place(world, kPlayerId, ro::sim::ActorKind::kPlayer, kCenter);
  world.set_player_id(kPlayerId);
  Place(world, kPupaId, ro::sim::ActorKind::kMonster, kCenter + 3);
  Place(world, kFarId, ro::sim::ActorKind::kMonster, kCenter + 30);

  ro::net::FrameTally frames;
  // Une sortie de vue qui nomme le Pupa (AID petit-boutiste après l'opcode),
  // et une trame qui ne le nomme pas.
  ro::net::Frame vanish{.opcode = kVanishOpcode,
                        .bytes = {0x80, 0x00, 0x28, 0x46, 0x2E, 0x00, 0x00}};
  frames.Record(vanish, ro::net::FrameOutcome::kApplied);
  frames.Record(ro::net::Frame{.opcode = kOtherOpcode, .bytes = {0x87, 0x00, 0x01}},
                ro::net::FrameOutcome::kApplied);

  const DebugSources sources{.world = &world, .frames = &frames};
  const std::string state = Ask(sources, "1 state");
  CHECK(Has(state, "\"id\":1,\"ok\":true"));
  CHECK(Has(state, "\"actor_count\":3"));

  // RAYON DE DIX : le Pupa à trois cases, pas celui à trente.
  const std::string near = Ask(sources, "2 actors radius=10 kind=monster");
  CHECK(Has(near, "\"total\":1"));
  CHECK(Has(near, "\"id\":3032616"));
  CHECK(Has(near, "\"dx\":3"));

  CHECK(Has(Ask(sources, "3 actor id=3032616"), "\"job\":1008"));
  CHECK(Has(Ask(sources, "4 actor id=999"), "\"ok\":false"));

  // LA TRAME QUI NOMME CET AID, et elle seule.
  const std::string named = Ask(sources, "5 frames aid=3032616");
  CHECK(Has(named, "\"opcode\":\"0x0080\""));
  CHECK_FALSE(Has(named, "0x0087"));
  CHECK(Has(Ask(sources, "6 frames opcode=0x0087"), "\"opcode\":\"0x0087\""));

  CHECK(Has(Ask(sources, "7 nope"), "\"ok\":false"));
}
