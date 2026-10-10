#include "ro/formats/named_effects.h"

#include <algorithm>
#include <format>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/formats/bson.h"

namespace ro::formats {
namespace {

// Les clés d'une fiche, telles que le client les demande une à une.
constexpr std::string_view kFilePathKey = "FilePath";
constexpr std::string_view kSoundPathKey = "SoundPath";
constexpr std::string_view kPosXKey = "PosX";
constexpr std::string_view kPosYKey = "PosY";
constexpr std::string_view kStartDelayKey = "StartDelayTime";
constexpr std::string_view kQuakeDelayKey = "QuakeDelay";
constexpr std::string_view kInfiniteKey = "IsInfinite";
constexpr std::string_view kFloorKey = "IsFloor";
constexpr std::string_view kAboveUiKey = "IsAboveUI";
constexpr std::string_view kSoundLoopKey = "IsSoundLoop";
constexpr std::string_view kToGroundKey = "IsToGround";

// Un champ absent, ou d'une autre sorte que celle attendue, rend le défaut.
//
// Une fiche incomplète ne coûte que ce qu'elle décrit : elle ne se joue pas, ou
// se joue sans son. Refuser le fichier entier pour elle priverait le joueur des
// deux cent vingt-deux autres.
std::string TextOf(const BsonDocument& record, std::string_view key) {
  const BsonValue* field = record.Find(key);
  return field != nullptr && field->kind == BsonValue::Kind::kText ? field->text : std::string();
}

bool FlagOf(const BsonDocument& record, std::string_view key) {
  const BsonValue* field = record.Find(key);
  return field != nullptr && field->kind == BsonValue::Kind::kBoolean && field->boolean;
}

std::int32_t IntegerOf(const BsonDocument& record, std::string_view key, std::int32_t fallback) {
  const BsonValue* field = record.Find(key);
  return field != nullptr && field->kind == BsonValue::Kind::kInteger ? field->integer : fallback;
}

float NumberOf(const BsonDocument& record, std::string_view key) {
  const BsonValue* field = record.Find(key);
  // Le client rétrécit lui-même ces deux doubles en flottants avant de les
  // écrire dans la position de l'effet.
  return field != nullptr && field->kind == BsonValue::Kind::kNumber
             ? static_cast<float>(field->number)
             : 0.0F;
}

}  // namespace

core::Result<NamedEffectTable> NamedEffectTable::Parse(std::span<const std::uint8_t> bytes) {
  core::Result<BsonDocument> root = ParseBson(bytes);
  if (!root) {
    return core::Fail(root.error().code, std::move(root.error().detail));
  }
  const BsonValue* table = root->Find(kNamedEffectsRoot);
  if (table == nullptr || table->kind != BsonValue::Kind::kDocument) {
    return core::Fail(core::ErrorCode::kCorruptData,
                      std::format("le document {} manque à la racine", kNamedEffectsRoot));
  }

  NamedEffectTable parsed;
  parsed.entries_.reserve(table->document->fields.size());
  for (const std::pair<std::string, BsonValue>& field : table->document->fields) {
    // Une fiche qui n'est pas un document veut dire que ce fichier n'est pas
    // celui que l'on croit : là, c'est la STRUCTURE qui a menti, et il n'y a
    // rien à sauver.
    if (field.second.kind != BsonValue::Kind::kDocument) {
      return core::Fail(core::ErrorCode::kCorruptData,
                        std::format("la fiche {} n'est pas un document", field.first));
    }
    const BsonDocument& record = *field.second.document;
    NamedEffect entry;
    entry.name = core::AsciiLower(field.first);
    entry.file = TextOf(record, kFilePathKey);
    entry.sound = TextOf(record, kSoundPathKey);
    entry.offset_x = NumberOf(record, kPosXKey);
    entry.offset_y = NumberOf(record, kPosYKey);
    entry.start_delay_ms = IntegerOf(record, kStartDelayKey, 0);
    entry.quake_delay_ticks = IntegerOf(record, kQuakeDelayKey, -1);
    entry.is_infinite = FlagOf(record, kInfiniteKey);
    entry.is_floor = FlagOf(record, kFloorKey);
    entry.is_above_ui = FlagOf(record, kAboveUiKey);
    entry.is_sound_loop = FlagOf(record, kSoundLoopKey);
    entry.is_to_ground = FlagOf(record, kToGroundKey);
    parsed.entries_.push_back(std::move(entry));
  }
  if (parsed.entries_.empty()) {
    return core::Fail(core::ErrorCode::kCorruptData, "aucun effet nommé");
  }
  // STABLE : deux noms qui s'abaissent sur le même gardent leur ordre du
  // fichier, et la recherche rend alors le premier — celui que le client garde,
  // son insertion ne remplaçant pas une clé déjà présente.
  std::ranges::stable_sort(parsed.entries_, {}, &NamedEffect::name);
  return parsed;
}

const NamedEffect* NamedEffectTable::Find(std::string_view name) const {
  const std::string key = core::AsciiLower(name);
  const auto found = std::ranges::lower_bound(entries_, key, {}, &NamedEffect::name);
  if (found == entries_.end() || found->name != key) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::formats
