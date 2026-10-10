#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ro::sim {

// UNE CHASSE d'une quête : quel monstre, combien sont tombés sur combien.
struct QuestHunt {
  std::uint32_t hunt_id = 0;
  // La race, la taille ou l'élément qu'une chasse sans monstre désigne ; zéro
  // pour une chasse d'un monstre précis.
  std::uint32_t mob_type = 0;
  std::uint32_t mob_id = 0;
  std::int16_t level_min = 0;
  std::int16_t level_max = 0;
  std::int16_t killed = 0;
  std::int16_t total = 0;
  // Le nom du monstre, ou la description de la chasse, en UTF-8.
  std::string mob_name;

  // Une chasse sans total connu n'est jamais achevée.
  [[nodiscard]] bool done() const { return total > 0 && killed >= total; }
};

// POURQUOI UNE QUÊTE QUITTE LE JOURNAL. ⚠ LES VALEURS SONT CELLES DU FIL
// (`e_moonlight_quest_end` du serveur).
enum class QuestEndReason : std::uint8_t {
  // Rendue au PNJ.
  kCompleted = 1,
  // Effacée sans avoir été achevée.
  kErased = 2,
  // Remplacée par l'étape suivante, `QuestEnding::next_quest_id`.
  kReplaced = 3,
  // Retirée par le serveur, sortie de sa base au rechargement.
  kRetired = 4,
};

struct QuestEnding {
  QuestEndReason reason = QuestEndReason::kCompleted;
  // La quête qui la remplace, pour `kReplaced` ; zéro sinon.
  std::uint32_t next_quest_id = 0;
};

// Vrai pour une raison que ce client connaît.
[[nodiscard]] bool IsKnownQuestEndReason(std::uint8_t wire_reason) noexcept;

struct QuestRecord {
  std::uint32_t quest_id = 0;
  bool active = false;
  // En secondes Unix ; zéro pour une quête sans limite de temps.
  std::uint32_t started_unix_s = 0;
  std::uint32_t expires_unix_s = 0;
  std::vector<QuestHunt> hunts;
};

// LES QUÊTES EN COURS DU PERSONNAGE, dans l'ordre où le serveur les a dites :
// une quête ajoutée va en fin.
//
// Le serveur envoie la liste entière à l'entrée sur la carte — jamais quand le
// personnage n'a aucune quête — puis chaque changement. Une quête achevée n'est
// jamais dans la liste : le retrait est le même pour l'achèvement et pour
// l'abandon. Le serveur Moonlight en dit la raison juste AVANT le retrait, à qui
// sait la lire : elle attend ici que le retrait la prenne (`TakeEnding`).
//
// ⚠ HORS DU MONDE : c'est un état de SESSION, que le changement de carte ne
// remet pas à zéro.
class QuestLog {
 public:
  [[nodiscard]] const std::vector<QuestRecord>& quests() const noexcept { return quests_; }
  // nullptr si le personnage n'a pas cette quête.
  [[nodiscard]] const QuestRecord* Find(std::uint32_t quest_id) const;

  // ⚠ REMPLACE : la liste entière. Les raisons en attente s'oublient.
  void Replace(std::vector<QuestRecord> quests);
  // Une quête neuve ; elle remplace celle qui porte le même identifiant, en
  // gardant sa place.
  void Add(QuestRecord quest);
  // LES TOTAUX D'UNE QUÊTE NEUVE (0x08FE) : le total, le compte et le monstre,
  // sur la chasse de ce RANG dans cette quête. Faux si la quête ou le rang
  // n'existent pas.
  // ⚠ LE MONSTRE EST POSÉ MÊME À ZÉRO : ce paquet porte l'identifiant brut de la
  // base, nul pour une chasse générique (race, taille, élément), alors que la
  // quête neuve et la liste entière mettent un monstre de remplacement à sa
  // place. Zéro dit à l'interface de ne pas proposer la fiche d'un monstre.
  bool SetHuntTotalAt(std::uint32_t quest_id, std::size_t hunt_rank, std::uint32_t mob_id,
                      std::int16_t total, std::int16_t killed);
  // De même, sur la chasse que ce `hunt_id` désigne — la forme d'un serveur
  // plus ancien. Faux si aucune quête n'en porte une.
  bool SetHuntTotal(std::uint32_t hunt_id, std::uint32_t mob_id, std::int16_t total,
                    std::int16_t killed);
  // Pose le total et le compte d'une chasse. Vrai si le compte a AUGMENTÉ :
  // c'est un progrès à montrer. Faux pour une chasse inconnue, un compte
  // inchangé ou en recul.
  bool UpdateHunt(std::uint32_t quest_id, std::uint32_t hunt_id, std::int16_t total,
                  std::int16_t killed);
  // Faux si le personnage n'a pas cette quête.
  bool Remove(std::uint32_t quest_id);
  // Faux si le personnage n'a pas cette quête.
  bool SetActive(std::uint32_t quest_id, bool active);
  // Oublie aussi les raisons en attente.
  void Clear();

  // LA RAISON DU PROCHAIN RETRAIT DE CETTE QUÊTE ; la dernière dite l'emporte.
  // Elle ne change pas le journal, et ne monte donc pas `revision`.
  void NoteEnding(std::uint32_t quest_id, QuestEnding ending);
  // La reprend et l'oublie ; rien si le serveur ne l'a pas dite.
  [[nodiscard]] std::optional<QuestEnding> TakeEnding(std::uint32_t quest_id);

  // Monte à chaque changement : qui en garde une copie la recompose quand il
  // bouge.
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

 private:
  [[nodiscard]] QuestRecord* FindMutable(std::uint32_t quest_id);

  std::vector<QuestRecord> quests_;
  // Les raisons dites et pas encore prises, par quête.
  std::vector<std::pair<std::uint32_t, QuestEnding>> pending_endings_;
  std::uint64_t revision_ = 0;
};

}  // namespace ro::sim
