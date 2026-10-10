#include "ro/ui/status_bar.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <imgui_internal.h>

#include "ro/ui/menu_icons.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/window_name.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Ce dont la barre se décolle du haut de l'écran et de la grille d'icônes, la
// toute première fois.
constexpr float kFirstPlaceMargin = 12.0F;

// Le cadre qu'on voit quand elle est déverrouillée. ⚠ IL N'EXISTE QUE LÀ : une
// barre d'états bordée en permanence serait un panneau posé sur le monde, et
// elle en cacherait plus qu'elle n'en montre.
constexpr float kPlacingBorderThickness = 1.0F;

// Ce que la barre empile de variables de style, et qu'elle doit dépiler par tous
// ses chemins de sortie.
constexpr int kStatusBarStyleVars = 2;

// Ce dont le fond deborde des cases, et son arrondi : le cadre RO, en plus
// discret.
constexpr float kBackgroundPadding = 3.0F;
constexpr float kBackgroundRounding = 3.0F;

template <typename Enum>
[[nodiscard]] Enum Bounded(int stored, Enum most) noexcept {
  if (stored < 0 || stored > static_cast<int>(most)) {
    return Enum{};
  }
  return static_cast<Enum>(stored);
}

}  // namespace

StatusAnchor StatusAnchorOf(int stored) noexcept {
  return Bounded(stored, StatusAnchor::kBottomRight);
}

StatusSort StatusSortOf(int stored) noexcept { return Bounded(stored, StatusSort::kLongest); }

StatusSweep StatusSweepOf(int stored) noexcept { return Bounded(stored, StatusSweep::kVertical); }

StatusBarGrid GridOfStatusBar(const StatusBarLayout& look) {
  StatusBarGrid grid;
  const int most = std::clamp(look.most, kFewestStatusIcons, kMostStatusIcons);
  const int rows = std::clamp(look.rows, 1, kMostStatusRows);
  grid.rows = static_cast<std::size_t>(rows);
  // La MÊME formule que `PlaceStatusRow` : le plafond réparti sur les lignes,
  // arrondi vers le haut.
  grid.columns = static_cast<std::size_t>((most + rows - 1) / rows);
  return grid;
}

float StatusCountdownRoom(const StatusBarLayout& look, float scale) {
  if (!look.show_countdown) {
    return 0.0F;
  }
  return std::clamp(look.countdown_px, kSmallestStatusCountdownPixels,
                    kLargestStatusCountdownPixels) *
         scale;
}

ImVec2 StatusBarSize(const StatusBarLayout& look, float scale) {
  const StatusBarGrid grid = GridOfStatusBar(look);
  const float side =
      std::clamp(look.side_px, kSmallestStatusSidePixels, kLargestStatusSidePixels) * scale;
  const float gap = std::clamp(look.gap_px, 0.0F, kLargestStatusGapPixels) * scale;
  const float below = StatusCountdownRoom(look, scale);
  const auto across = static_cast<float>(grid.columns);
  const auto down = static_cast<float>(grid.rows);
  return ImVec2(across * side + (across - 1.0F) * gap, down * (side + below) + (down - 1.0F) * gap);
}

ImVec2 StatusBarOrigin(ImVec2 window_pos, ImVec2 size, StatusAnchor anchor) {
  const bool rightward = anchor == StatusAnchor::kTopRight || anchor == StatusAnchor::kBottomRight;
  const bool downward = anchor == StatusAnchor::kBottomLeft || anchor == StatusAnchor::kBottomRight;
  return ImVec2(rightward ? window_pos.x + size.x : window_pos.x,
                downward ? window_pos.y + size.y : window_pos.y);
}

