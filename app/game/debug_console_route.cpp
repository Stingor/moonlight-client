#include "game/debug_console_route.h"

#include <array>
#include <optional>

#include "ro/sim/world.h"

namespace moonclient {

void ServeDebugConsole(DebugConsole& console, ro::ui::StaffToolsState& staff_tools,
                       bool staff_allowed, const ro::sim::World& world,
                       const ro::net::FrameTally& frames, const CompanionAiStage& companions,
                       std::uint32_t now_ms) {
  using ro::sim::AiCompanion;
  const std::array<DebugCompanion, 2> seen = {
      DebugCompanion{.name = "homunculus",
                     .actor_id = companions.actor_of(AiCompanion::kHomunculus),
                     .brain = &companions.brain_of(AiCompanion::kHomunculus)},
      DebugCompanion{.name = "mercenary",
                     .actor_id = companions.actor_of(AiCompanion::kMercenary),
                     .brain = &companions.brain_of(AiCompanion::kMercenary)}};
  console.Serve(
      staff_tools.debug_console && staff_allowed,
      DebugSources{.world = &world, .frames = &frames, .companions = seen, .now_ms = now_ms});
  const std::optional<std::uint16_t> port = console.port();
  staff_tools.debug_console_port = port.has_value() ? *port : 0;
  staff_tools.debug_console_connected = console.connected();
  staff_tools.debug_console_failure = console.failure();
}

}  // namespace moonclient
