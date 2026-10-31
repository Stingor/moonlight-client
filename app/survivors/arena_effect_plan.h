#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/render/camera.h"
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
// Au banc de fin de partie (horde, évolutions, piliers), vingt lancers et cent
// soixante vivants ne sautent plus rien, sans coût mesurable sur l'image.
inline constexpr std::size_t kMostArenaLaunchesPerFrame = 20;
inline constexpr std::size_t kMostArenaLivingEffects = 160;

// LES INTERRUPTEURS PROPRES À L'ARÈNE, indépendants de `/effect` et de
// `/mineffect` : le joueur règle le mode sans toucher à sa partie.
struct ArenaEffectSwitches {
  // Faux : aucun effet de skill, d'impact ni d'événement ; les chiffres restent.
  bool shown = true;
  // « EFFETS RÉDUITS » (U20) : seuls les chiffres et l'étoile de coup restent,
  // la gerbe ordinaire pour tout skill (`kPlainHitEffect`).
  bool reduced = false;
};

// ── LA PRIORITÉ (A) ─────────────────────────────────────────────────────────
//
// Au plafond, ce qui part d'abord est ce que le joueur a fait, et ce qu'on
// sacrifie d'abord est ce qui se répète. Chaque lancer porte un RANG ; dans une
// image, les lancers s'admettent par rang, puis dans l'ordre des nouvelles.
//
// Les rangs inférieurs au héros ne remplissent pas tout le plafond des
// vivants : une RÉSERVE leur reste fermée, que seul le héros entame, si bien
// qu'un sort lancé après une avalanche d'impacts trouve encore sa place. Les
// réserves ne sont tenues qu'avec les effets pleins : sous « Effets réduits »,
// rien du héros ne passe par le budget, et les impacts gardent tout le plafond.
//
// GARDE-FOUS : le héros reste borné par le plafond des lancers d'une image et
// par celui des vivants ; tous les rangs par le plafond des vivants OÙ QU'ILS
// SOIENT (`kArenaLivingAnywhereFactor`). Et la troupe des nœuds a sa propre
// limite, que rien ici ne lève.
enum class ArenaEffectRank : std::uint8_t {
  // Montée de niveau, évolution, carte ramassée, sorts du héros, flammes de ses
  // murs et de ses piliers.
  kHero,
  // Le premier impact de l'image sur un monstre, à `kArenaNearHitCells` du
  // héros au plus.
  kNearHit,
  // Le premier impact de l'image sur un monstre plus loin.
  kHit,
  // Ce qui se sacrifie d'abord : un impact de plus sur un monstre déjà touché
  // dans l'image, et les effets secondaires — flèches des monstres.
  kLoose,
};

// Un impact est proche à cette distance du héros, en cases.
inline constexpr std::int32_t kArenaNearHitCells = 5;
// La part du plafond des vivants que les impacts laissent au héros, puis celle
// que les impacts répétés et les effets secondaires laissent aux premiers.
inline constexpr std::size_t kArenaHeroLivingReserve = 16;
inline constexpr std::size_t kArenaHitLivingReserve = 16;
// LES VIVANTS HORS CHAMP ne comptent pas au plafond (B), mais tous ensemble ne
// dépassent jamais ce multiple du plafond : un héros qui court laisse derrière
// lui des effets qui vivent encore.
inline constexpr std::size_t kArenaLivingAnywhereFactor = 2;

// LES PLAFONDS EN VIGUEUR : ceux du dépôt, sauf au banc qui les essaie.
struct ArenaEffectCaps {
  std::size_t launches_per_frame = kMostArenaLaunchesPerFrame;
  std::size_t living = kMostArenaLivingEffects;
};

// ── CE QUI NE SE VOIT PAS NE COÛTE RIEN (B) ─────────────────────────────────
//
// LE CADRE VU, en sous-unités de la simulation : l'emprise au sol de la caméra
// de l'arène autour du héros, élargie de `kArenaSightMarginCells` de chaque
// côté. La marge couvre l'étendue d'un effet dont le point est dehors, le
// relief qui montre le sol plus loin que le plan du héros, la secousse, et le
// pas que fait la caméra entre l'image qui la pose et celle qui la lit.
//
// Un lancer hors du cadre ne part pas, et n'est pas compté « sauté » : il est
// « hors champ ». Un effet vivant hors du cadre ne compte pas au plafond des
// vivants. Ce qui dure — orbes, lueurs, effets au sol des zones — reste tenu
// hors champ, hors budget, et se voit dès qu'il revient ; une flamme de mur
// hors champ attend, et se rallume quand elle y revient.
inline constexpr std::int32_t kArenaSightMarginCells = 6;

struct ArenaSight {
  // Sans bornes, le cadre voit tout : la caméra n'est pas encore posée, ou un
  // coin de l'image regarde au-dessus de l'horizon.
  bool bounded = false;
  std::int32_t west_x = 0;
  std::int32_t east_x = 0;
  std::int32_t south_y = 0;
  std::int32_t north_y = 0;

  [[nodiscard]] bool Sees(ro::sim::FieldPoint point) const noexcept {
    return !bounded ||
           (point.x >= west_x && point.x <= east_x && point.y >= south_y && point.y <= north_y);
  }
};

// Le cadre vu d'une caméra de l'arène qui suit `hero`, pour une image de ce
// rapport : son emprise au sol (`GroundFootprintOf`) plus la marge.
[[nodiscard]] ArenaSight ArenaSightOf(const ro::render::Camera& camera, float aspect_ratio,
                                      ro::sim::FieldPoint hero);

