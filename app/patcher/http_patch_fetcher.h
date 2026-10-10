#pragma once

#include "patcher/patch_fetcher.h"

namespace moonclient::patcher {

// LE RÉSEAU DU PATCHER : celui du système (WinHTTP, libcurl), en GET, en HTTPS
// seulement.
//
// ⚠ LA VALIDATION DU CERTIFICAT EST CELLE DU SYSTÈME, et rien ne l'assouplit :
// une archive de patch écrit des exécutables dans le dossier du jeu.
// ⚠ UNE REDIRECTION VERS DU CLAIR EST REFUSÉE.
class HttpPatchFetcher final : public PatchFetcher {
 public:
  [[nodiscard]] ro::core::Result<std::string> FetchText(const std::string& url,
                                                        std::uint64_t limit_bytes,
                                                        std::stop_token stop) override;

  [[nodiscard]] ro::core::Result<void> FetchToFile(const std::string& url,
                                                   const std::filesystem::path& destination,
                                                   std::uint64_t limit_bytes,
                                                   const FetchProgressSink& on_progress,
                                                   std::stop_token stop) override;
};

}  // namespace moonclient::patcher
