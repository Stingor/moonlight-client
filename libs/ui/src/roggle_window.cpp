#include "ro/ui/roggle_window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include <imgui.h>

#include "ro/ui/minigames_window.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// La place que prennent les deux lignes du haut et les marges de la fenêtre.
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

// LES BILLES RESTANTES, en petits Porings le long du bord gauche.
constexpr float kSpareRadius = 6.0F;
constexpr float kSpareX = 14.0F;
constexpr float kSpareTopY = 64.0F;
constexpr float kSpareSpacingY = 16.0F;
constexpr int kMostSpareShown = 14;

// UN MOT monte de cette hauteur pendant sa vie ; un éclat grandit jusqu'à ce
// multiple du plot.
constexpr float kCalloutRisePx = 34.0F;
constexpr float kBurstGrowth = 1.2F;
// Les cases de la finale : le texte de leurs points, une marge sous le bord.
constexpr float kBinTextInsetPx = 4.0F;
// Le cadre du ralenti.
constexpr float kSlowFrameWidth = 4.0F;

// L'IMAGE D'UN PLATEAU D'OBJET : à peine visible sous les plots, et dévoilée
// en disques de ce rayon, assez grands pour se toucher à la maille la plus
// large.
constexpr float kPictureFaintShare = 0.14F;
constexpr float kRevealRadius = 30.0F;
// Les mots de la victoire, sous l'objet dévoilé.
constexpr float kWonTextBelowPx = 18.0F;

// LE CARNET : des cases d'icône, tant par rangée.
constexpr float kNotebookCellPx = 36.0F;
constexpr float kNotebookIconHalf = 14.0F;
constexpr int kNotebookColumns = 9;
constexpr float kNotebookHeight = 260.0F;

constexpr ImU32 kBoardColor = IM_COL32(18, 20, 34, 255);
constexpr ImU32 kBoardEdge = IM_COL32(80, 90, 140, 255);
constexpr ImU32 kOrangeColor = IM_COL32(255, 140, 20, 255);
constexpr ImU32 kOrangeLit = IM_COL32(255, 232, 150, 255);
constexpr ImU32 kOrangeRing = IM_COL32(255, 220, 120, 230);
constexpr ImU32 kBlueColor = IM_COL32(70, 130, 235, 255);
constexpr ImU32 kBlueLit = IM_COL32(210, 240, 255, 255);
constexpr ImU32 kBlueRing = IM_COL32(190, 225, 255, 230);
constexpr ImU32 kPurpleColor = IM_COL32(170, 80, 230, 255);
constexpr ImU32 kPurpleLit = IM_COL32(230, 200, 255, 255);
constexpr ImU32 kPurpleRing = IM_COL32(220, 170, 255, 230);
constexpr ImU32 kGreenColor = IM_COL32(60, 200, 90, 255);
constexpr ImU32 kGreenLit = IM_COL32(200, 255, 200, 255);
constexpr ImU32 kGreenRing = IM_COL32(170, 255, 170, 230);
constexpr ImU32 kPowerLetter = IM_COL32(255, 255, 255, 240);
constexpr ImU32 kShapeEdge = IM_COL32(0, 0, 0, 90);
constexpr ImU32 kBucketColor = IM_COL32(90, 220, 120, 255);
constexpr ImU32 kBucketEdge = IM_COL32(30, 90, 45, 255);
constexpr ImU32 kBinColor = IM_COL32(60, 50, 110, 255);
constexpr ImU32 kBestBinColor = IM_COL32(150, 110, 30, 255);
constexpr ImU32 kBinEdge = IM_COL32(200, 190, 255, 200);
constexpr ImU32 kCannonColor = IM_COL32(200, 200, 210, 255);
constexpr ImU32 kBarrelColor = IM_COL32(235, 235, 245, 255);
constexpr ImU32 kGuideColor = IM_COL32(255, 255, 255, 85);
constexpr ImU32 kVeilColor = IM_COL32(0, 0, 0, 150);
constexpr ImU32 kWonColor = IM_COL32(120, 255, 150, 255);
constexpr ImU32 kLostColor = IM_COL32(255, 120, 120, 255);
constexpr ImU32 kSlowColor = IM_COL32(255, 160, 40, 160);
constexpr ImU32 kPointsColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kStyleColor = IM_COL32(255, 220, 90, 255);
constexpr ImU32 kPowerColor = IM_COL32(140, 255, 160, 255);
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

