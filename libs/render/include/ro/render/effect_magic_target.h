#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA GRILLE DE VISÉE, sous-type 112 : sept cases sur sept, peintes À MÊME LE
// TERRAIN autour du nœud, et dont l'image TOURNE et RESPIRE.
//
// Le dessinateur et le mouvement du sous-type 112. Un seul identifiant
// l'emploie, EF_GROUNDSAMPLE.
//
// ⚠ CE DESSINATEUR N'ÉMET AUCUN QUAD, et c'est ce qui le sépare de toutes les
// figures portées jusqu'ici. Les autres bâtissent des sommets ; celui-ci
// peint à même le terrain — comme le curseur au sol et la trace de
// navigation — une fois par cellule et par
// passe. La géométrie n'est donc pas à lui : elle est celle du TERRAIN, et
// c'est le fichier de carte qui la donne, cellule par cellule.
//
// ⚠ D'OÙ SA SORTIE : des CASES, et non des quads. La figure ne connaît ni le
// .gnd ni le .gat, et ne saurait pas poser un coin — elle rend de quoi PEINDRE
// une case dont un autre tient le relief. C'est la même coupure que pour le
// billboard, qui rend des pixels autour d'une ancre qu'il ne sait pas projeter.
//
// ⚠ ET RIEN ICI NE LIT L'ALPHA DE LA PISTE. Les deux teintes sont écrites en
// dur dans le dessinateur, et la grille garde donc la même opacité du premier
// tic au dernier : elle ne s'allume pas et ne s'éteint pas, elle apparaît et
// disparaît avec son nœud. Lui prêter l'alpha de sa piste — que le bâtisseur
// laisse à zéro — la rendrait invisible.

// L'EFFET QUI PORTE CETTE GRILLE : EF_GROUNDSAMPLE.
//
// 🔴 C'EST LE CERCLE D'INCANTATION D'UN SORT LANCÉ AU SOL, celui qui se pose
// sur la case visée pendant qu'on prépare un Storm Gust. Il ne se confond pas
// avec l'aura élémentaire, qui naît sous le LANCEUR et dit l'élément du sort.
inline constexpr std::uint32_t kGroundSampleEffect = 513;

// CE QU'IL FAUT POUR PEINDRE UNE CASE : laquelle, avec quelle portion d'image
// et sous quelle teinte.
//
// La case est donnée en ÉCART à celle du nœud, parce que la figure ignore où le
// nœud se trouve sur la grille : c'est celui qui tient la carte qui l'y ramène.
struct EffectGroundCellPaint {
  std::int32_t cell_offset_x = 0;
  std::int32_t cell_offset_y = 0;
  // Les quatre coins, dans l'ordre de `GroundQuad` : abscisse de cellule
  // croissante d'abord, ordonnée de cellule ensuite.
  std::array<glm::vec2, 4> texture_uv = {};
  // En RGBA, comme `GroundQuad::color`. Le client l'écrit en ARGB.
  std::array<std::uint8_t, 4> color = {255, 255, 255, 255};
};

// LE CÔTÉ DU BLOC, en cases, et le demi-côté d'où les écarts partent.
//
// ⚠ SEPT EST IMPAIR, ET LE CENTRE N'EST DONC PAS UN BORD DE CASE : le client
// retranche trois à la case du nœud puis compte de zéro à six, ce qui met le
// nœud dans la case du milieu ; mais ses coordonnées de texture se comptent
// depuis TROIS ET DEMI, c'est-à-dire depuis le CENTRE de cette case. Les deux
// demi-côtés ne sont pas le même nombre, et les confondre décalerait l'image
// d'une demi-case.
inline constexpr std::int32_t kMagicTargetGridSide = 7;
inline constexpr std::int32_t kMagicTargetGridFirstCell = -(kMagicTargetGridSide / 2);
inline constexpr float kMagicTargetGridCenter = static_cast<float>(kMagicTargetGridSide) / 2.0F;

// LES DEUX TEINTES, en RGBA. Le client écrit 0xAA9999FF et 0x559999FF en ARGB :
// le même bleu lavande, à deux opacités.
inline constexpr std::array<std::uint8_t, 4> kMagicTargetNearColor = {0x99, 0x99, 0xFF, 0xAA};
inline constexpr std::array<std::uint8_t, 4> kMagicTargetFarColor = {0x99, 0x99, 0xFF, 0x55};

// CE QUE LA SECONDE PASSE AJOUTE À L'ÉCHELLE.
//
// ⚠ UNE ÉCHELLE PLUS GRANDE MONTRE UNE IMAGE PLUS PETITE : l'échelle multiplie
// les COORDONNÉES DE TEXTURE, si bien que le bloc de sept cases couvre une plus
// grande part de la planche. La seconde passe est donc un second cercle un peu
// plus petit et deux fois plus pâle, posé sur le premier.
inline constexpr float kMagicTargetSecondPassScale = 0.1F;

// LES DEUX PARTS DE L'ÉCHELLE que le mouvement fait battre : un socle, et la
// moitié d'un sinus qui tourne d'un degré par tic.
//
// L'échelle va donc de un à deux, et le cercle de visée passe de toute la
// grille à la moitié de la grille, une fois par six secondes.
inline constexpr float kMagicTargetScaleFloor = 1.5F;
inline constexpr float kMagicTargetScaleSwing = 0.5F;

// LES CASES À PEINDRE pour une piste : quarante-neuf cellules, chacune DEUX
// FOIS — la passe franche, puis la passe pâle.
//
// Rend un lot VIDE quand la piste est éteinte.
//
// ⚠ L'ORDRE EST CELUI DU CLIENT, et il compte : les deux passes d'une même case
// occupent le même plan, et c'est la seconde peinte qui se voit par-dessus.
[[nodiscard]] std::vector<EffectGroundCellPaint> ComposeMagicTargetGridCells(
    const EffectTrack& track);

// LE MOUVEMENT DU SOUS-TYPE 112 : l'image tourne d'un degré par tic, et son
// échelle suit le sinus du même angle.
//
// ⚠ C'EST TOUT CE QUE FAIT LE CAS. Ni alpha, ni durée, ni déplacement : la
// grille vit tant que son nœud vit, et le nœud tient sa vie de l'effet. Lui
// chercher une extinction serait en inventer une.
//
// ⚠ ET IL NE SUIT PAS SON PORTEUR. Le bâtisseur recopie la position une fois,
// au semis ; la marque reste où elle a été posée.
void AdvanceMagicTargetGridTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
