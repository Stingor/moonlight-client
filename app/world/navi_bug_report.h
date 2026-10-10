#pragma once

#include <optional>
#include <string_view>

#include "ro/formats/navi_tables.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/navi_search.h"
#include "ro/sim/packed_position.h"

#include "game/bug_report_context.h"

namespace moonclient {

// Ce que la fenêtre de navigation montrait quand le joueur a signalé un bug.
struct NaviBugScene {
  const ro::formats::NaviData* navi = nullptr;
  // Les tables sont encore en lecture.
  bool loading = false;
  const ro::sim::NaviGuide* guide = nullptr;
  std::string_view here_map;
  std::optional<ro::sim::CellPosition> player;
  std::string_view term;
  std::string_view bound_map;
  // Le résultat choisi dans la liste, s'il y en a un.
  std::optional<ro::sim::NaviHit> selected;
};

// LE RAPPORT D'UN BUG DE NAVIGATION : où est le joueur, ce qu'il cherchait, ce
// qu'il a choisi, et ce que le guidage en a fait — l'itinéraire liaison par
// liaison, avec l'identifiant de chacune, qui suffit à rejouer la route sur les
// tables du serveur.
//
// Le libellé nomme le résultat choisi, sinon la destination du guidage.
[[nodiscard]] BugReportContext NavigationBugReport(const NaviBugScene& scene);

}  // namespace moonclient
