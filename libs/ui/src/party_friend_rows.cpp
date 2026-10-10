// LES LIGNES DE LA FENÊTRE AMIS / GROUPE.
//
//   [icône de classe]  Lv.N Nom(Carte)          [états][-EXP][statut]
//                      [====jauge de PV====] N/N
//
// L'icône a sa place réservée d'abord, mais se peint en fin de ligne : on ne
// la centre que sur une hauteur connue.

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"

#include "party_friend_parts.h"

namespace ro::ui {
namespace {

constexpr float kLeastIconPixels = 12.0F;
constexpr float kLeastBarWidthPixels = 20.0F;
constexpr float kLeastBarHeightPixels = 3.0F;
constexpr float kLeastBuffPixels = 8.0F;
constexpr float kBuffGapPixels = 1.0F;
constexpr float kLeastCountdownPixels = 7.0F;
constexpr float kCountdownShare = 0.5F;
constexpr float kBarTextGapPixels = 6.0F;
constexpr float kBadgePadXPixels = 5.0F;
constexpr float kBadgePadYPixels = 1.0F;
constexpr float kBadgeMarginPixels = 4.0F;
constexpr float kBadgeRoundingPixels = 3.0F;
constexpr float kMarkGapPixels = 4.0F;
// Écart entre la couronne et le nom, en pixels d'écran.
constexpr float kCrownGapScreen = 3.0F;
constexpr float kHeadDropPixels = 2.0F;
constexpr float kHeadSlackPixels = 6.0F;
constexpr float kTargetBorderScreen = 1.5F;
constexpr float kMinimapPixels = 128.0F;
constexpr float kMinimapDotOuterPixels = 3.0F;
constexpr float kMinimapDotInnerPixels = 2.0F;
constexpr float kShadowScreen = 1.0F;

constexpr ImU32 kHealthFill = IM_COL32(64, 200, 72, 255);
constexpr ImU32 kMagicFill = IM_COL32(70, 130, 220, 255);
constexpr ImU32 kGaugeBack = IM_COL32(24, 24, 24, 200);
constexpr ImU32 kGaugeBorder = IM_COL32(0, 0, 0, 180);
constexpr ImU32 kGaugeShadow = IM_COL32(0, 0, 0, 180);
constexpr ImU32 kBadgeOffline = IM_COL32(105, 105, 105, 255);
constexpr ImU32 kBadgeMe = IM_COL32(58, 110, 190, 255);
constexpr ImU32 kBadgeOnline = IM_COL32(54, 138, 64, 255);
constexpr ImU32 kShareMark = IM_COL32(184, 112, 36, 255);
constexpr ImU32 kOfflineTint = IM_COL32(140, 140, 140, 220);
constexpr ImU32 kRowRule = IM_COL32(0, 0, 0, 40);
constexpr ImU32 kTargetBorder = IM_COL32(255, 255, 255, 200);
constexpr ImU32 kTargetHover = IM_COL32(255, 255, 255, 70);
constexpr ImU32 kMinimapDotHalo = IM_COL32(255, 255, 255, 230);
constexpr ImU32 kMinimapDot = IM_COL32(230, 90, 60, 255);

[[nodiscard]] ImVec2 Floor(ImVec2 point) {
  return {static_cast<float>(static_cast<int>(point.x)),
          static_cast<float>(static_cast<int>(point.y))};
}

// La carte sans son extension, telle qu'une table de noms la connaît.
[[nodiscard]] std::string_view BareMap(std::string_view raw) {
  return raw.substr(0, raw.find('.'));
}

// Le nom de carte à montrer : celui de la table, ou le nom nu faute de mieux.
[[nodiscard]] std::string_view ShownMap(const PartyMemberView& member) {
  return member.map_name.empty() ? BareMap(member.map_raw) : member.map_name;
}

// La tête, tenue dans sa case sans jamais l'agrandir : sur une ligne de liste on
// ne fait que réduire.
void DrawHead(const SkinImage& head, ImVec2 at, float box) {
  if (!head.usable()) {
    return;
  }
  const float fit = std::min({box / head.width, box / head.height, 1.0F});
  const float width = head.width * fit;
  const float height = head.height * fit;
  const ImVec2 low(at.x + (box - width) * 0.5F, at.y + (box - height) * 0.5F);
  ImGui::GetWindowDrawList()->AddImage(head.id, low, ImVec2(low.x + width, low.y + height));
}

// La pastille de statut, calée à droite de la ligne courante. Dessinée et non
// blittée : un texte reste net à toute échelle.
void DrawStatusBadge(const char* text, ImU32 fill, const char* tooltip) {
  ImGui::SameLine();
  const float rest = ImGui::GetContentRegionAvail().x;
  const ImVec2 text_size = ImGui::CalcTextSize(text);
  const float pad_x = PartyPx(kBadgePadXPixels);
  const float pad_y = PartyPx(kBadgePadYPixels);
  const ImVec2 size(text_size.x + pad_x * 2.0F, text_size.y + pad_y * 2.0F);
  const float margin = PartyPx(kBadgeMarginPixels);
  if (rest > size.x + margin) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rest - size.x - margin);
  }
  const ImVec2 at = Floor(ImGui::GetCursorScreenPos());
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y), fill,
                      PartyPx(kBadgeRoundingPixels));
  into->AddText(ImVec2(at.x + pad_x, at.y + pad_y), IM_COL32_WHITE, text);
  ImGui::Dummy(size);
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", tooltip);
  }
}

