#include "effects/effect_named_sounds.h"

#include <algorithm>
#include <array>
#include <tuple>

#include "ro/render/effect_frame.h"

namespace moonclient {
namespace {

// LES NEUF MÉTHODES QUI SONNENT D'ELLES-MÊMES, triées par famille, sujet et
// slot.
constexpr std::array<NamedMethodSound, 9> kNamedMethodSounds = {{
    // LES COMPÉTENCES DE MONSTRE, à l'emploi : leur bruit part tout de suite à
    // la place de la cible, avec l'effet qu'elles y posent.
    {NamedMethodFamily::kSkill, 786, 11, "effect\\NPC_AIMED_SHOWER.wav", 0,
     NamedMethodSoundPlace::kTarget, false, 0},
    {NamedMethodFamily::kSkill, 787, 11, "effect\\NPC_BLAZING_ERUPTION.wav", 0,
     NamedMethodSoundPlace::kTarget, false, 0},
    {NamedMethodFamily::kSkill, 790, 11, "effect\\NPC_FROST_FIELD.wav", 0,
     NamedMethodSoundPlace::kTarget, false, 0},
    // NPC_LIGHTNING_JUDGEMENT, à l'emploi : l'éclair tonne sur la cible, tout
    // de suite — pourvu que le lanceur soit connu.
    {NamedMethodFamily::kSkill, 791, 11, "effect\\NPC_LIGHTNING_JUDGEMENT.wav", 0,
     NamedMethodSoundPlace::kTarget, false, 0},
    {NamedMethodFamily::kSkill, 793, 11, "effect\\NPC_RESET_EFST.wav", 0,
     NamedMethodSoundPlace::kTarget, false, 0},
    // RA_UNLIMIT, à la pose de l'incantation : le bruit ordinaire d'un début
    // de sort, joué par la méthode elle-même — le client le pose sur le
    // joueur, quel que soit le lanceur ; ici, sur le lanceur.
    {NamedMethodFamily::kSkill, 5002, 8, "effect\\EF_BeginSpell.wav", 0,
     NamedMethodSoundPlace::kBearer, false, 0},
    // DK_STORMSLASH, à l'emploi : un coup par niveau, tiré parmi quatre, un
    // tous les quinze tics de l'effet semé sur le lanceur.
    {NamedMethodFamily::kSkill, 5213, 11, "effect\\DK_STORMSLASH%d.wav", 4,
     NamedMethodSoundPlace::kBearer, true, 15},
    // IG_SHIELD_SHOOTING, à l'emploi : le bouclier part avec son bruit, à la
    // place de la cible.
    {NamedMethodFamily::kSkill, 5265, 11, "effect\\IG_SHIELD_SHOOTING.wav", 0,
     NamedMethodSoundPlace::kTarget, false, 0},
    // CHARGINGPIERCE_COUNT, à la pose : le coup de la charge, avec son effet.
    {NamedMethodFamily::kState, 1175, 0, "effect\\DK_CHARGINGPIERCE3.wav", 0,
     NamedMethodSoundPlace::kBearer, false, 0},
}};

[[nodiscard]] auto KeyOf(const NamedMethodSound& sound) {
  return std::tuple{sound.family, sound.subject_id, sound.slot};
}

}  // namespace

std::span<const NamedMethodSound> NamedMethodSounds(NamedMethodFamily family,
                                                    std::uint16_t subject_id, std::uint8_t slot) {
  const auto found =
      std::ranges::equal_range(kNamedMethodSounds, std::tuple{family, subject_id, slot}, {}, KeyOf);
  return {found.begin(), found.end()};
}

void PlayNamedMethodSounds(NamedMethodFamily family, std::uint16_t subject_id, std::uint8_t slot,
                           std::uint32_t bearer_id, const ro::sim::CellPosition& bearer_cell,
                           const ro::sim::CellPosition& target_cell,
                           std::vector<SownNamedSound>& heard) {
  for (const NamedMethodSound& sound : NamedMethodSounds(family, subject_id, slot)) {
    if (sound.queued_per_level) {
      continue;
    }
    const bool on_target = sound.place == NamedMethodSoundPlace::kTarget;
    SownNamedSound played{sound.sound, on_target ? target_cell : bearer_cell};
    played.variants = sound.variants;
    played.carrier_id = on_target ? 0 : bearer_id;
    heard.push_back(played);
  }
}

void QueueNamedMethodSounds(std::uint16_t skill_id, std::uint8_t slot, std::int32_t level,
                            std::uint32_t bearer_id, const ro::sim::CellPosition& bearer_cell,
                            std::uint32_t started_ms, std::int32_t life_ticks,
                            std::vector<SownNamedSound>& heard) {
  for (const NamedMethodSound& sound :
       NamedMethodSounds(NamedMethodFamily::kSkill, skill_id, slot)) {
    if (!sound.queued_per_level) {
      continue;
    }
    for (std::int32_t rank = 0; rank < level; ++rank) {
      const std::int32_t tick = rank * sound.step_ticks;
      // L'instance joue sa file à chaque tic tant qu'elle vit : un bruit dû
      // après sa mort ne sonne pas.
      if (tick >= life_ticks) {
        break;
      }
      SownNamedSound queued{sound.sound, bearer_cell};
      queued.variants = sound.variants;
      queued.carrier_id = bearer_id;
      queued.due_ms = started_ms + static_cast<std::uint32_t>(tick * ro::render::kEffectTickMs);
      heard.push_back(queued);
    }
  }
}

std::int32_t NamedEffectLifeTicks(std::int32_t start_delay_ms, std::int32_t frame_count) {
  const std::int32_t delay_ticks =
      (std::max(start_delay_ms, 0) + ro::render::kEffectTickMs - 1) / ro::render::kEffectTickMs;
  return delay_ticks + std::max(frame_count, 0);
}

}  // namespace moonclient
