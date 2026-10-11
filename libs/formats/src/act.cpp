#include "ro/formats/act.h"

#include <cmath>
#include <string_view>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kActSignature{"AC", 2};

// Seuils de version, en majeure × 10 + mineure.
//
// 2.0 apporte teinte, échelle, rotation et son ; 2.1 la table des noms de sons ;
// 2.2 la table des cadences ; 2.3 les ancres ; 2.4 une échelle par axe ; 2.5 les
// dimensions par calque ; 2.6 des décalages en flottants.
constexpr std::int32_t kVersionLayerAttributes = 20;
constexpr std::int32_t kVersionSoundTable = 21;
constexpr std::int32_t kVersionSpeedTable = 22;
constexpr std::int32_t kVersionAnchors = 23;
constexpr std::int32_t kVersionScalePerAxis = 24;
constexpr std::int32_t kVersionLayerSize = 25;
constexpr std::int32_t kVersionFloatOffsets = 26;

// L'en-tête fait 16 octets : signature, version, compte d'actions, puis dix
// octets réservés.
constexpr std::size_t kHeaderSize = 16;

// Octets réservés en tête de chaque image d'animation.
constexpr std::size_t kFrameReservedBytes = 32;

// Une ancre occupe seize octets : quatre réservés, puis x, y et l'attribut.
constexpr std::size_t kAnchorReservedBytes = 4;

// Un nom de fichier son occupe un champ de longueur fixe.
constexpr std::size_t kSoundNameBytes = 40;

// Bornes de bon sens : elles refusent un compte corrompu avant qu'il ne devienne
// une allocation démesurée.
constexpr std::int32_t kMaxActions = 4096;
constexpr std::int32_t kMaxFrames = 4096;
constexpr std::int32_t kMaxLayers = 1024;
constexpr std::int32_t kMaxAnchors = 1024;
constexpr std::int32_t kMaxSounds = 4096;

core::Result<ActLayer> ReadLayer(ByteReader& reader, std::int32_t version) {
  ActLayer layer;
  if (version >= kVersionFloatOffsets) {
    // À partir de 2.6 les décalages sont des flottants, tronqués vers le bas.
    // Les lire en entiers donnerait le motif binaire du flottant, donc des
    // positions absurdes plutôt qu'une erreur.
    layer.offset_x = std::floor(reader.F32());
    layer.offset_y = std::floor(reader.F32());
  } else {
    layer.offset_x = static_cast<float>(reader.I32());
    layer.offset_y = static_cast<float>(reader.I32());
  }
  layer.frame_index = reader.I32();
  layer.mirrored = reader.I32() != 0;

  if (version >= kVersionLayerAttributes) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
      layer.tint[channel] = reader.U8();
    }
    layer.scale_x = reader.F32();
    // Avant 2.4 une seule échelle vaut pour les deux axes ; l'oublier écrase
    // l'échelle verticale à sa valeur par défaut et aplatit le sprite.
    layer.scale_y = layer.scale_x;
    if (version >= kVersionScalePerAxis) {
      layer.scale_y = reader.F32();
    }
    layer.rotation_degrees = reader.I32();
    layer.section = reader.I32();
    if (version >= kVersionLayerSize) {
      // Dimensions déclarées du calque. Elles ne font PAS foi pour composer un
      // acteur — un .act réédité garde parfois d'anciennes tailles, et la seule
      // source vraie est l'image du .spr —, mais le bouton d'image d'une
      // fenêtre pose ses calques par elles : on les garde pour lui.
      layer.declared_width = reader.I32();
      layer.declared_height = reader.I32();
    }
  }
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "calque tronqué");
  }
  // Il n'existe que deux sections ; une valeur hors de là désigne la première,
  // comme dans l'implémentation de référence.
  if (layer.section < 0 || layer.section > 1) {
    layer.section = 0;
  }
  return layer;
}

core::Result<ActFrame> ReadFrame(ByteReader& reader, std::int32_t version, bool read_anchors) {
  reader.Skip(kFrameReservedBytes);
  const std::int32_t layer_count = reader.I32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "compte de calques tronqué");
  }
  if (layer_count < 0 || layer_count > kMaxLayers) {
    return Fail(ErrorCode::kCorruptData, "compte de calques déraisonnable");
  }

  ActFrame frame;
  frame.layers.reserve(static_cast<std::size_t>(layer_count));
  for (std::int32_t index = 0; index < layer_count; ++index) {
    auto layer = ReadLayer(reader, version);
    if (!layer) {
      return std::unexpected(layer.error());
    }
    frame.layers.push_back(*layer);
  }

  if (version >= kVersionLayerAttributes) {
    frame.sound_index = reader.I32();
  }
  if (read_anchors && version >= kVersionAnchors) {
    const std::int32_t anchor_count = reader.I32();
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "compte d'ancres tronqué");
    }
    if (anchor_count < 0 || anchor_count > kMaxAnchors) {
      return Fail(ErrorCode::kCorruptData, "compte d'ancres déraisonnable");
    }
    frame.anchors.reserve(static_cast<std::size_t>(anchor_count));
    for (std::int32_t index = 0; index < anchor_count; ++index) {
      reader.Skip(kAnchorReservedBytes);
      ActAnchor anchor;
      anchor.x = reader.I32();
      anchor.y = reader.I32();
      anchor.attribute = reader.I32();
      if (!reader.ok()) {
        return Fail(ErrorCode::kUnexpectedEnd, "ancre tronquée");
      }
      frame.anchors.push_back(anchor);
    }
  }
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "image d'animation tronquée");
  }
  return frame;
}

