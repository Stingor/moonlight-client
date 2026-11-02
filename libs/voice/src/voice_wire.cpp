#include "ro/voice/voice_wire.h"

#include <algorithm>
#include <bit>
#include <string_view>
#include <utility>

#include "ro/core/text.h"

namespace ro::voice {
namespace {

using core::ErrorCode;
using core::Fail;

constexpr std::uint8_t kFrameMagicHigh = 0x56;  // 'V'
constexpr std::uint8_t kFrameMagicLow = 0x54;   // 'T'

// Les octets de l'en-tête audio reçu, dans l'ordre du fil.
constexpr std::size_t kHeardVolumeAt = 4;
constexpr std::size_t kHeardXAt = 8;
constexpr std::size_t kHeardYAt = 12;
constexpr std::size_t kHeardNameAt = 16;
constexpr std::size_t kHeardSequenceAt = 40;

// L'en-tête d'un audio envoyé par le TCP : canal, groupe, numéro.
constexpr std::size_t kTcpVoiceHeaderBytes = 7;

void PutU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8) {
    out.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void PutU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    out.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

[[nodiscard]] std::uint16_t ReadU16(std::span<const std::uint8_t> bytes, std::size_t at) {
  return static_cast<std::uint16_t>((bytes[at] << 8) | bytes[at + 1]);
}

[[nodiscard]] std::uint32_t ReadU32(std::span<const std::uint8_t> bytes, std::size_t at) {
  return (std::uint32_t{bytes[at]} << 24) | (std::uint32_t{bytes[at + 1]} << 16) |
         (std::uint32_t{bytes[at + 2]} << 8) | std::uint32_t{bytes[at + 3]};
}

void PutUdpPrefix(std::vector<std::uint8_t>& out, UdpKind kind) {
  PutU32(out, kUdpMagic);
  out.push_back(kUdpVersion);
  out.push_back(static_cast<std::uint8_t>(kind));
}

void PutUdpIdentity(std::vector<std::uint8_t>& out, const UdpIdentity& identity) {
  PutU64(out, identity.session_id);
  PutU32(out, identity.character_id);
  PutU64(out, identity.token);
}

}  // namespace

std::vector<std::uint8_t> EncodeFrame(FrameKind kind, std::span<const std::uint8_t> payload) {
  std::vector<std::uint8_t> out;
  out.reserve(kFrameHeaderBytes + payload.size());
  out.push_back(kFrameMagicHigh);
  out.push_back(kFrameMagicLow);
  out.push_back(kFrameVersion);
  out.push_back(static_cast<std::uint8_t>(kind));
  PutU32(out, static_cast<std::uint32_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

void FrameReader::Feed(std::span<const std::uint8_t> bytes) {
  pending_.insert(pending_.end(), bytes.begin(), bytes.end());
}

core::Result<std::optional<StreamFrame>> FrameReader::Next() {
  if (pending_.size() < kFrameHeaderBytes) {
    return std::optional<StreamFrame>();
  }
  const std::span<const std::uint8_t> head(pending_);
  if (head[0] != kFrameMagicHigh || head[1] != kFrameMagicLow || head[2] != kFrameVersion) {
    return Fail(ErrorCode::kBadSignature, "trame voix sans signature");
  }
  const std::uint8_t kind = head[3];
  if (kind < static_cast<std::uint8_t>(FrameKind::kText) ||
      kind > static_cast<std::uint8_t>(FrameKind::kClose)) {
    return Fail(ErrorCode::kCorruptData, "trame voix de genre inconnu");
  }
  const std::uint32_t length = ReadU32(head, 4);
  if (length > kFrameMostPayloadBytes) {
    return Fail(ErrorCode::kCorruptData, "trame voix trop longue");
  }
  if (pending_.size() < kFrameHeaderBytes + length) {
    return std::optional<StreamFrame>();
  }
  StreamFrame frame;
  frame.kind = static_cast<FrameKind>(kind);
  const auto body = pending_.begin() + static_cast<std::ptrdiff_t>(kFrameHeaderBytes);
  frame.payload.assign(body, body + static_cast<std::ptrdiff_t>(length));
  pending_.erase(pending_.begin(), body + static_cast<std::ptrdiff_t>(length));
  return std::optional<StreamFrame>(std::move(frame));
}

std::optional<HeardVoice> DecodeHeardVoice(std::span<const std::uint8_t> bytes) {
  if (bytes.size() <= kHeardHeaderBytes ||
      bytes.size() > kHeardHeaderBytes + kOpusMostPacketBytes) {
    return std::nullopt;
  }
  HeardVoice heard;
  heard.speaker_id = ReadU32(bytes, 0);
  heard.volume = std::bit_cast<float>(ReadU32(bytes, kHeardVolumeAt));
  heard.x = static_cast<std::int32_t>(ReadU32(bytes, kHeardXAt));
  heard.y = static_cast<std::int32_t>(ReadU32(bytes, kHeardYAt));
  const auto name = bytes.subspan(kHeardNameAt, kHeardNameBytes);
  const auto end = std::find(name.begin(), name.end(), std::uint8_t{0});
  // LE NOM SE COMPARE À CELUI DU PERSONNAGE À L'ÉCRAN : même décodage que le
  // serveur de jeu, quel que soit l'encodage que la base lui a donné.
  heard.name = core::Utf8FromWire(std::string_view(reinterpret_cast<const char*>(name.data()),
                                                   static_cast<std::size_t>(end - name.begin())));
  heard.sequence = ReadU16(bytes, kHeardSequenceAt);
  const auto opus = bytes.subspan(kHeardHeaderBytes);
  heard.opus.assign(opus.begin(), opus.end());
  // Un volume qui n'est pas un nombre de 0 à 1 ne vient pas du serveur.
  if (!(heard.volume >= 0.0F && heard.volume <= 1.0F)) {
    return std::nullopt;
  }
  return heard;
}

std::vector<std::uint8_t> EncodeTcpVoice(VoiceChannel channel, std::uint16_t sequence,
                                         std::span<const std::uint8_t> opus) {
  std::vector<std::uint8_t> out;
  out.reserve(kTcpVoiceHeaderBytes + opus.size());
  out.push_back(static_cast<std::uint8_t>(channel));
  // Le groupe n'est pas lu par le serveur, qui tient lui-même groupe et guilde.
  PutU32(out, 0);
  PutU16(out, sequence);
  out.insert(out.end(), opus.begin(), opus.end());
  return out;
}

std::vector<std::uint8_t> EncodeUdpHello(const UdpIdentity& identity) {
  std::vector<std::uint8_t> out;
  out.reserve(kUdpHelloBytes);
  PutUdpPrefix(out, UdpKind::kHello);
  PutUdpIdentity(out, identity);
  return out;
}

std::vector<std::uint8_t> EncodeUdpVoice(const UdpIdentity& identity, VoiceChannel channel,
                                         std::uint16_t sequence,
                                         std::span<const std::uint8_t> opus) {
  std::vector<std::uint8_t> out;
  out.reserve(kUdpVoiceHeaderBytes + opus.size());
  PutUdpPrefix(out, UdpKind::kVoice);
  PutUdpIdentity(out, identity);
  PutU16(out, sequence);
  out.push_back(static_cast<std::uint8_t>(channel));
  PutU32(out, 0);
  out.insert(out.end(), opus.begin(), opus.end());
  return out;
}

std::optional<UdpReceived> DecodeUdp(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kUdpPrefixBytes || ReadU32(bytes, 0) != kUdpMagic || bytes[4] != kUdpVersion) {
    return std::nullopt;
  }
  switch (bytes[5]) {
    case static_cast<std::uint8_t>(UdpKind::kHello):
      return UdpReceived{.kind = UdpKind::kHello};
    case static_cast<std::uint8_t>(UdpKind::kHeard): {
      std::optional<HeardVoice> heard = DecodeHeardVoice(bytes.subspan(kUdpPrefixBytes));
      if (!heard.has_value()) return std::nullopt;
      return UdpReceived{.kind = UdpKind::kHeard, .heard = std::move(heard)};
    }
    default:
      return std::nullopt;
  }
}

}  // namespace ro::voice
