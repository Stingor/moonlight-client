// Les réglages de la voix en jeu. Voir l'en-tête.

#include "options/voice_options.h"

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr const char* kActivationKey = "voice_activation_percent";

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
  into.Declare(kReleaseKey, &settings_.release_ms);
  into.Declare(kActivationKey, &settings_.activation_percent);
}

void VoiceOptions::ListDevicesWith(DeviceLister microphones, DeviceLister speakers) {
  list_microphones_ = std::move(microphones);
  list_speakers_ = std::move(speakers);
}

VoiceSettings VoiceOptions::settings() const noexcept {
  VoiceSettings sane = settings_;
  sane.channel = std::clamp(sane.channel, 0, static_cast<int>(kVoiceChosenChannelCount) - 1);
  sane.output_volume = std::clamp(sane.output_volume, 0.0F, 1.0F);
  sane.input_gain = std::clamp(sane.input_gain, 0.0F, kVoiceMostInputGain);
  sane.speaking_icon =
      std::clamp(sane.speaking_icon, 0, static_cast<int>(SpeakingIcon::kCount) - 1);
  sane.talk_mode = std::clamp(sane.talk_mode, 0, static_cast<int>(VoiceTalkMode::kCount) - 1);
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
  if (live_.capturing) {
    talk_key_text_ = Tr("Appuyez sur une touche - Échap annule, Suppr retire le raccourci.");
  } else {
    talk_key_text_ = std::string(Tr("Touche pour parler :")) + " " +
                     (live_.talk_key.empty() ? std::string(Tr("aucune")) : live_.talk_key);
  }
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kTalkKey),
      .label = talk_key_text_.c_str(),
      .help = Tr("On parle en tenant cette touche. C'est la même que « Parler (voix) » dans les "
                 "raccourcis clavier."),
      .kind = OptionKind::kAction,
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
  rows.push_back(OptionRowView{
      .id = VoiceRowId(VoiceRow::kChannel),
      .label = Tr("Canal de la voix"),
      .help = Tr("Où l'on parle et qui l'on entend : les joueurs proches, le groupe ou la guilde. "
                 "Dans un salon de discussion, la voix est celle du salon."),
      .kind = OptionKind::kChoice,
      .chosen = sane.channel,
      .choices = channel_names_,
      .disabled = !sane.enabled});
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
      talk_key_asked_ = true;
      break;
    case VoiceRow::kSpeakingIcon:
      settings_.speaking_icon = asked.chosen;
      break;
    case VoiceRow::kTalkMode:
      settings_.talk_mode = asked.chosen;
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
