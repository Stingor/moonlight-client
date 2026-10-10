#pragma once

#include <span>
#include <string_view>

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

// Un groupe montré d'un bloc, avec son intertitre.
//
// ⚠ `label` EST EN FRANÇAIS NU : c'est l'appelant qui le traduit au moment de
// l'afficher. L'envelopper ici le traduirait DEUX fois, et le catalogue se
// remplirait de textes déjà traduits dont on chercherait longtemps l'origine.
struct EmojiCategory {
  std::string_view label;
  std::span<const std::string_view> items;
};

[[nodiscard]] std::span<const EmojiCategory> EmojiCategories();

}  // namespace moonclient
