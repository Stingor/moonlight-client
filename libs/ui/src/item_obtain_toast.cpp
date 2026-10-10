#include "ro/ui/item_obtain_toast.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// L'échelle courante de l'interface : c'est elle qui décide de la taille des
// pièces, comme elle décide de celle du texte.
[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// La largeur d'une vue de texte. ⚠ PAR SES DEUX BORNES : une vue n'est pas
// terminée par un zéro, et la mesurer sans sa fin lirait ce qui suit.
[[nodiscard]] float TextWidth(ImFont& font, float pixels, std::string_view text) {
  if (text.empty()) {
    return 0.0F;
  }
  return font.CalcTextSizeA(pixels, FLT_MAX, 0.0F, text.data(), text.data() + text.size()).x;
}

// Écrit une vue de texte, par ses deux bornes pour la même raison.
void AddTextSpan(ImDrawList& into, ImFont& font, float pixels, ImVec2 at, ImU32 color,
                 std::string_view text) {
  if (text.empty()) {
    return;
  }
  into.AddText(&font, pixels, at, color, text.data(), text.data() + text.size());
}

// LE FOND LIBRE, peint à la place du cadre quand celui-ci est masqué.
//
// Contrairement au cadre en neuf morceaux, tout est peint en primitives : la
// couleur, l'opacité, l'arrondi et la bordure sont donc libres, ce que l'art ne
// permettait pas.
void DrawFreeBackground(ImDrawList& into, const ItemObtainToastLook& look, ImVec2 top_left,
                        ImVec2 bottom_right) {
  const float rounding = static_cast<float>(look.rounding_px);
  if (look.background_shown) {
    into.AddRectFilled(top_left, bottom_right, look.background, rounding);
  }
  if (look.border_shown && look.border_px > 0) {
    into.AddRect(top_left, bottom_right, look.border, rounding, 0,
                 static_cast<float>(look.border_px));
  }
}

// LA POIGNÉE D'ANCRE.
//
// 🔴 UNE VRAIE FENÊTRE, ET SEULEMENT DÉVERROUILLÉE. Le bandeau se peint sur la
// liste de premier plan : il passe au-dessus de tout, mais ne peut rien capter
// à la souris. Verrouillé, il redevient donc parfaitement transparent aux clics,
// ce qu'on attend d'une notification.
void DrawAnchor(const ItemObtainToastLook& look, float screen_width_px, ItemObtainAnchorGrip& grip,
                ItemObtainToastAsk* ask) {
  const bool centered = look.pos_x < 0;
  const float anchor_x = centered ? SnapToPixel((screen_width_px - grip.width_px) * 0.5F)
                                  : static_cast<float>(look.pos_x);
  // 🔴 CENTRÉ, UNE LARGEUR QUI CHANGE N'EST PAS UN GESTE : c'est le signe que la
  // poignée a été posée d'après une largeur qu'elle n'a plus — celle de repli à
  // sa naissance, ou celle d'une autre langue. On la REPOSE alors, au lieu de
  // lire l'écart comme un glisser qui ferait sortir du centrage.
  //
  // ⚠ ET SEULEMENT DANS CE CAS : imposer la position à chaque image
  // empêcherait purement et simplement de la déplacer à la souris.
  const bool recenter = centered && grip.applied && anchor_x != grip.applied_x;
  const bool impose = grip.replace || recenter;
  ImGui::SetNextWindowPos(ImVec2(anchor_x, static_cast<float>(look.pos_y)),
                          impose ? ImGuiCond_Always : ImGuiCond_Once);
  if (impose || !grip.applied) {
    grip.applied_x = anchor_x;
    grip.applied = true;
  }
  constexpr float kAnchorBackgroundAlpha = 0.45F;
  ImGui::SetNextWindowBgAlpha(kAnchorBackgroundAlpha);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin("###itemtoast_anchor", nullptr, flags)) {
    ImGui::TextUnformatted(core::Tr("Item obtenu — glisser pour placer"));
    grip.width_px = ImGui::GetWindowWidth();
    const ImVec2 at = ImGui::GetWindowPos();
    const int moved_x = static_cast<int>(SnapToPixel(at.x));
    const int moved_y = static_cast<int>(SnapToPixel(at.y));
    if (ask != nullptr) {
      ask->pos_x = look.pos_x;
      ask->pos_y = look.pos_y;
      // ⚠ UN DÉPLACEMENT HORIZONTAL SORT DU CENTRAGE — on ne peut pas à la fois
      // centrer et placer. Un déplacement purement VERTICAL le préserve :
      // descendre le bandeau ne doit pas le décentrer au passage.
      //
      // ⚠ L'ÉCART SE MESURE CONTRE L'ABSCISSE POSÉE, jamais contre celle qu'on
      // vient de recalculer : la seconde suit une largeur relevée après coup.
      if (moved_x != static_cast<int>(grip.applied_x)) {
        ask->pos_x = moved_x;
        ask->moved = true;
        // Le geste est pris en compte : sans cela, l'écart se redirait à chaque
        // image et le fichier de réglages serait touché sans fin.
        grip.applied_x = static_cast<float>(moved_x);
      }
      if (moved_y != look.pos_y) {
        ask->pos_y = moved_y;
        ask->moved = true;
      }
    }
  }
  ImGui::End();
  grip.replace = false;
}

}  // namespace

