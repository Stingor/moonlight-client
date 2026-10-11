#include "world/navi_session.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/sim/navi_route.h"
#include "ro/vfs/navi_library.h"
#include "ro/vfs/vfs.h"

namespace moonclient {
namespace {

using ro::sim::NaviGoalKind;
using ro::sim::NaviGuideRequest;

// Adopter les tables ne doit rien attendre : on regarde si c'est prêt, c'est tout.
constexpr std::chrono::milliseconds kNoWait{0};

}  // namespace

NaviSession::~NaviSession() {
  // Le fil de lecture ne tient que sa propre archive : l'attendre ici suffit à
  // ne rien laisser tourner derrière la partie.
  if (pending_.valid()) pending_.wait();
}

void NaviSession::Open(const std::filesystem::path& client_root,
                       const ro::vfs::MountOptions& mount) {
  if (pending_.valid() || data_ != nullptr) return;
  client_root_ = client_root;
  LoadNaviPrefs(client_root_, prefs);
  pending_ = std::async(std::launch::async, [mount]() -> Loaded {
    Loaded loaded;
    auto vfs = ro::vfs::Vfs::Mount(mount);
    if (!vfs) {
      ro::core::LogWarning("navigation : archive illisible ({}) — pas de guidage cette partie",
                           ro::core::Describe(vfs.error()));
      return loaded;
    }
    ro::vfs::NaviLoadReport report;
    auto data = ro::vfs::LoadNaviData(*vfs, report);
    if (!data) {
      ro::core::LogWarning("navigation : tables illisibles ({}) — pas de guidage cette partie",
                           ro::core::Describe(data.error()));
    } else {
      ro::core::LogInfo(
          "navigation : {} cartes, {} PNJ, {} apparitions, {} liaisons — {} octets lus en {} ms, "
          "analysés en {} ms",
          data->maps.size(), data->npcs.size(), data->spawns.size(), data->links.size(),
          report.bytes, report.read_ms, report.parse_ms);
      if (!report.degraded.empty())
        ro::core::LogWarning("navigation : données incomplètes — {}", report.degraded);
      const ro::formats::NaviSkipCounts& skipped = data->skipped;
      if (skipped.npcs_on_unknown_map + skipped.links_on_unknown_map +
              skipped.spawns_on_unknown_map !=
          0)
        ro::core::LogWarning(
            "navigation : génération périmée ? {} PNJ, {} liaisons et {} apparitions sur des "
            "cartes inconnues",
            skipped.npcs_on_unknown_map, skipped.links_on_unknown_map,
            skipped.spawns_on_unknown_map);
      if (data->link_steps == ro::formats::NaviSteps::kModulo256 ||
          data->npc_steps == ro::formats::NaviSteps::kModulo256)
        ro::core::LogWarning(
            "navigation : tables de distances d'un ancien générateur (longueurs modulo 256, "
            "redressées) — à régénérer");
      loaded.data = std::make_shared<const ro::formats::NaviData>(std::move(*data));
    }
    if (auto towns = ro::vfs::TownInfo::Load(*vfs)) {
      loaded.towns = std::move(*towns);
    } else {
      ro::core::LogInfo("navigation : pas de table des commodités ({})",
                        ro::core::Describe(towns.error()));
    }
    return loaded;
  });
}

void NaviSession::Poll(std::string_view here_map) {
  if (!pending_.valid() || pending_.wait_for(kNoWait) != std::future_status::ready) return;
  Loaded loaded = pending_.get();
  towns_ = std::move(loaded.towns);
  Adopt(std::move(loaded.data), here_map);
}

void NaviSession::Adopt(std::shared_ptr<const ro::formats::NaviData> data,
                        std::string_view here_map) {
  data_ = std::move(data);
  guide.SetData(data_);
  if (const std::optional<WaitingSearch> waiting = std::exchange(waiting_search_, std::nullopt);
      waiting.has_value() && data_ != nullptr) {
    Search(waiting->term, waiting->only_map, here_map);
  }
}

void NaviSession::Search(std::string_view term, std::string_view only_map,
                         std::string_view here_map) {
  if (data_ == nullptr) {
    waiting_search_ = WaitingSearch{.term = std::string(term), .only_map = std::string(only_map)};
    return;
  }
  navigation_view.Search(*data_, term,
                         only_map.empty() ? ro::formats::kNoNaviMap : data_->FindMap(only_map),
                         data_->FindMap(here_map));
}

void NaviSession::GuideToPlace(std::string_view map_name, int cell_x, int cell_y) {
  NaviGuideRequest request;
  request.map_name = std::string(map_name);
  // Une coordonnée nulle n'est pas une case : c'est un lien vers la carte.
  const bool precise = cell_x > 0 && cell_y > 0;
  request.kind = precise ? NaviGoalKind::kCell : NaviGoalKind::kMap;
  request.cell_x = cell_x;
  request.cell_y = cell_y;
  guide.Request(std::move(request));
}

void NaviSession::GuideToNpc(std::uint32_t npc_id) {
  NaviGuideRequest request;
  request.kind = NaviGoalKind::kNpc;
  request.npc_id = npc_id;
  guide.Request(std::move(request));
}

void NaviSession::GuideFromServer(const ro::sim::NavigationRequest& asked) {
  NaviGuideRequest request;
  request.map_name = asked.map;
  // Les options viennent du SCRIPT : c'est lui qui sait si son lieu ne
  // s'atteint qu'en passant par un service.
  request.options.allow_transport_services = asked.services.transport;
  request.options.allow_airport = asked.services.airship;
  switch (asked.target) {
    case ro::sim::NavigationTarget::kCell:
    case ro::sim::NavigationTarget::kNpc:
      if (asked.cell_x > 0 && asked.cell_y > 0) {
        request.kind = NaviGoalKind::kCell;
        request.cell_x = asked.cell_x;
        request.cell_y = asked.cell_y;
      }
      break;
    case ro::sim::NavigationTarget::kMap:
      break;
    case ro::sim::NavigationTarget::kMonster:
      // Un monstre n'a pas de case : sa carte, ou à défaut la première où il
      // apparaît.
      if (request.map_name.empty() && data_ != nullptr) {
        for (const ro::formats::NaviSpawn& spawn : data_->spawns) {
          if (spawn.sprite_class != asked.monster_id || spawn.map >= data_->maps.size()) continue;
          request.map_name = data_->maps[spawn.map].name;
          break;
        }
      }
      break;
  }
  guide.Request(std::move(request));
}

std::span<const NaviMemo> NaviSession::MemosOn(std::string_view map_name) const {
  const auto found = prefs.memos.find(map_name);
  if (found == prefs.memos.end()) return {};
  return found->second;
}

void NaviSession::AddMemo(std::string_view map_name, NaviMemo memo) {
  if (map_name.empty()) return;
  auto& memos = prefs.memos[std::string(map_name)];
  if (memos.size() >= static_cast<std::size_t>(ro::ui::kMinimapMemoCap)) return;
  memos.push_back(std::move(memo));
  SavePrefs();
}

void NaviSession::RemoveMemo(std::string_view map_name, std::size_t rank) {
  const auto found = prefs.memos.find(map_name);
  if (found == prefs.memos.end() || rank >= found->second.size()) return;
  found->second.erase(found->second.begin() + static_cast<std::ptrdiff_t>(rank));
  // Une carte vidée de ses marqueurs ne laisse pas d'entrée derrière elle.
  if (found->second.empty()) prefs.memos.erase(found);
  SavePrefs();
}

void NaviSession::SavePrefs() const { static_cast<void>(SaveNaviPrefs(client_root_, prefs)); }

}  // namespace moonclient
