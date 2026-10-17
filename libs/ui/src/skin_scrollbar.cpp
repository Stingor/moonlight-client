#include <algorithm>
#include <cmath>
#include <limits>

#include <imgui_internal.h>

#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES MESURES DE L'ASCENSEUR ──────────────────────────────────────────────
//
// ⚠ LA PISTE MORD SUR LES FLÈCHES de deux pixels, qui la recouvrent : sans ce
// recouvrement, la jointure laisse un trait clair visible à toutes les échelles.
constexpr float kScrollTrackOverlapPixels = 2.0F;
// De combien un clic de flèche avance, en lignes. C'est le pas de la molette
// d'ImGui : mêler les deux gestes ne doit pas changer de train.
constexpr float kScrollLinesPerStep = 3.0F;
// LE DÉFILEMENT D'UNE FLÈCHE MAINTENUE, en lignes par SECONDE : par image, il
// irait deux fois plus vite à 120 images qu'à 60.
constexpr float kScrollHeldLinesPerSecond = 36.0F;
// La place laissée au grip quand son art manque. ⚠ PAS ZÉRO : l'ascenseur
// descendrait alors jusqu'au coin et le grip d'ImGui, lui, serait toujours là.
constexpr float kResizeGripFallbackPixels = 13.0F;

// OÙ L'ASCENSEUR SE TIENT, flèches comprises. La piste utile, elle, se resserre
// entre les deux flèches — voir `PaintWindowScrollbar`.
struct ScrollRail {
  float left = 0.0F;
  float right = 0.0F;
  float top = 0.0F;
  float bottom = 0.0F;
  float arrow = 0.0F;
  bool usable = false;
};

// Les six pièces d'un ascenseur : la piste en trois, la poignée en trois.
constexpr std::array<render::SkinPiece, 6> kScrollPieces = {
    render::SkinPiece::kScrollUp,         render::SkinPiece::kScrollMiddle,
    render::SkinPiece::kScrollDown,       render::SkinPiece::kScrollGrabTop,
    render::SkinPiece::kScrollGrabMiddle, render::SkinPiece::kScrollGrabBottom};

[[nodiscard]] bool ScrollPaintable(const SkinArt& art) {
  return std::ranges::all_of(kScrollPieces,
                             [&art](render::SkinPiece piece) { return art.Get(piece).usable(); });
}

// Peint une bande VERTICALE en trois morceaux : deux bouts à leur taille, un
// milieu étiré. C'est la forme d'une piste et celle d'une poignée.
void PaintVerticalStrip(ImDrawList* into, const SkinArt& art, render::SkinPiece top,
                        render::SkinPiece middle, render::SkinPiece bottom, float x0, float x1,
                        float y0, float y1) {
  const float scale = Scale();
  const float span = y1 - y0;
  // Même règle que pour une bande horizontale : deux bouts qui se partagent une
  // place trop courte, plutôt qu'un milieu à l'envers.
  const float cap_top = std::min(art.Get(top).height * scale, span * 0.5F);
  const float cap_bottom = std::min(art.Get(bottom).height * scale, span - cap_top);
  const float middle_start = y0 + cap_top;
  const float middle_end = std::max(middle_start, y1 - cap_bottom);

  PaintArt(into, art, art.Get(top).id, ImVec2(x0, y0), ImVec2(x1, middle_start));
  if (middle_end > middle_start) {
    PaintArt(into, art, art.Get(middle).id, ImVec2(x0, middle_start), ImVec2(x1, middle_end));
  }
  PaintArt(into, art, art.Get(bottom).id, ImVec2(x0, middle_end), ImVec2(x1, y1));
}

// Peint les trois morceaux d'un visage de bouton.

// CE QU'UNE FENÊTRE DONNE À LA FORMULE DE LA POIGNÉE.
//
// 🔴 LES TROIS LIGNES SONT CELLES D'`ImGui::Scrollbar`, RECOPIÉES, marge de
// fenêtre comprise. `ContentSize.y` seul décrit un contenu plus court que ce
// qu'ImGui fait défiler, et la poignée peinte cesse alors de tomber sur celle
// qui écoute le clic.
[[nodiscard]] ScrollExtent ExtentOf(const ImGuiWindow* window) {
  ScrollExtent extent;
  if (window == nullptr) {
    return extent;
  }
  extent.visible = window->InnerRect.Max.y - window->InnerRect.Min.y;
  extent.content = window->ContentSize.y + window->WindowPadding.y * 2.0F;
  extent.scroll = window->Scroll.y;
  return extent;
}

