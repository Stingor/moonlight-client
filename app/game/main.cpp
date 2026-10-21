// LE POINT D'ENTRÉE, ET RIEN D'AUTRE.
//
// ⚠ IL NE FAIT QUE TROIS CHOSES : ouvrir le journal et le filet à plantages, lire
// la configuration, et remettre le reste à l'écran de connexion puis à la
// partie. Tout ce qui fait le jeu vit dans `game.h`, où chaque étape voit l'état
// sans qu'on ait à le lui passer.
//
// Ce fichier ne doit plus grossir : ce qui s'ajoute au client s'ajoute à la
// partie, pas au démarrage du programme.

#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/byte_source.h"
#include "ro/core/crash_report.h"
#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/core/program_arguments.h"
#include "ro/core/startup_report.h"
#include "ro/core/utf8_path.h"
#include "ro/net/connection.h"
#include "ro/sim/world.h"

#include "game/client_config.h"
#include "game/client_shell.h"
#include "game/command_line.h"
#include "game/game.h"
#include "game/server_choice.h"
#include "lobby/server_probe.h"
#include "patcher/patch_stage.h"
#include "shell/lobby_screen.h"
#include "web/web_login.h"

namespace {

// Le titre de la boîte qui dit pourquoi le client s'arrête.
constexpr const char* kStopTitle = "Moonlight-Destiny";
constexpr const char* kLogFileName = "moonclient.log";

int ReportError(const ro::core::Error& error) {
  // ⚠ CE QU'AUCUNE AUTRE LIGNE NE DIRA : le processus s'arrête ici. Le préfixe
  // « erreur » ne faisait que répéter la gravité que le motif du journal
  // imprime déjà.
  const std::string why = ro::core::Describe(error);
  ro::core::LogError("le client s'arrête : {}", why);
  // 🔴 ET À L'ÉCRAN, PAS SEULEMENT AU JOURNAL. Le client livré n'a pas de
  // console : sans cette boîte, il s'arrête sans que rien ne s'ouvre, et le
  // joueur relance un programme qu'il croit lent. La boîte ne demande pas que
  // SDL soit démarré, donc elle dit aussi ce qui échoue avant la fenêtre.
  const std::string shown = "Le client s'arrête :\n\n" + why + "\n\nDétails : moonclient.log";
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kStopTitle, shown.c_str(), nullptr);
  return 1;
}

// D'OÙ VIENNENT LE DOSSIER ET LE SERVEUR.
//
// 🔴 L'EXÉCUTABLE SE TIENT DANS L'INSTALLATION, et `moonclient.yml`, à côté de
// lui, déclare ses serveurs — voir `game/client_config.h`. C'est pour cela
// qu'aucune adresse n'est écrite dans ce programme : livrer un client qui
// connaît son serveur par cœur, c'est recompiler pour changer de serveur.
//
// ⚠ LA RACINE EST CELLE DE L'EXÉCUTABLE, et non le dossier courant. Les deux
// coïncident sur un double-clic depuis l'explorateur, mais pas sur un
// raccourci dont on a changé le dossier de départ. Seule une option d'essai
// (`--dossier`) la déplace.
[[nodiscard]] ro::core::Result<void> ResolveInstall(moonclient::Launch& launch) {
  if (launch.install_folder.empty()) {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
      return ro::core::Fail(
          ro::core::ErrorCode::kIoFailure,
          std::string("impossible de savoir où se trouve l'exécutable : ") + SDL_GetError());
    }
    // ⚠ SDL REND DE L'UTF-8 : lu comme un `char*` ordinaire, un dossier
    // « Ragnarök » s'abîmerait en page ANSI et la configuration deviendrait
    // introuvable.
    launch.install_folder = ro::core::PathFromUtf8(base);
  }
  if (launch.archive_folder.empty()) {
    launch.archive_folder = launch.install_folder;
  }

  const std::filesystem::path path =
      launch.install_folder / std::filesystem::path(moonclient::kClientConfigFileName);
  auto source = ro::core::FileByteSource::Open(path);
  if (!source) {
    return ro::core::Fail(ro::core::ErrorCode::kNotFound,
                          "pas de configuration : " + ro::core::Utf8FromPath(path) +
                              " — cet exécutable doit vivre DANS le dossier du jeu, à côté de"
                              " son moonclient.yml");
  }
  auto bytes = (*source)->ReadBlock(0, (*source)->size());
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  auto config = moonclient::ParseClientConfig(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!config) {
    return std::unexpected(config.error());
  }
  launch.servers = config->servers;
  launch.test_server = config->test_server;
  launch.web = config->web;
  launch.server_time_zone = config->server_time_zone;
  launch.archives = config->archives;
  launch.patcher = config->patcher;

  ro::core::LogInfo("installation : {} — {} archive(s) déclarée(s), lues dans {}",
                    ro::core::Utf8FromPath(launch.install_folder), launch.archives.size(),
                    ro::core::Utf8FromPath(launch.archive_folder));
  return {};
}

