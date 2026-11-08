// Les réglages de la voix en jeu. Voir l'en-tête.

#include "options/voice_options.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"

#include "options/percent.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. ⚠ GRAVÉES, et en anglais : les changer rend à tous les
// joueurs les réglages d'origine, sans que rien ne le dise.
constexpr const char* kEnabledKey = "voice_enabled";
constexpr const char* kChannelKey = "voice_channel";
constexpr const char* kOutputVolumeKey = "voice_output_volume";
constexpr const char* kInputGainKey = "voice_input_gain";
constexpr const char* kMicrophoneKey = "voice_microphone";
constexpr const char* kSpeakersKey = "voice_speakers";
constexpr const char* kSpeakingIconKey = "voice_speaking_icon";
constexpr const char* kTalkModeKey = "voice_talk_mode";
constexpr const char* kReleaseKey = "voice_release_ms";
constexpr const char* kPressKey = "voice_press_ms";
constexpr const char* kActivationKey = "voice_activation_percent";
constexpr const char* kListenedKey = "voice_listened";
constexpr const char* kSelfMutedKey = "voice_self_muted";
constexpr const char* kDeafenedKey = "voice_deafened";
constexpr const char* kBarShownKey = "voice_bar_shown";
constexpr const char* kBarLockedKey = "voice_bar_locked";
constexpr const char* kBarScaleKey = "voice_bar_scale";
constexpr const char* kBarOpacityKey = "voice_bar_opacity";
constexpr const char* kBarVerticalKey = "voice_bar_vertical";
constexpr const char* kBarSpeakersKey = "voice_bar_speakers";
constexpr const char* kBarFadeKey = "voice_bar_fade";

// La ligne de la touche d'un canal, au rang du canal.
constexpr std::array<VoiceRow, kVoiceChosenChannelCount> kTalkKeyRows = {
    VoiceRow::kTalkNearbyKey, VoiceRow::kTalkPartyKey, VoiceRow::kTalkGuildKey};
constexpr std::array<VoiceRow, kVoiceChosenChannelCount> kListenRows = {
    VoiceRow::kListenNearby, VoiceRow::kListenParty, VoiceRow::kListenGuild};

// Les images, rangées comme `SpeakingIcon`.
constexpr std::array<std::string_view, static_cast<std::size_t>(SpeakingIcon::kCount)>
    kSpeakingIconPaths = {
        "data\\ui_icons\\voice_microphone.png",     "data\\ui_icons\\voice_speaking_head.png",
        "data\\ui_icons\\voice_speech_balloon.png", "data\\ui_icons\\voice_speaker.png",
        "data\\ui_icons\\voice_musical_note.png",   ""};

void ComposeChoice(const std::vector<std::string>& known, const std::string& kept,
                   VoiceDeviceChoice& into) {
  into.names.assign(1, std::string());
  into.labels.assign(1, Tr("Celui du système"));
  into.chosen = 0;
  for (const std::string& name : known) {
    if (name == kept) into.chosen = static_cast<int>(into.names.size());
    into.names.push_back(name);
    into.labels.push_back(name);
  }
  if (!kept.empty() && into.chosen == 0) {
    into.chosen = static_cast<int>(into.names.size());
    into.names.push_back(kept);
    into.labels.push_back(kept + " " + Tr("(débranché)"));
  }
  into.shown.clear();
  for (const std::string& label : into.labels) into.shown.push_back(label.c_str());
}

void Choose(const VoiceDeviceChoice& choice, int chosen, std::string& into) {
  if (chosen >= 0 && static_cast<std::size_t>(chosen) < choice.names.size()) {
    into = choice.names[static_cast<std::size_t>(chosen)];
  }
}

// Ce que dit la ligne d'une touche : la touche posée, ou l'attente du geste.
[[nodiscard]] std::string TalkKeyText(const VoiceLiveView& live, VoiceTalkKey key,
                                      const char* named) {
  if (live.capturing == key) {
    return Tr("Appuyez sur une touche - Échap annule, Suppr retire le raccourci.");
  }
  const std::string& bound = live.talk_keys[static_cast<std::size_t>(key)];
  return std::string(named) + " " + (bound.empty() ? std::string(Tr("aucune")) : bound);
}

}  // namespace

