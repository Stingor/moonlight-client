#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/grf.h"

namespace ro::test_support {

// Construit une archive GRF valide EN MÉMOIRE.
//
// C'est la pièce qui rend le parseur testable : aucun asset Gravity ne peut être
// versionné dans ce dépôt, donc une suite de tests qui dépendrait d'un vrai GRF
// ne tournerait ni en CI ni chez un contributeur. Ici, l'archive est fabriquée à
// la demande, y compris dans ses formes pathologiques.

struct GrfFileSpec {
  // Octets bruts du nom, tels qu'ils iront dans la table (CP949 en vrai).
  std::string name;
  std::vector<std::uint8_t> content;
  // Par défaut une entrée fichier. Mettre kGrfFlagFile | 0x02 pour simuler une
  // entrée chiffrée, ou 0 pour un répertoire.
  std::uint8_t flags = formats::kGrfFlagFile;
};

struct GrfBuildOptions {
  std::uint32_t major = 2;

  // Écrit un `seed` non nul en 34..37 SOUS une signature 3.0.
  //
  // C'est le cas du « 2.0 déguisé » : le fichier se présente comme un 3.0 mais
  // porte encore le champ seed du 2.0 là où le 3.0 met les octets hauts de son
  // offset de table. Le lecteur doit alors retomber sur l'offset 32 bits.
  bool legacy_seed_under_v3 = false;
};

[[nodiscard]] std::vector<std::uint8_t> BuildGrf(const std::vector<GrfFileSpec>& files,
                                                 const GrfBuildOptions& options = {});

// Raccourci : contenu texte d'un fichier de test.
[[nodiscard]] std::vector<std::uint8_t> Bytes(std::string_view text);

// COMPRESSE EN ZLIB, comme l'archive range ses fichiers et comme le serveur
// garde un emblème de guilde. Rend vide si zlib refuse.
[[nodiscard]] std::vector<std::uint8_t> Deflate(std::span<const std::uint8_t> input);

}  // namespace ro::test_support
