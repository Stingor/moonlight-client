#include "ro/ui/roggle_window.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <imgui.h>

#include "ro/ui/minigames_window.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// La place que prennent le bandeau du haut et les marges de la fenêtre.
constexpr float kChromeWidth = 16.0F;
constexpr float kChromeHeight = 78.0F;
constexpr float kRounding = 6.0F;

// Les plots et la bille se dessinent un peu plus grands que leur contact.
constexpr float kPegDrawRadius = 13.0F;
constexpr float kBallDrawRadius = 13.0F;
constexpr float kCannonRadius = 9.0F;
constexpr float kBarrelLength = 24.0F;
constexpr float kGuideDotRadius = 2.0F;
// Un plot pas encore touché, un rien éteint.
constexpr float kUnlitAlpha = 0.88F;

constexpr ImU32 kBoardColor = IM_COL32(18, 20, 34, 255);
constexpr ImU32 kBoardEdge = IM_COL32(80, 90, 140, 255);
constexpr ImU32 kOrangeColor = IM_COL32(255, 140, 20, 255);
constexpr ImU32 kOrangeLit = IM_COL32(255, 232, 150, 255);
constexpr ImU32 kOrangeRing = IM_COL32(255, 220, 120, 230);
constexpr ImU32 kBlueColor = IM_COL32(70, 130, 235, 255);
constexpr ImU32 kBlueLit = IM_COL32(210, 240, 255, 255);
constexpr ImU32 kBlueRing = IM_COL32(190, 225, 255, 230);
constexpr ImU32 kShapeEdge = IM_COL32(0, 0, 0, 90);
constexpr ImU32 kBucketColor = IM_COL32(90, 220, 120, 255);
constexpr ImU32 kBucketEdge = IM_COL32(30, 90, 45, 255);
constexpr ImU32 kCannonColor = IM_COL32(200, 200, 210, 255);
constexpr ImU32 kBarrelColor = IM_COL32(235, 235, 245, 255);
constexpr ImU32 kGuideColor = IM_COL32(255, 255, 255, 85);
constexpr ImU32 kVeilColor = IM_COL32(0, 0, 0, 150);
constexpr ImU32 kWonColor = IM_COL32(120, 255, 150, 255);
constexpr ImU32 kLostColor = IM_COL32(255, 120, 120, 255);
constexpr ImVec4 kOrangeText(1.0F, 0.55F, 0.12F, 1.0F);

// LE PORING DE REPLI, quand le vrai sprite manque : une goutte rose, deux yeux
// et leurs reflets, un sourire.
constexpr ImU32 kPoringBody = IM_COL32(255, 160, 200, 255);
constexpr ImU32 kPoringShine = IM_COL32(255, 225, 238, 200);
constexpr ImU32 kPoringEdge = IM_COL32(205, 85, 135, 255);
constexpr ImU32 kPoringEye = IM_COL32(45, 30, 45, 255);
constexpr ImU32 kPoringGlint = IM_COL32(255, 255, 255, 235);
constexpr ImU32 kPoringSmile = IM_COL32(125, 45, 75, 255);
constexpr float kPoringRadius = 11.0F;

void DrawPoring(ImDrawList* draw, ImVec2 middle, float radius) {
  draw->AddCircleFilled(middle, radius, kPoringBody);
  draw->AddCircleFilled(ImVec2(middle.x - radius * 0.30F, middle.y - radius * 0.35F),
                        radius * 0.28F, kPoringShine);
  draw->AddCircle(middle, radius, kPoringEdge, 0, 1.6F);
  const float eye_x = radius * 0.40F;
  const float eye_y = -radius * 0.02F;
  const float eye_radius = radius * 0.17F;
  for (const float side : {-1.0F, 1.0F}) {
    const ImVec2 eye(middle.x + side * eye_x, middle.y + eye_y);
    draw->AddCircleFilled(eye, eye_radius, kPoringEye);
    draw->AddCircleFilled(ImVec2(eye.x + eye_radius * 0.3F, eye.y - eye_radius * 0.3F),
                          eye_radius * 0.4F, kPoringGlint);
  }
  constexpr float kSmileFrom = 0.18F * std::numbers::pi_v<float>;
  constexpr float kSmileTo = 0.82F * std::numbers::pi_v<float>;
  constexpr int kSmileSegments = 12;
  draw->PathArcTo(ImVec2(middle.x, middle.y + radius * 0.12F), radius * 0.34F, kSmileFrom, kSmileTo,
                  kSmileSegments);
  draw->PathStroke(kPoringSmile, 0, 2.0F);
}

// L'image, la plus grande qui tienne dans le carré de demi-côté `half`.
void DrawFitted(ImDrawList* draw, const SkinImage& image, ImVec2 middle, float half,
                ImU32 tint = IM_COL32_WHITE) {
  const float scale = std::min(2.0F * half / image.width, 2.0F * half / image.height);
  const float half_x = image.width * scale * 0.5F;
  const float half_y = image.height * scale * 0.5F;
  draw->AddImage(image.id, ImVec2(middle.x - half_x, middle.y - half_y),
                 ImVec2(middle.x + half_x, middle.y + half_y), ImVec2(0.0F, 0.0F),
                 ImVec2(1.0F, 1.0F), tint);
}