std::string_view SpeakingIconPath(SpeakingIcon icon) {
  const auto rank = static_cast<std::size_t>(icon);
  return rank < kSpeakingIconPaths.size() ? kSpeakingIconPaths[rank] : std::string_view();
}

void VoiceOptions::Declare(SettingsFile& into) {
  into.Declare(kEnabledKey, &settings_.enabled);
  into.Declare(kChannelKey, &settings_.channel);
  into.Declare(kOutputVolumeKey, &settings_.output_volume);
  into.Declare(kInputGainKey, &settings_.input_gain);
  into.Declare(kMicrophoneKey, &settings_.microphone);
  into.Declare(kSpeakersKey, &settings_.speakers);
  into.Declare(kSpeakingIconKey, &settings_.speaking_icon);
  into.Declare(kTalkModeKey, &settings_.talk_mode);
  into.Declare(kPressKey, &settings_.press_ms);
  into.Declare(kReleaseKey, &settings_.release_ms);
  into.Declare(kActivationKey, &settings_.activation_percent);
  into.Declare(kListenedKey, &settings_.listened);
  into.Declare(kSelfMutedKey, &settings_.self_muted);
  into.Declare(kDeafenedKey, &settings_.deafened);
  into.Declare(kBarShownKey, &settings_.bar_shown);
  into.Declare(kBarLockedKey, &settings_.bar_locked);
  into.Declare(kBarScaleKey, &settings_.bar_scale_percent);
  into.Declare(kBarOpacityKey, &settings_.bar_opacity_percent);
  into.Declare(kBarVerticalKey, &settings_.bar_vertical);
  into.Declare(kBarSpeakersKey, &settings_.bar_speakers);
  into.Declare(kBarFadeKey, &settings_.bar_fade);
}

std::optional<ro::ui::OptionsWindowAction> VoiceBarAction(const ro::ui::VoiceBarAnswer& answer,
                                                          const VoiceSettings& now) {
  ro::ui::OptionsWindowAction asked;
  const auto channels = static_cast<int>(kVoiceChosenChannelCount);
  if (answer.toggle_mute) {
    asked.changed = VoiceRowId(VoiceRow::kSelfMuted);
    asked.on = !now.self_muted;
  } else if (answer.toggle_deafen) {
    asked.changed = VoiceRowId(VoiceRow::kDeafened);
    asked.on = !now.deafened;
  } else if (answer.make_default >= 0 && answer.make_default < channels &&
             answer.make_default != now.channel) {
    asked.changed = VoiceRowId(VoiceRow::kChannel);
    asked.chosen = answer.make_default;
  } else if (answer.toggle_listen >= 0 && answer.toggle_listen < channels &&
             answer.toggle_listen != now.channel) {
    // ⚠ LE CANAL PAR DÉFAUT S'ÉCOUTE TOUJOURS : sa pastille ne l'éteint pas.
    asked.changed = VoiceRowId(kListenRows[static_cast<std::size_t>(answer.toggle_listen)]);
    asked.on = (now.listened & VoiceListenBit(answer.toggle_listen)) == 0;
  } else {
    return std::nullopt;
  }
  return asked;
}

bool VoicePressGate::Pass(bool held, std::uint32_t now_ms, std::uint32_t least_ms) noexcept {
  if (!held) {
    held_since_ms_.reset();
    return false;
  }
  if (!held_since_ms_.has_value()) held_since_ms_ = now_ms;
  return now_ms - *held_since_ms_ >= least_ms;
}

void VoiceOptions::ListDevicesWith(DeviceLister microphones, DeviceLister speakers) {
  list_microphones_ = std::move(microphones);
  list_speakers_ = std::move(speakers);
}

