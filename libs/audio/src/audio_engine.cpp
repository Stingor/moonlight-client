#include "ro/audio/audio_engine.h"

#include <algorithm>
#include <utility>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "ro/audio/sound_onset.h"

namespace ro::audio {
namespace {

// Boucler SANS FIN : la musique d'une carte joue tant qu'on y est.
constexpr int kLoopForever = -1;

// La table qui traduit nos types opaques vers ceux de SDL3_mixer. Les déclarer
// dans l'en-tête ferait entrer SDL_mixer.h dans tout ce qui l'inclut.
MIX_Mixer* AsMixer(void* handle) { return static_cast<MIX_Mixer*>(handle); }
MIX_Track* AsTrack(void* handle) { return static_cast<MIX_Track*>(handle); }
MIX_Audio* AsSound(void* handle) { return static_cast<MIX_Audio*>(handle); }

}  // namespace

// ⚠ TOUT CHAMP NOUVEAU S'AJOUTE ICI, ET DANS L'AFFECTATION CI-DESSOUS.
//
// Ces deux fonctions énumèrent l'état à la main — elles le doivent, parce que la
// source garde des pointeurs bruts qu'il faut annuler pour ne pas libérer deux
// fois. La conséquence est qu'un champ oublié REPART À SON DÉFAUT en silence.
//
// C'EST ARRIVÉ : les deux drapeaux d'audibilité ont été ajoutés sans être
// inscrits ici. `OpenAudio` coupe le son puis rend le mélangeur PAR VALEUR, si
// bien que la coupure était posée sur un objet aussitôt abandonné. Le journal
// écrivait « son : coupé au démarrage » et le son jouait — rien ne se
// contredisait, et il a fallu l'oreille d'un joueur.
AudioEngine::AudioEngine(AudioEngine&& other) noexcept
    : mixer_(other.mixer_),
      music_track_(other.music_track_),
      music_sound_(other.music_sound_),
      music_name_(std::move(other.music_name_)),
      music_volume_(other.music_volume_),
      sound_volume_(other.sound_volume_),
      channel_volumes_(other.channel_volumes_),
      music_audible_(other.music_audible_),
      sound_audible_(other.sound_audible_),
      spatial_sound_(other.spatial_sound_),
      sounds_(std::move(other.sounds_)),
      voices_(std::move(other.voices_)),
      owns_device_(other.owns_device_),
      limiter_(std::move(other.limiter_)) {
  other.mixer_ = nullptr;
  other.music_track_ = nullptr;
  other.music_sound_ = nullptr;
  other.sounds_.clear();
  other.voices_.clear();
}

AudioEngine& AudioEngine::operator=(AudioEngine&& other) noexcept {
  if (this != &other) {
    Close();
    mixer_ = other.mixer_;
    music_track_ = other.music_track_;
    music_sound_ = other.music_sound_;
    music_name_ = std::move(other.music_name_);
    music_volume_ = other.music_volume_;
    sound_volume_ = other.sound_volume_;
    channel_volumes_ = other.channel_volumes_;
    music_audible_ = other.music_audible_;
    sound_audible_ = other.sound_audible_;
    spatial_sound_ = other.spatial_sound_;
    sounds_ = std::move(other.sounds_);
    voices_ = std::move(other.voices_);
    owns_device_ = other.owns_device_;
    limiter_ = std::move(other.limiter_);
    other.mixer_ = nullptr;
    other.music_track_ = nullptr;
    other.music_sound_ = nullptr;
    other.sounds_.clear();
    other.voices_.clear();
  }
  return *this;
}

AudioEngine::~AudioEngine() { Close(); }

void AudioEngine::Close() noexcept {
  StopMusic();
  for (const Voice& voice : voices_) {
    MIX_DestroyTrack(AsTrack(voice.track));
  }
  voices_.clear();
  for (auto& [name, sound] : sounds_) {
    MIX_DestroyAudio(AsSound(sound.audio));
  }
  sounds_.clear();
  if (music_track_ != nullptr) {
    MIX_DestroyTrack(AsTrack(music_track_));
    music_track_ = nullptr;
  }
  if (mixer_ != nullptr) {
    // Le mélangeur part AVANT le limiteur : son fil ne l'appelle plus ensuite.
    MIX_DestroyMixer(AsMixer(mixer_));
    mixer_ = nullptr;
    MIX_Quit();
    if (owns_device_) {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
  }
}

namespace {}  // namespace

core::Result<AudioEngine> AudioEngine::Open() {
  // Le sous-système audio de SDL, explicitement : la fenêtre n'ouvre que la
  // vidéo, et rien ne garantit que le mélangeur le fera pour nous.
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  if (!MIX_Init()) {
    const std::string why = SDL_GetError();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, why});
  }
  // Le format du périphérique, et non le nôtre : le mélangeur reconvertit ce
  // qu'on lui donne, et lui imposer une fréquence ferait rééchantillonner deux
  // fois ce qui était déjà juste.
  MIX_Mixer* mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (mixer == nullptr) {
    const std::string why = SDL_GetError();
    MIX_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, why});
  }
  return Adopt(reinterpret_cast<Mixer*>(mixer), true);
}

