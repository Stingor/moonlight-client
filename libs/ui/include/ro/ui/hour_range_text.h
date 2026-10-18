#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ro::ui {

// UNE PLAGE D'HEURES DU SERVEUR, ses bornes en heures décimales de 0 à 24. La
// borne de fin peut précéder celle du début : la plage passe minuit.
struct HourRange {
  float from_hour = 0.0F;
  float to_hour = 0.0F;
};

// LA PLAGE EN TEXTE : « 20:00-06:00 », chaque borne sur 24 heures, à la minute
// la plus proche. La fin du jour s'écrit « 24:00 ».
[[nodiscard]] std::string HourRangeText(const HourRange& range);

// La plage lue dans `text`, avec tolérance : des espaces autour, un tiret, un
// demi-cadratin ou un cadratin entre les bornes, et chaque borne en « 20:30 »,
// « 20h30 », « 20h » ou « 20 ». Rien quand une borne sort du jour — une heure
// au-delà de 24, une minute au-delà de 59, ou « 24 » suivi de minutes.
[[nodiscard]] std::optional<HourRange> ParseHourRange(std::string_view text);

// La plage que porte le presse-papiers, s'il en porte une.
[[nodiscard]] std::optional<HourRange> ClipboardHourRange();

}  // namespace ro::ui
