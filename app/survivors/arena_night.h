#pragma once

#include <cstdint>
#include <optional>

namespace moonclient {

// LA NUIT DE L'ARÈNE (U19) : le ciel est celui du RSW jusqu'à 7:00 de partie,
// puis l'heure du ciel descend de midi vers le soir par paliers d'une minute,
// et le soir tient à partir de 15:00. Elle annonce les morts-vivants.
//
// 🔴 LE CIEL S'ARRÊTE À UN SOIR LISIBLE, pas à minuit : sous un ciel de minuit,
// le sol, la horde et les gemmes se confondent, et l'on ne peut plus rien
// éviter. Les heures de PARTIE (7:00, 15:00) restent celles du Codex, de la
// musique et du relevé : seule la lumière s'arrête plus tôt.
//
// L'heure se lit sur l'horloge de présentation, jamais sur `now_ms` : elle gèle
// avec la partie et se rejoue à l'identique.

inline constexpr std::uint64_t kArenaMsPerMinute = 60000;
// La première minute de la nuit, et celle où la nuit est entière.
inline constexpr std::uint64_t kArenaNightFallsMs = 7 * kArenaMsPerMinute;
inline constexpr std::uint64_t kArenaMidnightMs = 15 * kArenaMsPerMinute;
// Un palier par minute.
inline constexpr std::uint64_t kArenaNightStepMs = kArenaMsPerMinute;
// L'heure du ciel au premier palier : midi, dont la lumière du sol est celle du
// RSW au bit près, si bien que la bascule ne se voit qu'à l'astre.
inline constexpr float kArenaNightFirstHour = 12.0F;
// L'heure du dernier palier : le soir où le ciel s'arrête. Dix-neuf heures, le
// soleil encore au-dessus de l'horizon : le sol y garde deux fois la lumière
// de minuit. Une heure plus tard, l'astre se couche, et le sol est plus noir
// qu'à minuit.
inline constexpr float kArenaNightLastHour = 19.0F;
// L'heure du ciel vit dans [0, 24) : le soir n'y fait pas le tour du cadran.
inline constexpr float kArenaHoursPerDay = 24.0F;
static_assert(kArenaNightFirstHour < kArenaNightLastHour &&
              kArenaNightLastHour < kArenaHoursPerDay);

// LA PART DE NUIT TOMBÉE à `presentation_ms`, de zéro au premier palier (7:00)
// à un au dernier (15:00), par les mêmes paliers que le ciel ; rien avant 7:00.
// Ce qui grandit avec la nuit — la lanterne du héros, la lueur des objets — la
// suit.
[[nodiscard]] std::optional<float> ArenaNightShare(std::uint64_t presentation_ms) noexcept;

// L'heure du ciel de l'arène à `presentation_ms`, dans [0, 24) ; rien tant que
// la lumière est celle du RSW.
[[nodiscard]] std::optional<float> ArenaSkyHour(std::uint64_t presentation_ms) noexcept;

}  // namespace moonclient
