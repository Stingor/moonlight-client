#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_week.h"

#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LE CARNET EN JEU, sans disque ────────────────────────────────────────────
//
// La partie se nourrit du carnet (son départ) et le nourrit (son crédit). Tout
// se calcule ici sur un `SurvivorsSave` en mémoire ; `SurvivorsSaveFile` relit
// et écrit autour.

// UN DÉBLOCAGE (Q5) : un skill ou une carte de la réserve de la classe, retiré
// des parties libres tant que sa condition n'est pas remplie. Le défi et la
// partie classée partent avec tout (départ neutre).
enum class UnlockCondition : std::uint8_t {
  kCardFound,  // la carte de même clé est dans l'Album
  kFeat,       // l'exploit de même clé est atteint
};
struct SurvivorsUnlock {
  const char* key = "";  // clé du skill ou de la carte, et de sa condition
  UnlockCondition condition = UnlockCondition::kCardFound;
};

// UN EXPLOIT : atteint, et gardé dans l'Album, dès que son compteur au cumul
// touche son seuil.
enum class FeatCounter : std::uint8_t {
  kBestSurvivedMs,  // la plus longue partie survécue
  kChills,          // les glaçages au cumul
  kWindKills,       // les monstres Wind vaincus au cumul
  kBestLevel,       // le plus haut niveau atteint en une partie
  kFireKills,       // les monstres Fire vaincus au cumul
  kMet,             // une rencontre : 1 une fois faite, sans compteur au carnet
};
struct SurvivorsFeat {
  const char* key = "";
  FeatCounter counter = FeatCounter::kBestSurvivedMs;
  std::uint64_t threshold = 0;
};

// LES EXPLOITS DES RENCONTRES : rejoindre le Voleur sur la prairie, l'Archer dans
// le désert, le Marchand sur la neige. Chacun débloque sa classe, et s'inscrit à
// l'instant par `InscribeFeat`, sans attendre la fin de la partie : voir
// `survivors_meeting.h`.
inline constexpr std::string_view kMeetingFeat = "thief_met";
inline constexpr std::string_view kArcherMeetingFeat = "archer_met";
inline constexpr std::string_view kMerchantMeetingFeat = "merchant_met";

// Les tables livrées : Golem et Mastering Card, et les cartes d'Eclipse, de
// Dragon Fly, de Vagabond Wolf et de Baphomet, jusqu'à leur première chute,
// Sight jusqu'à une partie survécue 5:00, Frost Diver jusqu'à 500 glaçages,
// Earth Spike jusqu'à 300 monstres Wind vaincus, Thunderstorm jusqu'au
// niveau 25 dans une partie, Water Ball jusqu'à 500 monstres Fire vaincus,
// Ice Wall jusqu'à une partie survécue 10:00, et l'exploit de la rencontre.
[[nodiscard]] std::span<const SurvivorsUnlock> ShippedSurvivorsUnlocks();
[[nodiscard]] std::span<const SurvivorsFeat> ShippedSurvivorsFeats();

[[nodiscard]] bool IsUnlocked(const SurvivorsSave& save, const SurvivorsUnlock& unlock);

// Où en est un exploit : son compteur au cumul, et s'il est inscrit.
[[nodiscard]] std::uint64_t FeatProgressOf(const SurvivorsSave& save, const SurvivorsFeat& feat);
[[nodiscard]] bool FeatReached(const SurvivorsSave& save, const SurvivorsFeat& feat);
// INSCRIT UN EXPLOIT au carnet. Vrai quand il n'y était pas.
bool InscribeFeat(SurvivorsSave& save, std::string_view key);

// Le tableau de ces clés ; nul quand le carnet n'en a pas.
[[nodiscard]] const SurvivorsBoard* FindBoard(const SurvivorsSave& save, std::string_view class_key,
                                              std::string_view arena_key,
                                              std::string_view board_key,
                                              const ro::sim::IsoWeek& week);

