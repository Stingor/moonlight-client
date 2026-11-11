// Les teintures de calques. Voir l'en-tête.

#include "ro/net/player_style_layers.h"

#include <bit>
#include <format>
#include <set>
#include <utility>

#include <zlib.h>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/net/ui_caps.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/packet_lengths.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Où la trame porte sa longueur : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;
// L'en-tête d'un bloc de ZC : gid, nombre d'entrées.
constexpr std::size_t kBlockHead = 4 + 1;

// Une entrée du CZ : retire cette teinture.
constexpr std::uint8_t kEntryClear = 0x01;
// Une entrée, dans les deux sens : la queue du profil mixte suit les réglages.
constexpr std::uint8_t kEntryTail = 0x02;
// CZ : retire d'abord la tête et la peau.
constexpr std::uint8_t kRequestClearCharacter = 0x01;
// ZC : la charge est compressée.
constexpr std::uint8_t kPayloadZipped = 0x01;

// Le calque d'une entrée, tel que le fil le numérote.
constexpr std::uint8_t kWireHead = static_cast<std::uint8_t>(sim::StyleLayerKind::kHead);
constexpr std::uint8_t kWireLayerLast = static_cast<std::uint8_t>(sim::StyleLayerKind::kBodySkin);

static_assert(proto::CzMoonclientStyleLayers::kWireSize == 6,
              "l'en-tête du CZ des calques ne vaut plus ce que le serveur déclare");
static_assert(proto::ZcMoonclientStyleLayers::kWireSize == 7,
              "l'en-tête du ZC des calques ne vaut plus ce que le serveur déclare");
static_assert(kStyleLayerEntryMost <= UINT8_MAX, "une entrée doit tenir sa taille sur un octet");
// Le pire CZ : tête, peau du visage, quatre suites, huit emplacements, plus la
// peau de chacun des quatre corps.
constexpr std::size_t kRequestEntriesWorst = 1 + 1 + sim::kStyleBodyVariantsMost +
                                             (static_cast<std::size_t>(StyleSlot::kCostumeGarment) -
                                              static_cast<std::size_t>(StyleSlot::kHeadTop) + 1);
static_assert(proto::CzMoonclientStyleLayers::kWireSize +
                      kRequestEntriesWorst * kStyleLayerEntryMost +
                      sim::kStyleBodyVariantsMost * kStyleSkinEntryMost <=
                  kStyleLayersRequestMost,
              "le pire CZ doit tenir sous le plafond du serveur");

// Une peau, du visage ou d'un corps : un seul réglage, ni reste ni retouche.
[[nodiscard]] bool IsSkin(sim::StyleLayerKind layer) noexcept {
  return layer == sim::StyleLayerKind::kSkin || layer == sim::StyleLayerKind::kBodySkin;
}

[[nodiscard]] bool Neutral(const sim::StyleAdjust& adjust) noexcept {
  return adjust.hue == 0 && adjust.saturation == 0 && adjust.value == 0 && !adjust.absolute;
}

// Les rangs qu'un profil emploie ; zéro pour un profil inconnu.
[[nodiscard]] std::size_t RanksOf(std::uint8_t profile) noexcept {
  switch (profile) {
    case kStyleLayerRampProfile:
      return kStyleLayerRampRanks;
    case kStyleLayerHueClassProfile:
      return sim::kStyleLayerAdjustCount;
    default:
      return 0;
  }
}

// Les rangs qu'un calque admet sous un profil. La tête est palettisée, donc au
// profil 1 ; une peau n'a qu'un réglage.
[[nodiscard]] std::size_t RanksFor(sim::StyleLayerKind layer, std::uint8_t profile) noexcept {
  switch (layer) {
    case sim::StyleLayerKind::kHead:
      return profile == kStyleLayerRampProfile ? kStyleLayerRampRanks : 0;
    case sim::StyleLayerKind::kSkin:
    case sim::StyleLayerKind::kBodySkin:
      return profile == kStyleLayerRampProfile ? 1 : 0;
    case sim::StyleLayerKind::kBodyMore:
      return profile == kStyleLayerBodyMoreProfile ? kStyleLayerBodyMoreRanks : 0;
    case sim::StyleLayerKind::kHeadgear:
    case sim::StyleLayerKind::kGarment:
      return RanksOf(profile);
  }
  return 0;
}

