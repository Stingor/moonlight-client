#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/byte_source.h"
#include "ro/core/error.h"

namespace ro::formats {

// Archive de patch THOR, en lecture seule.
//
// Une archive porte une liste d'entrées à appliquer DANS L'ORDRE, soit à une
// archive GRF (fusion), soit au dossier du jeu. Disposition, entiers en petit
// boutiste :
//
//   en-tête   signature « ASSF (C) 2007 Aeomin DEV » (24 octets)
//             u8   fusion dans une GRF (1) ou dossier du jeu (0)
//             u32  nombre d'entrées de la table, entrée d'intégrité comprise
//             u16  mode de table ; seul 0x30 (table compressée) existe
//             u8   longueur, puis le nom de la GRF cible (vide = GRF par défaut)
//             u32  taille compressée de la table
//             u32  position de la table, depuis le début du fichier
//   données   un flux zlib par entrée, placés entre l'en-tête et la table
//   table     un flux zlib ; par entrée :
//             u8   longueur, puis le chemin (octets bruts, séparateur « \ »)
//             u8   drapeaux ; bit 0 : entrée SUPPRIMÉE, qui s'arrête là
//             u32  position des données, depuis le début du fichier
//             u32  taille compressée
//             u32  taille décompressée
//
// L'ordre des données ne suit pas celui de la table. Un même chemin peut
// revenir plusieurs fois : l'application dans l'ordre fait gagner la dernière.

// Nom de l'entrée qui porte les sommes de contrôle des autres. Elle ne
// s'applique pas : `ThorArchive` la range à part.
inline constexpr std::string_view kThorIntegrityEntryName = "data.integrity";

// Le seul mode de table connu : une table zlib dont la position et la taille
// compressée suivent le nom de la GRF cible.
inline constexpr std::uint16_t kThorTableModeCompressed = 0x30;

// Bit 0 des drapeaux d'entrée : le chemin est à SUPPRIMER de la cible.
inline constexpr std::uint8_t kThorFlagRemoved = 0x01;

enum class ThorTarget : std::uint8_t {
  kGameDirectory,  // les chemins sont relatifs au dossier du jeu
  kGrf,            // les chemins sont des chemins internes d'une archive GRF
};

struct ThorEntry {
  // OCTETS BRUTS, casse et séparateurs tels que stockés : du CP949 dans les
  // archives du jeu. La conversion vers l'UTF-8 appartient à `vfs`.
  std::string path;
  bool is_removed = false;
  // Absent d'une entrée supprimée, donc nuls.
  std::uint32_t data_offset = 0;
  std::uint32_t compressed_size = 0;
  std::uint32_t real_size = 0;
};

// Une ligne de l'entrée d'intégrité : le CRC-32 (zlib) du contenu décompressé.
struct ThorChecksum {
  std::string path;
  std::uint32_t crc32 = 0;
};

class ThorArchive {
 public:
  ThorArchive(const ThorArchive&) = delete;
  ThorArchive& operator=(const ThorArchive&) = delete;
  ThorArchive(ThorArchive&&) noexcept = default;
  ThorArchive& operator=(ThorArchive&&) noexcept = default;
  ~ThorArchive() = default;

  // Lit l'en-tête et la table, et refuse l'archive entière au moindre doute :
  // chemin vide, absolu ou remontant par « .. », drapeau inconnu, données hors
  // du fichier, nombre d'entrées qui contredit l'en-tête.
  [[nodiscard]] static core::Result<ThorArchive> Open(std::unique_ptr<core::ByteSource> source);
  [[nodiscard]] static core::Result<ThorArchive> OpenFile(const std::filesystem::path& path);

  [[nodiscard]] ThorTarget target() const noexcept { return target_; }
  [[nodiscard]] bool merges_into_grf() const noexcept { return target_ == ThorTarget::kGrf; }
  // Vide : la GRF par défaut de l'installation.
  [[nodiscard]] const std::string& target_grf_name() const noexcept { return target_grf_name_; }

  // Les entrées à appliquer, dans l'ordre de la table, SANS l'entrée
  // d'intégrité.
  [[nodiscard]] std::span<const ThorEntry> entries() const noexcept { return entries_; }

  // La DERNIÈRE entrée de ce chemin — celle qui l'emporte —, sans égard à la
  // casse ASCII ni au sens des séparateurs. Nul si absent.
  [[nodiscard]] const ThorEntry* Find(std::string_view path) const;

  // Décompresse le contenu, en exigeant la taille annoncée. Une entrée
  // supprimée n'a pas de contenu : c'est une erreur de le demander.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> Read(const ThorEntry& entry);

  // Le flux zlib de l'entrée, tel que stocké. C'est la forme qu'une archive
  // GRF range aussi : fusionner le recopie sans le recompresser. ⚠ RIEN N'EST
  // VÉRIFIÉ ICI : qui recopie sans décompresser vérifie par l'intégrité ou par
  // une empreinte de l'archive entière.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> ReadCompressed(const ThorEntry& entry);

  [[nodiscard]] bool has_integrity() const noexcept { return integrity_.has_value(); }
  // Les sommes que porte l'entrée d'intégrité ; vide si l'archive n'en a pas.
  [[nodiscard]] core::Result<std::vector<ThorChecksum>> ReadChecksums();
  // Recalcule chaque somme listée sur le contenu de la dernière entrée de ce
  // chemin, et exige que toute entrée non supprimée soit listée. Sans entrée
  // d'intégrité, il n'y a rien à vérifier : succès.
  [[nodiscard]] core::Result<void> VerifyIntegrity();

 private:
  ThorArchive() = default;

  std::unique_ptr<core::ByteSource> source_;
  std::vector<ThorEntry> entries_;
  std::optional<ThorEntry> integrity_;
  std::string target_grf_name_;
  ThorTarget target_ = ThorTarget::kGrf;
};

}  // namespace ro::formats
