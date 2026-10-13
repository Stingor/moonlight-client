#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/sim/misc_effect.h"

namespace ro::sim {

// CE QUE LE JOUEUR VOIT ET ENTEND QUAND IL GAGNE UN NIVEAU.
//
// 🔴 CELA PART DU PARAMÈTRE, ET NON DE L'EFFET DIVERS. Le serveur envoie les
// deux à chaque montée — `SP_BASELEVEL` puis `NOTIFYEFFECT_BASE_LEVEL_UP` —,
// mais le client ne cherche l'acteur de l'effet divers que parmi les AUTRES :
// sur soi, cet effet-là ne montre rien. C'est le nouveau niveau, reçu par
// ZC_PAR_CHANGE, qui sème l'effet sur le joueur et, pour le niveau de base,
// joue `levelup.wav`.
//
// ⚠ ET L'EFFET DÉPEND DE LA CLASSE, quand l'effet divers en a un par type :
// le super apprenti et la lignée taekwon ont le leur.

// Le son d'une montée de niveau de BASE. Celle du niveau de métier n'en a pas.
inline constexpr const char* kBaseLevelUpSound = "levelup.wav";

// Au-delà de ce niveau de métier, l'effet est celui du super apprenti.
inline constexpr std::int64_t kJobLevelUpPlainCeiling = 50;

// Les rangs de `e_notify_effect` dont l'effet sert ici.
inline constexpr std::uint32_t kNotifyBaseLevelUp = 0;
inline constexpr std::uint32_t kNotifyJobLevelUp = 1;
inline constexpr std::uint32_t kNotifySuperNoviceBaseLevelUp = 7;
inline constexpr std::uint32_t kNotifySuperNoviceJobLevelUp = 8;
inline constexpr std::uint32_t kNotifyTaekwonBaseLevelUp = 9;

// Les classes qui montent comme un super apprenti : le super apprenti et ses
// variantes, et les trois classes de l'extension Gangsi.
inline constexpr std::array<std::uint16_t, 13> kSuperNoviceLevelUpJobs = {
    23, 4045, 4050, 4051, 4052, 4128, 4172, 4190, 4191, 4192, 4193, 4307, 4314,
};

// La lignée taekwon, bébés et troisièmes classes compris.
inline constexpr std::array<std::uint16_t, 17> kTaekwonLevelUpJobs = {
    4046, 4047, 4048, 4049, 4225, 4226, 4227, 4238, 4239,
    4240, 4241, 4242, 4243, 4244, 4302, 4303, 4316,
};

// L'effet qu'une montée de niveau de BASE sème sur le joueur de cette classe.
[[nodiscard]] constexpr std::uint16_t BaseLevelUpEffectId(std::uint16_t job) {
  if (std::ranges::find(kSuperNoviceLevelUpJobs, job) != kSuperNoviceLevelUpJobs.end()) {
    return MiscEffectId(kNotifySuperNoviceBaseLevelUp);
  }
  if (std::ranges::find(kTaekwonLevelUpJobs, job) != kTaekwonLevelUpJobs.end()) {
    return MiscEffectId(kNotifyTaekwonBaseLevelUp);
  }
  return MiscEffectId(kNotifyBaseLevelUp);
}

// L'effet qu'une montée de niveau de MÉTIER sème sur le joueur : il ne dépend
// que du nouveau niveau, quelle que soit la classe.
[[nodiscard]] constexpr std::uint16_t JobLevelUpEffectId(std::int64_t job_level) {
  return MiscEffectId(job_level > kJobLevelUpPlainCeiling ? kNotifySuperNoviceJobLevelUp
                                                          : kNotifyJobLevelUp);
}

}  // namespace ro::sim
