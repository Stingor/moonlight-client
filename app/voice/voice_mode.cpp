#include "voice/voice_mode.h"

#include <algorithm>
#include <span>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/audio/voice_devices.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/ui_caps.h"
#include "ro/proto/packet_lengths.h"
#include "ro/voice/voice_engine.h"

#include "options/percent.h"

namespace moonclient {
namespace {

using ro::core::Tr;
namespace rv = ro::voice;

// Combien de temps le témoin rappelle de choisir la touche « Parler », après
// la mise en ligne : assez pour être lu, pas assez pour gêner qui n'en veut pas.
constexpr std::uint32_t kTalkKeyReminderMs = 15'000;

// Vrai quand la touche est tenue et que la fenêtre du jeu a le clavier.
[[nodiscard]] bool TalkKeyHeld(std::uint32_t talk_key) {
  if (talk_key == 0 || SDL_GetKeyboardFocus() == nullptr) return false;
  const SDL_Scancode scancode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(talk_key), nullptr);
  if (scancode == SDL_SCANCODE_UNKNOWN) return false;
  int count = 0;
  const bool* pressed = SDL_GetKeyboardState(&count);
  return pressed != nullptr && static_cast<int>(scancode) < count && pressed[scancode];
}

[[nodiscard]] std::string NoticeOf(const rv::VoiceStatus& status, const VoiceFacts& facts,
                                   std::uint32_t online_since_ms) {
  if (status.state == rv::VoiceLinkState::kLost) {
    return Tr("Voix : serveur injoignable, nouvel essai…");
  }
  if (status.refusal == rv::RefusalKind::kBanned) return Tr("Voix : coupée par un MJ.");
  if (status.refusal == rv::RefusalKind::kLicenseRequired) {
    return Tr("Voix : réservée aux comptes qui l'ont.");
  }
  if (status.refusal == rv::RefusalKind::kFlooded) {
    return Tr("Voix : coupée quelques minutes, trop de paquets.");
  }
  if (status.state != rv::VoiceLinkState::kOnline) return {};
  if (status.war && !status.in_room && facts.settings.channel == 0) {
    return Tr("Voix : la proximité se tait pendant la guerre de guilde.");
  }
  if (facts.talk_key == 0 && facts.settings.talk_mode != static_cast<int>(VoiceTalkMode::kVoice) &&
      facts.now_ms - online_since_ms < kTalkKeyReminderMs) {
    return Tr("Voix : choisissez la touche « Parler (voix) » dans les raccourcis.");
  }
  return {};
}

}  // namespace

VoiceMode::VoiceMode(const VoiceLengthOf& length_of) {
  const bool known = length_of ? length_of(ro::net::kVoiceServiceOpcode).has_value()
                               : ro::proto::PacketLength(ro::net::kVoiceServiceOpcode).has_value();
  caps_ = known ? ro::net::kUiCapVoice : 0U;
}

VoiceMode::~VoiceMode() { Stop(); }

ro::net::FrameOutcome VoiceMode::ApplyFrame(const ro::net::Frame& frame) {
  if (frame.opcode != ro::net::kVoiceServiceOpcode) return ro::net::FrameOutcome::kUnhandled;
  std::optional<ro::net::VoiceService> service = ro::net::DecodeVoiceService(frame.bytes);
  if (!service.has_value()) return ro::net::FrameOutcome::kUndecodable;
  // UN AUTRE SERVEUR VOIX : la liaison en cours ne mène plus au bon.
  if (service_.has_value() &&
      (service_->host != service->host || service_->port != service->port)) {
    StopFor("autre serveur voix");
  }
  ro::core::LogInfo("voix : le serveur la sert sur {}:{}", service->host, service->port);
  service_ = std::move(service);
  return ro::net::FrameOutcome::kApplied;
}

void VoiceMode::Start(const VoiceFacts& facts) {
  ro::core::Result<ro::audio::VoiceDevices> opened =
      ro::audio::VoiceDevices::Open(microphone_, speakers_);
  if (!opened.has_value()) {
    devices_failed_ = true;
    ro::core::LogWarning("voix : {}", ro::core::Describe(opened.error()));
    return;
  }
  devices_ = std::make_unique<ro::audio::VoiceDevices>(std::move(*opened));
  started_as_ = VoiceIdentity{.account_id = facts.account_id,
                              .character_id = facts.character_id,
                              .login_id1 = facts.login_id1};
  ro::audio::VoiceDevices* devices = devices_.get();
  rv::VoiceDevices hooks{
      .read_capture =
          [devices](std::span<std::int16_t> into) { return devices->ReadCapture(into); },
      .write_playback =
          [devices](std::span<const std::int16_t> stereo) { devices->WritePlayback(stereo); },
      .queued_playback = [devices] { return devices->QueuedPlayback(); }};
  engine_ = std::make_unique<rv::VoiceEngine>(rv::VoiceTarget{.host = service_->host,
                                                              .port = service_->port,
                                                              .account_id = facts.account_id,
                                                              .character_id = facts.character_id,
                                                              .login_id1 = facts.login_id1},
                                              std::move(hooks));
  was_online_ = false;
}

