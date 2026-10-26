#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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
[[nodiscard]] inline bool LoadoutWithinBounds(const SurvivorsLoadout& loadout) noexcept {
  for (const std::int32_t bonus : loadout.bonus.values) {
    if (bonus < -kMostStatBonus || bonus > kMostStatBonus) return false;
  }
  return true;
}
// Tout ce qui fixe une partie avant son premier pas : avec les règles et
// l'arène, de quoi la rejouer à l'identique.
// Les Id qu'un départ peut verrouiller : une borne du journal, au-delà de la
// taille de toute réserve de classe.
inline constexpr std::size_t kMostLockedIds = 64;
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
  [[nodiscard]] bool operator==(const SurvivorsStart&) const = default;
};
// LE DÉPART NEUTRE, celui du défi et de la partie classée : aucun bonus,
// aucune charge, aucune vie, aucune Cursed Water, rien de verrouillé. Tout le
// monde y joue la même partie.
[[nodiscard]] inline bool IsNeutralStart(const SurvivorsStart& start) noexcept {
  return start.loadout == SurvivorsLoadout{} && start.lives == 0 && start.cursed_water == 0 &&
         start.locked_ids.empty();
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
// LA CLÉ D'UN MORCEAU DU MONDE : ligne puis colonne, dans [0,
// kTerrainWorldChunks²). Des clés croissantes vont du sud au nord, puis
// d'ouest en est.
[[nodiscard]] constexpr std::uint32_t PlantChunkKey(std::int32_t chunk_x,
                                                    std::int32_t chunk_y) noexcept {
  return static_cast<std::uint32_t>(chunk_y) * static_cast<std::uint32_t>(kTerrainWorldChunks) +
         static_cast<std::uint32_t>(chunk_x);
}
// L'entrée d'un pas : le cap du stick, kNoHeading à l'arrêt.
struct SurvivorsInput {
  std::uint8_t heading = kNoHeading;
};
enum class AnswerKind : std::uint8_t { kPick, kReroll, kSkip, kBanish };
struct SurvivorsAnswer {
  AnswerKind kind = AnswerKind::kPick;
  std::uint8_t offer_index = 0;
  [[nodiscard]] bool operator==(const SurvivorsAnswer&) const = default;
};
// Les flux d'aléa, dérivés de la graine : un flux par usage, pour qu'un tirage
// de plus dans l'un ne décale pas les autres.
inline constexpr std::uint64_t kSpawnStream = 1;
inline constexpr std::uint64_t kOfferStream = 2;
inline constexpr std::uint64_t kCombatStream = 3;
// Le butin : l'ouverture des coffres.
inline constexpr std::uint64_t kLootStream = 4;
// Le décor : les plantes posées au départ. Sur un sol généré, chaque morceau
// tire ses plantes dans ce flux, sous sa propre graine (TerrainPlantSeed).
inline constexpr std::uint64_t kFieldStream = 5;
// La chute des cartes : elle ne décale ni les coffres ni les vagues.
inline constexpr std::uint64_t kCardStream = 6;
// Les cartes au sol se numérotent à part, au-dessus de tout numéro de
// naissance : une carte tombée ne décale pas les numéros des monstres et des
// objets qui naissent après elle.
inline constexpr std::uint32_t kFirstCardSerial = 0x80000000U;

// TOUT L'ÉTAT D'UNE PARTIE. Lu par app et par les essais ; écrit par les
// systèmes de src/survivors_systems.h, dans l'ordre de SurvivorsRun::Step.
// `rules` et `hero_class` pointent dans les règles passées à Start, qui doivent
// survivre à la partie.
struct SurvivorsField {
  const SurvivorsRules* rules = nullptr;
  const ClassRule* hero_class = nullptr;
  SurvivorsGround ground;
  SurvivorsStart start;
  std::uint32_t step = 0;
  SurvivorsPhase phase = SurvivorsPhase::kRunning;
  SurvivorsHero hero;
  std::vector<SurvivorsFoe> foes;           // ordre de naissance
  std::vector<SurvivorsShot> shots;         // ordre de naissance
  std::vector<SurvivorsStrike> strikes;     // ordre d'émission
  std::vector<SurvivorsBlast> blasts;       // ordre d'émission
  std::vector<SurvivorsZone> zones;         // ordre de pose
  std::vector<SurvivorsFoeShot> foe_shots;  // ordre de tir
  std::vector<SurvivorsHazard> hazards;     // ordre de pose
  std::vector<SurvivorsGem> gems;           // ordre de naissance
  std::vector<SurvivorsPlant> plants;       // ordre de pose
  // Sur un sol généré, les morceaux déjà éveillés, par clé strictement
  // croissante (PlantChunkKey) : un morceau ne pose ses plantes qu'une fois.
  std::vector<std::uint32_t> woken_chunks;
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

// Une orbe en vie, pour le dessin.
struct SurvivorsOrb {
  std::uint16_t skill_id = 0;
  FieldPoint position;
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
  // terrain, aucune case praticable à moins de spawn_search_ring × 2 du
  // départ. Le départ pose les plantes d'un sol borné (flux décor), ou éveille
  // les morceaux d'un sol généré autour du héros, avant le premier pas. Un sol
  // généré se prend tel quel, même pour une arène qui lit une carte : les
  // essais y choisissent la taille du cache.
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
  // Faux hors kChoosing, pour un index hors des offres, une charge épuisée, ou
  // un bannissement de kHeal ou kZeny.
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

 private:
  explicit SurvivorsRun(SurvivorsField field);
  SurvivorsField field_;
};

}  // namespace ro::sim
