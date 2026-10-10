#include "ro/render/map_sky.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "ro/formats/lua_table.h"

namespace ro::render {
namespace {

// D'UN STYLE DE CIEL À SON EFFET, tel que le client les associe.
//
// ⚠ LA SUITE A DES TROUS, et ils sont réels : six, douze, treize et quatorze ne
// lancent rien chez le client. Les combler par interpolation — 6 → 6xx par
// exemple — allumerait un ciel qu'aucune carte ne demande, sur un effet qui
// appartient à une compétence.
struct SkyStyleEffect {
  std::uint8_t style;
  std::uint32_t effect_id;
};

constexpr std::array<SkyStyleEffect, 12> kSkyStyleEffects = {{
    {.style = 0, .effect_id = 229},
    {.style = 1, .effect_id = 230},
    {.style = 2, .effect_id = 233},
    {.style = 3, .effect_id = 515},
    {.style = 4, .effect_id = 516},
    {.style = 5, .effect_id = 592},
    {.style = 7, .effect_id = 697},
    {.style = 8, .effect_id = 698},
    {.style = 9, .effect_id = 945},
    {.style = 10, .effect_id = 1045},
    {.style = 11, .effect_id = 1115},
    {.style = 15, .effect_id = 1145},
}};

// La table donne ses composantes de zéro à 255, dans l'ordre rouge, vert, bleu,
// et numérotées à partir de UN comme toute séquence Lua.
constexpr float kByteScale = 255.0F;
constexpr double kRedKey = 1.0;
constexpr double kGreenKey = 2.0;
constexpr double kBlueKey = 3.0;

std::optional<float> Component(const formats::LuaTable& color, double key) {
  const formats::LuaValue* found = color.Find(key);
  if (found == nullptr || found->kind != formats::LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  return static_cast<float>(found->number) / kByteScale;
}

}  // namespace

std::optional<std::array<float, 3>> SkyBackgroundOf(const formats::LuaTable& globals,
                                                    std::string_view map_name) {
  const formats::LuaValue* table = globals.Find(kMapSkyDataGlobal);
  if (table == nullptr || table->table == nullptr) {
    return std::nullopt;
  }
  const formats::LuaValue* entry = table->table->Find(std::string(map_name) + ".rsw");
  if (entry == nullptr || entry->table == nullptr) {
    return std::nullopt;
  }
  const formats::LuaValue* color = entry->table->Find(kBackgroundColorField);
  if (color == nullptr || color->table == nullptr) {
    return std::nullopt;
  }

  const std::optional<float> red = Component(*color->table, kRedKey);
  const std::optional<float> green = Component(*color->table, kGreenKey);
  const std::optional<float> blue = Component(*color->table, kBlueKey);
  // Une couleur à deux composantes n'est pas une couleur : mieux vaut n'en
  // rendre aucune que d'en compléter une au noir sans le dire.
  if (!red.has_value() || !green.has_value() || !blue.has_value()) {
    return std::nullopt;
  }
  return std::array<float, 3>{*red, *green, *blue};
}

std::optional<std::array<float, 3>> LoadSkyBackground(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(std::string(kMapSkyDataPath));
  if (!bytes) {
    return std::nullopt;
  }
  auto globals = formats::ReadLuaChunk(*bytes);
  if (!globals) {
    return std::nullopt;
  }
  return SkyBackgroundOf(*globals, map_name);
}

std::optional<std::uint32_t> SkyEffectIdForStyle(std::uint8_t style) {
  const auto found = std::ranges::find(kSkyStyleEffects, style, &SkyStyleEffect::style);
  if (found == kSkyStyleEffects.end()) {
    return std::nullopt;
  }
  return found->effect_id;
}

MapSkyLook SkyLookOf(const formats::LuaTable& globals, std::string_view map_name) {
  MapSkyLook look;
  look.background = SkyBackgroundOf(globals, map_name);

  const formats::LuaValue* table = globals.Find(kMapSkyDataGlobal);
  if (table == nullptr || table->table == nullptr) {
    return look;
  }
  const formats::LuaValue* entry = table->table->Find(std::string(map_name) + ".rsw");
  if (entry == nullptr || entry->table == nullptr) {
    return look;
  }

  // ⚠ LE CHAMP D'ÉTOILES D'ABORD, comme le client : il le lance AVANT de
  // parcourir la liste des nuages. L'ordre décide de ce qui se peint par-dessus
  // quand une carte porte les deux.
  const formats::LuaValue* star = entry->table->Find(kStarEffectField);
  if (star != nullptr && star->kind == formats::LuaValue::Kind::kBoolean && star->boolean) {
    if (const std::optional<std::uint32_t> id = SkyEffectIdForStyle(kStarSkyStyle)) {
      look.effects.push_back(*id);
    }
  }

  // La liste des styles, numérotée à partir de UN comme toute séquence Lua.
  if (const formats::LuaValue* clouds = entry->table->Find(kOldCloudEffectField);
      clouds != nullptr && clouds->table != nullptr) {
    for (double index = 1.0;; index += 1.0) {
      const formats::LuaValue* style = clouds->table->Find(index);
      if (style == nullptr || style->kind != formats::LuaValue::Kind::kNumber) {
        break;
      }
      // ⚠ UN STYLE INCONNU NE S'INVENTE PAS : le client tombe sur le défaut de
      // son switch et ne lance rien. Le nôtre non plus.
      if (const std::optional<std::uint32_t> id =
              SkyEffectIdForStyle(static_cast<std::uint8_t>(style->number))) {
        look.effects.push_back(*id);
      }
    }
  }

  if (const formats::LuaValue* parameterised = entry->table->Find(kCloudEffectField);
      parameterised != nullptr && parameterised->table != nullptr) {
    for (double index = 1.0;; index += 1.0) {
      const formats::LuaValue* one = parameterised->table->Find(index);
      if (one == nullptr) {
        break;
      }
      ++look.parameterised_clouds;
    }
  }

  const formats::LuaValue* fog = entry->table->Find(kBackgroundFogField);
  look.background_fog =
      fog != nullptr && fog->kind == formats::LuaValue::Kind::kBoolean && fog->boolean;
  return look;
}

MapSkyLook LoadMapSky(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(std::string(kMapSkyDataPath));
  if (!bytes) {
    return {};
  }
  auto globals = formats::ReadLuaChunk(*bytes);
  if (!globals) {
    return {};
  }
  return SkyLookOf(*globals, map_name);
}

}  // namespace ro::render
