#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/voice_overlay.h"

#include "game/settings_file.h"
#include "options/percent.h"

namespace moonclient {

// ── LA VOIX EN JEU, DANS LA PAGE « SON » ────────────────────────────────────
//
// L'interrupteur, la touche « Parler », le canal où l'on parle et où l'on
// écoute, le volume des voix et celui du micro, les deux appareils, l'icône
// au-dessus de qui parle, et comment le micro part : à la touche ou à la voix,
// après un appui tenu assez longtemps, avec un délai au relâchement et un
// seuil qu'on règle devant le niveau du micro. Le côté d'où vient une voix suit « Son spatialisé ».
// Voir l'ADR 0135.
inline constexpr int kVoiceRowBase = 9400;

enum class VoiceRow : int {
  kEnabled = 0,
  kChannel,
  kOutputVolume,
  kInputVolume,
  kMicrophone,
  kSpeakers,
  kTalkKey,
  kSpeakingIcon,
  kTalkMode,
  kRelease,
  kActivation,
  kInputLevel,
  // Ouvre la fenêtre des joueurs bloqués : un bouton, pas un réglage.
  kBlocks,
  kSelfMuted,
  kDeafened,
  kListenNearby,
  kListenParty,
  kListenGuild,
  kTalkNearbyKey,
  kTalkPartyKey,
  kTalkGuildKey,
  kBarShown,
  kBarLocked,
  kBarScale,
  kBarOpacity,
  kBarVertical,
  kBarSpeakers,
  kBarFade,
  kPress,
  kCount,
};

// LES TOUCHES DE LA VOIX : « Parler », sur le canal par défaut, puis celle de
// chaque canal, au rang du canal plus un.
enum class VoiceTalkKey : std::uint8_t {
  kDefault,
  kNearby,
  kParty,
  kGuild,
  kCount,
};
inline constexpr std::size_t kVoiceTalkKeyCount = static_cast<std::size_t>(VoiceTalkKey::kCount);

// COMMENT LE MICRO PART. ⚠ LE RANG EST GARDÉ DANS LE FICHIER.
enum class VoiceTalkMode : std::uint8_t {
  kPushToTalk,  // en tenant la touche « Parler »
  kVoice,       // dès que la voix dépasse le seuil ; la touche marche aussi
  kCount,
};

// LE SEUIL ET LE NIVEAU DU MICRO se montrent en pour cent de ce niveau, une
// moyenne quadratique sur une échelle où 1 est le plus fort possible : une voix
// ordinaire tient dans la première moitié.
inline constexpr float kVoiceLevelAtFullScale = 0.2F;
inline constexpr float kVoiceDefaultActivationPercent = 15.0F;
inline constexpr int kVoiceDefaultReleaseMs = 300;
inline constexpr int kVoiceMostReleaseMs = 1000;
// L'APPUI QUI FAIT PARLER : une touche frappée plus brièvement, comme une
// lettre écrite dans la conversation, n'ouvre pas le micro.
inline constexpr int kVoiceDefaultPressMs = 300;
inline constexpr int kVoiceMostPressMs = 1000;
// LA BARRE DE LA VOIX : sa taille et son opacité, en pour cent.
inline constexpr float kVoiceBarLeastScalePercent = 60.0F;
inline constexpr float kVoiceBarMostScalePercent = 200.0F;
inline constexpr float kVoiceBarLeastOpacityPercent = 20.0F;

// L'ICÔNE AU-DESSUS DE QUI PARLE, au choix du joueur : elle ne change que ce
// qu'il voit. ⚠ LE RANG EST GARDÉ DANS LE FICHIER : on ajoute à la fin.
enum class SpeakingIcon : std::uint8_t {
  kMicrophone,
  kSpeakingHead,
  kSpeechBalloon,
  kSpeaker,
  kMusicalNote,
  kNone,
  kCount,
};

// L'image d'une icône, sous le dossier de nos icônes, ou vide pour « aucune ».
[[nodiscard]] std::string_view SpeakingIconPath(SpeakingIcon icon);

// CE QUE LA PAGE MONTRE ET QUE LA BOUCLE SEULE SAIT : les touches de la voix,
// le niveau du micro à l'instant, et l'image de chaque icône de parole.
struct VoiceLiveView {
  // Au rang de `VoiceTalkKey` ; vide quand aucune touche n'est posée.
  std::array<std::string, kVoiceTalkKeyCount> talk_keys;
  // La touche dont la capture attend son geste, ou aucune.
  std::optional<VoiceTalkKey> capturing;
  // En pour cent de `kVoiceLevelAtFullScale` ; vide quand la voix est fermée.
  std::optional<float> input_percent;
  // Au rang de `SpeakingIcon`. Sans elles, l'icône se choisit dans une liste.
  std::array<ro::ui::SkinImage, static_cast<std::size_t>(SpeakingIcon::kCount)> icons{};
};

[[nodiscard]] constexpr int VoiceRowId(VoiceRow row) {
  return kVoiceRowBase + static_cast<int>(row);
}

struct VoiceSettings;

// UN CLIC SUR LA BARRE DE LA VOIX, tel que la page Son l'aurait demandé : il
// passe par le même chemin qu'un réglage de la page, et s'enregistre de même.
// Vide quand il ne change rien.
[[nodiscard]] std::optional<ro::ui::OptionsWindowAction> VoiceBarAction(
    const ro::ui::VoiceBarAnswer& answer, const VoiceSettings& now);

// LES CANAUX QU'ON CHOISIT, au rang du fil : le salon s'impose de lui-même.
inline constexpr std::size_t kVoiceChosenChannelCount = 3;
inline constexpr int kVoiceListenEvery = (1 << kVoiceChosenChannelCount) - 1;
// Le bit d'un canal dans `VoiceSettings::listened`.
[[nodiscard]] constexpr int VoiceListenBit(int channel) { return 1 << channel; }
// Le micro peut monter au double : un micro discret se rattrape sans réglage
// du système.
inline constexpr float kVoiceMostInputGain = 2.0F;

struct VoiceSettings {
  // ALLUMÉE par défaut : rien ne part sans la touche « Parler », et entendre
  // les autres est ce qu'on attend d'un serveur qui ouvre la voix.
  bool enabled = true;
  // LE CANAL PAR DÉFAUT, où part « Parler » et la voix : 0 proximité,
  // 1 groupe, 2 guilde.
  int channel = 0;
  // LES CANAUX ÉCOUTÉS, un bit par canal au rang de `channel`. ⚠ TOUS PAR
  // DÉFAUT : un joueur ne parle au groupe ou à la guilde que pour y être
  // entendu.
  int listened = kVoiceListenEvery;
  bool self_muted = false;
  bool deafened = false;
  // LA BARRE DE LA VOIX : voir `ro::ui::VoiceBarView`.
  bool bar_shown = true;
  bool bar_locked = false;
  float bar_scale_percent = kPercent;
  float bar_opacity_percent = kPercent;
  bool bar_vertical = false;
  bool bar_speakers = true;
  bool bar_fade = false;
  float output_volume = 1.0F;
  float input_gain = 1.0F;
  // LE NOM DE L'APPAREIL, vide pour celui du système.
  std::string microphone;
  std::string speakers;
  int speaking_icon = 0;  // une `SpeakingIcon`
  int talk_mode = 0;      // un `VoiceTalkMode`
  int press_ms = kVoiceDefaultPressMs;
  int release_ms = kVoiceDefaultReleaseMs;
  float activation_percent = kVoiceDefaultActivationPercent;
};

// LE PASSAGE D'UNE TOUCHE DE LA VOIX : elle ne fait parler qu'une fois tenue
// sans interruption depuis l'appui minimal. Un relâchement remet le compte à
// zéro.
class VoicePressGate {
 public:
  [[nodiscard]] bool Pass(bool held, std::uint32_t now_ms, std::uint32_t least_ms) noexcept;

