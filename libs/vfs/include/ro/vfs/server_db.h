#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// ── LA BASE DE DONNÉES DU SERVEUR, POUR LA FENÊTRE « BASE DE DONNÉES » ──────
//
// Trois tables exportées par `scripts/yaml_tables/server_db.py`, qui en dit les
// règles (ADR 0116) :
//
//   item_db.yml         une item_db rAthena, FUSIONNÉE comme le serveur la
//                       charge ; `Buy` et `Sell` sont les prix qu'il retient ;
//   mob_db.yml          une mob_db rAthena fusionnée, butins au taux ÉCRIT ;
//   mob_db_context.yml  les réglages de butin effectifs, les exceptions de
//                       `mob_item_ratio`, les cartes d'apparition, et les
//                       nombres que le serveur déduit de chaque monstre.
//
// 🔴 LES DEUX BASES SE PARCOURENT SANS ANALYSEUR YAML : leur script les écrit
// sous une forme stricte (`ro/formats/rathena_db_text.h`), qu'un curseur lit
// ligne à ligne. Seul le petit contexte passe par yaml-cpp. Elles ne se lisent
// qu'à la première ouverture de la fenêtre, jamais au démarrage.
//
// 🔴 UNE TABLE ABSENTE NE DIT RIEN : `known()` est faux, et la fenêtre le dit.

inline constexpr std::string_view kServerItemDbPath = "data\\yaml_files\\item_db.yml";
inline constexpr std::string_view kServerMobDbPath = "data\\yaml_files\\mob_db.yml";
inline constexpr std::string_view kServerDbContextPath = "data\\yaml_files\\mob_db_context.yml";

// Les centièmes de pour cent d'un taux qui tombe à coup sûr.
inline constexpr std::uint32_t kDropRateWhole = 10000;

// UN OBJET DE L'ITEM_DB. Les textes — type, emplacements, métiers — sont ceux
// de la base, en anglais, que l'appelant traduit.
struct ServerItem {
  std::uint32_t item_id = 0;
  std::string aegis_name;
  std::string name;
  // « Etc » quand la base ne le dit pas : le serveur en fait un objet divers.
  std::string type;
  std::string sub_type;
  std::uint32_t buy_zeny = 0;
  std::uint32_t sell_zeny = 0;
  std::uint32_t weight_tenths = 0;
  std::uint32_t attack = 0;
  std::uint32_t magic_attack = 0;
  std::uint32_t defense = 0;
  std::uint16_t range_cells = 0;
  std::uint16_t slots = 0;
  // Les métiers, les classes et les emplacements LEVÉS, dans l'ordre écrit.
  std::vector<std::string> jobs;
  std::vector<std::string> classes;
  std::vector<std::string> locations;
  std::string gender;
  std::uint16_t weapon_level = 0;
  std::uint16_t armor_level = 0;
  // Zéro : la base ne borne pas.
  std::uint16_t equip_level_min = 0;
  std::uint16_t equip_level_max = 0;
  bool refineable = false;
  std::uint32_t view = 0;
  std::string script;
  std::string equip_script;
  std::string unequip_script;
};

// UNE LIGNE DE BUTIN, au taux ÉCRIT dans la base, en centièmes de pour cent.
struct ServerDrop {
  std::uint32_t item_id = 0;
  std::uint32_t written_rate = 0;
  bool steal_protected = false;
};

// Le rang d'un monstre, celui de la fiche du serveur.
inline constexpr std::uint8_t kServerMonsterOrdinary = 0;
inline constexpr std::uint8_t kServerMonsterBoss = 1;
inline constexpr std::uint8_t kServerMonsterMvp = 2;

