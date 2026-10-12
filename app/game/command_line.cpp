#include "game/command_line.h"

#include <cstdio>

#include "ro/core/build_flavor.h"
#include "ro/core/utf8_path.h"

namespace moonclient {
namespace {

// Les options d'essai, chacune suivie d'un chemin.
constexpr std::string_view kFolderMarker = "--dossier";
constexpr std::string_view kArchivesMarker = "--archives";
constexpr std::string_view kCaptureMarker = "--capture";

constexpr int kUsageExitCode = 2;

// Lit les options d'essai, ou rend faux sur une option inconnue ou sans valeur.
[[maybe_unused]] bool ParseDevOptions(std::span<const std::string_view> args, Launch& launch) {
  for (std::size_t index = 0; index < args.size(); ++index) {
    // ⚠ UN ARGUMENT VIDE N'EST PAS UN ARGUMENT : un raccourci qui passe `""`
    // n'obtiendrait sinon que le mode d'emploi, sur une sortie que personne ne
    // lit.
    if (args[index].empty()) {
      continue;
    }
    if (index + 1 >= args.size() || args[index + 1].empty()) {
      return false;
    }
    // Les arguments arrivent en UTF-8 : voir `ProgramArguments`.
    const std::filesystem::path value = ro::core::PathFromUtf8(args[index + 1]);
    if (args[index] == kFolderMarker) {
      launch.install_folder = value;
    } else if (args[index] == kArchivesMarker) {
      launch.archive_folder = value;
    } else if (args[index] == kCaptureMarker) {
      launch.capture_file = value;
    } else {
      return false;
    }
    ++index;
  }
  return true;
}

}  // namespace

int Usage() {
  std::fputs(
      "Moonlight-Destiny — client Ragnarok\n"
      "\n"
      "Jouer :\n"
      "  Moonlight-Destiny\n"
      "\n"
      "  Le dossier du jeu est celui de l'exécutable, et le serveur celui\n"
      "  que moonclient.yml, à côté de lui, déclare en PREMIER.\n"
      "\n"
      "Options d'essai, construction Debug seulement :\n"
      "  --dossier <chemin>   le dossier du jeu : moonclient.yml, réglages,\n"
      "                       skin déposé ; l'exécutable n'y est pas\n"
      "  --archives <chemin>  où lire les archives que moonclient.yml nomme,\n"
      "                       sans rien y écrire\n"
      "  --capture <fichier>  enregistre le flux du serveur de zone, que\n"
      "                       roc-cli net frames redécoupe hors du jeu\n",
      stderr);
  return kUsageExitCode;
}

std::optional<Launch> ParseLaunch([[maybe_unused]] std::span<const std::string_view> args) {
  Launch launch;
  // 🔴 LA CONSTRUCTION LIVRÉE NE LIT AUCUN ARGUMENT. Le joueur lance par un
  // double-clic ; ce qu'un raccourci ajouterait ne doit rien changer à ce
  // qu'il obtient.
  if constexpr (ro::core::kDevTools) {
    if (!ParseDevOptions(args, launch)) {
      return std::nullopt;
    }
  }
  return launch;
}

}  // namespace moonclient
