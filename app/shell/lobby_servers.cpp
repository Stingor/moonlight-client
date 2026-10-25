// LE SERVEUR DE L'ÉCRAN DE CONNEXION : le proposer, en changer, et le dire.
//
// ⚠ MÊME CLASSE, AUTRE UNITÉ DE TRADUCTION, comme la porte Moonlight et la
// mise en page de la tablée. Les règles du choix vivent dans
// `game/server_choice.h` ; ici, on ne fait que les montrer et les suivre.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/log.h"

#include "shell/lobby_screen.h"
#include "world/map_stage.h"

namespace moonclient {
namespace {

// Le nom que le journal donne au serveur test qu'aucune ligne de
// `moonclient.yml` ne nomme. ⚠ EN FRANÇAIS, comme tout le journal.
constexpr const char* kProbedServerLogName = "serveur test";

// Ce qu'ajoute le journal à la ligne d'un serveur test. Le même mot que la
// pastille et le titre : on le cherche avec un seul motif.
[[nodiscard]] std::string_view TestMention(const ServerChoice& server) {
  return server.test ? " [TEST]" : "";
}

// Le nom que le journal donne au serveur : celui du fichier, ou son adresse.
[[nodiscard]] std::string LoggedName(const ServerChoice& server) {
  return server.probed ? std::string(kProbedServerLogName) : server.name;
}

}  // namespace

void LobbyScreen::ShowServer() {
  const ServerChoice& server = servers_.current();
  shell_.SetTestServer(server.test);
  shell_.SetLoginAddress(server.address.host + ":" + std::to_string(server.address.port));
  shell_.gl().SetTitle(WindowTitle(server.test));
  // LA PORTE DU SITE VISE LE SITE DE CE SERVEUR. Sans site, elle reste où
  // elle est : `Compose` la masque, et le formulaire classique s'affiche.
  if (!server.web_origin.empty() && server.web_origin != web_.origin()) {
    web_wire_.SetOrigin(server.web_origin);
    web_.Retarget(server.web_origin);
    form_.web.chosen = -1;
  }
}

void LobbyScreen::NoteConnectingServer() const {
  const ServerChoice& server = servers_.current();
  ro::core::LogInfo("connexion à {} — {}:{}{}", LoggedName(server), server.address.host,
                    server.address.port, TestMention(server));
}

void LobbyScreen::ComposeServers(ro::ui::LoginWindowView& view) const {
  if (!OffersServerChoice(servers_.entries)) {
    return;
  }
  for (const ServerChoice& one : servers_.entries) {
    view.servers.push_back(ro::ui::LoginServer{
        .name = one.probed ? std::string(ro::core::Tr("Serveur test (local)")) : one.name,
        .test = one.test});
  }
  view.server = static_cast<int>(servers_.chosen);
}

void LobbyScreen::RouteServer(int rank, std::uint32_t now_ms) {
  if (rank < 0 || static_cast<std::size_t>(rank) >= servers_.entries.size() ||
      static_cast<std::size_t>(rank) == servers_.chosen) {
    return;
  }
  const ServerChoice& wanted = servers_.entries[static_cast<std::size_t>(rank)];
  // ⚠ LA MACHINE À ÉTATS PEUT REFUSER : une connexion part peut-être déjà. Le
  // sélecteur garde alors l'ancien choix, et rien n'est retenu.
  if (!flow_.Retarget(wanted.address.host, wanted.address.port)) {
    return;
  }
  servers_.chosen = static_cast<std::size_t>(rank);
  ro::core::LogInfo("serveur choisi : {} — {}:{}{}", LoggedName(wanted), wanted.address.host,
                    wanted.address.port, TestMention(wanted));
  // 🔴 UNE IDENTIFICATION EN COURS VALAIT POUR L'AUTRE SERVEUR : son jeton et
  // son mot de passe à usage unique y sont écrits, pas ici. Elle s'abandonne,
  // et les champs tapés restent.
  web_.Forget();
  form_.web.chosen = -1;
  ShowServer();
  shell_.settings().options().login_memory().server = ServerMemoryKey(wanted.address);
  shell_.settings().file().Touch();
  // 🔴 LA VILLE SUIT LE SERVEUR : celle d'avant reste à l'écran jusqu'à ce que
  // la nouvelle arrive. Éteinte par le joueur, elle ne se rallume pas.
  if (shell_.settings().options().backdrop_wanted()) {
    flow_.OpenBackdrop(now_ms);
  }
}

}  // namespace moonclient
