#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE RUBAN TYPÉ : le ruban de traînée peint DEUX FOIS, la seconde selon un type.
//
// C'est le dessinateur du sous-type 149, et il sort par la voie B, deux fois
// par segment. Dix identifiants l'emploient, tous bâtis par le même bâtisseur :
// ce sont les bulles et le feu d'incantation.
//
// Il partage avec `ComposeEffectTrackRibbon` tout le balayage : ONZE POINTS, pas
// angulaire égal au balayage divisé par dix. Ce qui l'en sépare :
//
//   - SON RAYON DE BASE N'EST PAS LE RAYON À L'ANCRE. Le client y AJOUTE le
//     sinus de l'azimut de la piste, ce qui échelonne les quatre rubans d'un
//     nœud au lieu de les rendre concentriques ;
//   - IL PEINT CHAQUE SEGMENT DEUX FOIS : une passe à la variante de palette
//     ZÉRO et à l'alpha de la piste, puis une passe dont la variante ET le
//     multiplicateur d'alpha viennent du type de la piste ;
//   - SON IMAGE TOURNE AVEC LE TEMPS, et le type peut ralentir ce tour.
//
// 🔴 VINGT SEGMENTS, ET VINGT ET UN POINTS : il divise son balayage par VINGT,
// plafonne sa case à vingt et referme le tour sur elle. L'onde se lit donc sur
// les vingt et une cases de la piste, et la coordonnée de texture, que la
// primitive de la voie B compte sur vingt et un, en parcourt toute la largeur.
// Ce n'est pas le ruban de `ComposeEffectTrackRibbon`, qui s'arrête à dix.

// LE NOMBRE DE SEGMENTS, et la division dont la primitive tire sa coordonnée.
inline constexpr std::int32_t kTypedRibbonLastSegment = 20;
inline constexpr float kTypedRibbonTextureSpan = 21.0F;

// LE PLAFOND DE L'ALPHA MULTIPLIÉ. Le client écrête à deux cent cinquante-cinq
// après avoir multiplié, jamais avant.
inline constexpr std::int16_t kTypedRibbonAlphaCeiling = 255;

// DE COMBIEN LE TYPE DIX RALENTIT le tour de l'image. Un seul type le fait.
inline constexpr std::int32_t kTypedRibbonSlowFactor = 3;

// CE QU'UN TYPE DONNE À LA SECONDE PASSE.
struct EffectTypedRibbonPass {
  // La variante de la palette de traînée — le champ que le client écrit juste
  // avant d'appeler sa primitive. Voir `effect_trail_palette.h`.
  std::uint8_t trail_variant = 0;
  // Par combien l'alpha de la piste est multiplié, avant écrêtage.
  std::int16_t alpha_factor = 1;
  // ⚠ DEUX TYPES CONSULTENT UN RÉGLAGE DU JOUEUR que nous ne portons pas. Quand
  // il est FAUX, leur alpha double ; quand il est vrai, il reste tel quel. Le
  // champ dit que la question se pose, au lieu de figer une réponse — et
  // l'appelant tranche.
  bool doubles_when_setting_is_off = false;
  // Le type dix, et lui seul, divise les tics avant de choisir son image.
  bool slows_the_image = false;
};

// Ce que le type d'une piste demande à sa seconde passe. Tout type hors des
// douze connus rend celui du type zéro.
[[nodiscard]] EffectTypedRibbonPass EffectTypedRibbonPassFor(std::uint8_t type);

// LES QUADS D'UNE PISTE : deux par segment, la première passe puis la seconde.
//
// `image_count` est le nombre d'images que le nœud porte — le client en tire son
// index par un modulo, et zéro le ferait diviser par zéro. `setting_is_on` est le
// réglage que deux types consultent.
//
// Rend un lot VIDE quand la piste est éteinte, quand son balayage est négatif ou
// quand son alpha est nul. ⚠ ET C'EST LE CAS AU PREMIER TIC : le bâtisseur sème
// l'alpha à zéro, et le mouvement le monte de trois par tic.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeTypedRibbonQuads(const EffectTrack& track,
                                                                   glm::vec3 anchor,
                                                                   std::size_t image_count,
                                                                   bool setting_is_on = false);

}  // namespace ro::render
