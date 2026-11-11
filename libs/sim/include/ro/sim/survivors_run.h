#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/core/pcg32.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_flow.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_grid.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

struct SurvivorsLoadout {
  SurvivorsStats bonus;  // méta ; vide en défi et en partie classée
  std::uint8_t rerolls = 0;
  std::uint8_t skips = 0;
  std::uint8_t banishes = 0;
  [[nodiscard]] bool operator==(const SurvivorsLoadout&) const = default;
};
// Chaque bonus dans [−kMostStatBonus, kMostStatBonus] : ce que Start et le
// décodage d'un journal exigent, pour que les statistiques tiennent en 32 bits.
// Une statistique hors de celles d'un loadout y vaut zéro : le journal ne
// l'écrit pas.
[[nodiscard]] inline bool LoadoutWithinBounds(const SurvivorsLoadout& loadout) noexcept {
  for (std::size_t stat = 0; stat < loadout.bonus.values.size(); ++stat) {
    const std::int32_t bonus = loadout.bonus.values[stat];
    if (bonus < -kMostStatBonus || bonus > kMostStatBonus) return false;
    if (stat >= kSurvivorsLoadoutStatCount && bonus != 0) return false;
  }
  return true;
}
// Tout ce qui fixe une partie avant son premier pas : avec les règles et
// l'arène, de quoi la rejouer à l'identique.
// Les Id qu'un départ peut verrouiller : une borne du journal, au-delà de la
// taille de toute réserve de classe.
inline constexpr std::size_t kMostLockedIds = 64;
// CE QU'UN ALLIÉ APPORTE À UNE PARTIE COOP : tout ce qu'un départ fixe pour
// un joueur. La graine, l'arène et la Cursed Water sont celles de l'équipe.
struct SurvivorsAllyStart {
  std::uint16_t class_id = 0;
  std::uint16_t first_skill_id = 0;
  SurvivorsLoadout loadout;
  std::uint8_t lives = 0;
  std::uint8_t leaves = 0;
  std::vector<std::uint16_t> locked_ids;
  [[nodiscard]] bool operator==(const SurvivorsAllyStart&) const = default;
};
struct SurvivorsStart {
  std::uint64_t seed = 0;
  std::uint16_t class_id = 0;
  std::uint16_t arena_id = 0;
  std::uint16_t first_skill_id = 0;
  SurvivorsLoadout loadout;
  std::uint8_t lives = 0;         // relèves en réserve (Resurrection)
  std::uint8_t cursed_water = 0;  // rang, de 0 à kMostCursedWater
  // Les skills et les cartes retirés des réserves de la classe, par Id
  // strictement croissant : jamais offerts dans cette partie.
  std::vector<std::uint16_t> locked_ids;
  // LES FEUILLES D'YGGDRASIL de ce joueur : sans effet en solo.
  std::uint8_t leaves = 0;
  // Les autres joueurs d'une partie coop, au plus kMostHeroes − 1 ; vide en solo.
  std::vector<SurvivorsAllyStart> allies;
  [[nodiscard]] bool operator==(const SurvivorsStart&) const = default;
};
// LE DÉPART NEUTRE, celui du défi et de la partie classée : aucun bonus,
// aucune charge, aucune vie, aucune Cursed Water, rien de verrouillé. Tout le
// monde y joue la même partie.
[[nodiscard]] inline bool IsNeutralStart(const SurvivorsStart& start) noexcept {
  return start.loadout == SurvivorsLoadout{} && start.lives == 0 && start.cursed_water == 0 &&
         start.locked_ids.empty() && start.allies.empty();
}
// LA CASE DE DÉPART d'une arène : celle que la règle d'arène nomme, sinon le
// centre de la carte, ramenée à la case praticable la plus proche à deux
// anneaux d'apparition au plus. Rien si aucune n'est assez proche.
[[nodiscard]] std::optional<CellPosition> SurvivorsStartCell(const SurvivorsRules& rules,
                                                             const ArenaRule& arena_rule,
                                                             const SurvivorsGround& ground);
