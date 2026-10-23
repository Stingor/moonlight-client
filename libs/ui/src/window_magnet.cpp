#include "ro/ui/window_magnet.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include <imgui_internal.h>

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Ce qui n'est pas une voisine, quel que soit son rectangle : ce qui vit dans
// une autre fenêtre, ce qui surgit et disparaît, et ce que le joueur ne peut
// pas déplacer — un HUD fixe, une fenêtre verrouillée, un calque traversant.
constexpr ImGuiWindowFlags kNotANeighbour =
    ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup |
    ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

// Un rectangle vu sur UN axe : `vertical` faux lit la largeur, vrai la hauteur.
struct Span {
  float lo = 0.0F;
  float hi = 0.0F;
};

[[nodiscard]] Span Along(const MagnetBox& box, bool vertical) {
  return vertical ? Span{.lo = box.top, .hi = box.bottom} : Span{.lo = box.left, .hi = box.right};
}

// Deux étendues se chevauchent-elles, à `reach_px` près ?
[[nodiscard]] bool Near(Span one, Span other, float reach_px) {
  return one.lo <= other.hi + reach_px && other.lo <= one.hi + reach_px;
}

// Deux étendues sont-elles bout à bout ?
[[nodiscard]] bool Touching(Span one, Span other) {
  return std::abs(one.lo - other.hi) <= kWindowMagnetTouchSlackPixels ||
         std::abs(one.hi - other.lo) <= kWindowMagnetTouchSlackPixels;
}

// D'où vient la traction retenue sur un axe.
enum class PullKind : std::uint8_t { kScreen, kContact, kAligned };

struct Pull {
  // Le bord gauche (ou haut) de la fenêtre une fois tirée, et le bord qui la tire.
  float lo = 0.0F;
  float edge = 0.0F;
  float apart_px = 0.0F;
  PullKind kind = PullKind::kScreen;
  // La voisine qui tire ; sans objet pour l'écran.
  std::size_t neighbour = 0;
};

// La traction la plus proche d'un axe, dans la portée.
class NearestPull {
 public:
  NearestPull(float mine_lo, float reach_px) : mine_lo_(mine_lo), reach_px_(reach_px) {}

  void Offer(float lo, float edge, PullKind kind, std::size_t neighbour) {
    const float apart_px = std::abs(lo - mine_lo_);
    if (apart_px > reach_px_ || (best_.has_value() && apart_px >= best_->apart_px)) {
      return;
    }
    best_ =
        Pull{.lo = lo, .edge = edge, .apart_px = apart_px, .kind = kind, .neighbour = neighbour};
  }

  [[nodiscard]] const std::optional<Pull>& best() const { return best_; }

 private:
  float mine_lo_;
  float reach_px_;
  std::optional<Pull> best_;
};

// Bord à bord : contre les bords de l'écran, et contre le bord OPPOSÉ d'une
// voisine que la fenêtre longe sur l'autre axe.
[[nodiscard]] std::optional<Pull> ContactPull(const WindowMagnetAsk& ask, Span mine, Span across,
                                              bool vertical) {
  const float length = mine.hi - mine.lo;
  const float screen_extent = vertical ? ask.screen.y : ask.screen.x;
  NearestPull nearest(mine.lo, ask.reach_px);
  nearest.Offer(0.0F, 0.0F, PullKind::kScreen, 0);
  nearest.Offer(screen_extent - length, screen_extent, PullKind::kScreen, 0);
  for (std::size_t rank = 0; rank < ask.neighbours.size(); ++rank) {
    const MagnetBox& other = ask.neighbours[rank];
    if (!Near(across, Along(other, !vertical), ask.reach_px)) {
      continue;
    }
    const Span side = Along(other, vertical);
    nearest.Offer(side.hi, side.hi, PullKind::kContact, rank);
    nearest.Offer(side.lo - length, side.lo, PullKind::kContact, rank);
  }
  return nearest.best();
}

// Bords de même côté, contre une voisine ACCOLÉE sur l'autre axe.
[[nodiscard]] std::optional<Pull> AlignedPull(const WindowMagnetAsk& ask, Span mine, Span across,
                                              bool vertical) {
  const float length = mine.hi - mine.lo;
  NearestPull nearest(mine.lo, ask.reach_px);
  for (std::size_t rank = 0; rank < ask.neighbours.size(); ++rank) {
    const MagnetBox& other = ask.neighbours[rank];
    const Span side = Along(other, vertical);
    if (!Touching(across, Along(other, !vertical)) || !Near(mine, side, ask.reach_px)) {
      continue;
    }
    nearest.Offer(side.lo, side.lo, PullKind::kAligned, rank);
    nearest.Offer(side.hi - length, side.hi, PullKind::kAligned, rank);
  }
  return nearest.best();
}

