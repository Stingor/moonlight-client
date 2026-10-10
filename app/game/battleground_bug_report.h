#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/sim/battlefield.h"
#include "ro/sim/entry_queue.h"

#include "game/battleground_file.h"
#include "game/bug_report_context.h"

namespace moonclient {

// Ce que le joueur voyait du champ de bataille quand il a signalé un bug.
struct BattlegroundBugScene {
  const ro::sim::EntryQueue* queue = nullptr;
  const ro::sim::Battlefield* battlefield = nullptr;
  // Le terrain choisi dans la liste ; nul : aucun, ou fichier illisible.
  const BattlegroundField* field = nullptr;
  // Le fichier des terrains : lu ou non, et son nombre de terrains.
  bool table_loaded = false;
  std::size_t table_fields = 0;
  std::string_view map_name;
  bool on_battlefield = false;
  bool link_ready = false;
  std::uint32_t now_ms = 0;
};

// LE RAPPORT D'UN BUG DE CHAMP DE BATAILLE : la file telle que le client la
// suit — phase, terrain, effectif, attente, invitation —, le terrain choisi tel
// que le fichier le décrit (nom envoyé ET nom affiché), et, sur une carte de
// champ, le camp, le côté déduit, le score et le nombre de coéquipiers.
[[nodiscard]] BugReportContext BattlegroundBugReport(const BattlegroundBugScene& scene);

}  // namespace moonclient
