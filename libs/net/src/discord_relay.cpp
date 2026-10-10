// LE RELAI DU CANAL DISCORD. Voir l'en-tête.

#include "ro/net/discord_relay.h"

#include <algorithm>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_mention.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// L'opcode, puis la longueur annoncée, en tête de trame.
constexpr std::size_t kOpcodeBytes = 2;

// La forme d'une ligne relayée, « [#canal][Nom] texte », telle que 0x0F08
// l'écrit.
constexpr std::string_view kChannelOpen = "[#";
constexpr std::string_view kNameOpen = "][";
constexpr std::string_view kNameClose = "] ";

// Ce qu'un chemin d'avatar ne porte pas : il sortirait du CDN.
constexpr char kPathSeparator = '/';
constexpr std::string_view kParentStep = "..";
constexpr std::string_view kDoubleSeparator = "//";

}  // namespace

bool IsDiscordRelayMap(std::string_view map_name) {
  return core::AsciiStartsWithFolded(map_name, kDiscordRelayMapPrefix);
}

// 🔴 LE TEXTE SE LIT À LA MAIN : le décodeur engendré ne prend qu'UN octet du
// message, faute de savoir que sa queue va jusqu'à la longueur annoncée.
std::optional<std::string> ReadDiscordRelayText(std::span<const std::uint8_t> frame) {
  if (frame.size() < kDiscordRelayHeader) {
    return std::nullopt;
  }
  core::ByteReader reader(frame);
  reader.Skip(kOpcodeBytes);
  const std::size_t announced = reader.U16();
  if (!reader.ok() || announced < kDiscordRelayHeader) {
    return std::nullopt;
  }
  // ⚠ LA LONGUEUR ANNONCÉE NE SE CROIT PAS AU-DELÀ DE CE QUI EST ARRIVÉ.
  const std::size_t stop = std::min(announced, frame.size());
  const std::span<const std::uint8_t> body =
      frame.subspan(kDiscordRelayHeader, stop - kDiscordRelayHeader);
  const auto nul = std::ranges::find(body, std::uint8_t{0});
  const auto length = std::min(static_cast<std::size_t>(nul - body.begin()), kDiscordRelayTextMax);
  std::string_view bytes(reinterpret_cast<const char*>(body.data()), length);
  // 🔴 UN MESSAGE COUPÉ À LA BORNE PERD SA SÉQUENCE ENTAMÉE AVANT LA CONVERSION :
  // sans cela, un texte ASCII suivi d'un octet de tête orphelin ne passerait
  // pas pour de l'UTF-8, et repartirait entier en 1252 avec un « ð » en queue.
  if (length == kDiscordRelayTextMax) {
    bytes = core::WithoutDanglingUtf8(bytes);
  }
  // Le serveur relaie de l'UTF-8 ; un message écrit en 1252 se lit quand même.
  return core::Utf8FromWire(bytes);
}

std::optional<DiscordRelayAuthor> FindDiscordRelayAuthor(std::string_view text) {
  // Le lecteur est celui des mentions : un seul comportement pour les deux.
  const std::string_view name = sim::DiscordRelayAuthorName(text);
  if (name.empty()) {
    return std::nullopt;
  }
  return DiscordRelayAuthor{.offset = static_cast<std::size_t>(name.data() - text.data()),
                            .length = name.size()};
}

bool IsDiscordAvatarPath(std::string_view path) {
  if (path.empty() || path.front() == kPathSeparator || path.find(kParentStep) != path.npos ||
      path.find(kDoubleSeparator) != path.npos) {
    return false;
  }
  return std::ranges::all_of(path, [](char letter) {
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
           (letter >= '0' && letter <= '9') || letter == '_' || letter == '.' ||
           letter == kPathSeparator || letter == '-';
  });
}

std::string DiscordAvatarUrl(std::string_view path) {
  return IsDiscordAvatarPath(path) ? std::string(kDiscordCdnAddress) + std::string(path)
                                   : std::string();
}

std::optional<DiscordRichLine> ReadDiscordRichLine(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightDiscordRich fixed;
  if (!proto::Decode(frame, fixed)) {
    return std::nullopt;
  }
  const std::size_t filled = proto::ZcMoonlightDiscordRich::kWireSize +
                             std::size_t{fixed.channel_len} + std::size_t{fixed.name_len} +
                             std::size_t{fixed.avatar_len} + std::size_t{fixed.text_len};
  if (filled != frame.size() || fixed.text_len > kDiscordRichTextMax || fixed.channel_len == 0 ||
      fixed.name_len == 0) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> tail =
      frame.subspan(proto::ZcMoonlightDiscordRich::kWireSize);
  if (std::ranges::find(tail, std::uint8_t{0}) != tail.end()) {
    return std::nullopt;
  }
  const std::string_view rest(reinterpret_cast<const char*>(tail.data()), tail.size());
  const std::string_view channel = rest.substr(0, fixed.channel_len);
  const std::string_view name = rest.substr(fixed.channel_len, fixed.name_len);
  const std::string_view avatar = rest.substr(channel.size() + name.size(), fixed.avatar_len);
  const std::string_view said = rest.substr(channel.size() + name.size() + avatar.size());

  // Le serveur relaie de l'UTF-8 ; un morceau écrit en 1252 se lit quand même.
  // 🔴 LES DÉCALAGES SE COMPTENT APRÈS LA CONVERSION, morceau par morceau.
  DiscordRichLine line;
  line.author.name = core::Utf8FromWire(name);
  line.text = std::string(kChannelOpen) + core::Utf8FromWire(channel) + std::string(kNameOpen);
  line.author.name_offset = static_cast<std::uint32_t>(line.text.size());
  line.author.name_length = static_cast<std::uint32_t>(line.author.name.size());
  line.text += line.author.name + std::string(kNameClose);
  line.author.said_offset = static_cast<std::uint32_t>(line.text.size());
  line.text += core::Utf8FromWire(said);
  line.author.discord_user_id = fixed.discord_user_id;
  if (IsDiscordAvatarPath(avatar)) {
    line.author.avatar_path = std::string(avatar);
  }
  return line;
}

FrameOutcome ApplyDiscordRelayFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  std::optional<std::string> text;
  std::optional<sim::ChatAuthor> author;
  if (frame.opcode == proto::ZcMoonlightDiscordMsg::kOpcode) {
    text = ReadDiscordRelayText(frame.bytes);
  } else if (frame.opcode == proto::ZcMoonlightDiscordRich::kOpcode) {
    if (std::optional<DiscordRichLine> rich = ReadDiscordRichLine(frame.bytes)) {
      text = std::move(rich->text);
      author = std::move(rich->author);
    }
  } else {
    return FrameOutcome::kUnhandled;
  }
  if (!text) {
    return FrameOutcome::kUndecodable;
  }
  // 🔴 LE FILTRE EST ICI ET NULLE PART AILLEURS : le serveur envoie à toute la
  // carte, réglage coché ou non. La ligne jetée ne revient pas quand on recoche.
  if (text->empty() || !world.session().discord_chat || !IsDiscordRelayMap(world.map_name())) {
    return FrameOutcome::kIgnored;
  }
  world.RecordChat(sim::ChatKind::kPublic, sim::ChatType::kDiscord, 0, std::move(*text), now_ms,
                   kDiscordRelayRgb, std::move(author));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
