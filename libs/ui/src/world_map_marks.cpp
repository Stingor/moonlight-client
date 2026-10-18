// Les marques de la carte du monde. Voir `world_map_marks.h`.
//
// LES PLACES SUIVENT L'IMAGE, LES TAILLES RESTENT EN PIXELS D'ÉCRAN : le client
// d'origine étire l'image du monde sur l'écran, et garde la taille d'un visage,
// d'un trait ou d'une lettre — qui ne change qu'avec le palier de taille.

#include "world_map_marks.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ui_color.h"

namespace ro::ui {
namespace {

// Une teinte se peint rentrée d'un pixel d'écran.
constexpr float kTintInsetPixels = 1.0F;
// La coche de l'étape en cours et les visages, presque opaques.
constexpr ImU32 kMarkTint = IM_COL32(255, 255, 255, 0xEE);
// UN VISAGE : seize pixels de côté au premier palier, quatre de plus à chacun
// des suivants.
constexpr float kFaceHalfSize = 8.0F;
constexpr float kFaceHalfGrowth = 2.0F;
// LE FILET D'UNE CARTE DU GROUPE : doré pour la sienne, paille pour les autres.
constexpr ImU32 kPartyHereEdge = IM_COL32(232, 174, 35, 255);
constexpr ImU32 kPartyEdge = IM_COL32(247, 227, 126, 255);
// Un filet d'un pixel d'écran aux coins coupés de trois.
constexpr float kEdgeRounding = 3.0F;
constexpr float kEdgeThickness = 1.0F;
// LE TRAIT D'UN DONJON : deux pixels, et il arrive un pixel sous le centre du
// point.
constexpr float kDungeonLineHalfWidth = 1.0F;
constexpr float kDungeonLineDrop = 1.0F;
// Le nom d'un donjon : deux pixels sous le haut de son étiquette, et son ombre
// un pixel plus bas à droite.
constexpr float kDungeonNameTop = 2.0F;
constexpr float kShadowOffset = 1.0F;
// LE NOM D'UNE CARTE, à trois pixels du haut ou du bas, une carte sur deux.
constexpr float kFieldNameMargin = 3.0F;
// L'APERÇU : un fond sombre sous la minimap, le nom affiché en haut et le nom
// interne en bas.
constexpr ImU32 kPreviewBack = IM_COL32(0, 0, 0, 0x88);
constexpr float kPreviewTextLeft = 3.0F;
constexpr float kPreviewTitleTop = 3.0F;
constexpr float kPreviewNameTop = 115.0F;
constexpr ImU32 kPreviewTitleColor = IM_COL32(255, 210, 0, 255);
constexpr ImU32 kPreviewNameColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kPreviewNameOutline = IM_COL32(92, 92, 92, 255);
constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
constexpr float kPreviewFontPixels = 12.0F;
// LA LISTE DU GROUPE : un fond voilé, un filet blanc, cinq pixels de marge, et
// son propre nom en rose.
constexpr ImU32 kListBack = IM_COL32(0, 0, 0, 0x66);
constexpr ImU32 kListEdge = IM_COL32(255, 255, 255, 255);
constexpr float kListPadding = 5.0F;
constexpr float kListWidthPadding = 10.0F;
constexpr float kListHeightPadding = 7.0F;
constexpr float kListLineGap = 3.0F;
constexpr ImU32 kListSelfColor = IM_COL32(240, 128, 128, 255);
constexpr ImU32 kListOtherColor = IM_COL32(255, 255, 255, 255);
constexpr float kListFontPixels = 12.0F;

[[nodiscard]] ImVec2 TopLeft(const WorldMapFit& fit, const WorldMapRect& rect) {
  return ToScreen(fit, ImVec2(rect.left, rect.top));
}

[[nodiscard]] ImVec2 BottomRight(const WorldMapFit& fit, const WorldMapRect& rect) {
  return ToScreen(fit, ImVec2(rect.right, rect.bottom));
}

// Le bas droit de l'image sur l'écran : ce qui déborde au-delà se replie.
[[nodiscard]] ImVec2 ScreenEnd(const WorldMapFit& fit, const WorldMapView& view) {
  return ToScreen(fit, view.image_size);
}

[[nodiscard]] std::size_t TierOf(const WorldMapFit& fit, const WorldMapView& view) {
  return WorldMapSizeTier(view.image_size.x * fit.scale);
}

void Stretch(ImDrawList* into, const SkinImage& image, ImVec2 from, ImVec2 to, ImU32 tint) {
  if (image.usable()) {
    into->AddImage(image.id, from, to, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), tint);
  }
}

// Du blanc, à l'opacité d'une couleur.
[[nodiscard]] ImU32 WhiteAt(ImU32 color) {
  return (color & IM_COL32_A_MASK) | IM_COL32(255, 255, 255, 0);
}

[[nodiscard]] ImVec2 TextSize(float size_pixels, std::string_view text) {
  return ImGui::GetFont()->CalcTextSizeA(size_pixels, FLT_MAX, 0.0F, text.data(),
                                         text.data() + text.size());
}

void Write(ImDrawList* into, float size_pixels, ImVec2 at, ImU32 color, std::string_view text) {
  into->AddText(ImGui::GetFont(), size_pixels, at, color, text.data(), text.data() + text.size());
}

// Centré en largeur sur un rectangle de l'écran.
[[nodiscard]] float CenteredLeft(ImVec2 from, ImVec2 to, float text_width) {
  return (from.x + to.x - text_width) * 0.5F;
}

// L'aperçu de la carte survolée, s'il y a une carte.
[[nodiscard]] std::optional<ImVec2> PreviewOf(const WorldMapFit& fit, const WorldMapView& view,
                                              const WorldMapHover& hover) {
  if (hover.rect == nullptr) {
    return std::nullopt;
  }
  return WorldMapPreviewAt(TopLeft(fit, *hover.rect), BottomRight(fit, *hover.rect),
                           ScreenEnd(fit, view));
}

[[nodiscard]] float ListLineHeight() { return TextSize(kListFontPixels, " ").y + kListLineGap; }

// La place de la liste du groupe, à côté de l'aperçu : son coin et sa taille.
[[nodiscard]] std::optional<std::pair<ImVec2, ImVec2>> ListOf(const WorldMapFit& fit,
                                                              const WorldMapView& view,
                                                              const WorldMapHover& hover) {
  const std::optional<ImVec2> preview = PreviewOf(fit, view, hover);
  if (!preview.has_value() || hover.party == nullptr || hover.party->members.empty()) {
    return std::nullopt;
  }
  float widest = 0.0F;
  for (const WorldMapMemberView& member : hover.party->members) {
    widest = std::max(widest, TextSize(kListFontPixels, member.name).x);
  }
  const ImVec2 size(
      widest + kListWidthPadding,
      ListLineHeight() * static_cast<float>(hover.party->members.size()) + kListHeightPadding);
  const ImVec2 at =
      WorldMapMemberListAt(*preview, TopLeft(fit, hover.party->rect),
                           BottomRight(fit, hover.party->rect), size, ScreenEnd(fit, view));
  return std::pair{at, size};
}

}  // namespace

