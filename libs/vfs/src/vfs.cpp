#include "ro/vfs/vfs.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <system_error>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

using core::ErrorCode;
using core::Fail;
using core::Result;

// Le chemin interne passé par l'appelant est en UTF-8 ; la clé de recherche est
// en CP949 normalisé. Une conversion qui échoue n'est pas fatale : un chemin
// purement ASCII traverse tel quel, ce qui couvre l'immense majorité des appels.
// Le dossier du disque, tel qu'il commence toute clé interne.
constexpr std::string_view kDataFolderKey = "data/";
// Le même, sous la forme d'une clé déjà normalisée : minuscules, antislash.
constexpr std::string_view kDataFolderDiskKey = "data\\";

std::string ToLookupKey(std::string_view utf8_path) {
  auto cp949 = Utf8ToCp949(utf8_path);
  return formats::NormalizeGrfPath(cp949 ? std::string_view(*cp949) : utf8_path);
}

#ifndef _WIN32
// Retrouver un fichier dont on ne connaît pas la CASSE, segment par segment.
//
// ⚠ LA CLÉ DE RECHERCHE EST MINUSCULÉE, et c'est voulu : un GRF range ses
// entrées sans règle de casse, si bien que comparer autrement qu'en minuscules
// ferait manquer des fichiers présents. Mais le disque, lui, n'est pas un GRF.
// Windows ne fait pas la différence et le chemin minusculé y ouvre le fichier ;
// un système de fichiers sensible à la casse, si — et `SystemEN/itemInfo.lua`
// demandé comme `systemen/iteminfo.lua` est alors déclaré ABSENT alors qu'il
// est là. Le client perd la table des objets, donc les armes, sans une erreur.
//
// On ne parcourt les répertoires QUE si le chemin direct a échoué : le cas
// courant ne coûte rien, et un client installé tout en minuscules non plus.
std::filesystem::path ResolveIgnoringCase(const std::filesystem::path& root,
                                          const std::filesystem::path& relative) {
  std::filesystem::path resolved = root;
  std::error_code code;
  for (const std::filesystem::path& segment : relative) {
    if (segment.empty()) {
      continue;
    }
    const std::filesystem::path direct = resolved / segment;
    if (std::filesystem::exists(direct, code)) {
      resolved = direct;
      continue;
    }

    const std::string wanted = core::AsciiLower(segment.string());
    bool found = false;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(resolved, code)) {
      if (core::AsciiLower(entry.path().filename().string()) == wanted) {
        resolved = entry.path();
        found = true;
        break;
      }
    }
    if (!found) {
      return {};
    }
  }
  return resolved;
}
#endif

}  // namespace

// Un chemin interne normalisé (« data\sprite\x.spr ») devient un chemin disque.
//
// Les octets CP949 admettent DEUX orthographes sur disque. Extrait par un outil
// moderne, un dossier coréen porte son vrai nom Unicode ; extrait par le client
// — ou par un outil qui recrache les octets bruts — il porte ces mêmes octets
// relus en CP1252. Un client installé en français ne montre que la seconde
// forme. On essaie les deux, sinon le fichier est déclaré absent alors qu'il est
// là.
std::vector<std::filesystem::path> DiskSpellings(const std::filesystem::path& root,
                                                 std::string_view normalized_key) {
  std::string relative(normalized_key);
  std::replace(relative.begin(), relative.end(), '\\',
               static_cast<char>(std::filesystem::path::preferred_separator));

  std::vector<std::filesystem::path> spellings;
#ifdef _WIN32
  // Forme 1 : les octets sont du vrai CP949.
  if (auto utf8 = Cp949ToUtf8(relative); utf8) {
    spellings.push_back(root / std::filesystem::path(std::u8string(
                                   reinterpret_cast<const char8_t*>(utf8->data()), utf8->size())));
  }
  // Forme 2 : les octets ont été écrits tels quels par un outil qui les lisait
  // en CP1252. `path` construit depuis une std::string emploie la page ANSI,
  // ce qui reproduit exactement cette orthographe.
  //
  // ⚠ SAUF SI ELLE ÉGALE LA PREMIÈRE, ce qui est le cas de tout nom ASCII.
  std::filesystem::path ansi = root / std::filesystem::path(relative);
  if (spellings.empty() || spellings.front() != ansi) {
    spellings.push_back(std::move(ansi));
  }
#else
  spellings.emplace_back(root / std::filesystem::path(relative));
#endif
  return spellings;
}

