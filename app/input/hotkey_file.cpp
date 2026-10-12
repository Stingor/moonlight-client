#include "input/hotkey_file.h"

#include <string>

#include <SDL3/SDL_keyboard.h>
#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs d'une entrée. ⚠ CES NOMS SONT GRAVÉS au même titre que les clés
// d'action : les changer rend à tous les joueurs les touches d'origine.
constexpr const char* kKeyField = "key";
constexpr const char* kCtrlField = "ctrl";
constexpr const char* kShiftField = "shift";
constexpr const char* kAltField = "alt";

// L'en-tête du fichier. Il s'adresse à qui l'ouvrira dans un éditeur de texte,
// et c'est le cas d'usage qui a fait choisir des NOMS de touche plutôt que des
// codes.
constexpr const char* kFileHeader =
    "# Les raccourcis de moonclient.\n"
    "#\n"
    "# Une touche s'écrit par son NOM, jamais par un code : « S », « F12 »,\n"
    "# « Return ». Ce fichier se relit et se corrige donc à la main, et il ne\n"
    "# dépend pas de la façon dont une bibliothèque numérote ses touches.\n"
    "#\n"
    "# Une touche vide retire le raccourci. Une entrée inconnue est ignorée.\n";

[[nodiscard]] bool TruthOf(const YAML::Node& entry, const char* field) {
  const YAML::Node value = entry[field];
  return value && value.IsScalar() && value.as<bool>(false);
}

// L'entrée de cette action, ou un nœud absent.
[[nodiscard]] YAML::Node EntryOf(const YAML::Node& file, ActionRef action) {
  if (const YAML::Node entry = file[SavedKeyOfAction(action)]; entry && entry.IsMap()) {
    return entry;
  }
  return YAML::Node();
}

}  // namespace

std::filesystem::path HotkeyFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kHotkeyFileName;
}

void LoadHotkeys(const std::filesystem::path& client_root, Hotkeys& into) {
  const std::filesystem::path path = HotkeyFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;  // première partie : les raccourcis d'origine font l'affaire
  }
  YAML::Node file;
  try {
    file = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    // ⚠ LA SEULE EXCEPTION QU'ON ATTRAPE DE TOUT LE PROGRAMME, et elle vient
    // d'une bibliothèque qui n'offre pas d'autre voie. Un fichier de réglages
    // abîmé ne doit pas empêcher de jouer : on le dit, et on garde les touches
    // d'origine.
    ro::core::LogWarning("raccourcis : fichier illisible ({}) — les touches d'origine restent",
                         broken.what());
    return;
  }
  if (!file.IsMap()) {
    return;
  }

  for (const ActionRef action : AllBindableActions()) {
    const YAML::Node entry = EntryOf(file, action);
    if (!entry) {
      continue;
    }
    const YAML::Node named = entry[kKeyField];
    if (!named || !named.IsScalar()) {
      continue;
    }
    const std::string key_name = named.as<std::string>(std::string());
    // Une touche VIDE est un raccourci délibérément retiré, pas une entrée
    // abîmée : on la pose telle quelle.
    Binding binding;
    if (!key_name.empty()) {
      const std::uint32_t code = KeyOfFileName(key_name);
      if (code == SDLK_UNKNOWN) {
        // ⚠ UN NOM QU'ON NE RECONNAÎT PLUS LAISSE LE DÉFAUT EN PLACE. L'effacer
        // rendrait l'action inatteignable sans que rien ne l'explique.
        ro::core::LogWarning(
            "raccourcis : touche inconnue « {} » — ce raccourci garde sa touche d'origine",
            key_name);
        continue;
      }
      binding.key = code;
      binding.ctrl = TruthOf(entry, kCtrlField);
      binding.shift = TruthOf(entry, kShiftField);
      binding.alt = TruthOf(entry, kAltField);
    }
    static_cast<void>(into.Bind(action, binding));
  }
}

bool SaveHotkeys(const std::filesystem::path& client_root, const Hotkeys& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  // 🔴 CE QUI S'ÉCRIT : CE QUI A UNE TOUCHE, ET CE QUI EN AVAIT UNE PAR DÉFAUT.
  //
  // Les quatre-vingt-cinq cases sans défaut ni touche ne diraient rien en
  // s'écrivant, et noieraient le fichier. Mais une action QUI A UN DÉFAUT doit
  // s'écrire même vide : c'est la seule façon de dire « le joueur l'a retirée »
  // plutôt que « elle garde son défaut ». Sans cela, délier F1 de la première
  // case ne survivrait pas au redémarrage — et rien ne l'expliquerait.
  //
  // ⚠ LE DÉFAUT SE DEMANDE À UNE TABLE NEUVE, jamais à une liste recopiée ici :
  // une seconde liste finirait par diverger de `Hotkeys()`, et c'est ce jour-là
  // qu'un raccourci effacé ressusciterait.
  static const Hotkeys kFactory;
  for (const ActionRef action : AllBindableActions()) {
    const Binding binding = from.Of(action);
    if (!binding.bound() && !kFactory.Of(action).bound()) {
      continue;
    }
    const std::string name = binding.bound() ? FileNameOfKey(binding.key) : std::string();
    out << YAML::Key << SavedKeyOfAction(action);
    out << YAML::Value << YAML::BeginMap;
    out << YAML::Key << kKeyField << YAML::Value << name;
    // ⚠ LES MODIFICATEURS NE S'ÉCRIVENT QUE POSÉS. Une entrée qui ne porte que ce
    // qui est vrai se lit d'un coup d'œil ; trois « false » par ligne noieraient
    // le seul champ qui compte.
    if (binding.ctrl) {
      out << YAML::Key << kCtrlField << YAML::Value << true;
    }
    if (binding.shift) {
      out << YAML::Key << kShiftField << YAML::Value << true;
    }
    if (binding.alt) {
      out << YAML::Key << kAltField << YAML::Value << true;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndMap;

  // ⚠ ATOMIQUE : une coupure en cours d'écriture ne doit pas laisser un
  // fichier de touches à moitié écrit. Voir `ro::core::WriteFileAtomically`.
  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(HotkeyFilePath(client_root), text);
      !written) {
    // 🔴 EN ERREUR, ET SEULE DE SA FAMILLE. Les autres écritures atomiques de
    // réglages du dépôt sont des alertes parce que le prochain changement les
    // rejoue ; celle-ci, non — `SaveHotkeysIfNeeded` avance `saved_revision_`
    // AVANT d'écrire et jette le booléen, exprès, pour ne pas réessayer soixante
    // fois par seconde. La touche que le joueur vient de régler est donc perdue
    // pour de bon, et sans un mot ailleurs.
    ro::core::LogError(
        "raccourcis : {} — la touche réglée ne sera pas retenue au prochain "
        "démarrage",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
