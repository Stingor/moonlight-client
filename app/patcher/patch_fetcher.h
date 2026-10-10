#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

#include "ro/core/error.h"

namespace moonclient::patcher {

// OÙ EN EST UN TÉLÉCHARGEMENT.
struct FetchProgress {
  std::uint64_t received_bytes = 0;
  // Ce que le serveur annonce, s'il l'annonce.
  std::optional<std::uint64_t> announced_bytes;
};

using FetchProgressSink = std::function<void(const FetchProgress&)>;

// CE QUE LE PATCHER DEMANDE AU RÉSEAU : un texte, ou un fichier sur le disque.
//
// 🔴 BLOQUANT, ET C'EST VOULU. Le patcher tourne tout entier sur un fil à lui
// — lister, télécharger, vérifier, fusionner — et la boucle du client ne fait
// que lire sa progression. Une chaîne de patchs est une SÉQUENCE : la rendre
// asynchrone pièce par pièce la rendrait illisible sans rien gagner.
//
// ⚠ ABSTRAIT POUR ÊTRE ESSAYABLE : derrière cette interface, un essai répond
// ce qu'il veut, y compris une coupure en plein fichier.
//
// ⚠ CHAQUE APPEL EST BORNÉ : la taille vient du dehors. Au-delà de la borne,
// l'appel échoue et ne laisse rien derrière lui.
class PatchFetcher {
 public:
  PatchFetcher() = default;
  PatchFetcher(const PatchFetcher&) = delete;
  PatchFetcher& operator=(const PatchFetcher&) = delete;
  PatchFetcher(PatchFetcher&&) = delete;
  PatchFetcher& operator=(PatchFetcher&&) = delete;
  virtual ~PatchFetcher() = default;

  [[nodiscard]] virtual ro::core::Result<std::string> FetchText(const std::string& url,
                                                                std::uint64_t limit_bytes,
                                                                std::stop_token stop) = 0;

  // ⚠ LE FICHIER N'APPARAÎT QU'ENTIER : on écrit à côté, et on ne renomme
  // qu'une fois le dernier octet reçu. Un arrêt en route ne laisse jamais un
  // fichier tronqué sous le nom attendu.
  [[nodiscard]] virtual ro::core::Result<void> FetchToFile(const std::string& url,
                                                           const std::filesystem::path& destination,
                                                           std::uint64_t limit_bytes,
                                                           const FetchProgressSink& on_progress,
                                                           std::stop_token stop) = 0;
};

}  // namespace moonclient::patcher
