#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "web/web_transport.h"

namespace moonclient::web {

// COMBIEN D'OCTETS ON ACCEPTE DE LIRE D'UNE RÉPONSE.
//
// 🔴 UNE BORNE, PARCE QUE LA TAILLE VIENT DU DEHORS. Les réponses du site font
// quelques centaines d'octets ; une réponse sans fin — un serveur en panne, un
// portail captif qui diffuse, un proxy bavard — remplirait la mémoire sans
// qu'aucune couche plus haut ne s'en aperçoive. Le lecteur JSON borne la
// PROFONDEUR, pas la longueur : c'est ici que la longueur se borne.
//
// Deux cent cinquante-six kilo-octets laissent mille fois la place qu'une
// réponse demande.
inline constexpr std::size_t kWebBodyLimit = 256U * 1024U;

// LES DÉLAIS DE GARDE, en millisecondes.
inline constexpr int kWebResolveTimeoutMs = 10'000;
inline constexpr int kWebConnectTimeoutMs = 10'000;
inline constexpr int kWebSendTimeoutMs = 15'000;
inline constexpr int kWebReceiveTimeoutMs = 15'000;

// CE QU'UNE ORIGINE PEUT ÊTRE.
//
// 🔴 LE CLAIR N'EST PERMIS QU'AU SERVICE WEB DU SERVEUR DE JEU, et c'est une
// décision nommée, pas un drapeau qui traîne. Ce service n'écoute qu'en `http`
// — c'est lui qui le décide, et le client d'origine s'y adresse ainsi — et ce
// qui le traverse est une image d'emblème et le jeton d'une session de jeu,
// jamais le mot de passe d'un compte. Le login web, lui, reste en HTTPS seul.
enum class WebScheme : std::uint8_t { kSecureOnly, kPlainAllowed };

// LE TRANSPORT RÉEL, sur un fil d'exécution à part.
//
// 🔴 LE RÉSEAU DU SYSTÈME PLUTÔT QU'UNE BIBLIOTHÈQUE LIVRÉE, et c'est une
// décision du joueur : WinHTTP sous Windows, la libcurl de la distribution
// sous Linux. La TLS est alors celle du système, son magasin de certificats
// celui que l'administrateur entretient, et il n'y a rien à livrer ni à tenir
// à jour de notre côté. Seule `FetchOnce` change d'une plateforme à l'autre.
//
// ⚠ ET C'EST LA SEULE PIÈCE DU LOGIN WEB QUI CONNAISSE UNE PLATEFORME. Tout ce
// qui a des états, des refus et un jeton qui expire vit dans `moonclient_web`,
// qui se bâtit partout. La coupure est celle de `ro_ui` / `ro_ui_gl`.
//
// 🔴 LA VALIDATION DU CERTIFICAT NE SE DÉSACTIVE PAS, pas même pour un serveur
// de développement à certificat auto-signé : un interrupteur « à ne jamais
// activer en production » est un trou qu'il faut se rappeler de ne pas
// ouvrir. Ce qui traverse ce fil est un mot de passe de forum et un
// jeton qui vaut un compte de jeu : le jour où un serveur d'essai en aura
// besoin, ce sera une décision prise exprès, pas un drapeau qui traîne.
//
// ⚠ HTTPS SEULEMENT, sauf `WebScheme::kPlainAllowed`. Une origine en clair est
// refusée AVANT le premier octet : un repli silencieux sur `http://` mettrait
// le mot de passe sur le fil en clair, ce qui est exactement ce qu'on évite.
class HttpTransport final : public WebTransport {
 public:
  explicit HttpTransport(std::string origin, WebScheme scheme = WebScheme::kSecureOnly)
      : origin_(std::move(origin)), scheme_(scheme) {}
  ~HttpTransport() override = default;

  void Begin(const WebRequest& request) override;
  [[nodiscard]] std::optional<WebReply> Take() override;
  [[nodiscard]] bool busy() const override { return pending_ != nullptr; }
  void Abandon() override;

  // CHANGE DE SITE : la requête en cours s'abandonne, la suivante part vers
  // `origin`. Même règle de schéma qu'à la construction.
  void SetOrigin(std::string origin) {
    Abandon();
    origin_ = std::move(origin);
  }

 private:
  // LE CASIER QUE LE FIL REMPLIT.
  //
  // 🔴 PARTAGÉ PAR POINTEUR COMPTÉ, ET LE FIL EST DÉTACHÉ. Un `std::future`
  // aurait été plus court, et son destructeur BLOQUE jusqu'à la fin de la
  // tâche : fermer le client pendant une requête l'aurait figé jusqu'au délai
  // de garde, quinze secondes. Ici, abandonner lâche le casier ; le fil finit
  // d'écrire dans sa propre copie, qui meurt avec lui.
  struct Slot {
    std::mutex lock;
    WebReply reply;
    std::atomic<bool> done{false};
  };

  std::string origin_;
  WebScheme scheme_ = WebScheme::kSecureOnly;
  std::shared_ptr<Slot> pending_;
};

namespace detail {

// LE CORPS D'UN FORMULAIRE ENCODÉ (`application/x-www-form-urlencoded`).
[[nodiscard]] std::string FormBody(const WebRequest& request);

// L'origine est-elle permise à ce transport ? Le refus se fait AVANT le premier
// octet. Rend vrai pour une origine HTTPS.
[[nodiscard]] bool SchemeAllowed(std::string_view origin, WebScheme scheme, bool& secure);

// LA REQUÊTE, DU DÉBUT À LA FIN, sur le fil détaché : propre à chaque
// plateforme (`win_http_transport.cpp`, `curl_http_transport.cpp`).
[[nodiscard]] WebReply FetchOnce(const std::string& origin, WebScheme scheme,
                                 const WebRequest& request);

}  // namespace detail

}  // namespace moonclient::web
