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
#include "ro/render/companion_cart.h"
#include "ro/render/ground_light.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "world/map_stage.h"

// LES CHARRETTES DES ACTEURS : celles qui naissent derrière tout marchand qui
// en pousse une, et qu'il traîne. La mécanique vit dans
// `ro/render/companion_cart.h` ; ici, on la nourrit du monde et on la dessine.
namespace moonclient {

// L'ÉTAT « POUSSE UNE CHARRETTE », et sa valeur est le TYPE de la charrette.
//
// 🔴 C'EST LA SEULE SOURCE, pour soi comme pour les autres. Le mot d'options de
// l'acteur ne dit rien d'une charrette sous ce serveur : c'est cet état, posé à
// l'entrée en vue et à chaque changement, qui porte le type.
inline constexpr ro::sim::StatusEffectId kEfstOnPushCart = 673;

// Le type de charrette qu'un acteur pousse, zéro pour aucune.
[[nodiscard]] std::int32_t CartTypeOf(const ro::sim::Actor& actor);

// Ce qu'une image donne aux charrettes.
struct CartFrame {
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
  // L'OMBRE RONDE sous chaque charrette. Sous le soleil, sa silhouette en porte
  // une, et la ronde se tait.
  bool round_shadow = true;
};

class ActorCarts {
 public:
  // Fait naître et disparaître les charrettes, puis avance chacune d'autant de
  // tics de seize millisecondes que le temps en a compté. Rend ce qui se
  // dessine à cet instant, ombre comprise.
  //
  // ⚠ LE DESSIN LIT LE MONDE ET NE LE CHANGE PAS : une charrette n'est pas un
  // acteur du monde, on ne la clique pas.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Update(const CartFrame& frame);

  [[nodiscard]] std::size_t size() const noexcept { return carts_.size(); }

  // Le changement de carte emporte toutes les charrettes ; elles renaissent
  // derrière leurs maîtres quand ceux-ci reparaissent.
  void Forget() {
    carts_.clear();
    drawn_.clear();
    last_tick_ms_.reset();
  }

 private:
  // Les deux fichiers d'une charrette, lus, et sa cadence. ⚠ UN ÉCHEC SE RETIENT
  // AUSSI : relire à chaque image un fichier absent coûterait une recherche par
  // image.
  struct Art {
    std::optional<ro::formats::SprSprite> sprite;
    std::optional<ro::formats::ActAnimation> animation;
    float frame_delay = ro::render::kCartMinFrameDelay;
  };

  struct Kept {
    ro::render::Cart cart;
    const Art* art = nullptr;
    // Le type à la naissance : un autre type fait renaître la charrette.
    std::int32_t type = 0;
    ro::render::ComposedActor frame;
  };

  const Art* ArtFor(ro::vfs::Vfs& vfs, std::string_view spr_path, std::string_view act_path);
  void Census(const CartFrame& frame);
  void Tick(const CartFrame& frame, std::uint32_t tick_ms);
  void ComposeShadow(ro::vfs::Vfs& vfs);

  std::map<std::string, std::unique_ptr<Art>> arts_;
  std::map<std::uint32_t, Kept> carts_;
  std::vector<ro::render::DrawnActor> drawn_;
  std::optional<std::uint32_t> last_tick_ms_;
  // L'OMBRE, commune à toutes : la première image de `shadow.act`, à l'échelle
  // un. Composée à la première charrette.
  const Art* shadow_art_ = nullptr;
  ro::render::ComposedActor shadow_;
};

}  // namespace moonclient
