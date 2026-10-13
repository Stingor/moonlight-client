#include "ro/ui/broadcast_balloon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <span>
#include <vector>

#include "ro/ui/chat_log.h"

#include "chat_log_parts.h"

namespace ro::ui {
namespace {

using chat_log_detail::LayOutLine;
using chat_log_detail::Pen;
using chat_log_detail::PlaceFrame;
using chat_log_detail::PlaceTarget;

constexpr int kWholePercent = 100;

[[nodiscard]] float SnapToPixel(float value) { return std::floor(value + 0.5F); }

// Ce qui reste d'une couleur multipliée par `alpha`.
[[nodiscard]] ImU32 Faded(ImU32 color, float alpha) {
  const auto kept =
      static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU) * alpha);
  return (color & ~IM_COL32_A_MASK) | (kept << IM_COL32_A_SHIFT);
}

// Fond tout ce qui s'est peint depuis `first_vertex`.
//
// ⚠ APRÈS COUP, SUR LES SOMMETS : le moteur du fil peint ses couleurs propres —
// un lien, une suite colorée, une emote — et n'a pas d'opacité à recevoir.
void FadeSince(ImDrawList* into, int first_vertex, float alpha) {
  if (alpha >= 1.0F) {
    return;
  }
  for (int rank = first_vertex; rank < into->VtxBuffer.Size; ++rank) {
    into->VtxBuffer[rank].col = Faded(into->VtxBuffer[rank].col, alpha);
  }
}

// NOIRCIT tout ce qui s'est peint depuis `first_vertex`, en gardant l'opacité de
// chaque sommet : c'est ce qui fait d'une copie décalée du texte son ombre,
// liens, couleurs et emotes compris.
void ShadeSince(ImDrawList* into, int first_vertex) {
  for (int rank = first_vertex; rank < into->VtxBuffer.Size; ++rank) {
    into->VtxBuffer[rank].col &= IM_COL32_A_MASK;
  }
}

// Ce que la pose d'une bulle lit, le même pour toutes ses lignes.
struct BalloonFrame {
  ChatLogStyle style;
  PlaceFrame place;
};

[[nodiscard]] BalloonFrame FrameOf(const BalloonView& balloon, const BroadcastBalloonLook& look,
                                   const ChatRichArt* rich) {
  BalloonFrame frame;
  frame.style = ChatLogStyle{.font_size = balloon.font_px, .rich = rich};
  frame.place = PlaceFrame{.font = ImGui::GetFont(),
                           .wrap_width = balloon.wraps ? static_cast<float>(look.wrap_px) : FLT_MAX,
                           .step = balloon.font_px + kBalloonLineGap,
                           .view_top = -FLT_MAX,
                           .view_bottom = FLT_MAX,
                           .now_s = ImGui::GetTime()};
  return frame;
}

[[nodiscard]] ChatLineView LineOf(const BalloonLineView& line) {
  return ChatLineView{.text = line.text,
                      .color = line.color | IM_COL32_A_MASK,
                      .links = line.links,
                      .styles = line.styles,
                      .images = line.images};
}

// Pose toutes les lignes d'une bulle depuis `origin`, sur une seule plume : une
// ligne suivante commence sous la précédente. Tout nul dans `target` : on mesure.
float PlaceLines(const BalloonView& balloon, BalloonFrame& frame, ImVec2 origin,
                 const PlaceTarget& target, std::vector<float>* row_widths) {
  // ⚠ LE STYLE EST PRIS ICI, une fois la trame à sa place définitive : la
  // copier plus tôt laisserait la pose pointer sur un style mort.
  frame.place.style = &frame.style;
  frame.place.origin = origin;
  Pen pen{.row = frame.place.step, .row_widths = row_widths};
  for (const BalloonLineView& line : balloon.lines) {
    LayOutLine(frame.place, target, LineOf(line), &pen);
  }
  return pen.y;
}

