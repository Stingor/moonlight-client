#include "light_yaml.h"

#include <array>
#include <format>

#include "ground_yaml.h"

namespace ro::mapedit::detail {
namespace {

constexpr std::size_t kVectorSize = 3;
// Les deux bornes d'une plage d'heures.
constexpr std::size_t kHourBounds = 2;
// Ce qui ouvre une entrée de liste, et ce qui aligne ses autres clés.
constexpr std::string_view kEntryOpening = "  - ";
constexpr std::string_view kEntryIndent = "    ";

// Les champs d'une lumière, chacun à l'alignement d'une entrée de liste.
std::string LightFieldsText(const AddedLight& light) {
  std::string text = std::format("{}position: {}\n", kEntryIndent, VectorText(light.position));
  text += std::format("{}color: {}\n", kEntryIndent, VectorText(light.color));
  text += std::format("{}range: {}\n", kEntryIndent, light.range);
  if (light.night_only) {
    text += std::format("{}night_only: true\n", kEntryIndent);
  }
  text += HoursText(light.hours);
  if (light.flicker) {
    text += std::format("{}flicker: true\n", kEntryIndent);
  }
  if (!light.shadow) {
    text += std::format("{}shadow: false\n", kEntryIndent);
  }
  text += std::format("{}why: {}\n", kEntryIndent, DoubleQuoted(light.why));
  return text;
}

}  // namespace

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

std::string ReadHours(const YAML::Node& entry, const std::string& where,
                      std::optional<LitHours>& hours) {
  hours.reset();
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

std::string HoursText(const std::optional<LitHours>& hours) {
  if (!hours.has_value()) {
    return {};
  }
  return std::format("{}hours: [{}, {}]\n", kEntryIndent, hours->from_hour, hours->to_hour);
}

std::string ReadLight(const YAML::Node& entry, const std::string& where, AddedLight& light) {
  light = AddedLight();
  if (!entry.IsMap()) {
    return where + " n'est pas une table";
  }
  const YAML::Node position = entry["position"];
  const YAML::Node color = entry["color"];
  const YAML::Node range = entry["range"];
  const YAML::Node why = entry["why"];
  if (!position || !color || !range || !why) {
    return where + " : « position », « color », « range » et « why » sont dus";
  }
  light.why = why.as<std::string>();
  if (light.why.empty()) {
    return where + " : « why » est vide";
  }
  light.range = range.as<float>();
  if (!(light.range > 0.0F)) {
    return where + " : « range » doit être positive";
  }
  if (std::string refusal = ReadVector(position, where + " : « position »", light.position);
      !refusal.empty()) {
    return refusal;
  }
  if (std::string refusal = ReadVector(color, where + " : « color »", light.color);
      !refusal.empty()) {
    return refusal;
  }
  if (const YAML::Node night_only = entry["night_only"]) {
    light.night_only = night_only.as<bool>();
  }
  if (std::string refusal = ReadHours(entry, where, light.hours); !refusal.empty()) {
    return refusal;
  }
  if (const YAML::Node flicker = entry["flicker"]) {
    light.flicker = flicker.as<bool>();
  }
  if (const YAML::Node shadow = entry["shadow"]) {
    light.shadow = shadow.as<bool>();
  }
  return {};
}

std::string LightEntryText(const AddedLight& light) {
  return std::string(kEntryOpening) + LightFieldsText(light).substr(kEntryIndent.size());
}

std::string ReadModelLights(const YAML::Node& whole, std::string_view map_name, MapEdit& edit) {
  const YAML::Node lights = whole["model_lights"];
  if (!lights) {
    return {};
  }
  if (map_name != kModelLightsDocument) {
    return "« model_lights » n'appartient qu'au document des lumières de modèle";
  }
  if (!lights.IsSequence()) {
    return "« model_lights » n'est pas une liste";
  }
  for (std::size_t rank = 0; rank < lights.size(); ++rank) {
    const std::string where = std::format("lumière de modèle {}", rank);
    const YAML::Node file = lights[rank]["file"];
    if (!lights[rank].IsMap() || !file) {
      return where + " : « file » est dû";
    }
    ModelLight carried{.model_file = file.as<std::string>()};
    if (carried.model_file.empty()) {
      return where + " : « file » est vide";
    }
    if (std::string refusal = ReadLight(lights[rank], where, carried.light); !refusal.empty()) {
      return refusal;
    }
    edit.AddModelLight(std::move(carried));
  }
  return {};
}

std::string ModelLightsYaml(const MapEdit& edit) {
  if (edit.model_lights().empty()) {
    return {};
  }
  std::string text = "model_lights:\n";
  for (const ModelLight& carried : edit.model_lights()) {
    text += std::format("{}file: {}\n", kEntryOpening, SingleQuoted(carried.model_file));
    text += LightFieldsText(carried.light);
  }
  return text;
}

}  // namespace ro::mapedit::detail
