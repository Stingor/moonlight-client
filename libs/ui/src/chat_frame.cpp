#include "ro/ui/chat_frame.h"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

#include "ro/ui/skin_palette.h"
#include "ro/ui/window_bounds.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// L'épaisseur de la zone de saisie d'un bord. Assez large pour se viser sans
// soin, assez étroite pour ne pas manger le contenu.
constexpr float kResizeBandPixels = 8.0F;
// « La fenêtre touche-t-elle le bord de l'écran ? », à deux pixels près.
//
// ⚠ CETTE TOLÉRANCE-LÀ NE PASSE PAS PAR L'ÉCHELLE : c'est une distance en pixels
// d'ÉCRAN, pas une longueur d'interface. L'échelle n'a rien à y voir.
constexpr float kScreenTouchPixels = 2.0F;

// Les quatre bords, dans l'ordre où les tableaux ci-dessous les rangent.
enum Edge { kLeft = 0, kRight, kTop, kBottom, kEdgeCount };

constexpr const char* kEdgeIds[kEdgeCount] = {"##bord_gauche", "##bord_droit", "##bord_haut",
                                              "##bord_bas"};

// Ce que `BeginChatFrame` met de côté pour `EndChatFrame`, qui ne reçoit pas le
// style. ⚠ UNE SEULE À LA FOIS, comme la peinture d'une fenêtre : les chatbox ne
// s'imbriquent pas.
ChatStyle g_style;
int g_colors = 0;
int g_vars = 0;

// Les deux repères d'un geste, FIGÉS AU CLIC et tenus jusqu'au relâchement.
// `grab` est l'écart entre la souris et le bord saisi — sans lui, le bord saute
// sous le curseur au premier pixel. `fixed` est le bord OPPOSÉ, qui ne doit pas
// bouger du geste.
ImVec2 g_drag_grab;
ImVec2 g_drag_fixed;

// Le plafond de taille, relatif à l'écran. Jamais sous le plancher, même sur un
// écran minuscule : des bornes inversées ne veulent plus rien dire.
[[nodiscard]] ImVec2 LargestSize(const ChatStyle& style, ImVec2 display) {
  const ImVec2 share(display.x * kChatLargestScreenShare, display.y * kChatLargestScreenShare);
  return ImVec2(std::max(style.smallest_width, share.x), std::max(style.smallest_height, share.y));
}

// La hauteur, ramenée sur la grille des rangées. C'est la règle qui empêche le
// bas de la fenêtre de couper une ligne en deux.
[[nodiscard]] float SnapToRows(float wanted, const ChatStyle& style, float smallest,
                               float largest) {
  if (style.row_pixels <= 1.0F) {
    return wanted;
  }
  float rows = std::floor((wanted - style.chrome_pixels) / style.row_pixels + 0.5F);
  rows = std::max(rows, 1.0F);
  // Le plancher gagne sur le plafond : une fenêtre trop petite est inutilisable,
  // une fenêtre d'une rangée de trop ne l'est pas.
  while (style.chrome_pixels + rows * style.row_pixels < smallest) {
    rows += 1.0F;
  }
  while (rows > 1.0F && style.chrome_pixels + rows * style.row_pixels > largest) {
    rows -= 1.0F;
  }
  return style.chrome_pixels + rows * style.row_pixels;
}

