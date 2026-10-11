#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/companion_sight.h"
#include "ro/sim/companion_tactics.h"
#include "ro/sim/map_mode.h"

namespace ro::sim {

// ── LES CONTEXTES ET LES CONDUITES ENVERS LES JOUEURS (ADR 0092) ───────────
//
// Chaque personnage choisit, par type de compagnon, un profil pour chacun des
// trois contextes : PvE, PvP et WoE. Le contexte vient du mode que le serveur
// déclare pour la carte.
//
// Un profil peut porter une table « joueur ou classe → conduite », aux mêmes
// rangs et déclencheurs que les tactiques par monstre, lue seulement là où la
// carte ouvre la pleine visée des joueurs (voir `PlayerTargeting`).

// LES TROIS CONTEXTES, dans l'ordre où le fichier et la fenêtre les rangent.
enum class CompanionAiContext : std::uint8_t { kPvE, kPvP, kWoE };
inline constexpr std::size_t kCompanionAiContextCount = 3;

// UN DUEL : le serveur déclare la zone PvP libre et lève le bit « au-delà du
// groupe » pour le seul duelliste, sans le compteur de rang, que seule une
// vraie carte PvP lève, ni le bit « au-delà de la guilde ».
[[nodiscard]] constexpr bool IsDuelMap(const MapMode& mode) noexcept {
  return mode.kind == MapKind::kFreePvpZone && mode.attackable_beyond_party &&
         !mode.pvp_counter_shown && !mode.attackable_beyond_guild && !mode.battlefield;
}

// LE CONTEXTE D'UNE CARTE, qui choisit le profil : WoE pour une zone de siège,
// PvP pour toute autre carte qui fait des joueurs des adversaires, PvE sinon.
//
// ⚠ UN DUEL EST DU PVE : il se joue sur une carte ordinaire, contre un seul
// adversaire, et les passants n'y sont pas des cibles.
// ⚠ LE CHAMP DE BATAILLE EST DU PVP, faute de mieux : un profil de siège
// (emperium, gardiens) n'y a rien à faire. Les joueurs y sont visés selon leur
// camp, voir `PlayerStanceOf`.
[[nodiscard]] CompanionAiContext ContextOfMap(const MapMode& mode) noexcept;

// CE QUE L'IA PEUT FAIRE D'UN JOUEUR sur cette carte.
enum class PlayerTargeting : std::uint8_t {
  // Rien : une carte où les joueurs ne se frappent pas.
  kNone,
  // Riposter à ce qui attaque le camp, sans initiative, conduite ni
  // assistance. C'est le duel, où un seul joueur est un adversaire.
  kDefenceOnly,
  // Défense, assistance, conduites et leur initiative.
  kFull,
  // Le champ de bataille : chaque joueur se vise selon son camp, que le
  // serveur dit acteur par acteur. Voir `PlayerStanceOf`.
  kByCamp,
};

[[nodiscard]] PlayerTargeting PlayerTargetingOf(const MapMode& mode) noexcept;

// LE CAMP D'UN JOUEUR AU CHAMP DE BATAILLE, vu de notre personnage.
enum class BattleRelation : std::uint8_t {
  // Le serveur n'a pas dit son camp, ou pas encore le nôtre.
  kUnknown,
  kAlly,
  kEnemy,
};

// CE QUE L'IA PEUT FAIRE DE CE JOUEUR-CI.
enum class PlayerStance : std::uint8_t {
  // Jamais visé d'elle-même.
  kNone,
  // Riposte seulement.
  kDefenceOnly,
  // La pleine visée du PvP et du WoE : défense, assistance, et une initiative
  // que seule une conduite « toujours » donne — « attaquer de lui-même » ne
  // vise que des monstres.
  kConducts,
  // 🔴 COMME UN MONSTRE : défense, assistance, conduites, et l'initiative
  // d'« attaquer de lui-même » quand aucune ligne ne le désigne. Mêmes règles
  // de choix, de portée, d'engagement et de laisse.
  kAsMonster,
};

// LA VISÉE D'UN JOUEUR, la carte précisée par son camp.
//
// 🔴 AU CHAMP DE BATAILLE, L'ADVERSAIRE SE VISE COMME UN MONSTRE : c'est le
// seul endroit où chaque joueur porte un camp que le serveur nomme, et où
// l'initiative ne peut donc pas tomber sur un coéquipier. Un joueur du camp
// n'est jamais visé, même s'il frappe. Un joueur dont le camp n'est pas connu
// — pas encore dit, ou hors du champ — n'est qu'en défense.
[[nodiscard]] PlayerStance PlayerStanceOf(PlayerTargeting map, BattleRelation relation) noexcept;

// LA GUILDE DU MAÎTRE EST ÉPARGNÉE là où la carte rend hostile « au-delà de la
// guilde » — zone de siège, donjon GvG —, hors du champ de bataille, où les
// camps mêlent les guildes.
[[nodiscard]] bool OwnerGuildSpared(const MapMode& mode) noexcept;

// APRÈS UNE ENTRÉE SUR UNE CARTE, le mode reste neutre jusqu'à ce que le
// serveur le déclare. Une carte qui ne se déclare pas dans ce délai est tenue
// pour ordinaire.
inline constexpr std::uint32_t kCompanionContextSettleMs = 3000;

// LE CONTEXTE OÙ CHOISIR LE PROFIL, une image après l'autre.
//
// 🔴 UN SEUL BASCULEMENT PAR ENTRÉE DE CARTE : le monde remet le mode à neuf en
// entrant, et le serveur ne le redit qu'après. Pendant ce temps neutre, le
// contexte d'avant tient ; un mode de combat déclaré décide aussitôt, un mode
// resté neutre au-delà de `kCompanionContextSettleMs` décide PvE. Sans cela,
// passer d'une carte PvP à une autre ferait PvP → PvE → PvP.
//
// ⚠ LE DÉLAI PART DE L'ARRIVÉE (`arrived_ms`, l'avis de chargement envoyé), et
// non du début de l'image : une carte longue à charger ne doit pas le faire
// échoir avant que le serveur ait pu parler. L'arrivée peut être datée après
// `now_ms` : l'écart est signé.
class CompanionContextWatch {
 public:
  CompanionAiContext Observe(std::uint32_t map_entries, const MapMode& mode,
                             std::uint32_t arrived_ms, std::uint32_t now_ms) noexcept;
  [[nodiscard]] CompanionAiContext context() const noexcept { return context_; }

