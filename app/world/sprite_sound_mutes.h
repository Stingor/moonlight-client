#pragma once

#include <span>
#include <string>
#include <string_view>

namespace moonclient {

// LES SONS D'IMAGE QUE LE JOUEUR A FAIT TAIRE. Voir `SpriteSoundOptions`.
//
// ⚠ CE CLIENT JOUE TOUS LES SONS DES IMAGES D'UN .act, la première image d'une
// action comprise. Le client d'origine sautait celle-là : les sons de mort
// posés sur l'image 0 — celui du Doppelganger, ceux du Bio Lab — ne s'y
// entendaient jamais. Certains méritent de retrouver ce silence.
struct SpriteSoundMutes {
  // Le bruit de l'animation d'attente des pets, qui revient à chaque tour.
  bool pet_idle = false;
  // Les fichiers tus, sous leur clé : voir `SoundFileKey`.
  std::span<const std::string> files;
};

// LA CLÉ D'UN FICHIER SON : son nom seul, en minuscules ASCII.
//
// ⚠ LE NOM SEUL : un .act écrit « doppleganger_die.wav », un effet
// « effect\ef_hit2.wav », et le silence vaut pour le fichier où qu'il soit.
// ⚠ ASCII SEULEMENT : un nom coréen ne se met pas en minuscules octet par
// octet sans s'abîmer.
[[nodiscard]] std::string SoundFileKey(std::string_view path);

// Ce fichier est-il tu ?
[[nodiscard]] bool SilencesFile(const SpriteSoundMutes& mutes, std::string_view path);

}  // namespace moonclient