// LE GLISSER EN COURS, rangé DANS LA FENÊTRE qui le porte : relâcher au-dessus
// d'une autre ne fait pas défiler celle-là, et deux contextes ne se le
// partagent pas.
//
// ⚠ IL NE VIT QUE D'UNE IMAGE À LA SUIVANTE. Une fenêtre fermée pendant qu'on
// tient sa poignée ne repasse plus ici : rouvrie plus tard, bouton enfoncé,
// elle reprendrait un glisser que personne n'a redemandé. L'image où il a été
// vu pour la dernière fois le date, et un trou d'une image le lâche.
struct HeldGrab {
  ImGuiID frame_key = 0;
  ImGuiID offset_key = 0;
};
// L'image « jamais vue » : plus ancienne que toute image réelle.
constexpr int kNeverHeldFrame = std::numeric_limits<int>::min();

[[nodiscard]] HeldGrab HeldGrabKeys(ImGuiWindow* window) {
  return HeldGrab{.frame_key = window->GetID("##ascenseur-tenu"),
                  .offset_key = window->GetID("##ascenseur-prise")};
}

// CE QUE LE GESTE ÉCRIT : le défilement, tout de suite, et la cible d'ImGui
// annulée avec.
//
// 🔴 `ScrollTarget` EST CE QUI REPRENDRAIT LA MAIN. ImGui garde une position
// VISÉE, qu'il applique en fin d'image par-dessus ce qu'on vient d'écrire :
// sans ce `FLT_MAX` — sa valeur « aucune cible » — notre défilement serait
// effacé à chaque fois qu'un autre code aurait demandé un `SetScrollY`.
void ScrollTo(ImGuiWindow* window, float wanted) {
  window->Scroll.y = std::clamp(wanted, 0.0F, window->ScrollMax.y);
  window->ScrollTarget.y = FLT_MAX;
}

// OÙ L'ASCENSEUR SE TIENT : mesuré UNE fois, pour le dessin ET pour le geste.
// Les deux doivent lire exactement la même géométrie, sinon l'on clique à côté
// de ce que l'on voit.
[[nodiscard]] ScrollRail ScrollRailOf(ImGuiWindow* window, const SkinArt& art) {
  ScrollRail rail;
  if (window == nullptr || !window->ScrollbarY || !ScrollPaintable(art)) {
    return rail;
  }
  const ImRect slot = ImGui::GetWindowScrollbarRect(window, ImGuiAxis_Y);
  rail.left = slot.Min.x;
  rail.right = slot.Max.x;
  rail.top = slot.Min.y;
  rail.bottom = slot.Max.y;

  // 🔴 LA PLACE DU GRIP SE RETIRE DU BAS. Sans cela l'ascenseur descend jusqu'au
  // coin et recouvre la poignée de redimensionnement : elle reste dessinée, mais
  // ne s'attrape plus. On retire exactement la hauteur du bouton, et
  // seulement sur une fenêtre qui a VRAIMENT un grip — une fenêtre enfant ou une
  // table n'en a pas, et raccourcir la sienne laisserait un trou en bas.
  const bool has_grip = (window->Flags & ImGuiWindowFlags_ChildWindow) == 0 &&
                        (window->Flags & ImGuiWindowFlags_NoResize) == 0 &&
                        (window->Flags & ImGuiWindowFlags_AlwaysAutoResize) == 0;
  if (has_grip) {
    const SkinImage& grip = art.Get(render::SkinPiece::kResizeGrip);
    rail.bottom -= (grip.usable() ? grip.height : kResizeGripFallbackPixels) * Scale();
  }

  // Deux flèches et rien entre elles : la piste disparaît plutôt que de
  // s'inverser, et la poignée avec.
  rail.arrow = std::min(art.Get(render::SkinPiece::kScrollUp).height * Scale(),
                        (rail.bottom - rail.top) * 0.5F);
  rail.usable = rail.bottom - rail.top > 0.0F && rail.right > rail.left;
  return rail;
}

