#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

#include "ro/core/error.h"

#include "patcher/patch_fetcher.h"
#include "patcher/patch_run.h"

namespace moonclient::patcher {

// L'intervalle entre deux regards sur le serveur, une fois en jeu.
inline constexpr std::chrono::minutes kUpdateCheckPeriod{15};

// LE SERVEUR PUBLIE-T-IL PLUS NEUF QUE L'INSTALLATION ?
//
// Vrai quand la chaîne a un patch au-delà du dernier appliqué, ou quand le
// miroir publie un fichier absent, d'une autre taille, ou d'une autre
// empreinte que celle que le cache du miroir retient pour lui.
//
// 🔴 RIEN NE SE TÉLÉCHARGE NI NE SE HACHE : seules la liste et le manifeste
// passent sur le réseau, et un fichier dont le cache ne dit plus rien de sûr
// (taille ou date changées) ne compte pas. Le joueur est en pleine partie ;
// hacher une installation lui volerait des images, et le lancement suivant
// tranchera de toute façon.
//
// Une erreur seulement quand AUCUNE des deux sources n'a répondu.
[[nodiscard]] ro::core::Result<bool> ServerHasNewer(const PatchRunSettings& settings,
                                                    PatchFetcher& fetcher, std::stop_token stop);

// LE GUET DES MISES À JOUR, pendant que l'on joue.
//
// Un fil à lui regarde le serveur toutes les `period` — le premier regard
// après une première `period`, puisque le lancement vient de mettre
// l'installation à jour —, et s'arrête au premier qui trouve plus neuf : la
// nouvelle ne se dédit pas, et il n'y a rien de plus à apprendre.
//
// ⚠ IL NE FAIT QUE SAVOIR. Ni téléchargement ni relance en partie : la mise à
// jour s'installe au lancement suivant, quand le joueur le décide.
class UpdateWatch {
 public:
  UpdateWatch(PatchRunSettings settings, std::unique_ptr<PatchFetcher> fetcher,
              std::chrono::milliseconds period);
  UpdateWatch(const UpdateWatch&) = delete;
  UpdateWatch& operator=(const UpdateWatch&) = delete;
  UpdateWatch(UpdateWatch&&) = delete;
  UpdateWatch& operator=(UpdateWatch&&) = delete;
  // Arrête le fil, attente comprise ; une requête en cours va au bout de sa
  // borne de temps.
  ~UpdateWatch();

  [[nodiscard]] bool update_available() const noexcept {
    return update_available_.load(std::memory_order_acquire);
  }

 private:
  void Watch(std::stop_token stop);

  PatchRunSettings settings_;
  std::unique_ptr<PatchFetcher> fetcher_;
  std::chrono::milliseconds period_;
  std::atomic<bool> update_available_{false};
  std::mutex wait_lock_;
  std::condition_variable_any wake_;
  // ⚠ EN DERNIER : le fil lit tout ce qui précède, qui doit naître avant lui
  // et mourir après.
  std::jthread worker_;
};

}  // namespace moonclient::patcher