[[nodiscard]] std::uint16_t MaskOf(
    const std::array<sim::StyleAdjust, sim::kStyleLayerAdjustCount>& adjusts) noexcept {
  std::uint16_t mask = 0;
  for (std::size_t rank = 0; rank < sim::kStyleLayerAdjustCount; ++rank) {
    if (!Neutral(adjusts[rank])) {
      mask = static_cast<std::uint16_t>(mask | (1U << rank));
    }
  }
  return mask;
}

// Le reste se règle sur les pièces palettisées : tête, coiffe et cape au profil
// 1, suite du corps au profil 3.
[[nodiscard]] bool TakesRest(sim::StyleLayerKind layer, std::uint8_t profile) noexcept {
  if (layer == sim::StyleLayerKind::kBodyMore) {
    return profile == kStyleLayerBodyMoreProfile;
  }
  return !IsSkin(layer) && profile == kStyleLayerRampProfile;
}

// Les retouches : partout sauf sur une peau.
[[nodiscard]] bool TakesRetouches(sim::StyleLayerKind layer) noexcept { return !IsSkin(layer); }

// Des retouches au plus `kStyleRetouchesMost`, permises, d'index strictement
// croissants.
[[nodiscard]] bool RetouchesValid(std::span<const sim::StyleRetouch> retouches) noexcept {
  if (retouches.size() > sim::kStyleRetouchesMost) {
    return false;
  }
  for (std::size_t at = 0; at < retouches.size(); ++at) {
    if (!sim::StyleRetouchAllowed(retouches[at]) ||
        (at > 0 && retouches[at - 1].index >= retouches[at].index)) {
      return false;
    }
  }
  return true;
}

void WriteAdjust(core::ByteWriter& writer, const sim::StyleAdjust& adjust) {
  writer.I16(adjust.hue);
  writer.I8(adjust.saturation);
  writer.I8(adjust.value);
  writer.U8(adjust.absolute ? 1 : 0);
}

[[nodiscard]] core::Result<void> WriteEntry(core::ByteWriter& writer, std::uint8_t wire_layer,
                                            const sim::StyleLayerRecipe& recipe, bool clear);

// Une entrée du CZ, ou la raison de la refuser.
[[nodiscard]] core::Result<void> WriteOrder(core::ByteWriter& writer,
                                            const StyleLayerOrder& order) {
  const sim::StyleLayerRecipe& recipe = order.recipe;
  std::uint8_t wire_layer = 0;
  if (order.slot.has_value()) {
    if (LayerOfSlot(*order.slot) != recipe.layer) {
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "calques : l'emplacement ne porte pas ce calque");
    }
    wire_layer = static_cast<std::uint8_t>(*order.slot);
  } else if (recipe.layer == sim::StyleLayerKind::kHead || IsSkin(recipe.layer) ||
             recipe.layer == sim::StyleLayerKind::kBodyMore) {
    wire_layer = static_cast<std::uint8_t>(recipe.layer);
  } else {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "calques : une teinture d'objet se désigne par son emplacement");
  }
  return WriteEntry(writer, wire_layer, recipe, order.clear);
}

