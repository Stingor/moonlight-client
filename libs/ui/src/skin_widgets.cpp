#include "ro/ui/skin_widgets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Ce qu'un bouton laisse respirer au-dessus et au-dessous de son texte.
constexpr float kButtonTextMarginPixels = 1.0F;
// En deçà, le texte n'est plus lisible : mieux vaut qu'il dépasse un peu.
constexpr float kSmallestButtonTextPixels = 8.0F;

// Le sélecteur déroulé. Un seul nom suffit : ImGui n'en tient qu'un ouvert.
constexpr const char* kSwatchPopup = "##nuancier";

// Ce qu'il reste d'un écart vertical dans un panneau resserré.
constexpr float kCompactRatio = 0.60F;

// ⛔ PAS DE RABATTEMENT SUR LA HAUTEUR DU CADRE. Le texte d'un bouton est
// celui de l'interface, point — c'est ce que fait le jeu. L'art du PETIT
// bouton est plus bas qu'une ligne : son texte dépasse donc d'un pixel et demi
// en haut, et c'est voulu. On réserve pour cela trois pixels AU-DESSUS de l'art
// (`art_drop`) plutôt que de rapetisser le libellé.
//
// 🔴 UN RABATTEMENT SE VERRAIT PARTOUT : il écrirait les petits boutons en
// douze pixels quand le client les écrit en quinze, et l'écart se lirait sur
// chaque rangée de l'interface.

// ── UN LIBELLÉ PLUS LONG QUE SON BOUTON ────────────────────────────────────
//
// 🔴 IL SE RAPETISSE, PUIS IL SE COUPE — il ne déborde jamais. Un bouton du
// jeu dessine son texte À LA MAIN dans la liste d'affichage : ImGui ne le
// clippe pas, et un libellé trop long bave hors de l'art, par-dessus ce qui
// se trouve à côté.
//
// ⚠ ET CE N'EST PAS UN CAS RARE. La mise en page est taillée pour le
// français ; « Tout déposer » dans une demi-largeur, ou « +100M » dans une
// case de rangée, ne tiennent déjà pas — sans parler de l'anglais et de
// l'espagnol, qui rallongent presque tout.
//
// La taille descend par crans ENTIERS et non au pixel juste : chaque taille
// distincte fait graver une police de plus dans l'atlas.
//
// ⚠ PLANCHER BAS, ET DÉLIBÉRÉMENT : entre un libellé rapetissé et un libellé
// coupé, le rapetissé reste lisible et dit encore ce que fait le bouton —
// « Quitar todo el eq… » ne dit plus rien. La coupe ne sert qu'aux cas
// désespérés.
constexpr float kLabelSmallestShare = 0.65F;

// Ce qu'on colle derrière un libellé coupé. ⚠ TROIS POINTS ASCII et non le
// caractère d'ellipse : celui-là n'est pas garanti dans l'atlas.
constexpr const char* kLabelCut = "...";

struct FittedLabel {
  float size = 0.0F;
  const char* begin = nullptr;
  const char* end = nullptr;
  bool cut = false;
  // La largeur RENDUE, les points compris : c'est elle qui centre.
  float width = 0.0F;
};

// `room` est la place utile, coiffes de l'art déjà retirées. Une place nulle
// ou négative — un bouton minuscule — laisse le libellé tel quel : mieux vaut
// un débordement qu'un texte réduit à néant.
[[nodiscard]] FittedLabel FitButtonLabel(const char* label, float size, float room) {
  ImFont* const font = ImGui::GetFont();
  FittedLabel fit;
  fit.begin = label;
  fit.end = ImGui::FindRenderedTextEnd(label);
  fit.size = size;
  fit.width = font->CalcTextSizeA(size, FLT_MAX, 0.0F, fit.begin, fit.end).x;
  if (room <= 0.0F || fit.width <= room) {
    return fit;
  }

  // ⚠ ON PART D'UNE ESTIMATION PUIS ON DESCEND CRAN PAR CRAN, en mesurant à
  // chaque fois : la largeur n'est PAS proportionnelle à la taille — les
  // avances sont arrondies au pixel — et une seule tentative rate d'un pixel,
  // puis tombe dans la coupe alors qu'un cran de plus suffisait.
  const float smallest = std::max(1.0F, std::floor(size * kLabelSmallestShare));
  for (float trying = std::clamp(std::floor(size * (room / fit.width)), smallest, size);;
       trying -= 1.0F) {
    fit.size = trying;
    fit.width = font->CalcTextSizeA(trying, FLT_MAX, 0.0F, fit.begin, fit.end).x;
    if (fit.width <= room || trying <= smallest) {
      break;
    }
  }
  if (fit.width <= room) {
    return fit;
  }

  // Ça déborde même au plancher : couper au caractère, et coller les points.
  const float whole = fit.width;
  const float dots = font->CalcTextSizeA(fit.size, FLT_MAX, 0.0F, kLabelCut).x;
  const char* stop = fit.begin;
  const float kept =
      font->CalcTextSizeA(fit.size, std::max(1.0F, room - dots), 0.0F, fit.begin, fit.end, &stop).x;
  // 🔴 UNE COUPE QUI NE GARDE RIEN EST PIRE QUE LE DÉBORDEMENT, et le cas est
  // réel : un bouton large d'une hauteur de ligne, dont les coiffes mangent la
  // moitié, ne garde aucun caractère et n'écrit que « ... » — trois points plus
  // larges que le signe qu'ils remplacent. Même verdict quand la coupe n'est
  // pas plus étroite que le texte entier.
  if (stop == fit.begin || kept + dots >= whole) {
    fit.width = whole;
    return fit;
  }
  fit.end = stop;
  fit.cut = true;
  fit.width = kept + dots;
  return fit;
}