// Le trait qui montre le bord qui accroche. ⚠ IL COURT LÀ OÙ LES DEUX BORDS SE
// RENCONTRENT : le long du contact, ou sur les deux fenêtres à la fois quand
// elles s'alignent.
[[nodiscard]] MagnetLine LineOf(const Pull& pull, Span across, const WindowMagnetAsk& ask,
                                bool vertical) {
  Span run = across;
  if (pull.kind != PullKind::kScreen) {
    const Span other = Along(ask.neighbours[pull.neighbour], !vertical);
    if (pull.kind == PullKind::kAligned) {
      run = Span{.lo = std::min(across.lo, other.lo), .hi = std::max(across.hi, other.hi)};
    } else if (std::max(across.lo, other.lo) < std::min(across.hi, other.hi)) {
      run = Span{.lo = std::max(across.lo, other.lo), .hi = std::min(across.hi, other.hi)};
    }
  }
  return vertical ? MagnetLine{.from = ImVec2(run.lo, pull.edge), .to = ImVec2(run.hi, pull.edge)}
                  : MagnetLine{.from = ImVec2(pull.edge, run.lo), .to = ImVec2(pull.edge, run.hi)};
}

[[nodiscard]] bool IsNeighbour(const ImGuiWindow& window, const ImGuiWindow& moving,
                               const EarlierMagnet& earlier) {
  return &window != &moving && window.RootWindow == &window && window.WasActive && !window.Hidden &&
         !window.IsFallbackWindow && (window.Flags & kNotANeighbour) == 0 && window.Size.x > 0.0F &&
         window.Size.y > 0.0F && !earlier.Rides(window.ID);
}

}  // namespace

bool EarlierMagnet::Rides(ImGuiID window_id) const {
  return window_id != 0 && std::ranges::find(riders, window_id) != riders.end();
}

float WindowMagnetReachPixels(float interface_scale) {
  return kWindowMagnetReachPixels * interface_scale;
}

WindowMagnetCatch MagnetizedWindow(const WindowMagnetAsk& ask) {
  Span mine_x{.lo = ask.at.x, .hi = ask.at.x + ask.size.x};
  Span mine_y{.lo = ask.at.y, .hi = ask.at.y + ask.size.y};
  // LE BORD À BORD D'ABORD, sur les deux axes, depuis la place que donne la
  // souris : c'est lui qui décide si la fenêtre est accolée.
  std::optional<Pull> pull_x;
  std::optional<Pull> pull_y;
  if (!ask.held_across) {
    pull_x = ContactPull(ask, mine_x, mine_y, false);
  }
  if (!ask.held_down) {
    pull_y = ContactPull(ask, mine_y, mine_x, true);
  }
  if (pull_x.has_value()) {
    mine_x = Span{.lo = pull_x->lo, .hi = pull_x->lo + ask.size.x};
  }
  if (pull_y.has_value()) {
    mine_y = Span{.lo = pull_y->lo, .hi = pull_y->lo + ask.size.y};
  }
  // L'ALIGNEMENT ENSUITE, sur l'axe resté libre seulement : une fenêtre
  // accolée à sa voisine ne s'en décolle pas pour s'aligner sur une autre.
  if (!ask.held_across && !pull_x.has_value()) {
    pull_x = AlignedPull(ask, mine_x, mine_y, false);
    if (pull_x.has_value()) {
      mine_x = Span{.lo = pull_x->lo, .hi = pull_x->lo + ask.size.x};
    }
  }
  if (!ask.held_down && !pull_y.has_value()) {
    pull_y = AlignedPull(ask, mine_y, mine_x, true);
    if (pull_y.has_value()) {
      mine_y = Span{.lo = pull_y->lo, .hi = pull_y->lo + ask.size.y};
    }
  }

  WindowMagnetCatch caught{.at = ImVec2(mine_x.lo, mine_y.lo)};
  if (pull_x.has_value()) {
    caught.across = LineOf(*pull_x, mine_y, ask, false);
  }
  if (pull_y.has_value()) {
    caught.down = LineOf(*pull_y, mine_x, ask, true);
  }
  return caught;
}

WindowMagnetCatch MagnetizedMovingWindow(const ImGuiWindow& moving, const EarlierMagnet& earlier) {
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  std::vector<MagnetBox> neighbours;
  for (const ImGuiWindow* window : context.Windows) {
    if (window == nullptr || !IsNeighbour(*window, moving, earlier)) {
      continue;
    }
    neighbours.push_back(MagnetBox{.left = window->Pos.x,
                                   .top = window->Pos.y,
                                   .right = window->Pos.x + window->Size.x,
                                   .bottom = window->Pos.y + window->Size.y});
  }
  return MagnetizedWindow(WindowMagnetAsk{.at = moving.Pos,
                                          .size = moving.Size,
                                          .neighbours = neighbours,
                                          .screen = context.IO.DisplaySize,
                                          .reach_px = WindowMagnetReachPixels(Scale()),
                                          .held_across = earlier.held_across,
                                          .held_down = earlier.held_down});
}

void DrawWindowMagnetCatch(const WindowMagnetCatch& caught) {
  // ⚠ LE PREMIER PLAN : le trait longe un bord que la fenêtre elle-même couvre
  // à moitié, et une liste de fond le peindrait dessous.
  ImDrawList* into = ImGui::GetForegroundDrawList();
  for (const std::optional<MagnetLine>& line : {caught.across, caught.down}) {
    if (!line.has_value()) {
      continue;
    }
    into->AddLine(line->from, line->to, kWindowMagnetGlowColor, Px(kWindowMagnetGlowPixels));
    into->AddLine(line->from, line->to, kWindowMagnetLineColor, Px(kWindowMagnetLinePixels));
  }
}

}  // namespace ro::ui
