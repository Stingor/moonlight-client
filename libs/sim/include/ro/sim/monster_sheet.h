#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ro::sim {

// ── LA FICHE D'UN MONSTRE, TELLE QUE LE SERVEUR LA DONNE ────────────────────
//
// 🔴 LE CLIENT NE SAIT RIEN D'UN MONSTRE. Il n'a pas `mob_db` : ni son nom, ni
// ses statistiques, ni son butin, ni ses cartes d'apparition. Tout ce qui suit
// arrive par `CZ 0x0F1F -> ZC 0x0F20`, sur demande, et seulement pour un client
// qui a donné sa poignée de main.
//
// ⚠ LE SKILL SENSE NE SUFFIT PAS. Son paquet (`ZC 0x018C`) ne porte que neuf
// champs, neuf résistances et AUCUN nom ; le jeu d'origine va chercher celui-ci
// dans ce que l'envoi du skill a retenu, si bien qu'un Sense lancé par un
// coéquipier affiche le bon monstre sous le mauvais nom. Son relevé reste
// précieux pour une autre raison : il décrit l'exemplaire RÉELLEMENT croisé,
// qui peut différer de la base.

// Ce que le monstre laisse tomber.
struct MonsterDrop {
  std::uint32_t item_id = 0;
  // En centièmes de pour cent, déjà ajusté par le serveur : le même chiffre
  // que la fiche d'objet et que le bestiaire du site.
  std::uint32_t rate_hundredths = 0;
  // Vrai pour une récompense de MVP, faux pour un butin ordinaire.
  bool mvp_reward = false;
  // Le nom que le serveur donne à l'objet, en UTF-8 ; vide s'il n'en a pas.
  std::string name;
};

// Une carte où le monstre apparaît en permanence.
//
// ⚠ SEULS LES SPAWNS PERMANENTS : les invocations de script, les instances et
// les monstres d'une quête n'y figurent pas.
struct MonsterSpawn {
  std::uint16_t count = 0;
  std::string map_name;
};

// Une compétence que le monstre lance.
struct MonsterSkill {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
  // 🔴 LE NOM DU SERVEUR, en repli : l'installation ne nomme que les
  // compétences de JOUEUR, et l'essentiel de l'arsenal d'un monstre — les
  // `NPC_*` — n'existe que dans la table du serveur.
  std::string server_name;
};

// CE QUE CE MONSTRE RAPPORTE AU PERSONNAGE QUI REGARDE.
//
// 🔴 UN INSTANTANÉ, CALCULÉ PAR LE SERVEUR : malus de haut niveau, bonus d'EXP
// par race et par classe, manuel de combat, VIP, événement. Il ne vaut que pour
// l'état du personnage au moment de la demande, et vieillit dès qu'il change de
// niveau.
struct MonsterExpGain {
  std::uint32_t base_exp = 0;
  std::uint32_t job_exp = 0;
  // Ce qu'il faut pour le niveau suivant, dans chacune des deux barres.
  std::uint32_t next_base_exp = 0;
  std::uint32_t next_job_exp = 0;
  bool base_level_maxed = false;
  bool job_level_maxed = false;
};

// Le nombre de résistances élémentaires : une par élément d'ATTAQUE, dans
// l'ordre des constantes `ELE_*` du serveur.
inline constexpr std::size_t kMonsterResistCount = 10;

struct MonsterSheet {
  // La classe de SPRITE, celle qui dessine le monstre. ⚠ PAS FORCÉMENT SON
  // IDENTIFIANT : un monstre déguisé porte l'apparence d'un autre.
  std::uint32_t sprite_class = 0;
  std::uint16_t level = 0;
  std::uint32_t hp = 0;
  std::uint32_t sp = 0;
  // L'EXP de la base de données, la même pour tout le monde.
  std::uint32_t base_exp = 0;
  std::uint32_t job_exp = 0;
  std::uint32_t mvp_exp = 0;
  std::uint16_t atk_min = 0;
  std::uint16_t atk_max = 0;
  std::uint16_t matk_min = 0;
  std::uint16_t matk_max = 0;
  std::uint16_t def = 0;
  std::uint16_t def_bonus = 0;
  std::uint16_t mdef = 0;
  std::uint16_t mdef_bonus = 0;
  std::uint16_t str = 0;
  std::uint16_t agi = 0;
  std::uint16_t vit = 0;
  std::uint16_t int_stat = 0;
  std::uint16_t dex = 0;
  std::uint16_t luk = 0;
  std::uint16_t attack_range_cells = 0;
  std::uint8_t size = 0;
  std::uint8_t race = 0;
  std::uint8_t element = 0;
  std::uint8_t element_level = 0;
  // 0 ordinaire, 1 mini-boss, 2 MVP.
  std::uint8_t boss_rank = 0;
  std::uint8_t mob_class = 0;
  // Les bits de comportement du serveur : agressif, boss, détecteur…
  std::uint32_t mode = 0;
  // Le temps de marche d'UNE case : plus petit, plus rapide.
  std::uint16_t walk_ms_per_cell = 0;
  // Ce que le monstre ENCAISSE par élément d'attaque, en pour cent. ⚠ SIGNÉ :
  // en dessous de zéro, il s'en soigne.
  std::array<std::int16_t, kMonsterResistCount> resist_percent{};
  // Le nom affiché, en UTF-8.
  std::string name;
  std::vector<MonsterDrop> drops;
  std::vector<MonsterSpawn> spawns;
  std::vector<MonsterSkill> skills;