// Une couleur, à cette part de son opacité.
[[nodiscard]] ImU32 Faded(ImU32 color, float share) {
  const auto alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU);
  const auto faded = static_cast<ImU32>(std::clamp(alpha * share, 0.0F, 255.0F));
  return (color & ~IM_COL32_A_MASK) | (faded << IM_COL32_A_SHIFT);
}

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

// Un texte centré sur un point.
void CenteredText(ImDrawList* draw, ImVec2 middle, ImU32 color, const char* text) {
  const ImVec2 size = ImGui::CalcTextSize(text);
  draw->AddText(ImVec2(middle.x - size.x * 0.5F, middle.y - size.y * 0.5F), color, text);
}

struct PegLook {
  ImU32 color = kBlueColor;
  ImU32 lit = kBlueLit;
  ImU32 ring = kBlueRing;
};

[[nodiscard]] PegLook LookOf(RogglePegKind kind) {
  switch (kind) {
    case RogglePegKind::kOrange:
      return {kOrangeColor, kOrangeLit, kOrangeRing};
    case RogglePegKind::kPurple:
      return {kPurpleColor, kPurpleLit, kPurpleRing};
    case RogglePegKind::kGreen:
      return {kGreenColor, kGreenLit, kGreenRing};
    case RogglePegKind::kBlue:
      break;
  }
  return {};
}

[[nodiscard]] const SkinImage& ArtOf(const RoggleView& view, RogglePegKind kind) {
  switch (kind) {
    case RogglePegKind::kOrange:
      return view.orange_art;
    case RogglePegKind::kPurple:
      return view.purple_art;
    case RogglePegKind::kGreen:
      return view.green_art;
    case RogglePegKind::kBlue:
      break;
  }
  return view.blue_art;
}

// L'initiale du pouvoir d'un plot vert, posée sur lui.
[[nodiscard]] const char* PowerLetter(RogglePower power) {
  switch (power) {
    case RogglePower::kMagnumBreak:
      return "M";
    case RogglePower::kSafetyWall:
      return "S";
    case RogglePower::kDoubleStrafe:
      return "D";
    case RogglePower::kNone:
      break;
  }
  return "";
}

void DrawPeg(ImDrawList* draw, const RoggleView& view, const RogglePeg& peg, ImVec2 middle) {
  const PegLook look = LookOf(peg.kind);
  const SkinImage& image = ArtOf(view, peg.kind);
  if (image.usable()) {
    DrawFitted(draw, image, middle, kPegDrawRadius,
               peg.lit ? IM_COL32_WHITE : ImGui::GetColorU32(IM_COL32_WHITE, kUnlitAlpha));
    if (peg.lit) {
      draw->AddCircle(middle, kPegDrawRadius + 1.0F, look.ring, 0, 2.0F);
    }
  } else {
    draw->AddCircleFilled(middle, view.board.peg_radius, peg.lit ? look.lit : look.color);
    draw->AddCircle(middle, view.board.peg_radius, kShapeEdge, 0, 1.5F);
  }
  if (peg.power != RogglePower::kNone) {
    draw->AddCircle(middle, kPegDrawRadius + 2.0F, kGreenRing, 0, 1.5F);
    CenteredText(draw, middle, kPowerLetter, PowerLetter(peg.power));
  }
}

