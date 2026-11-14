#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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
//
// `kStranger` : le corps de la rencontre, que la simulation ignore. Il se compose
// comme un joueur, et `HordePoses` ne le pose pas : voir `StrangerPoseAt`.
//
// `kSummon` : un allié invoqué par un héros, plante ou homoncule, dessiné comme
// un monstre de sa classe, sous la teinte des alliés.
//
// `kCart` : le chariot d'un héros, que `ArenaCarts` dessine ; `HordePoses` ne
// le pose pas.
enum class HordeBodyKind : std::uint8_t {
  kHero,
  kFoe,
  kFalling,
  kPlant,
  kGem,
  kPickup,
  kStranger,
  kSummon,
  kCart
};

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
  // L'image tenue au lieu de celle que donne motion_ms : le héros qui incante
  // tient la première de son rang.
  std::optional<std::size_t> held_frame;
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
// UN CORPS DE LA HORDE QUI NAÎT — monstre, élite, invoqué, boss — paraît en
// fondu d'opacité pendant cette durée, depuis son pas de naissance, au lieu de
// surgir. Seule la présentation fond : la simulation le compte dès ce pas. Un
// monstre qui s'enfouit ressurgit de même, depuis le pas où il sort de terre.
inline constexpr std::uint32_t kHordeBirthFadeMs = 333;
// UNE OMBRE RONDE NE FOND PAS : le renderer la peint de la couleur de son
// calque, sans la transparence de son porteur. Un corps encore transparent —
// qui naît, ressurgit ou s'efface — n'en pose donc aucune, et un mort non plus.
[[nodiscard]] bool CastsRoundShadow(const HordePose& pose) noexcept;
// LE HÉROS LANCE DE TEMPS À AUTRE : un lancer qui le trouve immobile lui fait
// jouer son geste — l'attaque pour une classe de mêlée, l'incantation sinon —
// mais pas plus d'une fois par cette durée, comptée depuis le geste précédent.
// Ses skills partent bien plus souvent, et un geste à chacun le ferait trembler.
inline constexpr std::uint32_t kHeroGestureEveryMs = 2000;
// L'incantation tient sa première image pendant cette durée.
inline constexpr std::uint32_t kHeroCastHoldMs = 600;
// Un héros à terre reste à la dernière image de sa chute : ce temps la dépasse.
inline constexpr float kDownMotionMs = 100000.0F;
// Un mort, sa chute jouée, s'efface pendant cette durée.
inline constexpr std::uint32_t kFallFadeMs = 1000;
// Au plus autant de morts s'effacent à la fois ; le plus ancien cède.
inline constexpr std::size_t kMostFallingBodies = 128;
// L'élite paraît grandie, et dorée.
inline constexpr float kEliteScale = 1.5F;
inline constexpr std::array<std::uint8_t, 4> kEliteTint = {255, 210, 90, 255};
// Glacé, un monstre pâlit vers le bleu ; gelé, il prend le bleu du gel du jeu.
inline constexpr std::array<std::uint8_t, 4> kChilledTint = {160, 200, 255, 255};
// Gelé par un boss, le héros prend le même bleu tant qu'il marche ralenti.
inline constexpr std::array<std::uint8_t, 4> kFrostedHeroTint = kChilledTint;
// UN ALLIÉ INVOQUÉ verdit un peu : le joueur ne le prend pas pour la horde.
inline constexpr std::array<std::uint8_t, 4> kAlliedTint = {200, 255, 200, 255};

// La position d'un point de la simulation, en cases continues.
[[nodiscard]] glm::vec2 CellOfField(glm::vec2 subunits) noexcept;
// Le cap de la simulation (0 au nord, sens trigonométrique, 64 par tour) en
// angle de face du client (0 au sud, 90 à l'est).
[[nodiscard]] float ClientDegreesOfHeading(std::uint8_t heading) noexcept;

