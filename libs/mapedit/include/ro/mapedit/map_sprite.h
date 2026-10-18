#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ro::mapedit {

// UN SPRITE DE DÉCOR : le sprite d'une créature — monstre ou PNJ — posé dans la
// carte par l'éditeur, et seulement peint. Ce n'est pas une unité : le serveur
// ne le connaît pas, on ne le clique ni ne le cible. Un feu de camp et un
// Savage qui tourne à la broche en sont l'exemple.
//
// Son ANIMATION boucle à `frame_speed` fois la cadence du .act, ou reste FIGÉE
// sur `held_frame`. Son ORIENTATION est une des huit directions du .act, ou
// tourne de `turn_degrees_per_second` ; son ROULIS fait pivoter l'image dans
// son plan, d'un angle fixe ou en continu. Les angles se comptent dans le sens
// où les directions du .act croissent, en degrés.
struct MapSprite {
  // Le nom du sprite, tel que la table des classes le donne (« SAVAGE »), ou le
  // numéro de sa classe écrit en chiffres.
  std::string sprite;
  // Ses pieds, dans le repère du RSW.
  std::array<float, 3> position{};
  // Le rang d'animation du .act : 0 l'attente, 1 la marche, 2 l'attaque, 3 la
  // blessure, 4 la mort, pour une créature.
  std::uint32_t action = 0;
  // L'image tenue ; rien : l'animation boucle.
  std::optional<std::uint32_t> held_frame;
  // La cadence de la boucle, en fois celle du .act ; positive.
  float frame_speed = 1.0F;
  // La direction du .act à l'instant zéro, de 0 (de face) à 7, sous une caméra
  // qui n'a pas pivoté.
  std::uint32_t direction = 0;
  float turn_degrees_per_second = 0.0F;
  float roll_degrees = 0.0F;
  float roll_degrees_per_second = 0.0F;
  // L'échelle de l'acteur entier ; positive.
  float scale = 1.0F;
  // Translucide, de 0 à 1 ; rien : plein.
  std::optional<float> opacity;
  bool hidden = false;
  std::string why;

  friend bool operator==(const MapSprite&, const MapSprite&) = default;
};

// Les huit directions d'un .act, et l'angle qui les sépare.
inline constexpr std::uint32_t kSpriteDirectionCount = 8;
inline constexpr float kFullTurnDegrees = 360.0F;
inline constexpr float kSpriteDirectionDegrees =
    kFullTurnDegrees / static_cast<float>(kSpriteDirectionCount);

// Les bornes d'une cadence et d'une échelle : le document refuse ce qui sort
// de (0, borne].
inline constexpr float kLargestSpriteFrameSpeed = 16.0F;
inline constexpr float kLargestSpriteScale = 16.0F;

// La cadence et l'échelle tiennent-elles dans leurs bornes ? Faux pour NaN.
[[nodiscard]] constexpr bool SpriteFrameSpeedInBounds(float speed) noexcept {
  return speed > 0.0F && speed <= kLargestSpriteFrameSpeed;
}
[[nodiscard]] constexpr bool SpriteScaleInBounds(float scale) noexcept {
  return scale > 0.0F && scale <= kLargestSpriteScale;
}

// LE NUMÉRO DE CLASSE qu'écrit `sprite` quand il n'est fait que de chiffres ;
// rien pour un nom.
[[nodiscard]] std::optional<std::uint32_t> SpriteClassNumber(std::string_view sprite);

// L'ANGLE DE FACE à `seconds` secondes, ramené dans [0, 360) : la direction de
// départ, plus ce que la rotation a tourné depuis.
[[nodiscard]] float SpriteFacingDegreesAt(const MapSprite& sprite, double seconds);

// La direction du .act que montre cet angle, sous une caméra qui n'a pas
// pivoté : le huitième de tour le plus proche.
[[nodiscard]] std::uint32_t SpriteDirectionOfFacing(float facing_degrees);

// L'IMAGE À `seconds` secondes, dans une animation de `frame_count` images de
// `frame_interval_ms` chacune : celle que la boucle atteint à sa cadence, ou
// l'image tenue, ramenée à la dernière. Zéro sans image ni durée.
[[nodiscard]] std::size_t SpriteFrameAt(const MapSprite& sprite, double seconds,
                                        std::size_t frame_count, float frame_interval_ms);

// LE ROULIS à `seconds` secondes, ramené dans [0, 360).
[[nodiscard]] float SpriteRollDegreesAt(const MapSprite& sprite, double seconds);

}  // namespace ro::mapedit
