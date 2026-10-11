#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/skill_suite_table.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_draw_gate.h"
#include "world/actor_sounds.h"
#include "world/map_stage.h"

// LES CLASSES DU GESTIONNAIRE D'EFFETS EN JEU : ce qui est né, l'horloge qui
// les fait battre, et ce qu'elles donnent à peindre — des `.str`, des quads du
// monde, des sprites. Voir `ro/render/effect_class.h`.
namespace moonclient {

class EffectClassesInPlay {
 public:
  EffectClassesInPlay(ro::render::EffectCatalog& catalog, ro::vfs::Vfs& vfs)
      : catalog_(&catalog), vfs_(&vfs) {}

  // CE QUE LE GESTIONNAIRE REÇOIT À LA NAISSANCE : le porteur que l'instance
  // suit (zéro pour aucun), sa place dans notre repère, l'acteur lié dont elle
  // copie la place (zéro pour aucun), son paramètre, son retard.
  struct Birth {
    std::uint32_t effect_id = 0;
    std::uint32_t owner_id = 0;
    glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
    std::uint32_t bound_id = 0;
    // La place de l'acteur lié à la naissance, dans notre repère.
    std::optional<glm::vec3> bound_ground_position;
    std::int32_t level = -1;
    std::int32_t delay_ms = 0;
    // Voir `EffectClassBirth::owner_charm_element`.
    std::int32_t owner_charm_element = 0;
    std::int32_t owner_charm_count = 0;
    // NÉE D'UN CHAPEAU : la porte de dessin lui retire ses exceptions. Voir
    // `EffectDrawGate::Draws`.
    bool from_hat = false;
  };

  // Fait naître l'instance. Vrai quand une classe portée a répondu — même si
  // elle ne montre rien.
  bool Spawn(const Birth& birth, std::uint32_t now_ms);
  // RETIRE toutes les instances de ce numéro que ce porteur tient, comme le
  // client qui éteint un effet par son numéro.
  void Stop(std::uint32_t effect_id, std::uint32_t owner_id);
  // Vrai si ce porteur tient une instance vivante de ce numéro.
  [[nodiscard]] bool Carries(std::uint32_t effect_id, std::uint32_t owner_id) const;

  // LA VUE DE L'IMAGE et les deux réglages : à poser avant les naissances.
  void SeeScene(const ro::render::Camera& camera, float viewport_width, float viewport_height,
                bool minimal_effects);
  // LA PORTE DE DESSIN DE L'IMAGE : une instance dont le porteur est caché vit,
  // mais ne se dessine pas. Voir `EffectDrawGate`.
  void NoticeDrawGate(const EffectDrawGate& gate) { draw_gate_ = gate; }
  // DE COMBIEN FLOTTE CHAQUE PORTEUR QUI VOLE à cette image : voir
  // `ComposedScene::hover_pixels`. Nul, personne ne flotte.
  void NoticeHovers(const std::map<std::uint32_t, float>* hover_pixels) {
    hover_pixels_ = hover_pixels;
  }

  // Fait battre toutes les instances jusqu'à `now_ms`, puis compose ce qu'elles
  // montrent.
  void Advance(const ro::sim::World& world, const MapStage& stage, ro::sim::WalkPlanner& planner,
               std::uint32_t now_ms);

  [[nodiscard]] std::span<const ro::render::DrawnEffect> Drawn() const noexcept { return drawn_; }
  [[nodiscard]] std::span<const ro::render::DrawnEffect> DrawnBehindActors() const noexcept {
    return behind_;
  }
  [[nodiscard]] std::span<const ro::render::DrawnWorldQuads> WorldQuads() const noexcept {
    return quad_lots_;
  }
  // UN LOT DE SPRITES, rangé comme le client range leurs primitives : son mot de
  // rendu dit le mélange, l'épreuve de profondeur, et s'il se peint avant les
  // corps, avant les fichiers `.str` ou après eux.
  struct SpriteBatch {
    std::span<const ro::render::DrawnActor> actors;
    bool additive = false;
    bool depth_tested = true;
    bool before_str = false;
    bool behind_actors = false;
  };
  [[nodiscard]] std::span<const SpriteBatch> SpriteBatches() const noexcept {
    return sprite_batches_;
  }
  // LES QUADS D'ÉCRAN des classes qui peignent en plein écran (ADR 0022) :
  // leurs coins sont des pixels, et ils se peignent en dernier, sans caméra.
  [[nodiscard]] std::span<const ro::render::DrawnScreenQuads> ScreenQuads() const noexcept {
    return screen_quads_;
  }
  // Les sons du dernier battement, que l'appelant EMPORTE.
  [[nodiscard]] std::vector<SownNamedSound> TakeSounds() { return std::exchange(sounds_, {}); }
  // L'instant jusqu'où l'écran doit trembler, que l'appelant EMPORTE.
  [[nodiscard]] std::optional<std::uint32_t> TakeScreenQuake() {
    return std::exchange(quake_until_ms_, std::nullopt);
  }
  // UN EFFET QU'UNE INSTANCE DEMANDE SUR SON PORTEUR en s'en allant : son
  // numéro, le porteur, et sa place dans notre repère. L'appelant EMPORTE la
  // liste et fait naître ce que le porteur ne porte pas déjà.
  struct OwnerSpawn {
    std::uint32_t effect_id = 0;
    std::uint32_t owner_id = 0;
    glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  };
  [[nodiscard]] std::vector<OwnerSpawn> TakeOwnerSpawns() {
    return std::exchange(owner_spawns_, {});
  }

