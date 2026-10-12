// La carte du monde. Voir l'en-tête.

#include "ro/ui/world_map_window.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "world_map_flight.h"
#include "world_map_marks.h"

namespace ro::ui {
namespace {

// La première taille de la fenêtre, et la plus petite : l'image se lit encore.
constexpr float kFirstWidthPixels = 820.0F;
constexpr float kFirstHeightPixels = 700.0F;
constexpr float kLeastWidthPixels = 420.0F;
constexpr float kLeastHeightPixels = 380.0F;
// Un boss se lit en rouge dans la liste d'une carte : un rouge choisi pour du
// blanc, que le papier de la bulle traduit.
constexpr ImU32 kBossColor = IM_COL32(217, 38, 38, 255);
constexpr const char* kStaffMenuId = "##wm_staff_menu";

[[nodiscard]] float AreaOf(const WorldMapRect& rect) {
  return (rect.right - rect.left) * (rect.bottom - rect.top);
}

[[nodiscard]] bool Contains(const WorldMapRect& rect, ImVec2 point) {
  return point.x >= rect.left && point.x <= rect.right && point.y >= rect.top &&
         point.y <= rect.bottom;
}

// La même couleur, opaque.
[[nodiscard]] ImU32 Opaque(ImU32 color) {
  return (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 255);
}

// Le niveau, s'il y en a un, sur sa ligne.
void TellLevel(std::string_view level) {
  if (!level.empty()) {
    ImGui::Text(core::Tr("Niveau : %s"), std::string(level).c_str());
  }
}

void TellArea(const WorldMapAreaView& area) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(area.title.empty() ? std::string(area.map_name).c_str()
                                            : std::string(area.title).c_str());
  ImGui::TextDisabled("%s", std::string(area.map_name).c_str());
  TellLevel(area.level);
  // LES MONSTRES, les premiers seulement : une infobulle ne défile pas.
  const std::size_t shown = std::min(area.monsters.size(), kWorldMapMonstersShown);
  const ImU32 paper = PaintingLook().paper;
  const ImU32 boss_ink = ReadableOn(OnPaper(kBossColor, paper), paper);
  for (std::size_t rank = 0; rank < shown; ++rank) {
    const WorldMapMonsterView& monster = area.monsters[rank];
    if (monster.boss) {
      ImGui::PushStyleColor(ImGuiCol_Text, boss_ink);
    }
    ImGui::BulletText(core::Tr("%s (Nv %d) ×%d"), std::string(monster.name).c_str(), monster.level,
                      monster.amount);
    if (monster.boss) {
      ImGui::PopStyleColor();
    }
  }
  if (area.monsters.size() > shown) {
    ImGui::TextDisabled(core::Tr("… et %d autre(s)"),
                        static_cast<int>(area.monsters.size() - shown));
  }
  ImGui::TextDisabled("%s", core::Tr("Clic : y aller"));
  EndSkinTooltip();
}

void TellDungeon(const WorldMapDungeonView& dungeon) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(std::string(dungeon.title).c_str());
  TellLevel(dungeon.level);
  for (const WorldMapAreaView& floor : dungeon.floors) {
    ImGui::BulletText("%s",
                      std::string(floor.title.empty() ? floor.map_name : floor.title).c_str());
  }
  if (!dungeon.floors.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Clic : aller au premier étage"));
  }
  EndSkinTooltip();
}

}  // namespace

WorldMapFit FitWorldMap(ImVec2 image_size, ImVec2 room_min, ImVec2 room_max) {
  WorldMapFit fit;
  const float room_width = room_max.x - room_min.x;
  const float room_height = room_max.y - room_min.y;
  if (image_size.x <= 0.0F || image_size.y <= 0.0F || room_width <= 0.0F || room_height <= 0.0F) {
    fit.origin = room_min;
    return fit;
  }
  fit.scale = std::min(room_width / image_size.x, room_height / image_size.y);
  fit.origin = ImVec2(room_min.x + (room_width - image_size.x * fit.scale) * 0.5F,
                      room_min.y + (room_height - image_size.y * fit.scale) * 0.5F);
  return fit;
}

std::optional<std::size_t> WorldMapAreaAt(std::span<const WorldMapAreaView> areas,
                                          ImVec2 image_point) {
  std::optional<std::size_t> found;
  float smallest = FLT_MAX;
  for (std::size_t rank = 0; rank < areas.size(); ++rank) {
    if (Contains(areas[rank].rect, image_point) && AreaOf(areas[rank].rect) < smallest) {
      smallest = AreaOf(areas[rank].rect);
      found = rank;
    }
  }
  return found;
}

