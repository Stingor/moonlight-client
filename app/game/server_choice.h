#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "game/client_config.h"

namespace moonclient {

// LE CHOIX DU SERVEUR SUR L'ÉCRAN DE CONNEXION : quels serveurs proposer, lequel
// est un serveur TEST, et lequel présélectionner.
//
// 🔴 RIEN ICI NE TOUCHE AU RÉSEAU. La sonde du serveur test rend un booléen
// (voir `lobby/server_probe.h`), et ces règles le reçoivent tel quel : elles
// s'éprouvent sans écouteur ni délai.

// UN SERVEUR PROPOSÉ AU JOUEUR.
struct ServerChoice {
  // Le nom déclaré. ⚠ VIDE POUR LE SERVEUR TEST AJOUTÉ : son libellé se
  // traduit au dessin, dans la langue du moment.
  std::string name;
  ServerAddress address;
  // Vrai pour un serveur qu'il faut marquer TEST : adresse locale, ou celle du
  // serveur test.
  bool test = false;
  // Vrai pour l'entrée que la sonde a ajoutée, absente de `moonclient.yml`.
  bool probed = false;
  // L'ORIGINE DU SITE QUI IDENTIFIE SUR CE SERVEUR, ou vide : pas de porte du
  // site, le formulaire classique seul. Voir `WebLoginOriginOf`.
  std::string web_origin;
};

// LE SITE QUI IDENTIFIE SUR UN SERVEUR : celui que sa fiche déclare ; à défaut
// `default_origin` — sauf pour un serveur TEST, qui n'en a alors aucun.
//
// 🔴 LE SITE ÉCRIT LE MOT DE PASSE À USAGE UNIQUE DANS LA BASE DE SON SERVEUR.
// Celui de la production, présenté à un serveur test, s'y fait refuser comme
// un mauvais mot de passe : la porte ne s'offre donc pas, plutôt que d'échouer.
[[nodiscard]] std::string WebLoginOriginOf(std::string_view declared, bool test,
                                           std::string_view default_origin);

// Vrai pour une adresse de cette machine : `127.0.0.1`, `localhost`, `::1`.
[[nodiscard]] bool IsLocalHost(std::string_view host);

// Vrai quand deux adresses désignent le même serveur : même port, et même hôte
// — à la casse près, ou locaux tous les deux.
[[nodiscard]] bool SameServer(const ServerAddress& left, const ServerAddress& right);

// LA LISTE DU SÉLECTEUR : les serveurs de `moonclient.yml` dans l'ordre du
// fichier, puis le serveur test s'il a répondu et qu'aucune entrée ne le
// désigne déjà. `default_web_origin` : le site de la production.
[[nodiscard]] std::vector<ServerChoice> ComposeServerChoices(std::span<const ServerEntry> listed,
                                                             const ServerAddress& test_server,
                                                             bool test_detected,
                                                             std::string_view default_web_origin);

// La clé que retient le fichier de réglages : `hôte:port`. Le nom peut changer
// d'une configuration à l'autre, l'adresse désigne le serveur.
[[nodiscard]] std::string ServerMemoryKey(const ServerAddress& address);

// LE SERVEUR PRÉSÉLECTIONNÉ, au rang de `choices`.
//
// Le serveur retenu s'il est encore proposé ; à défaut le premier, qui est la
// production. 🔴 LE SERVEUR TEST RETENU QUI NE RÉPOND PLUS RAMÈNE AU PREMIER, en
// silence, même quand `moonclient.yml` le liste : un lancement ne doit pas
// viser un serveur qu'on sait éteint.
[[nodiscard]] std::size_t PreferredServer(std::span<const ServerChoice> choices,
                                          std::string_view remembered,
                                          const ServerAddress& test_server, bool test_detected);

// CE QUE L'ÉCRAN DE CONNEXION PROPOSE : la liste, et le rang choisi.
//
// ⚠ JAMAIS VIDE : `moonclient.yml` déclare au moins un serveur, et le
// lecteur refuse un fichier qui n'en a pas.
struct ServerOffer {
  std::vector<ServerChoice> entries;
  std::size_t chosen = 0;

  [[nodiscard]] const ServerChoice& current() const { return entries.at(chosen); }
};

// LA LISTE ET SA PRÉSÉLECTION, d'un seul geste : `ComposeServerChoices` puis
// `PreferredServer`.
[[nodiscard]] ServerOffer OfferServers(std::span<const ServerEntry> listed,
                                       const ServerAddress& test_server, bool test_detected,
                                       std::string_view remembered,
                                       std::string_view default_web_origin);

// Faut-il un sélecteur ? Pas pour un serveur seul : l'écran reste nu.
[[nodiscard]] inline bool OffersServerChoice(std::span<const ServerChoice> choices) {
  return choices.size() > 1;
}

}  // namespace moonclient
