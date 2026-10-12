#include "game/settings_profile_port.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

#include "input/hotkey_file.h"

namespace moonclient {
namespace {

// UN MODIFICATEUR DANS LA VALEUR : son mot, puis ce séparateur. ⚠ GRAVÉS, comme
// les champs de `hotkeys.yml` dont ils reprennent les noms.
constexpr char kModifierJoint = '+';
constexpr std::string_view kCtrlWord = "ctrl";
constexpr std::string_view kAltWord = "alt";
constexpr std::string_view kShiftWord = "shift";

[[nodiscard]] std::string KeyOfAction(ActionRef action) {
  return std::string(kProfileHotkeyPrefix) + SavedKeyOfAction(action);
}

// LA FORME QU'UN PROFIL RELU DU DISQUE PORTE : celle que rend `YAML::Dump`.
// La donner aussi à ce qu'on relève fait qu'une touche inchangée se compare
// égale à elle-même.
[[nodiscard]] std::string AsScalar(const std::string& text) { return YAML::Dump(YAML::Node(text)); }

[[nodiscard]] std::string TextOfBinding(Binding binding) {
  if (!binding.bound()) {
    return std::string();
  }
  std::string text;
  const auto hold = [&text](bool held, std::string_view word) {
    if (held) {
      text += word;
      text += kModifierJoint;
    }
  };
  hold(binding.ctrl, kCtrlWord);
  hold(binding.alt, kAltWord);
  hold(binding.shift, kShiftWord);
  return text + FileNameOfKey(binding.key);
}

// Le raccourci que dit ce texte, ou rien pour un nom de touche inconnu.
[[nodiscard]] std::optional<Binding> BindingOfText(std::string text) {
  Binding binding;
  if (text.empty()) {
    return binding;  // retiré délibérément
  }
  // ⚠ LES MOTS SE RETIRENT EN TÊTE, dans n'importe quel ordre, tant qu'il reste
  // un nom derrière : « + » seul, ou « Keypad + », est une touche.
  const std::array words{std::pair{kCtrlWord, &binding.ctrl}, std::pair{kAltWord, &binding.alt},
                         std::pair{kShiftWord, &binding.shift}};
  for (bool stripped = true; stripped;) {
    stripped = false;
    for (const auto& [word, held] : words) {
      const std::string prefix = std::string(word) + kModifierJoint;
      if (text.size() > prefix.size() && text.starts_with(prefix)) {
        *held = true;
        text.erase(0, prefix.size());
        stripped = true;
      }
    }
  }
  // Un nom inconnu rend le code nul : la touche n'est alors pas posée.
  binding.key = KeyOfFileName(text);
  if (!binding.bound()) {
    return std::nullopt;
  }
  return binding;
}

// CE QUI S'ÉCRIT, comme dans `hotkeys.yml` : ce qui a une touche, et ce qui en
// avait une à l'origine — vide, c'est la seule façon de dire « retirée ».
void CaptureHotkeys(const Hotkeys& from, SettingsValues& into) {
  static const Hotkeys kFactory;
  for (const ActionRef action : AllBindableActions()) {
    const Binding binding = from.Of(action);
    if (!binding.bound() && !kFactory.Of(action).bound()) {
      continue;
    }
    into.insert_or_assign(KeyOfAction(action), AsScalar(TextOfBinding(binding)));
  }
}

[[nodiscard]] bool HoldsHotkeys(const SettingsValues& profile) {
  const auto first = profile.lower_bound(std::string(kProfileHotkeyPrefix));
  return first != profile.end() && first->first.starts_with(kProfileHotkeyPrefix);
}

// Pose les touches du profil sur une table d'origine : une action absente du
// profil garde son raccourci d'origine, comme à la lecture de `hotkeys.yml`.
void PoseHotkeys(const SettingsValues& profile, Hotkeys& into) {
  into.RestoreDefaults();
  for (const ActionRef action : AllBindableActions()) {
    const auto found = profile.find(KeyOfAction(action));
    if (found == profile.end()) {
      continue;
    }
    std::string text;
    try {
      const YAML::Node node = YAML::Load(found->second);
      if (!node.IsScalar()) {
        continue;
      }
      text = node.as<std::string>(std::string());
    } catch (const YAML::Exception&) {
      continue;
    }
    const std::optional<Binding> binding = BindingOfText(std::move(text));
    if (!binding.has_value()) {
      // ⚠ UN NOM QU'ON NE RECONNAÎT PLUS LAISSE LE DÉFAUT EN PLACE : l'effacer
      // rendrait l'action inatteignable sans que rien ne l'explique.
      ro::core::LogWarning(
          "profils de réglages : touche illisible pour {} — ce raccourci garde sa touche "
          "d'origine",
          found->first);
      continue;
    }
    static_cast<void>(into.Bind(action, *binding));
  }
}

}  // namespace

SettingsProfilePort::SettingsProfilePort(std::filesystem::path client_root, SettingsFile& file,
                                         Hotkeys& hotkeys)
    : client_root_(std::move(client_root)), file_(file), hotkeys_(hotkeys) {
  LoadHotkeys(client_root_, inherited_hotkeys_);
}

std::uint64_t SettingsProfilePort::Revision() { return file_.revision() + hotkeys_.revision(); }

SettingsValues SettingsProfilePort::Capture() {
  SettingsValues values = file_.SharedValues();
  CaptureHotkeys(hotkeys_, values);
  return values;
}

void SettingsProfilePort::Pose(const SettingsValues& profile) {
  file_.AssignShared(profile);
  if (HoldsHotkeys(profile)) {
    PoseHotkeys(profile, hotkeys_);
  } else {
    // ⚠ PAR SES VALEURS, ET NON PAR AFFECTATION : la révision de la table
    // vivante doit grandir, et l'affecter lui donnerait celle de l'héritée.
    SettingsValues inherited;
    CaptureHotkeys(inherited_hotkeys_, inherited);
    PoseHotkeys(inherited, hotkeys_);
  }
  AfterAssign();
  // ⚠ LE FICHIER VIVANT SUIT : le profil posé est le réglage présent, et la
  // prochaine session le relira avant même d'entrer en jeu.
  file_.Touch();
}

SettingsValues SettingsProfilePort::Defaults() {
  SettingsValues values = file_.SharedDefaults();
  CaptureHotkeys(Hotkeys(), values);
  return values;
}

bool SettingsProfilePort::Load(SettingsProfileBook& into) {
  if (!LoadSettingsProfiles(client_root_, into)) {
    return false;
  }
  for (Named<SettingsValues>& named : into.profiles) {
    if (!HoldsHotkeys(named.profile)) {
      CaptureHotkeys(inherited_hotkeys_, named.profile);
    }
  }
  return true;
}

void SettingsProfilePort::Save(const SettingsProfileBook& from) {
  static_cast<void>(SaveSettingsProfiles(client_root_, from));
}

}  // namespace moonclient
