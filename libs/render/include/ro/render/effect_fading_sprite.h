#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/render/effect_node_pool_fwd.h"

namespace ro::render {

// LE MOUVEMENT DU SOUS-TYPE 85 : un sprite qui S'ÉLÈVE en s'allumant, puis
// s'efface.
//
// C'est l'ange d'une montée de niveau, ses ailes et ses plumes : `EF_ANGEL2` pour
// le super apprenti, `EF_ANGEL3` pour la lignée taekwon.
//
// ⚠ CE MOUVEMENT NE BOUGE PAS DES PISTES, seul de ceux que nous portons. Il mène
// l'ALPHA du nœud, son INDEX D'IMAGE et sa POSITION ; la piste ne lui sert que
// d'ancre. Le chercher dans les pistes ferait conclure qu'il ne fait rien.
//
// ⚠ NOTRE AXE VERTICAL MONTE : s'élever, c'est AJOUTER à l'ordonnée. Le bâtisseur
// pose l'ancre au-dessus de l'effet, et ce mouvement l'élève encore d'un dixième
// par tic. Retrancher enterrerait l'ange, et l'épreuve de profondeur le cacherait
// tout entier sans qu'aucun compte de nœuds ou de quads ne change.

// DE COMBIEN L'ANCRE S'ÉLÈVE À CHAQUE TIC.
inline constexpr float kFadingSpriteRisePerTick = 0.1F;

// À PARTIR DE CE TIC, plus aucun style ne monte : le corps ne fait plus que
// tenir, puis s'éteindre.
inline constexpr std::int32_t kFadingSpriteRiseEndTick = 50;

// LES MARQUES QUE CETTE FAMILLE EMPLOIE : le STYLE, qui choisit la rampe, et le
// TIC DE NAISSANCE, qui plafonne l'alpha des nœuds resemés.
inline constexpr std::size_t kFadingSpriteMarkOfStyle = 0;
inline constexpr std::size_t kFadingSpriteMarkOfBirthTick = 1;

// LES SEPT STYLES que le bâtisseur pose, nommés par ce qu'ils montrent.
inline constexpr std::uint8_t kFadingSpriteBody = 0;
inline constexpr std::uint8_t kFadingSpriteWings = 1;
inline constexpr std::uint8_t kFadingSpriteFirstFeather = 2;
inline constexpr std::uint8_t kFadingSpriteSecondFeather = 3;
inline constexpr std::uint8_t kFadingSpriteThirdFeather = 4;
inline constexpr std::uint8_t kFadingSpriteHanbokBody = 10;
inline constexpr std::uint8_t kFadingSpriteHanbokWings = 11;

// LES RAMPES DE MONTÉE, par style. Chacune est un pas et un plafond.
inline constexpr float kFadingSpriteRiseStep = 5.0F;
inline constexpr float kFadingSpriteBodyCeiling = 245.0F;
inline constexpr float kFadingSpriteWingsCeiling = 200.0F;
inline constexpr float kFadingSpriteFeatherCeiling = 150.0F;
inline constexpr float kFadingSpriteHanbokRiseStep = 10.0F;
inline constexpr float kFadingSpriteHanbokCeiling = 255.0F;
// ⚠ UN NŒUD RESEMÉ MONTE PLUS BAS QUE LE PRÉCÉDENT : son plafond vaut quarante
// MOINS son tic de naissance, et il n'avance que d'un par tic. C'est ce qui fait
// la traînée du battement d'ailes — la dernière aile est deux fois plus pâle que
// la première.
inline constexpr float kFadingSpriteRebornCeiling = 40.0F;
inline constexpr float kFadingSpriteRebornStep = 1.0F;
// Au-delà de ce tic, la plume cesse de monter même si son plafond n'est pas
// atteint.
inline constexpr std::int32_t kFadingSpriteFeatherRiseLastTick = 30;

// LES DESCENTES : à partir de quel tic, et de combien par tic.
inline constexpr std::int32_t kFadingSpriteBodyFallTick = 100;
inline constexpr float kFadingSpriteBodyFallStep = 4.0F;
inline constexpr std::int32_t kFadingSpriteFirstFeatherFallTick = 30;
inline constexpr std::int32_t kFadingSpriteSecondFeatherFallTick = 60;
inline constexpr std::int32_t kFadingSpriteThirdFeatherFallTick = 70;
inline constexpr std::int32_t kFadingSpriteOtherFallTick = 75;
inline constexpr float kFadingSpriteSlowFallStep = 3.0F;
inline constexpr float kFadingSpriteFastFallStep = 6.0F;

// L'INDEX D'IMAGE : il n'avance qu'APRÈS le trentième tic, d'un cran tous les
// trois tics, et il s'arrête à vingt-neuf. Le hanbok avance d'un tous les quatre
// et s'arrête à quinze.
//
// ⚠ LE BORNER EST CE QUI L'EMPÊCHE DE REBOUCLER : sans plafond, l'animation
// repartirait à zéro et l'ange battrait des ailes indéfiniment.
inline constexpr std::int32_t kFadingSpriteFrameFirstTick = 30;
inline constexpr std::int32_t kFadingSpriteFrameEveryTicks = 3;
inline constexpr std::int32_t kFadingSpriteLastFrame = 29;
inline constexpr std::int32_t kFadingSpriteHanbokFrameEveryTicks = 4;
inline constexpr std::int32_t kFadingSpriteHanbokLastFrame = 15;

// UN TIC DU NŒUD : son alpha, son image et sa position. Il n'a qu'une seule trace :
// le sprite se peint une fois, au point du nœud.
void AdvanceFadingSpriteNode(PooledEffectNode& pooled);

}  // namespace ro::render