// ── LE GESTE, PRIS ENTIÈREMENT ──────────────────────────────────────────────
//
// 🔴 IMGUI N'A PAS DE FLÈCHES. Sa barre est une piste et une poignée, rien de
// plus : sous les deux boutons qu'on peint, il n'y a jamais eu personne pour
// écouter le clic. Les dessiner sans les servir promettait un geste qui ne
// répond pas, ce qui est pire qu'un bouton absent.
//
// Et l'on ne peut pas prendre les flèches seules : dès que la piste se resserre
// entre elles, la poignée d'ImGui n'est plus là où on la peint. Les deux vont
// ensemble.
void DriveScrollbar(ImGuiWindow* window, const ScrollRail& rail, float track_top,
                    float track_height, float grab_height) {
  ImGuiContext* const context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    return;
  }
  const ImGuiIO& io = ImGui::GetIO();
  const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

  // 🔴 NE PAS DISPUTER UNE SOURIS QU'IMGUI TIENT DÉJÀ. Cet ascenseur lit la
  // souris BRUTE, donc il échappe à l'arbitrage par `ActiveId` qui empêche deux
  // widgets de répondre au même clic. Sans cette garde, la zone sensible du grip
  // de redimensionnement — plus large que son image — déborde sur la flèche du
  // bas, et un clic dans le recouvrement redimensionnait la fenêtre ET faisait
  // défiler.
  //
  // ⚠ UN GLISSER DÉJÀ COMMENCÉ CONTINUE : c'est NOUS qui le tenons, et ImGui n'a
  // aucun `ActiveId` pour lui. Sans l'exception, l'ascenseur se figerait dès
  // qu'un widget devient actif ailleurs dans la fenêtre.
  // ⚠ IL SE RELÂCHE au bouton levé, ou après une image sans passer ici : voir
  // `HeldGrab`.
  ImGuiStorage& kept = window->StateStorage;
  const HeldGrab keys = HeldGrabKeys(window);
  const int held_frame = kept.GetInt(keys.frame_key, kNeverHeldFrame);
  bool dragging = down && held_frame >= context->FrameCount - 1;
  if (!dragging) {
    kept.SetInt(keys.frame_key, kNeverHeldFrame);
  }
  const bool imgui_holds_mouse = context->ActiveId != 0;

  // 🔴 ET LA SOURIS DOIT ÊTRE SUR CETTE FENÊTRE-LÀ. `IsMouseHoveringRect` ne
  // connaît que des coordonnées : il ignore ce qui est posé PAR-DESSUS, et un
  // clic sur la fenêtre de devant attrapait aussi l'ascenseur de celle de
  // dessous. `ActiveId` n'y suffit pas — ImGui ne décide de déplacer une fenêtre
  // qu'à la FIN de l'image du clic, si bien qu'au moment qui compte il vaut
  // encore zéro. `HoveredWindow`, lui, est déjà juste.
  const ImGuiWindow* const hovered =
      context->HoveredWindow != nullptr ? context->HoveredWindow->RootWindow : nullptr;
  const bool mouse_on_us = hovered != nullptr && hovered == window->RootWindow;
  const bool may_take = !imgui_holds_mouse && mouse_on_us;

  // 🔴 ET IMGUI DOIT LÂCHER LE BOUTON GAUCHE SUR CE RAIL. Sa barre est toujours
  // là sous la nôtre — c'est elle qui réserve la place et que nous rendons
  // transparente — et elle continuait d'écouter : un clic sur une flèche
  // déclenchait son « clic dans la piste », qui TÉLÉPORTE la poignée sous le
  // curseur. La flèche du bas sautait donc en fin de liste au lieu d'avancer
  // d'un pas, ce qui se lit « la flèche ne marche pas ».
  //
  // ⚠ POSÉE DÈS LE SURVOL, PAS AU CLIC : ImGui traite sa barre dans `End()`,
  // après nous, mais il lit un bouton dont l'appartenance se décide avant. Et
  // elle TIENT pendant un glisser, sinon il repasserait à lui en cours de route.
  const ImGuiID ours = window->GetID("##ascenseur");
  const bool on_rail = ImGui::IsMouseHoveringRect(ImVec2(rail.left, rail.top),
                                                  ImVec2(rail.right, rail.bottom), false);
  if (dragging || (may_take && on_rail)) {
    ImGui::SetKeyOwner(ImGuiKey_MouseLeft, ours);
  }

  const float grab_start =
      track_top + (window->ScrollMax.y > 0.0F
                       ? (window->Scroll.y / window->ScrollMax.y) * (track_height - grab_height)
                       : 0.0F);

  // LE GLISSER DE LA POIGNÉE.
  if (track_height > grab_height) {
    const bool on_grab = ImGui::IsMouseHoveringRect(
        ImVec2(rail.left, grab_start), ImVec2(rail.right, grab_start + grab_height), false);
    if (!dragging && on_grab && clicked && may_take) {
      dragging = true;
      kept.SetFloat(keys.offset_key, io.MousePos.y - grab_start);
    }
    if (dragging) {
      kept.SetInt(keys.frame_key, context->FrameCount);
      const float drag_offset = kept.GetFloat(keys.offset_key);
      const float ratio = (io.MousePos.y - drag_offset - track_top) / (track_height - grab_height);
      ScrollTo(window, std::clamp(ratio, 0.0F, 1.0F) * window->ScrollMax.y);
    }
  }
  // ⚠ UN GLISSER EN COURS MANGE LES FLÈCHES : la poignée peut passer dessous en
  // fin de course, et l'on ajouterait un pas à chaque image sans l'avoir demandé.
  if (dragging) {
    return;
  }

  // LES FLÈCHES : un clic vaut un pas, et le maintien fait défiler doucement.
  // Le pas est celui de la molette d'ImGui — trois lignes —, pour qu'un client
  // qui mêle les deux gestes avance du même train.
  if (!may_take || !down) {
    return;
  }
  const float line = ImGui::GetTextLineHeightWithSpacing();
  const float step = line * kScrollLinesPerStep;
  // ⚠ LE MAINTIEN ATTEND LE DÉLAI DE RÉPÉTITION DU CLAVIER : sans lui, un clic
  // un peu lent ajoutait au pas quelques images de défilement continu.
  const bool repeating = io.MouseDownDuration[ImGuiMouseButton_Left] >= io.KeyRepeatDelay;
  if (!clicked && !repeating) {
    return;
  }
  const float moved = clicked ? step : line * kScrollHeldLinesPerSecond * io.DeltaTime;
  if (ImGui::IsMouseHoveringRect(ImVec2(rail.left, rail.top),
                                 ImVec2(rail.right, rail.top + rail.arrow), false)) {
    ScrollTo(window, window->Scroll.y - moved);
    return;
  }
  if (ImGui::IsMouseHoveringRect(ImVec2(rail.left, rail.bottom - rail.arrow),
                                 ImVec2(rail.right, rail.bottom), false)) {
    ScrollTo(window, window->Scroll.y + moved);
    return;
  }

  // LE CLIC DANS LA PISTE : une PAGE, du côté où l'on a cliqué.
  //
  // 🔴 IL FAUT LE REPRENDRE PUISQU'ON A PRIS LE BOUTON. ImGui le traitait, et
  // autrement : sa barre TÉLÉPORTE la poignée sous le curseur. Le client, lui,
  // avance d'un écran — c'est ce qu'on fait ici, et sans quoi un clic dans la
  // piste ne ferait plus rien du tout.
  //
  // ⚠ AU CLIC SEUL, jamais au maintien : une page par image emporterait la
  // liste entière avant qu'on ait relâché.
  if (!clicked || !on_rail) {
    return;
  }
  const float page = std::max(window->InnerRect.GetHeight() - line, line);
  ScrollTo(window, window->Scroll.y + (io.MousePos.y < grab_start ? -page : page));
}

}  // namespace