core::Result<AudioEngine> AudioEngine::OpenOffline(int rate_hz, int channels) {
  // Ni périphérique ni sous-système audio : un mélangeur que l'on fait tourner
  // soi-même n'en demande aucun.
  if (!MIX_Init()) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  const SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = channels, .freq = rate_hz};
  MIX_Mixer* mixer = MIX_CreateMixer(&spec);
  if (mixer == nullptr) {
    const std::string why = SDL_GetError();
    MIX_Quit();
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, why});
  }
  return Adopt(reinterpret_cast<Mixer*>(mixer), false);
}

namespace {

// Appelé par le fil audio, sur le mélange entier, juste avant qu'il ne parte.
void SDLCALL LimitMix(void* userdata, MIX_Mixer* /*mixer*/, const SDL_AudioSpec* spec, float* pcm,
                      int samples) {
  if (userdata == nullptr || spec == nullptr || pcm == nullptr || samples <= 0) {
    return;
  }
  static_cast<PeakLimiter*>(userdata)->Process(
      std::span<float>(pcm, static_cast<std::size_t>(samples)), spec->channels, spec->freq);
}

}  // namespace

core::Result<AudioEngine> AudioEngine::Adopt(Mixer* mixer, bool owns_device) {
  AudioEngine engine;
  engine.mixer_ = mixer;
  engine.owns_device_ = owns_device;
  engine.limiter_ = std::make_unique<PeakLimiter>();
  // 🔴 UN ÉCART VOULU AU JEU D'ORIGINE, qui somme ses bruits à pleine échelle
  // et écrête ce qui dépasse : plusieurs bruits forts à la fois y grésillent.
  // Ici tout se somme sous `kMixHeadroom`, et le limiteur remonte le mélange
  // puis ne baisse le gain QUE le temps d'un dépassement. Un mélange qui tient
  // sous la pleine échelle sort bit pour bit tel qu'il était.
  if (!MIX_SetMixerGain(AsMixer(mixer), 1.0F / kMixHeadroom) ||
      !MIX_SetPostMixCallback(AsMixer(mixer), LimitMix, engine.limiter_.get())) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  return engine;
}

bool AudioEngine::Render(std::span<float> pcm) {
  if (mixer_ == nullptr || owns_device_) {
    return false;
  }
  const auto bytes = static_cast<int>(pcm.size_bytes());
  return MIX_Generate(AsMixer(mixer_), pcm.data(), bytes) >= 0;
}

std::vector<std::string> AudioEngine::Decoders() const {
  std::vector<std::string> names;
  const int count = MIX_GetNumAudioDecoders();
  names.reserve(static_cast<std::size_t>(std::max(count, 0)));
  for (int index = 0; index < count; ++index) {
    if (const char* name = MIX_GetAudioDecoder(index); name != nullptr) {
      names.emplace_back(name);
    }
  }
  return names;
}

void AudioEngine::SetMusicAudible(bool audible) {
  music_audible_ = audible;
  if (!audible) {
    // ⚠ ON COUPE POUR DE BON, et on ne baisse pas le volume : un morceau qui
    // continue à zéro garde ses octets et son décodage, et se retrouverait
    // AILLEURS dans le morceau au moment de le rétablir.
    StopMusic();
  }
}

void AudioEngine::SetSoundAudible(bool audible) {
  sound_audible_ = audible;
  // Les bruits en cours se taisent — ou reprennent — avec eux.
  RemixPlayingVoices();
  // Les bruits retenus NE SONT PAS oubliés : ils sont courts, déjà décodés, et
  // les relire au rétablissement rendrait le premier coup silencieux.
}