void DrawHeader(RoggleState& state, const RoggleView& view, RoggleAsk& asked, const SkinArt& art) {
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
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Défi du jour"), art)) {
    asked.daily_game = true;
  }
  ImGui::SameLine();
  char notebook[64];
  std::snprintf(notebook, sizeof(notebook), core::Tr("Carnet (%d)"),
                static_cast<int>(view.notebook.size()));
  if (SkinSmallButton(notebook, art)) {
    state.notebook_open = !state.notebook_open;
  }

  FaintText(view.daily ? core::Tr("Défi du jour :") : core::Tr("Plateau :"));
  ImGui::SameLine();
  ImGui::TextUnformatted(view.layout_name);
  ImGui::SameLine(0.0F, gap);
  if (view.daily && view.daily_best > 0) {
    FaintText(core::Tr("Meilleur du jour :"));
    ImGui::SameLine();
    ImGui::Text("%d", view.daily_best);
  } else if (view.best_score > 0) {
    FaintText(core::Tr("Meilleur :"));
    ImGui::SameLine();
    ImGui::Text("%d", view.best_score);
  } else {
    FaintText(core::Tr("Visez à la souris, affinez à la molette, cliquez pour tirer."));
  }
}

void DrawSpareBalls(ImDrawList* draw, ImVec2 origin, int balls_left) {
  const int shown = std::min(balls_left, kMostSpareShown);
  for (int index = 0; index < shown; ++index) {
    DrawPoring(draw,
               ImVec2(origin.x + kSpareX,
                      origin.y + kSpareTopY + static_cast<float>(index) * kSpareSpacingY),
               kSpareRadius);
  }
}

void DrawFinaleBins(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  const RoggleBoard& board = view.board;
  if (view.finale_bins.empty()) {
    return;
  }
  const int best = *std::ranges::max_element(view.finale_bins);
  const float bin_width = board.width / static_cast<float>(view.finale_bins.size());
  const float top = origin.y + board.height - board.finale_bin_height;
  for (std::size_t index = 0; index < view.finale_bins.size(); ++index) {
    const float left = origin.x + static_cast<float>(index) * bin_width;
    const int points = view.finale_bins[index];
    const ImVec2 low(left, top);
    const ImVec2 high(left + bin_width, origin.y + board.height);
    draw->AddRectFilled(low, high, points == best ? kBestBinColor : kBinColor);
    draw->AddRect(low, high, kBinEdge);
    char text[16];
    std::snprintf(text, sizeof(text), "%d", points);
    CenteredText(draw, ImVec2(left + bin_width * 0.5F, top + board.finale_bin_height * 0.5F),
                 kPointsColor, text);
  }
}

void DrawBucket(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  const RoggleBoard& board = view.board;
  const float half_bucket = view.bucket_width * 0.5F;
  const ImVec2 low(origin.x + view.bucket_x - half_bucket,
                   origin.y + board.bucket_y - board.bucket_height * 0.5F);
  const ImVec2 high(origin.x + view.bucket_x + half_bucket,
                    origin.y + board.bucket_y + board.bucket_height * 0.5F);
  draw->AddRectFilled(low, high, kBucketColor, 3.0F);
  draw->AddRect(low, high, kBucketEdge, 3.0F, 0, 1.5F);
}

// LE MOT D'UN AVIS : son texte, dans `text`, et sa couleur.
[[nodiscard]] ImU32 CalloutText(const RoggleCallout& callout, char* text, std::size_t size) {
  switch (callout.kind) {
    case RoggleCalloutKind::kPoints:
    case RoggleCalloutKind::kBin:
      std::snprintf(text, size, "+%d", callout.points);
      return kPointsColor;
    case RoggleCalloutKind::kLongShot:
      std::snprintf(text, size, core::Tr("Tir lointain ! +%d"), callout.points);
      return kStyleColor;
    case RoggleCalloutKind::kBankShot:
      std::snprintf(text, size, core::Tr("Bande ! +%d"), callout.points);
      return kStyleColor;
    case RoggleCalloutKind::kVolley:
      std::snprintf(text, size, core::Tr("Rafale ! +%d"), callout.points);
      return kStyleColor;
    case RoggleCalloutKind::kFreeBall:
      std::snprintf(text, size, "%s", core::Tr("Bille offerte !"));
      return kStyleColor;
    case RoggleCalloutKind::kMagnumBreak:
      std::snprintf(text, size, "%s", "Magnum Break !");
      return kPowerColor;
    case RoggleCalloutKind::kSafetyWall:
      std::snprintf(text, size, "%s", "Safety Wall !");
      return kPowerColor;
    case RoggleCalloutKind::kDoubleStrafe:
      std::snprintf(text, size, "%s", "Double Strafe !");
      return kPowerColor;
    case RoggleCalloutKind::kFinale:
      std::snprintf(text, size, "%s", core::Tr("Dernier orange !"));
      return kStyleColor;
  }
  text[0] = '\0';
  return kPointsColor;
}

