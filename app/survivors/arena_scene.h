#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/camera.h"
#include "ro/render/point_lights.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/vfs/vfs.h"

#include "options/graphics_options.h"
#include "survivors/arena_effects.h"
#include "survivors/arena_ground_follow.h"
#include "survivors/arena_ground_plan.h"
#include "survivors/arena_ground_window.h"
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

// UN RECENTRAGE FAIT : la fenêtre passée de `from` à `to`, ce que devient un
// point du monde dessiné (`GroundWorldShift`), et ce que son montage a coûté à
// l'image, en millisecondes — le sol et le décor s'étaient téléversés pendant
// les images d'avant (`LitStage::Prepare`).
struct GroundRecentre {
  GroundWindowOrigin from;
  GroundWindowOrigin to;
  glm::vec3 world_delta{0.0F, 0.0F, 0.0F};
  double mount_ms = 0.0;
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
// fait partie du jeu. Avec elle viennent la lanterne du héros et la lueur des
// objets au sol (`arena_night_lights.h`), qui la gardent lisible.
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

  // LA CARTE OU LE SOL GÉNÉRÉ : quand `arena` nomme un terrain de `rules`, les
  // ressources de son biome se lisent une fois et la fenêtre de 160 × 160 cases
  // se bâtit autour du départ, sous la graine `run_seed` (ADR 0134) ; sinon la
  // carte de `arena`, comme ci-dessus.
  //
  // ⚠ ÉCHOUE EN LE DISANT si le terrain n'a pas d'aspect dans le catalogue
  // d'app, ou si l'installation n'a pas sa carte de référence.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs, const ro::sim::SurvivorsRules& rules,
                                            const ro::sim::ArenaRule& arena, std::uint64_t run_seed,
                                            const MapStageRead& read = {});

  // LE SOL GÉNÉRÉ DE `sources`, sa première fenêtre en `origin`, nommée
  // `name` : la voie ci-dessus une fois l'aspect trouvé et ses ressources lues.
  // `thread` dit où se bâtissent les fenêtres suivantes.
  [[nodiscard]] ro::core::Result<void> OpenGround(
      ro::vfs::Vfs& vfs, GroundSources sources, GroundWindowOrigin origin, std::string name,
      const MapStageRead& read = {}, GroundBuildThread thread = GroundBuildThread::kWorker);

  // LE SOL SUIT LE HÉROS (`arena_ground_follow.h`) : à chaque image, avant
  // `Begin`, il demande la fenêtre qu'il faut au fil de travail, la prépare
  // tranche par tranche, et la monte. Rend le recentrage fait à cette image —
  // l'appelant déplace alors de `world_delta` tout ce qu'il tient en
  // coordonnées du monde, dans la même image —, ou rien ; rien toujours sur une
  // carte. `shifts_without_trace` : les effets en cours se déplacent sans trace
  // (`ArenaEffects`).
  [[nodiscard]] std::optional<GroundRecentre> FollowHero(glm::vec2 hero_cell,
                                                         bool shifts_without_trace);
  // UN RECENTRAGE IMMÉDIAT sur `origin`, la fenêtre bâtie sur ce fil : pour les
  // essais et le banc. Échoue sur une carte.
  [[nodiscard]] ro::core::Result<GroundRecentre> Recentre(GroundWindowOrigin origin);

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

  // Le sol sous un point de la simulation, en coordonnées du monde dessiné :
  // là où se pose un corps et où la caméra vise. Sur le sol généré, la case de
  // la partie se compte depuis le coin de la fenêtre.
  [[nodiscard]] glm::vec3 GroundAt(ro::sim::FieldPoint point) const;
  // Le sol sous une position en cases continues : voir `HordePose::cell`.
  [[nodiscard]] glm::vec3 GroundAtCell(glm::vec2 cell) const;

  // L'OPTION U27 : la horde entre dans l'ombre du soleil. Fausse par défaut.
  void set_horde_sun_shadows(bool shown) noexcept { horde_sun_shadows_ = shown; }
  // Les ombres de l'image ouverte par `Begin`, à passer à `HordeComposer`.
  [[nodiscard]] HordeShadows ShadowsOf(const LitFrame& frame) const noexcept {
    return HordeShadows{.sun_shadows = frame.sun_shadows, .horde_sun_shadows = horde_sun_shadows_};
  }

  // Vrai pendant qu'une fenêtre se prépare (`LitStage::Prepare`).
  [[nodiscard]] bool preparing() const noexcept { return lit_.preparing(); }

  // LE COIN SUD-OUEST DE LA FENÊTRE DESSINÉE, en cases de la partie : (0, 0)
  // sur une carte.
  [[nodiscard]] GroundWindowOrigin window_origin() const noexcept { return origin_; }
  // Les ressources du sol généré, ou nullptr sur une carte.
  [[nodiscard]] const GroundSources* ground_sources() const noexcept { return ground_.get(); }

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

  // Monte la fenêtre préparée, de coin `prepared_origin_`, à la place de la
  // montrée.
  [[nodiscard]] ro::core::Result<GroundRecentre> MountPrepared();

  LitStage lit_;
  // Le sol généré : ses ressources, lues une fois, et la fenêtre montée.
  std::unique_ptr<GroundSources> ground_;
  GroundWindowOrigin origin_;
  // Le coin de la fenêtre que `LitStage` prépare, quand il en prépare une.
  GroundWindowOrigin prepared_origin_;
  ro::vfs::Vfs* vfs_ = nullptr;
  // Le maillage du sol des fenêtres à bâtir, celui de la dernière image ; lu
  // par le fil de travail.
  std::atomic<std::int32_t> ground_subdivisions_{ro::render::kFlatGroundSubdivisions};
  // ⚠ APRÈS `ground_` : ses fils lisent les ressources, et le suivi doit être
  // détruit — ses fils finis — avant elles.
  std::unique_ptr<GroundFollow<MapStage>> follow_;
  bool horde_sun_shadows_ = false;
  // Les effets de l'image en cours de peinture, le temps de `Paint`.
  ArenaEffects* painting_effects_ = nullptr;
  // LA LANTERNE ET LA LUEUR DES OBJETS de l'image, refaites à chaque `Paint`
  // dans des listes réutilisées : voir `ArenaLantern` et `ArenaItemHalos`.
  std::vector<ro::render::PointLight> carried_lights_;
  std::vector<ro::render::DrawnActor> item_halos_;
};

}  // namespace moonclient