// Les trois états d'un bouton, chacun en trois morceaux.
struct ButtonFaces {
  std::array<render::SkinPiece, 3> resting;
  std::array<render::SkinPiece, 3> hovered;
  std::array<render::SkinPiece, 3> pressed;
};

constexpr ButtonFaces kFaces = {
    {render::SkinPiece::kButtonLeft, render::SkinPiece::kButtonMiddle,
     render::SkinPiece::kButtonRight},
    {render::SkinPiece::kButtonHoverLeft, render::SkinPiece::kButtonHoverMiddle,
     render::SkinPiece::kButtonHoverRight},
    {render::SkinPiece::kButtonPressedLeft, render::SkinPiece::kButtonPressedMiddle,
     render::SkinPiece::kButtonPressedRight}};

// LE MÊME BOUTON, EN PETIT. Une barre qui doit partager sa ligne avec autre
// chose ne peut pas se payer deux boutons pleine taille.
constexpr ButtonFaces kSmallFaces = {
    {render::SkinPiece::kSmallButtonLeft, render::SkinPiece::kSmallButtonMiddle,
     render::SkinPiece::kSmallButtonRight},
    {render::SkinPiece::kSmallButtonHoverLeft, render::SkinPiece::kSmallButtonHoverMiddle,
     render::SkinPiece::kSmallButtonHoverRight},
    {render::SkinPiece::kSmallButtonPressedLeft, render::SkinPiece::kSmallButtonPressedMiddle,
     render::SkinPiece::kSmallButtonPressedRight}};

// ── CE QUI DISTINGUE LES DEUX BOUTONS DU JEU ───────────────────────────────
//
// Leurs pièces, et six mesures. Toutes sont des RELEVÉS sur le client, et
// aucune n'est symétrique entre les deux — c'est pourquoi elles se déclarent
// plutôt qu'elles ne se déduisent.
struct ButtonLook {
  const ButtonFaces& faces;
  // Largeur automatique = texte + coiffes + ceci. ⚠ NE SE RETRANCHE PAS de la
  // place du libellé plus bas : c'est du confort, pas de l'encombrement, et
  // l'y décompter rapetisserait des libellés qui tiennent très bien.
  float auto_pad = 0.0F;
  // Place du libellé = l'espace entre les coiffes, moins ceci.
  float room_margin = 0.0F;
  // 🔴 DE COMBIEN L'ART DESCEND SOUS LE HAUT DE L'ITEM. Le petit bouton est
  // plus court qu'une ligne de texte : on réserve un item plus haut que l'art
  // et l'on peint l'art dans sa partie basse.
  //
  // ⚠ SURTOUT PAS EN DÉPLAÇANT LE CURSEUR : quand le bouton OUVRE la ligne, ce
  // décalage en devient l'origine, et les boutons posés ensuite en `SameLine`
  // — qui reviennent à cette origine — se décalent encore d'autant. Le premier
  // paraît alors plus haut que ses voisins.
  float art_drop = 0.0F;
  // Le réglage vertical du libellé, pour le recentrer dans sa case.
  float text_dy = 0.0F;
  // Le libellé suit-il l'art qui s'enfonce ?
  bool press_sinks = false;
  // ⚠ RESSERRER CONTRE LE WIDGET PRÉCÉDENT, ET SEULEMENT SUR LA MÊME LIGNE :
  // l'art du jeu porte déjà sa marge, et l'espacement d'ImGui l'éloigne trop.
  // Quand le bouton OUVRE la ligne il n'y a rien à resserrer, et ces trois
  // pixels le feraient mordre sur la marge gauche — donc hors du rectangle de
  // découpe, donc rogné.
  bool tighten_sameline = false;
};

