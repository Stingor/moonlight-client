#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/sim/packed_position.h"
#include "ro/sim/world_fwd.h"

#include "world/actor_sounds.h"

namespace moonclient {

// LES BRUITS QUE LA MÉTHODE D'UNE ACTION NOMMÉE JOUE ELLE-MÊME, hors de la fiche
// de l'effet qu'elle sème.
//
// ⚠ LA FICHE NE LES PORTE PAS : ils sont écrits dans le code de la méthode, et
// chacun a sa façon d'être joué — tout de suite à la place de la cible, tout de
// suite avec l'effet, ou EMPILÉS sur l'effet semé, qui les joue à ses propres
// tics tant qu'il vit.

// Qui joue la méthode : une compétence ou un état, et le slot de son action.
enum class NamedMethodFamily : std::uint8_t { kSkill, kState };

// Où le bruit se fait entendre.
enum class NamedMethodSoundPlace : std::uint8_t {
  // Là où l'effet semé se tient, et en le suivant.
  kBearer,
  // À la place de la CIBLE, que l'effet se sème ailleurs ou non.
  kTarget,
};

struct NamedMethodSound {
  NamedMethodFamily family = NamedMethodFamily::kSkill;
  std::uint16_t subject_id = 0;
  std::uint8_t slot = 0;
  // Le fichier, relatif au dossier des sons ; `%d` à la place du numéro quand
  // il se tire parmi `variants`.
  std::string_view sound;
  std::uint8_t variants = 0;
  NamedMethodSoundPlace place = NamedMethodSoundPlace::kBearer;
  // EMPILÉ SUR L'EFFET SEMÉ : un bruit par niveau, tous les `step_ticks` tics
  // de l'instance, et seulement tant qu'elle vit. Faux : un seul, tout de
  // suite.
  bool queued_per_level = false;
  std::uint16_t step_ticks = 0;
};

// Les bruits d'une méthode, dans l'ordre où elle les joue ; vide pour presque
// toutes.
[[nodiscard]] std::span<const NamedMethodSound> NamedMethodSounds(NamedMethodFamily family,
                                                                  std::uint16_t subject_id,
                                                                  std::uint8_t slot);

// Ce qu'une méthode fait entendre sur-le-champ, posé sur qui le porte ou sur la
// cible. Les bruits empilés n'y sont pas : voir `QueueNamedMethodSounds`.
void PlayNamedMethodSounds(NamedMethodFamily family, std::uint16_t subject_id, std::uint8_t slot,
                           std::uint32_t bearer_id, const ro::sim::CellPosition& bearer_cell,
                           const ro::sim::CellPosition& target_cell,
                           std::vector<SownNamedSound>& heard);

// LES BRUITS EMPILÉS SUR L'EFFET QU'UNE MÉTHODE DE COMPÉTENCE VIENT DE SEMER :
// `level` bruits, le k-ième au tic k × pas de l'instance, compté dès sa
// création. Un bruit que l'instance ne vit pas assez longtemps pour atteindre
// meurt avec elle : `life_ticks` est sa vie, délai de départ compris — le
// délai la prolonge, l'image ne court pas pendant qu'il s'écoule.
void QueueNamedMethodSounds(std::uint16_t skill_id, std::uint8_t slot, std::int32_t level,
                            std::uint32_t bearer_id, const ro::sim::CellPosition& bearer_cell,
                            std::uint32_t started_ms, std::int32_t life_ticks,
                            std::vector<SownNamedSound>& heard);

// La vie en tics d'un effet nommé semé avec ce délai de départ et ces images :
// le client retire seize millisecondes au délai à chaque tic, et n'avance
// l'image qu'une fois le délai épuisé.
[[nodiscard]] std::int32_t NamedEffectLifeTicks(std::int32_t start_delay_ms,
                                                std::int32_t frame_count);

}  // namespace moonclient
