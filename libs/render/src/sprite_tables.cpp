// Les tables de sprites. Voir l'en-tête.

#include "ro/render/sprite_tables.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/formats/strict_text.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/pet_tables.h"

namespace ro::render {
namespace {

using formats::StrictText;

// ── Le découpage, par le curseur de forme stricte ──────────────────────────
//
// Une entrée se lit en entier avant d'être rangée : `<id>: <entier>` ou
// `<id>: {<champ>: <valeur>, …}`. Ce découpage ne connaît ni les champs ni leur
// sens.

// La valeur d'un champ.
struct FieldValue {
  enum class Kind : std::uint8_t { kText, kNumber, kBoolean, kList };
  Kind kind = Kind::kText;
  std::string text;
  double number = 0.0;
  bool boolean = false;
  std::vector<std::uint32_t> list;
};

// Un champ : son nom — un nom, ou un entier écrit en chiffres — et sa valeur.
struct Field {
  std::string name;
  FieldValue value;
};

struct Entry {
  std::uint32_t id = 0;
  bool is_table = false;
  std::uint32_t whole = 0;
  std::vector<Field> fields;
};

constexpr std::string_view kVersionKey = "version";
constexpr std::uint32_t kMostWhole = std::numeric_limits<std::uint32_t>::max();

core::Error ValueRefused(std::string what) {
  return core::Error{core::ErrorCode::kCorruptData, std::move(what)};
}

// La valeur d'un champ, ou faux quand le texte n'en porte pas une. Sa forme
// se choisit à son premier caractère, avant de rien franchir : une lecture
// refusée en chemin ne doit pas laisser une autre forme se lire après elle.
bool ReadValue(StrictText& table, FieldValue& value) {
  if (table.Comes('"')) {
    value.kind = FieldValue::Kind::kText;
    return table.ReadQuoted(value.text);
  }
  if (table.Comes('[')) {
    value.kind = FieldValue::Kind::kList;
    return table.ReadWholeList(kMostWhole, value.list);
  }
  if (table.Comes('t') || table.Comes('f')) {
    value.kind = FieldValue::Kind::kBoolean;
    return table.ReadBoolean(value.boolean);
  }
  value.kind = FieldValue::Kind::kNumber;
  return table.ReadNumber(value.number);
}

// Le nom d'un champ : un nom, ou des chiffres.
bool ReadFieldName(StrictText& table, std::string& name) {
  std::string_view key;
  if (table.ReadKey(key)) {
    name = key;
    return true;
  }
  std::uint32_t number = 0;
  if (!table.ReadWhole(kMostWhole, number)) {
    return false;
  }
  name = std::to_string(number);
  return true;
}

// La valeur d'une entrée, l'identifiant et son « : » déjà lus.
core::Result<void> ReadEntryValue(StrictText& table, Entry& entry) {
  if (!table.Take('{')) {
    if (!table.ReadWhole(kMostWhole, entry.whole)) {
      return std::unexpected(table.Refused("une entrée vaut un entier sans signe ou une table"));
    }
    return {};
  }
  entry.is_table = true;
  if (table.Take('}')) {
    return {};
  }
  do {
    Field field;
    if (!ReadFieldName(table, field.name) || !table.Take(':')) {
      return std::unexpected(table.Refused("un champ s'écrit « nom: valeur »"));
    }
    for (const Field& seen : entry.fields) {
      if (seen.name == field.name) {
        return std::unexpected(table.Refused("champ « " + field.name + " » redit"));
      }
    }
    if (!ReadValue(table, field.value)) {
      return std::unexpected(table.Refused(
          "valeur illisible : texte entre guillemets, nombre, booléen ou suite d'entiers attendu"));
    }
    entry.fields.push_back(std::move(field));
  } while (table.Take(','));
  if (!table.Take('}')) {
    return std::unexpected(table.Refused("« , » ou « } » attendu dans une table"));
  }
  return {};
}

// Les sections de chaque fichier.
constexpr std::string_view kClassesSection = "classes";
constexpr std::string_view kHeadgearsSection = "headgears";
constexpr std::string_view kWeaponsSection = "weapons";
constexpr std::string_view kGarmentsSection = "garments";
constexpr std::string_view kInheritedJobsSection = "inherited_jobs";
constexpr std::string_view kBigFemaleSection = "big_female";
constexpr std::string_view kBigMaleSection = "big_male";
constexpr std::string_view kSmallFemaleSection = "small_female";
constexpr std::string_view kSmallMaleSection = "small_male";

// Les champs.
constexpr std::string_view kSpriteField = "sprite";
constexpr std::string_view kRadiusField = "radius";
constexpr std::string_view kShadowField = "shadow";
constexpr std::string_view kSymbolField = "symbol";
constexpr std::string_view kFamilyField = "family";
constexpr std::string_view kHitSoundField = "hit_sound";
constexpr std::string_view kSmallLayerField = "small_layer";

core::Result<void> Refused(std::string what) {
  return std::unexpected(ValueRefused(std::move(what)));
}

core::Result<void> UnknownSection(std::string_view section) {
  return Refused("section « " + std::string(section) + " » inconnue");
}

core::Result<void> Expected(const Field& field, std::string_view what) {
  return Refused("le champ « " + std::string(field.name) + " » n'est pas " + std::string(what));
}

// Une entrée qui doit être une table, et ne pas être redite.
template <typename Table>
core::Result<void> FreshTable(const Entry& entry, const Table& table) {
  if (!entry.is_table) {
    return Refused("l'entrée " + std::to_string(entry.id) + " n'est pas une table");
  }
  if (table.contains(entry.id)) {
    return Refused("l'entrée " + std::to_string(entry.id) + " est redite");
  }
  return {};
}

core::Result<void> PutText(const Field& field, std::uint32_t id,
                           std::unordered_map<std::uint32_t, std::string>& into) {
  if (field.value.kind != FieldValue::Kind::kText) {
    return Expected(field, "un texte");
  }
  into.emplace(id, field.value.text);
  return {};
}

core::Result<void> PutNumber(const Field& field, std::uint32_t id,
                             std::unordered_map<std::uint32_t, double>& into) {
  if (field.value.kind != FieldValue::Kind::kNumber) {
    return Expected(field, "un nombre");
  }
  into.emplace(id, field.value.number);
  return {};
}

// Un nombre qui doit être un entier de 0 à 2^32 - 1.
core::Result<void> PutWhole(const Field& field, std::uint32_t id,
                            std::unordered_map<std::uint32_t, std::uint32_t>& into) {
  const double number = field.value.number;
  if (field.value.kind != FieldValue::Kind::kNumber || number < 0.0 ||
      number > std::numeric_limits<std::uint32_t>::max() || std::floor(number) != number) {
    return Expected(field, "un entier sans signe");
  }
  into.emplace(id, static_cast<std::uint32_t>(number));
  return {};
}

core::Result<void> TakeClass(const Entry& entry, SpriteTables& tables) {
  // Une classe peut ne porter qu'une ombre : la redite se juge sur les trois.
  if (auto fresh = FreshTable(entry, tables.class_sprites); !fresh) {
    return fresh;
  }
  if (tables.class_radii.contains(entry.id) || tables.class_shadows.contains(entry.id)) {
    return Refused("l'entrée " + std::to_string(entry.id) + " est redite");
  }
  for (const Field& field : entry.fields) {
    core::Result<void> put;
    if (field.name == kSpriteField) {
      put = PutText(field, entry.id, tables.class_sprites);
    } else if (field.name == kRadiusField) {
      put = PutNumber(field, entry.id, tables.class_radii);
    } else if (field.name == kShadowField) {
      put = PutNumber(field, entry.id, tables.class_shadows);
    } else {
      put = Refused("champ « " + std::string(field.name) + " » inconnu");
    }
    if (!put) {
      return put;
    }
  }
  return {};
}

// Les champs communs aux coiffes, aux armes et aux robes.
core::Result<void> TakeNamed(const Field& field, std::uint32_t id,
                             std::unordered_map<std::uint32_t, std::string>& sprites,
                             std::unordered_map<std::uint32_t, std::string>& symbols, bool& known) {
  known = true;
  if (field.name == kSpriteField) {
    return PutText(field, id, sprites);
  }
  if (field.name == kSymbolField) {
    return PutText(field, id, symbols);
  }
  known = false;
  return {};
}

core::Result<void> TakeHeadgear(const Entry& entry, SpriteTables& tables) {
  if (auto fresh = FreshTable(entry, tables.headgear_symbols); !fresh) {
    return fresh;
  }
  if (tables.headgear_sprites.contains(entry.id)) {
    return Refused("l'entrée " + std::to_string(entry.id) + " est redite");
  }
  for (const Field& field : entry.fields) {
    bool known = false;
    if (auto put =
            TakeNamed(field, entry.id, tables.headgear_sprites, tables.headgear_symbols, known);
        !put || !known) {
      return !put ? put : Refused("champ « " + std::string(field.name) + " » inconnu");
    }
  }
  return {};
}

core::Result<void> TakeWeapon(const Entry& entry, SpriteTables& tables) {
  if (auto fresh = FreshTable(entry, tables.weapon_symbols); !fresh) {
    return fresh;
  }
  if (tables.weapon_sprites.contains(entry.id) || tables.weapon_families.contains(entry.id) ||
      tables.weapon_hit_sounds.contains(entry.id)) {
    return Refused("l'entrée " + std::to_string(entry.id) + " est redite");
  }
  for (const Field& field : entry.fields) {
    bool known = false;
    core::Result<void> put =
        TakeNamed(field, entry.id, tables.weapon_sprites, tables.weapon_symbols, known);
    if (known) {
      if (!put) {
        return put;
      }
      continue;
    }
    if (field.name == kFamilyField) {
      put = PutWhole(field, entry.id, tables.weapon_families);
    } else if (field.name == kHitSoundField) {
      // Le bruit se rend au client en CP949, comme sa table le donnait.
      if (field.value.kind != FieldValue::Kind::kText) {
        return Expected(field, "un texte");
      }
      auto cp949 = vfs::Utf8ToCp949(field.value.text);
      if (!cp949) {
        return Expected(field, "un texte que le CP949 sait écrire");
      }
      tables.weapon_hit_sounds.emplace(entry.id, std::move(*cp949));
    } else {
      put = Refused("champ « " + std::string(field.name) + " » inconnu");
    }
    if (!put) {
      return put;
    }
  }
  return {};
}

core::Result<void> TakeGarment(const Entry& entry, SpriteTables& tables) {
  if (auto fresh = FreshTable(entry, tables.garment_symbols); !fresh) {
    return fresh;
  }
  if (tables.garment_sprites.contains(entry.id) || tables.small_layer_garments.contains(entry.id)) {
    return Refused("l'entrée " + std::to_string(entry.id) + " est redite");
  }
  for (const Field& field : entry.fields) {
    bool known = false;
    core::Result<void> put =
        TakeNamed(field, entry.id, tables.garment_sprites, tables.garment_symbols, known);
    if (known) {
      if (!put) {
        return put;
      }
      continue;
    }
    if (field.name != kSmallLayerField) {
      return Refused("champ « " + std::string(field.name) + " » inconnu");
    }
    if (field.value.kind != FieldValue::Kind::kBoolean) {
      return Expected(field, "un booléen");
    }
    if (field.value.boolean) {
      tables.small_layer_garments.insert(entry.id);
    }
  }
  return {};
}

// Une table de calques : par métier, {action: [images]}.
core::Result<void> TakeLayers(const Entry& entry, GarmentLayerTable& table) {
  if (auto fresh = FreshTable(entry, table); !fresh) {
    return fresh;
  }
  GarmentLayerFrames& by_action = table[entry.id];
  for (const Field& field : entry.fields) {
    std::uint32_t action = 0;
    const char* const end = field.name.data() + field.name.size();
    if (const auto [stop, error] = std::from_chars(field.name.data(), end, action);
        error != std::errc() || stop != end) {
      return Refused("l'action « " + std::string(field.name) + " » n'est pas un entier");
    }
    if (field.value.kind != FieldValue::Kind::kList) {
      return Expected(field, "une suite d'images");
    }
    if (by_action.contains(action)) {
      return Refused("l'action " + std::to_string(action) + " est redite");
    }
    by_action[action].assign(field.value.list.begin(), field.value.list.end());
  }
  return {};
}

core::Result<void> TakeLayerFile(std::string_view section, const Entry& entry,
                                 SpriteTables& tables) {
  if (section == kInheritedJobsSection) {
    if (entry.is_table) {
      return Refused("un métier hérité s'écrit « métier: métier »");
    }
    if (!tables.layer_inherited_jobs.emplace(entry.id, entry.whole).second) {
      return Refused("l'entrée " + std::to_string(entry.id) + " est redite");
    }
    return {};
  }
  if (section == kBigFemaleSection) {
    return TakeLayers(entry, tables.big_female_layers);
  }
  if (section == kBigMaleSection) {
    return TakeLayers(entry, tables.big_male_layers);
  }
  if (section == kSmallFemaleSection) {
    return TakeLayers(entry, tables.small_female_layers);
  }
  if (section == kSmallMaleSection) {
    return TakeLayers(entry, tables.small_male_layers);
  }
  return UnknownSection(section);
}

core::Result<void> TakeEntry(SpriteTableFile file, std::string_view section, const Entry& entry,
                             SpriteTables& tables) {
  switch (file) {
    case SpriteTableFile::kClasses:
      return section == kClassesSection ? TakeClass(entry, tables) : UnknownSection(section);
    case SpriteTableFile::kHeadgears:
      return section == kHeadgearsSection ? TakeHeadgear(entry, tables) : UnknownSection(section);
    case SpriteTableFile::kWeapons:
      return section == kWeaponsSection ? TakeWeapon(entry, tables) : UnknownSection(section);
    case SpriteTableFile::kGarments:
      return section == kGarmentsSection ? TakeGarment(entry, tables) : UnknownSection(section);
    case SpriteTableFile::kLayers:
      return TakeLayerFile(section, entry, tables);
  }
  return UnknownSection(section);
}

std::string_view PathOf(SpriteTableFile file) {
  for (const SpriteTablePath& known : kSpriteTablePaths) {
    if (known.file == file) {
      return known.path;
    }
  }
  return {};
}

}  // namespace

core::Result<void> ParseSpriteTableText(SpriteTableFile file, std::string_view yaml_text,
                                        SpriteTables& tables) {
  StrictText table(yaml_text, PathOf(file));
  std::string_view key;
  std::uint32_t version = 0;
  if (!table.ReadKey(key) || key != kVersionKey || !table.Take(':') ||
      !table.ReadWhole(kMostWhole, version)) {
    return std::unexpected(table.Refused("« version: <entier> » attendu en tête"));
  }
  if (version != kSpriteTablesVersion) {
    return std::unexpected(table.Refused("version " + std::to_string(version) + " non gérée"));
  }
  std::string_view section;
  while (!table.AtEnd()) {
    // Une section s'ouvre par son nom ; une entrée, par son identifiant.
    if (table.ReadKey(key)) {
      if (!table.Take(':')) {
        return std::unexpected(table.Refused("une section s'ouvre par « nom: »"));
      }
      section = key;
      continue;
    }
    Entry entry;
    if (!table.ReadWhole(kMostWhole, entry.id) || !table.Take(':')) {
      return std::unexpected(table.Refused("une entrée s'écrit « identifiant: valeur »"));
    }
    if (section.empty()) {
      return std::unexpected(table.Refused("entrée hors de toute section"));
    }
    if (auto read = ReadEntryValue(table, entry); !read) {
      return read;
    }
    if (auto taken = TakeEntry(file, section, entry, tables); !taken) {
      return std::unexpected(table.Refused(taken.error().detail));
    }
  }
  return {};
}

core::Result<SpriteTables> ReadSpriteTables(vfs::Vfs& vfs) {
  SpriteTables tables;
  for (const SpriteTablePath& known : kSpriteTablePaths) {
    auto bytes = vfs.Read(std::string(known.path));
    if (!bytes) {
      if (bytes.error().code == core::ErrorCode::kNotFound) {
        continue;
      }
      return std::unexpected(bytes.error());
    }
    if (auto parsed = ParseSpriteTableText(known.file, core::AsText(*bytes), tables); !parsed) {
      return std::unexpected(parsed.error());
    }
  }
  // LE .ACT D'UN ACCESSOIRE DE FAMILIER se lit dans la table des familiers, que
  // la fiche du familier lit aussi.
  auto pet_info = vfs.Read(std::string(vfs::kPetInfoPath));
  if (!pet_info) {
    if (pet_info.error().code != core::ErrorCode::kNotFound) {
      return std::unexpected(pet_info.error());
    }
    return tables;
  }
  vfs::PetTables pets;
  if (auto parsed = vfs::ParsePetInfo(core::AsText(*pet_info), pets); !parsed) {
    return std::unexpected(core::Error{
        parsed.error().code, std::string(vfs::kPetInfoPath) + " : " + parsed.error().detail});
  }
  tables.pet_accessory_acts = std::move(pets.accessory_act_by_item);
  return tables;
}

}  // namespace ro::render
