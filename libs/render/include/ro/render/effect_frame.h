#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/formats/str.h"

namespace ro::render {

// Ce qu'une couche d'effet MONTRE à une image donnée.
//
// Un fichier d'effet ne décrit pas chaque image : il pose des clés, et la
// couche se déforme de l'une à l'autre. Ce module fait ce calcul, et rien
// d'autre — aucune texture, aucun GPU. C'est ce qui le rend vérifiable au banc.

// Une clé, et ce qu'elle fait ensuite.
struct PreparedEffectKey {
  formats::StrKeyFrame key;
  // Vrai quand la couche se DÉFORME de cette clé vers la suivante ; faux pour
  // une coupure nette, où elle tient sa pose jusqu'à la clé d'après.
  bool morphs = false;
};

// Une couche prête à jouer.
struct PreparedEffectLayer {
  std::vector<PreparedEffectKey> keys;
  // Combien de textures la couche porte : les modes d'animation en dépendent.
  std::size_t texture_count = 0;
  // Où ses textures commencent dans la liste à plat de l'effet. Les couches
  // partagent souvent les mêmes fichiers, mais chacune les numérote pour elle :
  // une liste à plat évite d'avoir à porter le numéro de couche avec le numéro
  // de texture jusqu'au renderer.
  std::size_t first_texture = 0;
};

// LE TIC DU JEU FAIT SEIZE MILLISECONDES, et la cadence que le fichier déclare
// n'entre nulle part.
//
// Le client demande soixante images par seconde, mais il range `1000 / 60` en
// division ENTIÈRE, soit SEIZE, comme pas de sa logique de jeu. Le jeu joue donc
// ses effets à 62,5 images par seconde.
//
// L'horloge d'un effet est un COMPTEUR ENTIER que chaque pas incrémente d'un,
// et ses sons se déclenchent sur une ÉGALITÉ EXACTE avec ce compteur. Rien n'y lit la
// cadence du fichier — que les trois mille quatre cent vingt et un fichiers de
// l'installation déclarent pourtant tous à soixante.
//
// L'écart est de 4 % : invisible sur une image, une demi-seconde de décalage
// sur une boucle de vingt secondes.
inline constexpr std::int32_t kEffectTickMs = 16;

// Combien de tics tiennent dans une seconde. La division est ENTIÈRE, comme
// celle du client : cela sert de borne de rattrapage, pas de cadence.
inline constexpr std::int32_t kEffectTicksPerSecond = 1000 / kEffectTickMs;

struct PreparedEffect {
  std::vector<PreparedEffectLayer> layers;
  // Toutes les textures de toutes les couches, dans l'ordre des couches. Ce
  // sont des noms de FICHIER, relatifs au dossier de l'effet.
  std::vector<std::string> texture_names;
  std::int32_t last_frame = 0;
};

// Met un effet en état d'être joué.
//
// POURQUOI CE PASSAGE. Le fichier mêle deux sortes de clés : celles qui POSENT
// un état, et celles de type un, qui n'en portent aucun — elles disent
// seulement que la clé PRÉCÉDENTE se déforme vers la suivante. Les laisser dans
// la liste ferait lire des poses vides. On les retire donc, en reportant leur
// seule information sur la clé qu'elles suivent.
//
// Exception du format : une clé de type un suivie, à la MÊME image, d'une clé
// posée annule la déformation — c'est une coupure nette, pas un fondu.
[[nodiscard]] PreparedEffect PrepareEffect(const formats::StrEffect& effect);

// L'image d'un effet après `elapsed_ms`, ou RIEN quand il a fini de jouer.
//
// UNE IMAGE PAR TIC, et le tic fait seize millisecondes : voir `kEffectTickMs`.
//
// Un effet ne boucle PAS : il court de sa première image à sa dernière, puis
// disparaît. Ce qui doit durer — l'aura d'un statut, un piège posé — est
// RELANCÉ par l'appelant, jamais prolongé : c'est ainsi que le jeu procède, et
// c'est ce qui rend la fin d'un effet observable d'un seul endroit.
[[nodiscard]] std::optional<std::int32_t> EffectFrameAt(const PreparedEffect& effect,
                                                        float elapsed_ms);

// L'image d'un effet qui REJOUE SON FICHIER sans fin.
//
// 🔴 C'EST CE QUE FAIT LE CLIENT POUR CE QUI DOIT DURER, et non un resemis : il
// compte les tours et remet les clés au début quand l'un finit. La différence se VOIT — un `.str`
// resemé laisse un trou d'une image entre les deux tours, et le recouvrir
// montre les deux à la fois, ce qui est un FLASH en mélange additif. Les deux
// se lisent comme un clignotement régulier, et c'est ce que le pilier d'un
// butin faisait.
//
// ⚠ CELA SUPPOSE UN FICHIER FAIT POUR BOUCLER, dont la dernière image répète la
// première. Les piliers de butin le sont : leurs clés d'image 0 et 60 portent
// les mêmes coins et la même couleur. Un fichier qui s'éteint à la fin
// sauterait, lui, à chaque tour.
[[nodiscard]] std::int32_t EffectLoopFrameAt(const PreparedEffect& effect, float elapsed_ms);

// L'état d'une couche à une image, prêt à composer un quad.
struct EffectLayerFrame {
  // Les quatre coins, abscisses puis ordonnées, en pixels autour du centre.
  std::array<float, 8> corners = {};
  // La portion de texture : origine puis étendue.
  std::array<float, 4> uv = {};
  // Rouge, vert, bleu, alpha, de zéro à 255.
  std::array<float, 4> color = {};
  // Le centre de la couche, en pixels d'écran autour de kStrOrigin.
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  float angle_degrees = 0.0F;
  std::size_t texture_index = 0;
  // Les facteurs de mélange, dans la numérotation de Direct3D.
  std::int32_t blend_source = 0;
  std::int32_t blend_destination = 0;
};

// Ce que la couche montre à cette image, ou RIEN quand elle ne se dessine pas.
//
// Deux cas la laissent invisible : elle n'a pas COMMENCÉ — sa première clé est
// plus loin —, ou elle a FINI. Une couche finit à sa dernière clé, sauf si
// celle-ci se déforme : la couche tient alors sa pose jusqu'au bout de l'effet.
// Sans cette fin, une couche qui s'achève opaque reste figée à l'écran pendant
// que le reste de l'effet se termine.
//
// `frame_fraction`, dans ]-1, 1[, place le CONTINU — coins, coordonnées,
// couleur, place, angle — à `frame + frame_fraction`, déformation comprise :
// le lissage des effets, voir `effect_step_blend.h`. Négative, elle montre le
// chemin depuis l'image d'avant. Le discret suit l'image entière : la couche
// paraît ou finit, son image de texture et son mélange sont ceux de `frame`.
// Zéro rend exactement l'image.
[[nodiscard]] std::optional<EffectLayerFrame> EffectLayerAt(const PreparedEffectLayer& layer,
                                                            std::int32_t frame,
                                                            std::int32_t last_frame,
                                                            float frame_fraction = 0.0F);

}  // namespace ro::render
