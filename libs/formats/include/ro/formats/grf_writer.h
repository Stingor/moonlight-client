#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/byte_sink.h"
#include "ro/core/error.h"
#include "ro/formats/grf.h"

namespace ro::formats {

// ÉCRIT UNE ARCHIVE GRF 2.0, entrée après entrée, sans rien garder en mémoire
// que la table.
//
//   en-tête   « Master of Magic » et sa clé (30 octets), u32 position de la
//             table depuis la fin de l'en-tête, u32 graine nulle, u32 nombre
//             d'entrées + 7, u32 version 0x200
//   données   le flux zlib de chaque entrée, complété de zéros jusqu'à sa
//             taille alignée
//   table     u32 taille compressée, u32 taille décompressée, puis la table
//             zlib : nom terminé par NUL, u32 taille compressée, u32 taille
//             alignée, u32 taille réelle, u8 drapeaux, u32 position
//
// ⚠ LA TAILLE ALIGNÉE EST LE MULTIPLE DE HUIT SUPÉRIEUR, et les octets de
// complément sont ÉCRITS. Un lecteur qui lit la taille alignée — ce que font
// ceux qui gèrent le chiffrement — ne sort ainsi jamais de l'entrée.
//
// ⚠ 2.0 SEULEMENT : les positions sont des u32. Une archive qui dépasserait
// quatre gibioctets est refusée, pas tronquée.
//
// La sortie doit être VIDE au départ, et vivre plus longtemps que l'écrivain.
class GrfWriter {
 public:
  [[nodiscard]] static core::Result<GrfWriter> Begin(core::ByteSink& sink);

  // Ajoute un fichier dont le contenu est DÉJÀ compressé en zlib.
  [[nodiscard]] core::Result<void> AddCompressed(std::string_view name,
                                                 std::span<const std::uint8_t> zlib_stream,
                                                 std::uint32_t real_size);

  // Recopie une entrée d'une autre archive telle quelle : octets bruts sur sa
  // taille alignée, drapeaux compris. C'est ce qui garde une entrée chiffrée
  // intacte sans savoir la lire.
  //
  // ⚠ LES OCTETS PEUVENT MANQUER À LA FIN, de moins de huit : la dernière
  // entrée d'une archive s'arrête parfois à la fin du fichier, sans ses zéros
  // de complément. Ils sont alors rétablis.
  [[nodiscard]] core::Result<void> AddRaw(std::string_view name, const GrfEntry& entry,
                                          std::span<const std::uint8_t> aligned_bytes);

  // Écrit la table, puis l'en-tête. L'archive n'est valide qu'après.
  [[nodiscard]] core::Result<void> Finish();

 private:
  explicit GrfWriter(core::ByteSink& sink) : sink_(&sink) {}

  [[nodiscard]] core::Result<void> Append(std::string_view name, const GrfEntry& entry,
                                          std::span<const std::uint8_t> bytes);

  core::ByteSink* sink_ = nullptr;
  std::vector<std::uint8_t> table_;
  // Position de la prochaine donnée, comptée depuis la fin de l'en-tête.
  std::uint64_t data_end_ = 0;
  std::uint32_t entry_count_ = 0;
};

}  // namespace ro::formats
