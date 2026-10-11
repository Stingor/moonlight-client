#pragma once

#include <cstdint>

#include "ro/sim/status_effects.h"

namespace ro::net::status_chat {

// LES TEINTES DU CLIENT, en `0xRRGGBB` : cyan pour une pose, rouge pour une
// levée. Quelques états prennent l'autre, et leur table le dit.
inline constexpr std::uint32_t kPoseColor = 0x00FFFF;
inline constexpr std::uint32_t kLiftColor = 0xFF0000;

// Une phrase traduite, et sa teinte ; `text` nul pour « rien à écrire ».
struct Phrase {
  const char* text = nullptr;
  std::uint32_t color_rgb = 0;
};

// LES PHRASES QUE LES DEUX FORMES PARTAGENT, en cyan pour une pose et en rouge
// pour une levée : buffs de première classe, nourritures, objets de la
// boutique. Nul pour un état hors de la table.
[[nodiscard]] const char* SharedPoseText(sim::StatusEffectId effect);
[[nodiscard]] const char* SharedLiftText(sim::StatusEffectId effect);

// LES POSES QUE SEULE LA FORME À DURÉE ÉCRIT : celles des classes 4.
[[nodiscard]] Phrase TimedPosePhrase(sim::StatusEffectId effect);

// LES LEVÉES QUE SEULE LA FORME NUE ÉCRIT : celles des classes 4.
[[nodiscard]] Phrase BareLiftPhrase(sim::StatusEffectId effect);

}  // namespace ro::net::status_chat