// La même règle sur une carte.
[[nodiscard]] std::optional<CellPosition> SurvivorsStartCell(const SurvivorsRules& rules,
                                                             const ArenaRule& arena_rule,
                                                             const WalkMap& arena);
// LA GRILLE DES VOISINS D'UN SOL GÉNÉRÉ : un carré de ce côté, en cases,
// centré sur le héros à chaque reconstruction. Il couvre le rappel (48 cases)
// de part et d'autre ; un monstre au-delà tombe dans un seau du bord et reste
// trouvable.
inline constexpr std::int32_t kFollowingGridCells = 128;
// L'entrée d'un pas : le cap du stick, kNoHeading à l'arrêt.
struct SurvivorsInput {
  std::uint8_t heading = kNoHeading;
};
// kRaise : en coop, relever le héros `offer_index`. Une feuille part, et la
// montée de niveau avec elle : personne ne reçoit rien à ce niveau.
enum class AnswerKind : std::uint8_t { kPick, kReroll, kSkip, kBanish, kRaise };
struct SurvivorsAnswer {
  AnswerKind kind = AnswerKind::kPick;
  std::uint8_t offer_index = 0;
  [[nodiscard]] bool operator==(const SurvivorsAnswer&) const = default;
};
// CE QU'UN JOUEUR DIT À UN TIC : son cap, et son bulletin s'il en dépose un.
struct SurvivorsTeamInput {
  std::uint8_t heading = kNoHeading;
  std::optional<SurvivorsAnswer> ballot;
  // Le vote pour lequel ce bulletin a été déposé : voir `SurvivorsField::vote_serial`.
  std::uint8_t ballot_vote = 0;
  [[nodiscard]] bool operator==(const SurvivorsTeamInput&) const = default;
};
// Les flux d'aléa, dérivés de la graine : un flux par usage, pour qu'un tirage
// de plus dans l'un ne décale pas les autres.
inline constexpr std::uint64_t kSpawnStream = 1;
inline constexpr std::uint64_t kOfferStream = 2;
inline constexpr std::uint64_t kCombatStream = 3;
// Le butin : l'ouverture des coffres.
inline constexpr std::uint64_t kLootStream = 4;
// Le décor : les plantes, posées par blocs au départ, ou nées au fil des tués
// (plantes du départ comprises).
inline constexpr std::uint64_t kFieldStream = 5;
// La chute des cartes : elle ne décale ni les coffres ni les vagues.
inline constexpr std::uint64_t kCardStream = 6;
// Les cartes au sol se numérotent à part, au-dessus de tout numéro de
// naissance : une carte tombée ne décale pas les numéros des monstres et des
// objets qui naissent après elle.
inline constexpr std::uint32_t kFirstCardSerial = 0x80000000U;

// LA DISTANCE MAXIMALE entre deux héros debout : un héros ne s'éloigne pas
// davantage d'un autre. Elle garde la horde et ses apparitions sur un seul
// théâtre.
inline constexpr std::int32_t kTeamLeashCells = 30;
// Une réserve commune ne dépasse pas ce nombre de charges de chaque sorte :
// le double de ce qu'un solo apporte au rang maximal d'un service Kafra.
inline constexpr std::uint8_t kMostTeamCharges = 6;
// LE VOTE D'UNE MONTÉE DE NIVEAU se clôt à ce nombre de tics au plus : 10 s.
inline constexpr std::uint32_t kVoteTicks = 500;
// La pause commune d'un coffre ouvert en coop, en tics : 2 s.
inline constexpr std::uint32_t kChestHoldTicks = 100;
// L'ÉCHELLE D'UNE ÉQUIPE, en pour cent de la partie solo : l'EXP qu'un niveau
// demande, les PV de la horde, et ses lots et son plancher.
struct SurvivorsTeamScale {
  std::int32_t exp_needed_pct = 100;
  std::int32_t foe_hp_pct = 100;
  std::int32_t batch_pct = 100;
};
inline constexpr std::array<SurvivorsTeamScale, kMostHeroes> kTeamScales{{
    {.exp_needed_pct = 100, .foe_hp_pct = 100, .batch_pct = 100},
    {.exp_needed_pct = 60, .foe_hp_pct = 85, .batch_pct = 110},
    {.exp_needed_pct = 47, .foe_hp_pct = 78, .batch_pct = 110},
    {.exp_needed_pct = 40, .foe_hp_pct = 72, .batch_pct = 118},
}};
// CE QU'UN HÉROS POSSÈDE EN PROPRE, hors de la scène : voir `PutOnStage`.
struct SurvivorsKit {
  SurvivorsHero hero;
  const ClassRule* hero_class = nullptr;
  std::uint8_t lives_left = 0;
  std::vector<SurvivorsShot> shots;
  std::vector<SurvivorsStrike> strikes;
  std::vector<SurvivorsBlast> blasts;
  std::vector<SurvivorsZone> zones;
  std::vector<SurvivorsSummon> summons;
  SurvivorsAllyStart start;
};