Result<Vfs> Vfs::Mount(const MountOptions& options) {
  std::error_code code;
  if (!std::filesystem::is_directory(options.root, code)) {
    return Fail(ErrorCode::kNotFound, options.root.string() + " n'est pas un dossier");
  }

  Vfs mounted;
  mounted.root_ = options.root;
  if (options.prefer_data_directory) {
    const std::filesystem::path data_directory = options.root / "data";
    if (std::filesystem::is_directory(data_directory, code)) {
      mounted.data_directory_ = data_directory;
    }
  }

  for (const std::string& name : options.archives) {
    const std::filesystem::path archive_path = options.root / name;
    auto archive = formats::GrfArchive::OpenFile(archive_path);
    if (!archive) {
      if (options.skip_unreadable_archives) {
        ro::core::  // ⚠ CE QUE CETTE LIGNE EXPLIQUE, dix lignes plus bas : une archive écartée
                    // est la cause des « sprite absent », « modèle absent » et « son absent »
                    // qui rempliront le reste du journal.
            LogWarning("archive ignorée : {} ({}) — ce qu'elle porte manquera à tout le reste",
                       archive_path.string(), core::Describe(archive.error()));
        continue;
      }
      return std::unexpected(archive.error());
    }
    mounted.archives_.push_back(MountedArchive{archive_path, std::move(*archive)});
  }

  if (mounted.archives_.empty() && mounted.data_directory_.empty()) {
    return Fail(ErrorCode::kNotFound,
                options.root.string() + " : ni dossier data\\ ni archive lisible");
  }
  // APRÈS les archives, et il le faut : la table se lit par ce même système de
  // fichiers, et une installation la porte dans `moonlight.grf` comme sur le
  // disque.
  mounted.res_names_ = ResNameTable::Load(mounted);
  return mounted;
}

namespace {

// Le fichier d'une clé normalisée sous `root`, ou vide s'il n'y est pas.
[[nodiscard]] std::filesystem::path FindUnder(const std::filesystem::path& root,
                                              std::string_view normalized_key) {
  std::error_code code;
  for (const std::filesystem::path& candidate : DiskSpellings(root, normalized_key)) {
    if (std::filesystem::is_regular_file(candidate, code)) {
      return candidate;
    }
  }

#ifndef _WIN32
  // Le chemin direct a échoué : reste la casse. Voir ResolveIgnoringCase.
  std::string relative(normalized_key);
  std::replace(relative.begin(), relative.end(), '\\',
               static_cast<char>(std::filesystem::path::preferred_separator));
  const std::filesystem::path found = ResolveIgnoringCase(root, relative);
  if (!found.empty() && std::filesystem::is_regular_file(found, code)) {
    return found;
  }
#endif
  return {};
}

}  // namespace

void Vfs::OverlayFolder(std::string_view utf8_prefix, std::filesystem::path folder) {
  const std::string prefix = ToLookupKey(utf8_prefix);
  std::erase_if(overlays_, [&prefix](const auto& overlay) { return overlay.first == prefix; });
  if (!folder.empty()) {
    overlays_.emplace_back(prefix, std::move(folder));
  }
  ++overlay_revision_;
}

std::filesystem::path Vfs::ResolveOnDisk(std::string_view normalized_key) const {
  for (const auto& [prefix, folder] : overlays_) {
    if (normalized_key.starts_with(prefix)) {
      if (std::filesystem::path found = FindUnder(folder, normalized_key.substr(prefix.size()));
          !found.empty()) {
        return found;
      }
    }
  }
  // 🔴 LE DOSSIER `data\` NE GOUVERNE QUE CE QUI EST DESSOUS. Ce qui vit HORS de
  // lui — `SystemEN\` : la table des objets, les recettes — se lit sur le disque
  // quand il y est, et sinon dans les archives, comme tout le reste. Le couper
  // avec `data\` ferait perdre la copie disque à toute installation sans
  // dossier `data\` — celle d'un joueur, justement —, sans une ligne au journal.
  if (normalized_key.starts_with(kDataFolderDiskKey) && data_directory_.empty()) {
    return {};
  }
  return FindUnder(root_, normalized_key);
}

Result<std::vector<std::uint8_t>> Vfs::Read(std::string_view utf8_path) {
  return ReadKey(ToLookupKey(utf8_path), utf8_path);
}

Result<std::vector<std::uint8_t>> Vfs::ReadRaw(std::string_view cp949_path) {
  return ReadKey(formats::NormalizeGrfPath(cp949_path), cp949_path);
}

