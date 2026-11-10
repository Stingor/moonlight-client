// Les tenues : code court et garde-robe. Voir l'en-tête.

#include "ro/net/style_outfits.h"

#include <algorithm>
#include <format>
#include <set>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/net/player_style_layers.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Où la trame porte sa longueur : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;
// L'alphabet des codes, le même que celui du serveur : ni 0 ni O, ni 1 ni I ni L.
constexpr std::string_view kCodeAlphabet = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
// Un code se montre en deux groupes.
constexpr std::size_t kCodeGroup = 4;
constexpr char kCodeSeparator = '-';
// Un rangement listé commence par son numéro et la longueur de son nom.
constexpr std::size_t kListedHead = 1 + 1;
// Le premier octet imprimable d'un nom, et le DEL.
constexpr std::uint8_t kNameFirstPrintable = 0x20;
constexpr std::uint8_t kNameDelete = 0x7F;

static_assert(proto::CzMoonclientOutfit::kWireSize == 15,
              "l'en-tête du CZ des tenues ne vaut plus ce que le serveur déclare");
static_assert(proto::ZcMoonclientOutfit::kWireSize == 17,
              "l'en-tête du ZC des tenues ne vaut plus ce que le serveur déclare");
static_assert(kOutfitBodyBytes == 4 + 3 * 2 + sim::kStyleRampCount * kStyleLayerAdjustBytes,
              "un corps de tenue ne vaut plus une recette v7");

void WriteAdjust(core::ByteWriter& writer, const sim::StyleAdjust& adjust) {
  writer.I16(adjust.hue);
  writer.I8(adjust.saturation);
  writer.I8(adjust.value);
  writer.U8(adjust.absolute ? 1 : 0);
}

[[nodiscard]] sim::StyleAdjust ReadAdjust(core::ByteReader& reader) {
  sim::StyleAdjust adjust;
  adjust.hue = reader.I16();
  adjust.saturation = reader.I8();
  adjust.value = reader.I8();
  adjust.absolute = reader.U8() != 0;
  return adjust;
}

[[nodiscard]] bool NameOk(std::string_view name) {
  if (name.empty() || name.size() > kOutfitNameMost) {
    return false;
  }
  return std::ranges::all_of(name, [](char c) {
    const auto byte = static_cast<std::uint8_t>(c);
    return byte >= kNameFirstPrintable && byte != kNameDelete;
  });
}

// Ce qu'une tenue trop grosse abandonne, dans l'ordre : ses retouches, puis le
// reste de ses calques. Les pièces restent toujours.
enum class OutfitTrim : std::uint8_t { kNothing, kRetouches, kRest };

[[nodiscard]] sim::StyleOutfit Trimmed(const sim::StyleOutfit& outfit, OutfitTrim trim) {
  sim::StyleOutfit trimmed = outfit;
  for (sim::StyleLayerRecipe& layer : trimmed.layers) {
    if (trim != OutfitTrim::kNothing) {
      layer.retouches.clear();
    }
    if (trim == OutfitTrim::kRest) {
      layer.rest = {};
    }
  }
  return trimmed;
}

[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeWhole(const sim::StyleOutfit& outfit);

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodeOutfit(const sim::StyleOutfit& outfit) {
  // 🔴 UNE TENUE TIENT DANS SON PLAFOND : trop grosse, elle perd d'abord les
  // retouches de ses calques, puis leur reste.
  for (const OutfitTrim trim : {OutfitTrim::kNothing, OutfitTrim::kRetouches, OutfitTrim::kRest}) {
    core::Result<std::vector<std::uint8_t>> raw = EncodeWhole(Trimmed(outfit, trim));
    if (!raw || raw->size() <= kOutfitBytesMost) {
      return raw;
    }
  }
  return core::Fail(core::ErrorCode::kInvalidArgument,
                    std::format("tenue : au-delà de {} octets", kOutfitBytesMost));
}

namespace {

core::Result<std::vector<std::uint8_t>> EncodeWhole(const sim::StyleOutfit& outfit) {
  if (outfit.empty() || outfit.bodies.size() > sim::kStyleBodyVariantsMost ||
      outfit.layers.size() > kOutfitEntriesMost) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : nombre de corps ou d'entrées");
  }
  std::set<std::uint32_t> keys;
  for (const auto& [key, recipe] : outfit.bodies) {
    if (key == 0 || !keys.insert(key).second) {
      return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : clé de corps nulle ou répétée");
    }
  }
  if (outfit.fallback_body != 0 && !keys.contains(outfit.fallback_body)) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : le repli n'est pas un corps");
  }
  std::vector<std::uint8_t> raw;
  core::ByteWriter writer(raw);
  writer.U8(kOutfitVersion);
  writer.U8(static_cast<std::uint8_t>(outfit.bodies.size()));
  writer.U32(outfit.fallback_body);
  for (const auto& [key, recipe] : outfit.bodies) {
    writer.U32(key);
    writer.I16(recipe.body_palette);
    writer.I16(recipe.hair_palette);
    writer.I16(recipe.hair_style);
    for (const sim::StyleAdjust& adjust : recipe.ramps) {
      WriteAdjust(writer, adjust);
    }
  }
  writer.U8(static_cast<std::uint8_t>(outfit.layers.size()));
  std::set<std::pair<sim::StyleLayerKind, std::uint32_t>> seen;
  for (const sim::StyleLayerRecipe& layer : outfit.layers) {
    if (!seen.emplace(layer.layer, layer.key).second) {
      return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : calque répété");
    }
    if (core::Result<void> written = WriteShownStyleLayer(writer, layer); !written) {
      return std::unexpected(written.error());
    }
  }
  return raw;
}

}  // namespace

