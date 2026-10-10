#pragma once

#include <cstddef>

#include "ro/core/byte_sink.h"
#include "ro/core/byte_source.h"
#include "ro/core/error.h"
#include "ro/formats/thor.h"

namespace ro::formats {

// CE QU'UNE FUSION A FAIT, pour le journal.
struct GrfMergeReport {
  std::size_t kept_files = 0;
  std::size_t replaced_files = 0;
  std::size_t added_files = 0;
  std::size_t removed_files = 0;
};

// ÉCRIT DANS `merged` L'ARCHIVE GRF `previous` À LAQUELLE ON APPLIQUE `patch`.
//
// `previous` nul : l'archive n'existait pas, et le patch la crée.
//
// Les entrées du patch s'appliquent dans l'ordre : pour un même chemin, la
// dernière l'emporte, qu'elle remplace ou supprime. Les chemins se comparent
// sans égard à la casse ASCII ni au sens des séparateurs, comme le client les
// cherche ; les fichiers conservés gardent leur nom d'origine, octet pour
// octet, et leurs octets bruts.
//
// ⚠ CHAQUE FLUX DU PATCH EST DÉCOMPRESSÉ UNE FOIS AVANT D'ÊTRE RECOPIÉ, et
// doit rendre la taille annoncée. Le recopier sans l'éprouver rangerait dans
// l'archive un fichier que le jeu ne pourrait pas lire, et rien ne le dirait
// avant qu'il le demande.
//
// ⚠ SEULE UNE ARCHIVE 2.0 SE RÉÉCRIT : une autre version est refusée.
//
// Ce n'est pas une fusion EN PLACE, et c'est voulu : l'appelant écrit à côté,
// puis remplace l'ancienne d'un seul renommage. Un arrêt en route laisse
// l'ancienne intacte.
[[nodiscard]] core::Result<GrfMergeReport> MergeThorIntoGrf(ThorArchive& patch,
                                                            core::ByteSource* previous,
                                                            core::ByteSink& merged);

}  // namespace ro::formats