// LA POIGNÉE D'ANCRE : une vraie fenêtre, et seulement déverrouillée. Les bulles
// se peignent sur la liste de fond et ne captent rien à la souris.
void DrawGrip(const BalloonAnchor& anchor, float top_y, const char* grip_id, const char* label,
              BalloonAnchorGrip& grip, BalloonAnchorAsk* ask) {
  const float screen_width = ImGui::GetIO().DisplaySize.x;
  const float center = BalloonCenterX(anchor, screen_width);
  const float left = SnapToPixel(center - grip.width_px * 0.5F);
  const float top = SnapToPixel(top_y);
  // 🔴 UNE PLACE RECALCULÉE QUI CHANGE N'EST PAS UN GESTE : un réglage, une
  // largeur de poignée ou un écran qui change la déplacent, et elle se repose.
  // ⚠ ET SEULEMENT DANS CE CAS : imposer la place à chaque image empêcherait
  // de la tirer à la souris.
  const bool impose =
      grip.replace || !grip.applied || left != grip.applied_left_x || top != grip.applied_top_y;
  ImGui::SetNextWindowPos(ImVec2(left, top), impose ? ImGuiCond_Always : ImGuiCond_Once);
  if (impose) {
    grip.applied_left_x = left;
    grip.applied_top_y = top;
    grip.applied = true;
  }
  constexpr float kGripBackgroundAlpha = 0.45F;
  ImGui::SetNextWindowBgAlpha(kGripBackgroundAlpha);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin(grip_id, nullptr, flags)) {
    ImGui::TextUnformatted(label);
    grip.width_px = ImGui::GetWindowWidth();
    const ImVec2 at = ImGui::GetWindowPos();
    const float moved_left = SnapToPixel(at.x);
    const float moved_top = SnapToPixel(at.y);
    if (ask != nullptr && (moved_left != grip.applied_left_x || moved_top != grip.applied_top_y)) {
      ask->moved = true;
      ask->center_x = anchor.center_x;
      ask->top_y = anchor.top_y;
      // ⚠ CHAQUE AXE NE QUITTE SA PLACE PAR DÉFAUT QUE TIRÉ LUI-MÊME : descendre
      // le bandeau ne doit pas le décentrer, ni le déplacer de côté figer la
      // hauteur que le client règle sur celle de l'écran.
      if (moved_left != grip.applied_left_x) {
        ask->center_x = static_cast<int>(SnapToPixel(moved_left + grip.width_px * 0.5F));
      }
      if (moved_top != grip.applied_top_y) {
        ask->top_y = static_cast<int>(moved_top);
      }
      // Le geste est pris : sans cela, l'écart se redirait à chaque image.
      grip.applied_left_x = moved_left;
      grip.applied_top_y = moved_top;
    }
  }
  ImGui::End();
  grip.replace = false;
}

}  // namespace

float AnnouncementBalloonTop(const BalloonAnchor& anchor) {
  return static_cast<float>(anchor.top_y < 0 ? kNativeBalloonTop : anchor.top_y);
}

float HuntBalloonTop(const BalloonAnchor& anchor, float screen_height_px) {
  if (anchor.top_y >= 0) {
    return static_cast<float>(anchor.top_y);
  }
  // ⚠ ENTIER COMME CHEZ LE CLIENT : sa division tronque.
  return std::floor(kNativeHuntTopOn480 * screen_height_px / kNativeHuntReferenceHeight);
}

float BalloonCenterX(const BalloonAnchor& anchor, float screen_width_px) {
  return anchor.center_x < 0 ? std::floor(screen_width_px * 0.5F)
                             : static_cast<float>(anchor.center_x);
}

ImVec2 BalloonSize(float widest_row_px, float stacked_px) {
  return ImVec2(std::ceil(widest_row_px) + 2.0F * kBalloonPadX, stacked_px + kBalloonPadY);
}

float BalloonFontPixels(std::uint16_t source_px, int scale_percent) {
  const int percent = std::clamp(scale_percent, kLeastBalloonFontPercent, kMostBalloonFontPercent);
  return SnapToPixel(static_cast<float>(source_px) * static_cast<float>(percent) /
                     static_cast<float>(kWholePercent));
}

