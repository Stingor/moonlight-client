// Le cadre de HUD libre : sa géométrie, puis son dessin. Voir l'en-tête pour ce
// qui le sépare d'une fenêtre.

#include "ro/ui/hud_frame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

#include "ro/ui/window_bounds.h"
#include "ro/ui/window_places.h"

namespace ro::ui {
namespace {

// Les quatre façons dont un bord s'aligne sur un frère : même bord, bord opposé,
// juste après lui, juste avant lui.
constexpr int kSnapWays = 4;

// L'allumage d'un bord survolé, et son épaisseur.
constexpr ImU32 kLitEdge = IM_COL32(255, 220, 80, 210);
constexpr float kLitThickness = 2.0F;

// La poignée de retaille : discrète, mais présente.
constexpr ImU32 kGripFill = IM_COL32(255, 255, 255, 70);

// Le liseré d'un cadre bordé.
constexpr ImU32 kFrameBorder = IM_COL32(0, 0, 0, 160);

// Le repère de centre : un bras clair sur un liseré sombre, pour se lire aussi
// bien sur un fond pâle que sur une carte de nuit.
constexpr ImU32 kCenterUnder = IM_COL32(0, 0, 0, 170);
constexpr ImU32 kCenterOver = IM_COL32(255, 255, 255, 230);
constexpr float kCenterUnderThickness = 3.0F;
constexpr float kCenterOverThickness = 1.0F;
constexpr float kLongestCenterArm = 8.0F;
constexpr float kCenterArmShare = 0.25F;
constexpr float kShortestCenterArm = 2.0F;

// Le plancher de taille d'ImGui vaut 32×32 : un cadre de HUD peut être bien plus
// petit, et son padding imposerait sinon sa propre marge.
constexpr float kSmallestWindow = 4.0F;
constexpr int kPushedStyleVars = 4;

[[nodiscard]] float Rounded(float value) { return std::round(value); }

// Le côté qu'un rang retenu désigne ; un rang hors de l'énumération — un
// fichier corrigé à la main — retombe sur le bord de départ.
[[nodiscard]] AnchorSide SideAt(int rank) {
  return rank == static_cast<int>(AnchorSide::kCenter) ? AnchorSide::kCenter
         : rank == static_cast<int>(AnchorSide::kEnd)  ? AnchorSide::kEnd
                                                       : AnchorSide::kStart;
}

}  // namespace

bool HudSiblings::Shown(int rank) const {
  if (rank < 0 || rank >= static_cast<int>(frames.size())) {
    return false;
  }
  if (shown.empty()) {
    return true;
  }
  return rank < static_cast<int>(shown.size()) && shown[static_cast<std::size_t>(rank)];
}

bool HudGrip::Holds(const char* id) const { return id != nullptr && !held.empty() && held == id; }

int HudEdgesAt(const HudRect& rect, ImVec2 mouse) {
  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  int edges = kHudEdgeNone;
  if (mouse.x <= rect.x + kHudEdgeGrab) {
    edges |= kHudEdgeLeft;
  }
  if (mouse.x >= right - kHudEdgeGrab) {
    edges |= kHudEdgeRight;
  }
  if (mouse.y <= rect.y + kHudEdgeGrab) {
    edges |= kHudEdgeTop;
  }
  if (mouse.y >= bottom - kHudEdgeGrab) {
    edges |= kHudEdgeBottom;
  }
  // ⚠ LE COIN BAS-DROIT A UNE PRISE PLUS LARGE, et il en allume DEUX : c'est la
  // prise que le joueur cherche pour retailler, et la rater d'un pixel donnerait
  // un déplacement à la place.
  if (mouse.x >= right - kHudCornerGrab && mouse.y >= bottom - kHudCornerGrab) {
    edges |= kHudEdgeRight | kHudEdgeBottom;
  }
  return edges;
}

float SnappedToSiblings(float at, float extent, const HudSiblings& siblings, bool vertical) {
  float best = at;
  float nearest = kHudSnapReach;
  for (int rank = 0; rank < static_cast<int>(siblings.frames.size()); ++rank) {
    if (rank == siblings.mine || !siblings.Shown(rank)) {
      continue;
    }
    const HudRect& other = siblings.frames[static_cast<std::size_t>(rank)];
    const float start = vertical ? other.y : other.x;
    const float length = vertical ? other.height : other.width;
    const float ways[kSnapWays] = {start, start + length - extent, start + length, start - extent};
    for (const float candidate : ways) {
      const float apart = std::abs(candidate - at);
      if (apart < nearest) {
        nearest = apart;
        best = candidate;
      }
    }
  }
  return best;
}

std::uint32_t TouchingFrames(const HudSiblings& siblings, int seed) {
  const int count = std::min(static_cast<int>(siblings.frames.size()), kHudMostSiblings);
  if (seed < 0 || seed >= count) {
    return 0;
  }
  const auto touching = [](const HudRect& one, const HudRect& other) {
    return one.x - kHudTouchSlack < other.x + other.width &&
           other.x - kHudTouchSlack < one.x + one.width &&
           one.y - kHudTouchSlack < other.y + other.height &&
           other.y - kHudTouchSlack < one.y + one.height;
  };

  std::uint32_t block = 1U << static_cast<unsigned>(seed);
  // 🔴 TRANSITIVEMENT : A touche B, B touche C, les trois partent ensemble. Une
  // passe unique laisserait C derrière, et le bloc se disloquerait au geste.
  for (bool grown = true; grown;) {
    grown = false;
    for (int inside = 0; inside < count; ++inside) {
      if ((block & (1U << static_cast<unsigned>(inside))) == 0) {
        continue;
      }
      for (int outside = 0; outside < count; ++outside) {
        const std::uint32_t bit = 1U << static_cast<unsigned>(outside);
        if ((block & bit) != 0 || !siblings.Shown(outside)) {
          continue;
        }
        if (touching(siblings.frames[static_cast<std::size_t>(inside)],
                     siblings.frames[static_cast<std::size_t>(outside)])) {
          block |= bit;
          grown = true;
        }
      }
    }
  }
  return block;
}

HudRect ResizedTo(const HudRect& rect, int edges, ImVec2 mouse, ImVec2 offset,
                  const HudFrameLook& look, ImVec2 screen) {
  // 🔴 SEULS LES BORDS SAISIS BOUGENT, les opposés restent épinglés — et la
  // géométrie retombe des quatre positions de bord. Retailler en changeant la
  // taille ferait glisser le cadre quand on tire son bord gauche.
  float left = rect.x;
  float right = rect.x + rect.width;
  float top = rect.y;
  float bottom = rect.y + rect.height;

  if ((edges & kHudEdgeLeft) != 0) {
    left = mouse.x - offset.x;
    if (look.grid != nullptr) {
      left = look.grid->Snap(left, screen.x);
    }
    left = std::min(left, right - look.least_width);
  } else if ((edges & kHudEdgeRight) != 0) {
    right = mouse.x - offset.x;
    if (look.grid != nullptr) {
      right = look.grid->Snap(right, screen.x);
    }
    right = std::max(right, left + look.least_width);
  }
  if ((edges & kHudEdgeTop) != 0) {
    top = mouse.y - offset.y;
    if (look.grid != nullptr) {
      top = look.grid->Snap(top, screen.y);
    }
    top = std::min(top, bottom - look.least_height);
  } else if ((edges & kHudEdgeBottom) != 0) {
    bottom = mouse.y - offset.y;
    if (look.grid != nullptr) {
      bottom = look.grid->Snap(bottom, screen.y);
    }
    bottom = std::max(bottom, top + look.least_height);
  }

  HudRect grown;
  grown.x = Rounded(left);
  grown.y = Rounded(top);
  grown.width = Rounded(right - left);
  grown.height = Rounded(bottom - top);
  return grown;
}

namespace {

// Peint le repère de centre : deux traits COURTS qui se croisent, et non une
// croix pleine largeur. Le but est de désigner un POINT ; des lignes traversant
// tout le cadre se confondraient avec son contenu.
void PaintCenterMark(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right) {
  const ImVec2 middle((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F);
  // ⚠ UN BRAS PROPORTIONNEL, MAIS BORNÉ : fixe, il déborderait d'un cadre
  // minuscule et disparaîtrait dans un grand.
  const float arm = std::min(
      kLongestCenterArm,
      std::min(bottom_right.x - top_left.x, bottom_right.y - top_left.y) * kCenterArmShare);
  if (arm < kShortestCenterArm) {
    return;
  }
  const ImVec2 left(middle.x - arm, middle.y);
  const ImVec2 right(middle.x + arm, middle.y);
  const ImVec2 up(middle.x, middle.y - arm);
  const ImVec2 down(middle.x, middle.y + arm);
  into.AddLine(left, right, kCenterUnder, kCenterUnderThickness);
  into.AddLine(up, down, kCenterUnder, kCenterUnderThickness);
  into.AddLine(left, right, kCenterOver, kCenterOverThickness);
  into.AddLine(up, down, kCenterOver, kCenterOverThickness);
}

// Le geste se relâche : le cadre cesse d'être tenu.
void LetGo(HudGrip& grip) {
  grip.held.clear();
  grip.edges = kHudEdgeNone;
  grip.resizing = false;
  grip.block = 0;
}

}  // namespace

HudRect HudOnScreen(const HudRect& stored, ImVec2 screen) {
  if (screen.x <= 0.0F || screen.y <= 0.0F) {
    return stored;
  }
  const WindowPlace place{.across = SideAt(stored.across),
                          .down = SideAt(stored.down),
                          .offset_x = stored.x,
                          .offset_y = stored.y,
                          .width = stored.width,
                          .height = stored.height};
  const ScreenRect at = RectFromPlace(place, screen.x, screen.y, 1.0F);
  // DANS L'ÉCRAN, pour tous ceux qui montrent le cadre : la place retenue, elle,
  // ne change pas. Un cadre plus large que l'écran se colle au bord de départ.
  const float most_x = std::max(screen.x - stored.width, 0.0F);
  const float most_y = std::max(screen.y - stored.height, 0.0F);
  return HudRect{.x = Rounded(std::clamp(at.x_px, 0.0F, most_x)),
                 .y = Rounded(std::clamp(at.y_px, 0.0F, most_y)),
                 .width = stored.width,
                 .height = stored.height};
}

HudRect HudAnchored(const HudRect& on_screen, ImVec2 screen) {
  if (screen.x <= 0.0F || screen.y <= 0.0F) {
    return on_screen;
  }
  const WindowPlace place = PlaceFromRect(ScreenRect{.x_px = on_screen.x,
                                                     .y_px = on_screen.y,
                                                     .width_px = on_screen.width,
                                                     .height_px = on_screen.height},
                                          screen.x, screen.y, 1.0F);
  // ⚠ L'ÉCART N'EST PAS ARRONDI : au centre, il tombe sur une demi-unité quand
  // l'écran et le cadre n'ont pas la même parité, et l'arrondir décalerait le
  // cadre d'un pixel à chaque relecture.
  return HudRect{.x = place.offset_x,
                 .y = place.offset_y,
                 .width = on_screen.width,
                 .height = on_screen.height,
                 .across = static_cast<int>(place.across),
                 .down = static_cast<int>(place.down)};
}

bool BeginHudFrame(const char* id, HudRect& stored, const HudFrameLook& look,
                   const HudSiblings& stored_siblings, HudGrip& grip, bool* moved,
                   HudFrameClicks* clicks) {
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                           ImGuiWindowFlags_NoFocusOnAppearing |
                           // ⚠ JAMAIS DEVANT UNE VRAIE FENÊTRE : un HUD qu'on
                           // effleure ne doit pas passer par-dessus le sac ouvert.
                           ImGuiWindowFlags_NoBringToFrontOnFocus |
                           // Le déplacement et la retaille sont conduits ICI, et
                           // non par ImGui : lui les ferait sans aimant ni bloc.
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
  if (look.locked && !look.clickable) {
    flags |= ImGuiWindowFlags_NoInputs;
  }

  // 🔴 LA PLACE RETENUE EST ANCRÉE ; LE GESTE TRAVAILLE À L'ÉCRAN. Le cadre et
  // ses frères passent à l'écran du moment, ramenés dedans pour l'AFFICHAGE
  // seulement : la place retenue ne change que sous un geste, sinon passer à
  // un écran plus petit puis revenir perdrait la disposition.
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  HudRect rect = HudOnScreen(stored, display);
  std::array<HudRect, kHudMostSiblings> beside{};
  const std::size_t sibling_count = std::min(stored_siblings.frames.size(), beside.size());
  for (std::size_t rank = 0; rank < sibling_count; ++rank) {
    beside[rank] = HudOnScreen(stored_siblings.frames[rank], display);
  }
  const HudSiblings siblings{.frames = std::span<HudRect>(beside.data(), sibling_count),
                             .shown = stored_siblings.shown,
                             .mine = stored_siblings.mine};
  bool gestured = false;

  // ⚠ RÉ-ÉPINGLÉE À CHAQUE IMAGE sur la géométrie mémorisée : c'est ce qui garde
  // le dessin, la zone cliquable et la poignée en phase.
  ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(kSmallestWindow, kSmallestWindow));