// La marque « -EXP », de droite à gauche depuis `right`. Rend la largeur prise,
// marge comprise.
float DrawShareMark(float right, float top, const std::string& why, bool& hovered) {
  constexpr const char* kMark = "-EXP";
  const ImVec2 text_size = ImGui::CalcTextSize(kMark);
  const float pad_x = PartyPx(kBadgePadXPixels);
  const float pad_y = PartyPx(kBadgePadYPixels);
  const ImVec2 size(text_size.x + pad_x * 2.0F, text_size.y + pad_y * 2.0F);
  const ImVec2 low = Floor(ImVec2(right - size.x, top));
  const ImVec2 high(low.x + size.x, low.y + size.y);
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(low, high, kShareMark, PartyPx(kBadgeRoundingPixels));
  into->AddText(ImVec2(low.x + pad_x, low.y + pad_y), IM_COL32_WHITE, kMark);
  hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(low, high);
  if (hovered) {
    SkinTooltipFormat("%s\n%s", core::Tr("Ne reçoit pas d'EXP de vos kills"), why.c_str());
  }
  return size.x + PartyPx(kMarkGapPixels);
}

// Les états, de droite à gauche depuis `right`, les plus RÉCENTS gardés quand il
// y en a trop. Rend vrai si le curseur est sur l'un d'eux : son infobulle parle
// alors à la place de celle de la ligne.
bool DrawRowEffects(const PartyMemberView& member, float right, float top,
                    const PartyFriendLook& look) {
  if (member.statuses.empty()) {
    return false;
  }
  const std::vector<StatusCellView> newest(member.statuses.rbegin(), member.statuses.rend());
  const float side = PartyPx(std::max(kLeastBuffPixels, static_cast<float>(look.buff_px)));

  StatusCellStyle style;
  style.sweep = static_cast<StatusSweep>(
      std::clamp(look.buff_sweep, 0, static_cast<int>(StatusSweep::kVertical)));
  style.sweep_color = look.buff_sweep_color;
  style.icon_background = look.buff_icon_background;
  const float countdown =
      look.buff_time_auto ? std::max(PartyPx(kLeastCountdownPixels), side * kCountdownShare)
                          : PartyPx(static_cast<float>(std::clamp(
                                look.buff_time_px, static_cast<int>(kSmallestStatusCountdownPixels),
                                static_cast<int>(kLargestStatusCountdownPixels))));
  style.countdown_pixels = look.buff_time ? countdown : 0.0F;
  style.countdown_background = look.buff_time_background;
  style.alpha = StatusOpacityOf(look.buff_opacity_percent);

  StatusRowLayout layout;
  layout.side = side;
  layout.gap = PartyPx(kBuffGapPixels);
  layout.below = style.countdown_pixels;
  layout.most = std::max(1, look.buff_max);
  layout.rows = std::max(1, look.buff_rows);
  layout.sort = StatusSort::kArrival;
  layout.right_to_left = true;

  const std::vector<std::size_t> order = OrderStatusCells(newest, layout.sort, layout.most);
  const std::vector<ImVec2> places = PlaceStatusRow(order.size(), ImVec2(right, top), layout);
  const bool window_hovered = ImGui::IsWindowHovered();
  bool over = false;
  for (std::size_t rank = 0; rank < std::min(order.size(), places.size()); ++rank) {
    const ImVec2 low = places[rank];
    const ImVec2 high(low.x + side, low.y + side);
    if (DrawStatusCell(newest[order[rank]], low, high, style, window_hovered) && window_hovered &&
        ImGui::IsMouseHoveringRect(low, high)) {
      over = true;
    }
  }
  return over;
}