Result<std::vector<std::uint8_t>> Vfs::ReadKey(const std::string& key,
                                               std::string_view reported_path) {
  if (const std::filesystem::path on_disk = ResolveOnDisk(key); !on_disk.empty()) {
    auto source = core::FileByteSource::Open(on_disk);
    if (!source) {
      return std::unexpected(source.error());
    }
    return (*source)->ReadBlock(0, (*source)->size());
  }

  for (MountedArchive& mounted : archives_) {
    if (const formats::GrfEntry* entry = mounted.archive.Find(key); entry != nullptr) {
      return mounted.archive.Read(*entry, key);
    }
  }
  return Fail(ErrorCode::kNotFound, std::string(reported_path));
}

bool Vfs::ContainsRaw(std::string_view cp949_path) const {
  const std::string key = formats::NormalizeGrfPath(cp949_path);
  if (!ResolveOnDisk(key).empty()) {
    return true;
  }
  return std::ranges::any_of(
      archives_, [&key](const MountedArchive& mounted) { return mounted.archive.Contains(key); });
}

bool Vfs::Contains(std::string_view utf8_path) const {
  const std::string key = ToLookupKey(utf8_path);
  if (!ResolveOnDisk(key).empty()) {
    return true;
  }
  return std::ranges::any_of(
      archives_, [&key](const MountedArchive& mounted) { return mounted.archive.Contains(key); });
}

std::vector<std::string> Vfs::List(std::string_view utf8_prefix,
                                   std::string_view utf8_suffix) const {
  const std::string prefix = ToLookupKey(utf8_prefix);
  const std::string suffix = core::AsciiLower(utf8_suffix);

  // Un set trie et dédoublonne : le même fichier peut être dans plusieurs
  // archives, et l'appelant ne veut pas le voir deux fois.
  std::set<std::string> found;
  const auto keep = [&](std::string_view name) {
    if (name.starts_with(prefix) && (suffix.empty() || name.ends_with(suffix))) {
      found.emplace(name);
    }
  };
  // ⚠ PAR LE PRÉFIXE, ET NON PAR LA LISTE TRIÉE DE L'ARCHIVE : celle-ci copie
  // et trie tous ses noms, et une archive officielle en compte 339 000, pour
  // n'en garder ici qu'une poignée.
  for (const MountedArchive& mounted : archives_) {
    for (const std::string_view name : mounted.archive.NamesStartingWith(prefix)) {
      keep(name);
    }
  }

  // LE DISQUE AUSSI : le dossier `data\` passe avant les archives à la
  // lecture, il ne peut pas disparaître de l'inventaire. Ses noms prennent la
  // même clé que ceux d'une archive — CP949, minuscules, contre-obliques —,
  // si bien qu'un fichier présent des deux côtés ne se liste qu'une fois.
  //
  // ⚠ UN DOSSIER ILLISIBLE N'ARRÊTE RIEN : le parcours saute ce qu'il ne peut
  // pas ouvrir, et l'inventaire des archives reste entier.
  if (!data_directory_.empty()) {
    std::error_code code;
    for (std::filesystem::recursive_directory_iterator walk(
             data_directory_, std::filesystem::directory_options::skip_permission_denied, code);
         !code && walk != std::filesystem::recursive_directory_iterator(); walk.increment(code)) {
      if (!walk->is_regular_file(code)) {
        continue;
      }
      const std::u8string relative = walk->path().lexically_relative(data_directory_).u8string();
      keep(ToLookupKey(
          std::string(kDataFolderKey) +
          std::string(reinterpret_cast<const char*>(relative.data()), relative.size())));
    }
  }

  std::vector<std::string> listed;
  listed.reserve(found.size());
  for (const std::string& name : found) {
    listed.push_back(Cp949ToUtf8Lossy(name));
  }
  return listed;
}

std::filesystem::path FindOnDisk(const std::filesystem::path& root, std::string_view game_path) {
  std::string relative(game_path);
  std::replace(relative.begin(), relative.end(), '\\',
               static_cast<char>(std::filesystem::path::preferred_separator));
  std::error_code code;
  if (const std::filesystem::path direct = root / relative;
      std::filesystem::is_regular_file(direct, code)) {
    return direct;
  }
#ifndef _WIN32
  if (const std::filesystem::path found = ResolveIgnoringCase(root, relative);
      !found.empty() && std::filesystem::is_regular_file(found, code)) {
    return found;
  }
#endif
  return {};
}

}  // namespace ro::vfs
