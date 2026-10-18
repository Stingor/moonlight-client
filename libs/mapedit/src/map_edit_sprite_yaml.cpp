#include <cstdint>
#include <format>
#include <string>

#include "ground_yaml.h"
#include "sprite_yaml.h"

namespace ro::mapedit::detail {
namespace {

constexpr std::size_t kVectorSize = 3;

// Un sprite, ou le motif de son refus dans `refusal`.
MapSprite SpriteFrom(const YAML::Node& entry, const std::string& where, std::string& refusal) {
  MapSprite sprite;
  if (!entry.IsMap()) {
    refusal = where + " n'est pas une table";
    return sprite;
  }
  const YAML::Node name = entry["sprite"];
  const YAML::Node position = entry["position"];
  const YAML::Node why = entry["why"];
  if (!name || !position || !why) {
    refusal = where + " : « sprite », « position » et « why » sont dus";
    return sprite;
  }
  sprite.sprite = name.as<std::string>();
  sprite.why = why.as<std::string>();
  if (sprite.sprite.empty() || sprite.why.empty()) {
    refusal = where + " : « sprite » ou « why » est vide";
    return sprite;
  }
  if (!position.IsSequence() || position.size() != kVectorSize) {
    refusal = where + " : « position » n'a pas trois composantes";
    return sprite;
  }
  for (std::size_t axis = 0; axis < kVectorSize; ++axis) {
    sprite.position[axis] = position[axis].as<float>();
  }
  if (const YAML::Node action = entry["action"]) {
    sprite.action = action.as<std::uint32_t>();
  }
  if (const YAML::Node frame = entry["frame"]) {
    sprite.held_frame = frame.as<std::uint32_t>();
  }
  if (const YAML::Node speed = entry["speed"]) {
    sprite.frame_speed = speed.as<float>();
  }
  if (const YAML::Node direction = entry["direction"]) {
    sprite.direction = direction.as<std::uint32_t>();
  }
  if (const YAML::Node turn = entry["turn"]) {
    sprite.turn_degrees_per_second = turn.as<float>();
  }
  if (const YAML::Node roll = entry["roll"]) {
    sprite.roll_degrees = roll.as<float>();
  }
  if (const YAML::Node roll_speed = entry["roll_speed"]) {
    sprite.roll_degrees_per_second = roll_speed.as<float>();
  }
  if (const YAML::Node scale = entry["scale"]) {
    sprite.scale = scale.as<float>();
  }
  if (const YAML::Node opacity = entry["opacity"]) {
    sprite.opacity = opacity.as<float>();
  }
  if (const YAML::Node hidden = entry["hidden"]) {
    sprite.hidden = hidden.as<bool>();
  }
  if (sprite.direction >= kSpriteDirectionCount) {
    refusal = where + " : « direction » sort de 0 à 7";
  } else if (!SpriteFrameSpeedInBounds(sprite.frame_speed)) {
    refusal = std::format("{} : « speed » sort de 0 à {}", where, kLargestSpriteFrameSpeed);
  } else if (!SpriteScaleInBounds(sprite.scale)) {
    refusal = std::format("{} : « scale » sort de 0 à {}", where, kLargestSpriteScale);
  } else if (sprite.opacity.has_value() && !OpacityInBounds(*sprite.opacity)) {
    refusal = where + " : « opacity » sort de 0 à 1";
  }
  return sprite;
}

}  // namespace

std::string ReadSprites(const YAML::Node& sprites, MapEdit& edit) {
  if (!sprites.IsSequence()) {
    return "« sprites » n'est pas une liste";
  }
  for (std::size_t rank = 0; rank < sprites.size(); ++rank) {
    std::string refusal;
    MapSprite sprite = SpriteFrom(sprites[rank], std::format("sprite {}", rank), refusal);
    if (!refusal.empty()) {
      return refusal;
    }
    edit.AddSprite(std::move(sprite));
  }
  return {};
}

std::string SpritesYaml(const MapEdit& edit) {
  if (edit.sprites().empty()) {
    return {};
  }
  std::string text = "sprites:\n";
  // CE QUI S'ÉCARTE DU DÉFAUT s'écrit, sauf l'action et la direction, qui se
  // disent toujours : on les cherche d'un coup d'œil.
  for (const MapSprite& sprite : edit.sprites()) {
    text += std::format("  - sprite: {}\n", SingleQuoted(sprite.sprite));
    text += std::format("    position: {}\n", VectorText(sprite.position));
    text += std::format("    action: {}\n", sprite.action);
    if (sprite.held_frame.has_value()) {
      text += std::format("    frame: {}\n", *sprite.held_frame);
    }
    if (sprite.frame_speed != MapSprite().frame_speed) {
      text += std::format("    speed: {}\n", sprite.frame_speed);
    }
    text += std::format("    direction: {}\n", sprite.direction);
    if (sprite.turn_degrees_per_second != 0.0F) {
      text += std::format("    turn: {}\n", sprite.turn_degrees_per_second);
    }
    if (sprite.roll_degrees != 0.0F) {
      text += std::format("    roll: {}\n", sprite.roll_degrees);
    }
    if (sprite.roll_degrees_per_second != 0.0F) {
      text += std::format("    roll_speed: {}\n", sprite.roll_degrees_per_second);
    }
    if (sprite.scale != MapSprite().scale) {
      text += std::format("    scale: {}\n", sprite.scale);
    }
    if (sprite.opacity.has_value()) {
      text += std::format("    opacity: {}\n", *sprite.opacity);
    }
    if (sprite.hidden) {
      text += "    hidden: true\n";
    }
    text += std::format("    why: {}\n", DoubleQuoted(sprite.why));
  }
  return text;
}

}  // namespace ro::mapedit::detail
