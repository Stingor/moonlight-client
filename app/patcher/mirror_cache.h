#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace moonclient::patcher {

// CE QUE LE CLIENT SAIT DÉJÀ DES FICHIERS DU MIROIR, dans le dossier du jeu.
//
// Hacher toute l'installation à chaque lancement prendrait des secondes ; on
// retient donc l'empreinte de chaque fichier avec sa taille et sa date sur CE
// disque. Tant que les deux sont inchangées, l'empreinte retenue vaut.
//
// ⚠ LA DATE N'EST QU'UN INDICE LOCAL, jamais une comparaison avec le serveur :
// c'est l'empreinte qui décide si un fichier est à jour.
inline constexpr std::string_view kMirrorCacheFileName = "moonclient-patch-mirror.dat";

struct KnownFile {
  std::uint64_t size_bytes = 0;
  // `last_write_time` en tics de l'horloge des fichiers.
  std::int64_t write_ticks = 0;
  std::string sha256_hex;

  [[nodiscard]] bool operator==(const KnownFile&) const = default;
};

// Par chemin relatif, tel que le manifeste l'écrit.
using MirrorCache = std::map<std::string, KnownFile, std::less<>>;

// ⚠ UN FICHIER ILLISIBLE VAUT UN CACHE VIDE : tout se rehache, rien ne se perd.
[[nodiscard]] MirrorCache ParseMirrorCache(std::string_view text);
[[nodiscard]] std::string FormatMirrorCache(const MirrorCache& cache);

[[nodiscard]] MirrorCache ReadMirrorCache(const std::filesystem::path& install_root);
void WriteMirrorCache(const std::filesystem::path& install_root, const MirrorCache& cache);

}  // namespace moonclient::patcher