// De combien l'art du petit bouton descend, et de combien son libellé remonte.
constexpr float kSmallButtonDropPixels = 3.0F;
constexpr float kSmallButtonTextLiftPixels = 1.0F;
// Ce que le petit bouton retire à son voisin de ligne.
constexpr float kSmallButtonTightenPixels = 3.0F;
// Ce qui sépare le libellé du GRAND bouton de son art.
constexpr float kButtonRoomMarginPixels = 2.0F;

// ⚠ LE GRAND N'A NI CHUTE D'ART NI RESSERREMENT, le petit n'a ni confort de
// largeur ni marge de place, et le libellé ne s'enfonce que sur le grand.
constexpr ButtonLook kBigLook{.faces = kFaces,
                              .auto_pad = kButtonPaddingPixels,
                              .room_margin = kButtonRoomMarginPixels,
                              .press_sinks = true};
constexpr ButtonLook kSmallLook{.faces = kSmallFaces,
                                .art_drop = kSmallButtonDropPixels,
                                .text_dy = -kSmallButtonTextLiftPixels,
                                .tighten_sameline = true};

// Vrai quand les neuf pièces sont là. Il n'y a pas de demi-bouton à peindre :
// un état manquant ferait disparaître le bouton au survol.
[[nodiscard]] bool Paintable(const SkinArt& art, const ButtonFaces& faces) {
  const auto usable = [&art](const std::array<render::SkinPiece, 3>& face) {
    return art.Get(face[0]).usable() && art.Get(face[1]).usable() && art.Get(face[2]).usable();
  };
  return usable(faces.resting) && usable(faces.hovered) && usable(faces.pressed);
}

// La largeur qu'un bouton prendrait en taille automatique, DANS LA LANGUE
// COURANTE : de quoi dimensionner un conteneur sur ses libellés traduits.
[[nodiscard]] float AutoButtonWidth(const char* label, const SkinArt& art, const ButtonLook& look) {
  if (!Paintable(art, look.faces)) {
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0F;
  }
  // ⚠ LE TEXTE PLUS LES COIFFES, et le confort par-dessus : la largeur d'un
  // petit bouton automatique vaut PILE son libellé et ses deux bouts d'art.
  const float scale = Scale();
  const float caps =
      (art.Get(look.faces.resting[0]).width + art.Get(look.faces.resting[2]).width) * scale;
  const float text = ImGui::CalcTextSize(label, nullptr, true).x;
  return text + caps + look.auto_pad * scale;
}