void DrawPeg(ImDrawList* draw, const RoggleView& view, const RogglePeg& peg, ImVec2 middle) {
  const SkinImage& image = peg.orange ? view.orange_art : view.blue_art;
  if (image.usable()) {
    DrawFitted(draw, image, middle, kPegDrawRadius,
               peg.lit ? IM_COL32_WHITE : ImGui::GetColorU32(IM_COL32_WHITE, kUnlitAlpha));
    if (peg.lit) {
      draw->AddCircle(middle, kPegDrawRadius + 1.0F, peg.orange ? kOrangeRing : kBlueRing, 0, 2.0F);
    }
    return;
  }
  const ImU32 color =
      peg.orange ? (peg.lit ? kOrangeLit : kOrangeColor) : (peg.lit ? kBlueLit : kBlueColor);
  draw->AddCircleFilled(middle, view.board.peg_radius, color);
  draw->AddCircle(middle, view.board.peg_radius, kShapeEdge, 0, 1.5F);
}

void DrawHeader(const RoggleView& view, RoggleAsk& asked, const SkinArt& art) {
  const float gap = ImGui::GetFontSize();
  ImGui::Text(core::Tr("Score : %d"), view.score);
  ImGui::SameLine(0.0F, gap);
  ImGui::Text(core::Tr("Billes : %d"), view.balls_left);
  ImGui::SameLine(0.0F, gap);
  ImGui::TextColored(kOrangeText, core::Tr("Orange : %d"), view.orange_left);
  if (view.multiplier > 1) {
    ImGui::SameLine(0.0F, gap);
    ImGui::TextColored(kOrangeText, "×%d", view.multiplier);
  }
  ImGui::SameLine(0.0F, gap);
  if (SkinSmallButton(core::Tr("Nouvelle partie"), art)) {
    asked.new_game = true;
  }
  if (view.best_score > 0) {
    FaintText(core::Tr("Meilleur :"));
    ImGui::SameLine();
    ImGui::Text("%d", view.best_score);
  } else {
    FaintText(core::Tr("Visez à la souris, cliquez pour tirer."));
  }
}

void DrawOutcome(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  if (view.phase != RogglePhase::kWon && view.phase != RogglePhase::kLost) {
    return;
  }
  const float width = view.board.width;
  const float height = view.board.height;
  draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), kVeilColor, kRounding);
  const bool won = view.phase == RogglePhase::kWon;
  const char* title = won ? core::Tr("Gagné !") : core::Tr("Perdu…");
  const ImVec2 size = ImGui::CalcTextSize(title);
  draw->AddText(ImVec2(origin.x + (width - size.x) * 0.5F, origin.y + height * 0.5F - size.y),
                won ? kWonColor : kLostColor, title);
  const char* hint = core::Tr("« Nouvelle partie » pour rejouer.");
  const ImVec2 hint_size = ImGui::CalcTextSize(hint);
  draw->AddText(ImVec2(origin.x + (width - hint_size.x) * 0.5F, origin.y + height * 0.5F),
                IM_COL32_WHITE, hint);
}

}  // namespace

RoggleAsk DrawRoggleWindow(RoggleState& state, const RoggleView& view, const SkinArt& art) {
  RoggleAsk asked;
  if (!state.open) {
    return asked;
  }
  const RoggleBoard& board = view.board;
  static WindowName name(kRoggleTitle, kRoggleId);
  ImGui::SetNextWindowSize(ImVec2(board.width + kChromeWidth, board.height + kChromeHeight),
                           ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    EndSkinnedWindow();
    return asked;
  }
  DrawHeader(view, asked, art);

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const bool clicked =
      ImGui::InvisibleButton("##roggle_plateau", ImVec2(board.width, board.height));
  if (view.phase == RogglePhase::kAiming && ImGui::IsItemHovered()) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    asked.aim = RogglePoint{.x = mouse.x - origin.x, .y = mouse.y - origin.y};
    asked.launch = clicked;
  }
  const auto at = [origin](RogglePoint point) {
    return ImVec2(origin.x + point.x, origin.y + point.y);
  };

  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, ImVec2(origin.x + board.width, origin.y + board.height), kBoardColor,
                      kRounding);
  draw->AddRect(origin, ImVec2(origin.x + board.width, origin.y + board.height), kBoardEdge,
                kRounding);
  for (const RogglePoint point : view.guide) {
    draw->AddCircleFilled(at(point), kGuideDotRadius, kGuideColor);
  }
  for (const RogglePeg& peg : view.pegs) {
    DrawPeg(draw, view, peg, at(peg.at));
  }

  const float half_bucket = board.bucket_width * 0.5F;
  const ImVec2 bucket_low =
      at({view.bucket_x - half_bucket, board.bucket_y - board.bucket_height * 0.5F});
  const ImVec2 bucket_high =
      at({view.bucket_x + half_bucket, board.bucket_y + board.bucket_height * 0.5F});
  draw->AddRectFilled(bucket_low, bucket_high, kBucketColor, 3.0F);
  draw->AddRect(bucket_low, bucket_high, kBucketEdge, 3.0F, 0, 1.5F);

  const ImVec2 cannon = at(board.cannon);
  draw->AddCircleFilled(cannon, kCannonRadius, kCannonColor);
  if (view.phase == RogglePhase::kAiming) {
    const float dx = view.aim.x - board.cannon.x;
    const float dy = view.aim.y - board.cannon.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length > 1.0F) {
      draw->AddLine(
          cannon,
          ImVec2(cannon.x + dx / length * kBarrelLength, cannon.y + dy / length * kBarrelLength),
          kBarrelColor, 3.0F);
    }
  }
  if (view.phase == RogglePhase::kFlying) {
    if (view.ball_art.usable()) {
      DrawFitted(draw, view.ball_art, at(view.ball), kBallDrawRadius);
    } else {
      DrawPoring(draw, at(view.ball), kPoringRadius);
    }
  }
  DrawOutcome(draw, origin, view);
  DrawTrophyToast(view.toast, view.toast_alpha, origin, board.width);

  EndSkinnedWindow();
  return asked;
}

}  // namespace ro::ui