// TOUT L'ÉTAT D'UNE PARTIE. Lu par app et par les essais ; écrit par les
// systèmes de src/survivors_systems.h, dans l'ordre de SurvivorsRun::Step.
// `rules` et `hero_class` pointent dans les règles passées à Start, qui doivent
// survivre à la partie.
//
// À PLUSIEURS, UN SEUL HÉROS EST EN SCÈNE : `hero`, `hero_class`, `lives_left`,
// ses coups et la part joueur de `start`. Chaque système agit sur lui, et
// `allies` garde les autres. Entre deux pas, le héros 0 est en scène. Le niveau,
// l'EXP, le Battle Manual et les zenys sont ceux de l'équipe : ils restent dans
// `hero`, et `SyncTeam` les recopie aux autres.
struct SurvivorsField {
  const SurvivorsRules* rules = nullptr;
  const ClassRule* hero_class = nullptr;
  // Les vagues, les boss et les événements de l'arène (`ContentOf`), dans les
  // règles eux aussi : `wave_index`, `next_boss` et `next_event` y comptent.
  ArenaContent content;
  SurvivorsGround ground;
  SurvivorsStart start;
  std::uint32_t step = 0;
  SurvivorsPhase phase = SurvivorsPhase::kRunning;
  SurvivorsHero hero;
  std::vector<SurvivorsKit> allies;
  std::uint8_t stage = 0;        // l'index du héros en scène
  std::uint8_t leaves_left = 0;  // les feuilles d'Yggdrasil de l'équipe
  // Les tics écoulés : ils avancent aussi pendant un vote ou une pause de coffre.
  std::uint32_t ticks = 0;
  // Le vote ouvert : le bulletin de chaque héros, et les tics qu'il lui reste ;
  // 0 : il s'ouvrira au prochain tic.
  std::array<std::optional<SurvivorsAnswer>, kMostHeroes> ballots;
  // LES JOUEURS PARTIS : un robot joue leur héros, mais seuls les humains décident. Ils
  // ne comptent plus dans la majorité.
  std::array<bool, kMostHeroes> voter_gone = {};
  std::uint32_t vote_ticks_left = 0;
  // Le numéro des offres en cours, modulo 256. Un bulletin ne porte qu'un index : sans
  // lui, un clic parti pendant un vote compterait dans le suivant, sur une offre que
  // le joueur n'a pas lue.
  std::uint8_t vote_serial = 0;
  // Les tics de la pause d'un coffre : la partie n'avance pas tant qu'il en reste.
  std::uint32_t hold_ticks_left = 0;
  std::vector<SurvivorsFoe> foes;           // ordre de naissance
  std::vector<SurvivorsShot> shots;         // ordre de naissance
  std::vector<SurvivorsStrike> strikes;     // ordre d'émission
  std::vector<SurvivorsBlast> blasts;       // ordre d'émission
  std::vector<SurvivorsZone> zones;         // ordre de pose
  std::vector<SurvivorsSummon> summons;     // ordre d'invocation
  std::vector<SurvivorsFoeShot> foe_shots;  // ordre de tir
  std::vector<SurvivorsHazard> hazards;     // ordre de pose
  std::vector<SurvivorsGem> gems;           // ordre de naissance
  std::vector<SurvivorsPlant> plants;       // ordre de pose
  // LES CUVES ROMPUES (EventKind::kTanks) : le coin sud-ouest de chacune,
  // dans l'ordre de rupture.
  std::vector<CellPosition> burst_tanks;
  // Au fil des tués : `tally.kills` à la dernière naissance d'une plante (0
  // au départ). Les tués depuis cette naissance sont le compteur des plantes.
  std::uint32_t plant_kills_mark = 0;
  std::vector<SurvivorsPickup> pickups;  // ordre de chute
  std::vector<SurvivorsOffer> offers;
  std::vector<std::uint16_t> banished;
  std::uint8_t rerolls_left = 0;
  std::uint8_t skips_left = 0;
  std::uint8_t banishes_left = 0;
  std::uint8_t lives_left = 0;
  std::uint16_t pending_level_ups = 0;
  std::uint32_t next_serial = 1;
  std::size_t wave_index = 0;
  std::uint32_t wave_spawn_step = 0;
  // Les tirages déjà nés du lot dû (une meute compte pour un) : un lot
  // interrompu ne reprend que son reste.
  std::uint8_t wave_batch_born = 0;
  std::size_t next_boss = 0;
  // Les boss en vie, dans l'ordre de leur naissance.
  std::vector<SurvivorsBossTrack> boss_tracks;
  std::size_t next_event = 0;
  // Les membres déjà placés de l'événement dû : un événement que le plafond
  // arrête ne reprend que son reste.
  std::uint8_t event_members_placed = 0;
  // L'heure de la prochaine élite, de first_ms à last_ms par interval_ms.
  std::uint64_t next_elite_ms = 0;
  // LES PLACES DE NAISSANCE INTROUVABLES, depuis le départ : les essais rejetés
  // d'un point du cadre, et les naissances dont tous les essais l'ont été. Des
  // mesures pour l'outil d'équilibrage ; rien ne les lit pour décider.
  std::uint32_t spawn_tries_failed = 0;
  std::uint32_t spawn_draws_failed = 0;
  // LES PLANTES CASSÉES depuis le départ, par le héros ou par une aire : une
  // mesure pour l'outil d'équilibrage, que rien ne lit pour décider.
  std::uint32_t plants_broken = 0;
  SurvivorsTally tally;
  SurvivorsRecap recap;
  core::Pcg32 spawn_random;
  core::Pcg32 offer_random;
  core::Pcg32 combat_random;
  core::Pcg32 loot_random;
  core::Pcg32 field_random;
  core::Pcg32 card_random;
  // Le prochain numéro d'une carte au sol, à partir de kFirstCardSerial.
  std::uint32_t next_card_serial = kFirstCardSerial;
  // Les espèces de mini-boss et de boss qui ont déjà lâché leur carte garantie.
  std::vector<std::uint16_t> card_guarantees_spent;
  HordeFlow flow;
  SurvivorsGrid foe_grid;
  std::vector<FieldPoint> foe_points;  // tampon de la grille, dans l'ordre de `foes`
  std::vector<std::uint32_t> nearby;   // tampon des requêtes
  // Les tampons du ciblage du plus dense : les voisins d'un candidat, et le
  // compte de chaque candidat, par index de monstre.
  std::vector<std::uint32_t> around;
  std::vector<std::uint32_t> crowd;
  SurvivorsNews news;
};