 private:
  CompanionAiContext context_ = CompanionAiContext::kPvE;
  std::optional<std::uint32_t> seen_entries_;
  // L'arrivée de l'entrée vue, tant que son mode n'a pas décidé.
  std::optional<std::uint32_t> unsettled_since_ms_;
};

// CE QU'UNE LIGNE DE CONDUITE DÉSIGNE.
//
// Il n'y a pas de ligne « membre du groupe du maître » : la règle fixe qui ne
// les vise jamais la rendrait sans effet.
enum class PlayerConductKeyKind : std::uint8_t {
  // Un personnage, par son nom.
  kPlayer,
  // Une classe de joueur.
  kJob,
  // Tout joueur sans ligne plus précise.
  kAnyPlayer,
  // Un membre de la guilde du maître. Sans effet là où la règle fixe les
  // épargne, voir `OwnerGuildSpared`.
  kOwnerGuild,
};

struct PlayerConductKey {
  PlayerConductKeyKind kind = PlayerConductKeyKind::kPlayer;
  // Pour `kPlayer`.
  std::string name;
  // Pour `kJob`.
  std::uint16_t job = 0;

  [[nodiscard]] friend bool operator==(const PlayerConductKey&, const PlayerConductKey&) = default;
};

// AU PLUS CE NOMBRE DE LIGNES par profil, génériques comprises.
inline constexpr std::size_t kCompanionConductMost = 64;

// UNE CONDUITE ENVERS UN JOUEUR : le rang et le déclencheur des tactiques, et
// ceux de leurs champs avancés qui valent pour un joueur — l'emploi des skills,
// le skill d'ouverture, la SP à garder, la poursuite.
//
// ⚠ CEUX QUI NE VALENT PAS sont absents : l'évitement et le harcèlement ne
// suivent que des monstres, le poids ne compte que dans les comptages de
// monstres des règles, et le partage parle du monstre d'un autre joueur.
struct CompanionPlayerConduct {
  PlayerConductKey key;
  TacticRank rank = TacticRank::kMedium;
  TacticTrigger trigger = TacticTrigger::kAlways;
  TacticSkillUse skill_use = TacticSkillUse::kAlways;
  int skill_use_count = 0;
  std::uint16_t opening_skill_id = 0;
  std::int64_t sp_reserve_points = kTacticSpReserveFromBase;
  TacticChase chase = TacticChase::kAsBase;

