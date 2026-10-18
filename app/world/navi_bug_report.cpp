#include "world/navi_bug_report.h"

#include <array>
#include <cstdio>
#include <format>
#include <string>

#include "ro/core/i18n.h"
#include "ro/net/bug_report.h"

namespace moonclient {
namespace {

using ro::formats::NaviData;
using ro::sim::NaviGoalKind;
using ro::sim::NaviGuideStatus;
using ro::sim::NaviHitKind;

[[nodiscard]] std::string_view MapName(const NaviData& data, ro::formats::NaviMapIndex map) {
  return map < data.maps.size() ? std::string_view(data.maps[map].name) : std::string_view();
}

[[nodiscard]] const char* StatusWord(NaviGuideStatus status) {
  switch (status) {
    case NaviGuideStatus::kIdle:
      return "idle";
    case NaviGuideStatus::kPending:
      return "pending";
    case NaviGuideStatus::kGuiding:
      return "guiding";
    case NaviGuideStatus::kNoRoute:
      return "no_route";
    case NaviGuideStatus::kAlreadyHere:
      return "already_here";
    case NaviGuideStatus::kArrived:
      return "arrived";
  }
  return "?";
}

[[nodiscard]] const char* GoalWord(NaviGoalKind kind) {
  switch (kind) {
    case NaviGoalKind::kMap:
      return "map";
    case NaviGoalKind::kCell:
      return "cell";
    case NaviGoalKind::kNpc:
      return "npc";
  }
  return "?";
}

// Le résultat choisi, en membre JSON ; vide quand rien ne l'est.
[[nodiscard]] std::string SelectedFields(const NaviData& data, const ro::sim::NaviHit& hit) {
  switch (hit.kind) {
    case NaviHitKind::kMap:
      if (hit.rank >= data.maps.size()) return {};
      return std::format(R"("selected":{{"kind":"map","map":"{}"}})",
                         JsonEscaped(data.maps[hit.rank].name));
    case NaviHitKind::kNpc: {
      if (hit.rank >= data.npcs.size()) return {};
      const ro::formats::NaviNpc& npc = data.npcs[hit.rank];
      return std::format(
          R"("selected":{{"kind":"npc","npc_id":{},"name":"{}","map":"{}","x":{},"y":{},"sprite_class":{}}})",
          npc.npc_id, JsonEscaped(npc.name), JsonEscaped(MapName(data, npc.map)), npc.cell_x,
          npc.cell_y, npc.sprite_class);
    }
    case NaviHitKind::kSpawn: {
      if (hit.rank >= data.spawns.size()) return {};
      const ro::formats::NaviSpawn& spawn = data.spawns[hit.rank];
      return std::format(
          R"("selected":{{"kind":"monster","spawn_id":{},"name":"{}","map":"{}","sprite_class":{},"amount":{}}})",
          spawn.spawn_id, JsonEscaped(spawn.name), JsonEscaped(MapName(data, spawn.map)),
          spawn.sprite_class, spawn.amount);
    }
  }
  return {};
}

// Le guidage : la demande telle qu'elle a été faite, puis l'itinéraire.
[[nodiscard]] std::string GuideFields(const NaviData* data, const ro::sim::NaviGuide& guide) {
  const ro::sim::NaviGuideRequest& asked = guide.request();
  std::string out = std::format(
      R"("guide":{{"status":"{}","goal":{{"kind":"{}","map":"{}","x":{},"y":{},"npc_id":{}}},"transport":{},"airport":{},"current_step":{},"estimated_cost":{})",
      StatusWord(guide.status()), GoalWord(asked.kind), JsonEscaped(asked.map_name), asked.cell_x,
      asked.cell_y, asked.npc_id, asked.options.allow_transport_services,
      asked.options.allow_airport, guide.current_step(), guide.estimated_cost());
  out += R"(,"steps":[)";
  bool first = true;
  for (const ro::sim::NaviGuideStep& step : guide.steps()) {
    if (!first) out += ',';
    first = false;
    out += std::format(R"({{"link_id":{},"kind":{},"from":"{}","to":"{}","x":{},"y":{}}})",
                       step.link_id, step.link_kind,
                       data != nullptr ? JsonEscaped(MapName(*data, step.from_map)) : std::string(),
                       data != nullptr ? JsonEscaped(MapName(*data, step.to_map)) : std::string(),
                       step.exit_x, step.exit_y);
  }
  out += ']';
  if (const auto target = guide.local_target())
    out += std::format(R"(,"local_target":{{"x":{},"y":{}}})", target->x, target->y);
  out += std::format(R"(,"path_cells":{}}})", guide.path_cells().size());
  return out;
}

}  // namespace

BugReportContext NavigationBugReport(const NaviBugScene& scene) {
  std::string fields = std::format(R"("navigation":{{"tables":{{"loading":{})", scene.loading);
  if (scene.navi != nullptr)
    fields += std::format(R"(,"maps":{},"npcs":{},"links":{})", scene.navi->maps.size(),
                          scene.navi->npcs.size(), scene.navi->links.size());
  fields += '}';
  fields += std::format(R"(,"here":{{"map":"{}")", JsonEscaped(scene.here_map));
  if (scene.player.has_value())
    fields += std::format(R"(,"x":{},"y":{})", scene.player->x, scene.player->y);
  fields += '}';
  fields += std::format(R"(,"search":{{"term":"{}","bound_map":"{}"}})", JsonEscaped(scene.term),
                        JsonEscaped(scene.bound_map));
  if (scene.navi != nullptr && scene.selected.has_value()) {
    const std::string selected = SelectedFields(*scene.navi, *scene.selected);
    if (!selected.empty()) {
      fields += ',';
      fields += selected;
    }
  }
  if (scene.guide != nullptr) {
    fields += ',';
    fields += GuideFields(scene.navi, *scene.guide);
  }
  fields += '}';

  // Le libellé : ce qu'on regardait, sinon où l'on allait.
  std::string subject;
  if (scene.navi != nullptr && scene.selected.has_value()) {
    const NaviData& data = *scene.navi;
    const ro::sim::NaviHit& hit = *scene.selected;
    if (hit.kind == NaviHitKind::kMap && hit.rank < data.maps.size()) {
      subject = data.maps[hit.rank].name;
    } else if (hit.kind == NaviHitKind::kNpc && hit.rank < data.npcs.size()) {
      const ro::formats::NaviNpc& npc = data.npcs[hit.rank];
      subject =
          std::format("{} ({} {},{})", npc.name, MapName(data, npc.map), npc.cell_x, npc.cell_y);
    } else if (hit.kind == NaviHitKind::kSpawn && hit.rank < data.spawns.size()) {
      const ro::formats::NaviSpawn& spawn = data.spawns[hit.rank];
      subject = std::format("{} ({})", spawn.name, MapName(data, spawn.map));
    }
  }
  if (subject.empty() && scene.guide != nullptr && scene.guide->active() && scene.navi != nullptr)
    subject = MapName(*scene.navi, scene.guide->goal_map());
  std::array<char, kBugReportLabelBytes> label{};
  if (subject.empty()) {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Navigation"));
  } else {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Navigation : %s"), subject.c_str());
  }
  BugReportContext report = FieldsBugReport(label.data(), fields);
  report.category = static_cast<std::uint8_t>(ro::net::BugReportCategory::kNavigation);
  return report;
}

}  // namespace moonclient