// Le nombre de héros de la partie.
[[nodiscard]] inline std::size_t HeroCount(const SurvivorsField& field) noexcept {
  return field.allies.size() + 1;
}
// L'échelle de cette équipe.
[[nodiscard]] inline const SurvivorsTeamScale& TeamScaleOf(const SurvivorsField& field) noexcept {
  return kTeamScales[field.allies.size()];
}
// L'EXP que demande le niveau suivant de l'équipe.
[[nodiscard]] std::uint32_t TeamExpToNext(const SurvivorsField& field);
// Le héros `hero_index`, où qu'il soit. Son niveau et son EXP sont à jour entre
// deux pas.
[[nodiscard]] const SurvivorsHero& HeroAt(const SurvivorsField& field, std::size_t hero_index);
// La classe du héros `hero_index`.
[[nodiscard]] const ClassRule* ClassAt(const SurvivorsField& field, std::size_t hero_index);
// Les tirs du héros `hero_index`, pour le dessin.
[[nodiscard]] const std::vector<SurvivorsShot>& ShotsAt(const SurvivorsField& field,
                                                        std::size_t hero_index);
// Les coups du héros `hero_index`, pour le dessin.
[[nodiscard]] const std::vector<SurvivorsZone>& ZonesAt(const SurvivorsField& field,
                                                        std::size_t hero_index);
