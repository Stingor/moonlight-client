// Les recettes de la tablée. Voir l'en-tête.

#include "ro/net/char_select_styles.h"

#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/net/absent_styles.h"
#include "ro/net/player_style_layers.h"
#include "ro/net/session.h"
#include "ro/proto/common_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::net {
namespace {

// Les drapeaux de la réponse : la charge est compressée ; une autre trame suit.
constexpr std::uint8_t kPayloadZipped = 0x01;
constexpr std::uint8_t kAnswerContinues = 0x02;
constexpr std::uint8_t kKnownFlags = kPayloadZipped | kAnswerContinues;
// L'en-tête d'un bloc : le char_id.
constexpr std::size_t kBlockHead = 4;
// Les personnages d'un compte que la tablée assied, au plus : au-delà, les blocs
// s'ignorent.
constexpr std::size_t kCharactersMost = 15;

static_assert(proto::ChMoonclientCharStyles::kWireSize == 6,
              "la demande de la tablée ne vaut plus ce que le serveur déclare");
static_assert(proto::HcMoonclientCharStyles::kWireSize == 7,
              "l'en-tête de la réponse de la tablée ne vaut plus ce que le serveur déclare");

}  // namespace

std::vector<std::uint8_t> EncodeCharSelectStylesRequest() {
  proto::ChMoonclientCharStyles request;
  request.caps = kCharSelectStylesReads;
  return proto::Encode(request);
}

bool CharSelectStylesKnown() {
  return proto::PacketLength(proto::HcMoonclientCharStyles::kOpcode).has_value();
}

core::Result<std::vector<CharSelectStyle>> ReadCharSelectStylePayload(
    std::span<const std::uint8_t> raw) {
  std::vector<CharSelectStyle> blocks;
  core::ByteReader reader(raw);
  while (reader.remaining() > 0) {
    if (reader.remaining() < kBlockHead) {
      return core::Fail(core::ErrorCode::kCorruptData, "tablée : bloc tronqué");
    }
    CharSelectStyle block;
    block.char_id = reader.U32();
    if (!ReadShownStyle(reader, block.variants, block.layers)) {
      return core::Fail(core::ErrorCode::kCorruptData, "tablée : bloc tronqué");
    }
    if (block.char_id != 0 && blocks.size() < kCharactersMost) {
      blocks.push_back(std::move(block));
    }
  }
  return blocks;
}

core::Result<CharSelectStylesAnswer> ReadCharSelectStylesFrame(const Frame& frame) {
  proto::HcMoonclientCharStyles header;
  if (frame.opcode != proto::HcMoonclientCharStyles::kOpcode ||
      !proto::Decode(frame.bytes, header)) {
    return core::Fail(core::ErrorCode::kCorruptData, "tablée : en-tête illisible");
  }
  // Un drapeau que ce client ne connaît pas changerait la lecture de la charge.
  if ((header.flags & ~kKnownFlags) != 0) {
    return core::Fail(core::ErrorCode::kCorruptData, "tablée : drapeau inconnu");
  }
  const std::span<const std::uint8_t> body =
      std::span(frame.bytes).subspan(proto::HcMoonclientCharStyles::kWireSize);
  core::Result<std::vector<std::uint8_t>> raw = StylePayloadOf(
      static_cast<std::uint8_t>(header.flags & kPayloadZipped), header.raw_size, body);
  if (!raw) {
    return std::unexpected(raw.error());
  }
  core::Result<std::vector<CharSelectStyle>> blocks = ReadCharSelectStylePayload(*raw);
  if (!blocks) {
    return std::unexpected(blocks.error());
  }
  return CharSelectStylesAnswer{.blocks = std::move(*blocks),
                                .more = (header.flags & kAnswerContinues) != 0};
}

// ── LA PHASE DE PERSONNAGES ─────────────────────────────────────────────────

void CharacterPhase::RequestStyles() {
  // ⚠ SEULEMENT AU CHOIX, code PIN passé : avant, le serveur ferme sur tout
  // paquet qu'il n'attend pas.
  if (styles_asked_ || step_ != Step::kAwaitingSelection || !CharSelectStylesKnown() ||
      !link_.is_open()) {
    return;
  }
  styles_asked_ = true;
  link_.Send(EncodeCharSelectStylesRequest());
}

std::vector<CharSelectStylesAnswer> CharacterPhase::TakeStyles() {
  return std::exchange(styles_, {});
}

bool CharacterPhase::TookStyles(const Frame& frame) {
  if (frame.opcode != proto::HcMoonclientCharStyles::kOpcode) {
    return false;
  }
  // ⚠ UNE RÉPONSE ILLISIBLE N'EST PAS FATALE : la tablée garde son cache.
  if (core::Result<CharSelectStylesAnswer> answer = ReadCharSelectStylesFrame(frame)) {
    styles_.push_back(std::move(*answer));
  }
  return true;
}

}  // namespace ro::net
