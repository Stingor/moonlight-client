#include "ro/ui/party_grid.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "party_grid_parts.h"
#include "ui_popup.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr const char* kFrameId = "##party_frames";

constexpr float kFramePaddingPixels = 3.0F;
constexpr float kFrameRoundingPixels = 3.0F;
constexpr float kLeastFrameWidthPixels = 60.0F;
constexpr float kLeastFrameHeightPixels = 20.0F;

constexpr int kPercentScale = 100;
constexpr std::size_t kCaptionBytes = 48;
// Arrondi au plus proche d'un nombre de colonnes déduit d'une largeur.
constexpr float kNearestColumn = 0.5F;

[[nodiscard]] bool Shown(const PartyGridTileView& tile, const PartyGridLook& look) {
  if (!look.show_self && tile.is_me) {
    return false;
  }
  return look.show_offline || !tile.offline;
}

}  // namespace

PartyGridLayout PartyGridLayoutOf(const PartyGridLook& look, std::size_t shown_count, float scale) {
  PartyGridLayout layout;
  const int count = static_cast<int>(shown_count);
  const int asked = std::clamp(look.columns, kPartyGridFewestColumns, kPartyGridMostColumns);
  layout.columns = std::max(1, std::min(asked, count));
  layout.rows = (count + layout.columns - 1) / layout.columns;
  layout.gap = static_cast<float>(std::clamp(look.gap_px, 0, kPartyGridWidestGapPixels)) * scale;
  layout.tile_width =
      static_cast<float>(std::clamp(look.tile_width_px, kPartyGridNarrowestTilePixels,
                                    kPartyGridWidestTilePixels)) *
      scale;
  layout.tile_height =
      static_cast<float>(std::clamp(look.tile_height_px, kPartyGridShortestTilePixels,
                                    kPartyGridTallestTilePixels)) *
      scale;
  layout.padding = kFramePaddingPixels * scale;

  // ⚠ TRONQUÉES AU PIXEL ENTIER : le cadre se pose au pixel, et une largeur
  // fractionnaire ferait dériver sa comparaison à la largeur canonique.
  const auto columns = static_cast<float>(layout.columns);
  const auto rows = static_cast<float>(layout.rows);
  layout.frame_width = static_cast<float>(
      static_cast<int>(columns * layout.tile_width + std::max(0.0F, columns - 1.0F) * layout.gap +
                       layout.padding * 2.0F));
  layout.frame_height = static_cast<float>(
      static_cast<int>(rows * layout.tile_height + std::max(0.0F, rows - 1.0F) * layout.gap +
                       layout.padding * 2.0F));
  return layout;
}

ImVec2 PartyGridTileAt(const PartyGridLayout& layout, ImVec2 frame_top_left, std::size_t rank) {
  const auto columns = static_cast<std::size_t>(std::max(1, layout.columns));
  const auto column = static_cast<float>(rank % columns);
  const auto row = static_cast<float>(rank / columns);
  return ImVec2(frame_top_left.x + layout.padding + column * (layout.tile_width + layout.gap),
                frame_top_left.y + layout.padding + row * (layout.tile_height + layout.gap));
}

int PartyGridColumnsForWidth(const PartyGridLayout& layout, float frame_width) {
  const float unit = layout.tile_width + layout.gap;
  if (unit <= 1.0F) {
    return layout.columns;
  }
  const int columns = static_cast<int>(((frame_width - layout.padding * 2.0F + layout.gap) / unit) +
                                       kNearestColumn);
  return std::clamp(columns, kPartyGridFewestColumns, kPartyGridMostColumns);
}

ImU32 PartyGridHpInk(const PartyGridLook& look, int hp, int max_hp) {
  if (max_hp <= 0) {
    return look.hp_low;
  }
  const float fraction =
      std::clamp(static_cast<float>(hp) / static_cast<float>(max_hp), 0.0F, 1.0F);
  const int percent = static_cast<int>(fraction * static_cast<float>(kPercentScale));
  if (percent <= look.hp_low_percent) {
    return look.hp_low;
  }
  if (percent <= look.hp_mid_percent) {
    return look.hp_mid;
  }
  return look.hp_high;
}

int PartyGridHpPercent(int hp, int max_hp) {
  if (hp <= 0 || max_hp <= 0) {
    return 0;
  }
  return std::max(1, static_cast<int>((static_cast<float>(hp) / static_cast<float>(max_hp)) *
                                      static_cast<float>(kPercentScale)));
}

std::string PartyGridHpCaption(PartyGridHpText mode, int hp, int max_hp) {
  char written[kCaptionBytes] = {};
  const int percent = PartyGridHpPercent(hp, max_hp);
  switch (mode) {
    case PartyGridHpText::kNumbers:
      std::snprintf(written, sizeof(written), "%d/%d", hp, max_hp);
      break;
    case PartyGridHpText::kPercent:
      std::snprintf(written, sizeof(written), "%d %%", percent);
      break;
    case PartyGridHpText::kBoth:
      std::snprintf(written, sizeof(written), "%d/%d (%d %%)", hp, max_hp, percent);
      break;
    case PartyGridHpText::kNone:
      break;
  }
  return written;
}

