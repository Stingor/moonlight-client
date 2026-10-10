#pragma once

#include <array>
#include <cstddef>

#include <imgui.h>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_look.h"

namespace ro::ui {

// L'HABILLAGE, TEL QUE L'INTERFACE LE VOIT : des identifiants et des tailles.
//
// ⚠ PAS UNE TEXTURE, PAS UN OCTET D'OPENGL. `ui` ne connaît pas la carte
// graphique ; c'est `app` qui remplit cette table depuis `SkinTextures`. Même
// frontière que la `FrameScene` du monde, et elle achète la même chose : ces
// découpages se vérifient dans un essai, sans fenêtre et sans pilote.

// Une pièce prête à peindre. Une pièce absente garde une taille nulle.
struct SkinImage {
  ImTextureID id = 0;
  float width = 0.0F;
  float height = 0.0F;
  // LA MÊME PIÈCE RETOURNÉE POUR UN PAPIER SOMBRE, quand elle est peinte sur du
  // blanc ; nulle sinon. Voir `PaintOnPaper`.
  ImTextureID dark_paper_id = 0;

  [[nodiscard]] bool usable() const noexcept { return width > 0.0F && height > 0.0F; }
};

class SkinArt {
 public:
  void Set(render::SkinPiece piece, SkinImage image) {
    images_[static_cast<std::size_t>(piece)] = image;
  }
  [[nodiscard]] const SkinImage& Get(render::SkinPiece piece) const {
    return images_[static_cast<std::size_t>(piece)];
  }

  // Vrai quand la BARRE DE TITRE est là en entier : sans elle, l'habillage se
  // tait et l'interface garde son aspect ordinaire. Il n'y a pas de demi-barre à
  // peindre — deux morceaux sur trois seraient pires que pas de barre.
  //
  // ⚠ CE N'EST PAS LE CADRE DU CORPS, et il n'y en a pas : une fenêtre du jeu a
  // un corps d'un seul aplat bordé d'un filet. Voir `ro/ui/skin_palette.h`.
  [[nodiscard]] bool dresses_windows() const;

  // L'HABILLAGE QUE LE JOUEUR RÈGLE, posé par `app` à chaque image : il change
  // en jeu, sans recharger une seule pièce.
  void SetLook(const SkinLook& look) { look_ = look; }
  [[nodiscard]] const SkinLook& look() const noexcept { return look_; }

 private:
  std::array<SkinImage, render::kSkinPieceCount> images_;
  SkinLook look_;
};

// PEINT UNE PIÈCE DE LA FENÊTRE — barre de titre et ses boutons, ascenseur, pied
// —, sous la teinte et la luminosité que le joueur a réglées.
void PaintArt(ImDrawList* into, const SkinArt& art, ImTextureID id, ImVec2 top_left,
              ImVec2 bottom_right);

// PEINT UNE PIÈCE DE CADRE `sysbox` À LA COULEUR DU FOND QU'ELLE BORDE — le
// papier d'une description, le corps d'une boîte : son blanc devient ce fond,
// son filet s'en assombrit d'autant, et le cadre se fond dans ce qu'il entoure,
// quel que soit le thème. Sous l'opacité de la fenêtre, comme toute pièce.
//
// ⚠ PAS SOUS LA TEINTE DES IMAGES : elle colore le chrome des fenêtres, et
// appliquée ici elle cernait un papier sombre d'une bande claire.
void PaintFrameOn(ImDrawList* into, ImTextureID id, ImVec2 top_left, ImVec2 bottom_right,
                  ImU32 fill);

// LA TEINTE QUI PEINT UNE IMAGE DE L'HABILLAGE, prête pour la liste de dessin :
// la teinte réglée, grisée si l'item l'est, et l'opacité de la fenêtre.
//
// 🔴 `AddImage` N'APPLIQUE PAS L'OPACITÉ DU STYLE, contrairement aux widgets
// d'ImGui : sans ceci, la fenêtre devient translucide et ses images restent
// opaques par-dessus.
//
// ⚠ GRISÉE, L'IMAGE PREND `kSkinDisabledArtTint` ET NON L'EFFACEMENT DU
// GRISAGE : sous `BeginDisabled`, l'alpha du style porte les deux, et
// appliquer les deux effacerait l'art deux fois.
[[nodiscard]] ImU32 PaintedTint(ImU32 tint, bool disabled = false);

// VRAI QUAND LE PAPIER EST SOMBRE : le blanc s'y lit mieux que le noir.
[[nodiscard]] bool PaperIsDark(ImU32 paper);

// UNE COULEUR CHOISIE POUR DU BLANC, rendue pour ce papier : multipliée par un
// papier clair — le blanc la rend telle —, et sur un sombre, son encre retournée
// posée sur lui (voir `core::InkForDarkPaper`). L'opacité reste la sienne.
//
// ⚠ UN TEXTE PASSE ENSUITE PAR `ReadableOn` : la traduction garde une teinte,
// elle ne promet pas un contraste.
[[nodiscard]] ImU32 OnPaper(ImU32 color, ImU32 paper);

// UNE COULEUR CHOISIE POUR UN FOND CLAIR, posée sur ce fond : telle sur un fond
// clair, son encre retournée sur un sombre (voir `OnPaper`).
[[nodiscard]] ImU32 OnBackground(ImU32 color, ImU32 background);

// La même pour un texte, gardé lisible.
[[nodiscard]] ImU32 InkOn(ImU32 color, ImU32 background);

// UNE PIÈCE PEINTE SUR DU BLANC, posée sur ce papier que l'appelant a déjà
// étendu dessous : multipliée par un papier clair, et sur un sombre sa variante
// retournée, dont le blanc est transparent — faute d'elle, multipliée aussi.
// Sous l'opacité de la fenêtre, comme toute pièce. `uv_min` et `uv_max` en
// prennent une part.
void PaintOnPaper(ImDrawList* into, const SkinImage& image, ImVec2 top_left, ImVec2 bottom_right,
                  ImU32 paper, ImVec2 uv_min = ImVec2(0.0F, 0.0F),
                  ImVec2 uv_max = ImVec2(1.0F, 1.0F));

// LE DÉCOUPAGE EN TROIS, horizontalement : deux bouts à leur taille, un milieu
// étiré entre eux. C'est la forme d'une barre de titre et d'un bouton.
//
// Les trois rectangles sortent dans l'ordre gauche, milieu, droite. Le milieu
// peut être VIDE quand la place manque : deux bouts qui se chevauchent valent
// mieux qu'un milieu à l'envers.
struct StripRects {
  ImVec4 left;
  ImVec4 middle;
  ImVec4 right;
};
[[nodiscard]] StripRects HorizontalStrip(ImVec2 top_left, ImVec2 bottom_right, float left_width,
                                         float right_width);

// LE DÉCOUPAGE EN NEUF : quatre coins à leur taille, quatre bords étirés le long
// des côtés, et un centre. C'est la forme d'un cadre de fenêtre.
//
// ⚠ L'ORDRE EST CELUI DE LA LECTURE : haut-gauche, haut, haut-droite, gauche,
// centre, droite, bas-gauche, bas, bas-droite. Le centre y figure, même si le
// cadre de RO ne le peint pas — c'est là que va le fond.
using NineSliceRects = std::array<ImVec4, 9>;
[[nodiscard]] NineSliceRects NineSlice(ImVec2 top_left, ImVec2 bottom_right, ImVec2 corner);

}  // namespace ro::ui