void DrawCallouts(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  for (const RoggleCallout& callout : view.callouts) {
    char text[64];
    const ImU32 color = CalloutText(callout, text, sizeof(text));
    const float rise = callout.progress * kCalloutRisePx;
    const ImVec2 size = ImGui::CalcTextSize(text);
    // Un mot reste dans le plateau, même né contre un bord.
    const float x = std::clamp(origin.x + callout.at.x - size.x * 0.5F, origin.x,
                               origin.x + view.board.width - size.x);
    draw->AddText(ImVec2(x, origin.y + callout.at.y - size.y - rise),
                  Faded(color, 1.0F - callout.progress), text);
  }
}

void DrawBursts(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  for (const RoggleBurst& burst : view.bursts) {
    const ImVec2 middle(origin.x + burst.at.x, origin.y + burst.at.y);
    const float radius = kPegDrawRadius * (1.0F + burst.progress * kBurstGrowth);
    const float fade = 1.0F - burst.progress;
    const PegLook look = LookOf(burst.kind);
    draw->AddCircleFilled(middle, kPegDrawRadius * fade, Faded(look.lit, fade));
    draw->AddCircle(middle, radius, Faded(look.ring, fade), 0, 2.0F);
  }
}

// L'image d'un rectangle du plateau, clippée au cadre de l'image : `low` et
// `high` en pixels du plateau.
void DrawPicturePart(ImDrawList* draw, ImVec2 origin, const RogglePicture& picture, ImVec2 low,
                     ImVec2 high, ImU32 tint, float rounding) {
  const float left = std::max(low.x, picture.origin.x);
  const float top = std::max(low.y, picture.origin.y);
  const float right = std::min(high.x, picture.origin.x + picture.width);
  const float bottom = std::min(high.y, picture.origin.y + picture.height);
  if (right <= left || bottom <= top) {
    return;
  }
  const ImVec2 uv_low((left - picture.origin.x) / picture.width,
                      (top - picture.origin.y) / picture.height);
  const ImVec2 uv_high((right - picture.origin.x) / picture.width,
                       (bottom - picture.origin.y) / picture.height);
  draw->AddImageRounded(picture.art.id, ImVec2(origin.x + left, origin.y + top),
                        ImVec2(origin.x + right, origin.y + bottom), uv_low, uv_high, tint,
                        rounding);
}

void DrawWholePicture(ImDrawList* draw, ImVec2 origin, const RogglePicture& picture, ImU32 tint) {
  DrawPicturePart(draw, origin, picture, ImVec2(picture.origin.x, picture.origin.y),
                  ImVec2(picture.origin.x + picture.width, picture.origin.y + picture.height), tint,
                  0.0F);
}

// L'OBJET SOUS LES PLOTS : son ombre, et les disques déjà dévoilés.
void DrawPicture(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  const RogglePicture& picture = view.picture;
  if (!picture.usable()) {
    return;
  }
  DrawWholePicture(draw, origin, picture, Faded(IM_COL32_WHITE, kPictureFaintShare));
  for (const RogglePoint at : picture.revealed) {
    DrawPicturePart(draw, origin, picture, ImVec2(at.x - kRevealRadius, at.y - kRevealRadius),
                    ImVec2(at.x + kRevealRadius, at.y + kRevealRadius), IM_COL32_WHITE,
                    kRevealRadius);
  }
}

