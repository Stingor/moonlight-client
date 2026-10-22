#include "options/bubble_samples.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/sim/broadcast_balloon.h"

#include "options/broadcast_balloon_options.h"
#include "world/broadcast_balloons.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

constexpr float kSampleMarginPx = 8.0F;
constexpr double kMsPerSecond = 1000.0;
// Le blanc entre deux vies de la bulle qui boucle : sans lui, sa fin et sa
// renaissance se confondraient.
constexpr float kSampleRestMs = 1000.0F;

// Les deux moitiés du fond : un sol sombre, puis un sol clair.
constexpr ImU32 kDarkGround = IM_COL32(0x2E, 0x3A, 0x2C, 0xFF);
constexpr ImU32 kLightGround = IM_COL32(0xD8, 0xCF, 0xB4, 0xFF);

void PaintBackdrop(ImDrawList& into, ImVec2 min, ImVec2 max) {
  const float middle = std::floor((min.x + max.x) * 0.5F);
  into.AddRectFilled(min, ImVec2(middle, max.y), kDarkGround);
  into.AddRectFilled(ImVec2(middle, min.y), max, kLightGround);
}

[[nodiscard]] double NowMs() { return ImGui::GetTime() * kMsPerSecond; }

// ── LES BULLES DE CHAT ──────────────────────────────────────────────────────

// Où se tiennent les deux têtes, en fraction de la largeur du cadre : l'une
// sur le sol sombre, l'autre sur le clair.
constexpr float kFixedHeadAt = 0.27F;
constexpr float kLivingHeadAt = 0.73F;
// La tête, un disque sous le point d'où la bulle monte.
constexpr float kHeadRadiusPx = 6.0F;
constexpr ImU32 kHeadColor = IM_COL32(0xE8, 0xC8, 0xA0, 0xFF);
// Les rangées que le cadre réserve aux bulles : la plus longue phrase se
// replie rarement au-delà.
constexpr float kChatSampleRows = 3.0F;

void PaintHead(ImDrawList& into, ImVec2 head) {
  into.AddCircleFilled(ImVec2(head.x, head.y + kHeadRadiusPx), kHeadRadiusPx, kHeadColor);
}

// ── LES BULLES D'ANNONCE ────────────────────────────────────────────────────

// Les couleurs du jeu : le jaune des annonces et des chasses, l'orangé des
// annonces d'objet.
constexpr ImU32 kAnnouncementColor = IM_COL32(0xFF, 0xFF, 0x00, 0xFF);
constexpr ImU32 kItemAnnounceColor = IM_COL32(0xFF, 0xB0, 0x62, 0xFF);
// Combien d'annonces le cadre montre au plus : de quoi voir l'écart entre deux
// bulles et ce que le réglage du nombre change, sans qu'il grandisse sans fin.
constexpr int kMostSampleAnnouncements = 3;
// Ce que la bulle qui boucle vit avant son fondu.
constexpr int kSampleHoldMs = 1500;

struct SampleBalloon {
  std::string text;
  ImU32 color = 0;
  bool wraps = false;
  bool new_family = false;
};

// Les bulles du cadre. ⚠ LA DERNIÈRE ANNONCE EST CELLE QUI BOUCLE.
[[nodiscard]] std::vector<SampleBalloon> SampleBalloons(const ro::ui::BroadcastBalloonLook& look) {
  const std::array<SampleBalloon, kMostSampleAnnouncements> announcements{{
      {.text = Tr("Bienvenue ! Le serveur redémarre dans dix minutes."),
       .color = kAnnouncementColor,
       .wraps = true},
      {.text = Tr("Poring a raffiné un équipement avec succès : +9 Knife obtenu."),
       .color = kItemAnnounceColor},
      {.text = Tr("Une annonce plus longue se replie à la largeur réglée, sur autant de rangées "
                  "qu'il faut pour la lire jusqu'au bout."),
       .color = kAnnouncementColor,
       .wraps = true},
  }};
  const int shown = std::clamp(look.most_per_family, 1, kMostSampleAnnouncements);
  std::vector<SampleBalloon> balloons(announcements.begin(), announcements.begin() + shown);
  balloons.push_back(
      {.text = Tr("Poring [ 3 / 10 ]"), .color = kAnnouncementColor, .new_family = true});
  return balloons;
}

// L'opacité de la bulle qui boucle à cet instant, ou moins un quand elle est
// éteinte. Elle rejoue la FIN de sa vie : un instant pleine, puis son fondu.
[[nodiscard]] float LoopingAlpha(const ro::ui::BroadcastBalloonLook& look, double now_ms) {
  const int duration_ms =
      std::clamp(look.duration_ms, ro::ui::kShortestBalloonMs, ro::ui::kLongestBalloonMs);
  const int shown_ms = std::min(duration_ms, kSampleHoldMs + (look.fade ? look.fade_ms : 0));
  const double phase_ms =
      std::fmod(now_ms, static_cast<double>(static_cast<float>(shown_ms) + kSampleRestMs));
  if (phase_ms >= static_cast<double>(shown_ms)) {
    return -1.0F;
  }
  const auto at_ms =
      static_cast<std::uint32_t>(duration_ms - shown_ms) + static_cast<std::uint32_t>(phase_ms);
  return BalloonAlpha(0, at_ms, look);
}