  grip.pending = HudGrip::Pending();
  if (!ImGui::Begin(id, nullptr, flags)) {
    return false;
  }

  const ImVec2 top_left = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  int lit = kHudEdgeNone;

  if (!look.locked) {
    // ⚠ UNE SEULE ZONE CLIQUABLE COUVRANT TOUT : c'est l'endroit SAISI qui
    // décide du geste — un bord retaille, le reste déplace.
    ImGui::SetCursorPos(ImVec2(0.0F, 0.0F));
    ImGui::InvisibleButton("##hud_frame", size);
    // Un cadre qui ne se retaille pas n'a pas de bord à saisir : tout y déplace.
    const int under = look.resizable ? HudEdgesAt(rect, mouse) : kHudEdgeNone;

    if (ImGui::IsItemActivated()) {
      grip.held = id;
      grip.edges = under;
      grip.resizing = under != kHudEdgeNone;
      if (grip.resizing) {
        // L'écart au bord saisi, pour qu'il suive le curseur sans sauter.
        grip.offset.x = (under & kHudEdgeLeft) != 0    ? mouse.x - top_left.x
                        : (under & kHudEdgeRight) != 0 ? mouse.x - bottom_right.x
                                                       : 0.0F;
        grip.offset.y = (under & kHudEdgeTop) != 0      ? mouse.y - top_left.y
                        : (under & kHudEdgeBottom) != 0 ? mouse.y - bottom_right.y
                                                        : 0.0F;
      } else {
        grip.offset = ImVec2(mouse.x - top_left.x, mouse.y - top_left.y);
      }
      grip.block = (!grip.resizing && ImGui::GetIO().KeyCtrl && siblings.mine >= 0)
                       ? TouchingFrames(siblings, siblings.mine)
                       : 0;
    }

    // Les bords TIRÉS pendant une retaille, sinon ceux qui sont survolés.
    if (ImGui::IsItemActive() && grip.resizing && grip.Holds(id)) {
      lit = grip.edges;
    } else if (ImGui::IsItemHovered()) {
      lit = under;
    }

    if (ImGui::IsItemActive() && grip.Holds(id)) {
      if (grip.resizing) {
        const HudRect grown = ResizedTo(rect, grip.edges, mouse, grip.offset, look, screen);
        if (grown.x != rect.x || grown.y != rect.y || grown.width != rect.width ||
            grown.height != rect.height) {
          rect = grown;
          gestured = true;
        }
      } else {
        float at_x = mouse.x - grip.offset.x;
        float at_y = mouse.y - grip.offset.y;
        // 🔴 FRÈRES D'ABORD, GRILLE ENSUITE, ÉCRAN EN DERNIER. Chaque étape peut
        // défaire la précédente, et c'est l'écran qui doit avoir le dernier mot :
        // une aimantation peut elle-même pousser un cadre dehors.
        if (look.sticky && siblings.mine >= 0) {
          at_x = SnappedToSiblings(at_x, rect.width, siblings, false);
          at_y = SnappedToSiblings(at_y, rect.height, siblings, true);
        }
        if (look.grid != nullptr) {
          at_x = look.grid->Snap(at_x, screen.x);
          at_y = look.grid->Snap(at_y, screen.y);
        }
        const ImVec2 inside =
            ClampWindowPosToScreen(ImVec2(at_x, at_y), ImVec2(rect.width, rect.height));
        const float put_x = Rounded(inside.x);
        const float put_y = Rounded(inside.y);
        if (put_x != rect.x || put_y != rect.y) {
          const float by_x = put_x - rect.x;
          const float by_y = put_y - rect.y;
          rect.x = put_x;
          rect.y = put_y;
          gestured = true;
          // ⚠ LE BLOC SUIT DU MÊME ÉCART, sans aimant ni clamp : le tenir rigide
          // est tout l'intérêt du geste. Aimanter chaque frère le déformerait.
          const std::uint32_t mine = 1U << static_cast<unsigned>(std::max(siblings.mine, 0));
          if ((grip.block & ~mine) != 0) {
            for (int rank = 0; rank < static_cast<int>(siblings.frames.size()); ++rank) {
              if (rank == siblings.mine ||
                  (grip.block & (1U << static_cast<unsigned>(rank))) == 0) {
                continue;
              }
              HudRect& other = siblings.frames[static_cast<std::size_t>(rank)];
              other.x += by_x;
              other.y += by_y;
            }
          }
        }
      }
    } else if (grip.Holds(id) && !ImGui::IsItemActive()) {
      LetGo(grip);
    }
    // LE GESTE S'ÉCRIT dans la place retenue, ancrée de nouveau : un cadre tiré
    // vers l'autre bord s'ancre à celui-là. Les frères partis en bloc aussi.
    if (gestured) {
      stored = HudAnchored(rect, display);
      for (std::size_t rank = 0; rank < sibling_count; ++rank) {
        const bool in_block = (grip.block & (1U << static_cast<unsigned>(rank))) != 0;
        if (in_block && static_cast<int>(rank) != siblings.mine) {
          stored_siblings.frames[rank] = HudAnchored(beside[rank], display);
        }
      }
      if (moved != nullptr) {
        *moved = true;
      }
    }
  } else if (look.clickable) {
    // ⚠ UN BOUTON PLUTÔT QU'UN TEST DE SURVOL : c'est ce qui fait qu'ImGui
    // POSSÈDE vraiment l'appui, donc que le clic n'atteint pas le jeu et ne part
    // pas au sol derrière le cadre.
    ImGui::SetCursorPos(ImVec2(0.0F, 0.0F));
    ImGui::InvisibleButton("##hud_frame_proxy", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    if (clicks != nullptr) {
      clicks->hovered = ImGui::IsItemHovered();
      clicks->left = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      clicks->right = ImGui::IsItemClicked(ImGuiMouseButton_Right);
      // ⚠ LE GAUCHE SEUL : le bouton prend aussi le droit, qui ne s'enfonce pas.
      clicks->held = ImGui::IsItemActive() && ImGui::IsItemHovered() &&
                     ImGui::IsMouseDown(ImGuiMouseButton_Left);
    }
    // Le survol s'allume : sans lui, rien ne distingue un cadre qui AGIT d'un
    // cadre qui se contente d'afficher, et le joueur ne sait pas où son clic va.
    if (look.lit_on_hover && ImGui::IsItemHovered()) {
      lit = kHudEdgeLeft | kHudEdgeRight | kHudEdgeTop | kHudEdgeBottom;
    }
    ImGui::SetCursorPos(ImVec2(0.0F, 0.0F));
  }

  ImDrawList* into = ImGui::GetWindowDrawList();
  if (into != nullptr) {
    if (look.background.has_value()) {
      into->AddRectFilled(top_left, bottom_right, *look.background, look.rounding);
    }
    if (look.border) {
      into->AddRect(top_left, bottom_right, kFrameBorder, look.rounding);
    }
    // ⚠ LE REPÈRE SE PEINT AVANT LE CONTENU, comme le fond : dessiné après, il
    // barrerait ce qu'il doit sous-tendre.
    if (look.center_mark && !look.locked) {
      PaintCenterMark(*into, top_left, bottom_right);
    }
  }

  grip.pending.open = true;
  grip.pending.locked = look.locked;
  grip.pending.resizable = look.resizable;
  grip.pending.lit = lit;
  grip.pending.top_left = top_left;
  grip.pending.bottom_right = bottom_right;
  return true;
}

void EndHudFrame(HudGrip& grip) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  if (grip.pending.open && into != nullptr) {
    const ImVec2 top_left = grip.pending.top_left;
    const ImVec2 bottom_right = grip.pending.bottom_right;

    // 🔴 LA POIGNÉE, SANS QUOI RIEN NE DIT QUE LE CADRE SE RETAILLE : le jeu
    // dessine son propre curseur, donc la flèche du système ne paraît jamais.
    if (!grip.pending.locked && grip.pending.resizable) {
      into->AddTriangleFilled(ImVec2(bottom_right.x, bottom_right.y - kHudCornerGrab), bottom_right,
                              ImVec2(bottom_right.x - kHudCornerGrab, bottom_right.y), kGripFill);
    }
    if (grip.pending.lit != kHudEdgeNone) {
      const int lit = grip.pending.lit;
      if ((lit & kHudEdgeLeft) != 0) {
        into->AddLine(top_left, ImVec2(top_left.x, bottom_right.y), kLitEdge, kLitThickness);
      }
      if ((lit & kHudEdgeRight) != 0) {
        into->AddLine(ImVec2(bottom_right.x, top_left.y), bottom_right, kLitEdge, kLitThickness);
      }
      if ((lit & kHudEdgeTop) != 0) {
        into->AddLine(top_left, ImVec2(bottom_right.x, top_left.y), kLitEdge, kLitThickness);
      }
      if ((lit & kHudEdgeBottom) != 0) {
        into->AddLine(ImVec2(top_left.x, bottom_right.y), bottom_right, kLitEdge, kLitThickness);
      }
    }
    grip.pending.open = false;
  }
  ImGui::End();
  ImGui::PopStyleVar(kPushedStyleVars);
}

}  // namespace ro::ui
