#pragma once

#include <cstddef>

#include "ro/core/byte_sink.h"
#include "ro/core/byte_source.h"
#include "ro/core/error.h"

namespace ro::formats {

// CE QU'UN ALLÈGEMENT A FAIT, pour le journal.
struct GrfDeltaReport {
  // Chemins que la base n'a pas.
  std::size_t added_files = 0;
  // Chemins que la base a, avec un autre contenu.
  std::size_t changed_files = 0;
  // Chemins que la base a, avec le même contenu : ils ne sont pas écrits.
  std::size_t identical_files = 0;
};

// ÉCRIT DANS `delta` L'ARCHIVE `newer` PRIVÉE DE CE QUE `base` A DÉJÀ À
// L'IDENTIQUE.
//
// Montée DEVANT `base`, l'archive écrite fait voir au client exactement le
// contenu de `newer` là où `newer` a le fichier, et celui de `base` ailleurs.
//
// Les chemins se comparent comme le client les cherche, sans égard à la casse
// ASCII ni au sens des séparateurs ; pour un même chemin, la dernière entrée de
// la table l'emporte, comme à la lecture. Deux fichiers sont identiques quand
// leurs octets DÉCOMPRESSÉS le sont : deux outils compressent différemment le
// même fichier. Une entrée chiffrée, qu'on ne sait pas décompresser, n'est
// identique que par ses octets bruts.
//
// Les fichiers gardés conservent leur nom d'origine, octet pour octet, et leurs
// octets bruts.
//
// ⚠ L'ARCHIVE ÉCRITE EST UN GRF 2.0, quelle que soit la version lue : elle est
// refusée si elle dépasse quatre gibioctets.
[[nodiscard]] core::Result<GrfDeltaReport> WriteGrfDelta(core::ByteSource& base,
                                                         core::ByteSource& newer,
                                                         core::ByteSink& delta);

}  // namespace ro::formats