void VoiceMode::Stop() noexcept {
  engine_.reset();
  devices_.reset();
}

void VoiceMode::StopFor(std::string_view why) {
  // 🔴 UNE VOIX QUI SE TAIT DIT POURQUOI : sans cette ligne, une liaison fermée
  // par la partie ne se distingue pas d'un micro muet.
  if (engine_ != nullptr) {
    ro::core::LogInfo("voix : liaison fermée ({})", why);
  }
  Stop();
}

void VoiceMode::Pump(const VoiceFacts& facts) {
  if (!facts.settings.enabled) {
    devices_failed_ = false;
  }
  if (facts.settings.microphone != microphone_ || facts.settings.speakers != speakers_) {
    microphone_ = facts.settings.microphone;
    speakers_ = facts.settings.speakers;
    devices_failed_ = false;
    StopFor("autres appareils");
  }
  // UN AUTRE PERSONNAGE, OU UNE AUTRE SESSION, n'est pas celui que la liaison
  // a présenté au serveur voix : elle se rouvre sous la nouvelle identité.
  const VoiceIdentity identity{.account_id = facts.account_id,
                               .character_id = facts.character_id,
                               .login_id1 = facts.login_id1};
  if (identity != started_as_) {
    StopFor("autre personnage");
  }
  if (!facts.settings.enabled) {
    StopFor("voix éteinte");
  } else if (!facts.verified) {
    StopFor("poignée de main non reconnue");
  } else if (!service_.has_value()) {
    StopFor("serveur voix inconnu");
  } else if (facts.account_id == 0 || facts.character_id == 0) {
    StopFor("hors de la partie");
  } else if (engine_ == nullptr && !devices_failed_) {
    Start(facts);
  }
  const bool talking = engine_ != nullptr && TalkKeyHeld(facts.talk_key);
  if (engine_ != nullptr) {
    engine_->Steer(rv::VoiceControls{
        .talking = talking,
        .voice_activated = facts.settings.talk_mode == static_cast<int>(VoiceTalkMode::kVoice),
        .activation_level = facts.settings.activation_percent / kPercent * kVoiceLevelAtFullScale,
        .release_ms = static_cast<std::uint32_t>(facts.settings.release_ms),
        .channel = static_cast<rv::VoiceChannel>(facts.settings.channel),
        .output_gain = facts.settings.output_volume,
        .input_gain = facts.settings.input_gain,
        .spatial = facts.spatial});
  }
  Compose(facts);
}

void VoiceMode::Compose(const VoiceFacts& facts) {
  overlay_ = ro::ui::VoiceOverlayState();
  heard_.clear();
  blocked_.clear();
  input_percent_.reset();
  if (engine_ == nullptr) {
    if (devices_failed_ && facts.settings.enabled && service_.has_value()) {
      overlay_.notice = Tr("Voix : aucun appareil de son ne s'ouvre.");
    }
    return;
  }
  rv::VoiceStatus status = engine_->status();
  const bool online = status.state == rv::VoiceLinkState::kOnline;
  if (online && !was_online_) {
    online_since_ms_ = facts.now_ms;
  }
  was_online_ = online;
  overlay_.notice = NoticeOf(status, facts, online_since_ms_);
  // ⚠ CE QUI PART, ET NON LA TOUCHE : la voix et le délai au relâchement
  // font parler sans elle.
  if (status.transmitting) {
    overlay_.self_talking = Tr("Vous parlez");
  }
  input_percent_ = status.input_level / kVoiceLevelAtFullScale * kPercent;
  heard_ = std::move(status.nearby);
  blocked_ = std::move(status.blocked);
  for (rv::SpeakingVoice& voice : status.speaking) {
    heard_.push_back(rv::NearbyPlayer{.character_id = voice.speaker_id, .name = voice.name});
    overlay_.speaking.push_back(std::move(voice.name));
  }
}

VoiceMode::Peer VoiceMode::PeerOf(std::string_view name) const {
  const auto named = [name](const rv::NearbyPlayer& player) { return player.name == name; };
  if (std::ranges::any_of(blocked_, named)) return Peer::kBlocked;
  if (std::ranges::any_of(heard_, named)) return Peer::kHeard;
  return Peer::kUnknown;
}

std::vector<std::string> VoiceMode::BlockedNames() const {
  std::vector<std::string> names;
  names.reserve(blocked_.size());
  for (const rv::NearbyPlayer& player : blocked_) {
    names.push_back(player.name);
  }
  return names;
}

void VoiceMode::Block(std::string_view name, bool blocked) {
  if (engine_ == nullptr) return;
  const auto named = [name](const rv::NearbyPlayer& player) { return player.name == name; };
  const std::vector<rv::NearbyPlayer>& known = blocked ? heard_ : blocked_;
  const auto found = std::ranges::find_if(known, named);
  if (found == known.end()) return;
  engine_->Block(found->character_id, blocked);
}

}  // namespace moonclient
