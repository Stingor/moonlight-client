#pragma once

#include <cstdint>
#include <string_view>

namespace moonclient {

// LE CATALOGUE DES ÉMOTICÔNES DU PROTOCOLE : leur numéro, et le nom court par
// lequel on les désigne.
//
// 🔴 TOUTES NE SE PROPOSENT PAS. Le sprite en porte près de cent, mais une
// partie appartient au JEU et aux PNJ — le serveur les envoie, personne ne les
// demande. La grille du sélecteur les montrait quand même, en cases vides, et
// promettait donc des gestes qui n'en sont pas.
//
// ⚠ POUR EN EXCLURE UNE, LUI DONNER UN NOM VIDE — ne PAS retirer sa ligne, ce
// qui décalerait tout ce qui suit. Le rang EST le numéro que le serveur attend :
// un décalage ferait envoyer l'émoticône d'à côté, en silence.
//
// ⚠ ET LE NOM NE DÉCIDE PAS SEUL. Une installation peut ne pas porter l'action
// correspondante ; c'est `app` qui recoupe le catalogue avec ce que le sprite
// contient vraiment, et ne propose que l'intersection.

// Combien le PROTOCOLE en connaît. ⚠ Ce nombre ne bouge que si le protocole
// gagne des émoticônes — et alors on ajoute à la FIN, jamais au milieu.
inline constexpr std::uint16_t kKnownEmotions = 81;

// Le nom court d'une émoticône, ou une vue VIDE quand elle ne se propose pas.
[[nodiscard]] std::string_view EmotionName(std::uint16_t emotion);

// Se propose-t-elle au joueur ? C'est la question que pose la grille.
[[nodiscard]] bool EmotionIsOffered(std::uint16_t emotion);

}  // namespace moonclient
