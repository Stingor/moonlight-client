#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/model_sight.h"

namespace ro::render {

// LE QUADTREE DU DÉCOR, tel que le client le monte à l'ouverture d'une carte.
//
// Six étages : la racine couvre le sol, chaque nœud se coupe en quatre selon X
// et Z, et les feuilles — cinq étages sous la racine — rangent les poses dont
// la boîte les touche. Le client n'éprouve contre le segment du regard QUE les
// poses des feuilles que sa droite traverse.
//
// ⚠ LES BORNES QUI RANGENT NE SONT PAS CELLES QUE LA DROITE ÉPROUVE. Les poses
// se rangent d'après un découpage calculé sur l'emprise du sol, sans hauteur ;
// la droite éprouve ensuite les bornes du fichier quand il en porte, et sinon
// des feuilles relevées à la hauteur de leur sol et de leurs poses.
//
// Tout y compte dans le repère du client : voir `model_sight.h`.
class SightQuadTree {
 public:
  // L'étage des feuilles, la racine étant au zéro.
  static constexpr std::int32_t kLeafDepth = 5;

  struct Node {
    glm::vec3 lowest{};
    glm::vec3 highest{};
    // Rangs des enfants dans l'ordre du client ; aucun pour une feuille.
    std::array<std::uint32_t, 4> children{};
    bool leaf = false;
    // Les rangs des boîtes rangées ici, dans leur ordre.
    std::vector<std::uint32_t> items;
  };

  SightQuadTree() = default;

  // `gat` peut manquer : ses hauteurs ne comptent alors pour rien.
  [[nodiscard]] static SightQuadTree Build(const formats::RswScene& rsw, const formats::GndMap& gnd,
                                           const formats::GatMap* gat,
                                           std::span<const SightBox> boxes);

  // Les rangs des boîtes rangées dans les feuilles que la demi-droite traverse,
  // feuille après feuille dans l'ordre du parcours. Une boîte rangée dans
  // plusieurs feuilles traversées y paraît autant de fois.
  void CollectAlong(const glm::vec3& origin, const glm::vec3& direction,
                    std::vector<std::uint32_t>& found) const;

  // En profondeur d'abord, l'ordre du fichier : la racine au rang zéro.
  [[nodiscard]] std::span<const Node> nodes() const noexcept { return nodes_; }

 private:
  std::uint32_t Subdivide(const glm::vec3& lowest, const glm::vec3& highest, std::int32_t depth);
  void Insert(std::uint32_t node, std::uint32_t item, const SightBox& box);
  void Collect(std::uint32_t node, const glm::vec3& origin, const glm::vec3& direction,
               std::vector<std::uint32_t>& found) const;
  void RaiseLeaves(const formats::GndMap& gnd, const formats::GatMap* gat,
                   std::span<const SightBox> boxes);

  std::vector<Node> nodes_;
};

// LA DROITE DU REGARD CONTRE UN NŒUD : le test du client, axe par axe, sur une
// droite de longueur 999 999.
//
// 🔴 PARALLÈLE À UNE TRANCHE, LA DROITE NE COMPTE QUE LES SEPT DIXIÈMES DU
// NŒUD : son origine doit tomber dans la part centrale, sans quoi le nœud est
// manqué — et avec lui tout ce qu'il range. C'est le cas d'une caméra qui
// regarde droit vers le nord et d'un joueur au milieu de l'écran.
[[nodiscard]] bool RayMeetsNode(const glm::vec3& origin, const glm::vec3& direction,
                                const glm::vec3& lowest, const glm::vec3& highest);

// UNE BOÎTE ORIENTÉE CONTRE UN NŒUD : les quinze axes séparateurs du client, à
// un millionième près, avec ses propres rayons sur les neuf axes croisés.
[[nodiscard]] bool BoxMeetsNode(const SightBox& box, const glm::vec3& lowest,
                                const glm::vec3& highest);

}  // namespace ro::render