// Pose la pile dans le cadre, ou la mesure quand `into` est nul. Rend son bas.
float PaintSampleBalloons(ImDrawList* into, std::span<const SampleBalloon> samples,
                          const ro::ui::BroadcastBalloonLook& look, float center_x, float top_y,
                          float looping_alpha) {
  const float font_px =
      ro::ui::BalloonFontPixels(ro::sim::kBalloonFontPixels, look.font_scale_percent);
  std::vector<ro::ui::BalloonLineView> lines;
  lines.reserve(samples.size());
  std::vector<ro::ui::BalloonView> views;
  const std::size_t looping = samples.size() - 2;
  for (std::size_t rank = 0; rank < samples.size(); ++rank) {
    const SampleBalloon& sample = samples[rank];
    float alpha = 1.0F;
    if (rank == looping) {
      if (looping_alpha < 0.0F) {
        continue;
      }
      alpha = looping_alpha;
    }
    lines.push_back(ro::ui::BalloonLineView{.text = sample.text, .color = sample.color});
    views.push_back(ro::ui::BalloonView{.lines = std::span(&lines.back(), 1),
                                        .font_px = font_px,
                                        .wraps = sample.wraps,
                                        .alpha = alpha,
                                        .new_family = sample.new_family});
  }
  return ro::ui::PaintBroadcastBalloons(into, views, look, center_x, top_y);
}

}  // namespace

OptionRowView ChatBubbleSampleRow(const ro::ui::ChatBubbleLook& look) {
  // ⚠ LA BULLE GRANDIT AVEC SON TEXTE : le cadre réserve sa hauteur à l'échelle.
  const float scale = ro::ui::ChatBubbleScale(look);
  const float height =
      kSampleMarginPx * 2.0F + kChatSampleRows * ImGui::GetTextLineHeight() * scale +
      look.padding_y * 2.0F * scale + look.rise_pixels * scale + kHeadRadiusPx * 2.0F;
  return OptionRowView{
      .id = kChatBubbleSampleRow,
      .kind = OptionKind::kCanvas,
      .canvas_height = height,
      .paint = [look, fixed = std::string(Tr("Salut tout le monde !")),
                living = std::string(Tr("Quelqu'un pour une partie à Glast Heim ?"))](
                   ImDrawList& into, ImVec2 min, ImVec2 max) {
        PaintBackdrop(into, min, max);
        const float width = max.x - min.x;
        const float head_y = max.y - kSampleMarginPx - kHeadRadiusPx * 2.0F;
        const ImVec2 fixed_head(std::floor(min.x + width * kFixedHeadAt), head_y);
        const ImVec2 living_head(std::floor(min.x + width * kLivingHeadAt), head_y);
        PaintHead(into, fixed_head);
        PaintHead(into, living_head);
        const float life_ms = ro::ui::ChatBubbleLifeMs(look, living);
        const auto elapsed_ms =
            static_cast<float>(std::fmod(NowMs(), static_cast<double>(life_ms + kSampleRestMs)));
        const std::array bubbles{
            ro::ui::ChatBubbleView{.head = fixed_head, .text = fixed},
            ro::ui::ChatBubbleView{.head = living_head, .text = living, .elapsed_ms = elapsed_ms}};
        // ⚠ LE CADRE TIENT LIEU D'ÉCRAN : la largeur réglée en est une fraction.
        ro::ui::PaintChatBubbles(into, bubbles, look, width * look.width_fraction);
      }};
}

OptionRowView BroadcastBalloonSampleRow(const ro::ui::BroadcastBalloonLook& look) {
  std::vector<SampleBalloon> samples = SampleBalloons(look);
  const float stacked = PaintSampleBalloons(nullptr, samples, look, 0.0F, 0.0F, 1.0F);
  return OptionRowView{
      .id = kBroadcastBalloonSampleRow,
      .kind = OptionKind::kCanvas,
      .canvas_height = stacked + kSampleMarginPx * 2.0F,
      .paint = [look, samples = std::move(samples)](ImDrawList& into, ImVec2 min, ImVec2 max) {
        PaintBackdrop(into, min, max);
        PaintSampleBalloons(&into, samples, look, std::floor((min.x + max.x) * 0.5F),
                            min.y + kSampleMarginPx, LoopingAlpha(look, NowMs()));
      }};
}

}  // namespace moonclient