// Une jauge de ligne, PV ou SP. `stacked_next` : une autre vient se coller
// dessous, et le texte passe alors dedans d'office — à côté, il pousserait la
// suivante d'une hauteur de ligne.
void DrawRowBar(int current, int most, ImU32 fill, int text_mode, bool stacked_next,
                const PartyFriendLook& look) {
  if (most <= 0) {
    return;
  }
  const float fraction =
      std::clamp(static_cast<float>(current) / static_cast<float>(most), 0.0F, 1.0F);
  const std::string text = PartyGaugeCaption(current, most, text_mode);
  const float width = PartyPx(std::max(kLeastBarWidthPixels, static_cast<float>(look.bar_w)));
  const float height = PartyPx(std::max(kLeastBarHeightPixels, static_cast<float>(look.bar_h)));
  const float font_size =
      look.text_px > 0 ? PartyPx(static_cast<float>(look.text_px)) : ImGui::GetFontSize();
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 low = ImGui::GetCursorScreenPos();
  const ImVec2 high(low.x + width, low.y + height);
  into->AddRectFilled(low, high, kGaugeBack);
  if (fraction > 0.0F) {
    into->AddRectFilled(low, ImVec2(low.x + width * fraction, high.y), fill);
  }
  into->AddRect(low, high, kGaugeBorder);

  const bool inside = look.text_in_bars || stacked_next;
  if (!text.empty() && inside) {
    ImFont* font = ImGui::GetFont();
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, text.c_str());
    const ImVec2 at(low.x + (width - text_size.x) * 0.5F, low.y + (height - text_size.y) * 0.5F);
    const ImVec4 clip(low.x, low.y, high.x, high.y);
    into->AddText(font, font_size, ImVec2(at.x + kShadowScreen, at.y + kShadowScreen), kGaugeShadow,
                  text.c_str(), nullptr, 0.0F, &clip);
    into->AddText(font, font_size, at, IM_COL32_WHITE, text.c_str(), nullptr, 0.0F, &clip);
  }
  ImGui::Dummy(ImVec2(width, height));
  if (!text.empty() && !inside) {
    ImGui::SameLine(0.0F, PartyPx(kBarTextGapPixels));
    ImGui::TextUnformatted(text.c_str());
  }
}