// Les quatre bandes de saisie, et ce que le geste en cours veut faire de la
// fenêtre.
void ResizeByEdges(ImGuiWindow* window) {
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 display = io.DisplaySize;
  if (display.x <= 0.0F || display.y <= 0.0F) {
    return;
  }
  const ImVec2 smallest(g_style.smallest_width, g_style.smallest_height);
  const ImVec2 largest = LargestSize(g_style, display);

  const float band = kResizeBandPixels * Scale();
  const ImVec2 pos = window->Pos;
  const ImVec2 size = window->SizeFull;
  // ⚠ UN BORD COLLÉ À L'ÉCRAN EST INERTE : il ne peut pas s'écarter, donc le
  // tirer ferait grandir la fenêtre du côté opposé — l'inverse du geste demandé.
  const float right = pos.x + size.x;
  const float bottom = pos.y + size.y;
  //
  // ⚠ UN BOOLÉEN NOMMÉ PAR BORD, et ce n'est pas du zèle : écrits en liste,
  // « right < a, pos.y > b » se lit comme une liste de GABARIT, et le
  // formateur recolle tout sur une ligne. L'expression reste juste ; elle
  // devient indéchiffrable.
  const bool left_free = pos.x > kScreenTouchPixels;
  const bool right_free = right < display.x - kScreenTouchPixels;
  const bool top_free = pos.y > kScreenTouchPixels;
  const bool bottom_free = bottom < display.y - kScreenTouchPixels;
  const bool live[kEdgeCount] = {left_free, right_free, top_free, bottom_free};
  const ImVec2 band_min[kEdgeCount] = {
      ImVec2(pos.x, pos.y + band),
      ImVec2(right - band, pos.y + band),
      ImVec2(pos.x + band, pos.y),
      ImVec2(pos.x + band, bottom - band),
  };
  const ImVec2 band_size[kEdgeCount] = {
      ImVec2(band, size.y - 2.0F * band),
      ImVec2(band, size.y - 2.0F * band),
      ImVec2(size.x - 2.0F * band, band),
      ImVec2(size.x - 2.0F * band, band),
  };

  // ⚠ LE CURSEUR DE MISE EN PAGE ET LA BORNE DE CONTENU SONT RESTAURÉS : ces
  // bandes ne sont pas du contenu. Les laisser étendre `CursorMaxPos` donnerait à
  // la fenêtre quelques pixels de défilement fantôme — assez pour que la molette
  // fasse glisser tout l'habillage.
  const ImVec2 kept_cursor = window->DC.CursorPos;
  const ImVec2 kept_max = window->DC.CursorMaxPos;

  ImGuiContext& context = *ImGui::GetCurrentContext();
  ImVec2 wanted = size;
  bool changed = false;
  bool dragged_left = false;
  bool dragged_top = false;

  for (int edge = 0; edge < kEdgeCount; ++edge) {
    // Un bord qu'on est EN TRAIN de tirer reste posé même s'il vient de toucher
    // l'écran : le retirer en pleine image perdrait l'identifiant actif, et le
    // glissement s'interromprait au milieu du geste.
    const bool dragging_this = context.ActiveId == window->GetID(kEdgeIds[edge]);
    if ((!live[edge] && !dragging_this) || band_size[edge].x <= 0.0F || band_size[edge].y <= 0.0F) {
      continue;
    }
    ImGui::SetCursorScreenPos(band_min[edge]);
    // ⚠ `FlattenChildren` EST INDISPENSABLE : la zone de log est une fenêtre
    // ENFANT qui vient jusqu'à la marge, et un élément du parent n'est pas
    // survolable là où c'est l'enfant qui est sous la souris. Sans ce drapeau, la
    // bande utile se réduirait à l'épaisseur du filet.
    ImGui::InvisibleButton(kEdgeIds[edge], band_size[edge], ImGuiButtonFlags_FlattenChildren);
    if (ImGui::IsItemActivated()) {
      switch (edge) {
        case kLeft:
          g_drag_grab.x = io.MousePos.x - pos.x;
          g_drag_fixed.x = right;
          break;
        case kRight:
          g_drag_grab.x = io.MousePos.x - right;
          g_drag_fixed.x = pos.x;
          break;
        case kTop:
          g_drag_grab.y = io.MousePos.y - pos.y;
          g_drag_fixed.y = bottom;
          break;
        default:
          g_drag_grab.y = io.MousePos.y - bottom;
          g_drag_fixed.y = pos.y;
          break;
      }
    }
    const bool active = ImGui::IsItemActive();
    if (active || ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (!active) {
      continue;
    }
    changed = true;
    // ⚠ LE GESTE SE CALCULE DEPUIS LA POSITION ABSOLUE DE LA SOURIS, jamais en
    // cumulant les déplacements. Cumuler des écarts sur une taille déjà arrondie
    // à la rangée PERD tout mouvement plus court qu'une demi-rangée : le geste ne
    // produit rien tant qu'on bouge doucement, puis saute d'un cran. La largeur,
    // elle, n'étant pas quantifiée, le même code donnerait une largeur impeccable
    // et une hauteur inutilisable.
    const ImVec2 edge_now(io.MousePos.x - g_drag_grab.x, io.MousePos.y - g_drag_grab.y);
    switch (edge) {
      case kLeft:
        wanted.x = g_drag_fixed.x - edge_now.x;
        dragged_left = true;
        break;
      case kRight:
        wanted.x = edge_now.x - g_drag_fixed.x;
        break;
      case kTop:
        wanted.y = g_drag_fixed.y - edge_now.y;
        dragged_top = true;
        break;
      default:
        wanted.y = edge_now.y - g_drag_fixed.y;
        break;
    }
  }

  // ⚠ RESTAURATION PAR ÉCRITURE DIRECTE, surtout pas `SetCursorScreenPos` : ImGui
  // note « le curseur a été déplacé » et exige un élément derrière pour valider
  // l'agrandissement. Un appel en dernière position n'en a aucun, et il lève
  // « Code uses SetCursorPos() to extend window boundaries ». Ici on ne demande
  // rien à ImGui, on lui rend son état d'avant.
  window->DC.CursorPos = kept_cursor;
  window->DC.CursorMaxPos = kept_max;
  if (!changed) {
    return;
  }

  ImVec2 next_size(std::clamp(wanted.x, smallest.x, largest.x),
                   std::clamp(wanted.y, smallest.y, largest.y));
  next_size.y = SnapToRows(next_size.y, g_style, smallest.y, largest.y);

  // ⚠ LA POSITION SE DÉDUIT DU BORD FIGÉ, jamais d'un écart : c'est ce qui
  // garantit que le bord d'en face ne bouge pas d'un pixel, y compris quand une
  // borne ou l'arrondi à la rangée refusent la taille demandée.
  ImVec2 next_pos = pos;
  if (dragged_left) {
    next_pos.x = g_drag_fixed.x - next_size.x;
  }
  if (dragged_top) {
    next_pos.y = g_drag_fixed.y - next_size.y;
  }
  ImGui::SetWindowPos(window, next_pos);
  ImGui::SetWindowSize(window, next_size);
}

}  // namespace

