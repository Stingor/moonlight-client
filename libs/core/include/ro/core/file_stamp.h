#pragma once

#include <cstdint>
#include <string>

namespace ro::core {

// L'ÉTAT D'UN FICHIER DE L'INSTALLATION, tel que celui qui le sert le décrit :
// de quoi dire qu'un fichier a changé sans le relire.
//
// Deux états se COMPARENT CHAMP À CHAMP : un fichier dont un seul champ diffère
// est un autre fichier. Une empreinte du contenu demanderait de le lire, ce
// que l'état existe justement pour éviter.
//
// ⚠ CE N'EST PAS UNE PREUVE : une archive réécrite qui garderait le même
// fichier à la même place, à la même taille, passerait pour inchangée. C'est
// le prix d'un état qui se lit sans décompresser.
struct FileStamp {
  enum class Origin : std::uint8_t {
    // Un fichier du disque, sous le dossier du client.
    kDisk,
    // Une entrée d'archive.
    kArchive,
  };

  Origin origin = Origin::kDisk;
  // L'archive qui sert le fichier, par son NOM de fichier en UTF-8 — pas son
  // chemin : une installation déplacée garde ses états ; vide pour le disque.
  std::string container;
  // La place de l'entrée dans l'archive, en octets ; zéro pour le disque.
  std::uint64_t offset = 0;
  // Sa taille stockée — compressée pour une archive —, et sa taille réelle.
  std::uint64_t stored_size = 0;
  std::uint64_t size = 0;
  // La date d'écriture du fichier du disque, en tics de son horloge ; zéro pour
  // une archive, dont les entrées ne portent aucune date.
  std::int64_t written_ticks = 0;

  friend bool operator==(const FileStamp&, const FileStamp&) = default;
};

}  // namespace ro::core
