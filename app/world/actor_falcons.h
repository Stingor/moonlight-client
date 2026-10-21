#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/companion_falcon.h"
#include "ro/render/ground_light.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "world/map_stage.h"

// LES FAUCONS DES ACTEURS : ceux qui naissent sur tout porteur de l'option, le
// suivent, et que HT_SPRINGTRAP lance vers le piège visé — ou une compétence
// posée sur une case, vers cette case, ou le coup de Blitz Beat et de Falcon
// Assault, vers sa cible. La mécanique vit dans
// `ro/render/companion_falcon.h` ; ici, on la nourrit du monde et on la dessine.
namespace moonclient {

// La compétence qui lance le faucon du lanceur vers le piège qu'elle vise.
//
// ⚠ `scripts/etat_hors_des_tables.py` LA LIT ici pour ne plus compter ce geste
// comme manquant : son nom et sa forme sont un contrat.
inline constexpr std::uint16_t kSpringTrapSkill = 131;

// Ce qu'une image donne aux faucons.
struct FalconFrame {
  const ro::sim::World* world = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  const MapStage* stage = nullptr;
  ro::vfs::Vfs* vfs = nullptr;
  float camera_yaw_degrees = 0.0F;
  std::uint32_t now_ms = 0;
  // Le réglage « invocations translucides » du joueur.
  bool translucent = false;
  // La lumière de la carte, quand elle est allumée ; nulle sinon.
  const ro::render::GroundLight* light = nullptr;
  bool cold_light = false;
  // L'INTERPOLATION D'IMAGES du joueur : le faucon se peint entre ses deux
  // derniers tics. Voir `ro/render/tick_blend.h`.
  bool blended = false;
};

class ActorFalcons {
 public:
  // Fait naître et disparaître les faucons, lance ceux qu'un piège appelle, puis
  // avance chacun d'autant de tics de seize millisecondes que le temps en a
  // compté. Rend ce qui se dessine à cet instant.
  //
  // ⚠ LE DESSIN LIT LE MONDE ET NE LE CHANGE PAS : un faucon n'est pas un
  // acteur du monde, on ne le clique pas, il ne porte ni nom ni jauge.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Update(const FalconFrame& frame);

  [[nodiscard]] std::size_t size() const noexcept { return falcons_.size(); }

  // Le changement de carte emporte tous les faucons ; ils renaissent sur leurs
  // maîtres quand ceux-ci reparaissent.
  void Forget() {
    falcons_.clear();
    drawn_.clear();
    last_tick_ms_.reset();
  }

 private:
  // Les deux fichiers d'un oiseau, lus. ⚠ UN ÉCHEC SE RETIENT AUSSI : relire à
  // chaque image un fichier absent coûterait une recherche par image.
  struct Art {
    std::optional<ro::formats::SprSprite> sprite;
    std::optional<ro::formats::ActAnimation> animation;
  };

  struct Kept {
    ro::render::Falcon falcon;
    const Art* art = nullptr;
    // L'instant du dernier lancer SUIVI : un lancer plus ancien que le faucon,
    // ou déjà joué, ne le relance pas.
    std::uint32_t launched_ms = 0;
    ro::render::ComposedActor frame;
  };

  const Art* ArtFor(ro::vfs::Vfs& vfs, const ro::render::FalconArt& wanted);
  void Census(const FalconFrame& frame);
  void Launch(const FalconFrame& frame);
  void Tick(const FalconFrame& frame, std::uint32_t tick_ms);

  // Les coups déjà lus : chacun ne lance le faucon qu'une fois.
  ro::sim::StrikeCursor strike_cursor_;
  std::map<std::string, std::unique_ptr<Art>> arts_;
  std::map<std::uint32_t, Kept> falcons_;
  std::vector<ro::render::DrawnActor> drawn_;
  std::optional<std::uint32_t> last_tick_ms_;
};

}  // namespace moonclient
