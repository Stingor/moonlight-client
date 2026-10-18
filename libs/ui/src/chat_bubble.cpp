#include "ro/ui/chat_bubble.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "ro/ui/chat_log.h"

#include "chat_log_parts.h"
#include "ui_color.h"
#include "ui_strings.h"

namespace ro::ui {
namespace {

using chat_log_detail::LayOutLine;
using chat_log_detail::Pen;
using chat_log_detail::PlaceFrame;
using chat_log_detail::PlaceTarget;

// La fraction de fin de vie sur laquelle une bulle s'efface. ⚠ LA FIN SEULE :
// une bulle qui pâlit dès son apparition se lit comme un défaut d'affichage.
constexpr float kFadeFraction = 0.25F;

}  // namespace

std::size_t ChatBubbleShownCharacters(std::string_view text,
                                      std::span<const ChatInlineImage> images) {
  std::size_t shown = Utf8CodePoints(text);
  for (const ChatInlineImage& image : images) {
    if (image.length == 0 || image.offset >= text.size()) {
      continue;
    }
    const std::size_t covered = Utf8CodePoints(text.substr(image.offset, image.length));
    shown = shown - std::min(shown, covered) + 1;
  }
  return shown;
}

float ChatBubbleLifeMs(const ChatBubbleLook& look, std::string_view text,
                       std::span<const ChatInlineImage> images) {
  if (look.client_timing) {
    return kClientBubbleMs;
  }
  const float life = look.base_ms + look.per_character_ms *
                                        static_cast<float>(ChatBubbleShownCharacters(text, images));
  return std::clamp(life, kShortestBubbleMs, kLongestBubbleMs);
}

void PaintChatBubbles(ImDrawList& into_list, std::span<const ChatBubbleView> bubbles,
                      const ChatBubbleLook& look, float most_width_px, const ChatRichArt* rich) {
  if (bubbles.empty()) {
    return;
  }
  ImDrawList* into = &into_list;
  ImFont* const font = ImGui::GetFont();
  const float line_height = ImGui::GetTextLineHeight();
  const float most_width = std::max(0.0F, most_width_px);

  // Ce que le fil sait peindre, SANS SES VIGNETTES : leur rangée grandirait, et
  // leur attente demanderait un téléchargement que la bulle n'a pas à lancer.
  ChatRichArt bubble_rich;
  if (rich != nullptr) {
    bubble_rich = *rich;
    bubble_rich.picture_pixels = 0.0F;
    bubble_rich.seen_pictures = nullptr;
  }
  const ChatLogStyle style{.font_size = ImGui::GetFontSize(),
                           .rich = rich != nullptr ? &bubble_rich : nullptr};
  // Une largeur nulle laisse la bulle d'un seul tenant.
  PlaceFrame frame{.font = font,
                   .style = &style,
                   .wrap_width = most_width > 0.0F ? most_width : FLT_MAX,
                   .step = line_height,
                   .view_top = -FLT_MAX,
                   .view_bottom = FLT_MAX,
                   .now_s = ImGui::GetTime()};
  std::vector<float> row_widths;
  std::vector<float> row_shift;

  for (const ChatBubbleView& bubble : bubbles) {
    const float life_ms = ChatBubbleLifeMs(look, bubble.text, bubble.images);
    if (bubble.text.empty() || bubble.elapsed_ms >= life_ms) {
      continue;
    }
    // L'opacité réglée, puis le fondu de fin PAR-DESSUS : les deux se composent,
    // sinon baisser l'opacité ferait reparaître une bulle en pleine extinction.
    float alpha = std::clamp(look.opacity, 0.0F, 1.0F);
    if (look.fade) {
      const float left = 1.0F - bubble.elapsed_ms / life_ms;
      alpha *= std::min(1.0F, left / kFadeFraction);
    }
    const ChatLineView line{
        .text = bubble.text,
        .color = (bubble.text_color != 0 ? bubble.text_color : ImGui::GetColorU32(ImGuiCol_Text)) |
                 IM_COL32_A_MASK,
        .links = bubble.links,
        .styles = bubble.styles,
        .images = bubble.images};

    // 🔴 MESURER D'ABORD, PAR LE MÊME MOTEUR : la largeur de chaque rangée dit
    // de combien la décaler pour la centrer, et elle dépend des styles — un mot
    // gras, une emote, un nom d'objet n'ont pas la largeur de leur texte nu.
    row_widths.clear();
    frame.origin = ImVec2(0.0F, 0.0F);
    frame.row_shift = {};
    Pen measured{.row = line_height, .row_widths = &row_widths};
    LayOutLine(frame, PlaceTarget{}, line, &measured);
    const float widest = row_widths.empty() ? 0.0F : *std::ranges::max_element(row_widths);
    const float tall = measured.y;
    row_shift.clear();
    for (const float width : row_widths) {
      row_shift.push_back((widest - width) * 0.5F);
    }

    // ⚠ LA BULLE POUSSE VERS LE HAUT depuis la tête : son BAS est le point
    // donné, moins la montée. Prendre le haut pour référence la ferait descendre
    // sur le personnage à mesure que la phrase s'allonge.
    const float bottom = bubble.head.y - look.rise_pixels;
    const ImVec2 top_left(bubble.head.x - widest * 0.5F - look.padding_x,
                          bottom - tall - look.padding_y * 2.0F);
    const ImVec2 bottom_right(bubble.head.x + widest * 0.5F + look.padding_x, bottom);

    const int first_vertex = into->VtxBuffer.Size;
    if (look.background) {
      into->AddRectFilled(top_left, bottom_right, look.background_color, look.corner_radius);
      into->AddRect(top_left, bottom_right, look.border_color, look.corner_radius);
    }
    frame.origin = ImVec2(bubble.head.x - widest * 0.5F, top_left.y + look.padding_y);
    frame.row_shift = row_shift;
    Pen pen{.row = line_height};
    // ⚠ NI ZONES DE CLIC NI MÉMOIRE DES RÉVÉLATIONS : un spoiler reste masqué.
    LayOutLine(frame, PlaceTarget{.into = into}, line, &pen);
    FadeSince(into, first_vertex, alpha);
  }
}

void DrawChatBubbles(std::span<const ChatBubbleView> bubbles, const ChatBubbleLook& look,
                     const ChatRichArt* rich) {
  // ⚠ LA LISTE DE FOND : au-dessus du monde et des jauges, que le renderer a
  // déjà peints ; SOUS toutes les fenêtres, qu'une bulle ne doit pas traverser.
  PaintChatBubbles(*ImGui::GetBackgroundDrawList(), bubbles, look,
                   ImGui::GetIO().DisplaySize.x * look.width_fraction, rich);
}

}  // namespace ro::ui
