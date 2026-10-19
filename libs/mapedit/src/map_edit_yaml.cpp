#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

#include "ground_yaml.h"
#include "light_yaml.h"
#include "map_edit_header.h"
#include "sound_yaml.h"
#include "sprite_yaml.h"

namespace ro::mapedit {

using detail::DoubleQuoted;
using detail::HoursText;
using detail::SingleQuoted;
using detail::VectorText;

namespace {

constexpr int kSupportedVersion = 1;
constexpr std::size_t kVectorSize = 3;

core::Error Refused(std::string_view map_name, std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     MapEditPath(map_name) + " : " + std::move(detail)};
}

// Un vecteur de trois réels. Deux composantes ne font pas une position : mieux
// vaut refuser le document que d'en compléter une à zéro.
core::Result<std::array<float, 3>> VectorFrom(const YAML::Node& node, std::string_view map_name,
                                              const std::string& where) {
  if (!node.IsSequence() || node.size() != kVectorSize) {
    return std::unexpected(Refused(map_name, where + " n'a pas trois composantes"));
  }
  std::array<float, 3> vector{};
  for (std::size_t axis = 0; axis < kVectorSize; ++axis) {
    vector[axis] = node[axis].as<float>();
  }
  return vector;
}

core::Result<std::optional<std::array<float, 3>>> OptionalVectorFrom(const YAML::Node& entry,
                                                                     std::string_view field,
                                                                     std::string_view map_name,
                                                                     const std::string& where) {
  const YAML::Node node = entry[std::string(field)];
  if (!node) {
    return std::optional<std::array<float, 3>>();
  }
  core::Result<std::array<float, 3>> vector =
      VectorFrom(node, map_name, where + " : « " + std::string(field) + " »");
  if (!vector) {
    return std::unexpected(vector.error());
  }
  return std::optional<std::array<float, 3>>(*vector);
}

// L'opacité sous la clé « opacity », de 0 à 1 ; rien sans la clé.
core::Result<std::optional<float>> OpacityFrom(const YAML::Node& entry, std::string_view map_name,
                                               const std::string& where) {
  const YAML::Node node = entry["opacity"];
  if (!node) {
    return std::optional<float>();
  }
  const float opacity = node.as<float>();
  if (!OpacityInBounds(opacity)) {
    return std::unexpected(Refused(map_name, where + " : « opacity » sort de 0 à 1"));
  }
  return std::optional<float>(opacity);
}

// Les mots du document pour les façons de jouer d'un .rsw.
constexpr std::string_view kPlayStill = "still";
constexpr std::string_view kPlayOnce = "once";
constexpr std::string_view kPlayLooped = "loop";

// Le mot d'une façon de jouer ; toute valeur que le .rsw ne joue pas fige.
std::string_view PlayWord(std::int32_t animation_type) {
  if (animation_type == formats::kRswModelPlayLooped) {
    return kPlayLooped;
  }
  return animation_type == formats::kRswModelPlayOnce ? kPlayOnce : kPlayStill;
}

// La façon de jouer et la vitesse d'un ajout, sous « animation » et
// « animation_speed » ; une clé absente garde le défaut d'`AddedModel`.
core::Result<void> PlaybackFrom(const YAML::Node& entry, std::string_view map_name,
                                const std::string& where, AddedModel& added) {
  if (const YAML::Node play = entry["animation"]) {
    const std::string word = play.as<std::string>();
    if (word == kPlayStill) {
      added.animation_type = formats::kRswModelStill;
    } else if (word == kPlayOnce) {
      added.animation_type = formats::kRswModelPlayOnce;
    } else if (word == kPlayLooped) {
      added.animation_type = formats::kRswModelPlayLooped;
    } else {
      return std::unexpected(
          Refused(map_name, where + " : « animation » n'est ni still, ni once, ni loop"));
    }
  }
  if (const YAML::Node speed = entry["animation_speed"]) {
    added.animation_speed = speed.as<float>();
    if (!(added.animation_speed > 0.0F) || added.animation_speed > formats::kRswMaxAnimationSpeed) {
      return std::unexpected(Refused(map_name, where + " : « animation_speed » sort de ]0, 100]"));
    }
  }
  return {};
}

core::Result<ModelEdit> ModelEditFrom(const YAML::Node& entry, std::string_view map_name,
                                      const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node index = entry["index"];
  const YAML::Node file = entry["file"];
  const YAML::Node origin = entry["origin"];
  const YAML::Node why = entry["why"];
  if (!index || !file || !origin || !why) {
    return std::unexpected(
        Refused(map_name, where + " : « index », « file », « origin » et « why » sont dus"));
  }
  ModelEdit edit;
  edit.model_index = index.as<std::size_t>();
  edit.model_file = file.as<std::string>();
  edit.why = why.as<std::string>();
  if (edit.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> origin_vector =
      VectorFrom(origin, map_name, where + " : « origin »");
  if (!origin_vector) {
    return std::unexpected(origin_vector.error());
  }
  edit.origin = *origin_vector;
  auto position = OptionalVectorFrom(entry, "position", map_name, where);
  auto rotation = OptionalVectorFrom(entry, "rotation", map_name, where);
  auto scale = OptionalVectorFrom(entry, "scale", map_name, where);
  for (const auto* field : {&position, &rotation, &scale}) {
    if (!*field) {
      return std::unexpected(field->error());
    }
  }
  edit.position = *position;
  edit.rotation = *rotation;
  edit.scale = *scale;
  if (const YAML::Node hidden = entry["hidden"]) {
    edit.hidden = hidden.as<bool>();
  }
  auto opacity = OpacityFrom(entry, map_name, where);
  if (!opacity) {
    return std::unexpected(opacity.error());
  }
  edit.opacity = *opacity;
  if (const YAML::Node unlit = entry["unlit"]) {
    edit.unlit = unlit.as<bool>();
  }
  return edit;
}

core::Result<AddedModel> AddedModelFrom(const YAML::Node& entry, std::string_view map_name,
                                        const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node file = entry["file"];
  const YAML::Node position = entry["position"];
  const YAML::Node why = entry["why"];
  if (!file || !position || !why) {
    return std::unexpected(
        Refused(map_name, where + " : « file », « position » et « why » sont dus"));
  }
  AddedModel added;
  added.model_file = file.as<std::string>();
  added.why = why.as<std::string>();
  if (added.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> placed =
      VectorFrom(position, map_name, where + " : « position »");
  if (!placed) {
    return std::unexpected(placed.error());
  }
  added.position = *placed;
  auto rotation = OptionalVectorFrom(entry, "rotation", map_name, where);
  auto scale = OptionalVectorFrom(entry, "scale", map_name, where);
  if (!rotation) {
    return std::unexpected(rotation.error());
  }
  if (!scale) {
    return std::unexpected(scale.error());
  }
  added.rotation = rotation->value_or(added.rotation);
  added.scale = scale->value_or(added.scale);
  if (const YAML::Node hidden = entry["hidden"]) {
    added.hidden = hidden.as<bool>();
  }
  if (const YAML::Node unlit = entry["unlit"]) {
    added.unlit = unlit.as<bool>();
  }
  auto opacity = OpacityFrom(entry, map_name, where);
  if (!opacity) {
    return std::unexpected(opacity.error());
  }
  added.opacity = *opacity;
  if (core::Result<void> played = PlaybackFrom(entry, map_name, where, added); !played) {
    return std::unexpected(played.error());
  }
  return added;
}

core::Result<std::optional<LitHours>> HoursFrom(const YAML::Node& entry, std::string_view map_name,
                                                const std::string& where) {
  std::optional<LitHours> hours;
  if (std::string refusal = detail::ReadHours(entry, where, hours); !refusal.empty()) {
    return std::unexpected(Refused(map_name, std::move(refusal)));
  }
  return hours;
}

core::Result<AddedLight> AddedLightFrom(const YAML::Node& entry, std::string_view map_name,
                                        const std::string& where) {
  AddedLight light;
  if (std::string refusal = detail::ReadLight(entry, where, light); !refusal.empty()) {
    return std::unexpected(Refused(map_name, std::move(refusal)));
  }
  return light;
}

// La lueur d'un effet : couleur, portée strictement positive, vacillement.
core::Result<EffectGlow> GlowFrom(const YAML::Node& glow, std::string_view map_name,
                                  const std::string& where) {
  EffectGlow lit;
  if (const YAML::Node color = glow["color"]) {
    core::Result<std::array<float, 3>> tinted =
        VectorFrom(color, map_name, where + " : « glow.color »");
    if (!tinted) {
      return std::unexpected(tinted.error());
    }
    lit.color = *tinted;
  }
  if (const YAML::Node range = glow["range"]) {
    lit.range = range.as<float>();
  }
  if (!(lit.range > 0.0F)) {
    return std::unexpected(Refused(map_name, where + " : « glow.range » doit être positive"));
  }
  if (const YAML::Node flicker = glow["flicker"]) {
    lit.flicker = flicker.as<bool>();
  }
  if (const YAML::Node shadow = glow["shadow"]) {
    lit.shadow = shadow.as<bool>();
  }
  return lit;
}

core::Result<AddedEffect> AddedEffectFrom(const YAML::Node& entry, std::string_view map_name,
                                          const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node number = entry["number"];
  const YAML::Node position = entry["position"];
  const YAML::Node why = entry["why"];
  if (!number || !position || !why) {
    return std::unexpected(
        Refused(map_name, where + " : « number », « position » et « why » sont dus"));
  }
  AddedEffect effect;
  effect.number = number.as<std::int32_t>();
  effect.why = why.as<std::string>();
  if (effect.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> placed =
      VectorFrom(position, map_name, where + " : « position »");
  if (!placed) {
    return std::unexpected(placed.error());
  }
  effect.position = *placed;
  if (const YAML::Node speed = entry["emit_speed"]) {
    effect.emit_speed = speed.as<float>();
  }
  if (const YAML::Node param = entry["param"]) {
    if (!param.IsSequence() || param.size() != effect.param.size()) {
      return std::unexpected(Refused(map_name, where + " : « param » n'a pas quatre composantes"));
    }
    for (std::size_t rank = 0; rank < effect.param.size(); ++rank) {
      effect.param[rank] = param[rank].as<float>();
    }
  }
  if (const YAML::Node night_only = entry["night_only"]) {
    effect.night_only = night_only.as<bool>();
  }
  auto hours = HoursFrom(entry, map_name, where);
  if (!hours) {
    return std::unexpected(hours.error());
  }
  effect.hours = *hours;
  if (const YAML::Node glow = entry["glow"]) {
    core::Result<EffectGlow> lit = GlowFrom(glow, map_name, where);
    if (!lit) {
      return std::unexpected(lit.error());
    }
    effect.glow = *lit;
  }
  return effect;
}

core::Result<EffectEdit> EffectEditFrom(const YAML::Node& entry, std::string_view map_name,
                                        const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node index = entry["index"];
  const YAML::Node number = entry["number"];
  const YAML::Node origin = entry["origin"];
  const YAML::Node why = entry["why"];
  if (!index || !number || !origin || !why) {
    return std::unexpected(
        Refused(map_name, where + " : « index », « number », « origin » et « why » sont dus"));
  }
  EffectEdit edit;
  edit.effect_index = index.as<std::size_t>();
  edit.number = number.as<std::int32_t>();
  edit.why = why.as<std::string>();
  if (edit.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> from = VectorFrom(origin, map_name, where + " : « origin »");
  if (!from) {
    return std::unexpected(from.error());
  }
  edit.origin = *from;
  auto position = OptionalVectorFrom(entry, "position", map_name, where);
  if (!position) {
    return std::unexpected(position.error());
  }
  edit.position = *position;
  if (const YAML::Node hidden = entry["hidden"]) {
    edit.hidden = hidden.as<bool>();
  }
  if (const YAML::Node night_only = entry["night_only"]) {
    edit.night_only = night_only.as<bool>();
  }
  auto hours = HoursFrom(entry, map_name, where);
  if (!hours) {
    return std::unexpected(hours.error());
  }
  edit.hours = *hours;
  if (const YAML::Node glow = entry["glow"]) {
    core::Result<EffectGlow> lit = GlowFrom(glow, map_name, where);
    if (!lit) {
      return std::unexpected(lit.error());
    }
    edit.glow = *lit;
  }
  return edit;
}

core::Result<MeshEdit> MeshEditFrom(const YAML::Node& entry, std::string_view map_name,
                                    const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node index = entry["index"];
  const YAML::Node file = entry["file"];
  const YAML::Node origin = entry["origin"];
  const YAML::Node vertices = entry["vertices"];
  const YAML::Node why = entry["why"];
  if (!index || !file || !origin || !vertices || !why) {
    return std::unexpected(Refused(
        map_name, where + " : « index », « file », « origin », « vertices » et « why » sont dus"));
  }
  MeshEdit edit;
  edit.model_index = index.as<std::size_t>();
  edit.model_file = file.as<std::string>();
  edit.why = why.as<std::string>();
  if (edit.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> placed = VectorFrom(origin, map_name, where + " : « origin »");
  if (!placed) {
    return std::unexpected(placed.error());
  }
  edit.origin = *placed;
  if (const YAML::Node all = entry["all_poses"]) {
    edit.all_poses = all.as<bool>();
  }
  if (!vertices.IsSequence()) {
    return std::unexpected(Refused(map_name, where + " : « vertices » n'est pas une liste"));
  }
  for (std::size_t rank = 0; rank < vertices.size(); ++rank) {
    const YAML::Node vertex = vertices[rank];
    const std::string at = std::format("{}, sommet {}", where, rank);
    if (!vertex.IsMap() || !vertex["node"] || !vertex["vertex"] || !vertex["origin"] ||
        !vertex["position"]) {
      return std::unexpected(
          Refused(map_name, at + " : « node », « vertex », « origin » et « position » sont dus"));
    }
    VertexMove move;
    move.node = vertex["node"].as<std::string>();
    move.vertex = vertex["vertex"].as<std::size_t>();
    auto from = VectorFrom(vertex["origin"], map_name, at + " : « origin »");
    auto to = VectorFrom(vertex["position"], map_name, at + " : « position »");
    if (!from) {
      return std::unexpected(from.error());
    }
    if (!to) {
      return std::unexpected(to.error());
    }
    move.origin = *from;
    move.position = *to;
    edit.SetVertex(std::move(move));
  }
  return edit;
}

// La lueur d'un effet, sous sa clé ; rien sans lueur.
[[nodiscard]] std::string GlowText(const std::optional<EffectGlow>& glow) {
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

}  // namespace

core::Result<RevisedMapEdit> ParseRevisedMapEdit(std::string_view yaml_text,
                                                 std::string_view map_name) {
  RevisedMapEdit revised;
  MapEdit& edit = revised.edit;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused(map_name, "le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused(map_name, "version non gérée"));
    }
    const YAML::Node map = whole["map"];
    if (!map || map.as<std::string>() != map_name) {
      return std::unexpected(Refused(map_name, "« map » ne nomme pas cette carte"));
    }
    if (const YAML::Node revision = whole["revision"]) {
      const auto value = revision.as<std::int64_t>();
      if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(Refused(map_name, "« revision » n'est pas un entier positif"));
      }
      revised.revision = static_cast<std::uint32_t>(value);
    }
    if (const YAML::Node models = whole["models"]) {
      if (!models.IsSequence()) {
        return std::unexpected(Refused(map_name, "« models » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < models.size(); ++rank) {
        core::Result<ModelEdit> model =
            ModelEditFrom(models[rank], map_name, std::format("retouche de modèle {}", rank));
        if (!model) {
          return std::unexpected(model.error());
        }
        edit.SetModel(std::move(*model));
      }
    }
    if (const YAML::Node meshes = whole["meshes"]) {
      if (!meshes.IsSequence()) {
        return std::unexpected(Refused(map_name, "« meshes » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < meshes.size(); ++rank) {
        core::Result<MeshEdit> mesh =
            MeshEditFrom(meshes[rank], map_name, std::format("maillage {}", rank));
        if (!mesh) {
          return std::unexpected(mesh.error());
        }
        edit.SetMesh(std::move(*mesh));
      }
    }
    if (const YAML::Node edits = whole["map_effects"]) {
      if (!edits.IsSequence()) {
        return std::unexpected(Refused(map_name, "« map_effects » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < edits.size(); ++rank) {
        core::Result<EffectEdit> retouch =
            EffectEditFrom(edits[rank], map_name, std::format("retouche d'effet {}", rank));
        if (!retouch) {
          return std::unexpected(retouch.error());
        }
        edit.SetEffectEdit(std::move(*retouch));
      }
    }
    if (const YAML::Node effects = whole["effects"]) {
      if (!effects.IsSequence()) {
        return std::unexpected(Refused(map_name, "« effects » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < effects.size(); ++rank) {
        core::Result<AddedEffect> effect =
            AddedEffectFrom(effects[rank], map_name, std::format("effet {}", rank));
        if (!effect) {
          return std::unexpected(effect.error());
        }
        edit.AddEffect(std::move(*effect));
      }
    }
    if (const YAML::Node lights = whole["lights"]) {
      if (!lights.IsSequence()) {
        return std::unexpected(Refused(map_name, "« lights » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < lights.size(); ++rank) {
        core::Result<AddedLight> light =
            AddedLightFrom(lights[rank], map_name, std::format("lumière {}", rank));
        if (!light) {
          return std::unexpected(light.error());
        }
        edit.AddLight(std::move(*light));
      }
    }
    if (const YAML::Node added = whole["added"]) {
      if (!added.IsSequence()) {
        return std::unexpected(Refused(map_name, "« added » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < added.size(); ++rank) {
        core::Result<AddedModel> model =
            AddedModelFrom(added[rank], map_name, std::format("ajout {}", rank));
        if (!model) {
          return std::unexpected(model.error());
        }
        edit.AddModel(std::move(*model));
      }
    }
    if (const YAML::Node ground = whole["ground"]) {
      if (std::string refusal = detail::ReadGround(ground, edit); !refusal.empty()) {
        return std::unexpected(Refused(map_name, std::move(refusal)));
      }
    }
    if (const YAML::Node sprites = whole["sprites"]) {
      if (std::string refusal = detail::ReadSprites(sprites, edit); !refusal.empty()) {
        return std::unexpected(Refused(map_name, std::move(refusal)));
      }
    }
    if (std::string refusal = detail::ReadModelLights(whole, map_name, edit); !refusal.empty()) {
      return std::unexpected(Refused(map_name, std::move(refusal)));
    }
    if (std::string refusal = detail::ReadSounds(whole, edit); !refusal.empty()) {
      return std::unexpected(Refused(map_name, std::move(refusal)));
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(map_name, failure.what()));
  }
  return revised;
}

std::string RevisedMapEditToYaml(const MapEdit& edit, std::string_view map_name,
                                 std::uint32_t revision) {
  std::string text(detail::kMapEditHeader);
  text += std::format("version: {}\nmap: {}\n", kSupportedVersion, SingleQuoted(map_name));
  if (revision != 0) {
    text += std::format("revision: {}\n", revision);
  }
  text += "models:";
  if (edit.models().empty()) {
    text += " []";
  }
  text += "\n";
  for (const ModelEdit& model : edit.models()) {
    text += std::format("  - index: {}\n", model.model_index);
    text += std::format("    file: {}\n", SingleQuoted(model.model_file));
    text += std::format("    origin: {}\n", VectorText(model.origin));
    if (model.position.has_value()) {
      text += std::format("    position: {}\n", VectorText(*model.position));
    }
    if (model.rotation.has_value()) {
      text += std::format("    rotation: {}\n", VectorText(*model.rotation));
    }
    if (model.scale.has_value()) {
      text += std::format("    scale: {}\n", VectorText(*model.scale));
    }
    if (model.hidden) {
      text += "    hidden: true\n";
    }
    if (model.opacity.has_value()) {
      text += std::format("    opacity: {}\n", *model.opacity);
    }
    if (model.unlit) {
      text += "    unlit: true\n";
    }
    text += std::format("    why: {}\n", DoubleQuoted(model.why));
  }
  if (!edit.meshes().empty()) {
    text += "meshes:\n";
  }
  for (const MeshEdit& mesh : edit.meshes()) {
    text += std::format("  - index: {}\n", mesh.model_index);
    text += std::format("    file: {}\n", SingleQuoted(mesh.model_file));
    text += std::format("    origin: {}\n", VectorText(mesh.origin));
    if (mesh.all_poses) {
      text += "    all_poses: true\n";
    }
    text += "    vertices:\n";
    for (const VertexMove& move : mesh.vertices) {
      text += std::format("      - node: {}\n", SingleQuoted(move.node));
      text += std::format("        vertex: {}\n", move.vertex);
      text += std::format("        origin: {}\n", VectorText(move.origin));
      text += std::format("        position: {}\n", VectorText(move.position));
    }
    text += std::format("    why: {}\n", DoubleQuoted(mesh.why));
  }
  if (!edit.added().empty()) {
    text += "added:\n";
  }
  for (const AddedModel& added : edit.added()) {
    text += std::format("  - file: {}\n", SingleQuoted(added.model_file));
    text += std::format("    position: {}\n", VectorText(added.position));
    text += std::format("    rotation: {}\n", VectorText(added.rotation));
    text += std::format("    scale: {}\n", VectorText(added.scale));
    if (added.hidden) {
      text += "    hidden: true\n";
    }
    if (added.opacity.has_value()) {
      text += std::format("    opacity: {}\n", *added.opacity);
    }
    if (added.unlit) {
      text += "    unlit: true\n";
    }
    if (PlayWord(added.animation_type) != kPlayLooped) {
      text += std::format("    animation: {}\n", PlayWord(added.animation_type));
    }
    if (added.animation_speed != AddedModel{}.animation_speed) {
      text += std::format("    animation_speed: {}\n", added.animation_speed);
    }
    text += std::format("    why: {}\n", DoubleQuoted(added.why));
  }
  if (!edit.effects().empty()) {
    text += "effects:\n";
  }
  for (const AddedEffect& effect : edit.effects()) {
    text += std::format("  - number: {}\n", effect.number);
    text += std::format("    position: {}\n", VectorText(effect.position));
    text += std::format("    emit_speed: {}\n", effect.emit_speed);
    text += std::format("    param: [{}, {}, {}, {}]\n", effect.param[0], effect.param[1],
                        effect.param[2], effect.param[3]);
    if (effect.night_only) {
      text += "    night_only: true\n";
    }
    text += HoursText(effect.hours);
    text += GlowText(effect.glow);
    text += std::format("    why: {}\n", DoubleQuoted(effect.why));
  }
  if (!edit.effect_edits().empty()) {
    text += "map_effects:\n";
  }
  for (const EffectEdit& effect : edit.effect_edits()) {
    text += std::format("  - index: {}\n", effect.effect_index);
    text += std::format("    number: {}\n", effect.number);
    text += std::format("    origin: {}\n", VectorText(effect.origin));
    if (effect.position.has_value()) {
      text += std::format("    position: {}\n", VectorText(*effect.position));
    }
    if (effect.hidden) {
      text += "    hidden: true\n";
    }
    if (effect.night_only) {
      text += "    night_only: true\n";
    }
    text += HoursText(effect.hours);
    text += GlowText(effect.glow);
    text += std::format("    why: {}\n", DoubleQuoted(effect.why));
  }
  if (!edit.lights().empty()) {
    text += "lights:\n";
  }
  for (const AddedLight& light : edit.lights()) {
    text += detail::LightEntryText(light);
  }
  text += detail::ModelLightsYaml(edit);
  text += detail::GroundYaml(edit);
  text += detail::SpritesYaml(edit);
  text += detail::SoundsYaml(edit);
  return text;
}

}  // namespace ro::mapedit
