// Une tuile de la grille de groupe, ses icônes d'état, son infobulle et son menu.

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/status_bar.h"
#include "ro/ui/status_cell.h"

#include "party_grid_parts.h"

namespace ro::ui::party_grid {
namespace {

constexpr const char* kMenuId = "##party_tile_menu";

constexpr float kTilePaddingPixels = 3.0F;
constexpr float kTileRoundingPixels = 2.0F;
constexpr float kLargestJobIconPixels = 28.0F;
constexpr float kSmallestJobIconPixels = 8.0F;
constexpr ImU32 kOfflineJobIconTint = IM_COL32(140, 140, 140, 220);
constexpr ImU32 kSpTrackAlphaMask = IM_COL32(0, 0, 0, 242);

// Le liseré, par ordre de priorité : survolé, ciblé, moi, les autres.
constexpr ImU32 kHoveredBorder = IM_COL32(255, 255, 255, 210);
constexpr ImU32 kTargetBorder = IM_COL32(255, 255, 255, 165);
constexpr ImU32 kPlainBorder = IM_COL32(0, 0, 0, 190);
constexpr float kStrongBorderThickness = 2.0F;
constexpr float kPlainBorderThickness = 1.0F;

constexpr float kStatusGapPixels = 1.0F;
constexpr float kSmallestCountdownPixels = 7.0F;
constexpr float kCountdownShareOfIcon = 0.5F;

constexpr char kLeaderMark[] = "* ";
constexpr int kTwoLines = 2;

[[nodiscard]] ImU32 WithAlpha(ImU32 color, ImU32 alpha_mask) {
  return (color & ~IM_COL32_A_MASK) | (alpha_mask & IM_COL32_A_MASK);
}

// Les icônes d'état, de DROITE à gauche depuis `right` : les plus récentes
// restent au bord, à la même place d'une image à l'autre. Rend l'abscisse la
// plus à gauche atteinte, où le texte du nom se découpe.
float DrawTileStatuses(const PartyGridTileView& member, const PartyGridLook& look, float right,
                       float top, float bottom, float scale, bool* status_hovered) {
  if (!member.statuses_known || member.statuses.empty()) {
    return right;
  }
  const int rows = std::clamp(look.status_rows, 1, kPartyGridMostStatusRows);
  const float gap = kStatusGapPixels * scale;
  // La hauteur se PARTAGE entre les lignes, sans quoi deux rangées d'icônes
  // pleine taille déborderaient sur la tuile suivante.
  const float asked_side =
      static_cast<float>(std::clamp(look.status_px, kPartyGridSmallestStatusPixels,
                                    kPartyGridLargestStatusPixels)) *
      scale;
  const float side = std::min(
      asked_side, (bottom - top - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows));
  if (side < static_cast<float>(kPartyGridSmallestStatusPixels) * scale) {
    return right;
  }

  StatusCellStyle style;
  style.sweep = StatusSweepOf(look.status_sweep);
  style.sweep_color = look.status_sweep_color;
  style.icon_background = look.status_icon_background;
  // Taille automatique : la moitié de l'icône, avec un plancher — un texte de
  // taille fixe débordait sur la rangée du dessous dès qu'on réduisait les
  // icônes. Sinon, la taille réglée.
  const float countdown =
      look.status_countdown_auto
          ? std::max(kSmallestCountdownPixels * scale, side * kCountdownShareOfIcon)
          : static_cast<float>(std::clamp(look.status_countdown_px,
                                          static_cast<int>(kSmallestStatusCountdownPixels),
                                          static_cast<int>(kLargestStatusCountdownPixels))) *
                scale;
  style.countdown_pixels = look.status_countdown ? countdown : 0.0F;
  style.countdown_background = look.status_countdown_background;
  style.alpha = StatusOpacityOf(look.status_opacity_percent);

  StatusRowLayout row;
  row.side = side;
  row.gap = gap;
  row.below = style.countdown_pixels;
  row.most = std::clamp(look.status_most, kPartyGridFewestStatusIcons, kPartyGridMostStatusIcons);
  row.rows = rows;
  row.right_to_left = true;

  // Les plus RÉCENTS d'abord, et gardés quand il y en a trop : la liste arrive
  // du plus ancien au plus récent.
  std::vector<std::size_t> order(member.statuses.size());
  std::iota(order.rbegin(), order.rend(), std::size_t{0});
  order.resize(std::min(order.size(), static_cast<std::size_t>(row.most)));

  const std::vector<ImVec2> places = PlaceStatusRow(order.size(), ImVec2(right, top), row);
  const std::size_t drawn = std::min(order.size(), places.size());
  float edge = right;
  for (std::size_t rank = 0; rank < drawn; ++rank) {
    const ImVec2 at = places[rank];
    const ImVec2 far_corner(at.x + side, at.y + side);
    DrawStatusCell(member.statuses[order[rank]], at, far_corner, style, true);
    if (status_hovered != nullptr && ImGui::IsMouseHoveringRect(at, far_corner)) {
      *status_hovered = true;
    }
    edge = std::min(edge, at.x);
  }
  // Un écart d'air entre la dernière icône et le texte.
  return drawn > 0 ? edge - gap : right;
}

[[nodiscard]] std::string FirstLine(const PartyGridTileView& member, const PartyGridLook& look) {
  std::string line;
  if (member.is_leader) {
    line += kLeaderMark;
  }
  line += member.name;
  if (look.show_level) {
    line += " Lv.";
    line += std::to_string(member.base_level);
  }
  return line;
}

[[nodiscard]] bool MenuHasEntries(bool is_me, bool offline, bool same_map, bool i_am_leader) {
  if (is_me) {
    return false;
  }
  const bool whisper = !offline;
  const bool make_leader = !offline && i_am_leader && same_map;
  const bool kick = i_am_leader;
  return whisper || make_leader || kick;
}

}  // namespace

void DrawTile(ImDrawList& into, const PartyGridTileView& member, ImVec2 top_left,
              ImVec2 bottom_right, const PartyGridLook& look, const TilePaint& paint,
              bool* status_hovered) {
  const float rounding = kTileRoundingPixels * paint.scale;
  const float pad = kTilePaddingPixels * paint.scale;

  into.AddRectFilled(top_left, bottom_right, look.tile_background, rounding);

  // La barre de vie EST le fond de la tuile. PV inconnus : rien, car une tuile
  // vide se lirait « mort » alors que le membre n'est que trop loin.
  const bool has_hp = !member.offline && member.hp_known && member.max_hp > 0;
  if (has_hp) {
    const float fraction =
        std::clamp(static_cast<float>(member.hp) / static_cast<float>(member.max_hp), 0.0F, 1.0F);
    if (fraction > 0.0F) {
      into.AddRectFilled(
          top_left, ImVec2(top_left.x + (bottom_right.x - top_left.x) * fraction, bottom_right.y),
          PartyGridHpInk(look, member.hp, member.max_hp), rounding);
    }
  }

  // La barre de SP se résout AVANT le reste : elle mange le bas de la tuile, et
  // l'icône comme le texte se rangent dans ce qu'elle laisse.
  const bool sp_shown = look.show_sp && !member.offline && member.sp_known && member.max_sp > 0;
  const float sp_height =
      sp_shown ? static_cast<float>(std::clamp(look.sp_bar_px, kPartyGridThinnestSpBarPixels,
                                               kPartyGridThickestSpBarPixels)) *
                     paint.scale
               : 0.0F;
  const float bottom = bottom_right.y - sp_height;

  float text_x = top_left.x + pad;
  if (look.show_job_icon && member.job_icon.usable()) {
    const float available = bottom - top_left.y - pad;
    const float side = std::min(available, kLargestJobIconPixels * paint.scale);
    if (side >= kSmallestJobIconPixels * paint.scale) {
      const ImVec2 icon_top_left(top_left.x + pad,
                                 top_left.y + ((bottom - top_left.y) - side) * 0.5F);
      const ImVec2 icon_bottom_right(icon_top_left.x + side, icon_top_left.y + side);
      into.AddImage(member.job_icon.id, icon_top_left, icon_bottom_right, ImVec2(0.0F, 0.0F),
                    ImVec2(1.0F, 1.0F), member.offline ? kOfflineJobIconTint : IM_COL32_WHITE);
      text_x = icon_bottom_right.x + pad;
    }
  }

  if (sp_shown) {
    const float fraction =
        std::clamp(static_cast<float>(member.sp) / static_cast<float>(member.max_sp), 0.0F, 1.0F);
    const ImVec2 track(top_left.x, bottom);
    into.AddRectFilled(track, bottom_right, WithAlpha(look.tile_background, kSpTrackAlphaMask));
    if (fraction > 0.0F) {
      into.AddRectFilled(
          track, ImVec2(track.x + (bottom_right.x - track.x) * fraction, bottom_right.y), look.sp);
    }
  }

  if (paint.hovered) {
    into.AddRect(top_left, bottom_right, kHoveredBorder, rounding, ImDrawFlags_None,
                 kStrongBorderThickness);
  } else if (member.is_target) {
    into.AddRect(top_left, bottom_right, kTargetBorder, rounding, ImDrawFlags_None,
                 kStrongBorderThickness);
  } else if (member.is_me) {
    into.AddRect(top_left, bottom_right, look.self_border, rounding, ImDrawFlags_None,
                 kStrongBorderThickness);
  } else {
    into.AddRect(top_left, bottom_right, kPlainBorder, rounding, ImDrawFlags_None,
                 kPlainBorderThickness);
  }

  // Les icônes AVANT le texte, pour lui rendre sa limite.
  float text_right = bottom_right.x - pad;
  if (look.show_statuses && !member.offline) {
    text_right = DrawTileStatuses(member, look, bottom_right.x - pad, top_left.y + pad,
                                  bottom - pad, paint.scale, status_hovered);
  }

  const ImU32 ink = member.offline ? look.offline_text : !has_hp ? look.far_text : look.text;
  const std::string first = FirstLine(member, look);
  std::string second;
  if (member.offline) {
    second = core::Tr("Hors ligne");
  } else if (!has_hp) {
    second = core::Tr("Hors de portée");
  } else {
    second = PartyGridHpCaption(PartyGridHpTextOf(look.hp_text), member.hp, member.max_hp);
  }

  const float font_pixels =
      static_cast<float>(
          std::clamp(look.text_px, kPartyGridSmallestTextPixels, kPartyGridLargestTextPixels)) *
      paint.scale;
  const float line = font_pixels;
  ImFont* font = ImGui::GetFont();

  // Centré dans ce que la barre de SP laisse : une tuile basse n'écrit que le
  // nom, une tuile haute les deux lignes.
  const bool two_lines =
      !second.empty() && (bottom - top_left.y) >= line * static_cast<float>(kTwoLines);
  const float block = two_lines ? line * static_cast<float>(kTwoLines) : line;
  const float text_y = std::max(top_left.y, top_left.y + ((bottom - top_left.y) - block) * 0.5F);

  // Découpé à la tuile : un nom qui déborde se lirait comme celui de la voisine.
  const ImVec4 clip(text_x, top_left.y, text_right, bottom);
  into.AddText(font, font_pixels, ImVec2(text_x, text_y), ink, first.c_str(), nullptr, 0.0F, &clip);
  if (two_lines) {
    into.AddText(font, font_pixels, ImVec2(text_x, text_y + line), ink, second.c_str(), nullptr,
                 0.0F, &clip);
  }
}

void DrawTileTooltip(const PartyGridTileView& member, const PartyGridLook& look) {
  if (!BeginSkinTooltip()) {
    return;
  }
  const std::string name(member.name);
  ImGui::TextUnformatted(name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("Lv.%d", member.base_level);
  if (member.is_leader) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", core::Tr("(chef)"));
  }
  ImGui::Separator();

  const std::string job(member.job_label);
  ImGui::TextDisabled("%s", job.c_str());

  if (member.offline) {
    ImGui::TextDisabled("%s", core::Tr("Hors ligne"));
    EndSkinTooltip();
    return;
  }

  if (!member.map_label.empty()) {
    const std::string map(member.map_label);
    ImGui::TextDisabled("%s", map.c_str());
  }

  const bool hp_known = member.hp_known && member.max_hp > 0;
  if (hp_known) {
    ImGui::Text("%s %d/%d (%d %%)", core::Tr("PV"), member.hp, member.max_hp,
                PartyGridHpPercent(member.hp, member.max_hp));
  } else {
    ImGui::TextDisabled("%s", core::Tr("PV inconnus : hors de portée"));
  }

  if (look.show_sp) {
    // Deux causes à un SP absent : hors de portée, le serveur ne répond pas ; à
    // portée, la rotation des demandes n'est pas encore passée par ce membre.
    if (member.sp_known && member.max_sp > 0) {
      ImGui::Text("%s %d/%d", core::Tr("SP"), member.sp, member.max_sp);
    } else if (!hp_known) {
      ImGui::TextDisabled("%s", core::Tr("SP inconnus : hors de portée"));
    } else {
      ImGui::TextDisabled("%s", core::Tr("SP inconnus : réponse en attente"));
    }
  }
  EndSkinTooltip();
}

void AskMemberMenu(const PartyGridTileView& member, const PartyGridView& view,
                   PartyGridState& state, PartyGridAnswer& answer) {
  // Un acteur représente le membre : c'est le menu d'entité, qui porte déjà
  // chef de groupe et expulsion.
  if (!member.is_me && !member.offline && member.in_sight) {
    answer.verb = PartyGridVerb::kEntityMenu;
    answer.verb_account_id = member.account_id;
    answer.verb_name = member.name;
    return;
  }
  // Un menu sans entrée ne s'ouvre pas : sur ma propre tuile, il n'y aurait que
  // mon nom.
  if (!MenuHasEntries(member.is_me, member.offline, member.same_map, view.i_am_leader)) {
    return;
  }
  state.menu_account_id = member.account_id;
  state.menu_name = member.name;
  state.menu_is_me = member.is_me;
  state.menu_offline = member.offline;
  state.menu_same_map = member.same_map;
  state.open_menu = true;
}

void DrawMemberMenu(PartyGridState& state, const PartyGridView& view, PartyGridAnswer& answer) {
  if (state.open_menu) {
    ImGui::OpenPopup(kMenuId);
    state.open_menu = false;
  }
  if (!BeginSkinPopup(kMenuId)) {
    return;
  }
  ImGui::TextDisabled("%s", state.menu_name.c_str());
  ImGui::Separator();

  const auto choose = [&state, &answer](PartyGridVerb verb) {
    answer.verb = verb;
    answer.verb_account_id = state.menu_account_id;
    answer.verb_name = state.menu_name;
  };
  // Hors ligne, on ne chuchote pas et on ne confie pas le commandement ; le
  // serveur ne nomme chef que sur la même carte.
  if (!state.menu_offline && !state.menu_is_me) {
    if (ImGui::Selectable(core::Tr("Chuchoter"))) {
      choose(PartyGridVerb::kWhisper);
    }
    if (ImGui::Selectable(core::Tr("Chuchoter dans une fenêtre…"))) {
      choose(PartyGridVerb::kWhisperWindow);
    }
    if (view.i_am_leader && state.menu_same_map &&
        ImGui::Selectable(core::Tr("Nommer chef de groupe"))) {
      choose(PartyGridVerb::kMakeLeader);
    }
  }
  // Expulser reste offert hors ligne : c'est le geste attendu sur un absent.
  if (view.i_am_leader && !state.menu_is_me && ImGui::Selectable(core::Tr("Expulser du groupe"))) {
    choose(PartyGridVerb::kKick);
  }
  EndSkinPopup();
}

}  // namespace ro::ui::party_grid