std::size_t WorldMapSizeTier(float shown_width_pixels) {
  constexpr std::array<float, kWorldMapSizeTiers - 1> kFloors = {640.0F, 800.0F, 1024.0F};
  std::size_t tier = 0;
  for (const float floor : kFloors) {
    if (shown_width_pixels > floor) {
      ++tier;
    }
  }
  return tier;
}

ImVec2 WorldMapPreviewAt(ImVec2 rect_min, ImVec2 rect_max, ImVec2 screen_end) {
  constexpr float kGap = 10.0F;
  constexpr float kRoom = kWorldMapPreviewSize + kGap;
  ImVec2 at(rect_max.x + kGap, rect_min.y + kGap);
  if (rect_max.x + kRoom > screen_end.x) {
    at.x = rect_min.x - kRoom;
  }
  if (rect_min.y + kRoom > screen_end.y) {
    at.y = screen_end.y - kWorldMapPreviewSize;
  }
  return at;
}

ImVec2 WorldMapMemberListAt(ImVec2 preview_at, ImVec2 party_min, ImVec2 party_max, ImVec2 size,
                            ImVec2 screen_end) {
  constexpr float kGap = 10.0F;
  constexpr float kRightDrop = 17.0F;
  const bool on_right = preview_at.x + kWorldMapPreviewSize >= party_max.x;
  ImVec2 at(on_right ? preview_at.x + kWorldMapPreviewSize + kGap : preview_at.x - kGap - size.x,
            preview_at.y);
  if (at.x + size.x <= screen_end.x) {
    if (on_right) {
      at.y += kRightDrop;
    }
  } else {
    at.x = party_min.x - size.x - kGap;
  }
  if (at.y + size.y > screen_end.y) {
    at.y = screen_end.y - size.y + kGap;
  }
  return at;
}

void WriteWorldMapOutlined(ImDrawList* into, float size_pixels, ImVec2 at, ImU32 letter,
                           ImU32 outline, std::string_view text) {
  constexpr std::array<ImVec2, 4> kAround = {ImVec2(-1.0F, 0.0F), ImVec2(1.0F, 0.0F),
                                             ImVec2(0.0F, -1.0F), ImVec2(0.0F, 1.0F)};
  for (const ImVec2 offset : kAround) {
    Write(into, size_pixels, ImVec2(at.x + offset.x, at.y + offset.y), outline, text);
  }
  Write(into, size_pixels, at, letter, text);
}

