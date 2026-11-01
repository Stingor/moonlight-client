#include "ro/voice/opus_codec.h"

#include <string>

#include <opus/opus.h>

namespace ro::voice {
namespace {

using core::ErrorCode;
using core::Fail;

// Le plus long paquet qu'on laisse écrire à l'encodeur : celui que le serveur
// accepte encore.
constexpr std::size_t kMostEncodedBytes = 1500;

[[nodiscard]] core::Error OpusError(const char* what, int code) {
  return core::Error{ErrorCode::kCorruptData, std::string(what) + " : " + opus_strerror(code)};
}

}  // namespace

void OpusVoiceEncoder::Release::operator()(OpusEncoder* state) const noexcept {
  opus_encoder_destroy(state);
}

void OpusVoiceDecoder::Release::operator()(OpusDecoder* state) const noexcept {
  opus_decoder_destroy(state);
}

core::Result<OpusVoiceEncoder> OpusVoiceEncoder::Create() {
  int code = OPUS_OK;
  OpusEncoder* state = opus_encoder_create(kVoiceSampleRate, 1, OPUS_APPLICATION_VOIP, &code);
  if (state == nullptr || code != OPUS_OK) {
    return std::unexpected(OpusError("encodeur Opus", code));
  }
  OpusVoiceEncoder encoder;
  encoder.state_.reset(state);
  opus_encoder_ctl(state, OPUS_SET_BITRATE(kVoiceBitrate));
  opus_encoder_ctl(state, OPUS_SET_INBAND_FEC(1));
  opus_encoder_ctl(state, OPUS_SET_PACKET_LOSS_PERC(kVoiceExpectedLossPercent));
  opus_encoder_ctl(state, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  return encoder;
}

core::Result<std::vector<std::uint8_t>> OpusVoiceEncoder::Encode(
    std::span<const std::int16_t> frame) {
  if (frame.size() != kVoiceFrameSamples) {
    return Fail(ErrorCode::kInvalidArgument, "trame de voix de mauvaise durée");
  }
  std::vector<std::uint8_t> packet(kMostEncodedBytes);
  const opus_int32 written = opus_encode(state_.get(), frame.data(), static_cast<int>(frame.size()),
                                         packet.data(), static_cast<opus_int32>(packet.size()));
  if (written < 0) {
    return std::unexpected(OpusError("encodage Opus", written));
  }
  packet.resize(static_cast<std::size_t>(written));
  return packet;
}

core::Result<OpusVoiceDecoder> OpusVoiceDecoder::Create() {
  int code = OPUS_OK;
  OpusDecoder* state = opus_decoder_create(kVoiceSampleRate, 1, &code);
  if (state == nullptr || code != OPUS_OK) {
    return std::unexpected(OpusError("décodeur Opus", code));
  }
  OpusVoiceDecoder decoder;
  decoder.state_.reset(state);
  return decoder;
}

core::Result<void> OpusVoiceDecoder::Decode(std::span<const std::uint8_t> packet,
                                            std::span<std::int16_t> frame) {
  if (frame.size() != kVoiceFrameSamples) {
    return Fail(ErrorCode::kInvalidArgument, "trame de voix de mauvaise durée");
  }
  const int decoded =
      opus_decode(state_.get(), packet.data(), static_cast<opus_int32>(packet.size()), frame.data(),
                  static_cast<int>(frame.size()), 0);
  if (decoded < 0) {
    return std::unexpected(OpusError("décodage Opus", decoded));
  }
  // Un paquet de 10 ms remplit la moitié de la trame : le reste se tait.
  for (std::size_t at = static_cast<std::size_t>(decoded); at < frame.size(); ++at) {
    frame[at] = 0;
  }
  return {};
}

core::Result<void> OpusVoiceDecoder::Conceal(std::span<const std::uint8_t> next,
                                             std::span<std::int16_t> frame) {
  if (frame.size() != kVoiceFrameSamples) {
    return Fail(ErrorCode::kInvalidArgument, "trame de voix de mauvaise durée");
  }
  // Le paquet suivant, lu pour la redondance qu'il porte (`decode_fec`) ; sans
  // lui, un paquet nul demande au décodeur de prolonger.
  const bool from_next = !next.empty();
  const int decoded = opus_decode(state_.get(), from_next ? next.data() : nullptr,
                                  from_next ? static_cast<opus_int32>(next.size()) : 0,
                                  frame.data(), static_cast<int>(frame.size()), from_next ? 1 : 0);
  if (decoded < 0) {
    return std::unexpected(OpusError("reconstruction Opus", decoded));
  }
  for (std::size_t at = static_cast<std::size_t>(decoded); at < frame.size(); ++at) {
    frame[at] = 0;
  }
  return {};
}

}  // namespace ro::voice