// Une entrée, sous ce numéro de calque, ou la raison de la refuser.
[[nodiscard]] core::Result<void> WriteEntry(core::ByteWriter& writer, std::uint8_t wire_layer,
                                            const sim::StyleLayerRecipe& recipe, bool clear) {
  // La peau du visage sous la clé zéro ; tout autre calque, peau du corps
  // comprise, sous une clé non nulle.
  const bool face_skin = recipe.layer == sim::StyleLayerKind::kSkin;
  if (face_skin ? recipe.key != 0 : recipe.key == 0) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "calques : clé hors de son calque");
  }
  const std::uint16_t mask = clear ? std::uint16_t{0} : MaskOf(recipe.adjusts);
  const std::size_t ranks = RanksFor(recipe.layer, recipe.profile);
  if (ranks == 0 || (mask >> ranks) != 0) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      std::format("calques : profil {} ou rang hors de ce calque", recipe.profile));
  }
  // LA QUEUE, seulement si elle porte quelque chose : une queue vide ne part pas,
  // et deux envois du même style restent identiques à l'octet.
  const std::uint16_t rest_mask = clear ? std::uint16_t{0} : MaskOf(recipe.rest);
  const std::size_t retouches = clear ? 0 : recipe.retouches.size();
  if ((rest_mask != 0 && !TakesRest(recipe.layer, recipe.profile)) ||
      (retouches != 0 && !TakesRetouches(recipe.layer)) ||
      (!clear && !RetouchesValid(recipe.retouches))) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "calques : reste ou retouches hors de ce calque");
  }
  const bool tail = rest_mask != 0 || retouches != 0;
  const auto adjusts = static_cast<std::size_t>(std::popcount(mask));
  const auto rest_adjusts = static_cast<std::size_t>(std::popcount(rest_mask));
  const std::size_t size = kStyleLayerEntryHead + adjusts * kStyleLayerAdjustBytes +
                           (tail ? kStyleLayerTailHead + rest_adjusts * kStyleLayerAdjustBytes +
                                       retouches * kStyleLayerRetouchBytes
                                 : 0);
  writer.U8(static_cast<std::uint8_t>(size));
  writer.U8(wire_layer);
  writer.U32(recipe.key);
  writer.U8(recipe.profile);
  writer.U8(static_cast<std::uint8_t>((clear ? kEntryClear : 0) | (tail ? kEntryTail : 0)));
  writer.U16(mask);
  for (std::size_t rank = 0; rank < sim::kStyleLayerAdjustCount; ++rank) {
    if ((mask & (1U << rank)) != 0) {
      WriteAdjust(writer, recipe.adjusts[rank]);
    }
  }
  if (!tail) {
    return {};
  }
  writer.U16(rest_mask);
  for (std::size_t rank = 0; rank < sim::kStyleLayerAdjustCount; ++rank) {
    if ((rest_mask & (1U << rank)) != 0) {
      WriteAdjust(writer, recipe.rest[rank]);
    }
  }
  writer.U8(static_cast<std::uint8_t>(retouches));
  for (const sim::StyleRetouch& retouch : recipe.retouches) {
    writer.U8(retouch.index);
    writer.U8(retouch.red);
    writer.U8(retouch.green);
    writer.U8(retouch.blue);
  }
  return {};
}

// Une entrée lue : la teinture, ou rien si ce client ne sait pas la lire.
struct ReadEntry {
  std::optional<sim::StyleLayerRecipe> recipe;
  bool ok = false;
};

void ReadAdjust(core::ByteReader& reader, sim::StyleAdjust& adjust) {
  adjust.hue = reader.I16();
  adjust.saturation = reader.I8();
  adjust.value = reader.I8();
  // ⚠ TOUT NON-ZÉRO VAUT « ABSOLU », comme dans la recette du corps.
  adjust.absolute = reader.U8() != 0;
}

