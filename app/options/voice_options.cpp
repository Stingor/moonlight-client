// Les réglages de la voix en jeu. Voir l'en-tête.

#include "options/voice_options.h"

#include <algorithm>
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

void VoiceOptions::Declare(SettingsFile& into) {
  into.Declare(kEnabledKey, &settings_.enabled);
  into.Declare(kChannelKey, &settings_.channel);
  into.Declare(kOutputVolumeKey, &settings_.output_volume);
  into.Declare(kInputGainKey, &settings_.input_gain);
  into.Declare(kMicrophoneKey, &settings_.microphone);
  into.Declare(kSpeakersKey, &settings_.speakers);
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
    case VoiceRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
