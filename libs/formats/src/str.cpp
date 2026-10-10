#include "ro/formats/str.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kStrSignature{"STRM", 4};

// Ce que l'en-tête réserve après le nombre de couches, et qui ne porte rien.
constexpr std::size_t kReservedBytes = 16;
// Les deux octets qui suivent la version, et qu'aucun fichier n'emploie.
constexpr std::size_t kVersionPaddingBytes = 2;
// Un nom de texture occupe TOUJOURS cent vingt-huit octets, quel qu'il soit.
constexpr std::size_t kTextureNameBytes = 128;

// Le fichier écrit l'angle en 1024e de tour.
constexpr float kAngleUnitsPerTurn = 1024.0F;
constexpr float kDegreesPerTurn = 360.0F;

// « Fram » — les quatre octets du mot, lus comme un entier. Certains fichiers
// les laissent là où va la dernière image ; le nombre n'a alors aucun sens et
// se recalcule sur les images clés.
constexpr std::int32_t kFrameWordInsteadOfCount = 0x6d617246;

// Des plafonds qui ne refusent aucun fichier réel : ils empêchent une valeur
// corrompue de devenir une allocation de plusieurs gibioctets avant que la
// lecture n'ait eu le temps de buter sur la fin des octets.
constexpr std::int32_t kMaxLayers = 4096;
constexpr std::int32_t kMaxTexturesPerLayer = 4096;
constexpr std::int32_t kMaxKeyFramesPerLayer = 1 << 20;

// Un flottant que le fichier écrit infini ou non numérique ne se propage pas :
// il empoisonnerait toute interpolation qui le touche.
float Finite(float value) { return std::isfinite(value) ? value : 0.0F; }

StrKeyFrame ReadKeyFrame(ByteReader& reader) {
  StrKeyFrame key;
  key.frame_index = reader.I32();
  key.type = reader.I32();
  key.offset_x = reader.F32();
  key.offset_y = reader.F32();
  for (float& value : key.uv) {
    value = reader.F32();
  }
  for (float& value : key.corners) {
    value = reader.F32();
  }
  key.texture_index = reader.F32();
  key.animation = static_cast<StrAnimationType>(reader.I32());
  key.delay = Finite(reader.F32());
  key.angle_degrees = Finite(reader.F32()) * (kDegreesPerTurn / kAngleUnitsPerTurn);
  for (float& value : key.color) {
    value = reader.F32();
  }
  key.blend_source = reader.I32();
  key.blend_destination = reader.I32();
  key.multi_texture = reader.I32();
  return key;
}

}  // namespace

core::Result<StrEffect> StrEffect::Parse(std::span<const std::uint8_t> bytes) {
  ByteReader reader(bytes);

  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kStrSignature.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kStrSignature) {
    return Fail(ErrorCode::kBadSignature, "en-tête STR attendu « STRM »");
  }

  StrEffect effect;
  effect.major_version_ = reader.U8();
  effect.minor_version_ = reader.U8();
  reader.Skip(kVersionPaddingBytes);
  effect.frames_per_second_ = reader.I32();
  effect.last_frame_ = reader.I32();
  const std::int32_t layer_count = reader.I32();
  reader.Skip(kReservedBytes);
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête STR tronqué");
  }
  if (layer_count < 0 || layer_count > kMaxLayers) {
    return Fail(ErrorCode::kCorruptData, "nombre de couches STR : " + std::to_string(layer_count));
  }

  effect.layers_.resize(static_cast<std::size_t>(layer_count));
  for (StrLayer& layer : effect.layers_) {
    const std::int32_t texture_count = reader.I32();
    if (!reader.ok() || texture_count < 0 || texture_count > kMaxTexturesPerLayer) {
      return Fail(ErrorCode::kCorruptData, "nombre de textures STR hors bornes");
    }
    layer.texture_names.reserve(reader.AtMostRemaining(static_cast<std::size_t>(texture_count)));
    for (std::int32_t texture = 0; texture < texture_count; ++texture) {
      layer.texture_names.emplace_back(reader.FixedString(kTextureNameBytes));
    }

    const std::int32_t key_count = reader.I32();
    if (!reader.ok() || key_count < 0 || key_count > kMaxKeyFramesPerLayer) {
      return Fail(ErrorCode::kCorruptData, "nombre d'images clés STR hors bornes");
    }
    layer.key_frames.reserve(reader.AtMostRemaining(static_cast<std::size_t>(key_count)));
    for (std::int32_t key = 0; key < key_count; ++key) {
      layer.key_frames.push_back(ReadKeyFrame(reader));
    }
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "couche STR tronquée");
    }
  }

  // La dernière image se recalcule quand le fichier a laissé un mot à sa place.
  if (effect.last_frame_ == kFrameWordInsteadOfCount) {
    effect.last_frame_ = 0;
    for (const StrLayer& layer : effect.layers_) {
      for (const StrKeyFrame& key : layer.key_frames) {
        effect.last_frame_ = std::max(effect.last_frame_, key.frame_index);
      }
    }
  }
  return effect;
}

}  // namespace ro::formats
