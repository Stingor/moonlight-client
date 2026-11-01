#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "ro/net/socket.h"
#include "ro/voice/opus_codec.h"
#include "ro/voice/voice_messages.h"
#include "ro/voice/voice_mixer.h"
#include "ro/voice/voice_wire.h"

namespace ro::voice {

// ── LA LIAISON AU SERVEUR VOIX, un pas à la fois ────────────────────────────
//
// Elle ouvre le TCP, s'authentifie, ouvre l'UDP que le serveur offre, encode
// le micro quand le joueur parle et mêle ce qu'elle entend vers la sortie. Elle
// n'a pas de fil à elle : `VoiceEngine` la fait avancer, et un essai aussi.

// Combien attendre la connexion : le fil de la voix attend avec elle, et la
// partie attend le fil quand elle se ferme.
inline constexpr int kVoiceConnectTimeoutMs = 2000;
// Combien attendre avant de retenter une liaison perdue ou refusée.
inline constexpr std::uint32_t kVoiceRetryMs = 5000;
// Le battement de contrôle : le serveur n'en exige pas, il dit que la liaison
// vit des deux côtés.
inline constexpr std::uint32_t kVoicePingMs = 15'000;
// Le salut UDP, répété jusqu'à la réponse, puis entretenu : le serveur oublie
// l'adresse d'un client muet depuis 30 s.
inline constexpr std::uint32_t kVoiceHelloRetryMs = 1000;
inline constexpr std::uint32_t kVoiceHelloKeepMs = 10'000;
// Combien d'audio la sortie garde d'avance : assez pour qu'un pas en retard ne
// la vide pas, peu pour que la voix reste vive.
inline constexpr std::size_t kVoicePlaybackLeadFrames = 3;
// Au-delà, le TCP n'avance plus : l'audio qui suivrait s'y entasserait.
inline constexpr std::size_t kVoiceMostOutboundBytes = 64 * 1024;

// LES APPAREILS DE SON, vus de la voix : des fonctions, que `app` branche sur
// ceux de la couche de son. Un micro absent est une lecture vide.
struct VoiceDevices {
  // Mono, 48 kHz : rend combien d'échantillons sont écrits.
  std::function<std::size_t(std::span<std::int16_t>)> read_capture;
  // Stéréo entrelacée, 48 kHz.
  std::function<void(std::span<const std::int16_t>)> write_playback;
  // Combien d'échantillons stéréo attendent encore d'être joués.
  std::function<std::size_t()> queued_playback;
};

// OÙ ALLER ET QUI L'ON EST.
struct VoiceTarget {
  std::string host;
  std::uint16_t port = 0;
  std::uint32_t account_id = 0;
  std::uint32_t character_id = 0;
  std::uint32_t login_id1 = 0;
};

// CE QUE LE JOUEUR DEMANDE, redit à chaque pas.
struct VoiceControls {
  bool talking = false;  // la touche « Parler » est tenue
  VoiceChannel channel = VoiceChannel::kNearby;
  float output_gain = 1.0F;
  float input_gain = 1.0F;
  bool spatial = true;
  std::set<std::uint32_t> muted;
};

enum class VoiceLinkState : std::uint8_t {
  kConnecting,      // le TCP s'ouvre
  kAuthenticating,  // le serveur vérifie qui l'on est auprès du serveur de jeu
  kOnline,
  kLost,  // la liaison est tombée ou n'a pas pu s'ouvrir ; elle se retente
};

struct VoiceStatus {
  VoiceLinkState state = VoiceLinkState::kConnecting;
  // CE QUI A ÉTÉ DIT DE TRAVERS, pour le journal : la panne de la liaison, ou
  // le dernier refus du serveur. Vide sinon.
  std::string detail;
  std::optional<RefusalKind> refusal;
  bool udp = false;           // l'audio passe par l'UDP
  bool transmitting = false;  // le micro part vers le serveur
  bool war = false;           // la proximité est coupée par la guerre de guilde
  bool in_room = false;       // la voix est celle du salon de discussion
  std::vector<SpeakingVoice> speaking;
  std::vector<NearbyPlayer> nearby;
};

class VoiceLink {
 public:
  VoiceLink(VoiceTarget target, VoiceDevices devices);

  void Steer(const VoiceControls& controls) { controls_ = controls; }
  // Un pas : la liaison, l'écoute, le micro et la sortie.
  void Step(std::uint32_t now_ms);
  [[nodiscard]] VoiceStatus status(std::uint32_t now_ms) const;

 private:
  void Open(std::uint32_t now_ms);
  void Drop(std::uint32_t now_ms, std::string why);
  void ReadTcp(std::uint32_t now_ms);
  void ReadUdp(std::uint32_t now_ms);
  void Apply(const ServerMessage& message, std::uint32_t now_ms);
  void FollowControls();
  void Greet(std::uint32_t now_ms);
  void Capture();
  void Play(std::uint32_t now_ms);
  void SendText(const std::string& json);
  void SendVoice(std::span<const std::uint8_t> opus);
  void Flush(std::uint32_t now_ms);
  [[nodiscard]] VoiceChannel spoken_channel() const noexcept {
    return in_room_ ? VoiceChannel::kRoom : controls_.channel;
  }

  VoiceTarget target_;
  VoiceDevices devices_;
  VoiceControls controls_;

  net::TcpSocket tcp_;
  net::UdpSocket udp_;
  FrameReader reader_;
  std::vector<std::uint8_t> outbound_;
  std::uint64_t session_id_ = 0;
  UdpIdentity udp_identity_;
  bool udp_ready_ = false;
  std::uint32_t next_hello_ms_ = 0;
  std::uint32_t last_ping_ms_ = 0;
  std::uint32_t retry_at_ms_ = 0;

  VoiceLinkState state_ = VoiceLinkState::kConnecting;
  std::string detail_;
  std::optional<RefusalKind> refusal_;
  bool war_ = false;
  bool in_room_ = false;
  std::vector<NearbyPlayer> nearby_;
  std::optional<std::int32_t> own_x_;

  // CE QUE LE SERVEUR SAIT DE NOUS : ce qui a été dit, pour ne redire qu'un
  // changement. Vide tant que rien n'est dit.
  std::optional<VoiceChannel> told_channel_;
  std::optional<bool> told_talking_;

  std::optional<OpusVoiceEncoder> encoder_;
  std::vector<std::int16_t> captured_;
  std::uint16_t sequence_ = 0;
  VoiceMixer mixer_;
  std::vector<std::int16_t> mixed_ = std::vector<std::int16_t>(kVoiceOutputFrameSamples);
};

}  // namespace ro::voice
