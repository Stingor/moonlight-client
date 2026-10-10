#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// DÉCODE UN JPEG, en RGBA8, rangée zéro en haut.
//
// 🔴 CE N'EST PAS LE FORMAT ORDINAIRE DE CE CLIENT, et c'est pourquoi il est
// rare dans ses données : RO peint son interface en BMP et en TGA, sans canal
// alpha, avec le magenta pour transparence. Le JPEG n'y sert qu'à ce qui se
// regarde d'un bloc et ne se découpe pas — le fond de l'écran de connexion,
// quelques bandeaux, et les trente-deux images de chaque NAPPE D'EAU, dont
// l'opacité vient du réglage de la zone et jamais de l'image.
//
// 🔴 NE JAMAIS LUI APPLIQUER LA CLÉ MAGENTA. Le JPEG code AVEC PERTE : un pixel
// peint en magenta pur n'en ressort pas pur, et des pixels qui ne l'étaient pas
// peuvent s'en approcher. La clé percerait donc des trous au hasard et en
// manquerait ailleurs — voir `ApplyMagentaColorKey`, et qui a le droit de
// l'appeler.
//
// ⚠ L'ALPHA RENDU VAUT 255 PARTOUT, et c'est juste : un JPEG est opaque. On
// demande quand même quatre canaux, parce que le reste du programme ne connaît
// que le RGBA8.
//
// ⚠ IL S'APPUIE SUR `stb_image`, déjà une dépendance du dépôt. Écrire un
// décodeur JPEG à la main — Huffman, DCT, sous-échantillonnage — pour UNE
// illustration reviendrait à payer mille lignes pour un fichier.
[[nodiscard]] core::Result<Image> DecodeJpeg(std::span<const std::uint8_t> data);

}  // namespace ro::formats
