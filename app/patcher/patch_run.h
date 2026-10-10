#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>

#include "ro/core/error.h"

#include "game/client_config.h"
#include "patcher/patch_fetcher.h"

namespace moonclient::patcher {

// LE DOSSIER DES TÉLÉCHARGEMENTS, dans l'installation. Une archive y attend
// d'être appliquée, puis s'efface.
inline constexpr std::string_view kDownloadFolderName = "moonclient-patch-cache";

// Le suffixe que prend l'exécutable qu'un patch remplace pendant qu'il tourne.
inline constexpr std::string_view kReplacedExecutableSuffix = ".old";

// CE QUE LE PATCHER EST EN TRAIN DE FAIRE.
enum class PatchPhase : std::uint8_t {
  kListing,      // on demande la liste au serveur
  kDownloading,  // une archive arrive
  kApplying,     // une archive s'applique
  kDone,         // l'installation est à jour
  kFailed,       // arrêté sur une erreur : `failure` dit laquelle
  kStopped,      // arrêté à la demande
};

struct PatchProgress {
  PatchPhase phase = PatchPhase::kListing;
  // Le patch en cours, compté à partir de 1, sur ceux qui restaient.
  std::size_t patch_number = 0;
  std::size_t patch_count = 0;
  std::string archive_name;
  FetchProgress download;
  std::string failure;
};

// LE TABLEAU où le fil du patcher écrit, et que la boucle du client lit.
//
// ⚠ UNE COPIE À CHAQUE LECTURE : le fil écrit pendant que la boucle dessine,
// et une vue sur l'état partagé se déchirerait entre deux champs.
class PatchProgressBoard {
 public:
  void Publish(const PatchProgress& progress) {
    const std::lock_guard<std::mutex> held(lock_);
    progress_ = progress;
  }
  [[nodiscard]] PatchProgress Snapshot() const {
    const std::lock_guard<std::mutex> held(lock_);
    return progress_;
  }

 private:
  mutable std::mutex lock_;
  PatchProgress progress_;
};

struct PatchRunSettings {
  PatcherConfig config;
  // Le dossier du jeu : les patchs y écrivent, l'état s'y garde.
  std::filesystem::path install_root;
  // L'exécutable qui tourne. Un patch qui le remplace le RENOMME d'abord :
  // Windows refuse d'écraser un exécutable en cours, pas de le renommer.
  std::filesystem::path running_executable;
};

struct PatchRunOutcome {
  std::size_t applied_patches = 0;
  // Vrai : l'exécutable a été remplacé, et le client doit se relancer pour
  // être celui que la chaîne décrit.
  bool executable_replaced = false;
};

// APPLIQUE CE QUI MANQUE DE LA CHAÎNE, dans l'ordre, un patch après l'autre.
//
// Pour chacun : télécharger, vérifier l'empreinte de la liste, ouvrir
// l'archive et vérifier son intégrité interne, l'appliquer, puis enregistrer
// son index. L'état n'avance qu'après un patch ENTIÈREMENT appliqué : un arrêt
// en route fait reprendre ce patch-là au lancement suivant.
//
// 🔴 BLOQUANT, et fait pour un fil à part. `stop` s'éprouve entre deux
// morceaux de téléchargement et entre deux patchs ; un patch en cours
// d'application va au bout, parce qu'une moitié de patch est pire qu'un
// patch.
//
// ⚠ L'APPELANT TIENT LE VERROU DE L'INSTALLATION : deux clients qui patchent
// le même dossier en même temps se marcheraient dessus.
[[nodiscard]] ro::core::Result<PatchRunOutcome> RunPatches(const PatchRunSettings& settings,
                                                           PatchFetcher& fetcher,
                                                           PatchProgressBoard& board,
                                                           std::stop_token stop);

// Efface l'exécutable qu'un patch précédent a remplacé, s'il traîne encore.
// Au lancement suivant, il ne tourne plus : rien ne s'y oppose.
void RemoveReplacedExecutable(const std::filesystem::path& running_executable);

}  // namespace moonclient::patcher
