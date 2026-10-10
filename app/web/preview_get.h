#pragma once

#include <cstddef>
#include <string_view>

#include "ro/core/web_preview_policy.h"

namespace moonclient {

// UN SAUT DE L'APERÇU D'UNE ADRESSE : une requête GET par le réseau du système
// (WinHTTP, libcurl), sans suivre de redirection — la boucle des sauts, pure,
// est `ro::core::FetchPreviewBytes`, qui revérifie l'hôte à chaque saut.
//
// 🔴 HTTPS SEULEMENT, et l'hôte contacté est celui que la liste blanche a jugé
// (`ro::core::ParseWebAddress`) : quand le lecteur d'adresse du système et le
// nôtre divergent, rien ne part.
//
// Les délais et l'en-tête `Accept` sont ceux de `web_preview_policy.h`. Le corps
// s'arrête à `most_bytes` ; une taille annoncée au-delà se rend sans corps.
//
// ⚠ BLOQUANT : à appeler depuis un fil à part, jamais depuis la boucle.
[[nodiscard]] ro::core::PreviewReply PreviewGet(std::string_view url, std::size_t most_bytes);

}  // namespace moonclient
