#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/camera.h"
#include "ro/render/map_sky.h"
#include "ro/render/model_renderer.h"
#include "ro/render/point_light_stage.h"
#include "ro/render/point_lights.h"
#include "ro/render/scene_fog.h"
#include "ro/render/sky_clock.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/sun_shadow_map.h"
#include "ro/render/sun_shadow_pass.h"
#include "ro/render/terrain_renderer.h"
#include "ro/render/void_shade.h"
#include "ro/render/water_renderer.h"
#include "ro/vfs/vfs.h"

#include "options/graphics_options.h"
#include "world/map_stage.h"

namespace moonclient {

// LES CORPS D'UNE IMAGE, tels que la scène qui compose les a posés.
//
// ⚠ DES VUES : elles doivent vivre jusqu'à la fin de `LitStage::Paint`.
struct LitActors {
  // Les corps, qui seuls écrivent leur profondeur et portent ombre.
  std::span<const ro::render::DrawnActor> bodies;
  // Les halos, peints sur l'eau mais derrière tout corps.
  std::span<const ro::render::DrawnActor> halos;
  // Les voiles, peints après les halos.
  std::span<const ro::render::DrawnActor> veils;
  // LES CORPS QUI PORTENT L'OMBRE DU SOLEIL, pris parmi `bodies` ; rien : tous.
  // Ils se peignent avec les autres, dans le même tri de l'arrière vers
  // l'avant : seule la passe d'ombre les distingue.
  std::optional<std::span<const ro::render::DrawnActor>> sun_casters;
  // LES LUMIÈRES QUE LES CORPS PORTENT, ajoutées à celles de la carte : au
  // plus `ro::render::kMaxActorLights`, les plus proches de ce que vise la
  // caméra.
  std::span<const ro::render::PointLight> carried_lights;
};

// L'IMAGE EN COURS : ce que la page « Graphics » en a décidé à son ouverture,
// puis sa lumière une fois le ciel lu et les ombres peintes.
struct LitFrame {
  ro::render::SceneFog fog;
  bool fog_shown = false;
  ro::render::TerrainStages stages;
  // L'ombre du soleil est demandée, et la carte d'ombre n'a pas été refusée :
  // les corps composés pour cette image perdent leur ombre ronde.
  bool sun_shadows = false;
  ro::render::SkyLight sky;
  ro::render::SunShadowView sun;
  ro::render::PointLightSet points;
  // Ce que l'heure retire aux corps : rien sans astre, comme en partie.
  glm::vec3 body_daylight{1.0F};
};

// CE QUE LA SCÈNE QUI COMPOSE GLISSE ENTRE LES PASSES DE LA SCÈNE ÉCLAIRÉE :
// ses effets, à leur place dans la séquence.
class LitLayers {
 public:
  LitLayers() = default;
  LitLayers(const LitLayers&) = delete;
  LitLayers& operator=(const LitLayers&) = delete;
  LitLayers(LitLayers&&) = delete;
  LitLayers& operator=(LitLayers&&) = delete;
  virtual ~LitLayers() = default;

  // Après le sol et le décor opaque, avant les corps : ce qui passe derrière
  // eux.
  virtual void DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                                const LitFrame& frame) = 0;
  // Sur l'eau, avant les halos : les effets de la carte.
  virtual void DrawOverWater(const ro::render::Camera& camera, float aspect_ratio,
                             const LitFrame& frame) = 0;
};

// LA SCÈNE ÉCLAIRÉE : une carte, ses renderers, son ciel, ses ombres et ses
// lumières, et la séquence qui les peint.
//
// 🔴 UNE SEULE SÉQUENCE POUR TOUTES LES SCÈNES QUI MONTRENT UNE CARTE HORS DE
// LA PARTIE : sol, ombres, décor, effets derrière, corps, eau, effets de carte,
// halos, voiles, vide, translucides. Deux copies de cet ordre finiraient par ne
// plus peindre la même chose, et c'est la moins regardée qui se tromperait.
//
// ⚠ ELLE NE COMPOSE PERSONNE ET NE PEINT AUCUN EFFET : la scène qui l'emploie
// pose ses corps (`LitActors`) et glisse ses effets (`LitLayers`). La séquence,
// elle, ne se reconstruit pas ailleurs.
//
// ⚠ ELLE SUIT LA PAGE « GRAPHICS » À CHAQUE IMAGE : ombres et lumière de la
// carte, lumière froide, filtrage du décor, ombres temps réel du soleil et des
// lumières ponctuelles, cycle jour/nuit. Le brouillard est le choix de
// l'appelant.
class LitStage {
 public:
  LitStage() = default;

