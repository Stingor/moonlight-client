#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/ground_quad.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/scene_fog.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "survivors/arena_card_cues.h"
#include "survivors/arena_effect_plan.h"
#include "world/map_stage.h"

namespace moonclient {

// LES EFFETS DE L'ARÈNE : sorts, impacts, montée de niveau, évolution, murs et
// piliers, tirs des monstres, annonces et aires au sol, lueur des cartes.
//
// 🔴 LES MÊMES PIÈCES QUE LA PARTIE — catalogue, troupe des nœuds, moteur
// d'effets, leurs renderers et leurs peintres —, sur un monde PRIVÉ qui ne
// contient que le héros et ses orbes, reposés à chaque image. La horde n'y
// entre jamais :
// un effet que le client pose sur une cible se pose ici au sol, où elle se
// tient. Voir `SkillBearer`.
//
// ⚠ NI DÉPLACÉE NI COPIÉE : le moteur d'effets garde l'adresse du catalogue et
// de la troupe qui vivent à côté de lui.
class ArenaEffects {
 public:
  // Le sol sous un point de la simulation, en coordonnées du monde.
  using GroundOf = std::function<glm::vec3(ro::sim::FieldPoint)>;

  // ⚠ LE CATALOGUE DES SPRITES ET LES RÈGLES DOIVENT SURVIVRE AUX EFFETS : les
  // flèches des monstres s'y habillent, les skills s'y résolvent.
  ArenaEffects(ro::vfs::Vfs& vfs, const ro::render::SpriteCatalog& sprites,
               const ro::sim::SurvivorsRules& rules);
  ArenaEffects(const ArenaEffects&) = delete;
  ArenaEffects& operator=(const ArenaEffects&) = delete;
  ArenaEffects(ArenaEffects&&) = delete;
  ArenaEffects& operator=(ArenaEffects&&) = delete;
  ~ArenaEffects() = default;

  // L'ARÈNE MONTÉE : le sol que la troupe interroge, le chemin du héros.
  //
  // ⚠ LA SCÈNE DOIT SURVIVRE AUX EFFETS, ou être rouverte : ils en gardent
  // l'adresse.
  //
  // `origin_cells` : la case de la partie où commence la carte montée, (0, 0)
  // pour une carte, le coin de la fenêtre pour le sol généré.
  void Open(const MapStage& stage, glm::ivec2 origin_cells = {});

  // LE SOL SE RECENTRE (`ArenaScene::FollowHero`) : la carte montée a été
  // remplacée en place par la fenêtre de coin `origin_cells`, et chaque point du
  // monde dessiné se déplace de `world_delta`. Les effets en cours suivent :
  // les fichiers d'effet et les instances de classe sans trace, les nœuds
  // procéduraux à leur niveau (`EffectNodePool::Shift`) ; le monde privé
  // recompte ses cases depuis le nouveau coin.
  void Shift(glm::vec3 world_delta, glm::ivec2 origin_cells);
  // Vrai quand `Shift` ne laisserait aucune trace : aucun nœud procédural n'a
  // de piste allumée, dont les points ont le sens que leur figure leur donne.
  [[nodiscard]] bool ShiftsWithoutTrace() const { return !nodes_.tracks_alive(); }

  // UNE PARTIE NEUVE sur l'arène gardée (« Rejouer ») : aucun effet, aucune
  // flamme, aucun tir, aucune annonce ni orbe de la précédente, dont les
  // instants sont lus sur une autre horloge de présentation.
  void Restart();

  // Réduits ou éteints, les effets qui durent s'éteignent sur-le-champ.
  void set_switches(ArenaEffectSwitches switches);
  [[nodiscard]] ArenaEffectSwitches switches() const noexcept { return switches_; }

  // UNE IMAGE : lance ce que les nouvelles que l'appelant vient de prendre
  // font naître, entretient murs, piliers et orbes (`SurvivorsRun::Orbs`)
  // d'après l'état, tire les flèches des monstres et pose les annonces au sol.
  // `now_ms` est l'horloge de présentation, ramenée à celle des renderers.
  void Advance(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
               std::span<const ro::sim::SurvivorsOrb> orbs, const GroundOf& ground_of,
               std::uint32_t now_ms);

  // LE HÉROS À SA PLACE DESSINÉE, en cases continues (`HordePose::cell`) : les
  // effets qu'il porte le suivent entre deux pas au lieu de sauter de case en
  // case. À chaque image, après `Advance`, qui le pose au pas de la simulation.
  void FollowHero(glm::vec2 hero_cell);

  // AVANT LES CORPS : les annonces et les aires, couchées au sol, et les
  // effets qui passent derrière les corps.
  void DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                        const ro::render::SceneFog& fog, ro::render::SpriteRenderer& sprites);
  // APRÈS LES CORPS : fait vivre les deux familles d'effets et les peint, dans
  // l'ordre de la partie.
  void DrawAfterBodies(ro::render::SpriteRenderer& sprites, const ro::render::Camera& camera,
                       float aspect_ratio, float viewport_width, float viewport_height,
                       const ro::render::SceneFog& fog, std::uint32_t now_ms);

