#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

#include "survivors/survivors_looks.h"

// CE QUE LES NOUVELLES D'UNE IMAGE FONT NAÎTRE COMME EFFETS DANS L'ARÈNE,
// décidé sans GPU : `ArenaEffects` ne fait que jouer ce plan.
namespace moonclient {

// LES PLAFONDS DES EFFETS DE L'ARÈNE : au-delà, le visuel est sauté, jamais le
// coup. La simulation a déjà tout compté ; ce qui ne se voit pas ne change rien.
inline constexpr std::size_t kMostArenaLaunchesPerFrame = 12;
inline constexpr std::size_t kMostArenaLivingEffects = 96;

// LES INTERRUPTEURS PROPRES À L'ARÈNE, indépendants de `/effect` et de
// `/mineffect` : le joueur règle le mode sans toucher à sa partie.
struct ArenaEffectSwitches {
  // Faux : aucun effet de skill, d'impact ni d'événement ; les chiffres restent.
  bool shown = true;
  // « EFFETS RÉDUITS » (U20) : seuls les chiffres et l'étoile de coup restent,
  // la gerbe ordinaire pour tout skill (`kPlainHitEffect`).
  bool reduced = false;
};

// LE BUDGET D'UNE IMAGE : douze lancers au plus, et aucun quand quatre-vingt-
// seize effets vivent déjà. Chaque refus est compté « sauté ».
class ArenaEffectBudget {
 public:
  // Ouvre l'image, `living` effets étant en vie.
  void BeginFrame(std::size_t living) noexcept;
  // Vrai si ce lancer a sa place ; faux, il est compté sauté.
  [[nodiscard]] bool Admit() noexcept;

  [[nodiscard]] std::size_t launched() const noexcept { return launched_; }
  [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }
  [[nodiscard]] std::size_t skipped_total() const noexcept { return skipped_total_; }

 private:
  std::size_t living_ = 0;
  std::size_t launched_ = 0;
  std::size_t skipped_ = 0;
  std::size_t skipped_total_ = 0;
};

// LES VIES DES EFFETS LANCÉS, en instants de fin sur l'horloge de présentation
// d'UNE partie. Une horloge qui recule est celle d'une partie neuve : rien de
// la précédente ne vit plus, et son budget n'est plus entamé.
class ArenaEffectLives {
 public:
  void Add(std::uint32_t ends_ms);
  // Retire les vies finies à `now_ms`.
  void Expire(std::uint32_t now_ms);
  void Clear() noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return ends_ms_.size(); }

 private:
  std::vector<std::uint32_t> ends_ms_;
  std::uint32_t last_now_ms_ = 0;
};

enum class ArenaLaunchKind : std::uint8_t {
  // Porté par le héros.
  kOnHero,
  // Posé au sol en `at`.
  kOnGround,
};

struct ArenaLaunch {
  std::uint32_t effect_id = 0;
  ArenaLaunchKind kind = ArenaLaunchKind::kOnGround;
  ro::sim::FieldPoint at;
  // Le point visé, que la troupe lit pour un trait tendu ou un projectile.
  std::optional<ro::sim::FieldPoint> target;
};

// UNE ORBE À PEINDRE : l'effet de son skill, porté par un acteur du monde
// privé reposé là où la simulation la tient.
struct ArenaOrbLook {
  std::uint32_t effect_id = 0;
  ro::sim::FieldPoint at;
};

// L'ACTEUR DU MONDE PRIVÉ qui porte l'effet au sol d'une zone : cette base
// plus le numéro de la zone. Les zones sont numérotées avec les monstres à
// partir de 1, comme le héros et les orbes : sans base, une zone prendrait
// leur acteur. La base est sous celle des lueurs de coffre
// (`kFirstChestGlowActorId`).
inline constexpr std::uint32_t kFirstZoneGroundActorId = 0x20000000U;

// L'EFFET AU SOL D'UNE ZONE À PEINDRE : le visuel que l'unité du skill
// entretient dans le jeu — le cercle du pilier de feu qui attend —, porté par
// un acteur du monde privé posé au point de la zone.
struct ArenaZoneGround {
  std::uint32_t actor_id = 0;
  std::uint32_t effect_id = 0;
  ro::sim::FieldPoint at;
};

// L'APPARENCE DE CHAQUE SKILL DES RÈGLES, résolue une fois.
class ArenaSkillLooks {
 public:
  explicit ArenaSkillLooks(const ro::sim::SurvivorsRules& rules);
  // Nul pour un skill que les règles ne nomment pas.
  [[nodiscard]] const SkillLook* Find(std::uint16_t skill_id) const noexcept;

 private:
  std::vector<SkillLook> looks_;
};

// LES LANCERS D'UNE IMAGE, tirés des nouvelles que l'appelant vient de prendre,
// rangés dans `out` (vidé) par priorité : montée de niveau, évolution et carte
// ramassée, puis les sorts, puis les impacts. Le budget tranche dans cet ordre.
//
// 🔴 LES NOUVELLES SE LISENT UNE FOIS : un visuel sauté ne revient pas à
// l'image suivante, il est perdu. La simulation, elle, n'est relue par rien.
//
// ⚠ NI ZONES NI ORBES NE NAISSENT ICI : murs, piliers et orbes se lisent sur
// l'état, qui les garde tant qu'ils durent. Voir `ArenaEffects`.
void PlanArenaLaunches(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                       const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                       ArenaEffectBudget& budget, std::vector<ArenaLaunch>& out);

// LES ORBES À PEINDRE, une par orbe que `SurvivorsRun::Orbs` rend, dans son
// ordre, rangées dans `out` (vidé). Aucune sous « Effets réduits » ni sans
// effets : ce que l'arène montrait s'éteint à la bascule même.
//
// Hors budget : une orbe est un état qui dure, pas un éclat ; sautée, elle
// tournerait sans se voir jusqu'à son extinction.
void PlanArenaOrbs(std::span<const ro::sim::SurvivorsOrb> orbs, const ArenaSkillLooks& looks,
                   ArenaEffectSwitches switches, std::vector<ArenaOrbLook>& out);

// UN EFFET AU SOL SE MONTRE avec les effets pleins, et toujours s'il est de
// ceux que le jeu ne coupe jamais (`ro::render::IsForcedVisibleEffect`) : le
// cercle du pilier en attente marque où le pilier frappera, il reste sous
// « Effets réduits » comme sans effets.
[[nodiscard]] bool ArenaZoneGroundShown(std::uint32_t effect_id, ArenaEffectSwitches switches);

// LES EFFETS AU SOL DES ZONES, un par zone vivante dont le skill en a un
// (`SkillLook::ground_effect_id`), dans l'ordre des zones, au point où elle
// commence, rangés dans `out` (vidé). Une zone éteinte — expirée, ou au bout
// de ses coups — n'est plus dans l'état : son effet au sol s'éteint avec elle.
// Seuls ceux que `ArenaZoneGroundShown` montre sous `switches`.
//
// Hors budget, comme une orbe : l'effet au sol dure autant que sa zone.
void PlanArenaZoneGrounds(std::span<const ro::sim::SurvivorsZone> zones,
                          const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                          std::vector<ArenaZoneGround>& out);

}  // namespace moonclient
