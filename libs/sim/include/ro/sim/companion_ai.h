#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/companion_memory.h"
#include "ro/sim/companion_players.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/companion_sight.h"
#include "ro/sim/companion_skills.h"
#include "ro/sim/companion_tactics.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

class MonsterSheetBook;
class WalkMap;

// ── L'IA DES COMPAGNONS : HOMONCULE ET MERCENAIRE ───────────────────────────
//
// Le serveur ne fait jamais agir un compagnon de lui-même : il n'obéit qu'aux
// demandes du client (aller à une case, attaquer, revenir au maître, lancer un
// skill). `CompanionBrain` décide de ces demandes, une au plus par tick, à
// partir du monde, de ce que l'on sait du compagnon et de ses réglages.
//
// L'ordre direct du joueur prime ; à défaut, le comportement de base que règle
// `CompanionAiSettings` décide. Les règles si/alors et les tactiques par
// monstre de l'ADR 0092 s'intercalent entre les deux.

// LES DEUX COMPAGNONS QU'UNE IA MÈNE.
enum class AiCompanion : std::uint8_t { kHomunculus, kMercenary };

// LE TICK : l'IA d'un compagnon décide au plus une fois par cette période.
inline constexpr std::uint32_t kCompanionAiTickMs = 140;
// LA FILE DES ORDRES RÉSERVÉS (Maj) : au-delà, un ordre réservé se perd.
inline constexpr std::size_t kCompanionReserveCapacity = 10;
// UN DÉPLACEMENT NE PART PAS au-delà de ce nombre de cases de Manhattan depuis
// le compagnon : celui que l'IA calcule s'y raccourcit.
inline constexpr int kCompanionMoveMostCells = 24;
// UN « ALLER ICI » PLUS LONG que ce nombre de cases de Manhattan se coupe au
// point milieu, et le reste du trajet part en tête de la file réservée.
inline constexpr int kCompanionMoveLegCells = 15;
// LE SERVEUR REFUSE l'attaque d'un compagnon pendant ce délai après chaque
// entrée du maître sur une carte. C'est le serveur qui tient cette valeur
// (`homunc_teleport_acd`) ; le réglage d'usine la reprend, et l'IA y soumet
// aussi ses skills.
inline constexpr std::uint32_t kServerCompanionAttackHoldMs = 5000;

// CE QUE L'IA DEMANDE : au plus un par tick.
struct CompanionOrder {
  enum class Kind : std::uint8_t { kNone, kMove, kAttack, kReturnToOwner, kSkill };
  Kind kind = Kind::kNone;
  // kAttack, et kSkill sur un acteur.
  std::uint32_t target_id = 0;
  // kMove, et kSkill sur une case.
  CellPosition cell;
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;

  [[nodiscard]] friend bool operator==(const CompanionOrder&, const CompanionOrder&) = default;
};

// UN ORDRE DIRECT DU JOUEUR : aller ici, attaquer ceci, lancer un skill du
// compagnon sur un acteur ou une case.
struct DirectOrder {
  enum class Kind : std::uint8_t { kMove, kAttack, kSkillOnActor, kSkillOnCell };
  Kind kind = Kind::kMove;
  std::uint32_t target_id = 0;
  CellPosition cell;
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;
  // La portée du skill, mesurée depuis le COMPAGNON : il s'approche jusque-là.
  int skill_range_cells = 0;

  [[nodiscard]] friend bool operator==(const DirectOrder&, const DirectOrder&) = default;
};

// QUAND LE MAÎTRE S'ASSOIT.
enum class OwnerSeated : std::uint8_t {
  kNoChange,
  // Se placer à son côté, et ne rien attaquer de lui-même.
  kStayBesidePassive,
};

