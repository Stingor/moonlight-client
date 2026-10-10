#include "ro/render/map_effect_table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace ro::render {
namespace {

constexpr std::string_view kEffectToolFolder = "data\\luafiles514\\lua files\\effecttool\\";
// Le signe des cartes d'instance, que le nom d'une globale ne peut pas porter.
constexpr char kInstanceMark = '@';

// Les familles que le client va chercher dans la table d'une carte, dans son
// ordre. Les deux premières sont portées ; les autres se comptent.
constexpr std::string_view kEmitterFamily = "emitterInfo";
constexpr std::string_view kWindFamily = "windEffectInfo";
constexpr std::array<std::string_view, 4> kOtherFamilies = {"magicfloorInfo", "evilsPawInfo",
                                                            "animatedEmitterInfo", "ez2strInfo"};
constexpr std::string_view kVersionFamily = "effect_version";
// Le suffixe que lit l'ancien lecteur pour savoir s'il transmet `zenable`.
constexpr std::string_view kOldReaderVersionSuffix = "_version";

// 🔴 TROIS LECTEURS SELON LA VERSION, et le choix est celui du client : sous
// deux, l'ANCIEN lecteur, qui ne connaît que les émetteurs ; à deux, le lecteur
// complet ; au-delà, RIEN.
constexpr std::uint32_t kFullReaderVersion = 2;
// L'ancien lecteur ne transmet `zenable` que si la table déclare cette version.
constexpr double kOldReaderZenableVersion = 1.0;

// Le plafond d'une composante de couleur d'émetteur.
constexpr std::int32_t kColorCeiling = 255;

// Un champ LU : présent ET nombre. `luaL_checknumber` du client lève une erreur
// sinon, ce qui interrompt la lecture.
[[nodiscard]] std::optional<double> NumberOf(const formats::LuaValue* value) {
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  return value->number;
}

[[nodiscard]] const formats::LuaTable* TableField(const formats::LuaTable& entry,
                                                  std::string_view name) {
  const formats::LuaValue* found = entry.Find(name);
  if (found == nullptr || found->table == nullptr) {
    return nullptr;
  }
  return found->table.get();
}

// Les `count` premières composantes d'une séquence Lua, qui commence à UN.
template <std::size_t Count>
[[nodiscard]] std::optional<std::array<double, Count>> Sequence(const formats::LuaTable& entry,
                                                                std::string_view name) {
  const formats::LuaTable* sequence = TableField(entry, name);
  if (sequence == nullptr) {
    return std::nullopt;
  }
  std::array<double, Count> values{};
  for (std::size_t index = 0; index < Count; ++index) {
    const std::optional<double> value = NumberOf(sequence->Find(static_cast<double>(index + 1)));
    if (!value.has_value()) {
      return std::nullopt;
    }
    values[index] = *value;
  }
  return values;
}

[[nodiscard]] glm::vec3 VectorOf(const std::array<double, 3>& values) {
  return {static_cast<float>(values[0]), static_cast<float>(values[1]),
          static_cast<float>(values[2])};
}

// La conversion du client vers un entier : troncature vers zéro.
[[nodiscard]] std::int32_t Truncated(double value) { return static_cast<std::int32_t>(value); }

// Lit une entrée d'émetteur, message par message, dans l'ordre du lecteur Lua.
// Rend faux au premier champ obligatoire qui manque : les champs lus avant
// restent posés, comme chez le client.
[[nodiscard]] bool ReadEmitterEntry(const formats::LuaTable& entry, bool full_reader,
                                    bool old_reader_sends_zenable, MapEmitterSetting& setting) {
  const auto count = Sequence<1>(entry, "maxcount");
  if (!count) {
    return false;
  }
  setting.max_count = Truncated((*count)[0]);

  const auto first = Sequence<3>(entry, "dir1");
  if (!first) {
    return false;
  }
  setting.first_direction = VectorOf(*first);
  const auto second = Sequence<3>(entry, "dir2");
  if (!second) {
    return false;
  }
  setting.second_direction = VectorOf(*second);
  const auto gravity = Sequence<3>(entry, "gravity");
  if (!gravity) {
    return false;
  }
  setting.gravity = VectorOf(*gravity);
  const auto radius = Sequence<3>(entry, "radius");
  if (!radius) {
    return false;
  }
  setting.radius = VectorOf(*radius);

  const auto color = Sequence<4>(entry, "color");
  if (!color) {
    return false;
  }
  for (std::size_t channel = 0; channel < setting.color.size(); ++channel) {
    // ⚠ BORNÉ PAR LE HAUT SEULEMENT : le client ne relève pas un négatif.
    setting.color[channel] = std::min(Truncated((*color)[channel]), kColorCeiling);
  }

  // ⚠ LE LECTEUR PASSE TROIS VALEURS, le client n'en lit que DEUX : une
  // troisième absente n'est pas une faute.
  const auto rate = Sequence<2>(entry, "rate");
  if (!rate) {
    return false;
  }
  setting.rate = {static_cast<float>((*rate)[0]), static_cast<float>((*rate)[1])};
  const auto size = Sequence<2>(entry, "size");
  if (!size) {
    return false;
  }
  setting.size = {static_cast<float>((*size)[0]), static_cast<float>((*size)[1])};
  const auto life = Sequence<2>(entry, "life");
  if (!life) {
    return false;
  }
  setting.life = {static_cast<float>((*life)[0]), static_cast<float>((*life)[1])};

  const formats::LuaValue* texture = entry.Find("texture");
  if (texture == nullptr || texture->kind != formats::LuaValue::Kind::kString) {
    return false;
  }
  setting.texture = texture->text;

  const auto speed = Sequence<1>(entry, "speed");
  if (!speed) {
    return false;
  }
  setting.speed = static_cast<float>((*speed)[0]);
  const auto source = Sequence<1>(entry, "srcmode");
  if (!source) {
    return false;
  }
  setting.blend_source = Truncated((*source)[0]);
  const auto destination = Sequence<1>(entry, "destmode");
  if (!destination) {
    return false;
  }
  setting.blend_destination = Truncated((*destination)[0]);

  if (full_reader || old_reader_sends_zenable) {
    const auto zenable = Sequence<1>(entry, "zenable");
    if (!zenable) {
      return false;
    }
    setting.depth_tested = Truncated((*zenable)[0]) != 0;
  }
  if (!full_reader) {
    return true;
  }

  // LES QUATRE CHAMPS FACULTATIFS : le lecteur regarde s'ils existent avant de
  // les transmettre. Présents, leurs composantes restent obligatoires.
  if (entry.Find("billboard_off") != nullptr) {
    const auto flat = Sequence<1>(entry, "billboard_off");
    if (!flat) {
      return false;
    }
    setting.flat = Truncated((*flat)[0]) != 0;
  }
  if (entry.Find("rotate_angle") != nullptr) {
    const auto rotation = Sequence<3>(entry, "rotate_angle");
    if (!rotation) {
      return false;
    }
    setting.rotation_degrees = VectorOf(*rotation);
  }
  if (entry.Find("eternity") != nullptr) {
    const auto eternal = Sequence<1>(entry, "eternity");
    if (!eternal) {
      return false;
    }
    // ⚠ ÉGAL À UN, et non différent de zéro, contrairement aux deux autres
    // drapeaux : c'est la comparaison que le client fait en posant l'émetteur.
    setting.eternal = Truncated((*eternal)[0]) == 1;
  }
  if (entry.Find("scale") != nullptr) {
    const auto scale = Sequence<2>(entry, "scale");
    if (!scale) {
      return false;
    }
    setting.scale = {static_cast<float>((*scale)[0]), static_cast<float>((*scale)[1])};
  }
  return true;
}

[[nodiscard]] std::optional<double> ScalarField(const formats::LuaTable& entry,
                                                std::string_view name) {
  return NumberOf(entry.Find(name));
}

// Lit une entrée de vent. Tous ses champs sont obligatoires, et SCALAIRES —
// sauf la place, la couleur et la bascule.
[[nodiscard]] bool ReadWindEntry(const formats::LuaTable& entry, MapWindSetting& setting) {
  const auto position = Sequence<3>(entry, "pos");
  if (!position) {
    return false;
  }
  setting.position = VectorOf(*position);
  const auto rings = ScalarField(entry, "particleNum");
  if (!rings) {
    return false;
  }
  // Un mot de seize bits, par la conversion non signée du client.
  setting.rings = static_cast<std::uint16_t>(static_cast<std::uint64_t>(*rings));
  const auto color = Sequence<4>(entry, "color");
  if (!color) {
    return false;
  }
  // Des OCTETS : la troncature entière, puis les huit bits bas.
  setting.alpha_peak = static_cast<std::uint8_t>(Truncated((*color)[0]));
  for (std::size_t channel = 0; channel < setting.rgb.size(); ++channel) {
    setting.rgb[channel] = static_cast<std::uint8_t>(Truncated((*color)[channel + 1]));
  }
  const auto radius = ScalarField(entry, "radius");
  const auto thickness = ScalarField(entry, "thickness");
  const auto height = ScalarField(entry, "height");
  const auto speed = ScalarField(entry, "speed");
  const auto full_angle = ScalarField(entry, "fullAngle");
  if (!radius || !thickness || !height || !speed || !full_angle) {
    return false;
  }
  setting.radius = static_cast<float>(*radius);
  setting.thickness = static_cast<float>(*thickness);
  setting.height = static_cast<float>(*height);
  setting.speed = static_cast<float>(*speed);
  setting.full_angle = static_cast<std::uint16_t>(static_cast<std::uint64_t>(*full_angle));
  const auto tilt = Sequence<2>(entry, "rotateVector");
  if (!tilt) {
    return false;
  }
  setting.tilt_x_degrees = static_cast<float>((*tilt)[0]);
  setting.tilt_z_degrees = static_cast<float>((*tilt)[1]);
  const auto source = ScalarField(entry, "srcMode");
  const auto destination = ScalarField(entry, "destMode");
  if (!source || !destination) {
    return false;
  }
  setting.blend_source = static_cast<std::int32_t>(static_cast<std::uint64_t>(*source));
  setting.blend_destination = static_cast<std::int32_t>(static_cast<std::uint64_t>(*destination));
  const formats::LuaValue* texture = entry.Find("texture");
  if (texture == nullptr || texture->kind != formats::LuaValue::Kind::kString) {
    return false;
  }
  setting.texture = texture->text;
  return true;
}

// La clé d'une entrée, telle que le client la convertit : un entier.
[[nodiscard]] std::int32_t KeyOf(double key) { return static_cast<std::int32_t>(key); }

}  // namespace

