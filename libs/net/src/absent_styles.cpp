// Les styles hors de vue. Voir l'en-tête.

#include "ro/net/absent_styles.h"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/net/player_style_layers.h"
#include "ro/net/player_styles.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Où la trame porte sa longueur : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;
// L'en-tête d'un bloc : char_id, état.
constexpr std::size_t kBlockHead = 4 + 1;
// L'apparence d'un bloc montrable : métier, sexe, coupe, couleur.
constexpr std::size_t kLookBytes = 2 + 1 + 2 + 2;
// Les états d'un bloc.
constexpr std::uint8_t kStateNothing = 0;
constexpr std::uint8_t kStateShown = 1;
// Le seul drapeau de la réponse : la charge est compressée.
constexpr std::uint8_t kPayloadZipped = 0x01;
// Le résultat d'une réponse servie.
constexpr std::uint8_t kServed = 0;

static_assert(proto::CzMoonclientStyleAbsent::kWireSize == 5,
              "l'en-tête du CZ hors de vue ne vaut plus ce que le serveur déclare");
static_assert(proto::ZcMoonclientStyleAbsent::kWireSize == 10,
              "l'en-tête du ZC hors de vue ne vaut plus ce que le serveur déclare");

// Les corps d'un bloc, rangés comme un lot 0x0F27 les range au livre.
[[nodiscard]] bool ReadBodies(core::ByteReader& reader, sim::StyleVariants& variants) {
  const std::size_t count = reader.U8();
  if (!reader.ok() || reader.remaining() < count * kStyleBodyEntryBytes) {
    return false;
  }
  for (std::size_t rank = 0; rank < count; ++rank) {
    const std::optional<StyleBodyEntry> entry = ReadStyleBodyEntry(reader);
    // ⚠ AU-DELÀ DES CORPS QU'UN PERSONNAGE HABILLE, la variante est ignorée.
    if (!entry.has_value() || variants.by_body.size() >= sim::kStyleBodyVariantsMost) {
      continue;
    }
    variants.by_body[entry->body_key] = entry->recipe;
    // La première sert de repli tant qu'aucune n'est marquée.
    if (entry->fallback || variants.fallback_body == 0) {
      variants.fallback_body = entry->body_key;
    }
  }
  return reader.ok();
}

[[nodiscard]] bool ReadLayers(core::ByteReader& reader,
                              std::vector<sim::StyleLayerRecipe>& layers) {
  const std::size_t count = reader.U8();
  if (!reader.ok()) {
    return false;
  }
  for (std::size_t rank = 0; rank < count; ++rank) {
    core::Result<std::optional<sim::StyleLayerRecipe>> entry = ReadShownStyleLayer(reader);
    if (!entry) {
      return false;
    }
    // ⚠ AU-DELÀ DE CE QU'UN JOUEUR AFFICHE, l'entrée est ignorée.
    if (entry->has_value() && layers.size() < sim::kStyleLayersShownMax) {
      layers.push_back(std::move(**entry));
    }
  }
  return true;
}

}  // namespace

bool ReadShownStyle(core::ByteReader& reader, sim::StyleVariants& variants,
                    std::vector<sim::StyleLayerRecipe>& layers) {
  return ReadBodies(reader, variants) && ReadLayers(reader, layers);
}

core::Result<std::vector<std::uint8_t>> EncodeAbsentStylesRequest(
    std::span<const std::uint32_t> char_ids) {
  if (char_ids.empty() || char_ids.size() > kAbsentStyleIdsMost) {
    return core::Fail(
        core::ErrorCode::kInvalidArgument,
        std::format("hors de vue : {} char_id, de 1 à {}", char_ids.size(), kAbsentStyleIdsMost));
  }
  for (std::size_t at = 0; at < char_ids.size(); ++at) {
    // ⚠ LE SERVEUR REFUSE LA TRAME ENTIÈRE pour un zéro ou un doublon.
    if (char_ids[at] == 0 ||
        std::find(char_ids.begin(), char_ids.begin() + static_cast<std::ptrdiff_t>(at),
                  char_ids[at]) != char_ids.begin() + static_cast<std::ptrdiff_t>(at)) {
      return core::Fail(core::ErrorCode::kInvalidArgument, "hors de vue : char_id nul ou répété");
    }
  }
  proto::CzMoonclientStyleAbsent header;
  header.count = static_cast<std::uint8_t>(char_ids.size());
  std::vector<std::uint8_t> frame = proto::Encode(header);
  if (frame.size() != proto::CzMoonclientStyleAbsent::kWireSize) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "hors de vue : en-tête illisible");
  }
  core::ByteWriter writer(frame);
  for (const std::uint32_t char_id : char_ids) {
    writer.U32(char_id);
  }
  if (!writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(frame.size()))) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "hors de vue : longueur illisible");
  }
  return frame;
}