  [[nodiscard]] friend bool operator==(const CompanionPlayerConduct&,
                                       const CompanionPlayerConduct&) = default;
};

// LA CLASSE À PIED d'une classe montée : le serveur montre un Knight, un
// Crusader, un Lord Knight, un Paladin et leurs versions bébé, montés, sous une
// classe à part. Une conduite par classe vise la classe à pied.
[[nodiscard]] std::uint16_t UnmountedJob(std::uint16_t job) noexcept;

// UNE CLASSE QU'UNE CONDUITE PEUT DÉSIGNER : ni une variante montée ou de
// forme, ni un costume — que le serveur ne montre jamais comme classe propre
// d'un joueur ciblable.
[[nodiscard]] bool IsConductJob(std::uint16_t job) noexcept;

// UNE LIGNE DANS LE CADRE : énumérations connues, nombres dans leurs bornes, nom
// recevable pour une ligne de personnage.
[[nodiscard]] bool ValidConduct(const CompanionPlayerConduct& conduct) noexcept;

class CompanionPlayerConducts {
 public:
  [[nodiscard]] const CompanionPlayerConduct* Find(const PlayerConductKey& key) const noexcept;
  // Pose la ligne, en remplaçant celle de même clé. Rend faux quand la table
  // est pleine ou la ligne hors du cadre.
  bool Set(const CompanionPlayerConduct& conduct);
  bool Remove(const PlayerConductKey& key);
  [[nodiscard]] std::span<const CompanionPlayerConduct> lines() const noexcept { return lines_; }

  [[nodiscard]] friend bool operator==(const CompanionPlayerConducts&,
                                       const CompanionPlayerConducts&) = default;

 private:
  std::vector<CompanionPlayerConduct> lines_;
};

// LA LIGNE QUI S'APPLIQUE À CE JOUEUR, de la plus précise à la plus large : son
// nom, puis la guilde du maître s'il en est (`owner_guildmate`), puis sa
// classe (à pied, voir `UnmountedJob`), puis « tout autre joueur ». Un lien
// avec le maître passe avant la classe : un prêtre de la guilde suit la ligne
// de la guilde.
[[nodiscard]] const CompanionPlayerConduct* ConductFor(const CompanionPlayerConducts& conducts,
                                                       const Actor& player,
                                                       bool owner_guildmate) noexcept;

// LA CONDUITE EN TACTIQUE, pour le combat qui la lit : les champs qui ne valent
// pas pour un joueur y prennent leur valeur neutre.
[[nodiscard]] CompanionTactic TacticOfConduct(const CompanionPlayerConduct& conduct) noexcept;

// CE JOUEUR EST DE LA GUILDE DU MAÎTRE, d'après la guilde que son apparition
// déclare. Sans guilde au maître (zéro), personne ne l'est.
[[nodiscard]] constexpr bool OwnerGuildmate(const Actor& player,
                                            std::uint32_t owner_guild_id) noexcept {
  return owner_guild_id != 0 && player.guild_id == owner_guild_id;
}

// 🔴 LES RÈGLES FIXES, QU'AUCUN PROFIL NE RÈGLE : le maître, ses amis et les
// membres de son groupe ne sont jamais visés d'eux-mêmes ; là où la carte
// l'épargne (`guild_spared`, voir `OwnerGuildSpared`), la guilde du maître non
// plus. Un ordre direct reste permis, le serveur juge.
[[nodiscard]] bool ShieldedPlayer(const Actor& player, const OwnSide& side,
                                  std::span<const std::string> friend_names,
                                  std::uint32_t owner_guild_id, bool guild_spared) noexcept;

}  // namespace ro::sim
