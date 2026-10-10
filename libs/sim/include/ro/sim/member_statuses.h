#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

#include "ro/sim/status_effects.h"

namespace ro::sim {

// ── LES ÉTATS DES MEMBRES DU GROUPE, AVEC LEURS DURÉES ──────────────────────
//
// Le monde ne garde des autres acteurs que la PRÉSENCE de leurs états. Les
// surfaces du groupe montrent un compte à rebours et un voile : il leur faut
// l'échéance et une durée de référence, par membre. Ce registre vit à part du
// monde, à côté de `PartyRoster`.
//
// Deux sources l'alimentent :
//
//   · la réponse du serveur à une demande d'état complet, qui REMPLACE ce qu'on
//     savait du membre — ce qui n'y figure pas n'est plus actif ;
//   · les transitions diffusées à la zone (un état commence ou finit), qui ne
//     touchent que l'état qu'elles nomment.
//
// ⚠ ON NE SAIT RIEN D'UN MEMBRE QUI N'Y FIGURE PAS, ce qui n'est pas « aucun
// état ». Une réponse vide, elle, dit « aucun état ».

// Un refus du serveur (le compte n'est pas de mon groupe) fait taire les
// demandes sur ce compte pendant ce délai.
inline constexpr std::uint32_t kMemberStatusRefusedQuietMs = 15000;

// Un restant annoncé qui dépasse de plus que cette marge celui qu'on
// extrapolait est un état RELANCÉ : sa durée de référence repart de lui. En deçà,
// c'est l'écart d'horloge entre le serveur et nous.
inline constexpr std::uint32_t kMemberStatusRefreshMarginMs = 2000;

// Une entrée de la réponse complète : le restant et le total que le serveur
// annonce, zéro valant « sans échéance ».
struct StatusReport {
  StatusEffectId effect = 0;
  std::uint32_t remaining_ms = 0;
  std::uint32_t total_ms = 0;
};

// LA DURÉE DE RÉFÉRENCE D'UN ÉTAT : le plus grand restant jamais vu. Exacte quand
// on a vu l'état commencer, borne basse quand on l'a découvert en cours de route.
// Un état relancé repart de sa nouvelle durée ; un état permanent (`announced`
// nul) garde la précédente.
[[nodiscard]] std::uint32_t KeepLongestDuration(std::uint32_t announced,
                                                std::uint32_t previous_total,
                                                std::uint32_t previous_left);

class MemberStatuses {
 public:
  // La liste COMPLÈTE d'un membre. Elle remplace la précédente ; chaque état déjà
  // connu garde sa durée de référence s'il ne repart pas.
  void Tell(std::uint32_t account_id, std::span<const StatusReport> reported, std::uint32_t now_ms);

  // Un état qui commence (`active`) ou qui finit, annoncé à la zone. Le restant
  // `kUnknownDurationMs` ou nul vaut « sans échéance ».
  void Apply(std::uint32_t account_id, StatusEffectId effect, bool active,
             std::uint32_t remaining_ms, std::uint32_t total_ms, std::uint32_t now_ms);

  // Le serveur ne peut pas décrire ce compte : on oublie ce qu'on en savait.
  void Forget(std::uint32_t account_id);
  // Il REFUSE de le décrire : on oublie, et on se tait un moment.
  void Refuse(std::uint32_t account_id, std::uint32_t now_ms);
  [[nodiscard]] bool Refused(std::uint32_t account_id, std::uint32_t now_ms) const;

  // Retire les états échus, et tout compte que `keep` ne retient pas.
  void Purge(std::uint32_t now_ms, const std::function<bool(std::uint32_t)>& keep);

  // Tout oublier, refus compris : le monde repart.
  void Clear();

  // Les états connus d'un membre, du plus ancien au plus récent ; nul quand on
  // ne sait rien de lui.
  [[nodiscard]] const std::vector<ActiveStatus>* Find(std::uint32_t account_id) const;

 private:
  std::unordered_map<std::uint32_t, std::vector<ActiveStatus>> by_account_;
  std::unordered_map<std::uint32_t, std::uint32_t> refused_ms_;
};

}  // namespace ro::sim
