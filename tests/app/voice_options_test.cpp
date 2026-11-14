// LE CHOIX DU MICRO ET DE LA SORTIE : « celui du système » en tête, l'appareil
// gardé retrouvé par son nom, et un casque débranché qui reste nommé.

#include "options/voice_options.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::VoiceOptions;
using moonclient::VoiceRow;
using moonclient::VoiceRowId;
using ro::ui::OptionRowView;

namespace {

// Les appareils du « système », que l'essai change en cours de route.
struct FakeSystem {
  std::vector<std::string> microphones;
  std::vector<std::string> speakers;
};

void Plug(VoiceOptions& options, const FakeSystem& system) {
  options.ListDevicesWith([&system] { return system.microphones; },
                          [&system] { return system.speakers; });
}

[[nodiscard]] OptionRowView RowOf(VoiceOptions& options, VoiceRow row) {
  std::vector<OptionRowView> rows;
  options.ComposeInto(rows);
  for (const OptionRowView& view : rows) {
    if (view.id == VoiceRowId(row)) return view;
  }
  return {};
}

void Pick(VoiceOptions& options, VoiceRow row, int chosen) {
  ro::ui::OptionsWindowAction asked;
  asked.changed = VoiceRowId(row);
  asked.chosen = chosen;
  REQUIRE(options.Change(asked));
}

}  // namespace

TEST_CASE("Voix : le micro se choisit par son nom, le système en tête", "[voice][options]") {
  FakeSystem system{.microphones = {"Casque", "Webcam"}, .speakers = {"Haut-parleurs"}};
  VoiceOptions options;
  Plug(options, system);
  const OptionRowView microphone = RowOf(options, VoiceRow::kMicrophone);
  REQUIRE(microphone.choices.size() == 3);
  CHECK(microphone.chosen == 0);
  CHECK(std::string(microphone.choices[2]) == "Webcam");

  Pick(options, VoiceRow::kMicrophone, 2);
  CHECK(options.settings().microphone == "Webcam");
  // L'ORDRE DU SYSTÈME PEUT CHANGER : c'est le nom qui retrouve l'appareil.
  system.microphones = {"Webcam", "Casque"};
  CHECK(RowOf(options, VoiceRow::kMicrophone).chosen == 1);

  Pick(options, VoiceRow::kMicrophone, 0);
  CHECK(options.settings().microphone.empty());
}

TEST_CASE("Voix : un casque débranché reste nommé et choisi", "[voice][options]") {
  FakeSystem system{.microphones = {"Casque"}, .speakers = {"Haut-parleurs", "Casque"}};
  VoiceOptions options;
  Plug(options, system);
  static_cast<void>(RowOf(options, VoiceRow::kSpeakers));
  Pick(options, VoiceRow::kSpeakers, 2);
  REQUIRE(options.settings().speakers == "Casque");

  system.speakers = {"Haut-parleurs"};
  const OptionRowView speakers = RowOf(options, VoiceRow::kSpeakers);
  REQUIRE(speakers.choices.size() == 3);
  CHECK(speakers.chosen == 2);
  CHECK(std::string(speakers.choices[2]).starts_with("Casque "));
  // Le reprendre ne l'efface pas.
  Pick(options, VoiceRow::kSpeakers, 2);
  CHECK(options.settings().speakers == "Casque");
}

TEST_CASE("Voix : sans liste, seul le système se propose", "[voice][options]") {
  VoiceOptions options;
  CHECK(RowOf(options, VoiceRow::kMicrophone).choices.size() == 1);
}

