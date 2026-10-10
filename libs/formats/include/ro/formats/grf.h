#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/byte_source.h"
#include "ro/core/error.h"

namespace ro::formats {

// Archive GRF, en lecture seule, versions 2.0 et 3.0.
//
// Le 3.0 (signature « Event Horizon ») n'est PAS chiffré : c'est le 2.0 étendu
// au-delà de 4 Gio. Trois différences, et rien d'autre :
//   · l'offset de table est un u64 (octets 30..37), ce qui consomme le champ
//     `seed` du 2.0 ;
//   · l'en-tête de table porte un dword de rembourrage (toujours nul) devant le
//     couple taille compressée / taille décompressée ;
//   · une entrée fait 21 octets de métadonnées au lieu de 17, son offset de
//     données étant lui aussi un u64.
//
// Référence croisée : GRF/Core/GrfHeader.cs et GRF/Core/FileEntry.cs de
// GRFEditor.

// Taille de l'en-tête, et origine des offsets de données : un `data_offset`
// d'entrée se compte À PARTIR de la fin de l'en-tête, pas du début du fichier.
inline constexpr std::uint64_t kGrfHeaderSize = 46;

// Bit 0 : entrée fichier (sinon répertoire, que l'on n'indexe pas).
inline constexpr std::uint8_t kGrfFlagFile = 0x01;
// Bits 1 et 2 : chiffrement DES, réservé aux GRF protégés.
inline constexpr std::uint8_t kGrfFlagEncryptedMask = 0x06;

struct GrfEntry {
  std::uint64_t data_offset = 0;
  std::uint32_t compressed_size = 0;
  // Taille alignée sur DES. Conservée pour la fidélité au format ; inutile en
  // lecture d'un GRF non chiffré, où compressed_size suffit.
  std::uint32_t aligned_size = 0;
  std::uint32_t real_size = 0;
  std::uint8_t flags = 0;

  [[nodiscard]] bool is_encrypted() const noexcept { return (flags & kGrfFlagEncryptedMask) != 0; }
};

// UNE ENTRÉE FICHIER TELLE QUE LA TABLE L'ÉCRIT : nom brut, casse et
// séparateurs d'origine.
struct GrfListedEntry {
  std::string name;
  GrfEntry entry;
};

struct GrfListing {
  std::uint32_t version = 0;
  // Dans l'ordre de la table. Les répertoires n'y sont pas.
  std::vector<GrfListedEntry> files;
};

// LIT LA TABLE SANS LA NORMALISER, pour qui doit réécrire l'archive.
//
// ⚠ `GrfArchive` abaisse les noms en place pour les chercher : réécrire une
// archive depuis lui changerait la casse de chaque chemin. Cette lecture-ci
// garde les octets tels qu'ils sont, au prix d'une chaîne par entrée — ce qui
// ne se paie qu'au moment de fusionner un patch.
[[nodiscard]] core::Result<GrfListing> ListGrfEntries(core::ByteSource& source);

// Minuscules ASCII et séparateurs en antislash, comme le client normalise ses
// chemins d'archive. Seul A-Z est abaissé : un second octet CP949 peut tomber
// dans cette plage, et le toucher casserait le nom coréen.
[[nodiscard]] std::string NormalizeGrfPath(std::string_view path);

class GrfArchive {
 public:
  GrfArchive(const GrfArchive&) = delete;
  GrfArchive& operator=(const GrfArchive&) = delete;
  // Pas de `noexcept` : unordered_map ne garantit pas un déplacement sans
  // exception, et une spécification plus stricte que l'implicite sur une
  // fonction `= default` rend le programme mal formé.
  GrfArchive(GrfArchive&&) = default;
  GrfArchive& operator=(GrfArchive&&) = default;
  ~GrfArchive() = default;

  [[nodiscard]] static core::Result<GrfArchive> Open(std::unique_ptr<core::ByteSource> source);
  [[nodiscard]] static core::Result<GrfArchive> OpenFile(const std::filesystem::path& path);

  // Version brute de l'en-tête (0x0200, 0x0300…).
  [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
  [[nodiscard]] std::uint32_t major_version() const noexcept { return version_ >> 8; }

  [[nodiscard]] std::size_t entry_count() const noexcept { return entries_.size(); }

  // Noms normalisés, triés. Ce sont des OCTETS CP949, pas de l'UTF-8 : la
  // conversion pour affichage est un problème distinct, et la faire ici
  // rendrait la clé de recherche dépendante d'une table de transcodage.
  //
  // ⚠ BÂTIE À LA DEMANDE, ET JAMAIS AU MONTAGE. Énumérer TOUTE une archive est
  // un geste d'OUTIL — `roc-cli grf ls` — que le jeu ne fait jamais. La liste
  // coûte une copie de tous les noms puis un tri, soit près d'une seconde en
  // Debug sur une archive officielle, et elle reste en mémoire ensuite. Un
  // inventaire qui ne garde que quelques noms passe par `NamesStartingWith`.
  [[nodiscard]] const std::vector<std::string>& names() const;

  // Les noms qui commencent par ce préfixe, écrit dans n'importe quelle casse et
  // avec l'un ou l'autre séparateur, SANS ORDRE : ni copie ni tri de ceux
  // qu'on écarte.
  //
  // ⚠ DES VUES SUR LA TABLE DE L'ARCHIVE : elles ne vivent pas plus qu'elle.
  [[nodiscard]] std::vector<std::string_view> NamesStartingWith(std::string_view prefix) const;

  [[nodiscard]] const GrfEntry* Find(std::string_view path) const;
  [[nodiscard]] bool Contains(std::string_view path) const { return Find(path) != nullptr; }

  // Décompresse le contenu. Erreur si l'entrée est absente, chiffrée, ou si le
  // flux ne rend pas la taille annoncée.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> Read(std::string_view path);
  // La même lecture, pour une entrée DÉJÀ trouvée par `Find` : qui a cherché ne
  // cherche pas deux fois. `reported_path` ne sert qu'aux messages d'erreur.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> Read(const GrfEntry& entry,
                                                             std::string_view reported_path);

 private:
  GrfArchive() = default;

  std::unique_ptr<core::ByteSource> source_;
  // ⚠ LA TABLE DÉCOMPRESSÉE EST GARDÉE, ET C'EST ELLE QUI PORTE LES NOMS. Les
  // clés de `entries_` sont des VUES sur ces octets, normalisés sur place : une
  // archive officielle compte 339 000 entrées, et leur faire chacune sa propre
  // chaîne coûtait autant d'allocations pour recopier des octets déjà là.
  //
  // Le déplacement est sûr — un `vector` emporte son tampon SANS le bouger, donc
  // les vues restent valides — et la copie est interdite. Mais `table_` ne doit
  // plus jamais être vidée, réduite ni réallouée tant que `entries_` vit.
  std::vector<std::uint8_t> table_;
  std::unordered_map<std::string_view, GrfEntry> entries_;
  // `mutable` parce que `names()` est un ACCESSEUR : ce qu'il rend ne dépend
  // que des entrées, seul le moment où on le calcule change.
  mutable std::vector<std::string> names_;
  mutable bool names_listed_ = false;
  std::uint32_t version_ = 0;
};

}  // namespace ro::formats