  // Fait naître hors champ l'effet de chaque skill nommé, puis l'oublie : la
  // première image de la partie ne lit plus aucun fichier. `far_point` est un
  // point du sol que la caméra ne voit pas.
  void Warm(std::span<const std::uint16_t> skill_ids, glm::vec3 far_point,
            ro::render::SpriteRenderer& sprites, const ro::render::Camera& camera,
            float aspect_ratio, float viewport_width, float viewport_height, std::uint32_t now_ms);

  [[nodiscard]] const ArenaEffectBudget& budget() const noexcept { return budget_; }
  // Les effets que l'arène tient pour vivants : voir `kMostArenaLivingEffects`.
  // Une orbe vit tant qu'elle tourne, une lueur tant que sa carte reste.
  [[nodiscard]] std::size_t living() const noexcept {
    return lives_.size() + orb_effects_.size() + glow_actors_.size();
  }
  // Les cartes au sol qui luisent.
  [[nodiscard]] std::size_t card_glows() const noexcept { return glow_actors_.size(); }
  [[nodiscard]] std::size_t launched_total() const noexcept { return launched_total_; }
  // Les fichiers que les effets ont fait sonner à la dernière peinture, noms
  // formés : à passer par le budget de voix du mode.
  [[nodiscard]] std::vector<std::string> TakeSounds() { return std::exchange(heard_, {}); }

 private:
  // Un mur ou un pilier en train de brûler : un effet par case, relancé à la
  // fin de sa vie tant que la zone dure.
  struct ZoneFlame {
    std::uint32_t zone_serial = 0;
    ro::sim::FieldPoint at;
    std::uint32_t effect_id = 0;
    std::uint32_t relaunch_ms = 0;
  };
  // Un tir de monstre déjà parti.
  struct ShotSeen {
    std::uint32_t from_serial = 0;
    std::uint32_t expires_step = 0;
  };
  // Une annonce, couchée au sol jusqu'à son coup.
  struct WarningMark {
    ro::sim::SurvivorsWarning warning;
    std::uint32_t announced_step = 0;
  };

  void Launch(const ArenaLaunch& launch, const GroundOf& ground_of, std::uint32_t now_ms);
  void KeepZones(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                 std::uint32_t now_ms);
  void ShootFoeShots(const ro::sim::SurvivorsField& field, const GroundOf& ground_of);
  void LayGroundMarks(const ro::sim::SurvivorsField& field);
  // Allume, repose et éteint les orbes, un acteur du monde privé chacune.
  void KeepOrbs(std::span<const ro::sim::SurvivorsOrb> orbs, const GroundOf& ground_of,
                std::uint32_t now_ms);
  void ForgetOrbs();
  // Allume la lueur d'une carte ou d'un coffre tombé, et éteint celle d'un
  // butin parti : chaque lueur est portée par un acteur du monde privé, et sa
  // boucle s'éteint avec lui. Voir `PlanArenaRareGlows`.
  void KeepRareGlows(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                     std::uint32_t now_ms);
  void ForgetRareGlows();
  // Combien de temps un effet vit, pour le compte des vivants.
  [[nodiscard]] std::uint32_t LifeMsOf(std::uint32_t effect_id);
  // Pose un acteur du monde privé en cases continues, à la sous-case près.
  void PlaceActor(std::uint32_t actor_id, glm::vec2 cell);

  ro::vfs::Vfs* vfs_ = nullptr;
  const ro::render::SpriteCatalog* sprites_ = nullptr;
  ArenaSkillLooks looks_;
  ArenaEffectSwitches switches_;
  ArenaEffectBudget budget_;
  ro::render::EffectCatalog catalog_;
  EffectNodesInPlay nodes_;
  EffectsInPlay effects_;
  std::optional<ro::render::EffectRenderer> renderer_;
  std::optional<ro::render::GroundQuadRenderer> ground_;
  // Les marques au sol, à l'image blanche que leur couleur teinte.
  std::optional<ro::render::GroundQuadRenderer> marks_;
  ro::formats::Image white_;
  ro::sim::World world_;
  const MapStage* stage_ = nullptr;
  glm::ivec2 origin_cells_{0};
  std::optional<ro::sim::WalkPlanner> planner_;
  std::vector<ArenaLaunch> plan_;
  std::vector<ArenaOrbLook> orbs_;
  // L'effet que porte l'acteur de chaque orbe allumée, dans l'ordre des orbes.
  std::vector<std::uint32_t> orb_effects_;
  std::vector<ArenaRareGlow> glow_plan_;
  // L'acteur de chaque lueur allumée.
  std::vector<std::uint32_t> glow_actors_;
  std::vector<ZoneFlame> flames_;
  std::vector<ShotSeen> shots_;
  std::vector<WarningMark> warnings_;
  std::vector<ro::render::GroundQuad> mark_quads_;
  // L'instant où s'éteint chaque effet lancé.
  ArenaEffectLives lives_;
  std::size_t launched_total_ = 0;
  std::uint32_t step_ = 0;
  std::vector<std::string> heard_;
};

}  // namespace moonclient
