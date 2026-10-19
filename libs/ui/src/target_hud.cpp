// Le HUD de cible : six cadres frères.

#include "ro/ui/target_hud.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/gauge_fill.h"
#include "ro/ui/hud_caption.h"
#include "ro/ui/status_bar.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// De quoi écrire « 4294967295 / 4294967295 (100 %) » et le mot le plus long
// d'une traduction.
constexpr std::size_t kCaptionRoom = 96;

// L'identité ImGui de chaque cadre, stable et unique, sous un préfixe commun
// qui permet de reconnaître la famille.
constexpr std::string_view kPartIdPrefix = "##target_hud_";
constexpr std::array<const char*, kTargetPartCount> kPartIds = {
    "##target_hud_portrait", "##target_hud_name", "##target_hud_health",
    "##target_hud_magic",    "##target_hud_kind", "##target_hud_statuses",
};

// Ce dont le contenu d'un cadre se retire de ses bords.
constexpr float kInnerPadding = 3.0F;

constexpr ImU32 kCaptionColor = IM_COL32_WHITE;

// La réserve de magie qu'un monstre n'a pas : le serveur écrit 1 faute de
// pouvoir écrire 0.
constexpr std::uint32_t kAbsentMagicPool = 1;

// Le nom occupe la bande haute quand une ligne sociale le suit.
constexpr float kNameBandShare = 0.55F;
constexpr float kSocialDimming = 0.75F;
constexpr float kSocialTextShare = 0.85F;
// L'air laissé autour du niveau, réservé à droite du nom.
constexpr float kLevelRoom = 8.0F;

constexpr float kPercent = 100.0F;

[[nodiscard]] ImU32 Dimmed(ImU32 color, float share) {
  const ImVec4 lit = ImGui::ColorConvertU32ToFloat4(color);
  return ImGui::ColorConvertFloat4ToU32(ImVec4(lit.x * share, lit.y * share, lit.z * share, lit.w));
}

// La géométrie d'origine et les teintes d'origine, cadre par cadre.
constexpr std::array<HudRect, kTargetPartCount> kDefaultRects = {{
    {.x = 20.0F, .y = 20.0F, .width = 72.0F, .height = 72.0F},
    {.x = 96.0F, .y = 20.0F, .width = 190.0F, .height = 34.0F},
    {.x = 96.0F, .y = 56.0F, .width = 190.0F, .height = 16.0F},
    {.x = 96.0F, .y = 74.0F, .width = 190.0F, .height = 16.0F},
    {.x = 20.0F, .y = 96.0F, .width = 266.0F, .height = 18.0F},
    {.x = 20.0F, .y = 118.0F, .width = 266.0F, .height = 30.0F},
}};
constexpr ImU32 kFrameBackground = IM_COL32(13, 13, 18, 199);
constexpr ImU32 kStatusBackground = IM_COL32(13, 13, 18, 140);
constexpr ImU32 kHealthFill = IM_COL32(217, 69, 69, 255);
constexpr ImU32 kMagicFill = IM_COL32(77, 158, 242, 255);
constexpr ImU32 kKindText = IM_COL32(199, 199, 217, 255);

void PaintName(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const TargetView& target,
               ImU32 ink, float pixels) {
  const std::string social = target.is_player ? std::string(target.guild) : std::string();
  const bool two_lines = !social.empty();
  const float split =
      two_lines ? top_left.y + (bottom_right.y - top_left.y) * kNameBandShare : bottom_right.y;

  // La place du niveau est réservée AVANT de centrer le nom : sans cela un nom
  // long passerait sous le niveau.
  std::array<char, 32> level{};
  float level_width = 0.0F;
  ImFont* font = ImGui::GetFont();
  if (target.level.has_value() && *target.level > 0 && font != nullptr) {
    std::snprintf(level.data(), level.size(), "Lv. %d", static_cast<int>(*target.level));
    level_width = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, level.data()).x + kLevelRoom;
  }
  const float name_right = bottom_right.x - level_width;

  const std::string name =
      target.name.empty() ? std::string(core::Tr("(nom inconnu)")) : std::string(target.name);
  PaintHudCaption(into, top_left, ImVec2(name_right, split), name.c_str(), ink, pixels);
  if (two_lines) {
    PaintHudCaption(into, ImVec2(top_left.x, split), bottom_right, social.c_str(),
                    Dimmed(ink, kSocialDimming), pixels * kSocialTextShare);
  }
  if (level[0] != '\0') {
    PaintHudCaption(into, ImVec2(name_right, top_left.y), ImVec2(bottom_right.x, split),
                    level.data(), ink, pixels);
  }
}