// LA QUEUE d'une entrée lue, de `end` exclu. Tout ou rien : une queue qui
// déborde, un reste hors de son profil, une retouche interdite ou en désordre
// la font ignorer, et les pièces restent.
void ReadTail(core::ByteReader& reader, std::size_t end, sim::StyleLayerRecipe& recipe) {
  if (reader.offset() + kStyleLayerTailHead > end) {
    return;
  }
  sim::StyleLayerRecipe read = recipe;
  const std::uint16_t rest_mask = reader.U16();
  const auto rest_adjusts = static_cast<std::size_t>(std::popcount(rest_mask));
  if ((rest_mask >> sim::kStyleLayerAdjustCount) != 0 ||
      (rest_mask != 0 && !TakesRest(recipe.layer, recipe.profile)) ||
      reader.offset() + rest_adjusts * kStyleLayerAdjustBytes + 1 > end) {
    return;
  }
  for (std::size_t rank = 0; rank < sim::kStyleLayerAdjustCount; ++rank) {
    if ((rest_mask & (1U << rank)) != 0) {
      ReadAdjust(reader, read.rest[rank]);
    }
  }
  const std::size_t count = reader.U8();
  if ((count != 0 && !TakesRetouches(recipe.layer)) || count > sim::kStyleRetouchesMost ||
      reader.offset() + count * kStyleLayerRetouchBytes > end) {
    return;
  }
  for (std::size_t at = 0; at < count; ++at) {
    sim::StyleRetouch retouch;
    retouch.index = reader.U8();
    retouch.red = reader.U8();
    retouch.green = reader.U8();
    retouch.blue = reader.U8();
    read.retouches.push_back(retouch);
  }
  if (reader.ok() && RetouchesValid(read.retouches)) {
    recipe = std::move(read);
  }
}

[[nodiscard]] ReadEntry ReadOne(core::ByteReader& reader) {
  const std::size_t start = reader.offset();
  const std::size_t size = reader.U8();
  const std::uint8_t wire_layer = reader.U8();
  const std::uint32_t key = reader.U32();
  const std::uint8_t profile = reader.U8();
  // Seul le drapeau de queue vaut dans un lot.
  const std::uint8_t flags = reader.U8();
  const std::uint16_t mask = reader.U16();
  const auto adjusts = static_cast<std::size_t>(std::popcount(mask));
  // 🔴 LA TAILLE NE SE CROIT PAS SUR PAROLE : elle doit couvrir le masque, et
  // tenir dans ce qui reste.
  if (!reader.ok() || size < kStyleLayerEntryHead + adjusts * kStyleLayerAdjustBytes ||
      reader.remaining() < size - kStyleLayerEntryHead) {
    return {};
  }
  ReadEntry read{.ok = true};
  const bool known_layer = wire_layer >= kWireHead && wire_layer <= kWireLayerLast;
  if (known_layer) {
    sim::StyleLayerRecipe recipe;
    recipe.layer = static_cast<sim::StyleLayerKind>(wire_layer);
    recipe.key = key;
    recipe.profile = profile;
    const std::size_t ranks = RanksFor(recipe.layer, profile);
    if (ranks != 0 && (mask >> ranks) == 0) {
      for (std::size_t rank = 0; rank < sim::kStyleLayerAdjustCount; ++rank) {
        if ((mask & (1U << rank)) == 0) {
          continue;
        }
        ReadAdjust(reader, recipe.adjusts[rank]);
      }
      if ((flags & kEntryTail) != 0) {
        ReadTail(reader, start + size, recipe);
      }
      read.recipe = std::move(recipe);
    }
  }
  // Ce qui suit les réglages, ou toute l'entrée qu'on ne lit pas : enjambé.
  read.ok = reader.Seek(start + size) && reader.ok();
  return read;
}

// La charge d'un ZC, décompressée si elle l'est.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> RawPayload(
    std::uint8_t flags, std::size_t raw_size, std::span<const std::uint8_t> body) {
  if (raw_size > kStyleLayersRawMost) {
    return core::Fail(core::ErrorCode::kCorruptData, "calques : charge annoncée trop grosse");
  }
  if ((flags & kPayloadZipped) == 0) {
    if (body.size() != raw_size) {
      return core::Fail(core::ErrorCode::kCorruptData, "calques : taille brute fausse");
    }
    return std::vector<std::uint8_t>(body.begin(), body.end());
  }
  // 🔴 À LA TAILLE EXACTE ANNONCÉE, ET PLAFONNÉE : une bombe zlib s'arrête au
  // tampon, et un flux qui donne moins est refusé.
  std::vector<std::uint8_t> raw(raw_size);
  uLongf produced = static_cast<uLongf>(raw_size);
  if (raw_size == 0 ||
      uncompress(raw.data(), &produced, body.data(), static_cast<uLong>(body.size())) != Z_OK ||
      produced != raw_size) {
    return core::Fail(core::ErrorCode::kCorruptData,
                      "calques : le flux ne donne pas la taille annoncée");
  }
  return raw;
}

}  // namespace

