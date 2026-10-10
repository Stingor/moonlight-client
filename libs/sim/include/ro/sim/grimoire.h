#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"

namespace ro::sim {

// LE GRIMOIRE : L'ARBRE DU CLIENT ET CE QUE LE SERVEUR DIT, FUSIONNÉS.
//
// Ni l'un ni l'autre ne suffit. L'arbre donne la case, le niveau maximal, les
// prérequis et le nom ; le serveur donne le rang appris, le coût et la portée au
// rang appris. Une compétence peut n'être que dans l'un des deux : une case
// jamais apprise n'a pas de ligne serveur, et le serveur accorde des compétences
// qu'aucune grille ne place.
//
// ⚠ RIEN N'EST ENVOYÉ D'ICI. Réserver un point ne fait que le NOTER ; c'est
// « Appliquer » qui parle, et le serveur revalide chaque montée de toute façon.
// C'est ce qui permet de préparer une branche entière, de la voir, et de
// l'abandonner sans avoir rien dépensé.

// Ce qu'il advient d'une demande de réservation. La raison de l'échec est un
// CODE et non une phrase : le texte montré au joueur appartient à l'interface,
// qui seule connaît sa langue.
enum class ReserveOutcome : std::uint8_t {
  kReserved,
  // La compétence n'est dans aucune table du client.
  kUnknownSkill,
  // Plus un seul point libre, réservations déduites.
  kNoPointLeft,
  // Déjà au rang maximal — ou sans rang maximal déclaré, ce qui revient au
  // même : les tables ne disent pas jusqu'où elle monte.
  kAlreadyAtMax,
  // Il reste des points, mais pas assez pour la chaîne de prérequis qui manque.
  //
  // ⚠ CE QUI A PU ÊTRE PAYÉ RESTE RÉSERVÉ : la base de la chaîne est posée, et
  // le joueur voit où elle s'arrête. Un refus qui déferait tout n'avancerait
  // rien et ne dirait pas où ça bloque.
  kNotEnoughForPrerequisites,
};

// Une ligne du grimoire.
struct GrimoireEntry {
  std::uint16_t skill_id = 0;
  // Le métier dont la grille porte cette case. Zéro quand aucune grille de la
  // chaîne ne la place : le serveur l'a donnée quand même, ou elle n'est que
  // réservée.
  std::uint32_t job = 0;
  std::uint16_t position = 0;
  bool in_grid = false;
  // Ce que le SERVEUR a annoncé.
  std::uint16_t learned = 0;
  // Le rang VISÉ par la réservation du joueur, zéro quand il n'a rien réservé.
  // Ce n'est pas un écart : c'est le rang où l'on veut arriver.
  std::uint16_t reserved = 0;
  // ⚠ ZÉRO TANT QUE LA COMPÉTENCE N'EST PAS APPRISE : le serveur ne dit rien
  // d'une compétence qu'on ne sait pas. Le coût par rang de l'arbre, lui, est
  // toujours là — c'est `node` qui le porte.
  std::uint16_t sp_cost = 0;
  std::uint16_t range_cells = 0;
  std::uint16_t targeting = 0;
  // La fiche des tables du client. NUL pour une compétence que le serveur
  // accorde et que le client ne connaît pas : il n'y a alors ni nom, ni case,
  // ni rang maximal.
  const SkillNode* node = nullptr;

  // Le rang que le joueur VERRA : ce qu'il sait, ou ce qu'il a réservé.
  [[nodiscard]] std::uint16_t effective_level() const noexcept {
    return std::max(learned, reserved);
  }
  [[nodiscard]] bool known() const noexcept { return learned > 0; }
};

class Grimoire {
 public:
  // L'arbre est passé par référence et doit vivre plus longtemps : il est LU,
  // jamais copié — mille huit cent quatre-vingt-sept fiches.
  explicit Grimoire(const SkillTree& tree) : tree_(&tree) {}

  // Refait les lignes : la chaîne d'héritage du métier pour les cases, le livre
  // pour ce qui est appris, et le nombre de points libres.
  //
  // ⚠ LES RÉSERVATIONS SURVIVENT, sauf celles devenues sans objet — compétence
  // absente de l'arbre après un changement de métier, ou rang déjà atteint par
  // le serveur. Sans ce tri, une réservation périmée continuerait de compter des
  // points pour rien.
  void Refresh(std::uint32_t job, const SkillBook& book, std::int64_t skill_points);

  [[nodiscard]] std::span<const GrimoireEntry> entries() const noexcept { return entries_; }
  [[nodiscard]] const GrimoireEntry* Find(std::uint16_t skill_id) const;

  // Les points RÉSERVÉS, et ceux qui restent libres.
  [[nodiscard]] int reserved_points() const noexcept;
  [[nodiscard]] int points_left() const noexcept;
  [[nodiscard]] bool has_reservations() const noexcept { return !reservations_.empty(); }

  // Réserve un rang sur `skill_id` — ou tout ce qui reste jusqu'au maximum.
  //
  // ⚠ LES PRÉREQUIS MANQUANTS SE RÉSERVENT AUSSI, et TOUTE la chaîne, pas
  // seulement le rang précédent. La liste que porte une compétence n'est pas une
  // fermeture transitive : Cart Termination réclame Cart Boost, qui réclame
  // Pushcart 5, qui réclame Enlarge Weight Limit 5 — et cette dernière
  // n'apparaît nulle part dans la liste de Cart Termination. S'arrêter au rang
  // direct, ce que fait le client, laisse des trous dans l'arbre.
  //
  // Les plus PROFONDS d'abord : les points restants doivent aller à la base de
  // la chaîne, sinon on paie le sommet et le pied reste verrouillé.
  [[nodiscard]] ReserveOutcome Reserve(std::uint16_t skill_id, bool up_to_max = false);

  void CancelReservations();

  // Les montées à envoyer au serveur, UNE PAR RANG : c'est ainsi que le client
  // le fait, et le serveur revalide chacune.
  //
  // L'ordre est celui des réservations, donc celui des prérequis : `Reserve` pose
  // la base de la chaîne avant son sommet.
  [[nodiscard]] std::vector<std::uint16_t> PendingUpgrades() const;

 private:
  // Le rang appris d'une compétence, zéro si le serveur n'en a rien dit.
  [[nodiscard]] std::uint16_t LearnedLevelOf(std::uint16_t skill_id) const;
  [[nodiscard]] std::uint16_t ReservedLevelOf(std::uint16_t skill_id) const;
  // Pose ou relève une réservation, et rend ce qu'elle a réellement coûté.
  [[nodiscard]] int ReserveUpTo(const SkillNode& node, int wanted_level, int& points_left);
  // La ligne d'une compétence, créée si elle n'en avait pas encore.
  [[nodiscard]] GrimoireEntry& RowFor(std::uint16_t skill_id, std::uint32_t job,
                                      std::uint16_t position, bool in_grid);
  void MarkReservedRows();

  const SkillTree* tree_ = nullptr;
  std::vector<GrimoireEntry> entries_;
  std::unordered_map<std::uint16_t, std::size_t> row_by_skill_;
  // Les rangs visés, dans l'ordre où ils ont été posés — donc dans l'ordre des
  // prérequis. ⚠ ILS VIVENT À PART DES LIGNES, qui sont REFAITES à chaque
  // rafraîchissement : les ranger dedans les effacerait à chaque paquet du
  // serveur.
  std::vector<std::pair<std::uint16_t, std::uint16_t>> reservations_;
  std::int64_t skill_points_ = 0;
};

}  // namespace ro::sim