core::Result<sim::StyleOutfit> DecodeOutfit(std::span<const std::uint8_t> raw) {
  if (raw.size() < kOutfitHead || raw.size() > kOutfitBytesMost) {
    return core::Fail(core::ErrorCode::kCorruptData, "tenue : taille hors bornes");
  }
  core::ByteReader reader(raw);
  if (reader.U8() != kOutfitVersion) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "tenue : version inconnue");
  }
  const std::size_t bodies = reader.U8();
  sim::StyleOutfit outfit;
  outfit.fallback_body = reader.U32();
  if (bodies > sim::kStyleBodyVariantsMost || reader.remaining() < bodies * kOutfitBodyBytes) {
    return core::Fail(core::ErrorCode::kCorruptData, "tenue : corps tronqués");
  }
  for (std::size_t rank = 0; rank < bodies; ++rank) {
    const std::uint32_t key = reader.U32();
    sim::StyleRecipe recipe;
    recipe.body_palette = reader.I16();
    recipe.hair_palette = reader.I16();
    recipe.hair_style = reader.I16();
    for (sim::StyleAdjust& adjust : recipe.ramps) {
      adjust = ReadAdjust(reader);
    }
    outfit.bodies.emplace_back(key, recipe);
  }
  const std::size_t count = reader.U8();
  if (!reader.ok() || count > kOutfitEntriesMost) {
    return core::Fail(core::ErrorCode::kCorruptData, "tenue : entrées hors bornes");
  }
  for (std::size_t rank = 0; rank < count; ++rank) {
    core::Result<std::optional<sim::StyleLayerRecipe>> entry = ReadShownStyleLayer(reader);
    if (!entry) {
      return std::unexpected(entry.error());
    }
    if (entry->has_value()) {
      outfit.layers.push_back(**entry);
    }
  }
  if (!reader.ok() || reader.remaining() != 0) {
    return core::Fail(core::ErrorCode::kCorruptData, "tenue : octets en trop");
  }
  return outfit;
}

std::optional<std::string> NormalizeOutfitCode(std::string_view typed) {
  std::string code;
  for (const char c : typed) {
    if (c == ' ' || c == kCodeSeparator) {
      continue;
    }
    const char upper = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    if (kCodeAlphabet.find(upper) == std::string_view::npos ||
        code.size() == sim::kOutfitCodeLength) {
      return std::nullopt;
    }
    code.push_back(upper);
  }
  if (code.size() != sim::kOutfitCodeLength) {
    return std::nullopt;
  }
  return code;
}

std::string ShowOutfitCode(std::string_view code) {
  if (code.size() != sim::kOutfitCodeLength) {
    return std::string(code);
  }
  return std::format("{}{}{}", code.substr(0, kCodeGroup), kCodeSeparator, code.substr(kCodeGroup));
}

