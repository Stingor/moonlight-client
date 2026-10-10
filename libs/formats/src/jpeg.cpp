#include "ro/formats/jpeg.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <string>

#include <stb_image.h>

namespace ro::formats {
namespace {

// LE DÉBUT DE TOUT JPEG : le marqueur de début d'image, puis celui d'un segment.
constexpr std::array<std::uint8_t, 3> kJpegSignature = {0xFF, 0xD8, 0xFF};

}  // namespace

core::Result<Image> DecodeJpeg(std::span<const std::uint8_t> data) {
  // ⚠ PAS DE GARDE SUR LE VIDE, ET C'EST LE SABOTAGE QUI L'A DIT : la retirer
  // ne faisait échouer aucun essai. `stb_image` lit par une paire
  // (pointeur, longueur) dont il vérifie la borne avant chaque octet — une
  // entrée vide n'y déréférence rien et ressort refusée, avec un motif à lui
  // qui vaut mieux que le nôtre. Une garde qui ne garde rien est une ligne
  // qu'on relit en croyant qu'elle sert.
  //
  // ⚠ LA TAILLE, EN REVANCHE, PASSE PAR UN `int` : c'est l'interface de
  // `stb_image`, et un fichier plus grand que ce qu'un entier signé porte
  // arriverait NÉGATIF, donc lu comme une taille absurde. Une illustration du
  // client pèse moins d'un mégaoctet ; on refuse plutôt que de tronquer en
  // silence.
  if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "JPEG trop grand pour être décodé");
  }

  // 🔴 `stb_image` DEVINE LE FORMAT, et en connaît une dizaine : sans ce refus,
  // un « .jpg » de l'archive qui porterait un PSD ou un HDR passerait par des
  // décodeurs que le client n'a aucune raison d'exposer.
  if (data.size() < kJpegSignature.size() ||
      !std::equal(kJpegSignature.begin(), kJpegSignature.end(), data.begin())) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "JPEG illisible : signature absente");
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width,
                                          &height, &channels, 4);
  if (pixels == nullptr || width <= 0 || height <= 0) {
    // ⚠ LE MOTIF DE `stb_image` EST REPRIS TEL QUEL : il nomme le marqueur qui
    // a fâché, ce qu'aucune formule de notre cru ne saurait dire.
    const char* why = stbi_failure_reason();
    stbi_image_free(pixels);
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      std::string("JPEG illisible : ") + (why != nullptr ? why : "sans motif"));
  }

  Image image;
  image.width = width;
  image.height = height;
  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  image.pixels.assign(pixels, pixels + count);
  stbi_image_free(pixels);
  return image;
}

}  // namespace ro::formats
