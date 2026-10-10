#pragma once

#include <string_view>

namespace moonclient {

// OÙ LE CLIENT ÉCRIT CE QUI APPARTIENT AU JOUEUR.
//
// ⚠ DANS `savedata\`, AVEC LES FICHIERS DU CLIENT D'ORIGINE. C'est là que le jeu
// garde ce qui est au joueur — ses touches, la place de ses fenêtres, ses macros
// — et c'est donc là qu'il va chercher, et qu'une sauvegarde de dossier emporte
// nos réglages avec les siens.
//
// ⚠ UN NOM AJOUTÉ ICI SE VÉRIFIE CONTRE UNE VRAIE INSTALLATION, et l'extension
// ne suffit pas à nous distinguer : ce dossier porte déjà du YAML qui n'est pas
// le nôtre (`lang\`) à côté des `.lua` aux noms fixes du client
// (`UserKeys.lua`, `OptionInfo.lua`…). Écraser l'un des siens rendrait le client
// d'origine inutilisable.
//
// ⚠ ET TOUJOURS NOS PROPRES FICHIERS, JAMAIS LES SIENS. Écrire dans son
// `UserKeys.lua` mêlerait deux jeux de raccourcis qui ne parlent pas des mêmes
// actions, et il relirait un fichier qu'il ne comprendrait qu'à moitié.
inline constexpr std::string_view kSaveFolder = "savedata";

}  // namespace moonclient