PartyGridHpText PartyGridHpTextOf(int stored) noexcept {
  const int bounded = std::clamp(stored, static_cast<int>(PartyGridHpText::kNone),
                                 static_cast<int>(PartyGridHpText::kBoth));
  return static_cast<PartyGridHpText>(bounded);
}

PartyGridAnswer DrawPartyGrid(const PartyGridView& view, PartyGridLook& look, PartyGridState& state,
                              const AlignGrid& grid, HudGrip& grip) {
  PartyGridAnswer answer;
  if (!look.shown) {
    return answer;
  }

  // Filtrées AVANT la géométrie : une tuile vide au milieu casserait la lecture.
  std::vector<const PartyGridTileView*> shown;
  shown.reserve(view.tiles.size());
  for (const PartyGridTileView& tile : view.tiles) {
    if (Shown(tile, look)) {
      shown.push_back(&tile);
    }
  }
  if (shown.empty()) {
    return answer;
  }

  const float scale = Scale();
  const PartyGridLayout layout = PartyGridLayoutOf(look, shown.size(), scale);
  // La largeur suit la souris pendant qu'on tire le cadre ; relâché, elle
  // reprend la valeur que les colonnes déduites donnent. La hauteur suit
  // toujours le nombre de rangées.
  if (!grip.Holds(kFrameId)) {
    look.rect.width = layout.frame_width;
  }
  look.rect.height = layout.frame_height;

  // MAJ déverrouille le temps d'un déplacement, mais seulement curseur SUR la
  // grille : ailleurs, MAJ+clic reste l'attaque forcée du jeu. Un geste en cours
  // garde la main même quand le curseur sort du cadre en l'agrandissant.
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const HudRect on_screen = HudOnScreen(look.rect, ImGui::GetIO().DisplaySize);
  const bool over_frame =
      ImRect(ImVec2(on_screen.x, on_screen.y),
             ImVec2(on_screen.x + on_screen.width, on_screen.y + on_screen.height))
          .Contains(mouse);
  const bool unlock_override = ImGui::GetIO().KeyShift && (over_frame || grip.Holds(kFrameId));

  HudFrameLook frame;
  frame.locked = look.locked && !unlock_override;
  frame.border = false;
  frame.center_mark = true;
  frame.rounding = kFrameRoundingPixels * scale;
  frame.background = look.frame_background;
  frame.grid = &grid;
  frame.least_width = kLeastFrameWidthPixels * scale;
  frame.least_height = kLeastFrameHeightPixels * scale;
  frame.clickable = look.clickable;

  const HudSiblings alone;
  bool moved = false;
  bool status_hovered = false;
  HudFrameClicks clicks;
  const PartyGridTileView* hovered = nullptr;

  if (BeginHudFrame(kFrameId, look.rect, frame, alone, grip, &moved, &clicks)) {
    // L'origine vient de la FENÊTRE : déverrouillé, le cadre pose un bouton
    // invisible qui laisse le curseur ImGui sous lui.
    const ImVec2 window = ImGui::GetWindowPos();
    ImDrawList* into = ImGui::GetWindowDrawList();
    for (std::size_t rank = 0; rank < shown.size(); ++rank) {
      const ImVec2 top_left = PartyGridTileAt(layout, window, rank);
      const ImVec2 bottom_right(top_left.x + layout.tile_width, top_left.y + layout.tile_height);
      // Un test de rectangle et non un survol ImGui : verrouillé, le cadre est
      // sourd, et la grille répond « sur qui ? » sans prendre le clic au jeu.
      const bool here = ImRect(top_left, bottom_right).Contains(mouse);
      if (here) {
        hovered = shown[rank];
      }
      if (into != nullptr) {
        party_grid::DrawTile(*into, *shown[rank], top_left, bottom_right, look,
                             party_grid::TilePaint{.scale = scale, .hovered = here},
                             &status_hovered);
      }
    }

    if (hovered != nullptr) {
      // Pas d'acteur, pas de ciblage — sauf moi, qui me cible toujours.
      const bool reachable = hovered->is_me || hovered->in_sight;
      if (clicks.left && reachable) {
        answer.target_account_id = hovered->account_id;
      }
      if (clicks.right) {
        party_grid::AskMemberMenu(*hovered, view, state, answer);
      }
    }
  }
  EndHudFrame(grip);

  if (hovered != nullptr) {
    answer.hovered_account_id = hovered->account_id;
    if (look.cast_on_tile && (hovered->is_me || hovered->in_sight)) {
      answer.cast_account_id = hovered->account_id;
    }
  }

  // Aucune infobulle par-dessus un menu ouvert, ni sur une icône d'état, qui
  // porte la sienne.
  if (look.show_tooltip && hovered != nullptr && !status_hovered && !AnyPopupOpen()) {
    party_grid::DrawTileTooltip(*hovered, look);
  }
  party_grid::DrawMemberMenu(state, view, answer);

  if (moved) {
    // Seule une LARGEUR tirée change les colonnes. Un simple déplacement les
    // garde, même quand il y a moins de membres que de colonnes demandées.
    if (look.rect.width != layout.frame_width) {
      look.columns = PartyGridColumnsForWidth(layout, look.rect.width);
    }
    answer.moved = true;
  }
  return answer;
}

}  // namespace ro::ui
