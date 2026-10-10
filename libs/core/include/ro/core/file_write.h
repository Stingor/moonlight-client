#pragma once

#include <filesystem>
#include <functional>
#include <string_view>

#include "ro/core/byte_sink.h"
#include "ro/core/error.h"

namespace ro::core {

// ÉCRIRE UN FICHIER SANS JAMAIS LE LAISSER À MOITIÉ ÉCRIT.
//
// ⚠ UN FICHIER DE RÉGLAGES NE SE TRONQUE PAS. Écrire directement par-dessus la
// cible ouvre une fenêtre — courte, mais réelle — pendant laquelle le fichier
// existe vidé de son ancien contenu et pas encore rempli du nouveau. Une
// coupure, un disque plein ou un Alt+F4 pendant cette fenêtre ne perd pas la
// dernière modification : il perd TOUT ce que le fichier portait.
//
// On remplit donc un voisin temporaire, on le ferme — donc on le vide vraiment
// sur le disque —, et on le déplace PAR-DESSUS la cible. Le renommage d'un
// fichier vers un autre du MÊME dossier est atomique sur les deux systèmes que
// ce dépôt construit ; c'est pour cela que le temporaire est un voisin et non
// un fichier du dossier temporaire du système.
//
// ⚠ EN CAS D'ÉCHEC, LA CIBLE RESTE INTACTE — et le temporaire est retiré. Une
// écriture qui échoue vaut mieux qu'une écriture qui réussit à moitié : la
// première se signale, la seconde se découvre à la partie suivante.
//
// Le dossier parent est créé au besoin : un réglage enregistré avant que le
// dossier n'existe est le cas normal de la première partie.
[[nodiscard]] Result<void> WriteFileAtomically(const std::filesystem::path& path,
                                               std::string_view content);

// LA MÊME ÉCRITURE, pour ce qui ne tient pas dans un tampon : `fill` écrit dans
// le voisin temporaire, par morceaux, à travers une `ByteSink`.
//
// ⚠ `fill` PEUT LIRE LA CIBLE — c'est le cas d'une archive qu'on réécrit —,
// mais il doit l'avoir REFERMÉE en rendant la main : Windows refuse de
// remplacer un fichier encore ouvert.
[[nodiscard]] Result<void> WriteFileAtomicallyWith(
    const std::filesystem::path& path, const std::function<Result<void>(ByteSink&)>& fill);

}  // namespace ro::core
