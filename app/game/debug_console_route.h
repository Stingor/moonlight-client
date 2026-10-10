#pragma once

#include <cstdint>

#include "ro/net/frame_tally.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/staff_tools.h"

#include "debug/debug_console.h"
#include "game/companion_ai_stage.h"

namespace moonclient {

// UNE IMAGE DE LA CONSOLE DE MISE AU POINT : allumée par la case des outils du
// staff — et par le droit, revérifié à chaque image —, elle répond sur ce que
// le client sait, puis dit son état à la case.
void ServeDebugConsole(DebugConsole& console, ro::ui::StaffToolsState& staff_tools,
                       bool staff_allowed, const ro::sim::World& world,
                       const ro::net::FrameTally& frames, const CompanionAiStage& companions,
                       std::uint32_t now_ms);

}  // namespace moonclient