struct ServerMonster {
  std::uint32_t monster_id = 0;
  std::string aegis_name;
  // Le nom que le serveur MONTRE, selon `override_mob_names`.
  std::string name;
  std::uint16_t level = 1;
  std::uint32_t hp = 1;
  std::uint32_t sp = 1;
  std::uint32_t base_exp = 0;
  std::uint32_t job_exp = 0;
  std::uint32_t mvp_exp = 0;
  std::uint32_t attack = 0;
  std::uint32_t attack2 = 0;
  std::uint32_t defense = 0;
  std::uint32_t magic_defense = 0;
  std::uint16_t str = 1;
  std::uint16_t agi = 1;
  std::uint16_t vit = 1;
  std::uint16_t int_stat = 1;
  std::uint16_t dex = 1;
  std::uint16_t luk = 1;
  std::uint16_t attack_range_cells = 0;
  // Le temps de marche d'une case ; zéro quand la base ne le dit pas.
  std::uint16_t walk_ms_per_cell = 0;
  // ── LES NOMBRES QUE LE SERVEUR DÉDUIT, ceux de sa fiche ──────────────────
  std::uint8_t race = 0;
  std::uint8_t element = 0;
  std::uint8_t element_level = 1;
  std::uint8_t size = 0;
  std::uint8_t rank = kServerMonsterOrdinary;
  std::uint32_t mode = 0;
  // Un coffre au trésor (`RaceGroups: Treasure`) : ses taux sont à part.
  bool treasure = false;
  std::vector<ServerDrop> drops;
  std::vector<ServerDrop> mvp_drops;
};

struct ServerSpawn {
  std::string map_name;
  std::uint32_t count = 0;
};

// LES RÉGLAGES DE BUTIN EFFECTIFS DU SERVEUR, ceux de `conf/battle/drops.conf`
// après ses imports. Taux en pour cent (100 = tel quel), bornes en centièmes
// de pour cent.
struct DropRateFamily {
  std::int64_t rate = 100;
  std::int64_t rate_boss = 100;
  std::int64_t rate_mvp = 100;
  std::int64_t least = 1;
  std::int64_t most = kDropRateWhole;
  // Les bornes d'un mini-boss et d'un MVP, quand la famille en a de propres.
  std::int64_t least_boss = 1;
  std::int64_t most_boss = kDropRateWhole;
  std::int64_t least_mvp = 1;
  std::int64_t most_mvp = kDropRateWhole;
};

struct DropSettings {
  DropRateFamily common;
  DropRateFamily heal;
  DropRateFamily use;
  DropRateFamily equip;
  DropRateFamily card;
  // La récompense de MVP et le coffre au trésor n'ont qu'un taux.
  std::int64_t rate_mvp_reward = 100;
  std::int64_t least_mvp_reward = 1;
  std::int64_t most_mvp_reward = kDropRateWhole;
  std::int64_t rate_treasure = 100;
  std::int64_t least_treasure = 1;
  std::int64_t most_treasure = kDropRateWhole;
  bool logarithmic = false;
  // Un taux peut-il tomber à zéro ? Sinon il garde un centième de pour cent.
  bool rate_zero_allowed = false;
  // Un taux sous 50 % gagne un centième avant tout le reste.
  bool rate_increase = false;
  // L'EXP en pour cent.
  std::int64_t base_exp_rate = 100;
  std::int64_t job_exp_rate = 100;
  std::int64_t mvp_exp_rate = 100;
};

// UN MONSTRE QUI LÂCHE UN OBJET, au taux RÉEL.
struct ItemDropper {
  std::uint32_t monster_id = 0;
  std::uint32_t rate_hundredths = 0;
  bool mvp_reward = false;
};

class ServerDb {
 public:
  ServerDb() = default;

  // Une base lue en entier : objets et monstres se rangent par identifiant, et
  // l'index des butins se construit.
  [[nodiscard]] static ServerDb Known(std::vector<ServerItem> items,
                                      std::vector<ServerMonster> monsters, DropSettings settings);

  [[nodiscard]] bool known() const noexcept { return known_; }
  [[nodiscard]] std::span<const ServerItem> items() const noexcept { return items_; }
  [[nodiscard]] std::span<const ServerMonster> monsters() const noexcept { return monsters_; }
  [[nodiscard]] const DropSettings& settings() const noexcept { return settings_; }
  [[nodiscard]] const ServerItem* FindItem(std::uint32_t item_id) const;
  [[nodiscard]] const ServerMonster* FindMonster(std::uint32_t monster_id) const;

