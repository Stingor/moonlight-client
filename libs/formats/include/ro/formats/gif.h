#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// ── UN GIF ANIMÉ, À PARTIR D'IMAGES D'ÉCRAN ─────────────────────────────────
//
// C'est le seul format qu'un salon de discussion, un rapport de bug ou un wiki
// jouent tous les trois sans rien installer. Une vidéo demande un lecteur, un
// encodeur, et souvent un site pour la déposer.
//
// 🔴 UNE PALETTE UNIQUE POUR TOUT LE CLIP, ET NON UNE PAR IMAGE. Quantifier
// chaque image pour elle-même est plus simple et donne un résultat MOUVANT : le
// même pixel de ciel tombe sur une couleur voisine d'une image à l'autre, et les
// aplats se mettent à grouiller. Une seule palette supprime le scintillement —
// et, comme elle est calculée sur l'ensemble, elle ne se laisse pas capturer par
// une image atypique.
//
// 🔴 ET DES IMAGES DIFFÉRENTIELLES. Seuls les pixels qui ont changé depuis
// l'image précédente sont écrits ; le reste est l'index transparent au-dessus
// d'un mode de disposition 1 (« laisser en place »), donc la toile continue de
// les montrer. Chaque image est en outre rognée à la boîte de ce qui a bougé.
// Sur un écran de jeu largement immobile, c'est un ordre de grandeur — la
// différence entre un fichier qu'on poste et un fichier que personne n'ouvrira.
//
// ⚠ AUCUN TRAMAGE, ET C'EST UN CHOIX. Il sèmerait du bruit sur chaque aplat, et
// chaque pixel tramé compte comme « changé » : il défait exactement ce que la
// différence vient de gagner.
//
// ⚠ PAS D'ÉCRITURE DE FICHIER ICI. Cette couche ne touche pas au système de
// fichiers — c'est ce qui la rend éprouvable sur des images synthétiques. Le
// résultat est un tampon d'octets, que l'appelant pose où il veut.

// Le délai entre deux images, en CENTIÈMES DE SECONDE : c'est l'unité du format
// lui-même, et la convertir ici cacherait que le GIF ne sait pas faire mieux.
// ⚠ EN DEÇÀ DE DEUX, LES NAVIGATEURS N'OBÉISSENT PLUS — ils substituent 10 à un
// délai de 0 ou 1, héritage des pages qui s'en servaient pour tourner à fond.
inline constexpr int kFastestGifDelayCs = 2;

// Le nombre de couleurs de la table globale. ⚠ 255 ET NON 256 : l'index 0 est
// réservé à la transparence, sans laquelle il n'y a pas d'image différentielle.
inline constexpr int kGifPictureColors = 255;

// Encode des images RGBA8 de MÊME taille en un GIF animé qui boucle.
//
// L'alpha est ignoré : une capture d'écran est opaque, et l'index transparent
// sert ici à dire « inchangé », pas « translucide ».
//
// Échoue si la liste est vide, si deux images n'ont pas la même taille, ou si
// l'une d'elles est vide.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeAnimatedGif(
    std::span<const Image> frames, int delay_centiseconds);

}  // namespace ro::formats