// Les Id de la réserve de `class_rule` que le carnet n'a pas encore
// débloqués, strictement croissants : les `locked_ids` d'une partie libre.
[[nodiscard]] std::vector<std::uint16_t> LockedIdsOf(const ro::sim::SurvivorsRules& rules,
                                                     const ro::sim::ClassRule& class_rule,
                                                     const SurvivorsSave& save);

// L'ARÈNE PAR DÉFAUT : celle que la partie libre joue tant que l'accueil n'en a
// pas retenu d'autre, la première arène classée — celle du défi et de la
// partie classée (Q6, Q7). Nulle quand aucune ne l'est.
[[nodiscard]] const ro::sim::ArenaRule* DefaultArenaOf(const ro::sim::SurvivorsRules& rules);

// L'ARÈNE DE LA PARTIE LIBRE : celle d'Id `arena_id` quand les règles la
// connaissent, sinon l'arène par défaut. Toute arène du catalogue se joue en
// partie libre, classée ou non.
[[nodiscard]] const ro::sim::ArenaRule* FreeArenaOf(const ro::sim::SurvivorsRules& rules,
                                                    std::uint16_t arena_id);

// UNE CLASSE DÉBLOQUÉE (Q6 du lot 12) : la première l'est toujours ; le
// Chevalier, par le même exploit qu'Ice Wall, une partie tenue 10:00 ; le Voleur,
// par sa rencontre.
[[nodiscard]] bool ClassUnlocked(const ro::sim::SurvivorsRules& rules,
                                 const ro::sim::ClassRule& class_rule, const SurvivorsSave& save);
// LE NOM D'UNE CLASSE, dans la langue du joueur, distinct de celui de son métier
// (le Chevalier joue un Épéiste) ; nul pour une classe qui n'en a pas.
[[nodiscard]] const char* ClassTitleOf(const ro::sim::ClassRule& class_rule);

// La clé de l'exploit qui débloque `class_rule`, vide pour une classe ouverte
// d'emblée.
[[nodiscard]] std::string_view ClassUnlockFeat(const ro::sim::ClassRule& class_rule);

// LA CLASSE MONTRÉE À L'ACCUEIL : celle d'Id `class_id` quand les règles la
// connaissent et que le carnet l'a débloquée, sinon la première, cochée à sa
// place. Nulle sans classe.
[[nodiscard]] const ro::sim::ClassRule* ChosenClassOf(const ro::sim::SurvivorsRules& rules,
                                                      const SurvivorsSave& save,
                                                      std::uint16_t class_id);

// LE DÉPART D'UNE PARTIE LIBRE : la classe `ChosenClassOf(class_id)`, l'arène
// `FreeArenaOf(arena_id)`, le loadout et les vies des rangs achetés, la
// dernière Cursed Water choisie, et les verrous du carnet. Le héros part de son
// auto-attaque seule. La graine se tire par app à chaque partie.
[[nodiscard]] ro::sim::SurvivorsStart FreeStartOf(const ro::sim::SurvivorsRules& rules,
                                                  const SurvivorsSave& save, std::uint64_t seed,
                                                  std::uint16_t arena_id = 0,
                                                  std::uint16_t class_id = 0);

// La semaine ISO d'un instant UTC : le défi change le lundi à 00:00 UTC (Q9).
[[nodiscard]] ro::sim::IsoWeek IsoWeekOfUtc(std::int64_t utc_s) noexcept;

// LE SKILL IMPOSÉ PAR LA SEMAINE : l'un des `ro::sim::OpeningSkills`, de
// n'importe quelle classe, tiré par `SeedOfWeek` ; le même pour toutes les
// classes. 0 sans semaine ou sans skill.
[[nodiscard]] std::uint16_t ChallengeFirstSkill(const ro::sim::SurvivorsRules& rules,
                                                const ro::sim::IsoWeek& week);

