#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gr2_model.h"
#include "ro/render/actor_model_motion.h"
#include "ro/render/actor_model_pose.h"
#include "ro/render/actor_model_scene.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/camera.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/actor.h"
#include "ro/sim/guild.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "guild/emblem_plates.h"
#include "world/actor_view.h"
#include "world/map_stage.h"

namespace moonclient {

// LES ACTEURS QUI SONT DES MODÈLES : l'Emperium, les gardiens de forteresse,
// le drapeau de guilde, les coffres au trésor.
//
// Le client les reconnaît au nom que sa table des classes leur donne — un nom
// de modèle au lieu d'un nom de sprite — et les suit par un chemin à part : le
// modèle se pose, s'oriente, s'anime et s'éclaire selon ses propres règles, et
// seule son EMPRISE à l'écran rejoint celle des sprites, pour qu'on le clique,
// qu'on lise son nom et sa jauge comme ceux de tout le monde.
//
// Ce module tient ce qui DURE d'une image à l'autre — les modèles chargés, le
// mouvement et l'orientation de chaque acteur, les restes des morts — et rend,
// à chaque image, ce qui se peint.
class ActorModels {
 public:
  explicit ActorModels(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  struct Frame {
    const ro::render::SpriteCatalog* catalog = nullptr;
    // Pour l'instant où le coup d'un attaquant porte. Nul : les coups reçus ne
    // se voient pas.
    ro::render::ActorWardrobe* wardrobe = nullptr;
    const ro::sim::World* world = nullptr;
    ro::sim::WalkPlanner* planner = nullptr;
    const MapStage* stage = nullptr;
    const ro::render::Camera* camera = nullptr;
    const ro::sim::GuildBook* guild = nullptr;
    EmblemPlates* plates = nullptr;
    std::uint32_t now_ms = 0;
  };

  // Compose les acteurs en modèle de cette image. Chacun rejoint `scene` —
  // `drawn` et `ids` APPARIÉS, comme les autres — avec une emprise de modèle et
  // aucun quad ; un mort rejoint `vanished` et laisse son reste. Le recensement
  // apprend qu'ils ont un corps.
  void Compose(const Frame& frame, ComposedScene& scene, SceneCensus& census);

  // Ce qui se peint de cette image : les acteurs, puis les restes.
  [[nodiscard]] std::span<const ro::render::DrawnActorModel> drawn() const noexcept {
    return drawn_;
  }

  // Les effets que les maillages font naître à cette image, à jouer par leur
  // identifiant là où ils naissent.
  [[nodiscard]] std::span<const ro::render::ActorModelMeshEvent> mesh_events() const noexcept {
    return mesh_events_;
  }

 private:
  // Un modèle chargé, ou l'échec de son chargement, retenu pour ne pas le
  // redemander à chaque image.
  struct Loaded {
    std::optional<ro::render::ActorModelResource> resource;
    std::optional<std::int32_t> index;
  };

  // Ce que le client garde d'un acteur en modèle entre deux images.
  struct Tracked {
    std::string resource_name;
    const Loaded* model = nullptr;
    std::uint32_t job = 0;
    // Le mouvement joué et l'instant où son horloge est partie de zéro.
    ro::render::ActorModelMotion motion = ro::render::ActorModelMotion::kStand;
    std::uint32_t motion_since_ms = 0;
    float attack_speed = 1.0F;
    bool walking = false;
    std::uint32_t attack_seen_ms = 0;
    bool attack_seen = false;
    std::uint32_t hurt_seen_ms = 0;
    bool hurt_seen = false;
    // L'angle du corps imposé par une attaque, tant qu'elle dure.
    std::optional<float> attack_body_degrees;
    float yaw_degrees = 0.0F;
    std::uint32_t yaw_ticked_ms = 0;
    bool yaw_set = false;
    glm::vec3 feet{0.0F};
    // Le dernier tic où ses maillages ont fait naître un effet.
    std::uint32_t events_ticked_ms = 0;
  };

  // Le reste d'un mort : sa mort se joue à sa place, puis il s'efface.
  struct Remnant {
    const Loaded* model = nullptr;
    glm::vec3 feet{0.0F};
    float yaw_degrees = 0.0F;
    std::uint32_t since_ms = 0;
  };

  [[nodiscard]] const Loaded* Model(std::string_view resource_name);
  [[nodiscard]] const ro::formats::GrannyAnimation* Animation(
      const ro::render::SpriteCatalog& catalog, const Loaded& model,
      ro::render::ActorModelMotion motion);
  [[nodiscard]] const ro::render::ActorModelAnimation* Bound(
      const ro::render::SpriteCatalog& catalog, const Loaded& model,
      ro::render::ActorModelMotion motion);

  void Follow(const Frame& frame, const ro::sim::Actor& actor, Tracked& tracked);
  void Turn(Tracked& tracked, float body_degrees, std::uint32_t now_ms);
  void Paint(const Frame& frame, const ro::sim::Actor& actor, Tracked& tracked);
  void PaintRemnants(const Frame& frame);

  ro::vfs::Vfs* vfs_ = nullptr;
  std::map<std::string, std::unique_ptr<Loaded>> models_;
  std::map<std::string, std::optional<ro::formats::GrannyScene>> motion_files_;
  std::map<std::pair<const Loaded*, ro::render::ActorModelMotion>,
           std::optional<ro::render::ActorModelAnimation>>
      bindings_;
  std::map<std::uint32_t, Tracked> tracked_;
  // La carte des acteurs suivis.
  std::string stage_name_;
  std::vector<Remnant> remnants_;
  std::vector<ro::render::DrawnActorModel> drawn_;
  std::vector<ro::render::ActorModelMeshEvent> mesh_events_;
  // Le corps sans quad que portent les entrées des modèles dans la scène : les
  // couches qui lisent `DrawnActor::actor` n'y trouvent rien à peindre.
  ro::render::ComposedActor no_quads_;
};

}  // namespace moonclient