// Ce que la ligne ne peut pas porter : la classe, la carte entière, les PV et
// SP chiffrés, et la position sur une mini-carte.
void DrawRowTooltip(const PartyFriendView& view, const PartyMemberView& member) {
  if (!BeginSkinTooltip()) {
    return;
  }
  const std::string name(member.name);
  ImGui::TextUnformatted(name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("Lv.%d", member.base_level);
  ImGui::Separator();
  const std::string job(member.job_name);
  ImGui::TextDisabled("%s", job.c_str());
  if (member.offline) {
    ImGui::TextDisabled("%s", core::Tr("Hors ligne"));
    EndSkinTooltip();
    return;
  }
  if (!member.map_raw.empty()) {
    const std::string map(ShownMap(member));
    ImGui::TextDisabled("%s", map.c_str());
  }
  if (member.hp_known && member.max_hp > 0) {
    ImGui::Text("%s %d/%d", core::Tr("PV"), member.hp, member.max_hp);
  } else {
    ImGui::TextDisabled("%s", core::Tr("PV inconnus : hors de portée"));
  }
  if (member.sp_known && member.max_sp > 0) {
    ImGui::Text("%s %d/%d", core::Tr("SP"), member.sp, member.max_sp);
  }
  if (view.options.known && view.options.exp == 1) {
    const std::string why =
        PartyShareReason(PartyShareFlags(view, member), view.share_idle_seconds);
    if (!why.empty()) {
      ImGui::TextDisabled("%s %s", core::Tr("Hors du partage d'EXP :"), why.c_str());
    }
  }

  // La cellule (x, y) tombe sur la même fraction de l'image, y INVERSÉ : le
  // monde a son origine en bas.
  if (!member.map_raw.empty() && member.position_known) {
    const SkinImage& map = member.minimap;
    if (map.usable()) {
      const float width = PartyPx(kMinimapPixels);
      const float height = width * (map.height / map.width);
      const ImVec2 low = ImGui::GetCursorScreenPos();
      ImGui::Image(map.id, ImVec2(width, height));
      if (member.map_width_cells > 0 && member.map_height_cells > 0) {
        const float across = std::clamp(
            static_cast<float>(member.x) / static_cast<float>(member.map_width_cells), 0.0F, 1.0F);
        const float down = std::clamp(static_cast<float>(member.map_height_cells - member.y) /
                                          static_cast<float>(member.map_height_cells),
                                      0.0F, 1.0F);
        const ImVec2 dot(low.x + width * across, low.y + height * down);
        ImDrawList* into = ImGui::GetWindowDrawList();
        into->AddCircleFilled(dot, PartyPx(kMinimapDotOuterPixels), kMinimapDotHalo);
        into->AddCircleFilled(dot, PartyPx(kMinimapDotInnerPixels), kMinimapDot);
      }
    }
    ImGui::TextDisabled("%d, %d", member.x, member.y);
  }
  EndSkinTooltip();
}

[[nodiscard]] bool AnyPopupOpen() {
  return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

// La ligne 1 : « Lv.N Nom(Carte) ». La carte ne paraît que pour un membre en
// ligne.
[[nodiscard]] std::string RowLabel(const PartyMemberView& member, const PartyFriendLook& look) {
  std::string label;
  if (look.show_level) {
    std::array<char, 16> level = {};
    std::snprintf(level.data(), level.size(), "Lv.%d ", member.base_level);
    label = level.data();
  }
  label += member.name;
  if (member.offline || member.map_raw.empty() ||
      look.map_mode == static_cast<int>(PartyMapMode::kNone)) {
    return label;
  }
  std::string_view map = ShownMap(member);
  if (look.map_mode == static_cast<int>(PartyMapMode::kShort)) {
    map = ShortMapName(map);
  }
  if (!map.empty()) {
    label += '(';
    label += map;
    label += ')';
  }
  return label;
}

// La ligne 2 : les jauges, ou l'état quand il n'y a pas de PV. Pas de jauge
// vide pour des PV inconnus : elle se lirait « à zéro ».
void DrawGauges(const PartyMemberView& member, const PartyFriendLook& look) {
  if (member.offline) {
    ImGui::TextDisabled("%s", core::Tr("Hors ligne"));
    return;
  }
  if (!member.hp_known || member.max_hp <= 0) {
    ImGui::TextDisabled("%s", core::Tr("Hors de portée"));
    if (ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s", core::Tr("Le client ne connaît les PV que des membres visibles."));
    }
    return;
  }
  const bool has_sp = look.show_sp && member.sp_known && member.max_sp > 0;
  const bool stack = look.bars_stacked && has_sp && look.show_hp_bar;
  if (stack) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0F));
  }
  if (look.show_hp_bar) {
    DrawRowBar(member.hp, member.max_hp, kHealthFill, look.hp_text_mode, stack, look);
  } else if (look.hp_text_mode != static_cast<int>(PartyGaugeText::kNone)) {
    ImGui::TextUnformatted(PartyGaugeCaption(member.hp, member.max_hp, look.hp_text_mode).c_str());
  }
  if (has_sp) {
    DrawRowBar(member.sp, member.max_sp, kMagicFill, look.sp_text_mode, false, look);
  }
  if (stack) {
    ImGui::PopStyleVar();
  }
}

}  // namespace

