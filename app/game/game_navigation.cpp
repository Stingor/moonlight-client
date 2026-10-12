// LA MINIMAP ET LA NAVIGATION, vues de la boucle : les demandes du serveur, ce
// que les deux fenêtres ont rendu à l'image précédente, le guidage qui suit le
// joueur, puis les vues qu'elles dessineront.
//
// ⚠ À CÔTÉ DE LA BOUCLE : un sujet entier — un graphe lu en arrière-plan, un
// guidage, deux fenêtres et les portes qui y mènent.

#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/render/skin_pieces.h"
#include "ro/sim/movement.h"
#include "ro/sim/navi_search.h"

#include "chat/chat_monster_link.h"
#include "chat/chat_place_link.h"
#include "game/bug_report_context.h"
#include "game/game.h"
#include "world/battlefield_view.h"
#include "world/map_stage.h"
#include "world/minimap_compose.h"
#include "world/navi_bug_report.h"
#include "world/navigation_compose.h"
#include "world/world_map_routes.h"

namespace moonclient {
namespace {

constexpr std::string_view kGatExtension = ".gat";
// La racine des textures du client, où vivent les flèches de la Kafra.
constexpr std::string_view kTextureRoot = "data\\texture\\";

// La ligne choisie dans la fenêtre ; une référence hors liste quand rien ne
// l'est, que `HitAt` refuse.
[[nodiscard]] ro::ui::NavigationRowRef SelectedRow(const ro::ui::NavigationWindowState& state) {
  if (state.selected_group < 0 || state.selected_entry < 0)
    return {.group = static_cast<std::size_t>(-1), .entry = static_cast<std::size_t>(-1)};
  return {.group = static_cast<std::size_t>(state.selected_group),
          .entry = static_cast<std::size_t>(state.selected_entry)};
}

}  // namespace

void Game::AdvanceNavigation(std::uint32_t now_ms) {
  navi_.Poll(world_.map_name());
  UiStage::Windows::Minimap& minimap = windows_.minimap;
  UiStage::Windows::Navigation& navigation = windows_.navigation;
  const ro::ui::MinimapAnswer mini = std::exchange(minimap.answer, {});
  const ro::ui::NavigationAnswer asked = std::exchange(navigation.answer, {});
  const ro::formats::NaviData* data = navi_.data();
  const std::string& here = world_.map_name();
  NavigationComposer& results = navi_.navigation_view;
  // Un lieu partagé par sa balise `<NAVIL>`. Sans coordonnées, la carte entière.
  const auto share_place = [this](const std::string& map_name, int cell_x, int cell_y) {
    PostChatLink(PlaceLink{.map = map_name, .x = cell_x, .y = cell_y});
  };

  // ── Ce que le serveur demande (`navigateto`) ───────────────────────────────
  while (const std::optional<ro::sim::NavigationRequest> request =
             navi_.guidance.navigation.Take()) {
    navi_.GuideFromServer(*request);
    if (!request->hide_window) {
      navigation.state.open = true;
      navigation.state.raise = true;
    }
  }

  // ── Ce que la minimap a rendu ──────────────────────────────────────────────
  if (mini.guide_to.has_value()) navi_.GuideToPlace(here, mini.guide_to->x, mini.guide_to->y);
  if (mini.add_memo.has_value())
    navi_.AddMemo(here, NaviMemo{.cell_x = mini.add_memo->cell.x,
                                 .cell_y = mini.add_memo->cell.y,
                                 .name = mini.add_memo->name});
  if (mini.remove_memo.has_value()) navi_.RemoveMemo(here, *mini.remove_memo);
  if (mini.warp_to.has_value())
    SendStaffWarp(PlaceLink{.map = here, .x = mini.warp_to->x, .y = mini.warp_to->y}, Rights(),
                  link_, world_);
  if (mini.open_mvp_slot.has_value()) pieces_.mvp_tracker.RequestOpenOn(*mini.open_mvp_slot);
  if (mini.open_navigation) {
    navigation.state.open = true;
    navigation.state.raise = true;
  }
  if (mini.open_world_map) {
    if (pieces_.world_view.has_value()) {
      navigation.world_map.open = true;
      navigation.world_map.raise = true;
    } else {
      // Les tables manquent à l'installation : on le dit plutôt qu'un bouton
      // qui ne ferait rien.
      world_.RecordChat(ro::sim::ChatKind::kServer, 0,
                        ro::core::Tr("La carte du monde manque à l'installation."), now_ms);
    }
  }
  // ── Ce que la carte du monde a rendu ───────────────────────────────────────
  const ro::ui::WorldMapAction world_asked = std::exchange(navigation.world_map_answer, {});
  if (world_asked.world.has_value()) navigation.world_map.world = *world_asked.world;
  if (!world_asked.guide_to.empty()) navi_.GuideToPlace(world_asked.guide_to, 0, 0);
  if (!world_asked.warp_to.empty())
    SendStaffWarp(PlaceLink{.map = world_asked.warp_to}, Rights(), link_, world_);
  // ⚠ LES RÉGLAGES DE LA MINIMAP NE S'ÉCRIVENT PAS ICI : ils sont au profil de
  // disposition, dont le gardien relève lui-même chaque changement.

  // ── Ce que la navigation a rendu ───────────────────────────────────────────
  const ro::formats::NaviMapIndex here_index =
      data != nullptr ? data->FindMap(here) : ro::formats::kNoNaviMap;
  if (data != nullptr) {
    const ro::formats::NaviMapIndex bound = navigation.state.bound_map.empty()
                                                ? ro::formats::kNoNaviMap
                                                : data->FindMap(navigation.state.bound_map);
    if (asked.term.has_value()) {
      if (asked.term->empty())
        results.Clear();
      else
        results.Search(*data, *asked.term, bound, here_index);
    } else if (asked.unbound) {
      results.Search(*data, navigation.state.term.data(), ro::formats::kNoNaviMap, here_index);
    }
    if (asked.show_here) results.ShowContents(*data, here_index);
    if (asked.show_map_of.has_value()) {
      if (const auto hit = results.HitAt(*asked.show_map_of)) {
        ro::formats::NaviMapIndex map = ro::formats::kNoNaviMap;
        if (hit->kind == ro::sim::NaviHitKind::kMap) map = static_cast<std::uint16_t>(hit->rank);
        if (hit->kind == ro::sim::NaviHitKind::kNpc) map = data->npcs[hit->rank].map;
        if (hit->kind == ro::sim::NaviHitKind::kSpawn) map = data->spawns[hit->rank].map;
        results.ShowContents(*data, map);
      }
    }
    if (asked.go_to.has_value()) {
      if (const auto hit = results.HitAt(*asked.go_to)) {
        switch (hit->kind) {
          case ro::sim::NaviHitKind::kMap:
            navi_.GuideToPlace(data->maps[hit->rank].name, 0, 0);
            break;
          case ro::sim::NaviHitKind::kNpc:
            navi_.GuideToNpc(data->npcs[hit->rank].npc_id);
            break;
          case ro::sim::NaviHitKind::kSpawn:
            navi_.GuideToPlace(data->maps[data->spawns[hit->rank].map].name, 0, 0);
            break;
        }
      }
    }
    for (const std::optional<ro::ui::NavigationRowRef>& row : {asked.share, asked.link_to_chat}) {
      if (!row.has_value()) continue;
      const auto hit = results.HitAt(*row);
      if (!hit.has_value()) continue;
      if (hit->kind == ro::sim::NaviHitKind::kSpawn) {
        const ro::formats::NaviSpawn& spawn = data->spawns[hit->rank];
        PostChatLink(MonsterLink{.monster_id = spawn.sprite_class,
                                 .rank = spawn.is_boss() ? kMonsterRankMvp : kMonsterRankOrdinary,
                                 .name = spawn.name});
        continue;
      }
      std::string map;
      int x = 0;
      int y = 0;
      if (hit->kind == ro::sim::NaviHitKind::kMap) {
        map = data->maps[hit->rank].name;
      } else {
        const ro::formats::NaviNpc& npc = data->npcs[hit->rank];
        map = data->maps[npc.map].name;
        x = npc.cell_x;
        y = npc.cell_y;
      }
      share_place(map, x, y);
    }
    if (asked.warp_to.has_value()) {
      if (const auto hit = results.HitAt(*asked.warp_to)) {
        PlaceLink place;
        if (hit->kind == ro::sim::NaviHitKind::kMap) place.map = data->maps[hit->rank].name;
        if (hit->kind == ro::sim::NaviHitKind::kSpawn)
          place.map = data->maps[data->spawns[hit->rank].map].name;
        if (hit->kind == ro::sim::NaviHitKind::kNpc) {
          const ro::formats::NaviNpc& npc = data->npcs[hit->rank];
          place = PlaceLink{.map = data->maps[npc.map].name, .x = npc.cell_x, .y = npc.cell_y};
        }
        SendStaffWarp(place, Rights(), link_, world_);
      }
    }
    if (asked.open_monster.has_value() && monster_sheet_.has_value()) {
      if (const auto hit = results.HitAt(*asked.open_monster);
          hit.has_value() && hit->kind == ro::sim::NaviHitKind::kSpawn)
        monster_sheet_->Open(windows_.monster_sheet.state, data->spawns[hit->rank].sprite_class,
                             /*by_view=*/true);
    }
  }
  // « Copier le nom » : le nom INTERNE d'une carte, celui qu'une commande
  // accepte ; celui d'un PNJ ou d'un monstre tel que la liste le montre.
  if (data != nullptr && asked.copy_name.has_value()) {
    if (const auto hit = results.HitAt(*asked.copy_name)) {
      const std::string* name = nullptr;
      if (hit->kind == ro::sim::NaviHitKind::kMap) name = &data->maps[hit->rank].name;
      if (hit->kind == ro::sim::NaviHitKind::kNpc) name = &data->npcs[hit->rank].name;
      if (hit->kind == ro::sim::NaviHitKind::kSpawn) name = &data->spawns[hit->rank].name;
      if (name != nullptr) ImGui::SetClipboardText(name->c_str());
    }
  }
  // LE MENU D'UN MONSTRE DE LA LISTE, joué sur la ligne et non sur un jeton :
  // elle n'appartient à aucune table de liens. ⚠ SON IDENTIFIANT EST UNE CLASSE
  // DE SPRITE, celle des données de navigation : la fiche s'ouvre comme celle
  // d'un acteur.
  if (asked.monster_link.gesture != ro::ui::ChatLinkGesture::kNothing)
    FollowMonsterGesture(
        asked.monster_link.gesture,
        MonsterLink{.monster_id = asked.monster_link.item_id,
                    .rank = asked.monster_is_mvp ? kMonsterRankMvp : kMonsterRankOrdinary,
                    .name = asked.monster_name},
        /*by_view=*/true, now_ms);
  if (asked.report_bug) {
    NaviBugScene scene{.navi = data,
                       .loading = navi_.loading(),
                       .guide = &navi_.guide,
                       .here_map = here,
                       .term = navigation.state.term.data(),
                       .bound_map = navigation.state.bound_map,
                       .selected = results.HitAt(SelectedRow(navigation.state))};
    if (const ro::sim::Actor* self = world_.player(); self != nullptr)
      scene.player = ro::sim::StandingCell(*self);
    AskBugReport(windows_.bug_report, NavigationBugReport(scene));
  }
  if (asked.stop_guidance) navi_.guide.Stop();
  if (asked.trail_icon.has_value()) {
    navi_.prefs.trail_icon = *asked.trail_icon;
    navi_.SavePrefs();
  }
  navigation.state.trail_icon = navi_.prefs.trail_icon;
  const bool staff_warp = Rights().staff_tools;
  navigation.state.staff_warp = staff_warp;
  minimap.state.staff_warp = staff_warp;
  navigation.world_map.staff_warp = staff_warp;

  // ── Le guidage suit le joueur ──────────────────────────────────────────────
  const ro::sim::Actor* player = world_.player();
  const ro::sim::WalkMap* walk = stage_.has_value() ? PlannedMapOf(*stage_) : nullptr;
  if (player != nullptr && !here.empty())
    navi_.guide.Update(here, walk, ro::sim::StandingCell(*player));

  // ── Les vues ───────────────────────────────────────────────────────────────
  const MinimapImageAt image_at = [this](std::string_view path) -> ro::ui::SkinImage {
    if (!pieces_.game_images.has_value()) return {};
    std::string full(ro::render::kSkinRoot);
    full += path;
    const ro::render::IconTexture& icon = pieces_.game_images->GetOurs(full);
    if (!icon.usable()) return {};
    return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                             .width = static_cast<float>(icon.width_px),
                             .height = static_cast<float>(icon.height_px)};
  };
  std::string_view here_label;
  if (pieces_.map_display_names.has_value() && !here.empty())
    here_label = pieces_.map_display_names->DisplayNameOf(here + std::string(kGatExtension))
                     .value_or(std::string_view());

