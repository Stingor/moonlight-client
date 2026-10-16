#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace moonclient {

// L'HEURE QU'IL FAIT SUR LE SERVEUR, de 0 à 24 en heures décimales : l'instant
// `now`, lu dans le fuseau `zone` — un nom de la base des fuseaux, comme
// « Europe/Paris ». Le passage à l'heure d'été y est compris.
//
// ⚠ L'INSTANT EST CELUI DE L'HORLOGE DU JOUEUR. Le serveur n'envoie que des
// instants, et une horloge système réglée sur le réseau ne s'en écarte que de
// quelques secondes : à l'échelle d'une journée, rien. C'est le FUSEAU qui
// manquait, et `moonclient.yml` le donne.
//
// Vide quand le système ne connaît pas ce fuseau.
[[nodiscard]] std::optional<float> ServerSkyHour(std::string_view zone,
                                                 std::chrono::system_clock::time_point now);

// « HH:MM » d'une heure décimale de 0 à 24 : la minute COMMENCÉE, comme une
// horloge, jamais arrondie à la suivante.
[[nodiscard]] std::string ServerClockText(float hour);

}  // namespace moonclient