void DrawPartyRow(PartyFriendWindowState& state, const PartyFriendView& view,
                  const PartyMemberView& member, const PartyFriendLook& look,
                  PartyFriendAction& action) {
  const float icon = PartyPx(std::max(kLeastIconPixels, static_cast<float>(look.icon_px)));
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float row_width = ImGui::GetContentRegionAvail().x;

  // La case de l'icône sert aussi à la tête : l'un ou l'autre réglage la garde.
  const bool head_wanted = (look.head_mode & kPartyHeadInParty) != 0;
  const bool thumb = look.show_job_icon || head_wanted;
  if (thumb) {
    ImGui::Dummy(ImVec2(icon, icon));
    // La classe, quand l'infobulle de ligne — qui la porte — est éteinte.
    if (!look.show_tooltip && ImGui::IsItemHovered()) {
      const std::string job(member.job_name);
      SkinTooltipFormat("%s", job.c_str());
    }
    ImGui::SameLine();
  }

  ImGui::BeginGroup();
  if (member.leader && view.crown.usable()) {
    const float side = ImGui::GetTextLineHeight();
    ImGui::Image(view.crown.id, ImVec2(side, side));
    ImGui::SameLine(0.0F, kCrownGapScreen);
  }
  const std::string label = RowLabel(member, look);
  if (member.offline) {
    ImGui::TextDisabled("%s", label.c_str());
  } else {
    ImGui::TextUnformatted(label.c_str());
  }
  DrawGauges(member, look);
  ImGui::EndGroup();

  if (thumb) {
    const float content_height = std::max(icon, ImGui::GetItemRectSize().y);
    const ImVec2 at(origin.x, origin.y + (content_height - icon) * 0.5F);
    // La tête d'abord ; l'icône en repli, si elle est demandée.
    const bool head_drawn = head_wanted && !member.offline && member.head.usable();
    if (head_drawn) {
      DrawHead(member.head, at, icon);
    } else if (look.show_job_icon && member.job_icon.usable()) {
      ImGui::GetWindowDrawList()->AddImage(member.job_icon.id, at, ImVec2(at.x + icon, at.y + icon),
                                           ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                                           member.offline ? kOfflineTint : IM_COL32_WHITE);
    }
  }

  const char* badge = member.offline ? "OFF" : (member.is_me ? "ME" : "ON");
  DrawStatusBadge(badge, member.offline ? kBadgeOffline : (member.is_me ? kBadgeMe : kBadgeOnline),
                  member.offline ? core::Tr("Hors ligne")
                  : member.is_me ? core::Tr("Votre personnage")
                                 : core::Tr("En ligne"));
  const float badge_left = ImGui::GetItemRectMin().x;
  const float badge_top = ImGui::GetItemRectMin().y;

  // « -EXP » seulement quand l'EXP est partagée : en chacun pour soi, personne
  // n'est écarté de rien.
  bool share_hovered = false;
  float share_width = 0.0F;
  if (view.options.known && view.options.exp == 1) {
    const std::string why =
        PartyShareReason(PartyShareFlags(view, member), view.share_idle_seconds);
    if (!why.empty()) {
      share_width =
          DrawShareMark(badge_left - PartyPx(kMarkGapPixels), badge_top, why, share_hovered);
    }
  }
  bool state_hovered = false;
  if (look.show_buffs && !member.offline) {
    state_hovered =
        DrawRowEffects(member, badge_left - PartyPx(kMarkGapPixels) - share_width, badge_top, look);
  }

  const float bottom = ImGui::GetCursorScreenPos().y;
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddLine(ImVec2(origin.x, bottom), ImVec2(origin.x + row_width, bottom), kRowRule);

  const ImVec2 row_high(origin.x + row_width, bottom);
  const bool row_hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, row_high);
  // Un sprite représente ce membre : son clic droit ouvre le menu d'entité, qui
  // porte déjà tout ce qu'on ferait ici.
  if (row_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    if (!member.is_me && !member.offline && member.actor_present) {
      action.verb = PartyFriendVerb::kEntityMenu;
      action.account_id = member.account_id;
      action.char_id = member.char_id;
      action.name = std::string(member.name);
    } else {
      ImGui::OpenPopup(kPartyRowMenuId);
    }
  }
  // Pas de sprite, pas de cible : un HUD de cible vide ne vaut pas un clic muet.
  const bool targetable =
      look.click_targets && !member.offline && (member.is_me || member.actor_present);
  if (targetable && row_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    action.verb = PartyFriendVerb::kTarget;
    action.account_id = member.account_id;
  }
  if (look.click_targets) {
    if (member.account_id != 0 && member.account_id == view.target_account_id) {
      into->AddRect(origin, row_high, kTargetBorder, 0.0F, 0, kTargetBorderScreen);
    } else if (targetable && row_hovered) {
      into->AddRect(origin, row_high, kTargetHover);
    }
  }
  if (look.show_tooltip && row_hovered && !state_hovered && !share_hovered && !AnyPopupOpen()) {
    DrawRowTooltip(view, member);
  }
  DrawPartyRowMenu(state, view, member, action);
}

