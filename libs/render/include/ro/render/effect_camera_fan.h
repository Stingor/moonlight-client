#pragma once

#include <array>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE MOUVEMENT DU SOUS-TYPE 68 : l'éventail qui S'OUVRE PUIS SE DÉROBE.
//
// C'est le mouvement que le cas 68 du répartiteur de mouvement appelle, celui de
// l'Asura Strike.
//
// ⚠ CE N'EST PAS LE MOUVEMENT DU SOUS-TYPE 147, qui partage pourtant sa FIGURE —
// les deux sous-types tombent sur le même bloc du dessin, donc sur le
// dessinateur d'éventail en spirale. Ce qui les sépare tient en quatre
// constantes et un seuil :
//
//   | | 147 | 68 |
//   |---|---|---|
//   | montée d'une lame | +10 par tic | **+13** |
//   | chute des neuf | −4 par tic | **−5** |
//   | la dixième lame | styles 6 et 7, après 90 tics | **après 140 tics** |
//   | resserrement du rayon | −0,2 par tic | **−0,1** |
//
// Leur donner le même mouvement ferait s'ouvrir l'Asura au rythme d'un compteur
// de rotation — et c'est exactement le genre d'erreur que la forme empêche.
//
// ⚠ ET CE MOUVEMENT A UNE SECONDE BRANCHE, prise quand la marque de style vaut
// DEUX — celle du Soul Link. Même montée de treize, mais à d'autres plafonds
// (33 puis +23 par lame) ; HUIT lames retombent, de deux par tic seulement ; les
// DEUX dernières se dérobent après cent tics, et chacune, tant qu'elle vit,
// enfle le rayon d'un demi-centième — deux fois par tic quand les deux vivent.
// Et le rayon ne se resserre JAMAIS au début.

// LES DIX LAMES, et la case où elles commencent — la case zéro pousse
// l'éventail de côté, elle n'est pas une lame.
inline constexpr std::size_t kCameraFanBladeCount = 10;
inline constexpr std::size_t kCameraFanSlotOfFirstBlade = 1;
inline constexpr std::size_t kCameraFanSlotOfSideways = 0;
inline constexpr std::size_t kCameraFanMarkOfTexture = 0;
inline constexpr std::size_t kCameraFanMarkOfStyle = 4;

// LA MONTÉE : treize par tic pendant vingt tics, et chaque lame a SON plafond.
inline constexpr float kCameraFanRisePerTick = 13.0F;
inline constexpr float kCameraFanFirstCeiling = 70.0F;
inline constexpr float kCameraFanCeilingStep = 20.0F;
inline constexpr std::int32_t kCameraFanTicksOfRise = 20;

// LA CHUTE : cinq par tic, et seules les NEUF premières lames y sont. La
// dixième attend son propre seuil.
inline constexpr std::size_t kCameraFanBladesThatFall = 9;
inline constexpr float kCameraFanFallPerTick = 5.0F;

// LA DIXIÈME LAME SE DÉROBE TARD, et le rayon enfle tant qu'elle vit. Le seuil
// dépend de la marque de style : vingt-cinq pour l'une, cent quarante pour
// toutes les autres.
inline constexpr std::uint8_t kCameraFanQuickStyle = 4;
inline constexpr std::int32_t kCameraFanQuickOpeningTick = 25;
inline constexpr std::int32_t kCameraFanLateOpeningTick = 140;
inline constexpr float kCameraFanLastBladeFall = 5.0F;
inline constexpr float kCameraFanRadiusGrowth = 1.01F;

// AU DÉBUT, LE RAYON SE RESSERRE — sauf pour le style rapide, qui ne s'y prête
// pas.
inline constexpr std::int32_t kCameraFanTicksOfTightening = 50;
inline constexpr float kCameraFanTightenPerTick = 0.1F;

// LA BRANCHE DE LA MARQUE DEUX.
inline constexpr std::uint8_t kCameraFanTwinStyle = 2;
inline constexpr float kCameraFanTwinFirstCeiling = 33.0F;
inline constexpr float kCameraFanTwinCeilingStep = 23.0F;
inline constexpr std::size_t kCameraFanTwinBladesThatFall = 8;
inline constexpr float kCameraFanTwinFallPerTick = 2.0F;
inline constexpr std::int32_t kCameraFanTwinOpeningTick = 100;
inline constexpr float kCameraFanTwinLastBladesFall = 5.0F;
inline constexpr float kCameraFanTwinRadiusGrowth = 1.005F;

// VRAI quand ce nœud doit relire la position de son porteur à chaque tic. La
// PREMIÈRE piste décide pour tout le nœud : le client lit la marque à un
// déplacement absolu que sa boucle ne décale pas.
[[nodiscard]] bool CameraFanFollowsItsOwner(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// UN TIC DES QUATRE PISTES. Les deux angles de caméra se rangent dans les styles
// des deux premières, où la figure les relit — comme au sous-type 147.
void AdvanceCameraFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            std::int16_t billboard_turn_degrees,
                            std::int16_t billboard_tilt_degrees);

}  // namespace ro::render