// Les invocations du héros `hero_index`, pour le dessin.
[[nodiscard]] const std::vector<SurvivorsSummon>& SummonsAt(const SurvivorsField& field,
                                                            std::size_t hero_index);
// MET EN SCÈNE le héros `hero_index` : il échange sa place avec celui qui y est.
// L'échange ne touche ni le niveau, ni l'EXP, ni les zenys de l'équipe.
void PutOnStage(SurvivorsField& field, std::size_t hero_index);
// Recopie aux alliés le niveau, l'EXP et les zenys du héros en scène.
void SyncTeam(SurvivorsField& field);

// Une orbe en vie, pour le dessin.
struct SurvivorsOrb {
  std::uint16_t skill_id = 0;
  FieldPoint position;
  std::uint8_t hero_index = 0;  // le héros dont l'orbite tourne ; 0 en solo
};

// UNE PARTIE DU MODE SURVIVORS : des pas fixes de 20 ms, sans horloge ni
// flottant. Deux parties de mêmes règles, arène, départ et entrées rendent le
// même état, sous MSVC comme sous g++.
class SurvivorsRun {
 public:
  // kInvalidArgument : règles refusées par CheckSurvivorsRules, classe, arène ou
  // premier skill inconnus, premier skill hors des skills de départ, bonus de
  // loadout au-delà de ±kMostStatBonus, Cursed Water au-delà de
  // kMostCursedWater ou d'un rang que les règles n'ont pas, Id verrouillés non
  // strictement croissants, plus de kMostLockedIds, hors des réserves de la
  // classe, ou qui verrouillent le premier skill, sol borné vide ou de plus de
  // kMostArenaSideCells de côté, sol borné pour une arène qui nomme un
  // terrain, sol généré qui n'est pas celui que le rejeu redérive (la règle
  // de terrain de l'arène dans `rules`, à la même adresse, sous `start.seed`),
  // aucune case praticable à moins de spawn_search_ring × 2 du départ. Le
  // départ pose les plantes par blocs d'un sol borné, ou celles du départ des
  // plantes au fil des tués (flux décor), avant le premier pas. Seule la
  // taille du cache d'un sol généré reste au choix de l'appelant.
  [[nodiscard]] static core::Result<SurvivorsRun> Start(const SurvivorsRules& rules,
                                                        SurvivorsGround ground,
                                                        const SurvivorsStart& start);
  // Sur la carte `arena`, un sol borné.
  [[nodiscard]] static core::Result<SurvivorsRun> Start(const SurvivorsRules& rules, WalkMap arena,
                                                        const SurvivorsStart& start);
  // UNE ARÈNE GÉNÉRÉE naît sans installation : son sol est celui de son
  // terrain, sous la graine du départ. kInvalidArgument, en plus, pour une
  // arène inconnue ou qui lit une carte.
  [[nodiscard]] static core::Result<SurvivorsRun> Start(const SurvivorsRules& rules,
                                                        const SurvivorsStart& start);
  // La partie garde l'adresse de ses règles : des règles temporaires la
  // laisseraient pointer dans le vide dès la fin de l'expression.
  static core::Result<SurvivorsRun> Start(const SurvivorsRules&& rules, SurvivorsGround ground,
                                          const SurvivorsStart& start) = delete;
  static core::Result<SurvivorsRun> Start(const SurvivorsRules&& rules, WalkMap arena,
                                          const SurvivorsStart& start) = delete;
  static core::Result<SurvivorsRun> Start(const SurvivorsRules&& rules,
                                          const SurvivorsStart& start) = delete;
  // Un pas de 20 ms. Sans effet hors kRunning.
  void Step(SurvivorsInput input);
  // UN PAS À PLUSIEURS : le cap de chaque héros, dans l'ordre de leur index ; un
  // héros sans cap reste à l'arrêt. Sans effet hors kRunning.
  void StepTeam(std::span<const SurvivorsInput> inputs);
  // UN TIC DE PARTIE COOP, un jeu d'entrées par héros dans l'ordre de leur index :
  // un pas, le tic d'un vote, ou celui d'une pause de coffre. Un bulletin que
  // `BallotValid` refuse est ignoré. Sans effet sur une partie finie.
  void Tick(std::span<const SurvivorsTeamInput> inputs);
  // Le vote ouvert accepte ce bulletin : une offre présente, une charge ou une
  // feuille en réserve, un héros à terre pour une relève.
  [[nodiscard]] bool BallotValid(SurvivorsAnswer ballot) const;
  // Ce joueur a quitté la partie : son bulletin s'efface, et il ne vote plus.
  void MarkVoterGone(std::size_t player);
  // Faux hors kChoosing, pour un index hors des offres, une charge épuisée, ou
  // un bannissement de kHeal ou kZeny. Une relève : faux sans feuille, ou pour
  // un héros debout ou inconnu.
  [[nodiscard]] bool Answer(SurvivorsAnswer answer);
  // La partie s'arrête en kAbandoned, aussi pendant un choix, et son bilan
  // reflète l'état du moment (PV, niveau, monstres vivants), sans compter de
  // pas. Sans effet sur une partie finie.
  void Abandon();
  // Les nouvelles accumulées depuis le dernier appel ; la partie repart à vide.
  [[nodiscard]] SurvivorsNews TakeNews();
  [[nodiscard]] const SurvivorsField& field() const noexcept { return field_; }
  [[nodiscard]] const SurvivorsTally& tally() const noexcept { return field_.tally; }
  [[nodiscard]] std::uint32_t elapsed_ms() const noexcept { return field_.step * kSurvivorsStepMs; }
  // Les orbes du dernier pas exécuté (`step − 1`, ou le pas 0 avant le
  // premier), dans `out` (vidé), pour le dessin : même calcul que la
  // simulation, au pas dont les monstres et les coups sont visibles. Une
  // orbite acquise ou montée pendant un choix n'en montre aucune avant le pas
  // suivant.
  void Orbs(std::vector<SurvivorsOrb>& out) const;

  // ── POUR LES ESSAIS ──────────────────────────────────────────────────────
  // L'état, en écriture : un essai y pose des monstres, des gemmes, des PV, puis
  // fait avancer la partie. Le jeu ne s'en sert pas ; une partie retouchée ainsi
  // ne se rejoue plus depuis son journal.
  [[nodiscard]] SurvivorsField& field_for_tests() noexcept { return field_; }

  // Le robot d'un héros lit le héros en scène : son appelant le met en scène,
  // lit, puis y remet le héros 0.
  void PutHeroOnStage(std::size_t hero_index) { PutOnStage(field_, hero_index); }

 private:
  explicit SurvivorsRun(SurvivorsField field);
  SurvivorsField field_;
};

}  // namespace ro::sim
