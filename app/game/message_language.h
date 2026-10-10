#pragma once

// LES MOTS DE L'INTERFACE TENUS PAR `ItemWords`, dans la boucle de la partie.

#include <cstddef>

#include "ro/vfs/item_words.h"

namespace moonclient {

// SUIT LA LANGUE DE L'INTERFACE. Quand elle a changé depuis `followed_serial`,
// repose les mots dans la nouvelle langue, puis retient le nouveau compte.
//
// ⚠ À APPELER À CHAQUE IMAGE : la langue se change en jeu, et les mots ne se
// reposent pas d'eux-mêmes. Un compte qui ne peut pas être celui d'une langue
// force la première pose.
void FollowMessageLanguage(ro::vfs::ItemWords& words, std::size_t& followed_serial);

}  // namespace moonclient
