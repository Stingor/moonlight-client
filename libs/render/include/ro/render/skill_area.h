#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/effect_magic_target.h"

namespace ro::render {

// LA ZONE D'UN SORT, peinte au sol case par case.
//
// Elle paraît à DEUX moments, qui n'ont rien en commun sous le capot :
//
//   · le sort est ARMÉ et attend son clic. Aucun paquet n'existe alors : c'est
//     entièrement local, personne d'autre ne voit ce qu'on vise ;
//   · l'INCANTATION a commencé, et le serveur l'a annoncée par `ZC_SKILL_SCALE`.
//     Tout le monde autour voit la même chose.
//
// La TAILLE vient de la même source dans les deux cas —
// `SKILL_INFO_LIST[id].SkillScale[rang]`, voir `sim::SkillScaleAt` — pour que
// la prévisualisation et la zone annoncée ne puissent pas se contredire.
//
// ⚠ ELLE EST PUREMENT COSMÉTIQUE. C'est ce que le client DESSINE, pas ce que le
// serveur TOUCHE : rien ne garantit que les deux coïncident, et une zone plus
// petite que les dégâts serait un mensonge tranquille.
//
// ⚠ ET LE CLIENT NE LA DESSINE PAS AINSI. Il pose une instance de l'effet 1114
// à qui il ajoute un point par case, chacun portant
// `data\texture\warning_plane.tga` en rouge à cent cinquante d'alpha, à deux
// fois et demie l'échelle. Nous peignons des CASES, ce qui donne
// le motif et la couleur en réglage — et ce que le natif ne sait pas faire :
// la zone du sort qu'on tient encore en main.

// Le dessin d'une case.
//
// ⚠ DEUX DES TROIS SONT DES TEXTURES QUE LE JOUEUR POSSÈDE DÉJÀ, et c'est
// voulu : aucun asset n'est versionné ici. La troisième n'est pas un fichier
// mais un aplat, que le rendu bâtit.
//
// 🔴 EN `int`, PARCE QU'IL SE PERSISTE. Ce rang part dans le fichier de
// réglages par un `reinterpret_cast<int*>`, et la lecture écrit QUATRE octets :
// sur un `std::uint8_t`, elle débordait sur le joint qui suit dans la structure.
enum class SkillAreaPattern : int {
  // Les quatre équerres du marqueur de destination : la case est signalée par
  // ses coins, son milieu reste nu.
  kCorners = 0,
  // Le cadre carré que le client emploie pour ses propres zones.
  kFrame,
  // Le carreau plein, et son joint. Voir `SkillAreaLook::gap_percent`.
  kSolid,
};

// PAR DÉFAUT, LE BLANC À MOITIÉ TRANSPARENT : une zone se lit par sa
// FORME, et une couleur franche la ferait passer pour un effet du jeu.
inline constexpr std::uint32_t kDefaultSkillAreaColor = 0x80FFFFFFU;

// Les quatre octets d'une couleur empaquetée, dans l'ordre de `GroundQuad`.
[[nodiscard]] std::array<std::uint8_t, 4> SkillAreaColorBytes(std::uint32_t color) noexcept;

// Ce que le joueur règle.
struct SkillAreaLook {
  // 🔴 LA ZONE ANNONCÉE EST MONTRÉE PAR DÉFAUT, et celle du sort armé non.
  //
  // La première est ce que le client montre nativement — l'éteindre serait un
  // retrait. La seconde est une aide au visage découvert, que le natif n'a
  // jamais eue : celui qui la veut la demande.
  bool cast_shown = true;
  bool armed_shown = false;
  SkillAreaPattern pattern = SkillAreaPattern::kFrame;
  // Le joint, en pourcentage du côté de la case. N'a de sens que sur le carreau
  // plein : creuser un cadre ou des équerres ne ferait que les rapetisser.
  float gap_percent = 12.0F;
  // ⚠ UN SEUL ENTIER, ET NON QUATRE OCTETS : le fichier de réglages ne sait
  // déclarer que des types simples, et garder les deux formes côte à côte
  // ferait deux exemplaires à tenir d'accord. L'ordre est celui du nuancier —
  // alpha, bleu, vert, rouge du poids fort au faible —, ce qui n'engage rien :
  // c'est un entier, pas un type d'ImGui.
  std::uint32_t color = kDefaultSkillAreaColor;
};

// Les bornes d'un réglage, tenues ici parce que c'est ici qu'elles veulent dire
// quelque chose. Le joint se borne AU DESSIN et non à la saisie : un fichier de
// configuration écrit à la main n'est pas passé par le curseur.
inline constexpr float kSmallestSkillAreaGapPercent = 0.0F;
inline constexpr float kLargestSkillAreaGapPercent = 80.0F;

// La part de la case que le carreau occupe, joint déduit. Un pour les deux
// motifs qui dessinent un contour : les creuser les rapetisserait au lieu de
// les espacer.
[[nodiscard]] float SkillAreaFill(const SkillAreaLook& look) noexcept;

// La texture d'un motif. ⚠ TOUJOURS UN NOM, JAMAIS VIDE : le carreau plein n'a
// pas de fichier, mais un lot sans image est SAUTÉ par le rendu — il se serait
// donc contenté de ne rien peindre, et le réglage aurait paru cassé. Son nom ne
// désigne aucune archive : c'est une clé de cache, que `SkillAreaIsPainted`
// reconnaît pour bâtir l'aplat au lieu de le lire.
[[nodiscard]] std::string_view SkillAreaTexturePath(SkillAreaPattern pattern) noexcept;

// Vrai pour le motif dont l'image se BÂTIT au lieu de se lire.
[[nodiscard]] bool SkillAreaIsPainted(std::string_view texture_path) noexcept;

// ── LE CARREAU PLEIN, DESSINÉ ──────────────────────────────────────────────
//
// Son côté en texels. ⚠ IL NE DIT RIEN DE LA TAILLE À L'ÉCRAN : une case fait
// ce que la caméra en fait, et l'image s'y étire. Il dit la FINESSE — il faut
// de la place pour que le fondu du bord ait des paliers plutôt qu'une marche.
inline constexpr std::int32_t kSkillAreaTilePixels = 48;
// LE RAYON DES COINS, en texels.
//
// 🔴 UN APLAT UNI NE SE LIT PAS COMME UN CARRELAGE : neuf cases voisines y font
// une seule tache, et l'on ne voit plus ni la grille ni où le sort s'arrête. Ce
// sont les coins et le liseré qui redonnent chaque case.
inline constexpr float kSkillAreaTileCornerRadius = 8.0F;
// SUR COMBIEN DE TEXELS LE BORD SE FOND. Zéro rendrait une arête franche, que
// l'agrandissement d'une case transforme en escalier.
inline constexpr float kSkillAreaTileEdgeSoftness = 3.0F;
// L'épaisseur du liseré, mesurée VERS L'INTÉRIEUR depuis le bord : il y est
// plein, et retombe sur l'intérieur au bout de cette distance.
inline constexpr float kSkillAreaTileEdgeWidth = 5.0F;
// Ce que l'intérieur garde de l'opacité du liseré. Le réglage de couleur
// multiplie ensuite le tout : c'est un RAPPORT, pas une opacité finale.
inline constexpr std::uint8_t kSkillAreaTileInnerAlpha = 110;

// L'image du carreau plein. Blanche — c'est la teinte du quad qui donne la
// couleur —, et c'est l'ALPHA seul qui dessine.
[[nodiscard]] formats::Image ComposeSkillAreaTile();

// LES CASES D'UNE ZONE de `width_cells` sur `height_cells`, en écarts à la case
// CENTRALE.
//
// Le centrage est celui du client, division entière comprise :
// `cx + i - largeur / 2`. Une largeur paire penche donc d'un côté, exactement
// comme chez lui.
//
// Vide quand l'un des deux côtés est nul — c'est le cas de la grande majorité
// des compétences, qui ne déclarent aucune zone.
[[nodiscard]] std::vector<EffectGroundCellPaint> ComposeSkillAreaCells(const SkillAreaLook& look,
                                                                       std::uint16_t width_cells,
                                                                       std::uint16_t height_cells);

}  // namespace ro::render