// L'ITEM QUE L'ON POSE EST-IL GRISÉ ? Vrai sous `BeginDisabled`.
//
// ⚠ IMGUI NE GRISE QUE CE QU'IL DESSINE LUI-MÊME : l'art posé à la main doit se
// griser ici, sans quoi il resterait éclatant sous un libellé qui ne répond plus.
[[nodiscard]] bool NextItemDisabled() {
  return (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
}

void PaintHorizontalFace(ImDrawList* into, const SkinArt& art,
                         const std::array<render::SkinPiece, 3>& face, ImVec2 top_left,
                         ImVec2 bottom_right, float left_width, float right_width, ImU32 tint) {
  const StripRects parts = HorizontalStrip(top_left, bottom_right, left_width, right_width);
  const std::array<const ImVec4*, 3> where = {&parts.left, &parts.middle, &parts.right};
  for (std::size_t rank = 0; rank < 3; ++rank) {
    if (where[rank]->z <= where[rank]->x) {
      continue;
    }
    into->AddImage(art.Get(face[rank]).id, ImVec2(where[rank]->x, where[rank]->y),
                   ImVec2(where[rank]->z, where[rank]->w), ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                   tint);
  }
}

// LE BOUTON, UNE SEULE FOIS.
//
// ⚠ LES QUATRE NE DIFFÈRENT QUE PAR DEUX CHOSES : leur jeu de pièces, et le
// fait de rester enfoncés. Les écrire quatre fois, c'était quatre endroits où
// corriger le même détail — et trois où l'oublier.
[[nodiscard]] bool PaintedButton(const char* label, const SkinArt& art, const ButtonLook& look,
                                 ImVec2 size, bool active) {
  if (!Paintable(art, look.faces)) {
    // ⚠ SANS ART, LE RELIEF VIENT DE LA COULEUR, faute de mieux : un bouton
    // ordinaire ne sait pas rester enfoncé, et deux choix identiques ne se
    // distingueraient plus du tout.
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
    }
    const bool clicked = ImGui::Button(label, size);
    if (active) {
      ImGui::PopStyleColor();
    }
    return clicked;
  }

  const float scale = Scale();
  const SkinImage& left = art.Get(look.faces.resting[0]);
  const SkinImage& right = art.Get(look.faces.resting[2]);
  // 🔴 `size.y` EST LA PLACE PRISE, celle que rend `SkinSmallButtonHeight` : la
  // chute réservée au libellé s'en RETRANCHE pour donner la hauteur de l'art.
  //
  // ⚠ LES DEUX DOIVENT DIRE LA MÊME CHOSE. Si la mesure rendait l'item et que
  // la taille lisait l'art, un appelant qui passe l'une à l'autre — la barre de
  // titre le fait, le pied du coffre aussi — étirerait son art de la chute ET
  // recevrait un item plus haut de trois pixels que celui qu'il a réservé.
  // Dans une barre de titre, l'art en sortirait par le bas.
  const float art_drop = look.art_drop * scale;
  const ImVec2 wanted(size.x > 0.0F ? size.x : AutoButtonWidth(label, art, look),
                      size.y > 0.0F ? std::max(0.0F, size.y - art_drop) : left.height * scale);

  // Resserrer contre le widget d'à côté — voir `ButtonLook::tighten_sameline`.
  if (look.tighten_sameline && ImGui::GetCurrentWindow()->DC.IsSameLine) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - kSmallButtonTightenPixels * scale);
  }
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const bool disabled = NextItemDisabled();

  // ⚠ LE BOUTON INVISIBLE D'ABORD : c'est lui qui prend la place dans la
  // disposition et qui reçoit le clic. L'art n'est QUE de la peinture, et le
  // peindre avant laisserait un bouton qu'on voit sans pouvoir l'atteindre.
  const bool clicked = ImGui::InvisibleButton(label, ImVec2(wanted.x, wanted.y + art_drop));
  const bool held = ImGui::IsItemActive() || active;
  const bool over = ImGui::IsItemHovered();
  // LA MAIN QUI POINTE : c'est ce que le client montre au-dessus de tout ce
  // qui se clique, et c'est souvent le seul indice qu'une chose se clique.
  if (over) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  const std::array<render::SkinPiece, 3>& face =
      held ? look.faces.pressed : (over ? look.faces.hovered : look.faces.resting);

  // L'ART DANS LA PARTIE BASSE DE L'ITEM : la chute est au-dessus de lui.
  const ImVec2 painted(at.x, at.y + art_drop);
  ImDrawList* into = ImGui::GetWindowDrawList();
  // GRISÉ, L'ART S'EFFACE ET LE LIBELLÉ PASSE AU GRIS : l'état de l'art, lui,
  // reste celui du bouton — un choix retenu demeure enfoncé.
  PaintHorizontalFace(
      into, art, face, painted, ImVec2(painted.x + wanted.x, painted.y + wanted.y),
      left.width * scale, right.width * scale,
      PaintedTint(held || over ? art.look().button_hover_tint : art.look().button_tint, disabled));

  // LE LIBELLÉ, centré, et qui SUIT l'art quand il s'enfonce : sans cela le
  // bouton paraîtrait pressé sous un texte resté en place.
  // ⚠ MESURÉ À LA TAILLE OÙ IL SERA ÉCRIT, sinon le centrage porte sur une boîte
  // qui n'est pas celle qu'on dessine.
  // ⚠ LA TAILLE EST CELLE DE L'INTERFACE, rabattue par la seule LARGEUR : le
  // jeu n'écrit pas plus petit dans un petit bouton, il laisse son texte
  // dépasser de l'art — c'est à cela que sert la chute réservée plus haut.
  const float room = wanted.x - (left.width + right.width) * scale - look.room_margin * scale;
  const FittedLabel fit = FitButtonLabel(label, ImGui::GetFontSize(), room);
  const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(fit.size, FLT_MAX, 0.0F, fit.begin, fit.end);
  // ⚠ LE LIBELLÉ NE S'ENFONCE QUE SUR LE GRAND BOUTON : l'art du petit bouge
  // déjà assez peu pour que son texte suive sans qu'on le voie.
  const float sink = held && look.press_sinks ? kButtonSinkPixels * scale : 0.0F;
  // ⚠ CENTRÉ SUR L'ART, pas sur l'item : la chute est au-dessus, et le texte
  // la déborde volontairement.
  const ImVec2 where(at.x + (wanted.x - fit.width) * 0.5F,
                     painted.y + (wanted.y - text.y) * 0.5F + look.text_dy * scale + sink);
  // ⚠ LA COULEUR DES BOUTONS, ET NON CELLE DU TEXTE : le libellé est posé sur
  // l'art du bouton, pas sur le corps de la fenêtre.
  const ImU32 color = disabled ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                               : ImGui::GetColorU32(art.look().button_text);
  const auto write = [&](ImVec2 from) {
    into->AddText(ImGui::GetFont(), fit.size, from, color, fit.begin, fit.end);
    if (fit.cut) {
      into->AddText(ImGui::GetFont(), fit.size, ImVec2(from.x + text.x, from.y), color, kLabelCut);
    }
  };
  write(where);
  // ⚠ PAS DE GRAS SUR UN BOUTON GRISÉ : la graisse dit « enfoncé », et un
  // bouton qui ne répond plus ne s'enfonce pas.
  if (held && !disabled) {
    // UN FAUX GRAS : le même texte, décalé d'un pixel. ⚠ L'ART « ENFONCÉ » SEUL
    // SE CONFOND AVEC LE SURVOL sur les petites tailles, et c'est la graisse
    // qui tranche tout de suite — que le bouton soit tenu sous la souris ou
    // qu'il reste choisi. La police n'a qu'une graisse chargée, et en charger
    // une seconde pour un bouton coûterait un atlas entier.
    write(ImVec2(where.x + kFakeBoldPixels, where.y));
  }
  return clicked;
}

