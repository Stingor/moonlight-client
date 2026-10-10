#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// DÉCODE UN PNG, en RGBA8, rangée zéro en haut.
//
// Les données en portent quelques centaines : les illustrations des PNJ, les
// bannières de nom de carte, et quelques textures d'effet et d'interface.
//
// ⚠ L'ALPHA EST CELUI DU FICHIER, et c'est la différence avec le reste : un PNG
// dit lui-même où il est transparent. Palette, niveaux de gris ou RGB sans
// alpha ressortent opaques, en quatre canaux quand même — le reste du
// programme ne connaît que le RGBA8. La clé magenta reste le choix de
// l'appelant, comme pour les autres formats.
//
// ⚠ IL S'APPUIE SUR `stb_image`, déjà compilé pour le JPEG et les emblèmes.
[[nodiscard]] core::Result<Image> DecodePng(std::span<const std::uint8_t> data);

}  // namespace ro::formats
