#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::voice {

// ── LE FIL DU SERVEUR VOIX ──────────────────────────────────────────────────
//
// Deux liaisons : un flux TCP de trames, qui porte le contrôle en JSON et, à
// défaut d'UDP, l'audio ; et des datagrammes UDP, qui portent l'audio.
// L'audio est de l'Opus mono à 48 kHz, une trame de 20 ms par paquet. Tous les
// entiers sont gros-boutistes.

// LE CANAL D'UN PAQUET AUDIO, tel que le fil le numérote. Proximité, groupe et
// guilde se choisissent ; le salon s'impose dans un salon de discussion.
enum class VoiceChannel : std::uint8_t {
  kNearby = 0,
  kParty = 1,
  kGuild = 2,
  kRoom = 3,
  kWhisper = 4,
};
inline constexpr std::uint8_t kVoiceChannelCount = 5;

// LES CANAUX QU'ON ÉCOUTE, un bit par canal : la proximité, le groupe et la
// guilde. Le salon et l'appel privé s'entendent sans s'écouter.
inline constexpr std::uint8_t kVoiceListenedChannelCount = 3;
inline constexpr std::uint8_t kVoiceListenAll = (1U << kVoiceListenedChannelCount) - 1U;
[[nodiscard]] constexpr std::uint8_t ListenBit(VoiceChannel channel) {
  return static_cast<std::uint8_t>(channel) < kVoiceListenedChannelCount
             ? static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(channel))
             : std::uint8_t{0};
}

// ── LES TRAMES TCP : [56 54][version][genre][longueur:4] puis le contenu ────
inline constexpr std::size_t kFrameHeaderBytes = 8;
inline constexpr std::uint8_t kFrameVersion = 1;
// Au-delà, le flux est tenu pour corrompu : le serveur refuse lui aussi plus
// d'un mégaoctet par trame.
inline constexpr std::uint32_t kFrameMostPayloadBytes = 1024 * 1024;

enum class FrameKind : std::uint8_t {
  kText = 1,    // un message JSON
  kBinary = 2,  // un paquet audio
  kClose = 3,   // le pair raccroche
};

struct StreamFrame {
  FrameKind kind = FrameKind::kText;
  std::vector<std::uint8_t> payload;
};

[[nodiscard]] std::vector<std::uint8_t> EncodeFrame(FrameKind kind,
                                                    std::span<const std::uint8_t> payload);

// LE DÉCOUPAGE DU FLUX : les octets arrivent par morceaux quelconques, les
// trames sortent entières.
class FrameReader {
 public:
  void Feed(std::span<const std::uint8_t> bytes);
  // La trame suivante, rien si elle n'est pas encore entière, ou une erreur
  // quand l'en-tête ne se lit pas : le flux est alors perdu.
  [[nodiscard]] core::Result<std::optional<StreamFrame>> Next();

 private:
  std::vector<std::uint8_t> pending_;
};

// ── L'AUDIO REÇU : [personnage:4][volume:f32][x:4][y:4][nom:24][numéro:2] ───
inline constexpr std::size_t kHeardHeaderBytes = 42;
inline constexpr std::size_t kHeardNameBytes = 24;
// Le serveur refuse lui-même un paquet Opus plus long.
inline constexpr std::size_t kOpusMostPacketBytes = 1500;

struct HeardVoice {
  std::uint32_t speaker_id = 0;  // le char_id de qui parle
  // LE VOLUME QUE LE SERVEUR A CALCULÉ, de 0 à 1 : la distance pour la
  // proximité, 1 pour un groupe.
  float volume = 0.0F;
  std::int32_t x = 0;  // la case de qui parle
  std::int32_t y = 0;
  std::string name;
  std::uint16_t sequence = 0;
  std::vector<std::uint8_t> opus;
};

[[nodiscard]] std::optional<HeardVoice> DecodeHeardVoice(std::span<const std::uint8_t> bytes);

// L'AUDIO ENVOYÉ PAR LE TCP, à défaut d'UDP : [canal][groupe:4][numéro:2].
[[nodiscard]] std::vector<std::uint8_t> EncodeTcpVoice(VoiceChannel channel, std::uint16_t sequence,
                                                       std::span<const std::uint8_t> opus);

// ── LES DATAGRAMMES : ["VUDP"][version][genre] puis le corps ───────────────
inline constexpr std::uint32_t kUdpMagic = 0x56554450;  // "VUDP"
inline constexpr std::uint8_t kUdpVersion = 1;
inline constexpr std::size_t kUdpPrefixBytes = 6;
inline constexpr std::size_t kUdpHelloBytes = 26;
inline constexpr std::size_t kUdpVoiceHeaderBytes = 33;

enum class UdpKind : std::uint8_t {
  kHello = 1,  // le client se fait connaître ; le serveur répond du même
  kVoice = 2,  // l'audio du client
  kHeard = 3,  // l'audio d'un autre, relayé par le serveur
};

// CE QUI LIE UN DATAGRAMME À SA SESSION TCP : le serveur rejette tout
// datagramme dont l'un des trois ne correspond pas.
struct UdpIdentity {
  std::uint64_t session_id = 0;  // tiré par le client, annoncé à l'authentification
  std::uint32_t character_id = 0;
  std::uint64_t token = 0;  // remis par le serveur à l'authentification
};

[[nodiscard]] std::vector<std::uint8_t> EncodeUdpHello(const UdpIdentity& identity);
[[nodiscard]] std::vector<std::uint8_t> EncodeUdpVoice(const UdpIdentity& identity,
                                                       VoiceChannel channel, std::uint16_t sequence,
                                                       std::span<const std::uint8_t> opus);

struct UdpReceived {
  UdpKind kind = UdpKind::kHello;
  std::optional<HeardVoice> heard;  // pour `kHeard` seulement
};

// Ce que dit un datagramme du serveur, ou rien s'il n'est pas du serveur voix.
[[nodiscard]] std::optional<UdpReceived> DecodeUdp(std::span<const std::uint8_t> bytes);

}  // namespace ro::voice