// LES RÉGLAGES DU COMPORTEMENT DE BASE (niveau 1 de l'ADR 0092), sauf les skills
// automatiques. Les valeurs par défaut sont celles de l'IA d'origine ; « zéro »
// veut dire « aucun » partout où il borne une distance, un délai ou un seuil.
struct CompanionAiSettings {
  // Suivi : au-delà de cette distance au maître, il le rejoint.
  int follow_distance_cells = 3;
  // Au-delà, il revient au maître quoi qu'il fasse, selon que le maître est
  // immobile ou en marche.
  int leash_still_cells = 0;
  int leash_walking_cells = 0;
  // Engagement : une cible choisie seule doit être à cette distance du MAÎTRE,
  // immobile ou en marche. Sans borne, seule la vue du compagnon compte.
  int engage_still_cells = 0;
  int engage_walking_cells = 0;
  // Attaquer de lui-même le monstre visible le plus proche : d'usine selon
  // l'espèce, voir `BasicAggression`.
  bool attack_on_own = false;
  // PV et SP du compagnon, en pour cent, sous lesquels il ne prend plus
  // l'initiative.
  int own_attack_min_hp_percent = 0;
  int own_attack_min_sp_percent = 0;
  bool defend_owner = true;
  bool assist_owner = false;
  bool defend_partner = false;
  bool defend_self = true;
  // Ne pas prendre de lui-même un monstre qu'un autre joueur combat ; un ordre
  // direct passe outre.
  bool spare_others_monsters = true;
  // Poursuivre une cible hors de portée ; sinon il ne frappe qu'à portée.
  bool chase = true;
  // En pleine poursuite, passer à une cible plus prioritaire.
  bool retarget_while_chasing = false;
  // Lâcher une cible qu'il n'a pas touchée depuis ce délai.
  std::uint32_t drop_untouched_after_ms = 0;
  // Après l'entrée sur une carte, ni attaque ni skill avant ce délai. Suivre et
  // poursuivre ne sont pas bridés.
  std::uint32_t arrival_attack_hold_ms = kServerCompanionAttackHoldMs;
  // En attente : défendre quand même le maître et lui-même.
  bool standby_defends = false;
  // Revenir en attente quand un combat s'achève.
  bool standby_after_fight = false;
  // Garder l'attente au changement de carte.
  bool standby_kept_across_maps = false;
  // Après « aller ici » : rester sur la case, et s'y battre quand même (à
  // portée, sans la quitter).
  bool move_order_holds_cell = false;
  bool move_order_hold_fights = false;
  OwnerSeated owner_seated = OwnerSeated::kNoChange;
  // Sous ces PV, en pour cent : revenir au maître, ou fuir ce qui l'attaque.
  int return_below_hp_percent = 0;
  int flee_below_hp_percent = 0;
  // Les skills offensifs automatiques : voir `CompanionSkillSettings`.
  CompanionSkillSettings skills;
  // Les tactiques par monstre (niveau 2) : aucune d'usine, le niveau 1 décide.
  CompanionTactics tactics;
  // Les règles si/alors (niveau 3), lues de haut en bas : aucune d'usine. Au
  // plus `kCompanionRuleMost`.
  std::vector<CompanionRule> rules;
  // LES AMIS, par nom de personnage : défendus comme le maître, et visés par
  // « un ami » des règles. Au plus `kCompanionFriendMost`.
  std::vector<std::string> friends;
  // LES CONDUITES ENVERS LES JOUEURS, lues en PvP et en WoE seulement : aucune
  // d'usine. Au plus `kCompanionConductMost`.
  CompanionPlayerConducts player_conducts;

  [[nodiscard]] friend bool operator==(const CompanionAiSettings&,
                                       const CompanionAiSettings&) = default;
};

// LES CLASSES DES COMPAGNONS, telles que le serveur les numérote.
// Homoncule : la base, puis seize types de 1 à 16 rangés par lignées de quatre
// espèces (Lif, Amistr, Filir, Vanilmirth) — les huit premiers à leur forme de
// départ, les huit suivants évolués —, puis les cinq mutés de 48 à 52.
inline constexpr int kHomunculusJobBase = 6000;
inline constexpr int kHomunculusBasicTypeCount = 16;
inline constexpr int kHomunculusSpeciesPerLine = 4;
inline constexpr int kHomunculusLastUnevolvedType = 8;
inline constexpr int kHomunculusFirstMutatedType = 48;
inline constexpr int kHomunculusLastType = 52;
// Mercenaire : la base, puis les trente des trois guildes, dix rangs chacune.
inline constexpr int kMercenaryJobBase = 6016;
inline constexpr int kGuildMercenaryCount = 30;
inline constexpr int kGuildMercenaryRanks = 10;

// L'AGRESSIVITÉ D'ORIGINE, fixée par l'espèce (la classe de l'acteur).
// Homoncule : Filir et Vanilmirth attaquent d'eux-mêmes, Lif et Amistr non, une
// espèce hors des seize premières non plus. Mercenaire : les trente des trois
// guildes non, tout autre oui.
[[nodiscard]] bool BasicAggression(AiCompanion kind, std::uint16_t job) noexcept;

// LE COMPAGNON QU'UNE CLASSE DÉSIGNE : un homoncule de 6001 à 6052, hors des
// mercenaires humains de 6017 à 6046 qui recouvrent cette plage ; un
// mercenaire sinon.
[[nodiscard]] AiCompanion AiCompanionOfJob(std::uint16_t job) noexcept;

// LE PROFIL « IA BASIQUE » de ce compagnon.
[[nodiscard]] CompanionAiSettings BasicCompanionAiSettings(AiCompanion kind,
                                                           std::uint16_t job) noexcept;

