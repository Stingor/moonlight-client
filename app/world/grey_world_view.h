#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/grey_world.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/staff_tools.h"
#include "ro/vfs/vfs.h"

#include "world/map_stage.h"

namespace moonclient {

// LE QUADRILLAGE DE GREYWORLD, posé sur le relief autour du joueur.
//
// La figure (`ro::render::ComposeGreyWorldCells`) sait quelles cases peindre ;
// ici on sait où est le joueur et où lire l'image du motif.
class GreyWorldGrid {
 public:
  // CE QUI SE PEINT CETTE IMAGE : un seul lot, une seule image. Vide quand le
  // quadrillage est éteint, qu'il n'y a pas de grille, pas encore de joueur, ou
  // que l'image du motif ne se lit pas. Le résultat vit jusqu'au prochain appel.
  [[nodiscard]] std::span<const ro::render::DrawnGroundQuads> Cells(
      ro::vfs::Vfs& vfs, const ro::render::GreyWorldLook& look, const MapStage& stage,
      const ro::sim::World& world, ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

 private:
  // L'image d'un motif, lue une fois. ⚠ L'ÉCHEC SE RETIENT AUSSI : sans cela on
  // relirait l'archive à chaque image pour un fichier qui n'y est pas.
  [[nodiscard]] const ro::formats::Image* TextureFor(ro::vfs::Vfs& vfs, std::string_view path);

  std::map<std::string, std::optional<ro::formats::Image>, std::less<>> textures_;
  std::vector<ro::render::GroundQuad> quads_;
  std::vector<ro::render::DrawnGroundQuads> lots_;
  // Le chemin que le lot désigne doit survivre à l'appel : le renderer indexe
  // son téléversement dessus.
  std::string texture_path_;
};

// LE SOL UNI DE CETTE IMAGE, ou rien : celui de l'établi du staff, sinon celui
// de GreyWorld.
//
// 🔴 LA COULEUR DE L'ÉTABLI GAGNE. Les deux veulent la même chose du terrain,
// mais un fond de capture d'écran ne doit pas voir sa teinte changer sous lui à
// cause d'un réglage de confort.
//
// 🔴 ET L'ÉTABLI EST GATÉ SUR SON DROIT, parce que son réglage SURVIT au
// lancement : sans cela, un compte qui a perdu son niveau de groupe ouvrirait
// sa partie sur un terrain repeint, sans la fenêtre qui porte la case pour
// l'éteindre.
//
// ⚠ IL NE SAUTE RIEN : le terrain se dessine comme d'habitude, seule sa
// couleur change, donc ce qui passe derrière une colline reste caché.
[[nodiscard]] std::optional<std::array<float, 3>> FlatGroundOf(
    const ro::ui::FlatGroundLook& staff_ground, bool staff_tools_allowed,
    const ro::render::GreyWorldLook& grey_world);

// Vrai quand l'établi du staff impose sa couleur au sol : la page de GreyWorld
// le dit alors sous sa propre couleur, qui ne s'affiche pas.
[[nodiscard]] bool StaffGroundWins(const ro::ui::FlatGroundLook& staff_ground,
                                   bool staff_tools_allowed);

}  // namespace moonclient