std::string MapEffectToolPath(std::string_view map_name) {
  return std::string(kEffectToolFolder) + std::string(map_name) + ".lub";
}

std::string MapEffectToolGlobal(std::string_view map_name, std::string_view family) {
  std::string global = "_" + std::string(map_name) + "_" + std::string(family);
  // 🔴 LE PREMIER `@` DISPARAÎT DU NOM, et seulement lui : un `@` n'a pas sa
  // place dans un identifiant Lua. La table de `1@adv` s'appelle donc
  // `_1adv_emitterInfo` — alors que son FICHIER, lui, garde le `@`.
  if (const std::size_t at = global.find(kInstanceMark); at != std::string::npos) {
    global.erase(at, 1);
  }
  return global;
}

MapEffectTable ReadMapEffectTable(const formats::LuaTable& globals, std::string_view map_name) {
  MapEffectTable table;
  table.present = true;

  // ⚠ UNE VERSION ABSENTE VAUT ZÉRO, donc l'ancien lecteur : le client part
  // d'un entier nul et ne l'écrit que si la globale existe.
  std::uint32_t version = 0;
  if (const std::optional<double> declared =
          NumberOf(globals.Find(MapEffectToolGlobal(map_name, kVersionFamily)))) {
    version = static_cast<std::uint32_t>(static_cast<std::int64_t>(*declared));
  }
  table.version = version;
  if (version > kFullReaderVersion) {
    return table;
  }
  const bool full_reader = version == kFullReaderVersion;
  const std::string emitter_global = MapEffectToolGlobal(map_name, kEmitterFamily);
  const bool old_reader_sends_zenable =
      NumberOf(globals.Find(emitter_global + std::string(kOldReaderVersionSuffix))) ==
      kOldReaderZenableVersion;

  if (const formats::LuaValue* list = globals.Find(emitter_global);
      list != nullptr && list->table != nullptr) {
    for (const auto& [key, entry] : list->table->by_index) {
      if (entry.table == nullptr) {
        ++table.malformed_emitters;
        continue;
      }
      MapEmitterSetting setting;
      if (!ReadEmitterEntry(*entry.table, full_reader, old_reader_sends_zenable, setting)) {
        ++table.malformed_emitters;
      }
      table.emitters.insert_or_assign(KeyOf(key), std::move(setting));
    }
  }
  if (!full_reader) {
    return table;
  }

  if (const formats::LuaValue* list = globals.Find(MapEffectToolGlobal(map_name, kWindFamily));
      list != nullptr && list->table != nullptr) {
    for (const auto& [key, entry] : list->table->by_index) {
      if (entry.table == nullptr) {
        ++table.malformed_winds;
        continue;
      }
      MapWindSetting setting;
      if (!ReadWindEntry(*entry.table, setting)) {
        ++table.malformed_winds;
      }
      table.winds.insert_or_assign(KeyOf(key), std::move(setting));
    }
  }
  for (const std::string_view family : kOtherFamilies) {
    if (const formats::LuaValue* list = globals.Find(MapEffectToolGlobal(map_name, family));
        list != nullptr && list->table != nullptr) {
      table.other_tables.emplace(std::string(family),
                                 list->table->by_index.size() + list->table->by_name.size());
    }
  }
  return table;
}

std::optional<formats::LuaTable> LoadMapEffectToolGlobals(vfs::Vfs& vfs,
                                                          std::string_view map_name) {
  auto bytes = vfs.Read(MapEffectToolPath(map_name));
  if (!bytes) {
    return std::nullopt;
  }
  auto globals = formats::ReadLuaFile(*bytes, formats::LuaTable{});
  if (!globals) {
    return std::nullopt;
  }
  return std::move(*globals);
}

}  // namespace ro::render