// CE QUE L'APPELANT SAIT DU COMPAGNON, à chaque tick. Le maître est le joueur
// du monde.
struct CompanionFacts {
  std::uint32_t actor_id = 0;
  AiCompanion kind = AiCompanion::kHomunculus;
  std::uint16_t job = 0;
  // Là, ni au repos ni mort : sans cela l'IA ne décide rien.
  bool active = false;
  std::int64_t hp = 0;
  std::int64_t max_hp = 0;
  std::int64_t sp = 0;
  std::int64_t max_sp = 0;
  // Le délai entre deux attaques.
  std::uint32_t amotion_ms = 0;
  int attack_range_cells = 1;
  // L'autre compagnon du maître, zéro sans lui.
  std::uint32_t partner_id = 0;
  // Les membres du groupe du maître, par identifiant de compte : leurs
  // invocations sont du camp.
  std::vector<std::uint32_t> party_member_ids;
  // Ses SP sont connues : sans elles, aucun skill ne part de lui-même.
  bool sp_known = false;
  // Ses skills, dans l'ordre de son livre.
  std::vector<CompanionSkillFact> skills;
  // LES FICHES DE MONSTRE que le serveur a données, par classe de sprite ; nul
  // sans elles. Les conditions sur la race, l'élément, la taille ou le rang
  // d'une cible les lisent.
  const MonsterSheetBook* monster_sheets = nullptr;
  // LA CARTE DE MARCHE de la carte courante, nulle sans elle : une case visée ou
  // un pas d'écart y doivent être marchables.
  const WalkMap* walk_map = nullptr;
  // LA LISTE COMPLÈTE DES STATUTS DU COMPAGNON que le serveur rend sur demande,
  // et l'instant où elle est arrivée ; vide sans liste qui fasse foi.
  std::vector<StatusEffectId> statuses;
  std::optional<std::uint32_t> statuses_ms;
  // LA GUILDE DU MAÎTRE, zéro sans guilde : en WoE, ses membres ne sont jamais
  // visés d'eux-mêmes.
  std::uint32_t owner_guild_id = 0;
};

// UNE LISTE DE STATUTS DU COMPAGNON vaut ce temps : au-delà, elle est périmée.
inline constexpr std::uint32_t kCompanionStatusListFreshMs = 3000;

// POURQUOI LE DERNIER ORDRE. Les raisons d'une cible sont rangées de la plus
// prioritaire à la moins prioritaire.
enum class CompanionAiReason : std::uint8_t {
  kNone,
  kDefendOwner,
  kDefendSelf,
  kDefendPartner,
  // Un ami du profil, défendu comme le maître.
  kDefendFriend,
  kAssistOwner,
  kOwnInitiative,
  kFollowOwner,
  kStandby,
  kDirectMove,
  kDirectAttack,
  kDirectSkill,
  kLeash,
  kRetreat,
  kFlee,
  kOwnerSeated,
  // S'écarter d'un monstre que sa tactique dit d'éviter.
  kAvoid,
  // Un skill sur un monstre harcelable, pendant un combat contre un autre.
  kHarass,
  // Une règle si/alors, voir `CompanionBrain::deciding_rule`.
  kRule,
};

// LA RÈGLE QUI A DÉCIDÉ : son rang dans la liste, et quand.
struct DecidingRule {
  std::size_t index = 0;
  std::uint32_t at_ms = 0;

  [[nodiscard]] friend bool operator==(const DecidingRule&, const DecidingRule&) = default;
};

// LE GESTE D'ATTAQUE EN DEUX APPUIS : le premier appui sur un acteur le
// désigne, un second appui séparé sur le même acteur l'attaque. La désignation
// n'expire pas ; un appui sur un autre acteur la déplace.
//
// LA MARQUE est l'acteur que le dernier premier appui a désigné, et qui porte le
// marqueur de cible. ⚠ ELLE SURVIT AU SECOND APPUI : l'attaque partie, la cible
// reste marquée. Seul un ordre de marche l'efface — ni l'ordre réservé, ni
// l'attente, ni le changement de carte, ni l'absence du compagnon.
enum class AttackPress : std::uint8_t {
  // Le second appui d'un double-clic : il ne compte pas.
  kIgnored,
  kDesignated,
  kAttack,
  // Avec Maj : l'attaque part tout de suite dans la file réservée, sans
  // toucher ni à la désignation ni à la marque.
  kReserveAttack,
};

class AttackDesignation {
 public:
  [[nodiscard]] AttackPress Press(std::uint32_t actor_id, bool reserve,
                                  bool second_of_double) noexcept;
  // Un ordre de marche : la désignation et la marque s'éteignent.
  void Clear() noexcept {
    marked_id_ = 0;
    armed_ = false;
  }
  // L'acteur qu'un second appui attaquerait, ou zéro.
  [[nodiscard]] std::uint32_t designated_id() const noexcept { return armed_ ? marked_id_ : 0; }
  // L'acteur qui porte le marqueur de cible, ou zéro.
  [[nodiscard]] std::uint32_t marked_id() const noexcept { return marked_id_; }

