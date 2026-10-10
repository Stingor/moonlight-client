#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ro::render {

// CE QU'UN BÂTISSEUR DE SPRITE TIRE AU SORT, un tirage chacun.
//
// Le semis de sprites a son propre ordre — la vie, le pas, l'angle du bras,
// l'échelle, puis le lancer —, et chaque bâtisseur du client le sien : la
// gerbe d'impact tire son cap, son inclinaison et sa distance AVANT son pas et
// sa vie. Un patron qui nomme son ordre le fait respecter : tout est tiré
// d'abord, dans cet ordre, puis lu à sa place. Ce qu'il ne nomme pas ne se tire
// pas — la borne basse vaut.
enum class EffectSpriteDraw : std::uint8_t {
  kLife,
  kStep,
  kArmAngle,
  kScale,
  kLaunchYaw,
  kLaunchPitch,
  kLaunchDistance,
  kFallSpeed,
  kFallAcceleration,
  // Un tirage que le bâtisseur fait et jette.
  kDiscarded,
  kCount,
};
inline constexpr std::size_t kEffectSpriteDrawCount =
    static_cast<std::size_t>(EffectSpriteDraw::kCount);
inline constexpr std::size_t kEffectSpriteDrawOrderCapacity = 8;

// L'ORDRE D'UN BÂTISSEUR : `count` tirages, les premiers du tableau.
struct EffectSpriteDrawOrder {
  std::array<EffectSpriteDraw, kEffectSpriteDrawOrderCapacity> draws{};
  std::uint8_t count = 0;
};

// LES VALEURS TIRÉES D'AVANCE, par sorte ; vide quand le semis tire lui-même.
using EffectSpritePredrawn = std::array<std::optional<float>, kEffectSpriteDrawCount>;

// LES DEUX ANGLES D'UN PROJECTILE, dans le repère du client et en degrés : le
// cap (le 780 de son nœud) et l'inclinaison (le 784). Les étincelles de la
// flèche de feu partent d'eux.
struct EffectProjectileAngles {
  float yaw_degrees = 0.0F;
  float pitch_degrees = 0.0F;
};

// La valeur tirée d'avance pour cette sorte, ou rien.
[[nodiscard]] inline std::optional<float> EffectSpritePredrawnValue(
    const EffectSpritePredrawn& predrawn, EffectSpriteDraw draw) {
  return predrawn[static_cast<std::size_t>(draw)];
}

}  // namespace ro::render