StatusRowLayout RowOfStatusBar(const StatusBarLayout& look, float scale) {
  const StatusAnchor anchor = StatusAnchorOf(look.anchor);
  StatusRowLayout row;
  row.side = std::clamp(look.side_px, kSmallestStatusSidePixels, kLargestStatusSidePixels) * scale;
  row.gap = std::clamp(look.gap_px, 0.0F, kLargestStatusGapPixels) * scale;
  row.most = std::clamp(look.most, kFewestStatusIcons, kMostStatusIcons);
  row.rows = std::clamp(look.rows, 1, kMostStatusRows);
  row.below = StatusCountdownRoom(look, scale);
  row.sort = StatusSortOf(look.sort);
  row.right_to_left = anchor == StatusAnchor::kTopRight || anchor == StatusAnchor::kBottomRight;
  row.upward = anchor == StatusAnchor::kBottomLeft || anchor == StatusAnchor::kBottomRight;
  return row;
}

void DrawStatusBar(const StatusBarLayout& look, std::span<const StatusCellView> cells) {
  if (!look.shown) {
    return;
  }

  const float scale = Scale();
  const ImVec2 size = StatusBarSize(look, scale);

  // ⚠ UNE IDENTITÉ QUI NE CHANGE JAMAIS, et un titre qui se traduit : c'est ce
  // que `WindowName` existe pour tenir. L'objet est gardé d'une image à l'autre.
  static WindowName name(kStatusBarTitle, std::string(kStatusBarId));

  // ── L'ÉCHAPPATOIRE DE MAJ ──────────────────────────────────────────────────
  //
  // 🔴 SANS ELLE, LE VERROU EST UN ALLER SANS RETOUR. Verrouillée, la barre ne se
  // déplace plus et laisse passer les clics ; il faut donc rouvrir le panneau de
  // réglages pour la bouger d'un pixel. Garder MAJ au-dessus d'elle la rend
  // saisissable le temps d'un geste — c'est le même marché que le clic-traversant
  // des barres de raccourcis.
  //
  // 🔴 ET MAJ SEULE NE SUFFIT PAS : en Ragnarok, MAJ+clic est l'attaque forcée.
  // Une barre vide qui surgirait à chaque appui clignoterait pendant tout un
  // combat. Le curseur doit être DESSUS.
  //
  // ⚠ LA POSITION VIENT DE L'IMAGE PRÉCÉDENTE — c'est ImGui qui la retient, et la
  // fenêtre n'est pas encore ouverte. C'est sans conséquence : on ne demande cela
  // que d'une barre immobile.
  const ImGuiWindow* placed = ImGui::FindWindowByName(name.Get());
  bool grabbing = false;
  if (ImGui::GetIO().KeyShift && placed != nullptr) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool over = mouse.x >= placed->Pos.x && mouse.y >= placed->Pos.y &&
                      mouse.x < placed->Pos.x + size.x && mouse.y < placed->Pos.y + size.y;
    // ⚠ ET LE GLISSER SURVIT À LA SORTIE DU CADRE : sans cela, la barre se
    // reverrouillerait au premier pixel parcouru plus vite que le curseur.
    const ImGuiContext* context = ImGui::GetCurrentContext();
    grabbing = over || (context != nullptr && context->MovingWindow == placed);
  }
  const bool placing = !look.locked || grabbing;

  // Rien à montrer, rien à l'écran — surtout pas une emprise vide qu'on
  // prendrait pour une fonction en panne. En cours de placement, elle reste :
  // c'est justement ce qu'on est en train de poser.
  if (cells.empty() && !placing) {
    return;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  // La première fois, en haut à gauche de l'aire de jeu, à droite de la grille
  // des icônes de menu qui tient le coin : la rangée grandit vers la droite, loin
  // d'elle. Ensuite, là où le joueur l'a mise : c'est ImGui qui retient.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(screen->WorkPos.x + MenuIconsHomeRight(scale) + kFirstPlaceMargin * scale,
             screen->WorkPos.y + kFirstPlaceMargin * scale),
      ImGuiCond_FirstUseEver);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoNavFocus;
  // 🔴 SOURDE TANT QU'ELLE EST VERROUILLÉE. Une case d'état ne se clique pas :
  // tout ce que cette fenêtre prendrait à la souris, elle le prendrait au jeu —
  // et son emprise couvre la grille ENTIÈRE, donc bien plus que ses icônes.
  if (!placing) {
    flags |= ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoMove;
  }
  if (!ImGui::Begin(name.Get(), nullptr, flags)) {
    ImGui::End();
    ImGui::PopStyleVar(kStatusBarStyleVars);
    return;
  }

  const ImVec2 at = ImGui::GetWindowPos();
  const StatusRowLayout row = RowOfStatusBar(look, scale);
  const ImVec2 origin = StatusBarOrigin(at, size, StatusAnchorOf(look.anchor));

  // ── LE FOND, SOUS LES CASES MONTRÉES ──────────────────────────────────────
  //
  // ⚠ SOUS CE QUI SE VOIT, ET NON SUR L'EMPRISE : celle-ci réserve la grille
  // entière — quarante cases par défaut — et un aplat de cette taille serait un
  // bandeau en travers du monde pour trois buffs.
  ImDrawList* into = ImGui::GetWindowDrawList();
  const std::size_t shown = OrderStatusCells(cells, StatusSortOf(look.sort), row.most).size();
  if (into != nullptr && shown > 0) {
    const std::vector<ImVec2> places = PlaceStatusRow(shown, origin, row);
    ImVec2 top_left = places.empty() ? origin : places.front();
    ImVec2 bottom_right = top_left;
    for (const ImVec2& place : places) {
      top_left.x = std::min(top_left.x, place.x);
      top_left.y = std::min(top_left.y, place.y);
      bottom_right.x = std::max(bottom_right.x, place.x + row.side);
      bottom_right.y = std::max(bottom_right.y, place.y + row.side + row.below);
    }
    const float pad = kBackgroundPadding * scale;
    top_left.x -= pad;
    top_left.y -= pad;
    bottom_right.x += pad;
    bottom_right.y += pad;
    const float round = kBackgroundRounding * scale;
    if ((look.background_color & IM_COL32_A_MASK) != 0) {
      into->AddRectFilled(top_left, bottom_right, look.background_color, round);
    }
    if (look.border) {
      into->AddRect(top_left, bottom_right, kSkinBorderColor, round);
    }
  }

  if (placing && into != nullptr) {
    // De quoi voir ce qu'on DÉPLACE : l'emprise entière, bien plus large que les
    // icônes du moment. Sans elle on placerait à l'aveugle une zone qui déborde.
    into->AddRect(at, ImVec2(at.x + size.x, at.y + size.y), kSkinBorderColor, 0.0F, 0,
                  kPlacingBorderThickness * scale);
  }

  StatusCellStyle style;
  style.sweep = StatusSweepOf(look.sweep);
  style.sweep_color = look.sweep_color;
  style.countdown_pixels = StatusCountdownRoom(look, scale);
  style.countdown_background = look.countdown_background;
  style.icon_background = look.icon_background;
  style.alpha = std::clamp(look.opacity, kFaintestStatusOpacity, 1.0F);

  // 🔴 L'INFOBULLE SE TAIT QUAND UNE AUTRE FENÊTRE EST SOUS LE CURSEUR. La case
  // décide de son survol par un test de rectangle, qui ignore l'empilement des
  // fenêtres — c'est ce qui la rend dessinable dans une fenêtre sourde, et c'est
  // aussi ce qui lui ferait poser son infobulle À TRAVERS le panneau ouvert
  // par-dessus.
  const bool tooltip = !ImGui::GetIO().WantCaptureMouse;
  DrawStatusRow(cells, origin, row, style, tooltip);

  ImGui::End();
  ImGui::PopStyleVar(kStatusBarStyleVars);
}

}  // namespace ro::ui