VoiceSettings VoiceOptions::settings() const noexcept {
  VoiceSettings sane = settings_;
  sane.channel = std::clamp(sane.channel, 0, static_cast<int>(kVoiceChosenChannelCount) - 1);
  sane.listened &= kVoiceListenEvery;
  sane.bar_scale_percent =
      std::clamp(sane.bar_scale_percent, kVoiceBarLeastScalePercent, kVoiceBarMostScalePercent);
  sane.bar_opacity_percent =
      std::clamp(sane.bar_opacity_percent, kVoiceBarLeastOpacityPercent, kPercent);
  sane.output_volume = std::clamp(sane.output_volume, 0.0F, 1.0F);
  sane.input_gain = std::clamp(sane.input_gain, 0.0F, kVoiceMostInputGain);
  sane.speaking_icon =
      std::clamp(sane.speaking_icon, 0, static_cast<int>(SpeakingIcon::kCount) - 1);
  sane.talk_mode = std::clamp(sane.talk_mode, 0, static_cast<int>(VoiceTalkMode::kCount) - 1);
  sane.press_ms = std::clamp(sane.press_ms, 0, kVoiceMostPressMs);
  sane.release_ms = std::clamp(sane.release_ms, 0, kVoiceMostReleaseMs);
  sane.activation_percent = std::clamp(sane.activation_percent, 0.0F, kPercent);
  return sane;
}

