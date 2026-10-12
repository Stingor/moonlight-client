#include "world/blocked_npcs.h"

#include <array>
#include <cstdio>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/sim/world.h"

#include "game/yaml_load.h"
#include "world/entity_menu.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer débloquerait tous les PNJ de
// tous les joueurs, sans que rien ne le dise.
constexpr const char* kListField = "pnj_bloques";
constexpr const char* kIdField = "id";
constexpr const char* kNameField = "nom";

constexpr const char* kFileHeader =
    "# Les PNJ dont le clic gauche ne fait plus rien.\n"
    "#\n"
    "# Un GID par PNJ, et le nom qu'il portait quand on l'a bloqué — ce nom ne\n"
    "# sert qu'à s'y retrouver. Seuls les PNJ à identifiant fixe du serveur\n"
    "# (3000000 à 3000099) se bloquent : les autres changent de GID à chaque\n"
    "# démarrage, et une entrée hors de cette plage est ignorée.\n"
    "#\n"
    "# Le menu du clic droit sur le PNJ, ou la page « Menu contextuel » des\n"
    "# réglages, les débloque.\n";

// Assez pour une phrase et un nom de PNJ.
constexpr std::size_t kLineBytes = 256;

}  // namespace

bool BlockedNpcs::Toggle(std::uint32_t id, std::string_view name) {
  if (Remove(id)) {
    return false;
  }
  if (!IsFixedIdNpc(id)) {
    return false;
  }
  names_[id] = std::string(name);
  return true;
}

void BlockedNpcs::Put(std::uint32_t id, std::string name) {
  if (IsFixedIdNpc(id)) {
    names_[id] = std::move(name);
  }
}

bool BlockedNpcs::Remove(std::uint32_t id) {
  // Le rappel suit le blocage : rebloqué plus tard, ce PNJ doit pouvoir
  // réexpliquer son silence.
  explained_.erase(id);
  return names_.erase(id) != 0;
}

std::string BlockedNpcLabel(std::uint32_t id, std::string_view name) {
  if (!name.empty()) {
    return std::string(name);
  }
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), ro::core::Tr("Le NPC %u"), id);
  return line.data();
}

bool SwallowsLeftClick(BlockedNpcs& blocked, bool enabled, const ro::sim::Actor& actor,
                       ro::sim::World& world, std::uint32_t now_ms) {
  // Le cas courant — aucun PNJ bloqué — sort sans rien regarder d'autre.
  if (!enabled || blocked.empty() || !blocked.Has(actor.id)) {
    return false;
  }
  if (blocked.FirstExplanation(actor.id)) {
    const auto found = blocked.all().find(actor.id);
    const std::string who =
        BlockedNpcLabel(actor.id, found != blocked.all().end() ? found->second : actor.name);
    std::array<char, kLineBytes> line{};
    std::snprintf(line.data(), line.size(),
                  ro::core::Tr("%s : vous avez bloqué le clic gauche sur ce NPC. Clic droit "
                               "dessus pour le débloquer."),
                  who.c_str());
    world.RecordChat(ro::sim::ChatKind::kServer, 0, line.data(), now_ms, kClientSaysColor);
  }
  return true;
}

std::filesystem::path BlockedNpcsPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kBlockedNpcsFileName;
}

void LoadBlockedNpcs(const std::filesystem::path& client_root, BlockedNpcs& into) {
  const std::filesystem::path path = BlockedNpcsPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;  // personne n'a encore rien bloqué
  }
  YAML::Node whole;
  try {
    whole = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape, et pour la même raison qu'ailleurs : un
    // fichier abîmé ne doit pas empêcher de jouer.
    ro::core::LogWarning("PNJ bloqués : fichier illisible ({}) — aucun PNJ ne sera bloqué",
                         broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lève
  // hors du `try`, et le jeu ne se lancerait pas.
  if (!whole.IsMap()) {
    return;
  }
  const YAML::Node listed = whole[kListField];
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : listed) {
    if (!entry.IsMap()) {
      continue;
    }
    const YAML::Node id = entry[kIdField];
    const YAML::Node name = entry[kNameField];
    if (!id || !id.IsScalar()) {
      continue;
    }
    into.Put(id.as<std::uint32_t>(0U),
             name && name.IsScalar() ? name.as<std::string>(std::string()) : std::string());
  }
}

bool SaveBlockedNpcs(const std::filesystem::path& client_root, const BlockedNpcs& from) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kListField << YAML::Value << YAML::BeginSeq;
  // ⚠ DANS L'ORDRE DES GID, que la table tient déjà : un fichier qui se
  // réordonne à chaque écriture est illisible en diff.
  for (const auto& [id, name] : from.all()) {
    out << YAML::Flow << YAML::BeginMap << YAML::Key << kIdField << YAML::Value << id << YAML::Key
        << kNameField << YAML::Value << name << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(BlockedNpcsPath(client_root), text);
      !written) {
    ro::core::LogWarning(
        "PNJ bloqués : écriture impossible ({}) — le blocage ne survivra pas à cette partie",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
