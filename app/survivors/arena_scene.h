#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/camera.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/vfs/vfs.h"

#include "options/graphics_options.h"
#include "survivors/arena_effects.h"
#include "survivors/arena_numbers.h"
#include "survivors/horde_composer.h"
#include "world/lit_stage.h"
#include "world/map_stage.h"

namespace moonclient {

// LES CALQUES DE L'ARÈNE : ses effets et ses chiffres, que l'appelant possède.
// Nuls, rien ne s'y peint.
struct ArenaOverlays {
  ArenaEffects* effects = nullptr;
  const ArenaNumbers* numbers = nullptr;
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
};

// LA SCÈNE DE L'ARÈNE : la vraie carte de l'arène (U8), sœur de l'écran de
// connexion, peinte par la séquence de `LitStage`. Elle possède sa carte, ses
// renderers, sa carte d'ombre, ses lumières et son vide ; la carte du
// personnage reste dans les renderers de la partie, qui n'y sont pour rien.
//
// 🔴 ELLE SUIT LA PAGE « GRAPHICS » DU JOUEUR, SAUF L'HEURE. Ombres, lumières,
// sol arrondi, filtrage du décor et brouillard sont ceux qu'il a réglés : la
// caméra de l'arène est celle du jeu, et le brouillard que la table des cartes
// règle pour elle s'y montre comme en partie. L'heure, elle, est celle de la
// partie (`ArenaSkyHour`), cycle jour/nuit coché ou non : la nuit qui tombe
// fait partie du jeu.
//
// 🔴 L'OMBRE DU SOLEIL SUIT AUSSI « GRAPHICS » : éteinte, personne n'en porte ;
// allumée, le héros en porte une, et l'option U27 (`horde_sun_shadows`) y
// ajoute la horde. Sans l'option, la horde garde son ombre ronde.
//
// ⚠ ELLE NE COMPOSE PERSONNE : l'appelant compose la horde (`HordeComposer`)
// entre `Begin` et `Paint`, et fait vivre ses effets et ses chiffres
// (`ArenaOverlays`), qu'elle peint à leur place dans la séquence.
class ArenaScene : private LitLayers {
 public:
  ArenaScene() = default;

  ArenaScene(const ArenaScene&) = delete;
  ArenaScene& operator=(const ArenaScene&) = delete;
  ArenaScene(ArenaScene&&) = delete;
  ArenaScene& operator=(ArenaScene&&) = delete;
  ~ArenaScene() override = default;

  // Charge la carte de `arena` et téléverse sa géométrie.
  //
  // ⚠ ÉCHOUE EN LE DISANT si la règle ne nomme aucune carte ou si
  // l'installation ne l'a pas : une arène noire ne se distingue pas d'un
  // plantage.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs, const ro::sim::ArenaRule& arena,
                                            const MapStageRead& read = {});

  // OUVRE UNE IMAGE à `presentation_ms` de l'horloge de présentation. Voir
  // `LitStage::Begin` : les corps se composent après, leur ombre ronde dépend
  // de `LitFrame::sun_shadows`.
  [[nodiscard]] LitFrame Begin(const GraphicsOptions& graphics, std::uint64_t presentation_ms);

  // ÉCLAIRE ET PEINT L'IMAGE dans la cible déjà liée, au ciel de la partie à
  // `presentation_ms`, avec les corps que `horde` a composés pour elle, puis
  // les effets par-dessus les corps et les chiffres par-dessus tout.
  // `now_ms`, le temps d'écran, anime les lumières de la carte.
  void Paint(LitFrame& frame, const ro::render::Camera& camera, float aspect_ratio,
             const GraphicsOptions& graphics, std::uint64_t presentation_ms, std::uint32_t now_ms,
             const HordeComposer& horde, const ArenaOverlays& overlays = {});

  // LA CHAUFFE : peint hors champ, autour de `hero_cell`, une image de chaque
  // espèce de `species` et l'effet de chaque skill de `skill_ids`, derrière
  // l'écran de chargement. La première image de la partie ne lit plus rien.
  void Warm(HordeComposer& horde, ArenaEffects& effects, std::span<const std::uint16_t> species,
            std::span<const std::uint16_t> skill_ids, glm::vec2 hero_cell,
            const ro::render::Camera& camera, float aspect_ratio, float viewport_width,
            float viewport_height, std::uint32_t now_ms);

  // Le sol sous un point de la simulation, en coordonnées du monde : là où se
  // pose un corps et où la caméra vise.
  [[nodiscard]] glm::vec3 GroundAt(ro::sim::FieldPoint point) const;
  // Le sol sous une position en cases continues : voir `HordePose::cell`.
  [[nodiscard]] glm::vec3 GroundAtCell(glm::vec2 cell) const;

  // L'OPTION U27 : la horde entre dans l'ombre du soleil. Fausse par défaut.
  void set_horde_sun_shadows(bool shown) noexcept { horde_sun_shadows_ = shown; }
  // Les ombres de l'image ouverte par `Begin`, à passer à `HordeComposer`.
  [[nodiscard]] HordeShadows ShadowsOf(const LitFrame& frame) const noexcept {
    return HordeShadows{.sun_shadows = frame.sun_shadows, .horde_sun_shadows = horde_sun_shadows_};
  }

  [[nodiscard]] bool ready() const noexcept { return lit_.ready(); }
  // ⚠ SEULEMENT QUAND `ready()`.
  [[nodiscard]] const MapStage& stage() const noexcept { return lit_.stage(); }
  [[nodiscard]] std::string_view map_name() const noexcept { return lit_.map_name(); }
  [[nodiscard]] ro::render::SpriteRenderer& sprites() noexcept { return lit_.sprites(); }
  [[nodiscard]] const std::array<float, 4>& background() const noexcept {
    return lit_.background();
  }

 private:
  void DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                        const LitFrame& frame) override;
  void DrawOverWater(const ro::render::Camera& camera, float aspect_ratio,
                     const LitFrame& frame) override;

  LitStage lit_;
  bool horde_sun_shadows_ = false;
  // Les effets de l'image en cours de peinture, le temps de `Paint`.
  ArenaEffects* painting_effects_ = nullptr;
};

}  // namespace moonclient
