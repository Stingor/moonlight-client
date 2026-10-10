#include "ro/ui/window_bounds.h"

#include <cfloat>

#include <imgui_internal.h>

namespace ro::ui {
namespace {

// L'écran, ou rien quand il est de taille nulle.
[[nodiscard]] ImVec2 ScreenOrNothing() {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  return (screen.x > 0.0F && screen.y > 0.0F) ? screen : ImVec2(0.0F, 0.0F);
}

// Ce que le bord de l'écran laisse encore grandir à la fenêtre que le joueur
// TIENT par une poignée ou par un bord. `FLT_MAX` sur un axe : rien ne le
// contraint de ce côté — ou personne ne tient rien.
//
// ⚠ LE CÔTÉ TENU DÉCIDE, et c'est tout l'objet. Tirer le coin bas-droit laisse
// le coin haut-gauche en place : la largeur ne peut donc pas dépasser ce qui
// reste d'écran À DROITE. Tirer le bord gauche laisse au contraire le bord droit
// en place, et la largeur bute alors sur le bord GAUCHE de l'écran. Une borne
// unique interdirait l'un des deux gestes.
//
// ⚠ ET SEULEMENT PENDANT LE GESTE. Posée en permanence, cette borne
// empêcherait une fenêtre de se donner sa taille à l'ouverture tant que la passe
// de position ne l'a pas ramenée dans l'écran.
[[nodiscard]] ImVec2 EdgeLimitWhileHeld(ImGuiWindow* window, ImVec2 screen, ImGuiID held) {
  ImVec2 limit(FLT_MAX, FLT_MAX);
  if (window == nullptr || held == 0) {
    return limit;
  }
  // Ce qui reste d'écran de chaque côté, mesuré depuis le bord qui, lui, NE
  // BOUGE PAS pendant le geste.
  const float toward_right = screen.x - window->Pos.x;
  const float toward_left = window->Pos.x + window->SizeFull.x;
  const float toward_bottom = screen.y - window->Pos.y;
  const float toward_top = window->Pos.y + window->SizeFull.y;

  // ⚠ DEUX POIGNÉES, PAS QUATRE : ImGui n'en arme jamais plus de deux — le coin
  // bas-droit toujours, le bas-gauche quand le redimensionnement par les bords
  // est ouvert. Les deux autres rangs existent dans sa table, mais aucun clic ne
  // peut les atteindre.
  if (held == ImGui::GetWindowResizeCornerID(window, 0)) {
    return {toward_right, toward_bottom};
  }
  if (held == ImGui::GetWindowResizeCornerID(window, 1)) {
    return {toward_left, toward_bottom};
  }

  if (held == ImGui::GetWindowResizeBorderID(window, ImGuiDir_Right)) {
    limit.x = toward_right;
  } else if (held == ImGui::GetWindowResizeBorderID(window, ImGuiDir_Left)) {
    limit.x = toward_left;
  } else if (held == ImGui::GetWindowResizeBorderID(window, ImGuiDir_Down)) {
    limit.y = toward_bottom;
  } else if (held == ImGui::GetWindowResizeBorderID(window, ImGuiDir_Up)) {
    limit.y = toward_top;
  }
  return limit;
}

}  // namespace

ImVec2 ClampWindowPosToScreen(ImVec2 window_pos, ImVec2 window_size) {
  const ImVec2 screen = ScreenOrNothing();
  if (screen.x <= 0.0F) {
    return window_pos;
  }
  ImVec2 clamped = window_pos;
  clamped.x =
      window_size.x >= screen.x ? 0.0F : ImClamp(window_pos.x, 0.0F, screen.x - window_size.x);
  clamped.y =
      window_size.y >= screen.y ? 0.0F : ImClamp(window_pos.y, 0.0F, screen.y - window_size.y);
  return clamped;
}

void ConstrainNextWindowToScreen(const char* name) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  const ImVec2 screen = ScreenOrNothing();
  if (context == nullptr || screen.x <= 0.0F) {
    return;
  }
  // La fenêtre peut ne pas exister encore : c'est sa toute première image, elle
  // n'a donc ni position ni poignée, et seule la part d'écran la borne.
  const ImVec2 edge = EdgeLimitWhileHeld(ImGui::FindWindowByName(name), screen, context->ActiveId);
  const ImVec2 largest(ImMin(screen.x * kMaxScreenFraction, edge.x),
                       ImMin(screen.y * kMaxScreenFraction, edge.y));

  if ((context->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint) == 0) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0F, 0.0F), largest);
    return;
  }

  // ⚠ ON RABAT LE PLAFOND DE L'APPELANT, ON NE LUI CÈDE PAS LA MAIN. Renoncer
  // dès qu'une contrainte existe, « parce que la sienne est plus précise », est
  // se coucher devant les fautives : un plafond à `FLT_MAX` ne contraint rien, et
  // c'est la valeur qu'on écrit quand on ne veut borner que le minimum.
  //
  // Son MINIMUM est gardé : il sait mieux que nous ce que son contenu exige.
  ImRect& already = context->NextWindowData.SizeConstraintRect;
  // ⚠ UNE BORNE NÉGATIVE VEUT DIRE « N'Y TOUCHE PAS » pour ImGui : il garde alors
  // `SizeFull` tel quel sur cet axe, qui reste donc LIBRE. On la remplace par la
  // nôtre au lieu de la comparer.
  already.Min.x = ImMax(already.Min.x, 0.0F);
  already.Min.y = ImMax(already.Min.y, 0.0F);
  already.Max.x = already.Max.x < 0.0F ? largest.x : ImMin(already.Max.x, largest.x);
  already.Max.y = already.Max.y < 0.0F ? largest.y : ImMin(already.Max.y, largest.y);
  // Un minimum passé au-dessus du maximum donnerait un `ImClamp` inversé : sur un
  // petit écran, le plancher d'une fenêtre peut dépasser notre part d'écran.
  already.Min.x = ImMin(already.Min.x, already.Max.x);
  already.Min.y = ImMin(already.Min.y, already.Max.y);
}

}  // namespace ro::ui
