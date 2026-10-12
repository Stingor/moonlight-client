#pragma once

#include <filesystem>
#include <string_view>

#include "game/save_folder.h"
#include "input/hotkeys.h"

namespace moonclient {

// LES RACCOURCIS, SUR LE DISQUE.
//
// 🔴 LE JEU NE L'ÉCRIT PLUS : les touches suivent le profil de réglages. Il se
// LIT encore — c'est ce qu'hérite un profil qui n'a pas de touches, et ce qui
// vaut avant l'entrée en jeu. Voir `SettingsProfilePort`.
//
// ⚠ DANS `savedata\`, AVEC CEUX DU CLIENT. C'est là que le jeu garde ce qui
// appartient au joueur — ses touches, la place de ses fenêtres, ses macros — et
// c'est donc là qu'un joueur va chercher, et qu'une sauvegarde de dossier
// emporte nos réglages avec les siens.
//
// ⚠ EN YAML, ET NON DANS LE LUA DU CLIENT. On LIT très bien du Lua, mais on n'en
// ÉCRIT pas : il aurait fallu échapper les chaînes à la main, ce qui casse au
// premier guillemet venu. YAML apporte son écrivain avec lui.
//
// ⚠ NOTRE PROPRE FICHIER, ET NOS PROPRES CLÉS. Le `UserKeys.lua` du client
// nomme SES commandes et emploie SES codes de touche ; écrire dedans mêlerait
// deux jeux de raccourcis qui ne parlent pas des mêmes actions, et le client
// d'origine relirait un fichier qu'il ne comprendrait qu'à moitié.
//
// ⚠ LES TOUCHES S'ÉCRIVENT PAR LEUR NOM, pas par leur code. « S », « F12 »,
// « Return » se relisent, se corrigent à la main, et surtout ne dépendent pas de
// la bibliothèque qui les numérote : le jour où elle change, un fichier de
// nombres ne veut plus rien dire et personne ne s'en aperçoit.

// Notre fichier, dans le dossier que nomme `save_folder.h`.
inline constexpr std::string_view kHotkeyFileName = "hotkeys.yml";

// Le chemin du fichier, sous la racine du client.
[[nodiscard]] std::filesystem::path HotkeyFilePath(const std::filesystem::path& client_root);

// Lit les raccourcis gardés PAR-DESSUS ceux qui sont déjà là.
//
// ⚠ UN FICHIER ABSENT N'EST PAS UNE ERREUR : c'est la première partie. Une
// entrée illisible ou inconnue est SAUTÉE, et l'action garde son raccourci
// d'origine — un fichier écrit par une autre version ne doit pas laisser un
// joueur sans clavier.
void LoadHotkeys(const std::filesystem::path& client_root, Hotkeys& into);

// Écrit toutes les touches. Rend faux si le fichier n'a pas pu être écrit —
// dossier en lecture seule, disque plein : le dire vaut mieux que perdre en
// silence ce que le joueur vient de régler.
[[nodiscard]] bool SaveHotkeys(const std::filesystem::path& client_root, const Hotkeys& from);

}  // namespace moonclient