float ItemObtainFontPixels(const ItemObtainToastLook& look, float base_font_px) {
  const int percent = std::max(look.font_scale_percent, kLeastDrawableFontScalePercent);
  constexpr float kWholePercent = 100.0F;
  return SnapToPixel(base_font_px * static_cast<float>(percent) / kWholePercent);
}

float ItemObtainRowHeight(const ItemObtainToastLook& look, const ItemObtainMetrics& metrics) {
  const float pad_px = static_cast<float>(std::max(look.pad_vertical_px, 0)) * metrics.scale;
  const float icon_px = kToastIconPixels * metrics.scale;
  const float content_px =
      (look.show_icon && icon_px > metrics.font_px) ? icon_px : metrics.font_px;
  float height_px = SnapToPixel(content_px + 2.0F * pad_px);
  if (look.show_frame && metrics.tile_px >= 1.0F) {
    const float wanted_px = height_px;
    height_px = 2.0F * metrics.tile_px;
    if (wanted_px > height_px) {
      height_px += std::ceil((wanted_px - height_px) / metrics.tile_px) * metrics.tile_px;
    }
  }
  return height_px;
}

float ItemObtainTextLeft(const ItemObtainMetrics& metrics, bool has_icon) {
  const float left_px = has_icon ? kToastTextLeftPixels : kToastTextLeftNoIconPixels;
  return left_px * metrics.scale;
}

float ItemObtainRowWidth(const ItemObtainToastLook& look, const ItemObtainMetrics& metrics,
                         bool has_icon, float name_px, float suffix_px) {
  const float text_left_px = ItemObtainTextLeft(metrics, has_icon);
  float width_px =
      std::ceil(text_left_px + name_px + suffix_px + kToastRightPadPixels * metrics.scale);
  const float least_px = kLeastToastWidthPixels * metrics.scale;
  if (width_px < least_px) {
    width_px = least_px;
  }
  if (look.show_frame && metrics.tile_px >= 1.0F) {
    const float inner_px = width_px - 2.0F * metrics.tile_px;
    if (inner_px > 0.0F) {
      width_px = 2.0F * metrics.tile_px + std::ceil(inner_px / metrics.tile_px) * metrics.tile_px;
    }
  }
  return width_px;
}

float ItemObtainOriginX(const ItemObtainToastLook& look, const ItemObtainMetrics& metrics,
                        float row_width_px) {
  if (look.pos_x >= 0) {
    return static_cast<float>(look.pos_x);
  }
  // Le centrage divise par deux : une largeur impaire donne un x en .5.
  return SnapToPixel((metrics.screen_width_px - row_width_px) * 0.5F);
}