// Lit les tables qui suivent les actions : les noms de sons, puis les cadences.
void ReadTrailingTables(ByteReader& reader, std::int32_t version, std::vector<ActAction>& actions,
                        std::vector<std::string>& sound_files) {
  if (version < kVersionSoundTable) {
    return;
  }
  const std::int32_t sound_count = reader.I32();
  if (reader.ok() && sound_count >= 0 && sound_count <= kMaxSounds) {
    sound_files.reserve(static_cast<std::size_t>(sound_count));
    for (std::int32_t index = 0; index < sound_count; ++index) {
      const std::string_view name = reader.FixedString(kSoundNameBytes);
      if (!reader.ok()) {
        break;
      }
      sound_files.emplace_back(name);
    }
  }

  if (version < kVersionSpeedTable) {
    return;
  }
  // La table des cadences est la DERNIÈRE du fichier. Tronquée, elle ne fait
  // pas échouer la lecture : chaque action qu'elle n'atteint pas garde la
  // cadence par défaut, posée à la construction de l'action.
  for (ActAction& action : actions) {
    if (!reader.ok() || reader.remaining() < 4) {
      break;
    }
    action.speed_units = reader.F32();
  }
}

// Une passe de lecture complète. `read_anchors` sert au repli des .act 2.3/2.4.
core::Result<std::vector<ActAction>> ReadActions(std::span<const std::uint8_t> data,
                                                 std::int32_t version, bool read_anchors,
                                                 std::vector<std::string>& sound_files) {
  ByteReader reader(data);
  reader.Skip(4);  // signature et version, déjà lues
  const std::int32_t action_count = static_cast<std::int32_t>(reader.U16());
  if (!reader.Seek(kHeaderSize)) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête .act tronqué");
  }
  if (action_count < 0 || action_count > kMaxActions) {
    return Fail(ErrorCode::kCorruptData, "compte d'actions déraisonnable");
  }

  std::vector<ActAction> actions;
  actions.reserve(static_cast<std::size_t>(action_count));
  for (std::int32_t index = 0; index < action_count; ++index) {
    const std::int32_t frame_count = reader.I32();
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "compte d'images tronqué");
    }
    if (frame_count < 0 || frame_count > kMaxFrames) {
      return Fail(ErrorCode::kCorruptData, "compte d'images déraisonnable");
    }

    ActAction action;
    action.frames.reserve(static_cast<std::size_t>(frame_count));
    for (std::int32_t frame = 0; frame < frame_count; ++frame) {
      auto read = ReadFrame(reader, version, read_anchors);
      if (!read) {
        return std::unexpected(read.error());
      }
      action.frames.push_back(std::move(*read));
    }
    actions.push_back(std::move(action));
  }

  sound_files.clear();
  ReadTrailingTables(reader, version, actions, sound_files);
  return actions;
}

}  // namespace

core::Result<ActAnimation> ActAnimation::Parse(std::span<const std::uint8_t> data) {
  ByteReader header(data);
  if (header.FixedString(kActSignature.size()) != kActSignature) {
    return Fail(ErrorCode::kBadSignature, "signature .act absente");
  }
  const std::uint8_t minor = header.U8();
  const std::uint8_t major = header.U8();
  if (!header.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête .act tronqué");
  }

  ActAnimation animation;
  animation.version_ = static_cast<std::int32_t>(major) * 10 + static_cast<std::int32_t>(minor);

  auto actions = ReadActions(data, animation.version_, true, animation.sound_files_);
  if (actions) {
    animation.actions_ = std::move(*actions);
    return animation;
  }

  // Repli : quelques .act 2.3 et 2.4 déclarent zéro ancre alors que des octets
  // d'ancre suivent quand même, et la lecture déraille en plein milieu. Tout
  // relire sans les ancres est le seul remède connu — voir anchors_dropped().
  if (animation.version_ != kVersionAnchors && animation.version_ != kVersionScalePerAxis) {
    return std::unexpected(actions.error());
  }
  auto without_anchors = ReadActions(data, animation.version_, false, animation.sound_files_);
  if (!without_anchors) {
    return std::unexpected(without_anchors.error());
  }
  animation.actions_ = std::move(*without_anchors);
  animation.anchors_dropped_ = true;
  return animation;
}

}  // namespace ro::formats