void AudioEngine::StopMusic() {
  if (music_track_ != nullptr) {
    MIX_StopTrack(AsTrack(music_track_), 0);
  }
  if (music_sound_ != nullptr) {
    MIX_DestroyAudio(AsSound(music_sound_));
    music_sound_ = nullptr;
  }
  music_name_.clear();
}

core::Result<void> AudioEngine::PlayMusic(std::string name, std::span<const std::uint8_t> bytes) {
  // ⚠ MUET, ON NE GARDE MÊME PAS LE NOM : le morceau devra être redemandé au
  // rétablissement, et retenir son nom ferait croire qu'il joue déjà — la garde
  // du « même morceau » ci-dessous le refuserait alors pour de bon.
  if (mixer_ == nullptr || !music_audible_) {
    return {};
  }
  // Le MÊME morceau ne se relance pas : deux cartes voisines le partagent
  // souvent, et le redémarrer à chaque porte franchie s'entendrait.
  if (!name.empty() && name == music_name_) {
    return {};
  }
  StopMusic();
  if (bytes.empty()) {
    return {};
  }

  // ⚠ LES OCTETS NE SONT LUS QUE PENDANT L'APPEL : le mélangeur en tire sa
  // propre copie, qu'il décode au fil de la lecture, et referme le flux avant
  // de rendre la main. En garder une ici en ferait un double.
  SDL_IOStream* stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
  if (stream == nullptr) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  // predecode faux : un morceau de plusieurs minutes tiendrait en mémoire
  // décompressé, pour être lu une fois.
  MIX_Audio* sound = MIX_LoadAudio_IO(AsMixer(mixer_), stream, false, true);
  if (sound == nullptr) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  music_sound_ = reinterpret_cast<Sound*>(sound);

  if (music_track_ == nullptr) {
    MIX_Track* track = MIX_CreateTrack(AsMixer(mixer_));
    if (track == nullptr) {
      StopMusic();
      return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
    }
    music_track_ = reinterpret_cast<Track*>(track);
  }
  if (!MIX_SetTrackAudio(AsTrack(music_track_), sound)) {
    StopMusic();
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  MIX_SetTrackGain(AsTrack(music_track_), VolumeAmplitude(music_volume_));

  const SDL_PropertiesID options = SDL_CreateProperties();
  SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, kLoopForever);
  const bool played = MIX_PlayTrack(AsTrack(music_track_), options);
  SDL_DestroyProperties(options);
  if (!played) {
    StopMusic();
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }

  music_name_ = std::move(name);
  return {};
}

void AudioEngine::SetMusicVolume(float volume) {
  music_volume_ = std::clamp(volume, 0.0F, kFullVolume);
  if (music_track_ != nullptr) {
    MIX_SetTrackGain(AsTrack(music_track_), VolumeAmplitude(music_volume_));
  }
}

void AudioEngine::SetSoundVolume(float volume) {
  sound_volume_ = std::clamp(volume, 0.0F, kFullVolume);
  RemixPlayingVoices();
}

void AudioEngine::SetChannelVolume(SoundChannel channel, float volume) {
  channel_volumes_[IndexOf(channel)] = std::clamp(volume, 0.0F, kFullVolume);
  RemixPlayingVoices();
}

void AudioEngine::SetSpatialSound(bool spatial) { spatial_sound_ = spatial; }

core::Result<void> AudioEngine::LoadSound(const std::string& name,
                                          std::span<const std::uint8_t> bytes) {
  if (mixer_ == nullptr || bytes.empty() || sounds_.contains(name)) {
    return {};
  }
  // ⚠ LES OCTETS NE SONT LUS QUE PENDANT L'APPEL, comme ceux de la musique : le
  // bruit sort d'ici décodé.
  SDL_IOStream* stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
  if (stream == nullptr) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  // predecode VRAI, à l'inverse de la musique : un bruit est court et se rejoue
  // souvent, et le décoder à chaque coup porté ferait un à-coup par coup.
  MIX_Audio* sound = MIX_LoadAudio_IO(AsMixer(mixer_), stream, true, true);
  if (sound == nullptr) {
    return std::unexpected(core::Error{core::ErrorCode::kIoFailure, SDL_GetError()});
  }
  sounds_.emplace(name, LoadedSound{.audio = reinterpret_cast<Sound*>(sound),
                                    .opens_on_step = OpeningStep(bytes) > kOpeningStepThreshold});
  return {};
}