// LA PARTIE, une fois que l'écran de connexion a fait entrer le joueur.
//
// `back_to_characters` dit en sortant si le joueur a demandé, par le menu
// d'Échap, à revenir à la sélection du personnage, et si le serveur l'a
// accordé. `link_lost` porte le motif quand la partie s'est arrêtée sur une
// liaison perdue dont le joueur a pris acte, et reste vide sinon.
int Play(moonclient::ClientShell& shell, const std::string& map_name, ro::net::Connection link,
         ro::sim::World world, const ro::net::ZoneTicket& ticket, bool& back_to_characters,
         std::string& link_lost) {
  moonclient::Game game(shell, std::move(link), std::move(world), ticket);
  if (auto opened = game.Open(map_name); !opened) {
    return ReportError(opened.error());
  }
  if (auto played = shell.Run(game); !played) {
    return ReportError(played.error());
  }
  back_to_characters = game.back_to_characters();
  link_lost = game.link_lost_notice();
  return 0;
}

// L'ÉCRAN DE CONNEXION, puis la partie qu'il ouvre.
//
// 🔴 ET ELLE BOUCLE : « Sélection du personnage », dans le menu d'Échap, rend la
// main ici, et la tablée se rouvre avec les jetons de la connexion — sans
// formulaire ni mot de passe. Voir `ro/net/character_select_return.h`.
//
// 🔴 UNE LIAISON PERDUE RAMÈNE AU FORMULAIRE, et non à la tablée : sans
// serveur, les jetons de la connexion ne valent plus rien. Le motif s'affiche
// sous les champs.
int Connect(moonclient::ClientShell& shell, moonclient::ServerOffer servers) {
  std::optional<moonclient::lobby::Comeback> comeback;
  std::string link_lost;
  ro::ui::LoginDoor door = ro::ui::LoginDoor::kMoonlight;
  while (true) {
    moonclient::LobbyScreen lobby(shell, servers);
    // Chaque passage par l'écran ouvre une liaison de zone neuve, donc un
    // fichier de capture à elle : voir `ZoneStreamCapture`.
    lobby.ObserveZone(shell.ObserveZoneLink());
    if (comeback.has_value()) {
      lobby.ResumeAt(*comeback, door);
    } else if (!link_lost.empty()) {
      lobby.ReturnWithNotice(std::exchange(link_lost, std::string()), door);
    }
    if (auto opened = lobby.Open(static_cast<std::uint32_t>(SDL_GetTicks())); !opened) {
      return ReportError(opened.error());
    }
    if (auto shown = shell.Run(lobby); !shown) {
      return ReportError(shown.error());
    }
    // ⚠ NE PAS ÊTRE ENTRÉ N'EST PAS UN ÉCHEC : le joueur a fermé la fenêtre.
    if (!lobby.entered()) {
      return 0;
    }
    const moonclient::lobby::Comeback next = lobby.comeback();
    door = lobby.door();
    // Le serveur que le joueur a choisi : c'est lui que l'écran rouvre.
    servers = lobby.servers();
    bool back_to_characters = false;
    const int played = Play(shell, lobby.map_name(), lobby.TakeLink(), lobby.TakeWorld(),
                            lobby.ticket(), back_to_characters, link_lost);
    if (played == 0 && !link_lost.empty()) {
      comeback.reset();
      continue;
    }
    if (!back_to_characters || !next.usable()) {
      return played;
    }
    ro::core::LogInfo("retour à la sélection du personnage");
    comeback = next;
  }
}