  // ── DE QUEL MONSTRE CETTE FICHE PARLE ─────────────────────────────────────
  //
  // 🔴 DES FAITS DU SERVEUR, JAMAIS UNE DÉDUCTION. Des dizaines de monstres
  // partagent le nom et l'apparence d'un autre ; le préfixe d'un nom interne
  // n'en dit rien de sûr, et l'absence de butin non plus. Vides quand le
  // serveur ne les envoie pas.
  std::string aegis_name;
  // Cible d'une invocation de monstre par un autre.
  bool summoned = false;
  // Combien de monstres portent ce nom affiché, lui compris.
  std::uint8_t namesake_count = 1;
  // Le plus petit identifiant qui le porte ; zéro quand le serveur ne le dit pas.
  std::uint32_t namesake_origin = 0;

  // Absente d'un serveur qui ne la calcule pas.
  std::optional<MonsterExpGain> exp_gain;
};

// Le nombre de résistances du relevé de Sense : le neutre n'y est pas.
inline constexpr std::size_t kSenseResistCount = 9;

// CE QUE LE SKILL SENSE A RELEVÉ SUR UN EXEMPLAIRE.
//
// ⚠ PAS D'IDENTIFIANT DE BASE ICI : le paquet ne porte qu'une classe de sprite.
struct SenseReading {
  std::uint16_t sprite_class = 0;
  std::uint16_t level = 0;
  std::uint16_t size = 0;
  std::uint32_t hp = 0;
  std::int16_t def = 0;
  std::uint16_t race = 0;
  std::int16_t mdef = 0;
  std::uint16_t element = 0;
  // Eau, terre, feu, vent, poison, sacré, ombre, fantôme, mort-vivant.
  std::array<std::uint8_t, kSenseResistCount> resist_percent{};
};

// CE QUE LE SERVEUR A RÉPONDU, RANGÉ PAR IDENTIFIANT DEMANDÉ.
//
// 🔴 LA CLÉ EST CE QU'ON A DEMANDÉ, pas ce que le serveur a résolu : c'est ce
// que sa réponse renvoie en écho, donc la seule clé dont on dispose pour
// l'apparier. Un identifiant de sprite demandé par Sense et l'identifiant de
// base du même monstre sont deux entrées distinctes.
class MonsterSheetBook {
 public:
  void Receive(std::uint32_t asked_id, MonsterSheet sheet);
  // Le serveur ne connaît pas ce monstre.
  void NoteUnknown(std::uint32_t asked_id);

  [[nodiscard]] const MonsterSheet* Find(std::uint32_t asked_id) const;
  [[nodiscard]] bool IsUnknown(std::uint32_t asked_id) const;
  // Le rang de la dernière réponse sur cet identifiant — zéro pour aucune. Ce
  // qui change dit qu'une réponse neuve est arrivée, même identique.
  [[nodiscard]] std::uint32_t ArrivalOf(std::uint32_t asked_id) const;

  void ReceiveSense(SenseReading reading);
  [[nodiscard]] const std::optional<SenseReading>& sense() const noexcept { return sense_; }
  // Combien de relevés sont arrivés : chacun rouvre la fiche, même identique.
  [[nodiscard]] std::uint32_t sense_readings() const noexcept { return sense_readings_; }

  // Au changement de carte : les taux peuvent dépendre du serveur de zone.
  void Clear() noexcept;

 private:
  struct Entry {
    std::optional<MonsterSheet> sheet;
    bool unknown = false;
    std::uint32_t arrival = 0;
  };
  std::map<std::uint32_t, Entry> entries_;
  std::uint32_t arrivals_ = 0;
  std::optional<SenseReading> sense_;
  std::uint32_t sense_readings_ = 0;
};

}  // namespace ro::sim
