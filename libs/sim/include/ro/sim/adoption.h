#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"
#include "ro/sim/skill_book.h"

namespace ro::sim {

// ── L'ADOPTION : UN COUPLE FAIT D'UN JOUEUR SON ENFANT ──────────────────────
//
// Le serveur exige, pour celui qui demande (le « parent ») : être marié, au
// groupe de l'enfant, avec son conjoint dans ce même groupe ; porter l'alliance,
// lui ET son conjoint ; n'avoir pas encore d'enfant, ni l'un ni l'autre ; le
// niveau de base 70, tous deux. Pour l'enfant : n'avoir ni parents ni demande
// en cours, n'être pas marié, et être Novice, de première classe ou Super
// Novice.
//
// 🔴 LE CLIENT N'EN JUGE QUE CE QU'IL SAIT, et c'est ce qui décide de montrer
// l'entrée « Adopter » : son propre niveau, son alliance, son enfant, le groupe
// de l'enfant et sa classe. Le conjoint, le mariage de l'enfant et ses parents
// lui sont inconnus ; le serveur les revalide, et en refuse la plupart en
// silence.

// Le niveau de base que les deux parents doivent avoir atteint.
inline constexpr std::int64_t kAdoptionMinBaseLevel = 70;

// Les deux alliances que le mariage donne. Porter l'une ou l'autre suffit.
inline constexpr std::uint32_t kGroomWeddingRingItemId = 2634;
inline constexpr std::uint32_t kBrideWeddingRingItemId = 2635;

// La compétence que l'adoption donne pour toujours aux deux parents : qui la
// connaît a déjà un enfant.
inline constexpr std::uint16_t kCallBabySkillId = 410;

// Les classes qu'on peut adopter : du Novice au Voleur, et les deux Super
// Novice. Une classe bébé en est exclue d'elle-même : on ne l'adopte pas deux
// fois.
inline constexpr std::uint16_t kThiefJobId = 6;
inline constexpr std::uint16_t kSuperNoviceJobId = 23;
inline constexpr std::uint16_t kExpandedSuperNoviceJobId = 4190;

[[nodiscard]] constexpr bool AdoptableJob(std::uint16_t job_id) noexcept {
  return job_id <= kThiefJobId || job_id == kSuperNoviceJobId ||
         job_id == kExpandedSuperNoviceJobId;
}

// Une alliance est-elle ÉQUIPÉE dans ce sac ?
[[nodiscard]] bool WearsWeddingRing(const Inventory& bag);

// Ce que le client sait, au moment d'ouvrir le menu d'un autre joueur.
struct AdoptionFacts {
  // `kUnknownStat` tant que le serveur ne l'a pas dit.
  std::int64_t own_base_level = kUnknownStat;
  bool wears_wedding_ring = false;
  bool has_child = false;
  // La cible est membre de NOTRE groupe.
  bool target_in_party = false;
  // La classe que montre la cible.
  std::uint16_t target_job_id = 0;
};

// Ce que le client sait permet-il l'adoption ? Faux dès qu'une condition
// connue manque, ou qu'on ne la connaît pas encore.
[[nodiscard]] bool AdoptionLooksPossible(const AdoptionFacts& facts) noexcept;

[[nodiscard]] AdoptionFacts AdoptionFactsOf(std::int64_t own_base_level, const Inventory& bag,
                                            const SkillBook& skills, bool target_in_party,
                                            std::uint16_t target_job_id);

// UN COUPLE NOUS DEMANDE : les deux comptes que la réponse renverra, et le nom
// de celui qui demande, en UTF-8.
struct AdoptionInvitation {
  std::uint32_t parent_account_id = 0;
  std::uint32_t spouse_account_id = 0;
  std::string parent_name;
};

// Les trois refus que le serveur dit, dans sa numérotation.
enum class AdoptionRefusal : std::uint8_t {
  // L'un des parents a déjà un enfant.
  kAlreadyParent = 0,
  // L'un des parents n'a pas le niveau 70.
  kParentLevel = 1,
  // L'enfant visé est marié.
  kChildMarried = 2,
};
inline constexpr std::uint32_t kAdoptionRefusalCount = 3;

// CE QUE LE SERVEUR A DIT DE L'ADOPTION, en attendant que la partie le joue :
// la demande reçue, qui attend sa réponse, et les refus à dire.
class AdoptionDesk {
 public:
  // ⚠ UNE DEMANDE NOUVELLE REMPLACE L'ANCIENNE : le serveur ne garde que la
  // dernière, et répondre à l'autre serait refusé.
  void TellInvitation(AdoptionInvitation invitation) {
    invitation_ = std::move(invitation);
    ++invitation_seq_;
  }
  [[nodiscard]] const std::optional<AdoptionInvitation>& invitation() const noexcept {
    return invitation_;
  }
  // Grandit à chaque demande reçue : la boîte s'ouvre une fois par demande.
  [[nodiscard]] std::uint32_t invitation_seq() const noexcept { return invitation_seq_; }
  void ForgetInvitation() noexcept { invitation_.reset(); }

  void TellRefusal(AdoptionRefusal refusal) { refusals_.push_back(refusal); }
  [[nodiscard]] std::vector<AdoptionRefusal> TakeRefusals() { return std::exchange(refusals_, {}); }

 private:
  std::optional<AdoptionInvitation> invitation_;
  std::uint32_t invitation_seq_ = 0;
  std::vector<AdoptionRefusal> refusals_;
};

}  // namespace ro::sim