void PaintGauge(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const TargetView& target,
                bool is_magic, const TargetHudLook& look, ImU32 fill_color, float pixels) {
  const auto& value = is_magic ? target.magic : target.health;
  const auto& most = is_magic ? target.max_magic : target.max_health;
  const bool known = value.has_value() && most.has_value();
  const std::uint32_t now = known ? *value : 0;
  const std::uint32_t full = known ? *most : 0;

  // 🔴 DANS L'INTÉRIEUR DU CADRE : son trait se peint AVANT le contenu, et une
  // part pleine posée sur toute la surface le recouvrait. Voir `gauge_fill.h`.
  const RoundedBox interior = GaugeInterior(top_left, bottom_right, look.rounding,
                                            look.border ? kHudFrameBorderPixels : 0.0F);
  PaintGaugeFill(into, interior, GaugeFill(known, now, full, is_magic), GaugeGrowth::kRightward,
                 fill_color);
  const std::string caption =
      GaugeCaption(known, now, full, GaugeTextFor(target, is_magic, GaugeTextOf(look.gauge_text)),
                   GaugeNumbersAllowed(target, is_magic, known), is_magic);
  if (!caption.empty()) {
    PaintHudCaption(into, top_left, bottom_right, caption.c_str(), kCaptionColor, pixels);
  }
}

void PaintStatuses(ImVec2 top_left, ImVec2 bottom_right, const TargetView& target,
                   const TargetHudLook& look) {
  if (target.statuses.empty()) {
    return;
  }
  const float scale = Scale();
  StatusRowLayout row;
  row.side = scale * static_cast<float>(std::max(8, look.status_icon_pixels));
  row.gap = scale * static_cast<float>(std::max(0, look.status_gap_pixels));
  row.most = look.status_most;
  row.rows = look.status_rows;
  row.sort = StatusSortOf(look.status_sort);
  row.limit = bottom_right.x - kInnerPadding;

  StatusCellStyle style;
  style.sweep = StatusSweepOf(look.status_sweep);
  style.sweep_color = look.status_sweep_color;
  style.icon_background = look.status_icon_background;
  style.countdown_pixels =
      look.status_countdown ? scale * static_cast<float>(std::max(7, look.status_countdown_pixels))
                            : 0.0F;
  row.below = style.countdown_pixels;

  DrawStatusRow(target.statuses, ImVec2(top_left.x + kInnerPadding, top_left.y + kInnerPadding),
                row, style, /*tooltip=*/true);
}

void PaintPortrait(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const TargetView& target,
                   ImU32 ink, float pixels) {
  const ImVec2 inside_top(top_left.x + kInnerPadding, top_left.y + kInnerPadding);
  const ImVec2 inside_bottom(bottom_right.x - kInnerPadding, bottom_right.y - kInnerPadding);
  if (target.portrait.usable()) {
    into.AddImage(target.portrait.id, inside_top, inside_bottom);
    return;
  }
  if (target.model) {
    // Ces classes n'ont aucun sprite : le dire vaut mieux qu'un cadre vide.
    PaintHudCaption(into, top_left, bottom_right, core::Tr("(modèle 3D)"), ink, pixels);
  }
}

}  // namespace

GaugeText GaugeTextOf(int stored) noexcept {
  if (stored < 0 || stored > static_cast<int>(GaugeText::kBoth)) {
    return GaugeText::kValues;
  }
  return static_cast<GaugeText>(stored);
}

HudRect DefaultTargetPartRect(TargetPart part) {
  return kDefaultRects[static_cast<std::size_t>(part)];
}

std::array<TargetPartLook, kTargetPartCount> DefaultTargetParts() {
  std::array<TargetPartLook, kTargetPartCount> parts{};
  for (std::size_t rank = 0; rank < kTargetPartCount; ++rank) {
    parts[rank].rect = kDefaultRects[rank];
    parts[rank].background = kFrameBackground;
    parts[rank].foreground = IM_COL32_WHITE;
  }
  parts[static_cast<std::size_t>(TargetPart::kHealth)].foreground = kHealthFill;
  parts[static_cast<std::size_t>(TargetPart::kMagic)].foreground = kMagicFill;
  parts[static_cast<std::size_t>(TargetPart::kKind)].foreground = kKindText;
  auto& statuses = parts[static_cast<std::size_t>(TargetPart::kStatuses)];
  statuses.shown = false;
  statuses.background = kStatusBackground;
  return parts;
}

std::string GaugeCaption(bool known, std::uint32_t current, std::uint32_t most, GaugeText mode,
                         bool numbers_allowed, bool is_magic) {
  if (mode == GaugeText::kNothing || !numbers_allowed) {
    return {};
  }
  // « inconnus » plutôt qu'un zéro : une jauge vide et une jauge qu'on ne
  // connaît pas ne disent pas la même chose.
  if (!known) {
    return std::string(core::Tr("inconnus"));
  }
  if (is_magic && most <= kAbsentMagicPool) {
    return "No SP";
  }
  const float ratio = GaugeFill(known, current, most, is_magic);
  std::array<char, kCaptionRoom> written{};
  switch (mode) {
    case GaugeText::kPercent:
      std::snprintf(written.data(), written.size(), "%.0f %%",
                    static_cast<double>(ratio * kPercent));
      break;
    case GaugeText::kValues:
      std::snprintf(written.data(), written.size(), "%u / %u", current, most);
      break;
    case GaugeText::kBoth:
    default:
      std::snprintf(written.data(), written.size(), "%u / %u (%.0f %%)", current, most,
                    static_cast<double>(ratio * kPercent));
      break;
  }
  return std::string(written.data());
}