void DrawBroadcastBalloons(std::span<const BalloonView> balloons, const BroadcastBalloonLook& look,
                           const BalloonAnchor& anchor, float top_y, const char* grip_id,
                           const char* label, BalloonAnchorGrip& grip, BalloonAnchorAsk* ask,
                           const ChatRichArt* rich) {
  if (!anchor.locked) {
    DrawGrip(anchor, top_y, grip_id, label, grip, ask);
  }
  if (balloons.empty()) {
    return;
  }
  PaintBroadcastBalloons(ImGui::GetBackgroundDrawList(), balloons, look,
                         BalloonCenterX(anchor, ImGui::GetIO().DisplaySize.x), top_y, rich);
}

float PaintBroadcastBalloons(ImDrawList* into, std::span<const BalloonView> balloons,
                             const BroadcastBalloonLook& look, float center_x, float top_y,
                             const ChatRichArt* rich) {
  // Ce que le fil sait peindre, SANS SES VIGNETTES : une bulle ne télécharge
  // rien, et sa rangée ne doit pas grandir.
  ChatRichArt balloon_rich;
  const ChatRichArt* shown_rich = nullptr;
  if (rich != nullptr) {
    balloon_rich = *rich;
    balloon_rich.picture_pixels = 0.0F;
    balloon_rich.seen_pictures = nullptr;
    shown_rich = &balloon_rich;
  }

  std::vector<float> row_widths;
  float y = SnapToPixel(top_y);
  bool first = true;
  for (const BalloonView& balloon : balloons) {
    if (!first) {
      y += balloon.new_family ? kBalloonFamilyGap : kBalloonGap;
    }
    first = false;
    BalloonFrame frame = FrameOf(balloon, look, shown_rich);

    // 🔴 MESURER D'ABORD, PAR LE MÊME MOTEUR : la largeur d'une rangée dépend
    // des styles — un mot gras, une emote, un nom d'objet n'ont pas la largeur
    // de leur texte nu.
    row_widths.clear();
    const float stacked =
        PlaceLines(balloon, frame, ImVec2(0.0F, 0.0F), PlaceTarget{}, &row_widths);
    const float widest = row_widths.empty() ? 0.0F : *std::ranges::max_element(row_widths);
    const ImVec2 size = BalloonSize(widest, stacked);
    // ⚠ ENTIER COMME CHEZ LE CLIENT : un bord à x,5 baverait d'un pixel.
    const ImVec2 top_left(std::floor(center_x - size.x * 0.5F), y);
    const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);
    y = bottom_right.y;
    if (into == nullptr) {
      continue;
    }

    const int first_vertex = into->VtxBuffer.Size;
    // LE VOILE À L'INTÉRIEUR DU LISERÉ, puis le liseré par-dessus.
    into->AddRectFilled(ImVec2(top_left.x + 1.0F, top_left.y + 1.0F),
                        ImVec2(bottom_right.x - 1.0F, bottom_right.y - 1.0F), look.background,
                        kBalloonRounding);
    into->AddRect(top_left, bottom_right, look.border, kBalloonRounding);

    const ImVec2 text_at(top_left.x + kBalloonPadX, top_left.y + kBalloonPadY);
    // L'OMBRE D'ABORD, le texte ensuite : la même pose, décalée puis noircie.
    const int shadow_vertex = into->VtxBuffer.Size;
    PlaceLines(balloon, frame,
               ImVec2(text_at.x + kBalloonShadowOffset, text_at.y + kBalloonShadowOffset),
               PlaceTarget{.into = into}, nullptr);
    ShadeSince(into, shadow_vertex);
    PlaceLines(balloon, frame, text_at, PlaceTarget{.into = into}, nullptr);
    FadeSince(into, first_vertex, std::clamp(balloon.alpha, 0.0F, 1.0F));
  }
  return y;
}

}  // namespace ro::ui