[[nodiscard]] float ButtonHeight(const SkinArt& art, const ButtonLook& look) {
  if (!Paintable(art, look.faces)) {
    return ImGui::GetFrameHeight();
  }
  // ⚠ LA PLACE PRISE, ET NON LA HAUTEUR DE L'ART : le petit bouton réserve
  // trois pixels au-dessus du sien pour y laisser dépasser son libellé, et
  // c'est cette hauteur-là qu'un appelant doit réserver.
  return (art.Get(look.faces.resting[0]).height + look.art_drop) * Scale();
}

}  // namespace

float SkinButtonHeight(const SkinArt& art) { return ButtonHeight(art, kBigLook); }

float SkinSmallButtonHeight(const SkinArt& art) { return ButtonHeight(art, kSmallLook); }

float SkinSmallButtonTopCenteredOn(const SkinArt& art, float middle_y) {
  // Sans art, le bouton est celui d'ImGui : il n'a pas de chute, et son item
  // EST son dessin.
  if (!Paintable(art, kSmallLook.faces)) {
    return middle_y - ImGui::GetFrameHeight() * 0.5F;
  }
  const float scale = Scale();
  const float art_height = art.Get(kSmallLook.faces.resting[0]).height * scale;
  // 🔴 ARRONDI AU PIXEL, ET C'EST CE QUI DÉCIDE DE LA NETTETÉ. L'art du jeu est
  // du pixel-art échantillonné au plus proche : posé sur une ordonnée
  // fractionnaire — et le milieu d'une barre de hauteur impaire en est une —, il
  // tombe entre deux texels et ses bordures d'un pixel bavent. Le bouton paraît
  // sale à côté d'un autre dont la place est tombée juste.
  //
  // ⚠ AU PLANCHER : un demi-pixel se perd d'un côté ou de l'autre, et c'est
  // celui du haut qui place l'art comme en jeu.
  return std::floor(middle_y - art_height * 0.5F - kSmallLook.art_drop * scale);
}

float SkinFooterHeight(const SkinArt& art) {
  return SkinButtonHeight(art) + ImGui::GetStyle().ItemSpacing.y;
}

float SkinButtonWidth(const char* label, const SkinArt& art) {
  return AutoButtonWidth(label, art, kBigLook);
}

float SkinSmallButtonWidth(const char* label, const SkinArt& art) {
  return AutoButtonWidth(label, art, kSmallLook);
}

float SkinWidestButton(std::span<const char* const> labels, const SkinArt& art) {
  float widest = 0.0F;
  for (const char* label : labels) {
    widest = std::max(widest, AutoButtonWidth(label, art, kBigLook));
  }
  return widest;
}

bool SkinButton(const char* label, const SkinArt& art, ImVec2 size) {
  return PaintedButton(label, art, kBigLook, size, false);
}

bool SkinSmallButton(const char* label, const SkinArt& art, ImVec2 size) {
  return PaintedButton(label, art, kSmallLook, size, false);
}

bool SkinToggleButton(const char* label, bool active, const SkinArt& art, ImVec2 size) {
  return PaintedButton(label, art, kBigLook, size, active);
}

bool SkinSmallToggleButton(const char* label, bool active, const SkinArt& art, ImVec2 size) {
  return PaintedButton(label, art, kSmallLook, size, active);
}