// LE BUDGET D'UNE IMAGE : au plus `launches_per_frame` lancers, aucun quand
// `living` effets vivent déjà dans le champ, chaque rang sous son plafond
// (voir `ArenaEffectRank`). Chaque refus est compté « sauté » ; celui qu'une
// réserve seule a refusé l'est aussi « par priorité ».
class ArenaEffectBudget {
 public:
  ArenaEffectBudget() = default;
  explicit ArenaEffectBudget(ArenaEffectCaps caps) noexcept : caps_(caps) {}

  [[nodiscard]] ArenaEffectCaps caps() const noexcept { return caps_; }
  // Sous « Effets réduits » ou sans effets, faux : aucune réserve n'est tenue.
  void set_reserves_held(bool held) noexcept { reserves_held_ = held; }

  // Ouvre l'image, `living_seen` effets vivant dans le champ et
  // `living_anywhere` en tout.
  void BeginFrame(std::size_t living_seen, std::size_t living_anywhere) noexcept;
  // Ouvre l'image, tous les vivants étant dans le champ.
  void BeginFrame(std::size_t living) noexcept { BeginFrame(living, living); }
  // Vrai si ce lancer a sa place ; faux, il est compté sauté.
  [[nodiscard]] bool Admit(ArenaEffectRank rank = ArenaEffectRank::kHero) noexcept;
  // Un lancer hors du cadre vu : ni lancé ni sauté.
  void NoteUnseen() noexcept { ++unseen_total_; }

  // Le plafond des vivants dans le champ pour ce rang.
  [[nodiscard]] std::size_t LivingCeiling(ArenaEffectRank rank) const noexcept;

  [[nodiscard]] std::size_t launched() const noexcept { return launched_; }
  [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }
  [[nodiscard]] std::size_t skipped_total() const noexcept { return skipped_total_; }
  // Les sautés qu'une réserve seule a refusés : sans elle, ils partaient.
  [[nodiscard]] std::size_t ranked_out_total() const noexcept { return ranked_out_total_; }
  [[nodiscard]] std::size_t unseen_total() const noexcept { return unseen_total_; }

 private:
  ArenaEffectCaps caps_;
  bool reserves_held_ = true;
  std::size_t living_seen_ = 0;
  std::size_t living_anywhere_ = 0;
  std::size_t launched_ = 0;
  std::size_t skipped_ = 0;
  std::size_t skipped_total_ = 0;
  std::size_t ranked_out_total_ = 0;
  std::size_t unseen_total_ = 0;
};

// LES VIES DES EFFETS LANCÉS, en instants de fin sur l'horloge de présentation
// d'UNE partie, chacune avec le point où elle vit. Une horloge qui recule est
// celle d'une partie neuve : rien de la précédente ne vit plus, et son budget
// n'est plus entamé.
class ArenaEffectLives {
 public:
  // `at` : le point du sol où l'effet vit ; rien pour un effet que porte le
  // héros, toujours dans le champ.
  void Add(std::uint32_t ends_ms, std::optional<ro::sim::FieldPoint> at = std::nullopt);
  // Retire les vies finies à `now_ms`.
  void Expire(std::uint32_t now_ms);
  void Clear() noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return lives_.size(); }
  // Les vies dans le cadre vu.
  [[nodiscard]] std::size_t CountSeen(const ArenaSight& sight) const noexcept;

 private:
  struct Life {
    std::uint32_t ends_ms = 0;
    std::optional<ro::sim::FieldPoint> at;
  };
  std::vector<Life> lives_;
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
  ArenaEffectRank rank = ArenaEffectRank::kHero;
  // L'entrée de l'appelant qui a demandé ce lancer — une flamme de mur —,
  // rendue telle quelle ; rien pour un lancer né des nouvelles.
  std::optional<std::size_t> caller_index;
};

// Vrai si le lancer se voit : porté par le héros, ou son point ou sa cible dans
// le cadre.
[[nodiscard]] bool ArenaLaunchSeen(const ArenaLaunch& launch, const ArenaSight& sight) noexcept;

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

// LES LANCERS CANDIDATS D'UNE IMAGE, tirés des nouvelles que l'appelant vient
// de prendre, rangés dans `out` (vidé), chacun avec son rang : montée de
// niveau, évolution et carte ramassée, puis les sorts, au rang du héros ; puis
// les impacts, dans l'ordre des nouvelles.
//
// ⚠ NI ZONES NI ORBES NE NAISSENT ICI : murs, piliers et orbes se lisent sur
// l'état, qui les garde tant qu'ils durent. Voir `ArenaEffects`.
void CollectArenaLaunches(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                          const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                          std::vector<ArenaLaunch>& out);

// LE BUDGET TRANCHE : les candidats se rangent par rang, dans l'ordre où ils
// sont venus pour un même rang ; ceux hors du cadre s'écartent « hors champ »,
// les autres passent par `budget`. Ne restent dans `launches` que les admis.
void AdmitArenaLaunches(const ArenaSight& sight, ArenaEffectBudget& budget,
                        std::vector<ArenaLaunch>& launches);

// LES LANCERS D'UNE IMAGE : les candidats des nouvelles, admis par le budget.
//
// 🔴 LES NOUVELLES SE LISENT UNE FOIS : un visuel sauté ou hors champ ne revient
// pas à l'image suivante, il est perdu. La simulation, elle, n'est relue par
// rien.
void PlanArenaLaunches(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                       const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                       const ArenaSight& sight, ArenaEffectBudget& budget,
                       std::vector<ArenaLaunch>& out);

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