TEST_CASE("Voix : la touche « Parler » se demande depuis la page Son", "[voice][options]") {
  using moonclient::VoiceTalkKey;
  VoiceOptions options;
  moonclient::VoiceLiveView live;
  live.talk_keys[static_cast<std::size_t>(VoiceTalkKey::kDefault)] = "G";
  live.talk_keys[static_cast<std::size_t>(VoiceTalkKey::kParty)] = "H";
  options.ShowLive(live);
  const std::string shown = RowOf(options, VoiceRow::kTalkKey).label;
  CHECK(shown.ends_with(" G"));
  CHECK(std::string(RowOf(options, VoiceRow::kTalkPartyKey).label).ends_with(" H"));
  CHECK_FALSE(options.TakeTalkKeyRequest().has_value());

  // LA LIGNE ARME UNE DEMANDE, QUE LA BOUCLE PREND UNE FOIS.
  Pick(options, VoiceRow::kTalkKey, 0);
  CHECK(options.TakeTalkKeyRequest() == VoiceTalkKey::kDefault);
  CHECK_FALSE(options.TakeTalkKeyRequest().has_value());
  Pick(options, VoiceRow::kTalkGuildKey, 0);
  CHECK(options.TakeTalkKeyRequest() == VoiceTalkKey::kGuild);

  live.capturing = VoiceTalkKey::kDefault;
  options.ShowLive(live);
  CHECK(std::string(RowOf(options, VoiceRow::kTalkKey).label) != shown);
}

TEST_CASE("Voix : plusieurs canaux s'écoutent, le canal par défaut toujours", "[voice][options]") {
  VoiceOptions options;
  CHECK(options.settings().listened == moonclient::kVoiceListenEvery);
  ro::ui::OptionsWindowAction asked;
  asked.changed = VoiceRowId(VoiceRow::kListenGuild);
  asked.on = false;
  REQUIRE(options.Change(asked));
  CHECK(options.settings().listened == (moonclient::kVoiceListenEvery & ~4));
  CHECK_FALSE(RowOf(options, VoiceRow::kListenGuild).on);

  // LA BARRE DEMANDE CE QUE LA PAGE AURAIT DEMANDÉ.
  const auto through_bar = [&options](ro::ui::VoiceBarAnswer answer) {
    const std::optional<ro::ui::OptionsWindowAction> bar =
        moonclient::VoiceBarAction(answer, options.settings());
    return bar.has_value() && options.Change(*bar);
  };
  CHECK(through_bar(ro::ui::VoiceBarAnswer{.make_default = 2}));
  CHECK(options.settings().channel == 2);
  // LE CANAL PAR DÉFAUT EST COCHÉ ET ÉTEINT, même décoché dans le fichier, et
  // sa pastille ne l'éteint pas.
  CHECK(RowOf(options, VoiceRow::kListenGuild).on);
  CHECK(RowOf(options, VoiceRow::kListenGuild).disabled);
  CHECK_FALSE(through_bar(ro::ui::VoiceBarAnswer{.toggle_listen = 2}));
  CHECK(through_bar(ro::ui::VoiceBarAnswer{.toggle_listen = 0}));
  CHECK((options.settings().listened & 1) == 0);
  CHECK(through_bar(ro::ui::VoiceBarAnswer{.toggle_listen = 0}));
  CHECK((options.settings().listened & 1) == 1);
  CHECK(through_bar(ro::ui::VoiceBarAnswer{.toggle_mute = true}));
  CHECK(options.settings().self_muted);
  CHECK_FALSE(through_bar(ro::ui::VoiceBarAnswer{}));
}

TEST_CASE("Voix : le seuil ne se règle qu'à la voix, devant le niveau du micro",
          "[voice][options]") {
  VoiceOptions options;
  CHECK(RowOf(options, VoiceRow::kActivation).disabled);
  Pick(options, VoiceRow::kTalkMode, static_cast<int>(moonclient::VoiceTalkMode::kVoice));
  CHECK_FALSE(RowOf(options, VoiceRow::kActivation).disabled);

  options.ShowLive(moonclient::VoiceLiveView{.input_percent = 42.4F});
  CHECK(std::string(RowOf(options, VoiceRow::kInputLevel).label).ends_with(" 42 %"));

  // LE FICHIER PEUT PORTER N'IMPORTE QUOI : le délai reste dans ses bornes.
  ro::ui::OptionsWindowAction asked;
  asked.changed = VoiceRowId(VoiceRow::kRelease);
  asked.value = 5000.0F;
  REQUIRE(options.Change(asked));
  CHECK(options.settings().release_ms == moonclient::kVoiceMostReleaseMs);
}

