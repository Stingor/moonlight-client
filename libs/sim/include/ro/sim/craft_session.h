#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// L'ATELIER : ce que le serveur propose à fabriquer ou à affiner, et ce qu'il
// répond.
//
// 🔴 LE SERVEUR TIENT LA SESSION, PAS LE CLIENT. Un skill d'atelier — fabriquer
// des flèches, forger, affiner — arme côté serveur un « skill en attente », puis
// envoie une liste. Il n'accepte qu'UNE réponse à cette liste, et l'efface
// ensuite ; un personnage dont la liste reste sans réponse ne lance plus aucun
// skill. D'où la règle des fenêtres qui s'en servent : fermer, c'est répondre.
//
// Ce module ne retient que ce qui est ARRIVÉ, avec un rang qui ne recule
// jamais : deux listes identiques de suite sont deux réponses, et c'est le rang
// qui le dit. Ce qu'on en fait — sélection, relance, journal — appartient à
// qui dessine.

// LE CHEMIN DE RÉPONSE d'une liste de fabrication. Trois listes, trois paquets
// de réponse, et rien d'autre ne les distingue une fois la liste reçue.
enum class MakeListKind : std::uint8_t {
  // Flèches, convertisseurs élémentaires, poisons, leurres : la réponse ne porte
  // que l'objet.
  kArrow,
  // Cuisine, bombes, pharmacies de troisième classe : la réponse rappelle le
  // genre de liste.
  kMaking,
  // Forge, pharmacie, runes : la réponse porte trois matériaux optionnels.
  kProduce,
};

// Le genre de liste que la réponse des flèches rappelle aussi : le serveur et le
// client d'origine le posent pour les quatre skills qui passent par elle.
inline constexpr std::uint16_t kArrowMakeType = 2;
// Le genre de la cuisine. ⚠ UNE LISTE DE CUISINE VIDE ARME QUAND MÊME le serveur :
// elle se referme donc par une réponse, comme une liste pleine.
inline constexpr std::uint16_t kCookingMakeType = 1;

struct MakeList {
  MakeListKind kind = MakeListKind::kArrow;
  // Rappelé tel quel dans la réponse du chemin `kMaking`.
  std::uint16_t make_type = 0;
  // Les objets proposés, dans l'ordre du serveur, doublons compris.
  std::vector<std::uint32_t> item_ids;
};

// Ce que le serveur répond à une fabrication. ⚠ IL NE RÉPOND PAS TOUJOURS : les
// flèches, les convertisseurs, la forge et la pharmacie créent l'objet sans un
// mot.
struct MakeResult {
  // Pair : réussite ; impair : échec. Les paires se suivent par métier.
  std::int16_t code = 0;
  std::uint32_t item_id = 0;

  [[nodiscard]] bool succeeded() const noexcept { return code % 2 == 0; }
};

// UNE ARME QU'ON PEUT AFFINER, telle que la liste la décrit.
//
// ⚠ LA LISTE PORTE LE RAFFINEMENT ET LES CARTES, et c'est la seule source qui
// les donne avec la place : deux armes du même nom s'y distinguent par eux.
struct RefinableWeapon {
  // La place à renvoyer, TELLE QUELLE.
  std::int16_t index = 0;
  std::uint32_t item_id = 0;
  std::uint8_t refine = 0;
  std::array<std::uint32_t, kItemCardSlots> cards = {};
};

// Ce que le serveur répond à un affinage.
enum class RefineOutcome : std::uint8_t {
  kSucceeded = 0,
  // 🔴 L'ARME EST DÉTRUITE.
  kFailed = 1,
  // Le niveau du skill ne permet pas ce raffinement.
  kSkillTooLow = 2,
  // Le minerai manque. ⚠ L'OBJET DE LA RÉPONSE EST ALORS LE MINERAI, pas l'arme.
  kMissingOre = 3,
};

struct RefineResult {
  RefineOutcome outcome = RefineOutcome::kSucceeded;
  std::uint32_t item_id = 0;
};

// Un geste du joueur, noté quand il PART. Ni le skill ni l'objet qui ont ouvert
// une liste ne figurent dans elle : c'est la seule façon de le retrouver.
struct CraftTrigger {
  std::uint16_t id = 0;
  std::uint16_t level = 0;
  std::uint32_t at_ms = 0;
  std::uint64_t seq = 0;
};

