// LES SONS D'AMBIANCE DANS LE TEXTE DU DOCUMENT : « map_sounds », les
// retouches des sources du RSW, et « sounds », les sons ajoutés. Le format est
// décrit dans l'en-tête que l'éditeur écrit, voir map_edit_header.cpp.

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ground_yaml.h"
#include "sound_yaml.h"

namespace ro::mapedit::detail {
namespace {

constexpr std::size_t kVectorSize = 3;
// Une plage d'heures : sa borne de départ, sa borne de fin.
constexpr std::size_t kHourBounds = 2;
// Une emprise : sa largeur, sa hauteur.
constexpr std::size_t kSizeFields = 2;

// Lit un vecteur de trois réels dans `vector` ; rend le motif du refus.
std::string ReadVector(const YAML::Node& node, const std::string& where,
                       std::array<float, 3>& vector) {
  if (!node.IsSequence() || node.size() != kVectorSize) {
    return where + " n'a pas trois composantes";
  }
  for (std::size_t axis = 0; axis < kVectorSize; ++axis) {
    vector[axis] = node[axis].as<float>();
  }
  return {};
}

// La plage d'heures sous « hours » : deux heures de 0 à 24 ; rien sans la clé.
std::string ReadHours(const YAML::Node& entry, const std::string& where,
                      std::optional<LitHours>& hours) {
  const YAML::Node node = entry["hours"];
  if (!node) {
    return {};
  }
  if (!node.IsSequence() || node.size() != kHourBounds) {
    return where + " : « hours » n'a pas deux heures";
  }
  const LitHours read{.from_hour = node[0].as<float>(), .to_hour = node[1].as<float>()};
  for (const float hour : {read.from_hour, read.to_hour}) {
    if (!(hour >= 0.0F && hour <= kLitHoursPerDay)) {
      return where + " : « hours » sort de 0 à 24";
    }
  }
  hours = read;
  return {};
}

// Un réel facultatif sous `key`, strictement positif ; rien sans la clé.
std::string ReadPositive(const YAML::Node& entry, std::string_view key, const std::string& where,
                         std::optional<float>& value) {
  const YAML::Node node = entry[std::string(key)];
  if (!node) {
    return {};
  }
  const float read = node.as<float>();
  if (!(read > 0.0F)) {
    return std::format("{} : « {} » doit être positif", where, key);
  }
  value = read;
  return {};
}

// Le volume facultatif sous « volume », de 0 à 1 ; rien sans la clé.
std::string ReadVolume(const YAML::Node& entry, const std::string& where,
                       std::optional<float>& volume) {
  const YAML::Node node = entry["volume"];
  if (!node) {
    return {};
  }
  const float read = node.as<float>();
  if (!SoundVolumeInBounds(read)) {
    return where + " : « volume » sort de 0 à 1";
  }
  volume = read;
  return {};
}

// Ce que retouches et ajouts partagent : la nuit, la plage d'heures, la raison.
std::string ReadNightAndWhy(const YAML::Node& entry, const std::string& where, bool& night_only,
                            std::optional<LitHours>& hours, std::string& why) {
  if (const YAML::Node night = entry["night_only"]) {
    night_only = night.as<bool>();
  }
  why = entry["why"].as<std::string>();
  if (why.empty()) {
    return where + " : « why » est vide";
  }
  return ReadHours(entry, where, hours);
}

std::string ReadSoundEdit(const YAML::Node& entry, const std::string& where, MapEdit& edit) {
  if (!entry.IsMap() || !entry["index"] || !entry["file"] || !entry["origin"] || !entry["why"]) {
    return where + " : « index », « file », « origin » et « why » sont dus";
  }
  SoundEdit retouch;
  retouch.sound_index = entry["index"].as<std::size_t>();
  retouch.wave_file = entry["file"].as<std::string>();
  std::string refusal = ReadVector(entry["origin"], where + " : « origin »", retouch.origin);
  if (refusal.empty() && entry["position"]) {
    std::array<float, 3> position{};
    refusal = ReadVector(entry["position"], where + " : « position »", position);
    retouch.position = position;
  }
  if (const YAML::Node hidden = entry["hidden"]) {
    retouch.hidden = hidden.as<bool>();
  }
  for (std::string next :
       {ReadVolume(entry, where, retouch.volume),
        ReadPositive(entry, "range", where, retouch.range),
        ReadPositive(entry, "cycle", where, retouch.cycle_seconds),
        ReadNightAndWhy(entry, where, retouch.night_only, retouch.hours, retouch.why)}) {
    if (refusal.empty()) {
      refusal = std::move(next);
    }
  }
  if (refusal.empty()) {
    edit.SetSoundEdit(std::move(retouch));
  }
  return refusal;
}

std::string ReadAddedSound(const YAML::Node& entry, const std::string& where, MapEdit& edit) {
  if (!entry.IsMap() || !entry["file"] || !entry["position"] || !entry["range"] || !entry["why"]) {
    return where + " : « file », « position », « range » et « why » sont dus";
  }
  AddedSound sound;
  sound.wave_file = entry["file"].as<std::string>();
  if (sound.wave_file.empty()) {
    return where + " : « file » est vide";
  }
  std::string refusal = ReadVector(entry["position"], where + " : « position »", sound.position);
  if (const YAML::Node size = entry["size"]; refusal.empty() && size) {
    if (!size.IsSequence() || size.size() != kSizeFields) {
      refusal = where + " : « size » n'a pas deux composantes";
    } else {
      sound.width = size[0].as<std::int32_t>();
      sound.height = size[1].as<std::int32_t>();
      if (sound.width < 0 || sound.height < 0) {
        refusal = where + " : « size » est négative";
      }
    }
  }
  std::optional<float> volume;
  std::optional<float> range;
  std::optional<float> cycle;
  for (std::string next :
       {ReadVolume(entry, where, volume), ReadPositive(entry, "range", where, range),
        ReadPositive(entry, "cycle", where, cycle),
        ReadNightAndWhy(entry, where, sound.night_only, sound.hours, sound.why)}) {
    if (refusal.empty()) {
      refusal = std::move(next);
    }
  }
  if (!refusal.empty()) {
    return refusal;
  }
  sound.volume = volume.value_or(sound.volume);
  sound.range = range.value_or(sound.range);
  sound.cycle_seconds = cycle.value_or(sound.cycle_seconds);
  edit.AddSound(std::move(sound));
  return {};
}

// La nuit, la plage d'heures et la raison, à la fin d'une entrée.
std::string NightAndWhyText(bool night_only, const std::optional<LitHours>& hours,
                            std::string_view why) {
  std::string text;
  if (night_only) {
    text += "    night_only: true\n";
  }
  if (hours.has_value()) {
    text += std::format("    hours: [{}, {}]\n", hours->from_hour, hours->to_hour);
  }
  return text + std::format("    why: {}\n", DoubleQuoted(why));
}

}  // namespace

std::string ReadSounds(const YAML::Node& whole, MapEdit& edit) {
  if (const YAML::Node retouches = whole["map_sounds"]) {
    if (!retouches.IsSequence()) {
      return "« map_sounds » n'est pas une liste";
    }
    for (std::size_t rank = 0; rank < retouches.size(); ++rank) {
      std::string refusal =
          ReadSoundEdit(retouches[rank], std::format("retouche de son {}", rank), edit);
      if (!refusal.empty()) {
        return refusal;
      }
    }
  }
  if (const YAML::Node added = whole["sounds"]) {
    if (!added.IsSequence()) {
      return "« sounds » n'est pas une liste";
    }
    for (std::size_t rank = 0; rank < added.size(); ++rank) {
      std::string refusal = ReadAddedSound(added[rank], std::format("son {}", rank), edit);
      if (!refusal.empty()) {
        return refusal;
      }
    }
  }
  return {};
}

std::string SoundsYaml(const MapEdit& edit) {
  std::string text;
  if (!edit.sound_edits().empty()) {
    text += "map_sounds:\n";
  }
  for (const SoundEdit& retouch : edit.sound_edits()) {
    text += std::format("  - index: {}\n", retouch.sound_index);
    text += std::format("    file: {}\n", SingleQuoted(retouch.wave_file));
    text += std::format("    origin: {}\n", VectorText(retouch.origin));
    if (retouch.position.has_value()) {
      text += std::format("    position: {}\n", VectorText(*retouch.position));
    }
    if (retouch.hidden) {
      text += "    hidden: true\n";
    }
    if (retouch.volume.has_value()) {
      text += std::format("    volume: {}\n", *retouch.volume);
    }
    if (retouch.range.has_value()) {
      text += std::format("    range: {}\n", *retouch.range);
    }
    if (retouch.cycle_seconds.has_value()) {
      text += std::format("    cycle: {}\n", *retouch.cycle_seconds);
    }
    text += NightAndWhyText(retouch.night_only, retouch.hours, retouch.why);
  }
  if (!edit.sounds().empty()) {
    text += "sounds:\n";
  }
  for (const AddedSound& sound : edit.sounds()) {
    text += std::format("  - file: {}\n", SingleQuoted(sound.wave_file));
    text += std::format("    position: {}\n", VectorText(sound.position));
    text += std::format("    volume: {}\n", sound.volume);
    text += std::format("    size: [{}, {}]\n", sound.width, sound.height);
    text += std::format("    range: {}\n", sound.range);
    text += std::format("    cycle: {}\n", sound.cycle_seconds);
    text += NightAndWhyText(sound.night_only, sound.hours, sound.why);
  }
  return text;
}

}  // namespace ro::mapedit::detail