  // Les cartes d'apparition, du plus peuplé au moins peuplé.
  [[nodiscard]] std::span<const ServerSpawn> SpawnsOf(std::uint32_t monster_id) const;
  void NoteSpawns(std::uint32_t monster_id, std::vector<ServerSpawn> spawns);
  // L'exception de `mob_item_ratio` : ce ratio remplace celui de la famille,
  // pour les monstres nommés (tous si la liste est vide).
  void NoteRatio(std::uint32_t item_id, std::int64_t ratio, std::vector<std::uint32_t> monsters);

  // LE TAUX RÉEL d'une ligne de butin, en centièmes de pour cent : ce que le
  // serveur retient au chargement puis tire sans bonus de joueur. Zéro : le
  // serveur retire la ligne.
  [[nodiscard]] std::uint32_t RateOf(const ServerMonster& monster, const ServerDrop& drop,
                                     bool mvp_reward) const;
  // Les monstres qui lâchent cet objet, au taux réel décroissant.
  [[nodiscard]] std::vector<ItemDropper> DroppersOf(std::uint32_t item_id) const;

 private:
  struct Ratio {
    std::int64_t ratio = 0;
    std::vector<std::uint32_t> monsters;
  };
  struct DropRef {
    std::size_t monster = 0;
    std::size_t drop = 0;
    bool mvp_reward = false;
  };

  bool known_ = false;
  std::vector<ServerItem> items_;
  std::vector<ServerMonster> monsters_;
  DropSettings settings_;
  std::unordered_map<std::uint32_t, std::vector<ServerSpawn>> spawns_;
  std::unordered_map<std::uint32_t, Ratio> ratios_;
  std::unordered_map<std::uint32_t, std::vector<DropRef>> droppers_;
};

// ── LE TAUX, COMME LE SERVEUR ───────────────────────────────────────────────
//
// `mob_drop_ratio_adjust` au chargement, puis `mob_getdroprate` sans joueur :
//   1. un taux sous 50 % gagne un centième si `drop_rateincrease` (butin
//      ordinaire seulement) ;
//   2. le ratio est celui de la famille de l'objet — soin, usage, équipement,
//      carte, divers —, selon que le monstre est MVP, mini-boss ou ordinaire ;
//      un coffre au trésor a le sien, une récompense de MVP aussi ; une
//      exception de `mob_item_ratio` le remplace, et un ratio nul retire la
//      ligne ;
//   3. `taux × ratio / 100`, ou la formule logarithmique, borné entre le
//      minimum et le maximum de la famille ;
//   4. au tirage : jamais plus de 100 %, et jamais moins d'un centième de pour
//      cent sauf `drop_rate0item`.
// `item_type` est le `Type` de la base, sans la casse.
[[nodiscard]] std::uint32_t EffectiveDropRate(const DropSettings& settings,
                                              std::uint32_t written_rate,
                                              std::string_view item_type, std::uint8_t rank,
                                              bool treasure, bool mvp_reward,
                                              const std::int64_t* ratio_override);

// L'EXP RÉELLE d'un monstre, comme le serveur la retient au chargement :
// `écrite × taux / 100`, bornée entre zéro et le plus grand nombre d'EXP. Le
// taux est celui de la base, du job ou du MVP, en pour cent.
[[nodiscard]] std::uint64_t EffectiveExp(std::uint64_t written, std::int64_t rate_percent);

// Lit les trois tables depuis leur texte. Refuse un texte qui n'est pas du
// YAML, une en-tête qui n'est pas celle de la base attendue, ou un contexte
// d'une version inconnue.
[[nodiscard]] core::Result<ServerDb> ParseServerDb(std::string_view item_db_text,
                                                   std::string_view mob_db_text,
                                                   std::string_view context_text);

// Les trois textes de l'installation. ABSENTS, des textes vides : `ParseServerDb`
// rend alors une base inconnue.
struct ServerDbTexts {
  std::string item_db;
  std::string mob_db;
  std::string context;
};
[[nodiscard]] core::Result<ServerDbTexts> ReadServerDbTexts(Vfs& vfs);

}  // namespace ro::vfs