class CraftSession {
 public:
  // ── LA FABRICATION ────────────────────────────────────────────────────────
  void TellMakeList(MakeList list) {
    make_list_ = std::move(list);
    ++make_list_seq_;
  }
  [[nodiscard]] const MakeList& make_list() const noexcept { return make_list_; }
  [[nodiscard]] std::uint64_t make_list_seq() const noexcept { return make_list_seq_; }

  void TellMakeResult(MakeResult result) noexcept {
    make_result_ = result;
    ++make_result_seq_;
  }
  [[nodiscard]] const MakeResult& make_result() const noexcept { return make_result_; }
  [[nodiscard]] std::uint64_t make_result_seq() const noexcept { return make_result_seq_; }

  // LA MAÎTRISE CULINAIRE, poussée par ce serveur seul. Moins un : jamais reçue.
  void TellCookMastery(int mastery) noexcept {
    cook_mastery_ = mastery;
    ++cook_mastery_seq_;
  }
  [[nodiscard]] int cook_mastery() const noexcept { return cook_mastery_; }
  [[nodiscard]] std::uint64_t cook_mastery_seq() const noexcept { return cook_mastery_seq_; }

  // ── L'AFFINAGE ────────────────────────────────────────────────────────────
  void TellRefineList(std::vector<RefinableWeapon> weapons) {
    refine_list_ = std::move(weapons);
    ++refine_list_seq_;
  }
  [[nodiscard]] const std::vector<RefinableWeapon>& refine_list() const noexcept {
    return refine_list_;
  }
  [[nodiscard]] std::uint64_t refine_list_seq() const noexcept { return refine_list_seq_; }

  void TellRefineResult(RefineResult result) noexcept {
    refine_result_ = result;
    ++refine_result_seq_;
  }
  [[nodiscard]] const RefineResult& refine_result() const noexcept { return refine_result_; }
  [[nodiscard]] std::uint64_t refine_result_seq() const noexcept { return refine_result_seq_; }

  // ── LES REFUS DE SKILL ────────────────────────────────────────────────────
  //
  // Un skill refusé ne rouvre aucune liste : c'est ce qui dit à une relance
  // qu'elle est tombée, ou à une demande qu'elle a été rejetée.
  void NoteSkillRefused(std::uint16_t skill_id) noexcept {
    refused_skill_ = skill_id;
    ++refusal_seq_;
  }
  [[nodiscard]] std::uint16_t refused_skill() const noexcept { return refused_skill_; }
  [[nodiscard]] std::uint64_t refusal_seq() const noexcept { return refusal_seq_; }

  // ── CE QUE LE JOUEUR A ENVOYÉ ─────────────────────────────────────────────
  void NoteSkillCast(std::uint16_t skill_id, std::uint16_t level, std::uint32_t now_ms) noexcept {
    last_cast_ =
        CraftTrigger{.id = skill_id, .level = level, .at_ms = now_ms, .seq = last_cast_.seq + 1};
  }
  [[nodiscard]] const CraftTrigger& last_cast() const noexcept { return last_cast_; }

  // L'objet UTILISÉ, par son identifiant — résolu avant que le serveur ne le
  // consomme : la place, elle, ne désigne plus rien une fois l'exemplaire parti.
  void NoteItemUse(std::uint32_t item_id, std::uint32_t now_ms) noexcept {
    last_use_item_ = item_id;
    last_use_ = CraftTrigger{.id = 0, .level = 0, .at_ms = now_ms, .seq = last_use_.seq + 1};
  }
  [[nodiscard]] std::uint32_t last_use_item() const noexcept { return last_use_item_; }
  [[nodiscard]] const CraftTrigger& last_use() const noexcept { return last_use_; }

 private:
  MakeList make_list_;
  std::uint64_t make_list_seq_ = 0;
  MakeResult make_result_;
  std::uint64_t make_result_seq_ = 0;
  int cook_mastery_ = -1;
  std::uint64_t cook_mastery_seq_ = 0;
  std::vector<RefinableWeapon> refine_list_;
  std::uint64_t refine_list_seq_ = 0;
  RefineResult refine_result_;
  std::uint64_t refine_result_seq_ = 0;
  std::uint16_t refused_skill_ = 0;
  std::uint64_t refusal_seq_ = 0;
  CraftTrigger last_cast_;
  std::uint32_t last_use_item_ = 0;
  CraftTrigger last_use_;
};

}  // namespace ro::sim