WorldMapAction DrawWorldMapWindow(WorldMapWindowState& state, const WorldMapView& view,
                                  const SkinArt& art) {
  WorldMapAction action;
  if (!state.open) {
    return action;
  }
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kLeastWidthPixels, kLeastHeightPixels),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string title = std::string(core::Tr("Carte du monde")) + kWorldMapWindowId;
  bool open = true;
  const bool shown = BeginSkinnedWindow(title.c_str(), &open, art, SkinnedTitle::kBar,
                                        ImGuiWindowFlags_NoCollapse);
  if (!open) {
    state.open = false;
  }
  if (!shown) {
    EndSkinnedWindow();
    return action;
  }

  if (view.worlds.size() > 1) {
    int chosen = view.world;
    if (SkinCombo("##monde", &chosen, view.worlds, art) && chosen != view.world) {
      action.world = chosen;
    }
    ImGui::SameLine();
  }
  ImGui::Checkbox(core::Tr("Noms des cartes"), &state.show_names);

  const ImVec2 room_min = ImGui::GetCursorScreenPos();
  const ImVec2 room_max(room_min.x + ImGui::GetContentRegionAvail().x,
                        room_min.y + ImGui::GetContentRegionAvail().y);
  const WorldMapFit fit = FitWorldMap(view.image_size, room_min, room_max);
  ImGui::InvisibleButton("##carte", ImVec2(room_max.x - room_min.x, room_max.y - room_min.y));
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
  ImDrawList* into = ImGui::GetWindowDrawList();
  if (!view.image.usable() || fit.scale <= 0.0F) {
    into->AddText(room_min, ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  core::Tr("L'image de ce monde manque à l'installation."));
    EndSkinnedWindow();
    return action;
  }
  const ImVec2 image_max(fit.origin.x + view.image_size.x * fit.scale,
                         fit.origin.y + view.image_size.y * fit.scale);
  into->AddImage(view.image.id, fit.origin, image_max);

  // CE QUE LA SOURIS DÉSIGNE, avant de peindre : un donjon survolé passe en
  // pleine lumière. Un donjon passe avant : son étiquette est posée sur la mer
  // ou dans un champ.
  const WorldMapDungeonView* hovered_dungeon = nullptr;
  const WorldMapAreaView* hovered_area = nullptr;
  const WorldMapPartyView* hovered_party = nullptr;
  if (hovered) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 point((mouse.x - fit.origin.x) / fit.scale, (mouse.y - fit.origin.y) / fit.scale);
    const auto dungeon = std::ranges::find_if(
        view.dungeons, [point](const auto& candidate) { return Contains(candidate.label, point); });
    if (dungeon != view.dungeons.end()) {
      hovered_dungeon = &*dungeon;
    } else if (const auto area = WorldMapAreaAt(view.areas, point); area.has_value()) {
      hovered_area = &view.areas[*area];
    }
    const auto party = std::ranges::find_if(
        view.party, [point](const auto& place) { return Contains(place.rect, point); });
    if (party != view.party.end()) {
      hovered_party = &*party;
    }
  }

  // LES COUCHES, dans l'ordre du client : la recherche, l'itinéraire, la coche
  // de l'étape, les visages du groupe, les donjons.
  DrawWorldMapSearch(into, fit, view);
  DrawWorldMapRoute(into, fit, view);
  DrawWorldMapStepMark(into, fit, view);
  DrawWorldMapPartyFaces(into, fit, view);
  DrawWorldMapDungeons(into, fit, view, hovered_dungeon);

  // LA CARTE DÉSIGNÉE — le premier étage pour un donjon — : son voile, son
  // aperçu, et la liste du groupe qui s'y trouve.
  std::string_view hovered_map;
  WorldMapHover hover{.party = hovered_party};
  if (hovered_dungeon != nullptr) {
    hover.rect = &hovered_dungeon->label;
    TellDungeon(*hovered_dungeon);
    if (!hovered_dungeon->floors.empty()) {
      hovered_map = hovered_dungeon->floors.front().map_name;
    }
  } else if (hovered_area != nullptr) {
    hover.rect = &hovered_area->rect;
    TellArea(*hovered_area);
    hovered_map = hovered_area->map_name;
  }
  if (clicked && !hovered_map.empty()) {
    action.guide_to = std::string(hovered_map);
  }
  if (right_clicked && state.staff_warp && !hovered_map.empty()) {
    state.staff_menu_map = std::string(hovered_map);
    ImGui::OpenPopup(kStaffMenuId);
  }
  // L'aperçu se compose à l'image suivante : celui d'une autre carte attend.
  if (view.preview.has_value() && view.preview->map_name == hovered_map) {
    hover.preview = &*view.preview;
  }
  state.hovered_map = std::string(hovered_map);
  state.size_tier = WorldMapSizeTier(view.image_size.x * fit.scale);
  DrawWorldMapHover(into, fit, view, hover);
  DrawWorldMapWarpAgent(into, fit, view, hovered_map);
  // LES DIRIGEABLES, qui ne se montrent qu'au survol, puis les lettres et les
  // filets par-dessus les images.
  DrawWorldMapAirLines(into, fit, view, state, hovered_map);
  WriteWorldMapDungeonNames(into, fit, view, state.show_names);
  OutlineWorldMapParty(into, fit, view);
  if (state.show_names) {
    WriteWorldMapFieldNames(into, fit, view);
  }
  WriteWorldMapHover(into, fit, view, hover);
  if (hovered_dungeon != nullptr) {
    OutlineWorldMapRect(into, fit, hovered_dungeon->rect, Opaque(view.select_color));
  }
  if (hover.rect != nullptr) {
    OutlineWorldMapRect(into, fit, *hover.rect, Opaque(view.select_color));
  }
  if (BeginSkinPopup(kStaffMenuId)) {
    // Le nom INTERNE en tête : c'est celui que la commande portera.
    ImGui::TextDisabled(" %s ", state.staff_menu_map.c_str());
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Téléporter sur cette map"))) {
      action.warp_to = state.staff_menu_map;
    }
    EndSkinPopup();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