  ro::ui::MinimapLayout& minimap_layout = shell_.layouts().minimap();
  minimap.settings = &minimap_layout.settings;
  if (minimap_layout.shown && !here.empty()) {
    MinimapSources sources;
    sources.world = &world_;
    sources.now_ms = now_ms;
    sources.cells_width = walk != nullptr ? walk->width() : 0;
    sources.cells_height = walk != nullptr ? walk->height() : 0;
    sources.display_name = here_label;
    sources.towns = navi_.towns();
    sources.navi = data;
    sources.party = &party_;
    sources.guild = &guild_book_;
    sources.viewpoints = &navi_.guidance.viewpoints;
    sources.boss = navi_.guidance.boss;
    sources.guide = minimap_layout.settings.show_route ? &navi_.guide : nullptr;
    sources.memos = navi_.MemosOn(here);
    sources.store_search = &pieces_.trade.store_search().search();
    // LES TOMBES DU CARNET DE CHASSE, que son interrupteur et son réglage
    // éteignent ensemble.
    const MvpTrackerSettings hunt = settings_->options().pages().mvp_tracker.settings();
    sources.tombs =
        pieces_.mvp_tracker.TombsOn(here, hunt.enabled && hunt.show_tombs, LocalUnixSeconds());
    // LES COÉQUIPIERS DU CHAMP DE BATAILLE, dans la couleur de notre camp.
    sources.battle_mates_shown = settings_->options().pages().battleground.settings().minimap_mates;
    sources.battle_mate_color = BattleSideColor(world_.battlefield().own_side());
    sources.image_at = image_at;
    minimap.view = &navi_.minimap_view.Compose(sources);
  } else {
    minimap.view = nullptr;
  }
  // ── La carte du monde ──────────────────────────────────────────────────────
  if (navigation.world_map.open && pieces_.world_view.has_value()) {
    const std::vector<std::string> route = WorldRouteMaps(&navi_.guide);
    const std::string step_map = WorldRouteStepMap(&navi_.guide);
    const ro::sim::Actor* self = world_.Find(world_.player_id());
    std::vector<std::string> party_maps;
    const std::vector<WorldMapMember> party = WorldMapPartyMembers(
        party_, world_.player_id(),
        self != nullptr ? std::string_view(self->name) : std::string_view(), here, party_maps);
    // LA KAFRA de la ville désignée, si elle en a une. La garde-robe se vide à
    // chaque entrée de carte : la Kafra se recompose alors de zéro.
    const ro::vfs::KafraMoveMaps* kafra_maps =
        pieces_.kafra_move_maps.has_value() ? &*pieces_.kafra_move_maps : nullptr;
    WorldMapKafra::Shown kafra_shown;
    if (kafra_maps != nullptr && pieces_.wardrobe.has_value() &&
        !kafra_maps->DestinationsOf(navigation.world_map.hovered_map).empty()) {
      if (!navi_.kafra.has_value() || navi_.kafra_map_entries != world_.map_entries()) {
        navi_.kafra.emplace(*pieces_.wardrobe);
        navi_.kafra_map_entries = world_.map_entries();
      }
      kafra_shown =
          navi_.kafra->Compose(WorldMapKafraScale(navigation.world_map.size_tier), now_ms);
    }
    const MinimapImageAt texture_at = [this](std::string_view path) -> ro::ui::SkinImage {
      if (!pieces_.game_images.has_value()) return {};
      std::string full(kTextureRoot);
      full += path;
      const ro::render::IconTexture& icon = pieces_.game_images->GetOurs(full);
      if (!icon.usable()) return {};
      return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                               .width = static_cast<float>(icon.width_px),
                               .height = static_cast<float>(icon.height_px)};
    };
    navigation.world_map_view = &navi_.world_map_view.Compose(WorldMapSources{
        .tables = &*pieces_.world_view,
        .here_map = here,
        .route = route,
        .map_names = pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr,
        .step_map = step_map,
        .party = party,
        .preview_map = navigation.world_map.hovered_map,
        .kafra_maps = kafra_maps,
        .kafra_sprite = &kafra_shown,
        .navi = data,
        .search = navi_.navigation_view.groups(),
        .chosen_world = navigation.world_map.world,
        .image_at = image_at,
        .texture_at = texture_at});
  } else {
    navigation.world_map_view = nullptr;
  }

  if (!navigation.state.open) {
    navigation.view = nullptr;
    return;
  }
  // L'aperçu : de face, et l'attente qui tourne, comme dans la fiche d'un
  // monstre. ⚠ MUET : ses sons sont jetés, une fenêtre de recherche ne crie pas.
  if (navi_.preview_sprite.has_value() && navi_.preview_map_entries != world_.map_entries()) {
    navi_.preview_map_entries = world_.map_entries();
    navi_.preview_sprite->Forget();
  }
  if (navi_.preview_sprite.has_value()) static_cast<void>(navi_.preview_sprite->TakeSounds());
  const NavigationSpriteAt sprite_at = [this, now_ms](std::uint32_t sprite_class,
                                                      int side_px) -> ro::ui::SkinImage {
    if (!navi_.preview_sprite.has_value()) return {};
    const MonsterSprite::Shown shown =
        navi_.preview_sprite->Compose(MonsterSprite::Framing{.sprite_class = sprite_class,
                                                             .direction = 0,
                                                             .animate = true,
                                                             .box_width = side_px,
                                                             .box_height = side_px},
                                      now_ms);
    return shown.missing ? ro::ui::SkinImage{} : shown.image;
  };
  std::string_view bound_label;
  if (data != nullptr && !navigation.state.bound_map.empty()) {
    const ro::formats::NaviMapIndex bound = data->FindMap(navigation.state.bound_map);
    if (bound != ro::formats::kNoNaviMap) bound_label = data->maps[bound].display_name;
  }
  navigation.view = &results.Compose(
      NavigationSources{.navi = data,
                        .guide = &navi_.guide,
                        .current_map = here,
                        .current_map_label = here_label,
                        .bound_map_label = bound_label,
                        .image_at = image_at,
                        .selected = SelectedRow(navigation.state),
                        .preview_side_px = static_cast<int>(navigation.state.preview_side),
                        .sprite_at = sprite_at});
}

}  // namespace moonclient
