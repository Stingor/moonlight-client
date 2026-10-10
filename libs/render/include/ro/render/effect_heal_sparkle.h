#pragma once

#include <array>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉTINCELLE DU SOIN : le nœud du sous-type 58, et il n'y en a qu'un.
//
// Un seul bâtisseur le monte — celui de l'étincelle du soin — et un
// seul effet l'emploie, EF_FIRSTAID (309). C'est pourquoi cette famille n'a ni
// crible ni variantes : elle a UNE entrée, et son nom dit laquelle.
//
// ⚠ MÊME FIGURE QUE `kTintedQuads`, AUTRE MOUVEMENT, et c'est pourquoi c'est une
// forme à part. La forme d'un nœud lie sa figure ET son mouvement ; les ranger
// ensemble ferait défiler en cascade ce qui doit paraître sur place, et le
// dessinateur partagé ne le signalerait pas.
//
// Son mouvement est le cas 58 du répartiteur de mouvement.

// PENDANT COMBIEN DE TICS L'ALPHA MONTE, et de combien par tic. Vingt tics à
// quatre : l'étincelle atteint quatre-vingts, et non la pleine opacité.
inline constexpr std::int32_t kHealSparkleLastTickOfRise = 20;
inline constexpr std::int16_t kHealSparkleAlphaRise = 4;
// JUSQU'À QUEL TIC ELLE TIENT avant de s'effacer, et de combien par tic. Le
// palier dure quatre fois plus longtemps que la montée.
inline constexpr std::int32_t kHealSparkleLastTickOfHold = 100;
inline constexpr std::int16_t kHealSparkleAlphaFall = 1;
// DE COMBIEN L'ANGLE PROPRE TOURNE PAR TIC. C'est lui, et lui seul, qui fait
// respirer le rayon de la figure : le mouvement ne touche à rien d'autre.
inline constexpr float kHealSparkleSpinPerTick = 3.0F;

// 🔴 DE COMBIEN L'ÉTINCELLE S'ÉLÈVE AU-DESSUS DE SON PORTEUR, et le nom compte.
//
// Le bâtisseur écrit `ancre_y = instance.y - 12.0` — et le repère
// du client DESCEND, si bien qu'une SOUSTRACTION y élève. Recopier le signe au
// lieu du sens enfoncerait l'étincelle de douze unités sous les pieds.
//
// ⚠ LE BÂTISSEUR Y RANGE LA POSITION ENTIÈRE DE L'INSTANCE, et la figure n'y
// ajoute rien. Ce nombre est l'écart que le semis ajoute à la position du
// porteur pour faire l'ancre : douze vers le haut, exactement. C'est ce qui la
// fait paraître à hauteur de poitrine et non aux pieds.
inline constexpr float kHealSparkleLiftAboveOwner = 12.0F;

// Avance l'étincelle d'un tic, et range les deux angles de caméra.
//
// ⚠ TOUT SE PASSE SUR LA PISTE ZÉRO : le client écrit ces champs à des places
// ABSOLUES, que sa boucle des quatre pistes ne décale pas. Une boucle sur les quatre ferait
// paraître quatre étincelles là où il n'y en a qu'une. C'est le même piège que la marque du carré
// teinté — voir `effect_tinted_quad.h`.
//
// ⚠ ET LES DEUX ANGLES DE CAMÉRA VIVENT DANS DES CASES DE STYLE, celles des
// pistes zéro et un. Ce ne sont pas des styles, et aucune piste ne les lit comme
// tels : c'est le dessinateur qui va les y relire. ADR 0017.
void AdvanceHealSparkleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees);

}  // namespace ro::render