void VoiceOptions::ComposeInto(std::vector<OptionRowView>& rows) {
  const VoiceSettings sane = settings();
  channel_names_ = {Tr("Proximité"), Tr("Groupe"), Tr("Guilde")};
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kEnabled),
      .label = Tr("Voix en jeu"),
      .help =
          Tr("Entendre les autres joueurs, et leur parler en tenant la touche « Parler (voix) » "
             "des raccourcis. Seulement sur un serveur qui ouvre la voix."),
      .kind = OptionKind::kToggle,
      .on = sane.enabled});
  rows.back().heading = Tr("Voix");
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kSelfMuted),
                               .label = Tr("Micro coupé"),
                               .help = Tr("Rien ne part, ni à la touche ni à la voix."),
                               .kind = OptionKind::kToggle,
                               .on = sane.self_muted,
                               .indented = true,
                               .disabled = !sane.enabled});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kDeafened),
                               .label = Tr("Sourdine"),
                               .help = Tr("Vous n'entendez plus personne, et rien ne part."),
                               .kind = OptionKind::kToggle,
                               .on = sane.deafened,
                               .same_line = true,
                               .disabled = !sane.enabled});
  const bool by_voice = sane.talk_mode == static_cast<int>(VoiceTalkMode::kVoice);
  mode_names_ = {Tr("En tenant la touche"), Tr("À la voix")};
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kTalkMode),
      .label = Tr("Mode de parole"),
      .help = Tr("En tenant la touche, rien ne part sans elle. À la voix, le micro part dès qu'il "
                 "vous entend plus fort que le seuil ; la touche marche aussi."),
      .kind = OptionKind::kChoice,
      .chosen = sane.talk_mode,
      .choices = mode_names_,
      .disabled = !sane.enabled});
  talk_key_texts_[static_cast<std::size_t>(VoiceTalkKey::kDefault)] =
      TalkKeyText(live_, VoiceTalkKey::kDefault, Tr("Touche pour parler :"));
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kTalkKey),
      .label = talk_key_texts_[static_cast<std::size_t>(VoiceTalkKey::kDefault)].c_str(),
      .help = Tr("On parle sur le canal par défaut en tenant cette touche. C'est la même que "
                 "« Parler (voix) » dans les raccourcis clavier."),
      .kind = OptionKind::kAction,
      .disabled = !sane.enabled});
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kPress),
      .label = Tr("Appui minimal"),
      .help = Tr("Combien tenir une touche de la voix avant que le micro parte : une lettre "
                 "frappée en écrivant dans la conversation ne vous fait pas parler. 0 : dès "
                 "l'appui."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(sane.press_ms),
      .lowest = 0.0F,
      .highest = static_cast<float>(kVoiceMostPressMs),
      .format = "%.0f ms",
      .disabled = !sane.enabled});
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kRelease),
      .label = Tr("Délai au relâchement"),
      .help = Tr("Combien la parole tient encore après la touche lâchée, ou après votre dernier "
                 "mot à la voix : une fin de phrase ne se coupe pas."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(sane.release_ms),
      .lowest = 0.0F,
      .highest = static_cast<float>(kVoiceMostReleaseMs),
      .format = "%.0f ms",
      .disabled = !sane.enabled});
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kActivation),
      .label = Tr("Seuil de la voix"),
      .help = Tr("Le micro part quand son niveau dépasse ce seuil. Réglez-le au-dessus du niveau "
                 "de votre pièce en silence, et sous celui de votre voix."),
      .kind = OptionKind::kSlider,
      .value = sane.activation_percent,
      .lowest = 0.0F,
      .highest = kPercent,
      .format = "%.0f %%",
      .disabled = !sane.enabled || !by_voice});
  level_text_ =
      std::string(Tr("Niveau du micro :")) + " " +
      (live_.input_percent.has_value() ? std::to_string(std::lround(*live_.input_percent)) + " %"
                                       : std::string(Tr("voix fermée")));
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kInputLevel),
                               .label = level_text_.c_str(),
                               .kind = OptionKind::kNote});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kOutputVolume),
                               .label = Tr("Volume des voix"),
                               .kind = OptionKind::kSlider,
                               .value = sane.output_volume * kPercent,
                               .lowest = 0.0F,
                               .highest = kPercent,
                               .format = "%.0f %%",
                               .disabled = !sane.enabled});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kInputVolume),
                               .label = Tr("Volume du micro"),
                               .help = Tr("Au-delà de 100 %, le micro est amplifié."),
                               .kind = OptionKind::kSlider,
                               .value = sane.input_gain * kPercent,
                               .lowest = 0.0F,
                               .highest = kVoiceMostInputGain * kPercent,
                               .format = "%.0f %%",
                               .disabled = !sane.enabled});
  ComposeChoice(list_microphones_ ? list_microphones_() : std::vector<std::string>(),
                sane.microphone, microphone_choice_);
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kMicrophone),
      .label = Tr("Micro de la voix"),
      .help = Tr("Le micro où l'on parle. Un casque débranché laisse la place à celui du "
                 "système, jusqu'à son retour."),
      .kind = OptionKind::kChoice,
      .chosen = microphone_choice_.chosen,
      .choices = microphone_choice_.shown,
      .disabled = !sane.enabled});
  ComposeChoice(list_speakers_ ? list_speakers_() : std::vector<std::string>(), sane.speakers,
                speakers_choice_);
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kSpeakers),
                               .label = Tr("Sortie des voix"),
                               .help = Tr("Où l'on entend les autres joueurs."),
                               .kind = OptionKind::kChoice,
                               .chosen = speakers_choice_.chosen,
                               .choices = speakers_choice_.shown,
                               .disabled = !sane.enabled});
  icon_names_ = {Tr("Micro"),        Tr("Tête qui parle"),  Tr("Bulle"),
                 Tr("Haut-parleur"), Tr("Note de musique"), Tr("Aucune")};
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kSpeakingIcon),
      .label = Tr("Icône de parole"),
      .help = Tr("Ce qui s'affiche au-dessus de qui parle, vous compris. Seul votre écran change."),
      .kind = OptionKind::kPicture,
      .chosen = sane.speaking_icon,
      .choices = icon_names_,
      // ⚠ PAS UNE IMAGE, PAS DE CASES : six cercles barrés ne disent rien.
      .pictures = std::ranges::any_of(live_.icons, &ro::ui::SkinImage::usable)
                      ? std::span<const ro::ui::SkinImage>(live_.icons)
                      : std::span<const ro::ui::SkinImage>(),
      .disabled = !sane.enabled});
  rows.push_back(
      OptionRowView{.id = VoiceRowId(VoiceRow::kBlocks),
                    .label = Tr("Joueurs bloqués…"),
                    .help = Tr("Ceux que vous n'entendez plus, ou dont vous ne recevez plus les "
                               "chuchotements : on les y débloque, on en bloque d'autres."),
                    .command = "/blocage",
                    .kind = OptionKind::kAction});
  ComposeChannelsInto(rows, sane);
  ComposeBarInto(rows, sane);
}

