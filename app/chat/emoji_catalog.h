#pragma once

#include <span>
#include <string_view>

#include "ro/ui/chat_strip.h"

namespace moonclient {

// LA PALETTE D'EMOJI UNICODE.
//
// Rien à voir avec `emotion_catalog.h`, malgré l'usage voisin, et la distinction
// vaut d'être tenue :
//
//   · une ÉMOTICONE du jeu est une image du GRF (`emotion.act`). Elle voyage sur
//     le fil sous forme de NUMÉRO, et seul un client de Ragnarok sait la
//     redessiner ;
//   · un EMOJI est du TEXTE. Il part tel quel, et n'importe quoi qui sache
//     l'écrire le montre.
//
// D'où deux onglets distincts dans le sélecteur plutôt qu'une grille mélangée :
// ce qui arrive au bout du fil n'est pas de même nature. Et deux gestes
// distincts au clic — l'émoticone S'ENVOIE seule, l'emoji S'INSÈRE dans la
// phrase qu'on est en train d'écrire.

// Un groupe montré d'un bloc, avec son intertitre DÉJÀ TRADUIT.
//
// ⚠ `label` NE VIT QUE JUSQU'AU PROCHAIN CHANGEMENT DE LANGUE : c'est le
// pointeur que rend `Tr`, et il appartient au catalogue.
struct EmojiCategory {
  std::string_view label;
  std::span<const std::string_view> items;
};

[[nodiscard]] std::span<const EmojiCategory> EmojiCategories();

// Reprend les intertitres de groupes remplis par `EmojiCategories()`, dans le
// même ordre. À appeler à chaque image : gardé d'une image à l'autre, un
// intertitre pointerait dans un catalogue de langue déjà remplacé.
void RelabelEmojiGroups(std::span<ro::ui::ChatEmojiGroup> groups);

}  // namespace moonclient