core::Result<void> RequestAbsentStyles(Connection& link, std::span<const std::uint32_t> char_ids) {
  core::Result<std::vector<std::uint8_t>> frame = EncodeAbsentStylesRequest(char_ids);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  if (!link.is_open()) {
    return core::Fail(core::ErrorCode::kIoFailure, "hors de vue : la liaison est fermée");
  }
  link.Send(*frame);
  return {};
}

core::Result<std::vector<AbsentStyleBlock>> ReadAbsentStylePayload(
    std::span<const std::uint8_t> raw) {
  std::vector<AbsentStyleBlock> blocks;
  core::ByteReader reader(raw);
  while (reader.remaining() > 0) {
    if (reader.remaining() < kBlockHead) {
      return core::Fail(core::ErrorCode::kCorruptData, "hors de vue : bloc tronqué");
    }
    AbsentStyleBlock block;
    block.char_id = reader.U32();
    const std::uint8_t state = reader.U8();
    if (state == kStateNothing) {
      blocks.push_back(std::move(block));
      continue;
    }
    // 🔴 UN ÉTAT INCONNU CHANGERAIT LA LECTURE DE LA SUITE : rien n'est appliqué.
    if (state != kStateShown || reader.remaining() < kLookBytes) {
      return core::Fail(core::ErrorCode::kCorruptData, "hors de vue : état inconnu ou tronqué");
    }
    sim::AbsentStyle& style = block.style;
    style.shown = true;
    style.look.job = reader.U16();
    style.look.sex = reader.U8();
    style.look.hair = reader.U16();
    style.look.hair_color = reader.U16();
    if (!ReadBodies(reader, style.variants) || !ReadLayers(reader, style.layers)) {
      return core::Fail(core::ErrorCode::kCorruptData, "hors de vue : bloc tronqué");
    }
    blocks.push_back(std::move(block));
  }
  return blocks;
}

FrameOutcome ApplyAbsentStylesFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcMoonclientStyleAbsent::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonclientStyleAbsent header;
  if (!proto::Decode(frame.bytes, header)) {
    return FrameOutcome::kUndecodable;
  }
  if (header.result != kServed) {
    world.styles().HearAbsentRefusal(sim::AbsentRefusal{
        .result = header.result, .wait_ms = header.wait_ms, .heard_at_ms = now_ms});
    return FrameOutcome::kApplied;
  }
  // Un drapeau que ce client ne connaît pas changerait la lecture de la charge.
  if ((header.flags & ~kPayloadZipped) != 0) {
    return FrameOutcome::kIgnored;
  }
  const std::span<const std::uint8_t> body =
      std::span(frame.bytes).subspan(proto::ZcMoonclientStyleAbsent::kWireSize);
  core::Result<std::vector<std::uint8_t>> raw = StylePayloadOf(header.flags, header.raw_size, body);
  if (!raw) {
    return FrameOutcome::kUndecodable;
  }
  core::Result<std::vector<AbsentStyleBlock>> blocks = ReadAbsentStylePayload(*raw);
  if (!blocks) {
    return FrameOutcome::kUndecodable;
  }
  for (AbsentStyleBlock& block : *blocks) {
    if (block.char_id == 0) {
      continue;
    }
    block.style.heard_at_ms = now_ms;
    world.styles().RememberAbsent(block.char_id, std::move(block.style));
  }
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
