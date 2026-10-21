#include "effect_yaml.h"

#include <format>
#include <utility>

#include "ground_yaml.h"
#include "light_yaml.h"

namespace ro::mapedit::detail {
namespace {

// Ce qui ouvre une entrée de liste, et ce qui aligne ses autres clés.
constexpr std::string_view kEntryOpening = "  - ";
constexpr std::string_view kEntryIndent = "    ";

// Les champs d'un effet, chacun à l'alignement d'une entrée de liste.
std::string EffectFieldsText(const AddedEffect& effect) {
  std::string text = std::format("{}number: {}\n", kEntryIndent, effect.number);
  text += std::format("{}position: {}\n", kEntryIndent, VectorText(effect.position));
  text += std::format("{}emit_speed: {}\n", kEntryIndent, effect.emit_speed);
  text += std::format("{}param: [{}, {}, {}, {}]\n", kEntryIndent, effect.param[0], effect.param[1],
                      effect.param[2], effect.param[3]);
  if (effect.night_only) {
    text += std::format("{}night_only: true\n", kEntryIndent);
  }
  text += HoursText(effect.hours);
  text += GlowText(effect.glow);
  text += std::format("{}why: {}\n", kEntryIndent, DoubleQuoted(effect.why));
  return text;
}

}  // namespace

std::string ReadGlow(const YAML::Node& glow, const std::string& where, EffectGlow& lit) {
  lit = EffectGlow();
  if (const YAML::Node color = glow["color"]) {
    if (std::string refusal = ReadVector(color, where + " : « glow.color »", lit.color);
        !refusal.empty()) {
      return refusal;
    }
  }
  if (const YAML::Node range = glow["range"]) {
    lit.range = range.as<float>();
  }
  if (!(lit.range > 0.0F)) {
    return where + " : « glow.range » doit être positive";
  }
  if (const YAML::Node flicker = glow["flicker"]) {
    lit.flicker = flicker.as<bool>();
  }
  if (const YAML::Node shadow = glow["shadow"]) {
    lit.shadow = shadow.as<bool>();
  }
  return {};
}

std::string GlowText(const std::optional<EffectGlow>& glow) {
  if (!glow.has_value()) {
    return {};
  }
  std::string text = std::format("    glow:\n      color: {}\n      range: {}\n",
                                 VectorText(glow->color), glow->range);
  // CE QUI S'ÉCARTE DU DÉFAUT s'écrit : une lueur vacille et porte ombre sans
  // qu'on le dise.
  if (!glow->flicker) {
    text += "      flicker: false\n";
  }
  if (!glow->shadow) {
    text += "      shadow: false\n";
  }
  return text;
}

std::string ReadEffect(const YAML::Node& entry, const std::string& where, AddedEffect& effect) {
  effect = AddedEffect();
  if (!entry.IsMap()) {
    return where + " n'est pas une table";
  }
  const YAML::Node number = entry["number"];
  const YAML::Node position = entry["position"];
  const YAML::Node why = entry["why"];
  if (!number || !position || !why) {
    return where + " : « number », « position » et « why » sont dus";
  }
  effect.number = number.as<std::int32_t>();
  effect.why = why.as<std::string>();
  if (effect.why.empty()) {
    return where + " : « why » est vide";
  }
  if (std::string refusal = ReadVector(position, where + " : « position »", effect.position);
      !refusal.empty()) {
    return refusal;
  }
  if (const YAML::Node speed = entry["emit_speed"]) {
    effect.emit_speed = speed.as<float>();
  }
  if (const YAML::Node param = entry["param"]) {
    if (!param.IsSequence() || param.size() != effect.param.size()) {
      return where + " : « param » n'a pas quatre composantes";
    }
    for (std::size_t rank = 0; rank < effect.param.size(); ++rank) {
      effect.param[rank] = param[rank].as<float>();
    }
  }
  if (const YAML::Node night_only = entry["night_only"]) {
    effect.night_only = night_only.as<bool>();
  }
  if (std::string refusal = ReadHours(entry, where, effect.hours); !refusal.empty()) {
    return refusal;
  }
  if (const YAML::Node glow = entry["glow"]) {
    EffectGlow lit;
    if (std::string refusal = ReadGlow(glow, where, lit); !refusal.empty()) {
      return refusal;
    }
    effect.glow = lit;
  }
  return {};
}

std::string EffectEntryText(const AddedEffect& effect) {
  return std::string(kEntryOpening) + EffectFieldsText(effect).substr(kEntryIndent.size());
}

std::string ReadModelEffects(const YAML::Node& whole, std::string_view map_name, MapEdit& edit) {
  const YAML::Node effects = whole["model_effects"];
  if (!effects) {
    return {};
  }
  if (map_name != kModelLightsDocument) {
    return "« model_effects » n'appartient qu'au document des lumières de modèle";
  }
  if (!effects.IsSequence()) {
    return "« model_effects » n'est pas une liste";
  }
  for (std::size_t rank = 0; rank < effects.size(); ++rank) {
    const std::string where = std::format("effet de modèle {}", rank);
    const YAML::Node file = effects[rank]["file"];
    if (!effects[rank].IsMap() || !file) {
      return where + " : « file » est dû";
    }
    ModelEffect carried{.model_file = file.as<std::string>()};
    if (carried.model_file.empty()) {
      return where + " : « file » est vide";
    }
    if (std::string refusal = ReadEffect(effects[rank], where, carried.effect); !refusal.empty()) {
      return refusal;
    }
    edit.AddModelEffect(std::move(carried));
  }
  return {};
}

std::string ModelEffectsYaml(const MapEdit& edit) {
  if (edit.model_effects().empty()) {
    return {};
  }
  std::string text = "model_effects:\n";
  for (const ModelEffect& carried : edit.model_effects()) {
    text += std::format("{}file: {}\n", kEntryOpening, SingleQuoted(carried.model_file));
    text += EffectFieldsText(carried.effect);
  }
  return text;
}

}  // namespace ro::mapedit::detail