bool SkinCheckBox(const char* label, bool* ticked, const SkinArt& art) {
  const SkinImage& empty = art.Get(render::SkinPiece::kCheckBoxEmpty);
  const SkinImage& full = art.Get(render::SkinPiece::kCheckBoxTicked);
  if (ticked == nullptr) {
    return false;
  }
  if (!empty.usable() || !full.usable()) {
    return ImGui::Checkbox(label, ticked);
  }

  const float scale = Scale();
  const ImVec2 box(empty.width * scale, empty.height * scale);
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const ImVec2 text = ImGui::CalcTextSize(label, nullptr, true);
  const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
  // GRISÉE COMME LE BOUTON : l'art s'efface, le libellé passe au gris.
  const bool disabled = NextItemDisabled();

  // ⚠ LE LIBELLÉ FAIT PARTIE DE LA CIBLE. Une case de onze pixels est une cible
  // minuscule ; le client, comme ImGui, laisse cliquer le mot à côté.
  const float row = std::max(box.y, text.y);
  const bool clicked =
      ImGui::InvisibleButton(label, ImVec2(box.x + (text.x > 0.0F ? gap + text.x : 0.0F), row));
  if (clicked) {
    *ticked = !*ticked;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }

  ImDrawList* into = ImGui::GetWindowDrawList();
  const float box_top = RowTextTop(at.y, row, box.y);
  into->AddImage(*ticked ? full.id : empty.id, ImVec2(at.x, box_top),
                 ImVec2(at.x + box.x, box_top + box.y), ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                 PaintedTint(art.look().check_tint, disabled));
  if (text.x > 0.0F) {
    into->AddText(ImVec2(at.x + box.x + gap, RowTextTop(at.y, row, text.y)),
                  ImGui::GetColorU32(disabled ? ImGuiCol_TextDisabled : ImGuiCol_Text), label,
                  ImGui::FindRenderedTextEnd(label));
  }
  return clicked;
}

float SkinCheckBoxWidth(const char* label, const SkinArt& art) {
  const SkinImage& empty = art.Get(render::SkinPiece::kCheckBoxEmpty);
  const float text = ImGui::CalcTextSize(label, nullptr, true).x;
  const float box = empty.usable() ? empty.width * Scale() : ImGui::GetFrameHeight();
  return box + (text > 0.0F ? ImGui::GetStyle().ItemInnerSpacing.x + text : 0.0F);
}

bool SkinRadio(const char* label, bool chosen, const SkinArt& art) {
  const SkinImage& off = art.Get(render::SkinPiece::kRadioOff);
  const SkinImage& on = art.Get(render::SkinPiece::kRadioOn);
  if (!off.usable() || !on.usable()) {
    return ImGui::RadioButton(label, chosen);
  }

  const float scale = Scale();
  const ImVec2 button(off.width * scale, off.height * scale);
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const ImVec2 text = ImGui::CalcTextSize(label, nullptr, true);
  const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
  // GRISÉ COMME LA CASE : l'art s'efface, le libellé passe au gris.
  const bool disabled = NextItemDisabled();

  // ⚠ LE LIBELLÉ FAIT PARTIE DE LA CIBLE, comme pour la case à cocher : un rond
  // de onze pixels est une cible minuscule, et le client laisse cliquer le mot.
  const float row = std::max(button.y, text.y);
  const bool clicked =
      ImGui::InvisibleButton(label, ImVec2(button.x + (text.x > 0.0F ? gap + text.x : 0.0F), row));

  ImDrawList* into = ImGui::GetWindowDrawList();
  const float button_top = RowTextTop(at.y, row, button.y);
  into->AddImage(chosen ? on.id : off.id, ImVec2(at.x, button_top),
                 ImVec2(at.x + button.x, button_top + button.y), ImVec2(0.0F, 0.0F),
                 ImVec2(1.0F, 1.0F), PaintedTint(art.look().check_tint, disabled));
  if (text.x > 0.0F) {
    into->AddText(ImVec2(at.x + button.x + gap, RowTextTop(at.y, row, text.y)),
                  ImGui::GetColorU32(disabled ? ImGuiCol_TextDisabled : ImGuiCol_Text), label,
                  ImGui::FindRenderedTextEnd(label));
  }
  return clicked;
}

// Ce qu'une bulle d'aide laisse écrire avant de passer à la ligne, en largeurs
// de caractère.
constexpr float kHelpWrapChars = 35.0F;

void SkinHelpMarker(const char* title, std::span<const char* const> lines) {
  ImGui::TextDisabled("(?)");
  // ⚠ TEMPORISÉ : le marqueur vit dans un pied que la souris traverse pour
  // atteindre autre chose, et une bulle qui saute au passage gêne.
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    return;
  }
  std::string help = core::Tr(title);
  help += '\n';
  for (const char* const line : lines) {
    help += '\n';
    help += core::Tr(line);
  }
  // ⚠ LA BULLE S'ÉLARGIT JUSQU'AU REPLI, marges comprises : bornée plus étroit,
  // elle couperait le texte que le repli laisse passer.
  const float wrap = ImGui::GetFontSize() * kHelpWrapChars;
  if (!BeginSkinTooltip(wrap + 2.0F * ImGui::GetStyle().WindowPadding.x)) {
    return;
  }
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
  ImGui::TextUnformatted(help.c_str());
  ImGui::PopTextWrapPos();
  EndSkinTooltip();
}