// Le voile, puis deux lignes centrées dont la première finit à `text_y`, en
// pixels du plateau.
void DrawVeil(ImDrawList* draw, ImVec2 origin, const RoggleView& view, ImU32 color,
              const char* title, const char* hint, float text_y) {
  const float width = view.board.width;
  const float height = view.board.height;
  draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), kVeilColor, kRounding);
  const ImVec2 size = ImGui::CalcTextSize(title);
  draw->AddText(ImVec2(origin.x + (width - size.x) * 0.5F, origin.y + text_y - size.y), color,
                title);
  const ImVec2 hint_size = ImGui::CalcTextSize(hint);
  draw->AddText(ImVec2(origin.x + (width - hint_size.x) * 0.5F, origin.y + text_y), IM_COL32_WHITE,
                hint);
}

void DrawOutcome(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  const float middle_y = view.board.height * 0.5F;
  if (view.phase == RogglePhase::kWon && view.picture.usable()) {
    // L'OBJET DÉVOILÉ en entier, par-dessus le voile, et les mots dessous.
    const RogglePicture& picture = view.picture;
    const float below =
        picture.origin.y + picture.height + kWonTextBelowPx + ImGui::GetTextLineHeight();
    DrawVeil(draw, origin, view, kWonColor, core::Tr("Item dévoilé !"),
             core::Tr("Il rejoint le carnet."), below);
    DrawWholePicture(draw, origin, picture, IM_COL32_WHITE);
    return;
  }
  if (view.phase == RogglePhase::kWon || view.phase == RogglePhase::kLost) {
    const bool won = view.phase == RogglePhase::kWon;
    DrawVeil(draw, origin, view, won ? kWonColor : kLostColor,
             won ? core::Tr("Gagné !") : core::Tr("Perdu…"),
             core::Tr("« Nouvelle partie » pour rejouer."), middle_y);
    return;
  }
  if (view.paused) {
    DrawVeil(draw, origin, view, IM_COL32_WHITE, core::Tr("Pause"),
             core::Tr("Cliquez sur la fenêtre pour reprendre."), middle_y);
  }
}