void DrawFriendRow(PartyFriendWindowState& state, const PartyFriendView& view,
                   const FriendView& row, const PartyFriendLook& look, PartyFriendAction& action) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float row_width = ImGui::GetContentRegionAvail().x;

  // Un ami est presque toujours hors de portée : sans tête connue, rien n'est
  // réservé.
  const bool head_here =
      (look.head_mode & kPartyHeadInFriends) != 0 && row.online && row.head.usable();
  const float head_box = ImGui::GetTextLineHeight() + PartyPx(kHeadSlackPixels);
  std::string label = head_here ? "      " : "";
  label += row.name;
  if (row.online) {
    ImGui::TextUnformatted(label.c_str());
  } else {
    ImGui::TextDisabled("%s", label.c_str());
  }
  if (head_here) {
    DrawHead(row.head, ImVec2(origin.x, origin.y - PartyPx(kHeadDropPixels)), head_box);
  }
  DrawStatusBadge(row.online ? "ON" : "OFF", row.online ? kBadgeOnline : kBadgeOffline,
                  row.online ? core::Tr("En ligne") : core::Tr("Hors ligne"));

  const float bottom = ImGui::GetCursorScreenPos().y;
  if (ImGui::IsWindowHovered() &&
      ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + row_width, bottom)) &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    if (row.online && row.actor_present) {
      action.verb = PartyFriendVerb::kEntityMenu;
      action.account_id = row.account_id;
      action.char_id = row.char_id;
      action.name = std::string(row.name);
    } else {
      ImGui::OpenPopup(kPartyRowMenuId);
    }
  }
  DrawFriendRowMenu(state, view, row, action);
}

}  // namespace ro::ui