 private:
  std::uint32_t marked_id_ = 0;
  bool armed_ = false;
};

// L'IA D'UN COMPAGNON : une instance par compagnon.
class CompanionBrain {
 public:
  // UN TICK. Rend au plus un ordre, et jamais deux fois le même tant qu'il
  // court. Ne décide rien moins de `kCompanionAiTickMs` après le précédent.
  [[nodiscard]] std::optional<CompanionOrder> Tick(const World& world, const CompanionFacts& facts,
                                                   const CompanionAiSettings& settings,
                                                   std::uint32_t now_ms);

  // UN ORDRE DIRECT : il prime sur tout, vide la file réservée et lève l'attente.
  void Command(const DirectOrder& order);
  // UN ORDRE RÉSERVÉ (Maj) : il attend son tour, et se joue quand le compagnon
  // n'a plus rien à faire. Rend faux quand la file est pleine.
  bool Reserve(const DirectOrder& order);
  // « EN ATTENTE » : une bascule. Le compagnon colle au maître et ne riposte
  // pas ; il en sort par une seconde bascule ou par un ordre direct.
  void ToggleStandby();
  [[nodiscard]] bool standby() const noexcept { return standby_; }

  // UNE NOUVELLE CARTE : tout repart à zéro, sauf ce que garde la mémoire (voir
  // `ro/sim/companion_memory.h`), l'attente seulement si `keep_standby`. Le
  // délai d'arrivée court depuis `now_ms`.
  void ForgetMap(bool keep_standby, std::uint32_t now_ms);

  // ── LA MÉMOIRE ────────────────────────────────────────────────────────────
  //
  // Ce qui survit, mesuré depuis `now_ms`.
  [[nodiscard]] CompanionMemory Remember(std::uint32_t now_ms) const;
  // Reprend une mémoire mesurée depuis `now_ms` ; l'attente suit la sienne. Les
  // états de règle se rattachent à la liste de règles au tick suivant, par
  // contenu et par rang.
  void Recall(const CompanionMemory& memory, std::uint32_t now_ms);
  // UN AUTRE PROFIL : tout repart de zéro sauf `memory` et le délai d'arrivée
  // sur la carte, qui court toujours.
  void Restart(const CompanionMemory& memory, std::uint32_t now_ms);

  // LE SERVEUR A REFUSÉ CE SKILL du compagnon : il se suspend.
  void NoteSkillRefused(std::uint16_t skill_id, std::uint32_t now_ms) {
    skills_.NoteRefused(skill_id, now_ms);
  }

  [[nodiscard]] CompanionAiReason last_reason() const noexcept { return last_reason_; }
  [[nodiscard]] std::size_t reserved_count() const noexcept { return reserved_.size(); }
  // La cible du combat en cours, zéro sans combat.
  [[nodiscard]] std::uint32_t target_id() const noexcept { return target_id_; }
  // LA CASE QU'IL GARDE après « aller ici », tant qu'il la tient — combat
  // parti de la case compris. Rien sinon.
  [[nodiscard]] std::optional<CellPosition> held_cell() const noexcept {
    if (mode_ == Mode::kHoldCell || (mode_ == Mode::kFight && fight_from_hold_)) {
      return held_cell_;
    }
    return std::nullopt;
  }
  // LA DERNIÈRE RÈGLE QUI A DÉCIDÉ, pour la fenêtre qui la surligne.
  [[nodiscard]] const std::optional<DecidingRule>& deciding_rule() const noexcept {
    return deciding_rule_;
  }
  // LA FICHE DE MONSTRE qu'une condition a voulu lire au dernier tick sans
  // l'avoir : à demander au serveur. Zéro sans demande.
  [[nodiscard]] std::uint32_t wanted_monster_sheet() const noexcept {
    return wanted_monster_sheet_;
  }

  // POURQUOI UN ACTEUR N'A PAS ÉTÉ PRIS pour cible, au dernier tic qui en a
  // cherché une : pour le journal, qui sépare ainsi une cible refusée d'un
  // combat qui n'émet plus.
  enum class TargetRefusal : std::uint8_t {
    kNotFoe,
    kAbandoned,
    kConcealed,
    kOwnSummon,
    kBeyondSight,
    // Hors de portée d'attaque, sans poursuite permise.
    kOutOfReach,
    kBeyondEngagement,
    kBeyondLeash,
    kIgnored,
    kHeld,
    // Seuils de PV ou de SP « pour attaquer de lui-même » non tenus, ou attente.
    kInitiativeBarred,
    // Le monstre d'un autre joueur, laissé.
    kSpared,
    // Ni défense, ni assistance, ni initiative permise par le profil.
    kNoMotive,
  };
  // Rien quand ce tic n'a pas cherché de cible, ou l'a prise.
  [[nodiscard]] std::optional<TargetRefusal> RefusalOf(std::uint32_t actor_id) const;
  // Le mode en cours, en nombre : pour le journal.
  [[nodiscard]] int mode_number() const noexcept { return static_cast<int>(mode_); }