  LitStage(const LitStage&) = delete;
  LitStage& operator=(const LitStage&) = delete;
  LitStage(LitStage&&) = delete;
  LitStage& operator=(LitStage&&) = delete;
  ~LitStage() = default;

  // Charge la carte et téléverse sa géométrie ; lit son ciel par les mêmes
  // tables que la partie. Voir `MapStageRead` pour ce que `read` demande.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs, std::string_view map_name,
                                            const MapStageRead& read = {});

  // MONTE UNE CARTE DÉJÀ ASSEMBLÉE (`AssembleMapStage`) et téléverse sa
  // géométrie, sans document d'édition. Son ciel — intérieur ou plein air — se
  // lit sous le nom `sky_map_name`, celui de la carte dont elle prend la lumière.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs, MapStage stage,
                                            std::string_view sky_map_name,
                                            ro::core::LoadProgress* progress = nullptr);

  // OUVRE UNE IMAGE : le sol arrondi selon le réglage, le brouillard, les
  // étages du terrain, l'ombre du soleil demandée, et le décor animé posé à
  // `elapsed_ms`.
  //
  // ⚠ AVANT LA COMPOSITION DES CORPS : leur ombre ronde dépend de
  // `LitFrame::sun_shadows`, et ils doivent être posés avant `Paint`, qui peint
  // leur silhouette dans l'ombre du soleil.
  [[nodiscard]] LitFrame Begin(const GraphicsOptions& graphics, bool fog_shown,
                               std::uint32_t elapsed_ms);

  // ÉCLAIRE ET PEINT L'IMAGE ouverte par `Begin`, dans la cible déjà liée.
  //
  // `sky_hour`, dans [0, 24), n'est lue que si le joueur a le cycle jour/nuit
  // ou que l'heure est imposée, et que la carte est en plein air ; rien garde
  // la lumière du RSW.
  // `elapsed_ms` anime l'eau ; `now_ms` les lumières de la carte.
  //
  // Au retour, `frame` porte la lumière de l'image : les effets qui se peignent
  // après la séquence la lisent.
  void Paint(LitFrame& frame, const ro::render::Camera& camera, float aspect_ratio,
             const GraphicsOptions& graphics, std::optional<float> sky_hour,
             std::uint32_t elapsed_ms, std::uint32_t now_ms, const LitActors& actors,
             LitLayers& layers);

  // L'HEURE IMPOSÉE : vrai, `Paint` lit `sky_hour` même quand le joueur n'a pas
  // le cycle jour/nuit. C'est l'heure d'une mise en scène, pas celle du
  // serveur. Faux par défaut.
  void set_sky_hour_imposed(bool imposed) noexcept { sky_hour_imposed_ = imposed; }

  [[nodiscard]] bool ready() const noexcept { return stage_.has_value(); }
  // ⚠ SEULEMENT QUAND `ready()`.
  [[nodiscard]] MapStage& stage() noexcept { return *stage_; }
  [[nodiscard]] const MapStage& stage() const noexcept { return *stage_; }
  // Le nom de la carte montée, ou vide.
  [[nodiscard]] std::string_view map_name() const noexcept;
  // Les sprites de la scène : corps, halos, voiles, et ce que ses effets y
  // posent.
  [[nodiscard]] ro::render::SpriteRenderer& sprites() noexcept { return sprites_; }
  // LA COULEUR DE FOND DE LA CARTE MONTÉE, ou celle d'un écran sans carte.
  [[nodiscard]] const std::array<float, 4>& background() const noexcept { return background_; }
  // Intérieur ou carte fermée : ni astre ni heure.
  [[nodiscard]] bool closed_to_sky() const noexcept { return closed_to_sky_; }
  // Le document d'édition de la carte et celui des lumières de modèle, copiés
  // au montage ; vides sans magasin.
  [[nodiscard]] const ro::mapedit::MapEdit& document() const noexcept { return document_; }
  [[nodiscard]] const ro::mapedit::MapEdit& model_book() const noexcept { return model_book_; }

 private:
  // Pose `stage`, crée les renderers et téléverse ; le ciel se lit sous
  // `sky_map_name`.
  [[nodiscard]] ro::core::Result<void> Mount(ro::vfs::Vfs& vfs, MapStage stage,
                                             std::string_view sky_map_name,
                                             ro::core::LoadProgress* progress);
  // LE CIEL DE L'IMAGE : la lumière du RSW à l'heure `sky_hour` quand le joueur
  // a le cycle, ou que l'heure est imposée, et que la carte est en plein air ;
  // celle du RSW sinon.
  [[nodiscard]] ro::render::SkyLight SkyOf(const GraphicsOptions& graphics,
                                           std::optional<float> sky_hour) const;
  // L'OMBRE DE L'ASTRE, vide quand elle n'est pas demandée ou que la carte
  // graphique a refusé sa carte d'ombre.
  [[nodiscard]] ro::render::SunShadowView DrawSunShadow(
      bool asked, const ro::render::Camera& camera, float aspect_ratio,
      const ro::render::SkyLight& sky, std::span<const ro::render::DrawnActor> bodies);
  // LES LUMIÈRES DE LA CARTE, puis celles que portent les corps, leurs ombres
  // peintes quand le joueur a les ombres temps réel ; vide si la carte
  // graphique a refusé leur étage.
  [[nodiscard]] ro::render::PointLightSet DrawPointLights(
      const GraphicsOptions& graphics, const ro::render::Camera& camera, float aspect_ratio,
      const ro::render::SkyLight& sky, std::uint32_t now_ms, const LitActors& actors);
  // Le vide sous la carte prend l'heure du ciel, et l'astre s'y montre.
  void ShadeVoid(const ro::render::SkyLight& sky, const ro::render::Camera& camera,
                 float aspect_ratio);
  // Ce qui porte ombre : le sol, le décor et les corps.
  [[nodiscard]] ro::render::SunShadowCasters ShadowCasters(
      std::span<const ro::render::DrawnActor> bodies);

  std::optional<MapStage> stage_;
  ro::render::TerrainRenderer terrain_;
  // LA NAPPE D'EAU de la carte : une île en a, et la scène la montre comme la
  // partie.
  ro::render::WaterRenderer water_;
  // Les fontaines et les moulins tournent comme en partie. Refaite à chaque
  // montage : elle part de la première image de la carte.
  ro::render::ModelClock model_clock_;
  ro::render::ModelRenderer models_;
  ro::render::SpriteRenderer sprites_;
  // ⚠ POSÉE DÈS LA CONSTRUCTION ET NON À VIDE : un écran sans carte l'emploie
  // pour effacer, et quatre zéros y donneraient un noir transparent.
  std::array<float, 4> background_ = ro::render::kUnlistedMapClearColor;
  bool closed_to_sky_ = false;
  bool sky_hour_imposed_ = false;
  // L'OMBRE DU SOLEIL, LES LUMIÈRES PONCTUELLES ET LE VIDE À L'HEURE, créés à
  // leur première demande. Un refus de la carte graphique est retenu : il ne se
  // retente pas à chaque image, et la carte reste debout sans eux.
  ro::render::SunShadowMap sun_map_;
  bool sun_map_refused_ = false;
  ro::render::PointLightStage point_stage_;
  bool point_stage_refused_ = false;
  ro::render::VoidShade void_shade_;
  bool void_shade_refused_ = false;
  // LE DOCUMENT D'ÉDITION DE LA CARTE et celui des lumières de modèle : leurs
  // lumières s'allument et leurs effets se lèvent comme en partie.
  ro::mapedit::MapEdit document_;
  ro::mapedit::MapEdit model_book_;
};

}  // namespace moonclient