void DrawWorldMapSearch(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view) {
  const auto paint = [&fit, into](const WorldMapAreaView& area) {
    if (area.search_color == 0) {
      return;
    }
    const ImVec2 from = TopLeft(fit, area.rect);
    const ImVec2 to = BottomRight(fit, area.rect);
    const ImVec2 inner_from(from.x + kTintInsetPixels, from.y + kTintInsetPixels);
    const ImVec2 inner_to(to.x - kTintInsetPixels, to.y - kTintInsetPixels);
    into->AddRectFilled(inner_from, inner_to, area.search_color);
    Stretch(into, area.search_image, inner_from, inner_to, IM_COL32_WHITE);
  };
  for (const WorldMapAreaView& area : view.areas) {
    paint(area);
  }
  for (const WorldMapDungeonView& dungeon : view.dungeons) {
    for (const WorldMapAreaView& floor : dungeon.floors) {
      paint(floor);
    }
  }
}

void DrawWorldMapStepMark(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view) {
  if (view.step_mark.has_value()) {
    Stretch(into, view.step_mark_image, TopLeft(fit, *view.step_mark),
            BottomRight(fit, *view.step_mark), kMarkTint);
  }
}

void DrawWorldMapPartyFaces(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view) {
  const float half = kFaceHalfSize + kFaceHalfGrowth * static_cast<float>(TierOf(fit, view));
  for (const WorldMapPartyView& place : view.party) {
    const ImVec2 center = CenterOnScreen(fit, place.rect);
    Stretch(into, place.here ? view.party_face_here : view.party_face,
            ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half),
            kMarkTint);
  }
}

void DrawWorldMapDungeons(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                          const WorldMapDungeonView* hovered) {
  for (const WorldMapDungeonView& dungeon : view.dungeons) {
    const bool lit = &dungeon == hovered;
    const ImU32 tint = lit ? IM_COL32_WHITE : WhiteAt(view.dungeon_color);
    if (!dungeon.floors.empty()) {
      const ImVec2 spot = CenterOnScreen(fit, dungeon.rect);
      if (const auto band = WorldMapBand(
              CenterOnScreen(fit, dungeon.label), ImVec2(spot.x, spot.y + kDungeonLineDrop),
              kDungeonLineHalfWidth, lit ? Opaque(view.select_color) : view.dungeon_color);
          band.has_value()) {
        into->AddQuadFilled(band->corners[0], band->corners[1], band->corners[3], band->corners[2],
                            band->color);
      }
      Stretch(into, view.dungeon_spot, TopLeft(fit, dungeon.rect), BottomRight(fit, dungeon.rect),
              tint);
    }
    Stretch(into, view.dungeon_label, TopLeft(fit, dungeon.label), BottomRight(fit, dungeon.label),
            tint);
  }
}

void DrawWorldMapHover(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                       const WorldMapHover& hover) {
  const std::optional<ImVec2> preview = PreviewOf(fit, view, hover);
  if (!preview.has_value()) {
    return;
  }
  into->AddRectFilled(TopLeft(fit, *hover.rect), BottomRight(fit, *hover.rect), view.select_box);
  if (hover.preview != nullptr && hover.preview->image.usable()) {
    const ImVec2 end(preview->x + kWorldMapPreviewSize, preview->y + kWorldMapPreviewSize);
    into->AddRectFilled(*preview, end, kPreviewBack);
    Stretch(into, hover.preview->image, *preview, end, IM_COL32_WHITE);
  }
  if (const auto list = ListOf(fit, view, hover); list.has_value()) {
    into->AddRectFilled(list->first,
                        ImVec2(list->first.x + list->second.x, list->first.y + list->second.y),
                        kListBack);
  }
}

void WriteWorldMapHover(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                        const WorldMapHover& hover) {
  if (const auto list = ListOf(fit, view, hover); list.has_value()) {
    const auto& [at, size] = *list;
    into->AddRect(at, ImVec2(at.x + size.x, at.y + size.y), kListEdge, kEdgeRounding, 0,
                  kEdgeThickness);
    float top = at.y + kListPadding;
    for (const WorldMapMemberView& member : hover.party->members) {
      WriteWorldMapOutlined(into, kListFontPixels, ImVec2(at.x + kListPadding, top),
                            member.self ? kListSelfColor : kListOtherColor, kBlack, member.name);
      top += ListLineHeight();
    }
  }
  const std::optional<ImVec2> preview = PreviewOf(fit, view, hover);
  if (!preview.has_value() || hover.preview == nullptr) {
    return;
  }
  WriteWorldMapOutlined(into, kPreviewFontPixels,
                        ImVec2(preview->x + kPreviewTextLeft, preview->y + kPreviewTitleTop),
                        kPreviewTitleColor, kBlack, hover.preview->title);
  WriteWorldMapOutlined(into, kPreviewFontPixels,
                        ImVec2(preview->x + kPreviewTextLeft, preview->y + kPreviewNameTop),
                        kPreviewNameColor, kPreviewNameOutline, hover.preview->map_name);
}

