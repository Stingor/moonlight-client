#include "ro/voice/voice_link.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <variant>

#include "ro/core/error.h"
#include "ro/core/log.h"

namespace ro::voice {
namespace {

// Ce qu'une lecture prend au plus, du TCP comme d'un datagramme.
constexpr std::size_t kReadChunkBytes = 16 * 1024;
constexpr std::size_t kDatagramBytes = 2048;
// Ce que le micro rend en un pas au plus : un pas très en retard ne lit pas
// une minute de son d'un coup.
constexpr std::size_t kMostCaptureFramesPerStep = 50;

constexpr float kSampleCeiling = static_cast<float>(std::numeric_limits<std::int16_t>::max());

// Un numéro de liaison tiré au hasard, jamais nul.
[[nodiscard]] std::uint64_t DrawSessionId() {
  std::random_device entropy;
  std::uint64_t drawn = 0;
  while (drawn == 0) {
    drawn = (std::uint64_t{entropy()} << 32) | std::uint64_t{entropy()};
  }
  return drawn;
}

// La moyenne quadratique d'une trame, de 0 à 1.
[[nodiscard]] float LevelOf(std::span<const std::int16_t> frame) {
  if (frame.empty()) return 0.0F;
  double sum = 0.0;
  for (const std::int16_t sample : frame) {
    const double scaled = static_cast<double>(sample) / static_cast<double>(kSampleCeiling);
    sum += scaled * scaled;
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(frame.size())));
}

[[nodiscard]] std::span<const std::uint8_t> BytesOf(const std::string& text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

}  // namespace

VoiceLink::VoiceLink(VoiceTarget target, VoiceDevices devices)
    : target_(std::move(target)), devices_(std::move(devices)) {
  if (core::Result<OpusVoiceEncoder> made = OpusVoiceEncoder::Create(); made.has_value()) {
    encoder_.emplace(std::move(*made));
  } else {
    core::LogWarning("voix : {}", core::Describe(made.error()));
  }
}

void VoiceLink::Step(std::uint32_t now_ms) {
  if (!tcp_.is_open()) {
    if (state_ == VoiceLinkState::kLost && now_ms < retry_at_ms_) {
      Capture(now_ms);
      Play(now_ms);
      return;
    }
    Open(now_ms);
    if (!tcp_.is_open()) return;
  }
  ReadTcp(now_ms);
  if (!tcp_.is_open()) return;
  ReadUdp(now_ms);
  Greet(now_ms);
  if (state_ == VoiceLinkState::kOnline) {
    FollowControls();
    if (now_ms - last_ping_ms_ >= kVoicePingMs) {
      last_ping_ms_ = now_ms;
      SendText(PingMessage(now_ms));
    }
  }
  Capture(now_ms);
  Play(now_ms);
  Flush(now_ms);
}

void VoiceLink::Open(std::uint32_t now_ms) {
  state_ = VoiceLinkState::kConnecting;
  core::Result<net::TcpSocket> opened =
      net::TcpSocket::Connect(target_.host, target_.port, kVoiceConnectTimeoutMs);
  if (!opened.has_value()) {
    Drop(now_ms, core::Describe(opened.error()));
    return;
  }
  tcp_ = std::move(*opened);
  reader_ = FrameReader();
  outbound_.clear();
  session_id_ = DrawSessionId();
  told_channel_.reset();
  told_listened_.reset();
  told_talking_.reset();
  listens_many_ = false;
  refusal_.reset();
  detail_.clear();
  state_ = VoiceLinkState::kAuthenticating;
  last_ping_ms_ = now_ms;
  SendText(AuthMessage(VoiceLogin{.account_id = target_.account_id,
                                  .character_id = target_.character_id,
                                  .login_id1 = target_.login_id1,
                                  .session_id = session_id_}));
  Flush(now_ms);
}

void VoiceLink::Drop(std::uint32_t now_ms, std::string why) {
  if (state_ != VoiceLinkState::kLost) {
    core::LogInfo("voix : liaison perdue ({})", why);
  }
  tcp_.Close();
  udp_.Close();
  udp_ready_ = false;
  in_room_ = false;
  war_ = false;
  nearby_.clear();
  state_ = VoiceLinkState::kLost;
  detail_ = std::move(why);
  retry_at_ms_ = now_ms + kVoiceRetryMs;
}

void VoiceLink::ReadTcp(std::uint32_t now_ms) {
  std::array<std::uint8_t, kReadChunkBytes> chunk = {};
  while (tcp_.is_open()) {
    core::Result<net::ReceiveResult> read = tcp_.Receive(chunk);
    if (!read.has_value()) {
      Drop(now_ms, core::Describe(read.error()));
      return;
    }
    if (read->state == net::ReceiveState::kClosed) {
      Drop(now_ms, "le serveur voix a fermé la liaison");
      return;
    }
    if (read->state == net::ReceiveState::kWouldBlock) break;
    reader_.Feed(std::span<const std::uint8_t>(chunk.data(), read->bytes));
  }
  while (tcp_.is_open()) {
    core::Result<std::optional<StreamFrame>> next = reader_.Next();
    if (!next.has_value()) {
      Drop(now_ms, core::Describe(next.error()));
      return;
    }
    if (!next->has_value()) return;
    StreamFrame& frame = **next;
    switch (frame.kind) {
      case FrameKind::kText: {
        const std::string json(frame.payload.begin(), frame.payload.end());
        core::Result<ServerMessage> message = ParseServerMessage(json);
        if (message.has_value()) {
          Apply(*message, now_ms);
        } else {
          core::LogWarning("voix : {}", core::Describe(message.error()));
        }
        break;
      }
      case FrameKind::kBinary:
        if (const std::optional<HeardVoice> heard = DecodeHeardVoice(frame.payload)) {
          mixer_.Hear(*heard, now_ms);
        }
        break;
      case FrameKind::kClose:
        Drop(now_ms, "le serveur voix a raccroché");
        return;
    }
  }
}

void VoiceLink::ReadUdp(std::uint32_t now_ms) {
  std::array<std::uint8_t, kDatagramBytes> datagram = {};
  while (udp_.is_open()) {
    core::Result<std::optional<std::size_t>> read = udp_.Receive(datagram);
    if (!read.has_value()) {
      core::LogWarning("voix : {}", core::Describe(read.error()));
      udp_.Close();
      udp_ready_ = false;
      return;
    }
    if (!read->has_value()) return;
    const std::optional<UdpReceived> received =
        DecodeUdp(std::span<const std::uint8_t>(datagram.data(), **read));
    if (!received.has_value()) continue;
    if (received->kind == UdpKind::kHello) {
      udp_ready_ = true;
    } else if (received->heard.has_value()) {
      mixer_.Hear(*received->heard, now_ms);
    } else if (received->coop.has_value() && coop_received_.size() < kVoiceMostCoopHeld) {
      coop_received_.push_back(*received->coop);
    }
  }
}

void VoiceLink::Apply(const ServerMessage& message, std::uint32_t now_ms) {
  std::visit(
      [&](const auto& said) {
        using Said = std::decay_t<decltype(said)>;
        if constexpr (std::is_same_v<Said, AuthAccepted>) {
          state_ = VoiceLinkState::kOnline;
          blocked_.clear();
          udp_identity_ = UdpIdentity{.session_id = session_id_,
                                      .character_id = target_.character_id,
                                      .token = said.udp_token};
          if (said.udp_port != 0 && said.udp_token != 0) {
            core::Result<net::UdpSocket> opened =
                net::UdpSocket::Connect(target_.host, said.udp_port);
            if (opened.has_value()) {
              udp_ = std::move(*opened);
              next_hello_ms_ = now_ms;
            } else {
              core::LogWarning("voix : UDP indisponible, l'audio passe par le TCP ({})",
                               core::Describe(opened.error()));
            }
          }
          core::LogInfo("voix : en ligne");
        } else if constexpr (std::is_same_v<Said, OwnPlace>) {
          own_x_ = said.x;
        } else if constexpr (std::is_same_v<Said, NearbyPlayers>) {
          nearby_ = said.players;
        } else if constexpr (std::is_same_v<Said, WarState>) {
          war_ = said.active;
        } else if constexpr (std::is_same_v<Said, RoomJoined>) {
          in_room_ = true;
        } else if constexpr (std::is_same_v<Said, RoomLeft>) {
          in_room_ = false;
        } else if constexpr (std::is_same_v<Said, ListenAccepted>) {
          listens_many_ = true;
        } else if constexpr (std::is_same_v<Said, BlockedPlayers>) {
          blocked_ = said.players;
        } else if constexpr (std::is_same_v<Said, BlockChanged>) {
          std::erase_if(blocked_, [&](const NearbyPlayer& player) {
            return player.character_id == said.character_id;
          });
          if (said.blocked) {
            blocked_.push_back(NearbyPlayer{.character_id = said.character_id, .name = said.name});
          }
        } else if constexpr (std::is_same_v<Said, Refusal>) {
          refusal_ = said.kind;
          detail_ = said.message;
          core::LogWarning("voix : le serveur refuse ({})", said.message);
        }
      },
      message);
}

void VoiceLink::Block(std::uint32_t character_id, bool blocked) {
  if (state_ != VoiceLinkState::kOnline || character_id == 0) return;
  SendText(BlockMessage(character_id, blocked));
}

void VoiceLink::FollowControls() {
  // ⚠ LES CANAUX ÉCOUTÉS D'ABORD : un serveur qui les connaît ne laisse plus
  // le canal unique qui suit les défaire.
  const auto listened = static_cast<std::uint8_t>(
      (controls_.listened | ListenBit(controls_.channel)) & kVoiceListenAll);
  if (told_listened_ != listened) {
    told_listened_ = listened;
    SendText(SetListenMessage(listened));
  }
  if (!listens_many_ && told_channel_ != controls_.channel) {
    told_channel_ = controls_.channel;
    SendText(SetChannelMessage(controls_.channel));
  }
}

void VoiceLink::FollowTalking(std::uint32_t now_ms) {
  if (state_ != VoiceLinkState::kOnline) return;
  if (controls_.talking) {
    last_spoke_ms_ = now_ms;
  }
  const std::uint32_t hold_ms = controls_.voice_activated
                                    ? std::max(controls_.release_ms, kVoiceActivationLeastHoldMs)
                                    : controls_.release_ms;
  const bool held = last_spoke_ms_.has_value() && now_ms - *last_spoke_ms_ < hold_ms;
  const bool silenced = controls_.self_muted || controls_.deafened;
  const bool talking = encoder_.has_value() && !silenced && (controls_.talking || held);
  if (told_talking_ != talking) {
    told_talking_ = talking;
    SendText(TalkMessage(talking, session_id_));
  }
}

void VoiceLink::Greet(std::uint32_t now_ms) {
  if (!udp_.is_open() || now_ms < next_hello_ms_) return;
  const std::vector<std::uint8_t> hello = EncodeUdpHello(udp_identity_);
  static_cast<void>(udp_.Send(hello));
  next_hello_ms_ = now_ms + (udp_ready_ ? kVoiceHelloKeepMs : kVoiceHelloRetryMs);
}

void VoiceLink::Capture(std::uint32_t now_ms) {
  if (devices_.read_capture) {
    const std::size_t before = captured_.size();
    captured_.resize(before + kVoiceFrameSamples * kMostCaptureFramesPerStep);
    const std::size_t read =
        devices_.read_capture(std::span<std::int16_t>(captured_).subspan(before));
    captured_.resize(before + std::min(read, captured_.size() - before));
  }
  std::size_t used = 0;
  while (captured_.size() - used >= kVoiceFrameSamples) {
    const std::span<std::int16_t> frame(captured_.data() + used, kVoiceFrameSamples);
    used += kVoiceFrameSamples;
    // ⚠ LE MICRO SE LIT MÊME QUAND ON SE TAIT : sans quoi, la touche enfoncée,
    // partiraient d'abord les secondes entendues avant. Et c'est à le lire
    // qu'on sait si la voix dépasse le seuil.
    if (controls_.input_gain != 1.0F) {
      for (std::int16_t& sample : frame) {
        sample = static_cast<std::int16_t>(std::lround(std::clamp(
            static_cast<float>(sample) * controls_.input_gain, -kSampleCeiling, kSampleCeiling)));
      }
    }
    input_level_ = LevelOf(frame);
    if (controls_.voice_activated && input_level_ >= controls_.activation_level) {
      last_spoke_ms_ = now_ms;
    }
    FollowTalking(now_ms);
    if (state_ != VoiceLinkState::kOnline || told_talking_ != true) continue;
    core::Result<std::vector<std::uint8_t>> packet = encoder_->Encode(frame);
    if (packet.has_value()) {
      SendVoice(*packet);
    }
  }
  captured_.erase(captured_.begin(), captured_.begin() + static_cast<std::ptrdiff_t>(used));
  // SANS MICRO, OU ENTRE DEUX TRAMES : la touche et le délai se suivent aussi.
  FollowTalking(now_ms);
}

void VoiceLink::Play(std::uint32_t now_ms) {
  if (!devices_.write_playback || !devices_.queued_playback) return;
  const MixSettings settings{.output_gain = controls_.deafened ? 0.0F : controls_.output_gain,
                             .spatial = controls_.spatial,
                             .listener_x = own_x_,
                             .muted = controls_.muted};
  while (devices_.queued_playback() < kVoicePlaybackLeadFrames * kVoiceOutputFrameSamples) {
    mixer_.MixFrame(mixed_, settings, now_ms);
    devices_.write_playback(mixed_);
  }
}

void VoiceLink::SendText(const std::string& json) {
  const std::vector<std::uint8_t> frame = EncodeFrame(FrameKind::kText, BytesOf(json));
  outbound_.insert(outbound_.end(), frame.begin(), frame.end());
}

void VoiceLink::SendCoop(std::span<const std::uint8_t> payload) {
  if (!udp_.is_open() || !udp_ready_) return;
  // Un envoi refusé est une trame perdue : la suivante la redit.
  (void)udp_.Send(EncodeUdpCoop(udp_identity_, payload));
}

std::vector<CoopRelayed> VoiceLink::TakeCoop() {
  return std::exchange(coop_received_, std::vector<CoopRelayed>{});
}

void VoiceLink::SendVoice(std::span<const std::uint8_t> opus) {
  const std::uint16_t sequence = sequence_++;
  if (udp_.is_open() && udp_ready_) {
    const std::vector<std::uint8_t> datagram =
        EncodeUdpVoice(udp_identity_, spoken_channel(), sequence, opus);
    if (core::Result<bool> sent = udp_.Send(datagram); sent.has_value()) return;
  }
  // À DÉFAUT D'UDP, LE TCP — tant qu'il avance : une trame retenue derrière
  // d'autres arriverait trop tard pour servir.
  if (outbound_.size() > kVoiceMostOutboundBytes) return;
  const std::vector<std::uint8_t> payload = EncodeTcpVoice(spoken_channel(), sequence, opus);
  const std::vector<std::uint8_t> frame = EncodeFrame(FrameKind::kBinary, payload);
  outbound_.insert(outbound_.end(), frame.begin(), frame.end());
}

void VoiceLink::Flush(std::uint32_t now_ms) {
  while (tcp_.is_open() && !outbound_.empty()) {
    core::Result<std::size_t> sent = tcp_.Send(outbound_);
    if (!sent.has_value()) {
      Drop(now_ms, core::Describe(sent.error()));
      return;
    }
    if (*sent == 0) return;
    outbound_.erase(outbound_.begin(), outbound_.begin() + static_cast<std::ptrdiff_t>(*sent));
  }
}

VoiceStatus VoiceLink::status(std::uint32_t now_ms) const {
  return VoiceStatus{.state = state_,
                     .detail = detail_,
                     .refusal = refusal_,
                     .udp = udp_ready_,
                     .transmitting = state_ == VoiceLinkState::kOnline && told_talking_ == true,
                     .input_level = input_level_,
                     .war = war_,
                     .in_room = in_room_,
                     .speaking = mixer_.speaking(now_ms),
                     .nearby = nearby_,
                     .blocked = blocked_};
}

}  // namespace ro::voice