bool BeginChatFrame(const char* name, const ChatStyle& style, ImGuiWindowFlags flags) {
  g_style = style;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, style.body);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_Border, style.border);
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kChatFieldColor);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kChatFieldHoverColor);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kChatFieldActiveColor);
  ImGui::PushStyleColor(ImGuiCol_PopupBg, kSkinPopupColor);
  ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, kSkinSelectionColor);
  // L'ascenseur d'ImGui rendu transparent : c'est `PaintWindowScrollbar` qui le
  // peint, avec les pièces du jeu.
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32_BLACK_TRANS);
  g_colors = 13;

  const float scale = Scale();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.rounding);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0F);
  // ⚠ À L'ÉCHELLE, OBLIGATOIREMENT : c'est la place que `PaintWindowScrollbar`
  // remplit de pièces mises à l'échelle. Un emplacement resté à 13 px les
  // laisserait déborder sur le contenu.
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, kSkinScrollbarWidth * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.padding, style.padding));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x, style.line_gap));
  g_vars = 8;

  // ⚠ PAS D'OPACITÉ GLOBALE : celle de cette fenêtre est dans l'alpha de son
  // fond. Les cumuler rendrait le TEXTE translucide en même temps que le fond, ce
  // que le chat natif ne fait pas.
  //
  // ⚠ `NoResize` COUPE LE REDIMENSIONNEMENT D'IMGUI, poignées de coin comprises —
  // aucun drapeau ne sait les désactiver séparément. Ce sont nos quatre bords,
  // posés par `EndChatFrame`, qui redimensionnent.
  flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize;
  if (!style.movable) {
    flags |= ImGuiWindowFlags_NoMove;
  }

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowSizeConstraints(ImVec2(style.smallest_width, style.smallest_height),
                                      LargestSize(style, display));
  // ⚠ APRÈS le plancher, jamais avant : c'est lui qui rabat ce qui vient d'être
  // posé sur ce que l'écran permet.
  ConstrainNextWindowToScreen(name);
  return ImGui::Begin(name, nullptr, flags);
}

void EndChatFrame() {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  // Verrouillée : on ne pose PAS les bandes. Les laisser en les rendant inertes
  // coûterait le même curseur « main » au survol, donc la promesse muette d'un
  // geste qui ne se produira pas.
  if (g_style.resizable && window != nullptr && !window->Collapsed && !window->Hidden) {
    ResizeByEdges(window);
  }
  ImGui::End();
  ImGui::PopStyleVar(g_vars);
  ImGui::PopStyleColor(g_colors);
}

}  // namespace ro::ui
