#include "ro/render/texture_bleed.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ro::render {
namespace {

// Décalages des quatre voisins directs. Les diagonales n'apporteraient rien :
// la propagation par vagues successives les atteint en deux temps.
constexpr int kNeighbourDx[] = {-1, 1, 0, 0};
constexpr int kNeighbourDy[] = {0, 0, -1, 1};

}  // namespace

formats::Image BleedTransparentEdges(const formats::Image& image) {
  formats::Image filled = image;
  if (image.width <= 0 || image.height <= 0) {
    return filled;
  }

  const auto width = static_cast<std::size_t>(image.width);
  const auto height = static_cast<std::size_t>(image.height);
  const std::size_t count = width * height;
  if (filled.pixels.size() < count * 4) {
    return filled;
  }

  // Un parcours en largeur depuis TOUS les pixels opaques à la fois : chaque
  // pixel transparent reçoit ainsi la couleur du pixel opaque le plus proche,
  // et chacun n'est traité qu'une fois.
  //
  // ⚠ TROIS CHOIX DE STRUCTURE, ET CHACUN A ÉTÉ MESURÉ. Cette passe tourne sur
  // toutes les textures d'une carte au chargement : quinze secondes à l'entrée
  // de Prontera en Debug, contre un démarrage instantané en Release. La
  // différence ne venait pas de l'algorithme, qui est linéaire, mais de ce avec
  // quoi il était écrit.
  //
  //   `vector<uint8_t>` et non `vector<bool>` — le second est une spécialisation
  //   à BITS dont chaque accès passe par un proxy et un masque. Un octet par
  //   pixel coûte de la mémoire qu'on rend aussitôt, et il se lit d'un accès.
  //
  //   Un `vector` et un CURSEUR, et non une `queue`. Cette file n'AJOUTE que :
  //   rien n'est jamais retiré par devant, on avance seulement. La `queue` par
  //   défaut est un `deque`, qui alloue par blocs — des milliers d'allocations
  //   pour un parcours qui en demande UNE.
  //
  //   Un POINTEUR sur les pixels. L'indexation d'un `vector` est vérifiée en
  //   Debug à chaque accès, et il y en a quatre par pixel.
  std::vector<std::uint8_t> known(count, 0);
  std::vector<std::size_t> frontier;
  frontier.reserve(count);
  std::uint8_t* const pixels = filled.pixels.data();

  for (std::size_t index = 0; index < count; ++index) {
    if (pixels[index * 4 + 3] != 0) {
      known[index] = 1;
      frontier.push_back(index);
    }
  }
  // Image entièrement transparente : aucune couleur d'où partir.
  if (frontier.empty()) {
    return filled;
  }

  for (std::size_t head = 0; head < frontier.size(); ++head) {
    const std::size_t index = frontier[head];
    const auto x = static_cast<int>(index % width);
    const auto y = static_cast<int>(index / width);

    for (int direction = 0; direction < 4; ++direction) {
      const int nx = x + kNeighbourDx[direction];
      const int ny = y + kNeighbourDy[direction];
      if (nx < 0 || ny < 0 || nx >= image.width || ny >= image.height) {
        continue;
      }
      const std::size_t neighbour =
          static_cast<std::size_t>(ny) * width + static_cast<std::size_t>(nx);
      if (known[neighbour] != 0) {
        continue;
      }
      known[neighbour] = 1;
      // La couleur se propage, l'alpha reste à zéro : le pixel demeure
      // invisible, mais il ne pollue plus la moyenne des mipmaps.
      pixels[neighbour * 4 + 0] = pixels[index * 4 + 0];
      pixels[neighbour * 4 + 1] = pixels[index * 4 + 1];
      pixels[neighbour * 4 + 2] = pixels[index * 4 + 2];
      frontier.push_back(neighbour);
    }
  }

  return filled;
}

bool HasTransparentTexel(const formats::Image& image) {
  // Un POINTEUR, comme pour le parcours : l'indexation vérifiée d'un `vector`
  // coûterait en Debug plus que la lecture elle-même.
  const std::size_t count = image.pixels.size() / 4;
  const std::uint8_t* const pixels = image.pixels.data();
  for (std::size_t index = 0; index < count; ++index) {
    if (pixels[index * 4 + 3] == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace ro::render