// LE SERVEUR QUE L'ÉCRAN DE CONNEXION VISE EN S'OUVRANT : le dernier choisi,
// sinon le premier du fichier. Voir `game/server_choice.h`.
//
// ⚠ LE JOURNAL DIT LEQUEL, et ce n'est pas du bavardage : la production et le
// serveur d'essai peuvent ne différer QUE par le port. Rien à l'écran ne
// distinguerait l'un de l'autre, sans la marque TEST.
moonclient::ServerOffer ChooseServer(moonclient::ClientShell& shell,
                                     const moonclient::Launch& launch,
                                     const moonclient::lobby::ProbeInFlight& probe) {
  const bool detected =
      probe.Detected(std::chrono::milliseconds(moonclient::lobby::kTestServerProbeMs));
  ro::core::LogInfo("serveur test {}:{} : {}", launch.test_server.host, launch.test_server.port,
                    detected ? "détecté" : "absent");
  moonclient::ServerOffer offer = moonclient::OfferServers(
      launch.servers, launch.test_server, detected,
      shell.settings().options().login_memory().server, moonclient::web::kWebLoginOrigin);
  const moonclient::ServerChoice& server = offer.current();
  ro::core::LogInfo("serveur : {} — {}:{}{} (sur {} proposé(s))",
                    server.probed ? "serveur test" : server.name, server.address.host,
                    server.address.port, server.test ? " [TEST]" : "", offer.entries.size());
  return offer;
}

// Un dossier qui refuse l'écriture n'arrête pas le client, mais le joueur doit
// le savoir : ni ses réglages, ni le patch, ni un rapport de plantage n'y
// survivront.
void WarnUnwritable(const ro::core::Error& error) {
  const std::string why = ro::core::Describe(error);
  ro::core::LogError("installation : {} — réglages, patch et rapports seront perdus", why);
  const std::string shown =
      "Le dossier du jeu refuse l'écriture :\n\n" + why +
      "\n\nLe jeu risque de se fermer ou de ne rien enregistrer. Causes habituelles : le jeu est"
      " installé dans Program Files, ou un antivirus (protection contre les rançongiciels) bloque"
      " MoonClient.exe. Déplacez le dossier du jeu (par exemple C:\\Jeux) ou autorisez"
      " l'exécutable dans l'antivirus.";
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, kStopTitle, shown.c_str(), nullptr);
}

