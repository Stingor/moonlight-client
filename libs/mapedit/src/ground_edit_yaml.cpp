#include <cstdint>
#include <format>
#include <string>

#include "ground_yaml.h"

namespace ro::mapedit::detail {
namespace {

// Un point : sa colonne, sa rangée, sa levée.
constexpr std::size_t kPointFields = 3;

}  // namespace

std::string DoubleQuoted(std::string_view text) {
  std::string quoted = "\"";
  for (const char letter : text) {
    if (letter == '"' || letter == '\\') {
      quoted += '\\';
      quoted += letter;
    } else if (letter == '\n') {
      quoted += "\\n";
    } else {
      quoted += letter;
    }
  }
  return quoted + "\"";
}

std::string SingleQuoted(std::string_view text) {
  std::string quoted = "'";
  for (const char letter : text) {
    quoted += letter;
    if (letter == '\'') {
      quoted += '\'';
    }
  }
  return quoted + "'";
}

std::string VectorText(const std::array<float, 3>& vector) {
  return std::format("[{}, {}, {}]", vector[0], vector[1], vector[2]);
}

std::string ReadGround(const YAML::Node& ground, MapEdit& edit) {
  if (!ground.IsMap()) {
    return "« ground » n'est pas une table";
  }
  const YAML::Node why = ground["why"];
  const YAML::Node points = ground["points"];
  if (!why || !points) {
    return "« ground » : « why » et « points » sont dus";
  }
  if (!points.IsSequence()) {
    return "« ground » : « points » n'est pas une liste";
  }
  for (std::size_t rank = 0; rank < points.size(); ++rank) {
    const YAML::Node point = points[rank];
    if (!point.IsSequence() || point.size() != kPointFields) {
      return std::format("« ground », point {} : [colonne, rangée, levée] attendu", rank);
    }
    edit.LiftGround(GroundEdit{.corner_x = point[0].as<std::int32_t>(),
                               .corner_y = point[1].as<std::int32_t>(),
                               .lift = point[2].as<float>()});
  }
  edit.SetGroundWhy(why.as<std::string>());
  if (!edit.ground().empty() && edit.ground_why().empty()) {
    return "« ground » : « why » est vide";
  }
  return {};
}

std::string GroundYaml(const MapEdit& edit) {
  if (edit.ground().empty()) {
    return {};
  }
  std::string text =
      std::format("ground:\n  why: {}\n  points:\n", DoubleQuoted(edit.ground_why()));
  for (const GroundEdit& point : edit.ground()) {
    text += std::format("    - [{}, {}, {}]\n", point.corner_x, point.corner_y, point.lift);
  }
  return text;
}

}  // namespace ro::mapedit::detail
