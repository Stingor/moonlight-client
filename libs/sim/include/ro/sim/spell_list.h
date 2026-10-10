#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// LES SORTS QU'AUTOSPELL PROPOSE, en attente du choix du joueur.
//
// Le serveur envoie la liste au lancement de la compétence, puis attend la
// réponse : tant qu'elle ne vient pas, le sort choisi n'est pas armé — et la
// compétence semble ne rien faire.
//
// ⚠ UN IDENTIFIANT NUL OU NÉGATIF N'EST PAS UN SORT, et une liste qui n'en porte
// aucun n'ouvre rien : c'est ce que fait le client.
class SpellListOffer {
 public:
  void Offer(std::span<const std::int32_t> skill_ids) {
    skill_ids_.clear();
    for (const std::int32_t skill_id : skill_ids) {
      if (skill_id > 0) {
        skill_ids_.push_back(static_cast<std::uint16_t>(skill_id));
      }
    }
    open_ = !skill_ids_.empty();
  }

  // Un choix fait, ou un refus : la liste s'en va.
  void Close() noexcept {
    open_ = false;
    skill_ids_.clear();
  }

  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] std::span<const std::uint16_t> skill_ids() const noexcept { return skill_ids_; }

 private:
  bool open_ = false;
  std::vector<std::uint16_t> skill_ids_;
};

// LE NIVEAU QU'AUTOSPELL ARMERA pour ce sort, d'après la règle du serveur : un
// plafond qui dépend du sort et du niveau d'Autospell, puis le niveau appris du
// sort. Zéro pour un sort qu'Autospell n'arme pas.
//
// ⚠ LE LIEN D'ÂME DU SAGE porte les trois éclairs au niveau dix. Sur un Sage, un
// lien d'âme est toujours le sien : sa présence suffit à le dire.
[[nodiscard]] constexpr std::uint16_t AutospellArmedLevel(std::uint16_t skill_id,
                                                          std::uint16_t autospell_level,
                                                          std::uint16_t learned_level,
                                                          bool sage_linked) noexcept {
  constexpr std::uint16_t kNapalmBeat = 11;
  constexpr std::uint16_t kSoulStrike = 13;
  constexpr std::uint16_t kColdBolt = 14;
  constexpr std::uint16_t kFrostDiver = 15;
  constexpr std::uint16_t kFireBall = 17;
  constexpr std::uint16_t kFireBolt = 19;
  constexpr std::uint16_t kLightningBolt = 20;
  constexpr std::uint16_t kLinkedBoltCap = 10;
  if (autospell_level == 0 || learned_level == 0) {
    return 0;
  }
  std::uint16_t cap = 1;
  switch (skill_id) {
    case kNapalmBeat:
      cap = 3;
      break;
    case kColdBolt:
    case kFireBolt:
    case kLightningBolt:
      cap = sage_linked ? kLinkedBoltCap
                        : static_cast<std::uint16_t>(autospell_level >= 4   ? 3
                                                     : autospell_level == 3 ? 2
                                                                            : 1);
      break;
    case kSoulStrike:
      cap = static_cast<std::uint16_t>(autospell_level >= 7 ? 3 : autospell_level == 6 ? 2 : 1);
      break;
    case kFireBall:
      cap = static_cast<std::uint16_t>(autospell_level >= 9 ? 2 : 1);
      break;
    case kFrostDiver:
      cap = 1;
      break;
    default:
      return 0;
  }
  return cap < learned_level ? cap : learned_level;
}

}  // namespace ro::sim
