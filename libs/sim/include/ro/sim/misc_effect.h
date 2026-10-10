#pragma once

#include <array>
#include <cstdint>

namespace ro::sim {

// CE QUE LE CLIENT MONTRE POUR UN « EFFET DIVERS ».
//
// 🔴 ZC_NOTIFY_EFFECT (0x019B) NE PORTE PAS UN IDENTIFIANT D'EFFET, mais un
// TYPE — dix cas que le serveur nomme dans `e_notify_effect`
// (src/map/clif.hpp) et que le client traduit par un switch fixe. Le prendre pour un identifiant
// montrerait l'effet 0 à chaque montée de niveau.
//
// C'est le paquet de la MONTÉE DE NIVEAU, du raffinement et de la fabrication :
// le joueur le voit plusieurs fois par heure, et il n'arrivait nulle part.
//
// ⚠ LE TYPE 4 NE MONTRE RIEN. Le client y lève un drapeau global au lieu de
// semer un effet — c'est la fin de partie, et elle se traite ailleurs. Lui
// donner un effet inventerait une image que le natif n'a pas.
struct MiscEffectEntry {
  // Le type, tel que `e_notify_effect` le numérote.
  std::uint8_t type = 0;
  // L'effet que le client sème sur l'acteur, résolu par la table du client.
  std::uint16_t effect_id = 0;
};

// Les neuf types qui montrent quelque chose, dans l'ordre du switch du client.
inline constexpr std::array<MiscEffectEntry, 9> kMiscEffects = {{
    {0, 371},  // NOTIFYEFFECT_BASE_LEVEL_UP            → angel.str
    {1, 158},  // NOTIFYEFFECT_JOB_LEVEL_UP             → JobLvUP.str
    {2, 155},  // NOTIFYEFFECT_REFINE_FAILURE           → bs_RefineFailed.str
    {3, 154},  // NOTIFYEFFECT_REFINE_SUCCESS           → bs_RefineSuccess.str
    {5, 305},  // NOTIFYEFFECT_PHARMACY_SUCCESS         → p_success.str
    {6, 306},  // NOTIFYEFFECT_PHARMACY_FAILURE         → p_failed.str
    {7, 338},  // NOTIFYEFFECT_SUPER_NOVICE_BASE_LEVEL_UP  (procédural)
    {8, 337},  // NOTIFYEFFECT_SUPER_NOVICE_JOB_LEVEL_UP   → JobLvUP.str
    {9, 582},  // NOTIFYEFFECT_TAEKWON_BASE_LEVEL_UP       (procédural)
}};

// L'effet d'un type, ou ZÉRO quand le client n'en montre aucun.
//
// Zéro est un identifiant d'effet valide ailleurs — c'est EF_HIT1 —, mais aucun
// type d'effet divers ne le porte : il sert donc ici de « rien à montrer ».
[[nodiscard]] constexpr std::uint16_t MiscEffectId(std::uint32_t type) {
  for (const MiscEffectEntry& entry : kMiscEffects) {
    if (entry.type == type) {
      return entry.effect_id;
    }
  }
  return 0;
}

}  // namespace ro::sim