bool AudioEngine::KnowsSound(const std::string& name) const { return sounds_.contains(name); }

void AudioEngine::ForgetSound(const std::string& name) {
  const auto found = sounds_.find(name);
  if (found == sounds_.end()) {
    return;
  }
  MIX_Audio* const audio = AsSound(found->second.audio);
  for (Voice& voice : voices_) {
    MIX_Track* const track = AsTrack(voice.track);
    if (voice.sound == found->second.audio) {
      // ⚠ AUCUN POINTEUR NE SURVIT AU DÉCODÉ : un bruit chargé après sa
      // libération pourrait recevoir son adresse, et se fondre dans cette voix.
      voice.sound = nullptr;
    }
    if (MIX_GetTrackAudio(track) == audio) {
      // Au repos, elle le rend ci-dessous ; en train de le jouer, elle
      // l'achève et le rendra au premier `PlaySound` qui la trouve au repos.
      voice.holds_forgotten = true;
    }
  }
  ReleaseForgottenVoices();
  MIX_DestroyAudio(audio);
  sounds_.erase(found);
}

void AudioEngine::ReleaseForgottenVoices() {
  for (Voice& voice : voices_) {
    // Seul ce fil lance une voix : celle qui ne joue pas ne repartira pas entre
    // le test et le détachement.
    if (voice.holds_forgotten && !MIX_TrackPlaying(AsTrack(voice.track))) {
      MIX_SetTrackAudio(AsTrack(voice.track), nullptr);
      voice.holds_forgotten = false;
    }
  }
}

std::size_t AudioEngine::VoicesHoldingForgottenSounds() const {
  return static_cast<std::size_t>(
      std::ranges::count_if(voices_, [](const Voice& voice) { return voice.holds_forgotten; }));
}

std::vector<std::string> AudioEngine::HeldSoundNames() const {
  std::vector<std::string> names;
  names.reserve(sounds_.size());
  for (const auto& [name, sound] : sounds_) {
    names.push_back(name);
  }
  return names;
}

AudioEngine::Voice* AudioEngine::FreeVoice() {
  for (Voice& voice : voices_) {
    if (!MIX_TrackPlaying(AsTrack(voice.track))) {
      return &voice;
    }
  }
  if (voices_.size() >= kSoundVoices) {
    // Toutes les voix chantent : le bruit de trop se PERD, plutôt que d'en
    // couper un autre au hasard. C'est ce que fait le jeu, et cela s'entend
    // moins qu'un son tranché net.
    return nullptr;
  }
  MIX_Track* track = MIX_CreateTrack(AsMixer(mixer_));
  if (track == nullptr) {
    return nullptr;
  }
  voices_.push_back(Voice{.track = reinterpret_cast<Track*>(track)});
  return &voices_.back();
}

float AudioEngine::GainOf(const Voice& voice) const {
  if (!sound_audible_) {
    return 0.0F;
  }
  return std::clamp(
      MixedSoundGain(voice.asked_gain, sound_volume_, channel_volumes_[IndexOf(voice.channel)]),
      0.0F, kFullVolume);
}

void AudioEngine::RemixPlayingVoices() {
  for (const Voice& voice : voices_) {
    if (MIX_TrackPlaying(AsTrack(voice.track))) {
      MIX_SetTrackGain(AsTrack(voice.track), GainOf(voice));
    }
  }
}