void DrawCannon(ImDrawList* draw, ImVec2 origin, const RoggleView& view) {
  const RoggleBoard& board = view.board;
  const ImVec2 cannon(origin.x + board.cannon.x, origin.y + board.cannon.y);
  draw->AddCircleFilled(cannon, kCannonRadius, kCannonColor);
  if (view.phase != RogglePhase::kAiming) {
    return;
  }
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

// LA SOURIS ET LA MOLETTE sur le plateau : viser quand la souris bouge, affiner
// au cran, tirer au clic.
void ReadBoardInput(const RoggleView& view, ImVec2 origin, bool clicked, RoggleAsk& asked) {
  if (view.phase != RogglePhase::kAiming || !ImGui::IsItemHovered()) {
    return;
  }
  const ImGuiIO& io = ImGui::GetIO();
  if (io.MouseDelta.x != 0.0F || io.MouseDelta.y != 0.0F) {
    asked.aim = RogglePoint{.x = io.MousePos.x - origin.x, .y = io.MousePos.y - origin.y};
  }
  // La molette vers le haut tourne le canon dans le sens contraire des
  // aiguilles d'une montre.
  asked.aim_nudge = -static_cast<int>(std::lround(io.MouseWheel));
  // Le clic qui rend le focus à une partie en pause la reprend, sans tirer.
  asked.launch = clicked && !view.paused;
}

// LE CARNET : une case par objet dévoilé, son nom au survol.
void DrawNotebook(RoggleState& state, const RoggleView& view, const SkinArt& art) {
  static WindowName name(kRoggleNotebookTitle, kRoggleNotebookId);
  const float width = kNotebookCellPx * static_cast<float>(kNotebookColumns) + kChromeWidth;
  ImGui::SetNextWindowSize(ImVec2(width, kNotebookHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.notebook_open, art, SkinnedTitle::kBar)) {
    EndSkinnedWindow();
    return;
  }
  ImGui::Text(core::Tr("Items dévoilés : %d sur %d"), static_cast<int>(view.notebook.size()),
              view.notebook_total);
  if (view.notebook.empty()) {
    FaintText(core::Tr("Videz un plateau d'item de son orange pour l'y ranger."));
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (std::size_t index = 0; index < view.notebook.size(); ++index) {
    const RoggleNotebookEntry& entry = view.notebook[index];
    if (index % static_cast<std::size_t>(kNotebookColumns) != 0) {
      ImGui::SameLine(0.0F, 0.0F);
    }
    ImGui::PushID(static_cast<int>(index));
    const ImVec2 low = ImGui::GetCursorScreenPos();
    static_cast<void>(ImGui::InvisibleButton("##objet", ImVec2(kNotebookCellPx, kNotebookCellPx)));
    const ImVec2 middle(low.x + kNotebookCellPx * 0.5F, low.y + kNotebookCellPx * 0.5F);
    if (entry.icon.usable()) {
      DrawFitted(draw, entry.icon, middle, kNotebookIconHalf);
    } else {
      draw->AddCircleFilled(middle, kNotebookIconHalf * 0.5F, kOrangeColor);
    }
    if (ImGui::IsItemHovered() && BeginSkinTooltip()) {
      ImGui::TextUnformatted(entry.name);
      EndSkinTooltip();
    }
    ImGui::PopID();
  }
  EndSkinnedWindow();
}

}  // namespace

RoggleAsk DrawRoggleWindow(RoggleState& state, const RoggleView& view, const SkinArt& art) {
  RoggleAsk asked;
  if (!state.open) {
    state.notebook_open = false;
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
  asked.watching = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  DrawHeader(state, view, asked, art);

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const bool clicked =
      ImGui::InvisibleButton("##roggle_plateau", ImVec2(board.width, board.height));
  ReadBoardInput(view, origin, clicked, asked);

  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 far_corner(origin.x + board.width, origin.y + board.height);
  draw->AddRectFilled(origin, far_corner, kBoardColor, kRounding);
  draw->AddRect(origin, far_corner, view.slowed ? kSlowColor : kBoardEdge, kRounding, 0,
                view.slowed ? kSlowFrameWidth : 1.0F);
  DrawPicture(draw, origin, view);
  for (const RogglePoint point : view.guide) {
    draw->AddCircleFilled(ImVec2(origin.x + point.x, origin.y + point.y), kGuideDotRadius,
                          kGuideColor);
  }
  for (const RogglePeg& peg : view.pegs) {
    DrawPeg(draw, view, peg, ImVec2(origin.x + peg.at.x, origin.y + peg.at.y));
  }
  DrawBursts(draw, origin, view);
  if (view.finale) {
    DrawFinaleBins(draw, origin, view);
  } else {
    DrawBucket(draw, origin, view);
  }
  DrawSpareBalls(draw, origin, view.balls_left);
  DrawCannon(draw, origin, view);
  if (view.phase == RogglePhase::kFlying) {
    for (const RogglePoint ball : view.balls) {
      const ImVec2 middle(origin.x + ball.x, origin.y + ball.y);
      if (view.ball_art.usable()) {
        DrawFitted(draw, view.ball_art, middle, kBallDrawRadius);
      } else {
        DrawPoring(draw, middle, kPoringRadius);
      }
    }
  }
  DrawCallouts(draw, origin, view);
  DrawOutcome(draw, origin, view);
  DrawTrophyToast(view.toast, view.toast_alpha, origin, board.width);

  EndSkinnedWindow();
  // ⚠ APRÈS LE JEU : une fenêtre ne s'ouvre pas dans une autre.
  if (state.notebook_open) {
    DrawNotebook(state, view, art);
  }
  return asked;
}

}  // namespace ro::ui
