#include "items/card_album_delta.h"

#include <algorithm>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Le nom du fichier et sa clé racine. ⚠ GRAVÉS : ce sont ceux que le script lit.
constexpr const char* kFileName = "card_album_delta.yml";
constexpr const char* kRootKey = "card_album_delta";
constexpr const char* kAlbumField = "album";
constexpr const char* kCardField = "card";
constexpr const char* kActionField = "action";
constexpr const char* kRateField = "rate";
constexpr const char* kAddWord = "add";
constexpr const char* kRemoveWord = "remove";

// L'en-tête est écrit à la main : l'émetteur ne porte pas de commentaires, et un
// bon de travail qu'on ouvre sans savoir ce qu'il commande s'applique de travers.
constexpr const char* kHeader =
    "# Bons de travail « albums de cartes », posés EN JEU par le staff.\n"
    "#\n"
    "# Ce fichier ne dit PAS ce qu'un album contient : il dit ce qu'on veut y\n"
    "# ajouter (action: add, avec son poids de tirage) ou en retirer (remove).\n"
    "#\n"
    "# La vérité est serveur : moonlight/db/import/item_group_db.yml, groupes\n"
    "# CARDALBUM (album 616) et MAGICCARDALBUM (album 12246). Le fichier client\n"
    "# packageitem.lub en est ENGENDRÉ — ne jamais l'éditer à la main.\n"
    "#\n"
    "# À appliquer par :  python apply_card_album_delta.py\n"
    "#   (dépôt des données du client, data/luafiles514/lua files/probabilityinfo/,\n"
    "#   auprès de gen_packageitem.py)\n"
    "# Il patche le YAML serveur, régénère le lub, puis vide ce fichier.\n";

}  // namespace

CardAlbumDelta::CardAlbumDelta(std::filesystem::path client_root)
    : path_(std::move(client_root) / kSaveFolder / kFileName) {}

void CardAlbumDelta::Load() {
  if (loaded_) {
    return;
  }
  // Même sur un échec : ne pas retourner au disque à chaque image.
  loaded_ = true;
  std::error_code missing;
  if (!std::filesystem::exists(path_, missing)) {
    return;
  }
  YAML::Node root;
  try {
    root = YAML::LoadFile(path_.string());
  } catch (const YAML::Exception& broken) {
    // Un document abîmé se traite comme un fichier absent : repartir de rien
    // vaut mieux qu'appliquer la moitié d'une intention.
    ro::core::LogWarning("album : bons de travail illisibles ({})", broken.what());
    return;
  }
  const YAML::Node list = root.IsMap() ? root[kRootKey] : YAML::Node();
  if (!list || !list.IsSequence()) {
    return;
  }
  for (const YAML::Node& node : list) {
    if (!node.IsMap()) {
      continue;
    }
    AlbumDeltaEntry entry;
    entry.album_id = node[kAlbumField].as<std::uint32_t>(0U);
    entry.card_id = node[kCardField].as<std::uint32_t>(0U);
    entry.rate = node[kRateField].as<int>(0);
    entry.action = node[kActionField].as<std::string>(kAddWord) == kRemoveWord
                       ? AlbumDeltaAction::kRemove
                       : AlbumDeltaAction::kAdd;
    if (entry.album_id == 0 || entry.card_id == 0) {
      continue;
    }
    entries_.push_back(entry);
  }
}

bool CardAlbumDelta::Flush() const {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kRootKey << YAML::Value << YAML::BeginSeq;
  for (const AlbumDeltaEntry& entry : entries_) {
    out << YAML::BeginMap;
    out << YAML::Key << kAlbumField << YAML::Value << entry.album_id;
    out << YAML::Key << kCardField << YAML::Value << entry.card_id;
    const bool remove = entry.action == AlbumDeltaAction::kRemove;
    out << YAML::Key << kActionField << YAML::Value << (remove ? kRemoveWord : kAddWord);
    // Le poids n'est écrit que pour un ajout : un poids sur un retrait laisserait
    // croire au script qu'il a quelque chose à reporter.
    if (!remove) {
      out << YAML::Key << kRateField << YAML::Value << entry.rate;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  std::error_code ignored;
  std::filesystem::create_directories(path_.parent_path(), ignored);
  const std::string text = std::string(kHeader) + out.c_str() + "\n";
  if (auto written = ro::core::WriteFileAtomically(path_, text); !written) {
    ro::core::LogWarning("album : bon de travail non écrit ({}) : {}", path_.string(),
                         written.error().detail);
    return false;
  }
  return true;
}

const std::vector<AlbumDeltaEntry>& CardAlbumDelta::entries() {
  Load();
  return entries_;
}

const AlbumDeltaEntry* CardAlbumDelta::Find(std::uint32_t album_id, std::uint32_t card_id) {
  Load();
  const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const AlbumDeltaEntry& e) {
    return e.album_id == album_id && e.card_id == card_id;
  });
  return found == entries_.end() ? nullptr : &*found;
}

bool CardAlbumDelta::Set(const AlbumDeltaEntry& entry) {
  Load();
  if (entry.album_id == 0 || entry.card_id == 0) {
    return false;
  }
  const std::vector<AlbumDeltaEntry> before = entries_;
  const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const AlbumDeltaEntry& e) {
    return e.album_id == entry.album_id && e.card_id == entry.card_id;
  });
  if (found == entries_.end()) {
    entries_.push_back(entry);
  } else {
    *found = entry;
  }
  // 🔴 L'ÉCRITURE D'ABORD, L'ÉTAT ENSUITE : si le disque refuse, on rend
  // EXACTEMENT ce qu'il y avait — y compris l'intention que celle-ci remplaçait.
  if (!Flush()) {
    entries_ = before;
    return false;
  }
  ++revision_;
  return true;
}

bool CardAlbumDelta::Clear(std::uint32_t album_id, std::uint32_t card_id) {
  Load();
  const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const AlbumDeltaEntry& e) {
    return e.album_id == album_id && e.card_id == card_id;
  });
  if (found == entries_.end()) {
    return true;
  }
  const AlbumDeltaEntry saved = *found;
  entries_.erase(found);
  if (!Flush()) {
    entries_.push_back(saved);
    return false;
  }
  ++revision_;
  return true;
}

}  // namespace moonclient