void DrawItemObtainToast(std::span<const ItemObtainToastEntry> lines,
                         const ItemObtainToastLook& look, const SkinArt& art,
                         ItemObtainAnchorGrip& grip, ItemObtainToastAsk* ask) {
  ItemObtainMetrics metrics;
  metrics.scale = Scale();
  metrics.screen_width_px = ImGui::GetIO().DisplaySize.x;

  if (!look.locked) {
    DrawAnchor(look, metrics.screen_width_px, grip, ask);
  }
  if (lines.empty()) {
    return;
  }

  ImDrawList* into = ImGui::GetForegroundDrawList();
  ImFont* font = ImGui::GetFont();
  if (into == nullptr || font == nullptr) {
    return;
  }

  metrics.font_px = ItemObtainFontPixels(look, ImGui::GetFontSize());
  metrics.tile_px = DescPanelEdge(art);
  const float row_height_px = ItemObtainRowHeight(look, metrics);
  const float gap_px = static_cast<float>(std::max(look.row_gap_px, 0)) * metrics.scale;
  const float icon_left_px = kToastIconLeftPixels * metrics.scale;

  float y = static_cast<float>(look.pos_y);
  for (const ItemObtainToastEntry& line : lines) {
    const bool has_icon = look.show_icon && line.icon.usable();
    const float text_left_px = ItemObtainTextLeft(metrics, has_icon);
    const float name_px = TextWidth(*font, metrics.font_px, line.name);
    const float suffix_px = TextWidth(*font, metrics.font_px, line.suffix);
    const float width_px = ItemObtainRowWidth(look, metrics, has_icon, name_px, suffix_px);
    const float x = ItemObtainOriginX(look, metrics, width_px);
    const ImVec2 top_left(x, y);
    const ImVec2 bottom_right(x + width_px, y + row_height_px);

    if (look.show_frame) {
      DrawDescPanelFrame(into, art, top_left, bottom_right, /*fill_bg=*/true);
    } else {
      DrawFreeBackground(*into, look, top_left, bottom_right);
    }

    if (has_icon) {
      // Blit un pour un, comme le client : l'icône est déjà à sa taille, et la
      // redimensionner ne ferait que la rendre floue.
      //
      // ⚠ CENTRÉE DANS LA LIGNE, et non posée à un décalage fixe : le client
      // écrit y+5 parce que sa ligne fait 32 ; la nôtre en fait 28, et le même
      // décalage ferait déborder l'icône du cadre par le bas.
      const ImVec2 icon_at(x + icon_left_px,
                           SnapToPixel(y + (row_height_px - line.icon.height) * 0.5F));
      into->AddImage(line.icon.id, icon_at,
                     ImVec2(icon_at.x + line.icon.width, icon_at.y + line.icon.height));
    }

    // ⚠ UN Y ENTIER : ImGui échantillonne l'atlas de police au plus proche, et
    // un y fractionnaire suffit à rendre tous les glyphes flous.
    const float text_y = SnapToPixel(y + (row_height_px - metrics.font_px) * 0.5F);
    const ImVec2 name_at(x + text_left_px, text_y);
    if (line.damaged) {
      // Un équipement CASSÉ ne change pas de couleur : c'est cette ombre-là qui
      // le fait paraître rouge, comme dans la conversation et sur les cases.
      AddTextSpan(
          *into, *font, metrics.font_px,
          ImVec2(name_at.x + kChatDamagedShadowOffset, name_at.y + kChatDamagedShadowOffset),
          kChatDamagedShadow, line.name);
    }
    AddTextSpan(*into, *font, metrics.font_px, name_at, look.text, line.name);
    AddTextSpan(*into, *font, metrics.font_px, ImVec2(SnapToPixel(name_at.x + name_px), text_y),
                look.quantity, line.suffix);

    y += row_height_px + gap_px;
  }
}

}  // namespace ro::ui