// LE CORPS DE LA RENCONTRE au point `at`, d'une opacité de 0 à 1 : debout, tourné
// vers la caméra, sans autre geste.
[[nodiscard]] HordePose StrangerPoseAt(ro::sim::FieldPoint at, float opacity) noexcept;

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
  // LES LANCERS DES DERNIERS PAS peuvent commencer un geste de leur héros à
  // `presentation_ms` (voir `kHeroGestureEveryMs`), allié compris.
  void NoticeCasts(std::span<const ro::sim::SurvivorsCast> casts,
                   const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms);
  // LES COUPS DES DERNIERS PAS : un monstre qu'un piège retient (`FreezeHolds`)
  // ne bleuit pas tant que ce gel dure. ⚠ AVANT `Pose`, comme `NoticeFalls`.
  //
  // L'AUTO-ATTAQUE QUI TOUCHE fait jouer au héros immobile son attaque à
  // `presentation_ms`, à chaque coup : elle n'attend pas `kHeroGestureEveryMs`.
  // En marche, il marche : un geste joué en avançant le ferait glisser.
  void NoticeHits(std::span<const ro::sim::SurvivorsHit> hits, const ro::sim::SurvivorsField& field,
                  std::uint64_t presentation_ms);

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
  // Le lacet de la caméra de l'arène, que les faces des corps suivent.
  void set_camera_yaw_degrees(float yaw_degrees) noexcept { camera_yaw_degrees_ = yaw_degrees; }

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
    // Gelé par un piège qui retient : il ne bleuit pas.
    bool held = false;
    // Son dernier tir vu, par son pas d'extinction, et l'instant où son geste
    // a commencé ; aucun au départ.
    std::uint32_t seen_shot_expires_step = 0;
    std::optional<std::uint64_t> shot_started_ms;
  };
  // Le dernier tir de chaque tireur, par serial croissant.
  struct LatestShot {
    std::uint32_t from_serial = 0;
    std::uint32_t expires_step = 0;
  };
  std::vector<LatestShot> latest_shots_;
  // Un mort qui joue sa chute puis s'efface.
  struct FallingBody {
    std::uint32_t serial = 0;
    std::uint16_t foe_id = 0;
    glm::vec2 cell{0.0F};
    // Son cap à la mort ; rien s'il n'avait pas été vu vivant : il tombe
    // alors face à la caméra.
    std::optional<std::uint8_t> facing;
    bool elite = false;
    std::uint64_t fell_ms = 0;
  };

  void PoseHero(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                std::uint32_t blend_per_mille, const HordeTimesOf& times_of);
  // Les alliés d'une partie coop, après le héros : `look_id` est l'index du héros + 1.
  void PoseAllies(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                  std::uint32_t blend_per_mille, const HordeTimesOf& times_of);
  // Le geste en cours du héros `hero_index`, sur un corps immobile : la mêlée frappe, les
  // autres incantent. `pose.look_id` doit déjà nommer sa tenue.
  void PoseGesture(HordePose& pose, const ro::sim::SurvivorsField& field, std::size_t hero_index,
                   std::uint64_t presentation_ms, const HordeTimesOf& times_of) const;
  // Vrai, et la pose en attaque, quand l'auto-attaque du héros vient de toucher.
  [[nodiscard]] bool PoseStrike(HordePose& pose, std::size_t hero_index,
                                std::uint64_t presentation_ms, const HordeTimesOf& times_of) const;
  void PoseFoes(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                std::uint32_t blend_per_mille, const HordeTimesOf& times_of);
  void PoseFalling(std::uint64_t presentation_ms, const HordeTimesOf& times_of);
  void PoseGround(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                  std::uint32_t blend_per_mille);
  // Les alliés invoqués de chaque héros : ils frappent au coup qu'annonce leur
  // nouvelle, marchent quand ils bougent, se tiennent sinon.
  void PoseSummons(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                   std::uint32_t blend_per_mille, const HordeTimesOf& times_of);

  std::vector<FoeMemory> memory_;
  // Le dernier coup de chaque allié invoqué : son numéro, et l'instant du coup.
  struct SummonStrike {
    std::uint32_t serial = 0;
    std::uint64_t started_ms = 0;
  };
  std::vector<SummonStrike> summon_strikes_;
  std::vector<FoeMemory> next_memory_;
  std::vector<FallingBody> falling_;
  std::vector<HordePose> poses_;
  float camera_yaw_degrees_ = 0.0F;
  // Le début du dernier geste de chaque héros, s'il en a fait un, et s'il ne
  // lançait que le coup de son arme : un coup d'arme se fait du geste
  // d'attaque, quelle que soit la classe.
  std::array<std::optional<std::uint64_t>, ro::sim::kMostHeroes> gesture_started_ms_;
  std::array<bool, ro::sim::kMostHeroes> gesture_weapon_only_ = {};
  // Le dernier coup d'auto-attaque de chaque héros qui a touché un monstre.
  std::array<std::optional<std::uint64_t>, ro::sim::kMostHeroes> strike_started_ms_;
};

}  // namespace moonclient