void VoiceOptions::ComposeBarInto(std::vector<OptionRowView>& rows, const VoiceSettings& sane) {
  const bool off = !sane.enabled || !sane.bar_shown;
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kBarShown),
      .label = Tr("Barre de la voix"),
      .help = Tr("Une petite barre à placer où l'on veut : le micro, la sourdine, et une pastille "
                 "par canal. Un clic écoute un canal ou ne l'écoute plus, un clic droit en fait le "
                 "canal par défaut. Sans elle, qui parle s'affiche sur le bord gauche."),
      .kind = OptionKind::kToggle,
      .on = sane.bar_shown,
      .disabled = !sane.enabled});
  rows.back().heading = Tr("Barre de la voix");
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kBarLocked),
                               .label = Tr("Verrouillée"),
                               .help = Tr("Elle ne se déplace plus à la souris."),
                               .kind = OptionKind::kToggle,
                               .on = sane.bar_locked,
                               .indented = true,
                               .disabled = off});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kBarVertical),
                               .label = Tr("En colonne"),
                               .kind = OptionKind::kToggle,
                               .on = sane.bar_vertical,
                               .same_line = true,
                               .disabled = off});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kBarSpeakers),
                               .label = Tr("Qui parle, dessous"),
                               .kind = OptionKind::kToggle,
                               .on = sane.bar_speakers,
                               .indented = true,
                               .disabled = off});
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kBarFade),
      .label = Tr("Estompée au repos"),
      .help = Tr("Presque transparente quand personne ne parle et que la souris est ailleurs."),
      .kind = OptionKind::kToggle,
      .on = sane.bar_fade,
      .same_line = true,
      .disabled = off});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kBarScale),
                               .label = Tr("Taille de la barre"),
                               .kind = OptionKind::kSlider,
                               .value = sane.bar_scale_percent,
                               .lowest = kVoiceBarLeastScalePercent,
                               .highest = kVoiceBarMostScalePercent,
                               .format = "%.0f %%",
                               .indented = true,
                               .disabled = off});
  rows.push_back(OptionRowView{.id = VoiceRowId(VoiceRow::kBarOpacity),
                               .label = Tr("Opacité de la barre"),
                               .kind = OptionKind::kSlider,
                               .value = sane.bar_opacity_percent,
                               .lowest = kVoiceBarLeastOpacityPercent,
                               .highest = kPercent,
                               .format = "%.0f %%",
                               .indented = true,
                               .disabled = off});
}

void VoiceOptions::ComposeChannelsInto(std::vector<OptionRowView>& rows,
                                       const VoiceSettings& sane) {
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kChannel),
      .label = Tr("Canal par défaut"),
      .help = Tr("Où part la voix avec la touche « Parler (voix) », et à la voix. Vous l'écoutez "
                 "toujours. Dans un salon de discussion, la voix est celle du salon."),
      .kind = OptionKind::kChoice,
      .chosen = sane.channel,
      .choices = channel_names_,
      .disabled = !sane.enabled});
  rows.back().heading = Tr("Canaux");
  for (std::size_t channel = 0; channel < kVoiceChosenChannelCount; ++channel) {
    const bool shown = (sane.listened & VoiceListenBit(static_cast<int>(channel))) != 0;
    rows.push_back(OptionRowView{
        .id = VoiceRowId(kListenRows[channel]),
        .label = channel_names_[channel],
        .help = Tr("Les canaux dont vous entendez les voix, plusieurs à la fois. Un joueur ne "
                   "vous entend que s'il écoute le canal où vous parlez."),
        .kind = OptionKind::kToggle,
        // ⚠ LE CANAL PAR DÉFAUT S'ÉCOUTE TOUJOURS : sa case le dit, cochée.
        .on = shown || static_cast<int>(channel) == sane.channel,
        .same_line = channel != 0,
        .disabled = !sane.enabled || static_cast<int>(channel) == sane.channel});
  }
  rows[rows.size() - kVoiceChosenChannelCount].heading = Tr("Canaux écoutés");
  const std::array<const char*, kVoiceChosenChannelCount> named = {
      Tr("Parler en proximité :"), Tr("Parler au groupe :"), Tr("Parler à la guilde :")};
  for (std::size_t channel = 0; channel < kVoiceChosenChannelCount; ++channel) {
    const auto key = static_cast<VoiceTalkKey>(channel + 1);
    talk_key_texts_[channel + 1] = TalkKeyText(live_, key, named[channel]);
    rows.push_back(OptionRowView{
        .id = VoiceRowId(kTalkKeyRows[channel]),
        .label = talk_key_texts_[channel + 1].c_str(),
        .help = Tr("On parle sur ce canal en tenant sa touche, quel que soit le canal par "
                   "défaut, et on l'écoute pendant ce temps. Ce sont aussi des raccourcis "
                   "clavier."),
        .kind = OptionKind::kAction,
        .disabled = !sane.enabled});
  }
  rows[rows.size() - kVoiceChosenChannelCount].heading = Tr("Touche de chaque canal");
}