void SkinTooltip(const char* text) {
  if (text == nullptr || text[0] == 0 ||
      !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    return;
  }
  SkinTooltipFormat("%s", text);
}

SkinColorPick SkinColorSwatch(const char* label, ImU32* color, const SkinArt& art, bool with_alpha,
                              bool numbers) {
  SkinColorPick told;
  if (color == nullptr) {
    return told;
  }
  ImGui::PushID(label);
  ImVec4 chosen = ImGui::ColorConvertU32ToFloat4(*color);
  const float side = kSwatchSidePixels * Scale();
  // ⚠ L'APERÇU MONTRE LA TRANSPARENCE, damier compris : une pastille peinte
  // opaque mentirait sur la couleur qu'on vient de choisir.
  const ImGuiColorEditFlags button_flags =
      with_alpha ? ImGuiColorEditFlags_AlphaPreview : ImGuiColorEditFlags_NoAlpha;
  if (ImGui::ColorButton("##pastille", chosen, button_flags, ImVec2(side, side))) {
    ImGui::OpenPopup(kSwatchPopup);
  }
  // Le libellé à droite, comme chez ImGui, et sans ce qui suit `##` : c'est
  // l'identité, elle ne se montre pas.
  const std::string_view shown = ShownTitle(label);
  if (!shown.empty()) {
    // Même règle que la case : la pastille est plus HAUTE qu'une ligne, et un
    // libellé laissé en haut de rangée flotterait au-dessus d'elle.
    const float top = ImGui::GetItemRectMin().y;
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetCursorScreenPos().x, RowTextTop(top, side, ImGui::GetTextLineHeight())));
    ImGui::TextUnformatted(shown.data(), shown.data() + shown.size());
  }

  told.open = BeginSkinPopup(kSwatchPopup);
  if (told.open) {
    // `NoInputs` coupe d'un coup les deux rangées de curseurs ET le champ
    // hexadécimal : ImGui les garde sous la même condition. `NoSidePreview`
    // retire l'aperçu latéral, que la pastille juste au-dessus donne déjà.
    const ImGuiColorEditFlags pick_flags =
        ImGuiColorEditFlags_NoSidePreview |
        (with_alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha) |
        (numbers ? 0 : ImGuiColorEditFlags_NoInputs);
    if (ImGui::ColorPicker4("##selecteur", &chosen.x, pick_flags)) {
      *color = ImGui::ColorConvertFloat4ToU32(chosen);
      told.changed = true;
    }
    // ⚠ UN BOUTON POUR REFERMER, même si un clic à côté suffit : le sélecteur
    // recouvre ce qui suit, et « à côté » tombe justement sur ce qu'il cache.
    if (SkinButton(core::Tr("Fermer"), art)) {
      ImGui::CloseCurrentPopup();
    }
    EndSkinPopup();
  }
  ImGui::PopID();
  return told;
}

float RowTextTop(float row_top, float row_height, float text_height) {
  return std::floor(row_top + (row_height - text_height) * 0.5F);
}

void PushCompactStyle() {
  const ImGuiStyle& style = ImGui::GetStyle();
  // ⚠ LE VERTICAL SEUL. Resserrer aussi l'horizontal collerait un libellé à
  // son contrôle, et c'est cet écart-là qui dit lequel commande lequel.
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(style.FramePadding.x, std::floor(style.FramePadding.y * kCompactRatio)));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(style.ItemSpacing.x, std::floor(style.ItemSpacing.y * kCompactRatio)));
}

void PopCompactStyle() { ImGui::PopStyleVar(2); }

void SkinHelpMarker(const char* help) { SkinHelpMarker(help, nullptr); }

void SkinHelpMarker(const char* help, const char* warning) {
  // Une mise en garde sans aide a son marqueur : la bulle la montre seule.
  const bool has_help = help != nullptr && help[0] != '\0';
  const bool has_warning = warning != nullptr && warning[0] != '\0';
  if (!has_help && !has_warning) {
    return;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted("(?)");
  ImGui::PopStyleColor();
  // ⚠ LISIBLE SUR UNE RANGÉE GRISÉE : c'est souvent elle qui dit pourquoi la
  // rangée ne répond pas, et une aide muette là laisserait le joueur sans raison.
  // ⚠ D'OÙ LA BULLE OUVERTE ICI, et non par `SkinTooltip` : celui-ci reteste le
  // survol à sa façon, qui ne voit pas un item grisé.
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipWarned(help, warning);
  }
}

