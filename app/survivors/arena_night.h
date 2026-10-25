#pragma once

#include <cstdint>
#include <optional>

namespace moonclient {

// LA NUIT DE L'ARÈNE (U19) : le ciel est celui du RSW jusqu'à 7:00 de partie,
// puis l'heure du ciel descend de midi à minuit par paliers d'une minute, et
// minuit tient à partir de 15:00. Elle annonce les morts-vivants.
//
// L'heure se lit sur l'horloge de présentation, jamais sur `now_ms` : elle gèle
// avec la partie et se rejoue à l'identique.

inline constexpr std::uint64_t kArenaMsPerMinute = 60000;
// La première minute de la nuit, et celle où minuit tombe.
inline constexpr std::uint64_t kArenaNightFallsMs = 7 * kArenaMsPerMinute;
inline constexpr std::uint64_t kArenaMidnightMs = 15 * kArenaMsPerMinute;
// Un palier par minute.
inline constexpr std::uint64_t kArenaNightStepMs = kArenaMsPerMinute;
// L'heure du ciel au premier palier : midi, dont la lumière du sol est celle du
// RSW au bit près, si bien que la bascule ne se voit qu'à l'astre.
inline constexpr float kArenaNightFirstHour = 12.0F;
// L'heure du dernier palier, minuit, comptée après midi.
inline constexpr float kArenaNightLastHour = 24.0F;

// L'heure du ciel de l'arène à `presentation_ms`, dans [0, 24) ; rien tant que
// la lumière est celle du RSW.
[[nodiscard]] std::optional<float> ArenaSkyHour(std::uint64_t presentation_ms) noexcept;

}  // namespace moonclient
