#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/sprite_frame.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"

// LES POSES DE LA HORDE : de l'état de la simulation et de l'horloge de
// présentation, ce que chaque corps montre à cette image — motion, instant de
// la motion, direction, position, teinte, échelle. Rien n'y touche la carte
// graphique : `HordeComposer` habille ensuite ces poses.
namespace moonclient {

// Ce qu'un corps est pour la simulation.
enum class HordeBodyKind : std::uint8_t { kHero, kFoe, kFalling, kPlant, kGem, kPickup };

// LA POSE D'UN CORPS à cette image.
struct HordePose {
  HordeBodyKind kind = HordeBodyKind::kFoe;
  // Le serial de la simulation (monstre, mort, plante, objet), le rang d'une
  // gemme, zéro pour le héros.
  std::uint32_t sim_id = 0;
  // La classe (monstre, mort, plante) ou l'objet (gemme, objet au sol) ; zéro
  // pour le héros, dont l'apparence est celle du personnage.
  std::uint16_t look_id = 0;
  ro::render::Motion motion = ro::render::Motion::kIdle;
  // Depuis le début de la motion, à l'horloge de présentation.
  float motion_ms = 0.0F;
  std::size_t direction = 0;
  // La position interpolée, en cases continues : le centre de la case (x, y)
  // est en (x, y).
  glm::vec2 cell{0.0F};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  float scale = 1.0F;
  [[nodiscard]] bool operator==(const HordePose&) const = default;
};

// LES DURÉES DES MOTIONS d'une apparence, en millisecondes : un tour de
// marche, et une attaque, une blessure, une mort jouées une fois. Zéro pour
// une motion que son `.act` ne porte pas.
struct HordeMotionTimes {
  float walk_ms = 0.0F;
  float attack_ms = 0.0F;
  float hurt_ms = 0.0F;
  float die_ms = 0.0F;
};
// Les durées de l'apparence d'un corps ; `HordeComposer::TimesOf` les lit.
using HordeTimesOf = std::function<HordeMotionTimes(HordeBodyKind kind, std::uint16_t look_id)>;

// LA MARCHE D'UNE ESPÈCE A QUATRE PHASES, décalées par serial : les Porings ne
// sautent pas ensemble, et une espèce n'use que quatre images de marche par
// direction, ce qui allonge les lots de dessin.
inline constexpr std::uint32_t kHordeWalkPhaseCount = 4;
// L'instant de marche du corps `sim_id` à `presentation_ms`.
[[nodiscard]] float WalkingMs(std::uint64_t presentation_ms, float walk_ms,
                              std::uint32_t sim_id) noexcept;

// Une blessure se rejoue au plus une fois par cette durée.
inline constexpr std::uint32_t kHurtReplayMs = 300;
// Un mort, sa chute jouée, s'efface pendant cette durée.
inline constexpr std::uint32_t kFallFadeMs = 1000;
// Au plus autant de morts s'effacent à la fois ; le plus ancien cède.
inline constexpr std::size_t kMostFallingBodies = 128;
// L'élite paraît grandie, et dorée.
inline constexpr float kEliteScale = 1.5F;
inline constexpr std::array<std::uint8_t, 4> kEliteTint = {255, 210, 90, 255};
// Glacé, un monstre pâlit vers le bleu ; gelé, il prend le bleu du gel du jeu.
inline constexpr std::array<std::uint8_t, 4> kChilledTint = {160, 200, 255, 255};

// La position d'un point de la simulation, en cases continues.
[[nodiscard]] glm::vec2 CellOfField(glm::vec2 subunits) noexcept;
// Le cap de la simulation (0 au nord, sens trigonométrique, 64 par tour) en
// angle de face du client (0 au sud, 90 à l'est).
[[nodiscard]] float ClientDegreesOfHeading(std::uint8_t heading) noexcept;

// LES POSES D'UNE PARTIE, image après image.
//
// 🔴 ELLE SE SOUVIENT D'UNE IMAGE À L'AUTRE, et c'est pourquoi elle est un
// objet : la blessure bridée et les morts qui s'effacent ne se lisent pas dans
// l'état d'un pas. Les morts sortent de la simulation au pas de leur chute ;
// leur effacement vit ici, d'après les nouvelles `falls`.
class HordePoses {
 public:
  // LES MORTS DES DERNIERS PAS commencent leur chute à `presentation_ms`.
  //
  // ⚠ AVANT `Pose` : leur direction et leur rang d'élite se lisent dans le
  // souvenir de l'image précédente, que `Pose` remplace.
  void NoticeFalls(std::span<const ro::sim::SurvivorsFall> falls, std::uint64_t presentation_ms);

  // POSE TOUS LES CORPS à `presentation_ms` : le héros d'abord, puis les
  // monstres, les morts, les plantes, les objets et les gemmes. Les positions
  // s'interpolent entre `previous_position` et `position` selon
  // `blend_per_mille`, la part du pas suivant déjà écoulée (voir
  // `SurvivorsPlay::blend_per_mille`) ; un monstre caché n'a pas de pose.
  void Pose(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
            std::uint32_t blend_per_mille, const HordeTimesOf& times_of);

  [[nodiscard]] std::span<const HordePose> poses() const noexcept { return poses_; }
  // Les morts en train de s'effacer.
  [[nodiscard]] std::size_t falling() const noexcept { return falling_.size(); }

  // Une partie neuve n'a rien vu.
  void Reset();

 private:
  // Ce qu'une image retient d'un monstre vivant, par serial croissant.
  struct FoeMemory {
    std::uint32_t serial = 0;
    std::uint32_t seen_hurt_step = 0;
    // L'instant où sa blessure en cours a commencé ; aucune au départ.
    std::uint64_t hurt_started_ms = 0;
    bool hurting = false;
    std::uint8_t facing = 0;
    bool elite = false;
  };
  // Un mort qui joue sa chute puis s'efface.
  struct FallingBody {
    std::uint32_t serial = 0;
    std::uint16_t foe_id = 0;
    glm::vec2 cell{0.0F};
    std::size_t direction = 0;
    bool elite = false;
    std::uint64_t fell_ms = 0;
  };

  void PoseHero(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                std::uint32_t blend_per_mille, const HordeTimesOf& times_of);
  void PoseFoes(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                std::uint32_t blend_per_mille, const HordeTimesOf& times_of);
  void PoseFalling(std::uint64_t presentation_ms, const HordeTimesOf& times_of);
  void PoseGround(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                  std::uint32_t blend_per_mille);

  std::vector<FoeMemory> memory_;
  std::vector<FoeMemory> next_memory_;
  std::vector<FallingBody> falling_;
  std::vector<HordePose> poses_;
};

}  // namespace moonclient