void PaintWindowResizeGrip(const SkinArt& art, float inset_pixels) {
  const SkinImage& grip = art.Get(render::SkinPiece::kResizeGrip);
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  // ⚠ UNE FENÊTRE QUI S'AJUSTE À SON CONTENU NE SE REDIMENSIONNE PAS : ImGui
  // n'y pose aucune zone de saisie, et une poignée y promettrait un geste mort.
  constexpr ImGuiWindowFlags kNotResizable =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
  if (!grip.usable() || window == nullptr || (window->Flags & kNotResizable) != 0) {
    return;
  }
  // 🔴 REPLIÉE, LA FENÊTRE N'EST PLUS QUE SA BARRE, et son coin bas-droit est
  // donc celui de la barre : la poignée venait s'y poser au milieu des boutons
  // système. Elle y promettait un redimensionnement qui n'existe plus — ImGui
  // coupe le sien dès le repli —, en plus de salir le seul morceau qu'il reste
  // à voir.
  if (window->Collapsed) {
    return;
  }
  const float scale = Scale();
  const float inset = inset_pixels * scale;
  const ImVec2 corner(window->Pos.x + window->Size.x - inset,
                      window->Pos.y + window->Size.y - inset);
  ImDrawList* into = window->DrawList;
  into->PushClipRect(window->Pos, ImVec2(corner.x + inset, corner.y + inset), false);
  PaintArt(into, art, grip.id,
           ImVec2(corner.x - grip.width * scale, corner.y - grip.height * scale), corner);
  into->PopClipRect();
}

bool SkinColorSwatch(const char* label, std::array<float, 3>& rgb, const SkinArt& art,
                     bool* opened) {
  // La pastille du client fait vingt pixels de côté, comme celle des fonds de
  // conversation d'où cette forme est reprise.
  constexpr float kSwatchPixels = 20.0F;

  bool changed = false;
  if (opened != nullptr) {
    *opened = false;
  }
  ImGui::PushID(label);
  const float side = kSwatchPixels * Scale();
  const ImVec4 shown(rgb[0], rgb[1], rgb[2], 1.0F);
  if (ImGui::ColorButton("##pastille", shown, ImGuiColorEditFlags_NoAlpha, ImVec2(side, side))) {
    ImGui::OpenPopup("nuancier");
  }
  ImGui::SameLine();
  // Le libellé peut porter un identifiant caché (`##`) : on n'affiche que ce
  // qui précède, comme le fait ImGui pour tous ses widgets. Coupé à la main
  // plutôt qu'avec `FindRenderedTextEnd`, qui vit dans l'en-tête interne — une
  // brique partagée n'a pas à y faire dépendre ses appelants.
  const char* visible_end = label;
  while (*visible_end != '\0' && !(visible_end[0] == '#' && visible_end[1] == '#')) {
    ++visible_end;
  }
  ImGui::TextUnformatted(label, visible_end);

  const bool open = BeginSkinPopup("nuancier");
  if (open) {
    // ⚠ PAS D'ALPHA : l'octet de transparence d'une entrée de palette ne veut
    // rien dire — seul l'index zéro est transparent, et la recoloration le
    // laisse intact. Un curseur qui ne commande rien invite à le bouger pour
    // rien.
    std::array<float, 4> picked = {rgb[0], rgb[1], rgb[2], 1.0F};
    if (ImGui::ColorPicker4("##choix", picked.data(),
                            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha)) {
      rgb = {picked[0], picked[1], picked[2]};
      changed = true;
    }
    if (SkinButton(core::Tr("Fermer"), art)) {
      ImGui::CloseCurrentPopup();
    }
    EndSkinPopup();
  }
  if (opened != nullptr) {
    *opened = open;
  }
  ImGui::PopID();
  return changed;
}

bool SkinHyperlink(const char* label) {
  // La couleur des liens de l'habillage, que la palette de la fenêtre pose.
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextLink));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  if (!ImGui::IsItemHovered()) {
    return false;
  }
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  // ⚠ LE TRAIT SE POSE SUR LE BAS DU RECTANGLE DE L'ITEM, et non à une
  // distance choisie sous la ligne de base : la hauteur d'une ligne suit
  // l'échelle de l'interface, un écart en dur ne la suivrait pas.
  const ImVec2 corner_min = ImGui::GetItemRectMin();
  const ImVec2 corner_max = ImGui::GetItemRectMax();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(corner_min.x, corner_max.y),
                                      ImVec2(corner_max.x, corner_max.y),
                                      ImGui::GetColorU32(ImGuiCol_TextLink));
  return ImGui::IsItemClicked();
}

}  // namespace ro::ui
