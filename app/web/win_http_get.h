#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace moonclient {

// CE QUE WINHTTP CONTACTERA pour `PreviewGet` : l'hôte, le port, et le chemin
// avec sa requête, sans le fragment.
struct WinHttpTarget {
  std::wstring host;
  std::uint16_t port = 0;
  std::wstring path_and_query;
};

// 🔴 L'ADRESSE DÉCOUPÉE PAR WINHTTP, SEULEMENT SI SON HÔTE EST CELUI QUE LA
// LISTE BLANCHE A JUGÉ (`ro::core::ParseWebAddress`). Deux lecteurs d'adresse
// divergent sur les formes tordues — « a@b@hôte », un « \ » avant un « @ » — ;
// quand ils divergent, rien ne part. Rien non plus hors de HTTPS.
[[nodiscard]] std::optional<WinHttpTarget> CrackJudgedUrl(std::string_view url);

}  // namespace moonclient