float GaugeFill(bool known, std::uint32_t current, std::uint32_t most, bool is_magic) {
  if (!known || most == 0) {
    return 0.0F;
  }
  if (is_magic && most <= kAbsentMagicPool) {
    return 0.0F;
  }
  const float part = static_cast<float>(current) / static_cast<float>(most);
  return std::clamp(part, 0.0F, 1.0F);
}

bool GaugeNumbersAllowed(const TargetView& target, bool is_magic, bool known) {
  return GaugeNumbersAllowed(target.is_player, target.ally, is_magic, known);
}

bool GaugeNumbersAllowed(bool is_player, bool ally, bool is_magic, bool known) {
  return !(!is_magic && is_player && known && !ally);
}

GaugeText GaugeTextFor(const TargetView& target, bool is_magic, GaugeText chosen) {
  if (is_magic || !target.health_in_percent || chosen == GaugeText::kNothing) {
    return chosen;
  }
  return GaugeText::kPercent;
}

bool TargetPartApplies(TargetPart part, const TargetView& target) {
  if (target.is_npc && (part == TargetPart::kHealth || part == TargetPart::kMagic)) {
    return false;
  }
  if (target.is_player && part == TargetPart::kKind) {
    return false;
  }
  // Le SP d'un joueur ne se ferme que devant un inconnu.
  if (target.is_player && part == TargetPart::kMagic && !target.ally) {
    return false;
  }
  return true;
}

bool TargetHudHeld(const HudGrip& grip) { return grip.held.starts_with(kPartIdPrefix); }

void DrawTargetHud(const TargetView& target, const AlignGrid& grid, TargetHudLook& look,
                   HudGrip& grip, TargetHudAnswer& answer) {
  // Sans cible, les cadres ne restent qu'en mode placement, ou tant que l'un
  // d'eux est saisi : on ne retire pas sous les doigts ce qu'on pose.
  if (!target.present && !look.layout_mode && !TargetHudHeld(grip)) {
    return;
  }

  std::array<HudRect, kTargetPartCount> rects{};
  std::array<bool, kTargetPartCount> shown{};
  for (std::size_t rank = 0; rank < kTargetPartCount; ++rank) {
    rects[rank] = look.parts[rank].rect;
    shown[rank] = look.parts[rank].shown;
  }
  const float pixels = ImGui::GetFontSize() *
                       std::clamp(look.text_scale, kTargetTextScaleLeast, kTargetTextScaleMost);

  for (std::size_t rank = 0; rank < kTargetPartCount; ++rank) {
    const auto part = static_cast<TargetPart>(rank);
    if (!look.parts[rank].shown || !TargetPartApplies(part, target)) {
      continue;
    }

    HudFrameLook frame;
    frame.locked = look.locked;
    frame.border = look.border;
    frame.rounding = look.rounding;
    frame.background = look.parts[rank].background;
    frame.grid = &grid;
    frame.sticky = look.sticky;
    frame.clickable = target.clickable;

    HudSiblings siblings;
    siblings.frames = rects;
    siblings.shown = shown;
    siblings.mine = static_cast<int>(rank);

    bool moved = false;
    HudFrameClicks clicks;
    // `rects[rank]` lui-même : le cadre y écrit, et écrit aussi chez ses frères
    // en déplacement de bloc.
    if (BeginHudFrame(kPartIds[rank], rects[rank], frame, siblings, grip, &moved, &clicks)) {
      ImDrawList* into = ImGui::GetWindowDrawList();
      const ImVec2 top_left = ImGui::GetWindowPos();
      const ImVec2 size = ImGui::GetWindowSize();
      const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);
      const ImU32 ink = look.parts[rank].foreground;

      if (into != nullptr) {
        switch (part) {
          case TargetPart::kPortrait:
            if (target.present) {
              PaintPortrait(*into, top_left, bottom_right, target, ink, pixels);
            }
            break;
          case TargetPart::kName:
            if (target.present) {
              PaintName(*into, top_left, bottom_right, target, ink, pixels);
            }
            break;
          case TargetPart::kHealth:
          case TargetPart::kMagic:
            PaintGauge(*into, top_left, bottom_right, target, part == TargetPart::kMagic, look, ink,
                       pixels);
            break;
          case TargetPart::kKind:
            if (target.present) {
              const std::string species(target.species);
              PaintHudCaption(*into, top_left, bottom_right, species.c_str(), ink, pixels);
            }
            break;
          case TargetPart::kStatuses:
            PaintStatuses(top_left, bottom_right, target, look);
            break;
        }
      }
    }
    EndHudFrame(grip);

    answer.moved = answer.moved || moved;
    answer.hovered = answer.hovered || clicks.hovered;
    answer.left_click = answer.left_click || clicks.left;
    answer.right_click = answer.right_click || clicks.right;
  }

  // Les frères emportés par un bloc ont bougé aussi : tous se recopient.
  for (std::size_t rank = 0; rank < kTargetPartCount; ++rank) {
    look.parts[rank].rect = rects[rank];
  }
}

}  // namespace ro::ui