  // 🔴 UN ABANDON EXPIRE : sans quoi un monstre seul à portée, lâché une fois,
  // ne serait plus jamais repris tant qu'aucune autre cible ne se présente.
  static constexpr std::uint32_t kAbandonedRetakeMs = 10000;

  struct Situation;

 private:
  enum class Mode : std::uint8_t { kIdle, kFight, kMoveOrder, kHoldCell, kSkillOrder };

  struct TargetPick {
    std::uint32_t actor_id = 0;
    CompanionAiReason reason = CompanionAiReason::kNone;
    int distance_cells = 0;
    // Le rang de sa tactique ; « moyen » sans ligne.
    TacticRank rank = TacticRank::kMedium;
    // La ligne qui le gouverne, recopiée : le profil peut changer en plein
    // combat.
    std::optional<CompanionTactic> tactic;
  };
  // Mieux classé : le rang, puis la raison, puis la distance, puis l'id.
  [[nodiscard]] static bool Outranks(const TargetPick& pick, const TargetPick& than) noexcept;
  [[nodiscard]] std::optional<CompanionOrder> Step(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> StepBase(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> StepIdle(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> StepFight(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> StepMoveOrder(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> StepHold(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> StepSkillOrder(const Situation& s);
  [[nodiscard]] std::optional<CompanionOrder> FollowOwner(const Situation& s, int threshold_cells,
                                                          CompanionAiReason reason);
  [[nodiscard]] std::optional<CompanionOrder> Flee(const Situation& s);
  // UN DÉPLACEMENT vers cette case, sauf s'il y court déjà.
  [[nodiscard]] std::optional<CompanionOrder> MoveTo(const Situation& s, CellPosition destination,
                                                     CompanionAiReason reason);
  // S'ÉCARTER d'un monstre que sa tactique dit d'éviter, s'il est trop près.
  // `avoiding` dit si l'évitement prend la main à ce tick, ordre ou non.
  [[nodiscard]] std::optional<CompanionOrder> TryAvoid(const Situation& s, bool& avoiding);
  // UN SKILL SUR UN MONSTRE HARCELABLE pendant le combat contre un autre ; la
  // cible courante ne change pas.
  [[nodiscard]] std::optional<CompanionOrder> TryHarass(const Situation& s);
  // Retient quand chaque monstre a été vu, ceux qu'on a vus bouger, attaquer ou
  // incanter, et ceux qui ont attaqué le camp.
  void NoteMonsters(const Situation& s);
  // UN MONSTRE IMMOBILE : vu depuis `kStillObservationMs` au moins, sans jamais
  // bouger, attaquer ni incanter.
  [[nodiscard]] bool IsStill(std::uint32_t actor_id, std::uint32_t now_ms) const;
  // LA LIGNE QUI GOUVERNE CE MONSTRE à cet instant, ou aucune.
  [[nodiscard]] const CompanionTactic* TacticOf(const Situation& s, const Actor& monster) const;
  // LE SEUIL D'ÉVITEMENT de ce monstre, quand sa ligne dit de l'éviter à cet
  // instant : toujours, ou « s'il attaque » et il a attaqué le camp.
  [[nodiscard]] std::optional<int> AvoidThresholdOf(const Situation& s, const Actor& monster) const;
  // UNE CASE À PORTÉE DE SUIVI DU MAÎTRE, hors du seuil de tout monstre évité,
  // la plus proche du compagnon ; vide quand aucun monstre n'est évité près du
  // maître, ou qu'aucune case ne convient. C'est ce qui empêche l'évitement et
  // le suivi de se renvoyer le compagnon.
  [[nodiscard]] std::optional<CellPosition> SafeCellNearOwner(const Situation& s) const;
  // UN JOUEUR QUE L'IA PEUT VISER D'ELLE-MÊME : sur une carte qui rend les
  // joueurs adversaires, ciblable, et hors des règles fixes (voir
  // `ShieldedPlayer`). La vue et le rang « Ignorer » de sa conduite se jugent à
  // part.
  [[nodiscard]] bool IsPlayerFoe(const Situation& s, const Actor& actor) const;
  // CE QUE LA CARTE, ET AU CHAMP DE BATAILLE SON CAMP, PERMETTENT SUR CE JOUEUR.
  [[nodiscard]] static PlayerStance StanceOf(const Situation& s, const Actor& player);
  // SA CONDUITE, en tactique ; vide sans ligne, et hors de la pleine visée.
  [[nodiscard]] std::optional<CompanionTactic> PlayerTacticOf(const Situation& s,
                                                              const Actor& player) const;
  // UN ENNEMI QU'UNE ACTION DE RÈGLE PEUT VISER : un monstre (`IsRuleTarget`),
  // ou un joueur adversaire que sa conduite n'ignore pas.
  [[nodiscard]] bool IsRuleFoe(const Situation& s, const Actor& actor) const;
  // LA LIGNE QUI GOUVERNE LA CIBLE AUTOMATIQUE, relue à chaque tick : un
  // « Ignorer » posé en plein combat, une invocation qu'on découvre du camp, un
  // joueur qui rejoint le groupe ou une carte qui n'est plus PvP la font lâcher
  // (faux). Sinon le rang et la ligne recopiés se mettent à jour.
  [[nodiscard]] bool RereadTargetLine(const Situation& s, const Actor& target);
  // La poursuite pour la cible courante : celle de sa tactique, sinon le réglage.
  [[nodiscard]] bool ChaseAllowed(const Situation& s) const;
  // CE MONSTRE EST À UN AUTRE JOUEUR et la règle de partage ne permet pas de le
  // prendre : celle de sa tactique, sinon celle du niveau 1.
  // `engaged` : la cible est déjà engagée, et « s'il ne bouge pas » ne se juge
  // qu'au choix ; un pas ne la fait pas lâcher.
  [[nodiscard]] bool SparedFor(const Situation& s, const Actor& monster,
                               const CompanionTactic* tactic, bool engaged = false) const;
  // LES SKILLS AUTOMATIQUES RÉGLÉS POUR UNE CIBLE : sa SP à garder et le niveau
  // imposé par « une fois au niveau L » ; rien quand sa tactique les interdit ou
  // qu'elle en a épuisé l'usage. L'usage se compte aux skills partis (tracés) et
  // à ceux qui attendent leur trace.
  struct TunedSkills {
    CompanionSkillSettings settings;
    std::uint16_t forced_level = 0;
  };
  [[nodiscard]] std::optional<TunedSkills> TunedFor(const Situation& s,
                                                    const CompanionTactic* tactic,
                                                    std::uint32_t target_id) const;
  // Celle de la cible courante : sans tactique pour un ordre direct.
  [[nodiscard]] std::optional<TunedSkills> TunedForTarget(const Situation& s) const;
  // Les skills partis depuis le tick précédent comptent dans l'usage de leur
  // cible, et une ouverture partie ne se relance plus ; ceux d'une règle
  // comptent dans son délai et son quota.
  void CountTracedSkills(std::uint32_t now_ms);

  // ── LES RÈGLES SI/ALORS ───────────────────────────────────────────────────
  //
  // Ce qu'une règle a fait, à son rang : quand elle a agi pour la dernière
  // fois, combien de fois sur la cible de combat `quota_target_id`, et jusqu'à
  // quand un refus la suspend.
  struct RuleState {
    std::optional<std::uint32_t> last_acted_ms;
    std::uint32_t quota_target_id = 0;
    int quota_uses = 0;
    std::optional<std::uint32_t> suspended_until_ms;
  };
  // CE QU'UNE RÈGLE A DÉCIDÉ : un ordre, ou rien pour « rester passif ».
  // ⚠ AUCUNE AUTRE ACTION NE DÉCIDE SANS ORDRE : une action qui n'a rien à
  // envoyer — déjà arrivée, contre un mur, l'ordre qui court déjà — est
  // infaisable, et la suite a sa chance.
  struct RuleDecision {
    std::optional<CompanionOrder> order;
  };
  // Les usages d'une règle sur la cible de combat courante : ceux tracés, et
  // ceux qui attendent leur trace.
  [[nodiscard]] int QuotaUses(std::size_t index) const;
  // Un usage de plus, sur la cible de combat courante ; un autre combat
  // repart de zéro.
  void CountRuleUse(std::size_t index, std::uint32_t combat_target_id, std::uint32_t now_ms);
  // Une liste de règles changée repart de zéro ; les refus du serveur
  // suspendent leur règle.
  void SyncRules(const CompanionAiSettings& settings, std::uint32_t now_ms);
  // LA LECTURE : la première règle vraie ET faisable décide ; vide quand aucune.
  [[nodiscard]] std::optional<RuleDecision> TryRules(const Situation& s);
  // Retient ce que les conditions lisent dans la durée : depuis quand le maître
  // et le compagnon sont attaqués, quand chaque skill du compagnon a paru.
  void NoteRuleFacts(const Situation& s);
  [[nodiscard]] bool RuleHolds(const Situation& s, const CompanionRule& rule);
  // VRAI, FAUX, ou INCONNU (vide) : la lecture tient l'inconnu pour faux.
  [[nodiscard]] std::optional<bool> ConditionHolds(const Situation& s,
                                                   const RuleCondition& condition);
  [[nodiscard]] std::optional<bool> TargetFact(const Situation& s, const RuleCondition& condition,
                                               const Actor& target);
  // L'action de la règle `index`, si elle est faisable maintenant.
  [[nodiscard]] std::optional<RuleDecision> RuleAct(const Situation& s, std::size_t index,
                                                    const CompanionRule& rule);
  [[nodiscard]] std::optional<RuleDecision> RuleCast(const Situation& s, std::size_t index,
                                                     const CompanionRule& rule);
  // L'acteur que vise une cible d'action, nul sans lui.
  [[nodiscard]] const Actor* AimedActor(const Situation& s, RuleAim aim, int range_cells) const;
  // LA MEILLEURE CASE pour une zone de `radius_cells` lancée à `range_cells`.
  [[nodiscard]] std::optional<CellPosition> BestCell(const Situation& s, int range_cells,
                                                     int radius_cells) const;
  // UN ENNEMI QUE LES RÈGLES COMPTENT dans leurs conditions : un monstre
  // visible, ciblable, hors des invocations du camp ; son poids est celui de sa
  // tactique.
  [[nodiscard]] bool IsRuleEnemy(const Situation& s, const Actor& actor) const;
  // UN ENNEMI QU'UNE RÈGLE PEUT VISER ou compter sous sa zone : en plus, ni
  // « Ignorer », ni invocation d'un autre joueur, ni monstre d'un autre joueur
  // que le partage interdit.
  [[nodiscard]] bool IsRuleTarget(const Situation& s, const Actor& actor) const;
  // UNE CASE OÙ ALLER OU LANCER : marchable, quand le client a la carte.
  [[nodiscard]] static bool WalkableCell(const Situation& s, CellPosition cell);
  // Les statuts du compagnon : porté, pas porté, ou inconnu (vide).
  [[nodiscard]] static std::optional<bool> CompanionWears(const Situation& s,
                                                          StatusEffectId status);
  // Un coup du compagnon sur sa cible, porté depuis la première attaque restée
  // sans réponse, répond à toutes.
  void NoteAttackAnswered(const Situation& s);
  [[nodiscard]] float WeightOf(const Situation& s, const Actor& monster) const;
  // Le poids des monstres qui attaquent `victim_id`.
  [[nodiscard]] float AttackersWeight(const Situation& s, std::uint32_t victim_id) const;
  // UN SKILL OFFENSIF SUR LA CIBLE DU COMBAT, si le mode le permet ici et
  // qu'un skill part. `in_attack_range` : la cible est à portée d'attaque simple.
  [[nodiscard]] std::optional<CompanionOrder> TrySkill(const Situation& s, const Actor& target,
                                                       CellPosition target_cell,
                                                       bool in_attack_range);
  // Les skills peuvent viser cette cible à cet instant : délai d'arrivée passé,
  // et pas le monstre d'un autre joueur.
  [[nodiscard]] bool SkillsAllowedOn(const Situation& s, const Actor& target) const;
  [[nodiscard]] SkillMoment MomentOf(const Situation& s, CellPosition target_cell) const;
  // LA CASE OÙ ALLER pour avoir `target_cell` à portée, ramenée à la bride des
  // déplacements.
  [[nodiscard]] static CellPosition ApproachDestination(const Situation& s,
                                                        CellPosition target_cell, int range_cells);
  [[nodiscard]] std::optional<CompanionOrder> MoveToward(const Situation& s,
                                                         CellPosition target_cell, int range_cells,
                                                         CompanionAiReason reason);
  [[nodiscard]] std::optional<CompanionOrder> Emit(const Situation& s, CompanionOrder order,
                                                   CompanionAiReason reason);

  // LA MEILLEURE CIBLE AUTOMATIQUE, voir `Outranks`, filtrée par les tactiques.
  // `defence_only` ne retient que ce qui attaque le camp. `holding_cell` ne
  // retient que ce qui est déjà à portée d'attaque ; sans lui, c'est la
  // poursuite permise pour chaque candidat qui en décide.
  [[nodiscard]] std::optional<TargetPick> SelectTarget(const Situation& s, bool defence_only,
                                                       bool holding_cell) const;
  // La cible lâchée depuis moins de `kAbandonedRetakeMs`.
  [[nodiscard]] bool IsAbandoned(std::uint32_t actor_id, std::uint32_t now_ms) const noexcept;
  // Le délai d'arrivée court encore : ni attaque ni skill.
  [[nodiscard]] bool ArrivalHolding(const Situation& s) const;
  [[nodiscard]] bool AttackReady(const Situation& s) const;
  [[nodiscard]] bool MoveSettled(std::uint32_t now_ms) const;
  [[nodiscard]] bool TargetLeftUntouched(const Situation& s) const;
  [[nodiscard]] bool AttacksUnanswered(const Situation& s) const;

  void StartDirect(const Situation& s, const DirectOrder& order);
  void StartFight(const TargetPick& pick, bool direct, bool from_hold, std::uint32_t now_ms);
  // Le combat s'achève : `over` quand la cible est perdue (et non quand un
  // autre souci l'interrompt), ce qui peut faire revenir en attente.
  void EndFight(const Situation& s, bool over);
  void ClearFight();

  Mode mode_ = Mode::kIdle;
  bool standby_ = false;
  std::optional<DirectOrder> pending_;
  std::deque<DirectOrder> reserved_;

  // Le combat en cours.
  std::uint32_t target_id_ = 0;
  CompanionAiReason target_reason_ = CompanionAiReason::kNone;
  bool target_direct_ = false;
  bool fight_from_hold_ = false;
  std::uint32_t engaged_since_ms_ = 0;
  // La première attaque envoyée que nul coup du compagnon n'a encore suivie.
  std::optional<std::uint32_t> unanswered_since_ms_;
  // La dernière cible lâchée faute de l'avoir touchée : elle n'est plus choisie
  // seule jusqu'à la prochaine carte.
  // LA CIBLE LÂCHÉE — jamais touchée, ou ses attaques sans réponse — et quand.
  // Elle n'est pas reprise d'elle-même avant `kAbandonedRetakeMs`.
  std::uint32_t abandoned_target_id_ = 0;
  std::uint32_t abandoned_at_ms_ = 0;
  // Les refus de la dernière recherche de cible, vidés à chaque tic.
  mutable std::vector<std::pair<std::uint32_t, TargetRefusal>> refusals_;
  TacticRank target_rank_ = TacticRank::kMedium;
  std::optional<CompanionTactic> target_tactic_;

  // CE QUE LES TACTIQUES RETIENNENT SUR CETTE CARTE, par acteur.
  // Quand chaque monstre a été vu pour la première fois.
  std::map<std::uint32_t, std::uint32_t> first_seen_ms_;
  // Les monstres vus bouger, attaquer ou incanter : ils ne sont pas immobiles.
  std::set<std::uint32_t> active_monsters_;
  // Les monstres vus attaquer le camp.
  std::set<std::uint32_t> camp_attackers_;
  // Les cibles « Tenir » lâchées après une frappe, et quand.
  std::map<std::uint32_t, std::uint32_t> held_released_ms_;
  // Les skills automatiques partis (tracés) sur chaque cible.
  std::map<std::uint32_t, int> skill_uses_;
  // Les cibles qui ont reçu leur skill d'ouverture, et les monstres harcelés.
  // L'ouverture attendue : quel skill, sur quelle cible.
  std::set<std::uint32_t> opened_;
  std::map<std::uint32_t, std::uint16_t> opening_wanted_;
  std::set<std::uint32_t> harassed_;

  // L'ordre direct en cours : la case d'un « aller ici », le skill.
  CellPosition move_destination_;
  // La case où il s'est arrêté : celle qu'il garde. ⚠ PAS FORCÉMENT LA
  // DESTINATION, un trajet pouvant s'arrêter en chemin.
  CellPosition held_cell_;
  bool move_sent_ = false;
  DirectOrder skill_order_;

  // De quoi ne pas répéter un ordre qui court.
  std::optional<CellPosition> moved_to_cell_;
  std::optional<CellPosition> followed_owner_cell_;
  std::optional<std::uint32_t> last_move_sent_ms_;
  std::optional<std::uint32_t> last_attack_sent_ms_;

  // LES RÈGLES : leur état par rang, et la liste à laquelle il se rapporte —
  // une liste changée repart de zéro.
  std::vector<RuleState> rule_states_;
  std::vector<CompanionRule> rules_seen_;
  std::optional<DecidingRule> deciding_rule_;
  std::uint32_t wanted_monster_sheet_ = 0;
  // Depuis quand le maître et le compagnon sont attaqués sans interruption.
  std::optional<std::uint32_t> owner_attacked_since_ms_;
  std::optional<std::uint32_t> self_attacked_since_ms_;
  // Quand chaque skill du compagnon a paru pour la dernière fois (incantation,
  // ou coup qui le porte).
  std::map<std::uint16_t, std::uint32_t> skill_seen_ms_;
  // Quand le cerveau a envoyé chaque skill pour la dernière fois.
  std::map<std::uint16_t, std::uint32_t> skill_sent_ms_;
  // « NE RIEN FAIRE » A DÉCIDÉ CE TICK : ni attaque ni skill, seulement les
  // déplacements de sûreté.
  bool offense_held_ = false;

  std::optional<std::uint32_t> last_tick_ms_;
  std::optional<std::uint32_t> arrived_ms_;
  CompanionAiReason last_reason_ = CompanionAiReason::kNone;
  CompanionSkillCaster skills_;
};

}  // namespace ro::sim