sim::StyleLayerKind LayerOfSlot(StyleSlot slot) noexcept {
  switch (slot) {
    case StyleSlot::kGarment:
    case StyleSlot::kCostumeGarment:
      return sim::StyleLayerKind::kGarment;
    case StyleSlot::kHeadTop:
    case StyleSlot::kHeadMid:
    case StyleSlot::kHeadLow:
    case StyleSlot::kCostumeHeadTop:
    case StyleSlot::kCostumeHeadMid:
    case StyleSlot::kCostumeHeadLow:
      break;
  }
  return sim::StyleLayerKind::kHeadgear;
}

core::Result<std::vector<std::uint8_t>> EncodeStyleLayers(std::span<const StyleLayerOrder> orders,
                                                          bool clear_character) {
  if (orders.size() > UINT8_MAX || (orders.empty() && !clear_character)) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "calques : nombre d'entrées hors bornes");
  }
  // ⚠ UN CALQUE, UNE ENTRÉE : le serveur refuse la trame qui en répète un.
  std::set<std::pair<std::uint8_t, std::uint32_t>> seen;
  for (const StyleLayerOrder& order : orders) {
    const std::uint8_t target = order.slot.has_value()
                                    ? static_cast<std::uint8_t>(*order.slot)
                                    : static_cast<std::uint8_t>(order.recipe.layer);
    // Un emplacement se désigne une fois, quelle que soit la clé.
    const std::uint32_t key = order.slot.has_value() ? 0 : order.recipe.key;
    if (!seen.emplace(target, key).second) {
      return core::Fail(core::ErrorCode::kInvalidArgument, "calques : calque répété");
    }
  }
  proto::CzMoonclientStyleLayers header;
  header.flags = clear_character ? kRequestClearCharacter : std::uint8_t{0};
  header.count = static_cast<std::uint8_t>(orders.size());
  std::vector<std::uint8_t> frame = proto::Encode(header);
  if (frame.size() != proto::CzMoonclientStyleLayers::kWireSize) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "calques : en-tête illisible");
  }
  core::ByteWriter writer(frame);
  for (const StyleLayerOrder& order : orders) {
    if (core::Result<void> written = WriteOrder(writer, order); !written) {
      return std::unexpected(written.error());
    }
  }
  if (frame.size() > kStyleLayersRequestMost ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(frame.size()))) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      std::format("calques : trame de {} octets, au-delà de {}", frame.size(),
                                  kStyleLayersRequestMost));
  }
  return frame;
}

core::Result<void> SendStyleLayers(Connection& link, std::span<const StyleLayerOrder> orders,
                                   bool clear_character) {
  core::Result<std::vector<std::uint8_t>> frame = EncodeStyleLayers(orders, clear_character);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  if (!link.is_open()) {
    return core::Fail(core::ErrorCode::kIoFailure, "calques : la liaison est fermée");
  }
  link.Send(*frame);
  return {};
}

core::Result<std::vector<StyleLayerBlock>> ReadStyleLayerPayload(
    std::span<const std::uint8_t> raw) {
  std::vector<StyleLayerBlock> blocks;
  core::ByteReader reader(raw);
  while (reader.remaining() > 0) {
    if (reader.remaining() < kBlockHead) {
      return core::Fail(core::ErrorCode::kCorruptData, "calques : bloc tronqué");
    }
    StyleLayerBlock block;
    block.gid = reader.U32();
    const std::size_t count = reader.U8();
    for (std::size_t rank = 0; rank < count; ++rank) {
      const ReadEntry entry = ReadOne(reader);
      if (!entry.ok) {
        return core::Fail(core::ErrorCode::kCorruptData, "calques : entrée tronquée");
      }
      // ⚠ AU-DELÀ DE CE QU'UN JOUEUR AFFICHE, l'entrée est ignorée.
      if (entry.recipe.has_value() && block.layers.size() < sim::kStyleLayersShownMax) {
        block.layers.push_back(*entry.recipe);
      }
    }
    blocks.push_back(std::move(block));
  }
  return blocks;
}

