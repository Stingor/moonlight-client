#pragma once

#include <string>
#include <string_view>

namespace ro::formats {

// L'ORIGINE D'UNE ADRESSE DE SERVICE WEB : le schéma, l'hôte et le port.
//
// Le client en tire toutes ses adresses web en y collant des chemins écrits en
// dur : `/emblem/upload`, `/emblem/download`, et les autres. L'adresse elle-même
// vient de la configuration du client (`web`, dans `moonclient.yml`).
//
// ⚠ ELLE S'ÉCRIT D'ORDINAIRE SANS SCHÉMA, et c'est alors du `http` en clair. Une
// adresse qui en porte déjà un le garde tel quel. La barre finale éventuelle
// est retirée : les chemins s'y collent avec la leur.
[[nodiscard]] std::string AssistOrigin(std::string_view address);

}  // namespace ro::formats