bool VoiceOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kVoiceRowBase || asked.changed >= VoiceRowId(VoiceRow::kCount)) {
    return false;
  }
  switch (static_cast<VoiceRow>(asked.changed - kVoiceRowBase)) {
    case VoiceRow::kEnabled:
      settings_.enabled = asked.on;
      break;
    case VoiceRow::kChannel:
      settings_.channel = asked.chosen;
      break;
    case VoiceRow::kOutputVolume:
      settings_.output_volume = asked.value / kPercent;
      break;
    case VoiceRow::kInputVolume:
      settings_.input_gain = asked.value / kPercent;
      break;
    case VoiceRow::kMicrophone:
      Choose(microphone_choice_, asked.chosen, settings_.microphone);
      break;
    case VoiceRow::kSpeakers:
      Choose(speakers_choice_, asked.chosen, settings_.speakers);
      break;
    case VoiceRow::kTalkKey:
      talk_key_asked_ = VoiceTalkKey::kDefault;
      break;
    case VoiceRow::kTalkNearbyKey:
      talk_key_asked_ = VoiceTalkKey::kNearby;
      break;
    case VoiceRow::kTalkPartyKey:
      talk_key_asked_ = VoiceTalkKey::kParty;
      break;
    case VoiceRow::kTalkGuildKey:
      talk_key_asked_ = VoiceTalkKey::kGuild;
      break;
    case VoiceRow::kSelfMuted:
      settings_.self_muted = asked.on;
      break;
    case VoiceRow::kBarShown:
      settings_.bar_shown = asked.on;
      break;
    case VoiceRow::kBarLocked:
      settings_.bar_locked = asked.on;
      break;
    case VoiceRow::kBarVertical:
      settings_.bar_vertical = asked.on;
      break;
    case VoiceRow::kBarSpeakers:
      settings_.bar_speakers = asked.on;
      break;
    case VoiceRow::kBarFade:
      settings_.bar_fade = asked.on;
      break;
    case VoiceRow::kBarScale:
      settings_.bar_scale_percent = asked.value;
      break;
    case VoiceRow::kBarOpacity:
      settings_.bar_opacity_percent = asked.value;
      break;
    case VoiceRow::kDeafened:
      settings_.deafened = asked.on;
      break;
    case VoiceRow::kListenNearby:
    case VoiceRow::kListenParty:
    case VoiceRow::kListenGuild: {
      const int bit = VoiceListenBit(asked.changed - VoiceRowId(VoiceRow::kListenNearby));
      settings_.listened = asked.on ? (settings().listened | bit) : (settings().listened & ~bit);
      break;
    }
    case VoiceRow::kSpeakingIcon:
      settings_.speaking_icon = asked.chosen;
      break;
    case VoiceRow::kTalkMode:
      settings_.talk_mode = asked.chosen;
      break;
    case VoiceRow::kPress:
      settings_.press_ms = static_cast<int>(std::lround(asked.value));
      break;
    case VoiceRow::kRelease:
      settings_.release_ms = static_cast<int>(std::lround(asked.value));
      break;
    case VoiceRow::kActivation:
      settings_.activation_percent = asked.value;
      break;
    case VoiceRow::kInputLevel:
    case VoiceRow::kBlocks:
    case VoiceRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