FrameOutcome ApplyStyleLayersFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonclientStyleLayers::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonclientStyleLayers header;
  if (!proto::Decode(frame.bytes, header)) {
    return FrameOutcome::kUndecodable;
  }
  // Un drapeau que ce client ne connaît pas changerait la lecture de la charge.
  if ((header.flags & ~kPayloadZipped) != 0) {
    return FrameOutcome::kIgnored;
  }
  const std::span<const std::uint8_t> body =
      std::span(frame.bytes).subspan(proto::ZcMoonclientStyleLayers::kWireSize);
  core::Result<std::vector<std::uint8_t>> raw = RawPayload(header.flags, header.raw_size, body);
  if (!raw) {
    return FrameOutcome::kUndecodable;
  }
  core::Result<std::vector<StyleLayerBlock>> blocks = ReadStyleLayerPayload(*raw);
  if (!blocks) {
    return FrameOutcome::kUndecodable;
  }
  for (StyleLayerBlock& block : *blocks) {
    if (block.gid != 0) {
      world.styles().RememberLayers(block.gid, std::move(block.layers));
    }
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyStyleVerdictFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcMoonclientStyleVerdict::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonclientStyleVerdict packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  switch (static_cast<sim::StyleSubject>(packet.subject)) {
    case sim::StyleSubject::kAtelier:
      world.styles().SetAtelier(
          sim::StyleAtelier{.open = packet.result != 0, .closes_at_ms = now_ms + packet.detail});
      return FrameOutcome::kApplied;
    case sim::StyleSubject::kBody:
    case sim::StyleSubject::kLayers: {
      sim::StyleVerdict verdict;
      verdict.subject = static_cast<sim::StyleSubject>(packet.subject);
      verdict.raw_result = packet.result;
      // ⚠ UN RÉSULTAT INCONNU SE LIT COMME UN REFUS : rien ne dit qu'il a été
      // retenu.
      verdict.result = packet.result <= sim::kStyleVerdictLastKnown
                           ? static_cast<sim::StyleVerdictResult>(packet.result)
                           : sim::StyleVerdictResult::kUnavailable;
      verdict.wait_ms = packet.wait_ms;
      verdict.evicted_body_key = verdict.subject == sim::StyleSubject::kBody ? packet.detail : 0;
      world.styles().HearVerdict(verdict);
      return FrameOutcome::kApplied;
    }
  }
  return FrameOutcome::kIgnored;
}

core::Result<void> WriteShownStyleLayer(core::ByteWriter& writer,
                                        const sim::StyleLayerRecipe& recipe) {
  return WriteEntry(writer, static_cast<std::uint8_t>(recipe.layer), recipe, false);
}

core::Result<std::optional<sim::StyleLayerRecipe>> ReadShownStyleLayer(core::ByteReader& reader) {
  if (reader.remaining() < kStyleLayerEntryHead) {
    return core::Fail(core::ErrorCode::kCorruptData, "calques : entrée tronquée");
  }
  const ReadEntry entry = ReadOne(reader);
  if (!entry.ok) {
    return core::Fail(core::ErrorCode::kCorruptData, "calques : entrée tronquée");
  }
  return entry.recipe;
}

std::uint32_t StyleLayerCaps() {
  // ⚠ LES TENUES VOYAGENT SOUS LE MÊME BIT : leur réponse doit être connue aussi.
  const bool known = proto::PacketLength(proto::ZcMoonclientStyleLayers::kOpcode).has_value() &&
                     proto::PacketLength(proto::ZcMoonclientStyleVerdict::kOpcode).has_value() &&
                     proto::PacketLength(proto::ZcMoonclientOutfit::kOpcode).has_value();
  return known ? kUiCapStyleLayers : 0U;
}

}  // namespace ro::net