TEST_CASE("Voix : l'icône de parole se choisit, « aucune » comprise", "[voice][options]") {
  VoiceOptions options;
  const OptionRowView icon = RowOf(options, VoiceRow::kSpeakingIcon);
  REQUIRE(icon.choices.size() == static_cast<std::size_t>(moonclient::SpeakingIcon::kCount));
  CHECK(icon.chosen == static_cast<int>(moonclient::SpeakingIcon::kMicrophone));

  Pick(options, VoiceRow::kSpeakingIcon, static_cast<int>(moonclient::SpeakingIcon::kNone));
  CHECK(options.settings().speaking_icon == static_cast<int>(moonclient::SpeakingIcon::kNone));
  CHECK(moonclient::SpeakingIconPath(moonclient::SpeakingIcon::kNone).empty());
  CHECK(moonclient::SpeakingIconPath(moonclient::SpeakingIcon::kMicrophone)
            .ends_with("voice_microphone.png"));
}

TEST_CASE("Voix : l'icône se choisit sur son image, et dans une liste sans elles",
          "[voice][options]") {
  VoiceOptions options;
  CHECK(RowOf(options, VoiceRow::kSpeakingIcon).pictures.empty());

  moonclient::VoiceLiveView live;
  live.icons[static_cast<std::size_t>(moonclient::SpeakingIcon::kMicrophone)] =
      ro::ui::SkinImage{.id = 1, .width = 32.0F, .height = 32.0F};
  options.ShowLive(live);
  const OptionRowView icon = RowOf(options, VoiceRow::kSpeakingIcon);
  CHECK(icon.kind == ro::ui::OptionKind::kPicture);
  CHECK(icon.pictures.size() == icon.choices.size());
  CHECK_FALSE(icon.pictures[static_cast<std::size_t>(moonclient::SpeakingIcon::kNone)].usable());
}

TEST_CASE("Voix : une touche frappée plus brièvement que l'appui minimal ne fait pas parler",
          "[voice][options]") {
  constexpr std::uint32_t kLeastMs = 300;
  moonclient::VoicePressGate gate;
  // Une lettre écrite dans la conversation : appuyée puis lâchée vite.
  CHECK_FALSE(gate.Pass(true, 1000, kLeastMs));
  CHECK_FALSE(gate.Pass(true, 1080, kLeastMs));
  CHECK_FALSE(gate.Pass(false, 1100, kLeastMs));
  // Un relâchement remet le compte à zéro : le nouvel appui repart de lui-même.
  CHECK_FALSE(gate.Pass(true, 1200, kLeastMs));
  CHECK_FALSE(gate.Pass(true, 1499, kLeastMs));
  CHECK(gate.Pass(true, 1500, kLeastMs));
  CHECK(gate.Pass(true, 2500, kLeastMs));
  CHECK_FALSE(gate.Pass(false, 2510, kLeastMs));
  // Sans appui minimal, la touche parle dès qu'elle est tenue.
  CHECK(gate.Pass(true, 3000, 0));
}

TEST_CASE("Voix : l'appui minimal vaut 300 ms, et se borne", "[voice][options]") {
  VoiceOptions options;
  CHECK(options.settings().press_ms == moonclient::kVoiceDefaultPressMs);
  CHECK(RowOf(options, VoiceRow::kPress).kind == ro::ui::OptionKind::kSlider);
  ro::ui::OptionsWindowAction asked;
  asked.changed = VoiceRowId(VoiceRow::kPress);
  asked.value = 5000.0F;
  CHECK(options.Change(asked));
  CHECK(options.settings().press_ms == moonclient::kVoiceMostPressMs);
}