void AudioEngine::PlaySound(const std::string& name, float gain, float pan, SoundChannel channel) {
  // ⚠ AVANT TOUTE GARDE : une voix qui achevait un bruit oublié ne rend son
  // décodé que détachée, et le bruit qu'on demande ici peut très bien ne pas
  // la prendre.
  ReleaseForgottenVoices();
  // Le réglage du joueur MULTIPLIE ce que le site d'appel demande : la distance
  // et la direction restent celles de la scène, le volume général et celui de
  // la famille les couvrent.
  const float wanted = MixedSoundGain(gain, sound_volume_, channel_volumes_[IndexOf(channel)]);
  if (mixer_ == nullptr || !sound_audible_ || !(wanted > 0.0F)) {
    return;
  }
  const auto found = sounds_.find(name);
  if (found == sounds_.end()) {
    return;
  }
  const Sound* sound = found->second.audio;
  const std::uint64_t now_ms = SDL_GetTicks();

  // LE MÊME BRUIT, PARTI À L'INSTANT DANS LA MÊME FAMILLE : on le renforce au
  // lieu de le doubler. Voir `MergeOnset`.
  for (Voice& playing : voices_) {
    if (playing.sound == sound && playing.channel == channel &&
        SameOnset(playing.started_ms, now_ms) && MIX_TrackPlaying(AsTrack(playing.track))) {
      const MergedOnset merged = MergeOnset(playing.asked_gain, playing.asked_pan, gain, pan);
      playing.asked_gain = merged.gain;
      playing.asked_pan = merged.pan;
      MIX_SetTrackGain(AsTrack(playing.track), GainOf(playing));
      ApplySide(playing);
      return;
    }
  }

  Voice* free = FreeVoice();
  if (free == nullptr) {
    return;
  }
  Track* voice = free->track;
  if (!MIX_SetTrackAudio(AsTrack(voice), AsSound(found->second.audio))) {
    return;
  }
  // LA VOIX RETIENT CE QU'ON LUI DEMANDE, et non son gain final : c'est ce qui
  // permet de le recalculer quand un curseur bouge pendant qu'elle joue.
  free->asked_gain = gain;
  free->asked_pan = pan;
  free->channel = channel;
  free->sound = sound;
  free->started_ms = now_ms;
  free->holds_forgotten = false;
  MIX_SetTrackGain(AsTrack(voice), GainOf(*free));
  ApplySide(*free);

  // UN BRUIT QUI CLAQUE AU DÉPART entre par un fondu très court — voir
  // `OpeningStep`. Un écart voulu au jeu d'origine, qui le laisse claquer ;
  // les autres bruits partent tels quels.
  const SDL_PropertiesID options = found->second.opens_on_step ? SDL_CreateProperties() : 0;
  if (options != 0) {
    SDL_SetNumberProperty(options, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, kOpeningFadeMs);
  }
  MIX_PlayTrack(AsTrack(voice), options);
  if (options != 0) {
    SDL_DestroyProperties(options);
  }
}

void AudioEngine::StopSounds(SoundChannel channel) {
  for (Voice& voice : voices_) {
    if (voice.channel != channel || !MIX_TrackPlaying(AsTrack(voice.track))) {
      continue;
    }
    MIX_StopTrack(AsTrack(voice.track),
                  MIX_TrackMSToFrames(AsTrack(voice.track), kSilencingFadeMs));
    // ⚠ UNE VOIX QUI S'ÉTEINT NE SE REJOINT PLUS : le même bruit relancé
    // pendant son fondu s'y fondrait, et s'éteindrait avec elle.
    voice.sound = nullptr;
  }
}

void AudioEngine::StopSound(const std::string& name) {
  const auto found = sounds_.find(name);
  if (found == sounds_.end()) {
    return;
  }
  for (Voice& voice : voices_) {
    if (voice.sound != found->second.audio || !MIX_TrackPlaying(AsTrack(voice.track))) {
      continue;
    }
    MIX_StopTrack(AsTrack(voice.track),
                  MIX_TrackMSToFrames(AsTrack(voice.track), kSilencingFadeMs));
    // Comme pour une famille : la voix qui s'éteint ne se rejoint plus.
    voice.sound = nullptr;
  }
}

void AudioEngine::ApplySide(const Voice& voice) const {
  // LE CÔTÉ, en gains de voie. On s'écarte du centre sans jamais passer sous
  // zéro.
  //
  // ⚠ POSÉ MÊME QUAND ON N'EN VEUT PAS, et c'est nécessaire : les voix se
  // prennent dans une réserve, et celle qu'on vient d'obtenir garde les gains
  // du bruit d'avant. Ne rien écrire ici laisserait un son censé être centré
  // sortir du côté où jouait le précédent.
  MIX_StereoGains gains;
  if (spatial_sound_) {
    const float side = std::clamp(voice.asked_pan, -1.0F, 1.0F);
    gains.left = std::clamp(1.0F - std::max(side, 0.0F), 0.0F, kFullVolume);
    gains.right = std::clamp(1.0F + std::min(side, 0.0F), 0.0F, kFullVolume);
  } else {
    // ⚠ CENTRÉ, ET NON MUET D'UN CÔTÉ : c'est le mode 2D du natif, qui ne perd
    // que la direction. La distance, elle, est déjà dans le gain de la voix.
    gains.left = kFullVolume;
    gains.right = kFullVolume;
  }
  MIX_SetTrackStereo(AsTrack(voice.track), &gains);
}

}  // namespace ro::audio