  void Forget();

 private:
  struct Living {
    std::unique_ptr<ro::render::EffectClassInstance> instance;
    std::uint32_t owner_id = 0;
    std::uint32_t bound_id = 0;
    bool from_hat = false;  // voir `Birth::from_hat`
  };

  [[nodiscard]] std::optional<glm::vec3> PlaceOf(std::uint32_t id, const ro::sim::World& world,
                                                 const MapStage& stage,
                                                 ro::sim::WalkPlanner& planner,
                                                 std::uint32_t at_ms) const;
  [[nodiscard]] ro::render::EffectClassView View() const;
  [[nodiscard]] const ro::render::LoadedEffect* StrFor(const std::string& path);
  [[nodiscard]] const ro::formats::Image& TextureFor(const std::string& path);
  [[nodiscard]] const ro::render::LoadedActorSprite* SpriteFor(const std::string& path);
  void Compose(const ro::render::EffectClassOutput& output);

  ro::render::EffectCatalog* catalog_ = nullptr;
  ro::vfs::Vfs* vfs_ = nullptr;
  EffectDrawGate draw_gate_;
  const std::map<std::uint32_t, float>* hover_pixels_ = nullptr;
  std::optional<ro::render::Camera> camera_;
  float viewport_width_ = 0.0F;
  float viewport_height_ = 0.0F;
  bool minimal_effects_ = false;
  std::vector<Living> living_;
  std::uint32_t last_tick_ms_ = 0;
  bool clock_started_ = false;
  std::uint32_t seed_ = 1;
  std::map<std::string, const ro::render::LoadedEffect*> strs_;
  std::map<std::string, ro::formats::Image> textures_;
  // La révision des surcharges du VFS à la lecture : un skin changé fait
  // relire les textures.
  std::uint64_t textures_revision_ = 0;
  std::map<std::string, ro::render::LoadedActorSprite> sprites_;
  std::vector<ro::render::DrawnEffect> drawn_;
  std::vector<ro::render::DrawnEffect> behind_;
  std::vector<ro::render::EffectWorldQuad> quads_;
  std::vector<ro::render::DrawnWorldQuads> quad_lots_;
  std::vector<ro::render::ComposedActor> composed_;
  std::vector<ro::render::DrawnActor> actors_;
  std::vector<SpriteBatch> sprite_batches_;
  std::vector<std::vector<ro::render::EffectScreenQuad>> screen_quad_lots_;
  std::vector<ro::render::DrawnScreenQuads> screen_quads_;
  std::vector<SownNamedSound> sounds_;
  std::optional<std::uint32_t> quake_until_ms_;
  std::vector<OwnerSpawn> owner_spawns_;
  // Les effets du porteur à ce tic, relus pour chaque instance.
  std::vector<ro::render::ActorEffectPlay> owner_effects_;
};

// LA NAISSANCE D'UNE CLASSE PAR L'ÉTAPE D'UNE SUITE : le porteur et la place
// que l'étape désigne, la cible pour acteur lié, et le niveau que le paquet de
// la compétence annonce — que seules les classes qui le reçoivent lisent. Rend
// la place où la classe naît, d'où part son bruit ; rien quand aucune classe
// portée ne répond à l'effet de l'étape, ou qu'elle refuse de naître.
std::optional<glm::vec3> SpawnSkillClass(EffectClassesInPlay& classes, const ro::sim::World& world,
                                         const MapStage& stage, ro::sim::WalkPlanner& planner,
                                         const ro::sim::Actor& target, std::uint32_t caster_id,
                                         std::uint32_t started_ms,
                                         const ro::render::SkillSuiteStep& step,
                                         std::uint32_t delay_ms, std::int32_t skill_level);

}  // namespace moonclient