 private:
  std::optional<std::uint32_t> held_since_ms_;
};

// UN CHOIX D'APPAREIL tel que la ligne le montre : « celui du système » en
// tête, puis ceux que le système connaît, puis celui qu'on avait gardé s'il
// est débranché, pour que la ligne dise vrai.
struct VoiceDeviceChoice {
  std::vector<std::string> names;   // ce que la ligne choisie garde
  std::vector<std::string> labels;  // ce qu'elle montre
  std::vector<const char*> shown;   // les mêmes, pour la ligne
  int chosen = 0;
};

class VoiceOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  // Les noms des appareils que le système connaît, demandés à chaque
  // `ComposeInto` : un casque se branche pendant que la page est ouverte.
  using DeviceLister = std::function<std::vector<std::string>()>;
  void ListDevicesWith(DeviceLister microphones, DeviceLister speakers);
  // LES TOUCHES ET LE NIVEAU DU MICRO, que la boucle seule sait : elle les
  // redit à chaque image, et reprend la demande de capture qu'une ligne de
  // touche arme.
  void ShowLive(VoiceLiveView live) { live_ = std::move(live); }
  [[nodiscard]] std::optional<VoiceTalkKey> TakeTalkKeyRequest() noexcept {
    return std::exchange(talk_key_asked_, std::nullopt);
  }
  // Ajoute ses lignes à celles de la page Son, à chaque image où elle est
  // ouverte.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);
  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);
  // Bornées : le fichier peut porter n'importe quoi.
  [[nodiscard]] VoiceSettings settings() const noexcept;

 private:
  // Le canal par défaut, les canaux écoutés et la touche de chaque canal.
  void ComposeChannelsInto(std::vector<ro::ui::OptionRowView>& rows, const VoiceSettings& sane);
  // L'apparence de la barre de la voix.
  static void ComposeBarInto(std::vector<ro::ui::OptionRowView>& rows, const VoiceSettings& sane);

  VoiceSettings settings_;
  std::array<const char*, kVoiceChosenChannelCount> channel_names_{};
  DeviceLister list_microphones_;
  DeviceLister list_speakers_;
  VoiceDeviceChoice microphone_choice_;
  VoiceDeviceChoice speakers_choice_;
  std::array<const char*, static_cast<std::size_t>(SpeakingIcon::kCount)> icon_names_{};
  std::array<const char*, static_cast<std::size_t>(VoiceTalkMode::kCount)> mode_names_{};
  VoiceLiveView live_;
  std::array<std::string, kVoiceTalkKeyCount> talk_key_texts_;
  std::string level_text_;
  std::optional<VoiceTalkKey> talk_key_asked_;
};

}  // namespace moonclient
