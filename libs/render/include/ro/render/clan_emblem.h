#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "ro/formats/image.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── L'EMBLÈME DE CLAN, À GAUCHE DU NOM ──────────────────────────────────────
//
// Chaque clan a son image dans l'installation, nommée par son identifiant sur
// deux chiffres. Elle prend la place de l'emblème de guilde — même coin, même
// géométrie — chez qui n'a pas de guilde. ⚠ JAMAIS DE CADRE autour d'elle.

// Le plus grand identifiant que le nom du fichier sait écrire : deux chiffres.
inline constexpr std::int32_t kLastClanEmblemId = 99;

// Le chemin de l'emblème de ce clan, en UTF-8 : le VFS le transcode.
[[nodiscard]] std::string ClanEmblemPath(std::int32_t clan_id);

// LES EMBLÈMES DE CLAN que l'installation porte, par identifiant.
//
// ⚠ LEUR ADRESSE EST LEUR IDENTITÉ pour la couche d'écran, comme celle des
// insignes de boss : une image rangée ne bouge plus tant que des quads la
// désignent. `std::map` ne déplace jamais une entrée.
struct ClanEmblems {
  std::map<std::int32_t, formats::Image> by_clan;

  // L'image de ce clan, ou nul : clan inconnu, ou fichier absent.
  [[nodiscard]] const formats::Image* Of(std::int32_t clan_id) const;
};

// Lit chaque emblème que l'installation porte. Un fichier absent se tait — la
// plupart des identifiants n'en ont pas —, un fichier illisible se dit au
// journal.
[[nodiscard]] ClanEmblems LoadClanEmblems(vfs::Vfs& vfs);

}  // namespace ro::render
