#pragma once

#include <string>
#include <string_view>

namespace ro::vfs {

// ── LES MOTS QUE LE CLIENT D'ORIGINE TIRAIT DE SA TABLE DE MESSAGES ─────────
//
// Ce sont des textes de NOTRE interface, posés par `core::Tr` à la lecture des
// mots et reposés à chaque changement de langue : voir `FillMessageWords` et
// l'ADR 0090. La table du jeu natif n'est plus lue ; elle reste une ressource à
// consulter (`scripts/msgstring.py chercher`).
//
// ⚠ PRIVÉ À `vfs` : cet en-tête vit dans `src/`.

// UN TEXTE ENTRE DES ESPACES DE BORD QUI LUI APPARTIENNENT. Une clé de
// traduction n'en porte pas ; certains de ces textes se collent pourtant à un
// autre — « Beloved » devant le nom d'un œuf, « - 3 obtained. » derrière celui
// d'un objet —, et leur espace est le séparateur.
[[nodiscard]] std::string Edged(std::string_view lead, const char* text, std::string_view trail);

// Journalise, une fois par nom, un gabarit qu'on ne sait pas imprimer, et rend
// une vue vide : l'appelant se tait plutôt que d'écrire une ligne fausse.
std::string_view NoteMissingWord(std::string_view name);

}  // namespace ro::vfs