// LE CLIENT, du dossier d'installation à la fermeture de la partie.
int Run(int argc, char** argv) {
  const std::vector<std::string> owned = ro::core::ProgramArguments(argc, argv);
  const std::vector<std::string_view> args(owned.begin(), owned.end());
  std::optional<moonclient::Launch> launch = moonclient::ParseLaunch(args);
  if (!launch.has_value()) {
    return moonclient::Usage();
  }
  if (auto resolved = ResolveInstall(*launch); !resolved) {
    return ReportError(resolved.error());
  }
  // 🔴 LA SONDE PART TOUT DE SUITE, EN FOND : le patch et les archives lui
  // laissent largement son délai avant que l'écran de connexion ne la lise.
  const moonclient::lobby::ProbeInFlight test_probe = moonclient::lobby::ProbeInFlight::Launch(
      std::make_shared<moonclient::lobby::TcpServerProbe>(), launch->test_server.host,
      launch->test_server.port);
  if (auto writable = ro::core::ProbeWritableFolder(launch->install_folder); !writable) {
    WarnUnwritable(writable.error());
  }
  moonclient::ClientShell shell(launch->install_folder, launch->archive_folder, launch->archives,
                                launch->web);
  shell.SetServerTimeZone(launch->server_time_zone);
  if (!launch->capture_file.empty()) {
    shell.CaptureZoneStreams(launch->capture_file);
  }
  if (auto opened = shell.OpenWindow(moonclient::kDefaultWidth, moonclient::kDefaultHeight);
      !opened) {
    return ReportError(opened.error());
  }
  // ⚠ LE PILOTE GRAPHIQUE ET LES SURCOUCHES SE CHARGENT AVEC LA FENÊTRE : c'est
  // après elle qu'ils se voient, et qu'ils ont pu prendre la place du filet.
  ro::core::RearmCrashReporter("avec la fenêtre");
  ro::core::LogForeignModules("après la fenêtre");
  // 🔴 LE PATCH PASSE ENTRE LA FENÊTRE ET LES ARCHIVES : il les réécrit.
  if (launch->patcher.has_value()) {
    auto ended = moonclient::patcher::UpdateInstall(shell, *launch->patcher);
    if (!ended) {
      return ReportError(ended.error());
    }
    if (*ended == moonclient::patcher::PatchStageEnd::kQuit ||
        *ended == moonclient::patcher::PatchStageEnd::kRestart) {
      return 0;
    }
  } else {
    ro::core::LogInfo("démarrage : pas de patcheur déclaré dans moonclient.yml");
  }
  if (auto archived = shell.OpenArchives(); !archived) {
    return ReportError(archived.error());
  }
  ro::core::LogInfo("démarrage : terminé, écran de connexion");
  return Connect(shell, ChooseServer(shell, *launch, test_probe));
}

}  // namespace

int main(int argc, char** argv) {
  // LE FILET D'ABORD, avant même le journal : ouvrir celui-ci peut déjà
  // échouer, et le rapport n'a besoin que de son dossier.
  //
  // LE DOSSIER DES RAPPORTS est celui d'où l'on lance, à côté du journal : un
  // joueur qui signale une panne trouve les deux ensemble, sans avoir à chercher
  // dans les dossiers cachés du système.
  std::error_code no_folder;
  ro::core::InstallCrashReporter(std::filesystem::current_path(no_folder));
  // Le journal AVANT tout le reste : ce qui échoue pendant la lecture des
  // arguments doit se dire comme le reste, et au même endroit.
  //
  // ⚠ ET IL VA DANS UN FICHIER, pas seulement sur la console. Une console se
  // ferme avec la fenêtre, défile, ou n'existe pas selon la façon dont le client
  // a été lancé — si bien qu'« aucune ligne » ne distingue pas un client muet
  // d'un journal que personne n'a lu. Le fichier est dans le dossier courant —
  // celui de l'exécutable sur un double-clic —, et il répond quand la console
  // ne répond plus.
  if (auto logged = ro::core::InitLogging(kLogFileName); !logged) {
    // Le seul endroit où le dire est l'écran : le journal est justement ce qui
    // manque.
    const std::string shown = "Le journal ne peut pas s'écrire :\n\n" +
                              ro::core::Describe(logged.error()) +
                              "\n\nSi le jeu se ferme, déplacez son dossier hors de Program Files"
                              " ou autorisez MoonClient.exe dans l'antivirus.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, kStopTitle, shown.c_str(), nullptr);
  }
  ro::core::LogMachine();
  ro::core::LogForeignModules("au démarrage");

  const int exit_code = Run(argc, argv);
  ro::core::EndLogFile(std::format("le client se ferme (code {})", exit_code));
  return exit_code;
}