// LE DÉPART DU DÉFI D'UNE SEMAINE pour la classe `class_id` : neutre, sur la
// première arène classée, la graine `SeedOfWeek` et le skill que la semaine
// impose. Le même pour tous les carnets, et la même graine pour toutes les
// classes, chacune à son tableau. Un Id que les règles ne connaissent pas reste
// tel quel, et la partie refuse de commencer.
[[nodiscard]] ro::sim::SurvivorsStart ChallengeStartOf(const ro::sim::SurvivorsRules& rules,
                                                       const ro::sim::IsoWeek& week,
                                                       std::uint16_t class_id);

// Ce que devient une partie au tableau des records.
enum class RecordVerdict : std::uint8_t {
  kPlaced,       // inscrite, à `record_rank`
  kBelowBoard,   // hors des dix meilleures
  kAbandoned,    // abandonnée, interrompue ou d'équipe : jamais aux records (Q10)
  kNotNeutral,   // un défi ou une partie classée joués avec la méta : refusés
  kNotThisWeek,  // un défi dont la graine ou le bolt n'est pas celui de sa semaine
};

// CE QUE LA PARTIE A AJOUTÉ AU CARNET, pour l'écran de fin.
struct SurvivorsCredit {
  std::uint64_t zeny = 0;          // versés au solde, Cursed Water comprise
  std::uint64_t zeny_balance = 0;  // le solde après la partie
  std::uint64_t score = 0;         // le score rangé : `FreeRunScore`
  // Les cartes entrées dans l'Album pour la première fois, par Id d'objet,
  // dans l'ordre de ramassage.
  std::vector<std::uint16_t> new_cards;
  std::vector<std::string> new_feats;  // les clés des exploits atteints
  RecordVerdict record = RecordVerdict::kBelowBoard;
  std::optional<std::size_t> record_rank;  // 0 en tête
  // Hors du tableau : le score de sa dernière ligne, celui qu'il fallait battre.
  std::optional<std::uint64_t> score_to_beat;
  // L'instant du crédit, que le tableau garde comme date de la partie.
  std::int64_t played_at_utc_s = 0;
  [[nodiscard]] bool operator==(const SurvivorsCredit&) const = default;
};

// PARTIR SANS MINIMAP (cahier « lisibilité », Q4) majore les Zeny de la
// partie libre d'autant, après la Cursed Water.
inline constexpr std::uint64_t kWithoutMinimapZenyPct = 15;
[[nodiscard]] std::uint64_t WithoutMinimapZeny(std::uint64_t zeny, bool without_minimap) noexcept;

// CRÉDITE LE CARNET D'UNE PARTIE FINIE, ou interrompue. Toute partie verse ses
// Zeny (`RunZeny`, majorés par la Cursed Water du départ, puis par une
// partie sans minimap) et ses cartes
// ramassées, défi, abandon et liaison perdue compris (Q10) ; ses compteurs
// s'ajoutent, et les exploits qu'ils atteignent s'inscrivent. Seule une partie
// qui n'est pas abandonnée entre aux records, au tableau de `ending.board` ; au
// défi, il faut de plus un départ neutre sur la graine et le bolt de sa
// semaine, et à la partie classée un départ neutre (Q8). Un tableau de défi
// d'une autre semaine que celle de `now_utc_s` ne garde que sa meilleure
// partie (Q11).
SurvivorsCredit CreditRun(const ro::sim::SurvivorsRules& rules, SurvivorsSave& save,
                          const SurvivorsEnding& ending, const ro::sim::SurvivorsStart& start,
                          std::int64_t now_utc_s);

// CE QU'UNE PARTIE A VU ENTRE AU CODEX DU CARNET, quelle que soit son issue :
// ajouté dans l'ordre du relevé, sans doublon, et rien n'est jamais retiré.
// Vrai quand le carnet a changé.
bool CreditSightings(SurvivorsSave& save, const SurvivorsCodexSeen& sightings);

}  // namespace moonclient
