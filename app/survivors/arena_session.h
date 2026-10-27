#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/camera.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "options/graphics_options.h"
#include "survivors/arena_camera.h"
#include "survivors/arena_effects.h"
#include "survivors/arena_numbers.h"
#include "survivors/arena_scene.h"
#include "survivors/horde_composer.h"
#include "survivors/horde_poses.h"

namespace moonclient {

// L'ARÈNE MONTÉE POUR UNE PARTIE : la scène, ses corps, ses effets, ses
// chiffres et sa caméra, tenus ensemble comme le banc `survivors render` les
// tient. app la monte derrière l'écran de chargement, la fait vivre à chaque
// image, et la garde pour « Rejouer ».
//
// ⚠ NI DÉPLACÉE NI COPIÉE : les effets gardent l'adresse de la scène, et le
// compositeur celle de la garde-robe, qui doit lui survivre.

// LA HORDE RESTANTE S'EFFACE pendant ce temps après la victoire (Q7).
inline constexpr std::uint32_t kVictoryFadeMs = 1000;

// CE QU'UNE IMAGE DE L'ARÈNE LIT de la partie et de l'écran.
struct ArenaFrameFacts {
  const ro::sim::SurvivorsField* field = nullptr;
  std::span<const ro::sim::SurvivorsOrb> orbs;
  // Les nouvelles de cette image seulement.
  const ro::sim::SurvivorsNews* news = nullptr;
  std::uint64_t presentation_ms = 0;
  // La part du pas suivant déjà écoulée : voir `SurvivorsPlay::blend_per_mille`.
  std::uint32_t blend_per_mille = 0;
  // Le temps d'arrivée de l'étage, que la descente de la caméra suit.
  std::uint32_t arrival_ms = 0;
  // Le temps d'écran depuis la victoire ; rien hors du bandeau « Victoire ! ».
  std::optional<std::uint64_t> victory_ms;
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
};

class ArenaSession {
 public:
  // ⚠ LA GARDE-ROBE, LE VFS ET LES RÈGLES DOIVENT SURVIVRE À L'ARÈNE.
  ArenaSession(ro::vfs::Vfs& vfs, ro::render::ActorWardrobe& wardrobe,
               const ro::sim::SurvivorsRules& rules);
  ArenaSession(const ArenaSession&) = delete;
  ArenaSession& operator=(const ArenaSession&) = delete;
  ArenaSession(ArenaSession&&) = delete;
  ArenaSession& operator=(ArenaSession&&) = delete;
  ~ArenaSession() = default;

  // Charge la carte de l'arène. Les chiffres qui manquent se disent au journal
  // et la partie se joue sans eux ; la carte qui manque refuse la partie.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs, const ro::sim::ArenaRule& arena);
  // Le sol praticable de l'arène, pour la simulation.
  [[nodiscard]] ro::core::Result<ro::sim::WalkMap> WalkMapOf() const;

  // Une partie neuve : rien de vu, aucun effet ni chiffre de la précédente,
  // la caméra repart du héros.
  void Restart();
  // L'apparence du héros pour cette image : voir `HeroLook`.
  void SetHeroLook(const ro::render::ActorLook& look) { composer_.SetHeroLook(look); }
  void SetSwitches(ArenaEffectSwitches switches) { effects_.set_switches(switches); }
  void SetHordeSunShadows(bool shown) { scene_.set_horde_sun_shadows(shown); }
  void Wheel(float notches) noexcept { camera_.Wheel(notches); }
  void SetQuake(bool enabled) noexcept { camera_.set_quake_enabled(enabled); }

  // LA CHAUFFE derrière l'écran de chargement : voir `ArenaScene::Warm`.
  void Warm(std::span<const std::uint16_t> species, std::span<const std::uint16_t> skill_ids,
            glm::vec2 hero_cell, float viewport_width, float viewport_height, std::uint32_t now_ms);

  // FAIT VIVRE L'IMAGE : les morts, les poses, les corps, les effets et les
  // chiffres, puis la caméra. Rien n'est peint.
  void Compose(const ArenaFrameFacts& facts, const GraphicsOptions& graphics, std::uint32_t now_ms);
  // PEINT L'IMAGE COMPOSÉE dans la cible liée.
  void Paint(const ArenaFrameFacts& facts, const GraphicsOptions& graphics, std::uint32_t now_ms);

  // Un point du monde sur l'écran, en points depuis le haut à gauche ; rien
  // derrière la caméra.
  [[nodiscard]] std::optional<glm::vec2> ScreenOf(glm::vec3 world, float viewport_width,
                                                  float viewport_height) const;
  // LE POINT DU SOL VISÉ PAR LA SOURIS, en sous-unités de la simulation, sur
  // le sol plat du héros : seul le cap compte, et la caméra est figée au nord.
  [[nodiscard]] std::optional<ro::sim::FieldPoint> FieldPointUnder(glm::vec2 screen,
                                                                   ro::sim::FieldPoint hero,
                                                                   float viewport_width,
                                                                   float viewport_height) const;
  // Le sol sous un point de la simulation, ou sous une position en cases
  // continues : voir `ArenaScene::GroundAt`.
  [[nodiscard]] glm::vec3 GroundAt(ro::sim::FieldPoint point) const {
    return scene_.GroundAt(point);
  }
  [[nodiscard]] glm::vec3 GroundAtCell(glm::vec2 cell) const { return scene_.GroundAtCell(cell); }
  // Les pieds du héros dessiné, en cases continues ; rien avant la première pose.
  [[nodiscard]] std::optional<glm::vec2> hero_cell() const noexcept;

  [[nodiscard]] ArenaScene& scene() noexcept { return scene_; }
  [[nodiscard]] const ArenaEffects& effects() const noexcept { return effects_; }
  // Les bruits des effets de la dernière peinture : voir `ArenaEffects::TakeSounds`.
  [[nodiscard]] std::vector<std::string> TakeEffectSounds() { return effects_.TakeSounds(); }
  [[nodiscard]] const HordeComposer& composer() const noexcept { return composer_; }
  [[nodiscard]] const ro::render::Camera& camera() const noexcept { return camera_.camera(); }
  // Ce qu'ont coûté les effets à la dernière composition, en millisecondes.
  [[nodiscard]] double effects_ms() const noexcept { return effects_ms_; }
  // Les recentrages du sol généré depuis l'ouverture, et le plus cher de leurs
  // montages, en millisecondes.
  [[nodiscard]] std::size_t recentres() const noexcept { return recentres_; }
  [[nodiscard]] double most_mount_ms() const noexcept { return most_mount_ms_; }

 private:
  [[nodiscard]] float AspectOf(float width, float height) const noexcept;
  // LE SOL SUIT LE HÉROS DESSINÉ, et la session avec lui : voir
  // `ArenaScene::FollowHero`.
  void FollowGround();

  ArenaScene scene_;
  HordeComposer composer_;
  ArenaEffects effects_;
  ArenaNumbers numbers_;
  HordePoses poses_;
  ArenaCamera camera_;
  // Les poses de l'image, retouchées pour l'effacement de la victoire.
  std::vector<HordePose> faded_;
  LitFrame frame_;
  // Le monde sans personne que la caméra interroge pour la secousse.
  ro::sim::World nobody_;
  double effects_ms_ = 0.0;
  std::size_t recentres_ = 0;
  double most_mount_ms_ = 0.0;
};

}  // namespace moonclient