ScrollGrab VerticalScrollGrab(float track_height, float visible, float content, float scroll,
                              float smallest_grab) {
  ScrollGrab grab;
  if (track_height <= 0.0F) {
    return grab;
  }
  // Tout tient : la poignée occupe la piste entière, et ne bouge pas.
  const float whole = std::max(std::max(content, visible), 1.0F);
  const float height = std::clamp(track_height * (visible / whole),
                                  std::min(track_height, smallest_grab), track_height);
  const float travel = std::max(content - visible, 1.0F);
  const float ratio = std::clamp(scroll / travel, 0.0F, 1.0F);
  grab.start = ratio * (track_height - height);
  grab.end = grab.start + height;
  return grab;
}

ScrollExtent WindowScrollExtent() { return ExtentOf(ImGui::GetCurrentWindow()); }

void PaintWindowScrollbar(const SkinArt& art) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ScrollRail rail = ScrollRailOf(window, art);
  if (!rail.usable) {
    return;
  }

  // 🔴 LA PISTE VIT ENTRE LES DEUX FLÈCHES. La poignée d'ImGui, elle, court
  // d'un bord à l'autre du créneau — il n'a pas de flèches à épargner : peinte
  // telle quelle, elle passerait SOUS celles qu'on redessine par-dessus, et
  // disparaîtrait en fin de course. L'ascenseur du client ne fait jamais ça.
  //
  // Confiner la peinture sans confiner le GESTE aurait donné pire : on aurait
  // cliqué à côté de la poignée qu'on voit. C'est pourquoi l'interaction se
  // prend entière ci-dessous.
  const float track_top = rail.top + rail.arrow;
  const float track_bottom = rail.bottom - rail.arrow;
  const float track_height = track_bottom - track_top;

  const ImGuiStyle& style = ImGui::GetStyle();
  const ScrollExtent extent = ExtentOf(window);
  const float travel = window->ScrollMax.y;
  const bool scrollable = travel > 0.0F && track_height > 0.0F;

  ScrollGrab grab = VerticalScrollGrab(track_height, extent.visible, extent.content,
                                       window->Scroll.y, style.GrabMinSize);
  if (scrollable) {
    DriveScrollbar(window, rail, track_top, track_height, grab.end - grab.start);
    // ⚠ REFAITE APRÈS LE GESTE : le défilement vient de changer, et une poignée
    // calculée avant traînerait d'une image derrière le doigt.
    grab = VerticalScrollGrab(track_height, extent.visible, extent.content, window->Scroll.y,
                              style.GrabMinSize);
  }

  ImDrawList* into = window->DrawList;
  // ⚠ LA PISTE DÉBORDE DE LA ZONE DE CONTENU : sans élargir le découpage, elle
  // serait rognée, comme le cadre l'était.
  into->PushClipRect(window->Pos,
                     ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y), false);
  // ⚠ LA PISTE MORD SUR LES FLÈCHES de deux pixels, qui la recouvrent ensuite :
  // sans ce recouvrement, la jointure laisse un trait clair visible à toutes les
  // échelles. Même règle que la piste du curseur de réglage.
  const float overlap = kScrollTrackOverlapPixels * Scale();
  PaintArt(into, art, art.Get(render::SkinPiece::kScrollMiddle).id,
           ImVec2(rail.left, track_top - overlap), ImVec2(rail.right, track_bottom + overlap));
  PaintArt(into, art, art.Get(render::SkinPiece::kScrollUp).id, ImVec2(rail.left, rail.top),
           ImVec2(rail.right, rail.top + rail.arrow));
  PaintArt(into, art, art.Get(render::SkinPiece::kScrollDown).id,
           ImVec2(rail.left, rail.bottom - rail.arrow), ImVec2(rail.right, rail.bottom));
  if (scrollable && grab.end > grab.start) {
    PaintVerticalStrip(into, art, render::SkinPiece::kScrollGrabTop,
                       render::SkinPiece::kScrollGrabMiddle, render::SkinPiece::kScrollGrabBottom,
                       rail.left, rail.right, std::floor(track_top + grab.start),
                       std::floor(track_top + grab.end));
  }
  into->PopClipRect();

  // LA MAIN QUI POINTE sur tout l'ascenseur, comme sur celui d'ImGui.
  //
  // ⚠ SEULEMENT SI SA FENÊTRE EST SOUS LA SOURIS : le rectangle seul vaut aussi
  // pour un ascenseur caché derrière une autre fenêtre, qui donnerait la main à
  // un endroit où rien ne se clique.
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
      ImGui::IsMouseHoveringRect(ImVec2(rail.left, rail.top), ImVec2(rail.right, rail.bottom),
                                 false)) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
}

}  // namespace ro::ui