void WriteWorldMapDungeonNames(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                               bool with_levels) {
  const float size = view.dungeon_name.sizes[TierOf(fit, view)];
  const auto write = [&](std::string_view text, ImVec2 from, ImVec2 to, float top) {
    const ImVec2 at(CenteredLeft(from, to, TextSize(size, text).x), top);
    Write(into, size, ImVec2(at.x + kShadowOffset, at.y + kShadowOffset), view.dungeon_name.shadow,
          text);
    Write(into, size, at, view.dungeon_name.letter, text);
  };
  for (const WorldMapDungeonView& dungeon : view.dungeons) {
    const ImVec2 from = TopLeft(fit, dungeon.label);
    const ImVec2 to = BottomRight(fit, dungeon.label);
    if (!dungeon.title.empty()) {
      write(dungeon.title, from, to, from.y + kDungeonNameTop);
    }
    // LE NIVEAU, sous le nom, quand les noms des cartes se montrent.
    if (with_levels && !dungeon.level.empty()) {
      write(dungeon.level, from, to, from.y + size + kDungeonNameTop);
    }
  }
}

void OutlineWorldMapParty(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view) {
  for (const WorldMapPartyView& place : view.party) {
    OutlineWorldMapRect(into, fit, place.rect, place.here ? kPartyHereEdge : kPartyEdge);
  }
}

void WriteWorldMapFieldNames(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view) {
  const std::size_t tier = TierOf(fit, view);
  const float name_size = view.field_name.sizes[tier];
  const float level_size = view.field_level.sizes[tier];
  // LES CARTES GROUPE PAR GROUPE, dans l'ordre où chaque groupe paraît : le nom
  // va en bas puis en haut, une carte sur deux, d'un bout à l'autre du monde.
  std::map<int, std::size_t> group_rank;
  for (const WorldMapAreaView& area : view.areas) {
    group_rank.try_emplace(area.group, group_rank.size());
  }
  std::vector<const WorldMapAreaView*> ordered;
  for (const WorldMapAreaView& area : view.areas) {
    ordered.push_back(&area);
  }
  std::ranges::stable_sort(ordered, {}, [&group_rank](const WorldMapAreaView* area) {
    return group_rank.at(area->group);
  });
  bool at_top = false;
  for (const WorldMapAreaView* area : ordered) {
    if (area->title.empty()) {
      continue;
    }
    const ImVec2 from = TopLeft(fit, area->rect);
    const ImVec2 to = BottomRight(fit, area->rect);
    into->AddRect(from, to, view.field_edge, kEdgeRounding, 0, kEdgeThickness);
    float name_top = 0.0F;
    float level_top = 0.0F;
    if (at_top) {
      name_top = from.y + kFieldNameMargin;
      level_top = name_top + name_size + kFieldNameMargin;
    } else {
      name_top = to.y - name_size - kFieldNameMargin;
      level_top = name_top - level_size - kFieldNameMargin;
    }
    at_top = !at_top;
    const ImVec2 at(CenteredLeft(from, to, TextSize(name_size, area->title).x), name_top);
    Write(into, name_size, ImVec2(at.x + kShadowOffset, at.y + kShadowOffset),
          view.field_name.shadow, area->title);
    Write(into, name_size, ImVec2(at.x - kShadowOffset, at.y - kShadowOffset),
          view.field_name.shadow, area->title);
    Write(into, name_size, at, view.field_name.letter, area->title);
    if (!area->level.empty()) {
      WriteWorldMapOutlined(
          into, level_size,
          ImVec2(CenteredLeft(from, to, TextSize(level_size, area->level).x), level_top),
          view.field_level.letter, view.field_level.shadow, area->level);
    }
  }
}

void OutlineWorldMapRect(ImDrawList* into, const WorldMapFit& fit, const WorldMapRect& rect,
                         ImU32 color) {
  into->AddRect(TopLeft(fit, rect), BottomRight(fit, rect), color, kEdgeRounding, 0,
                kEdgeThickness);
}

}  // namespace ro::ui