core::Result<std::vector<std::uint8_t>> EncodeOutfitRequest(const OutfitRequest& request) {
  const sim::OutfitAction action = request.action;
  const bool with_slot =
      action == sim::OutfitAction::kWardrobeStore || action == sim::OutfitAction::kWardrobeClear;
  const bool with_code = action == sim::OutfitAction::kFetch;
  const bool with_name = action == sim::OutfitAction::kWardrobeStore;
  const bool with_outfit =
      action == sim::OutfitAction::kPublish || action == sim::OutfitAction::kWardrobeStore;
  if (action < sim::OutfitAction::kPublish || action > sim::OutfitAction::kWardrobeClear) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : action inconnue");
  }
  if (with_slot && request.slot >= kWardrobeSlotsMost) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : rangement hors bornes");
  }
  if (with_code && NormalizeOutfitCode(request.code) != request.code) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : code mal formé");
  }
  if (with_name && !NameOk(request.name)) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "tenue : nom vide, trop long ou illisible");
  }
  std::vector<std::uint8_t> outfit;
  if (with_outfit) {
    if (!request.outfit.has_value()) {
      return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : rien à envoyer");
    }
    core::Result<std::vector<std::uint8_t>> encoded = EncodeOutfit(*request.outfit);
    if (!encoded) {
      return std::unexpected(encoded.error());
    }
    outfit = std::move(*encoded);
  }
  // 🔴 TOUT CHAMP INUTILE À L'ACTION PART NUL : le serveur refuse le contraire.
  proto::CzMoonclientOutfit header;
  header.action = static_cast<std::uint8_t>(action);
  header.slot = with_slot ? request.slot : std::uint8_t{0};
  header.code = with_code ? request.code : std::string();
  header.name_length = with_name ? static_cast<std::uint8_t>(request.name.size()) : std::uint8_t{0};
  std::vector<std::uint8_t> frame = proto::Encode(header);
  if (frame.size() != proto::CzMoonclientOutfit::kWireSize) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : en-tête illisible");
  }
  core::ByteWriter writer(frame);
  if (with_name) {
    writer.Text(request.name);
  }
  writer.Bytes(outfit);
  if (!writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(frame.size()))) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "tenue : trame illisible");
  }
  return frame;
}

core::Result<void> SendOutfitRequest(Connection& link, const OutfitRequest& request) {
  core::Result<std::vector<std::uint8_t>> frame = EncodeOutfitRequest(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  if (!link.is_open()) {
    return core::Fail(core::ErrorCode::kIoFailure, "tenue : la liaison est fermée");
  }
  link.Send(*frame);
  return {};
}

FrameOutcome ApplyOutfitFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonclientOutfit::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonclientOutfit header;
  if (!proto::Decode(frame.bytes, header)) {
    return FrameOutcome::kUndecodable;
  }
  const std::span<const std::uint8_t> payload =
      std::span(frame.bytes).subspan(proto::ZcMoonclientOutfit::kWireSize);
  sim::OutfitReply reply;
  reply.action = static_cast<sim::OutfitAction>(header.action);
  reply.raw_result = header.result;
  reply.wait_ms = header.wait_ms;
  reply.slot = header.slot;
  reply.code = header.code;
  std::vector<sim::StyleWardrobeEntry> wardrobe;
  if (reply.accepted() && reply.action == sim::OutfitAction::kFetch) {
    core::Result<sim::StyleOutfit> outfit = DecodeOutfit(payload);
    if (!outfit) {
      return FrameOutcome::kUndecodable;
    }
    reply.outfit = std::move(*outfit);
  }
  if (reply.accepted() && reply.action == sim::OutfitAction::kWardrobeList) {
    core::ByteReader reader(payload);
    const std::size_t count = reader.U8();
    if (!reader.ok() || count > kWardrobeSlotsMost) {
      return FrameOutcome::kUndecodable;
    }
    for (std::size_t rank = 0; rank < count; ++rank) {
      sim::StyleWardrobeEntry entry;
      if (reader.remaining() < kListedHead) {
        return FrameOutcome::kUndecodable;
      }
      entry.slot = reader.U8();
      const std::size_t name_length = reader.U8();
      entry.name = std::string(reader.FixedString(name_length));
      const std::size_t outfit_length = reader.U16();
      const std::span<const std::uint8_t> raw = reader.Bytes(outfit_length);
      if (!reader.ok() || entry.slot >= kWardrobeSlotsMost) {
        return FrameOutcome::kUndecodable;
      }
      core::Result<sim::StyleOutfit> outfit = DecodeOutfit(raw);
      if (!outfit) {
        // Un rangement illisible se tait ; les autres restent.
        continue;
      }
      entry.outfit = std::move(*outfit);
      wardrobe.push_back(std::move(entry));
    }
    if (reader.remaining() != 0) {
      return FrameOutcome::kUndecodable;
    }
  }
  world.outfits().Hear(std::move(reply), std::move(wardrobe));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
