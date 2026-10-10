#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace moonclient::web {

// LE TRANSPORT, VU PAR LA MACHINE À ÉTATS : on lance, on repasse, on prend.
//
// 🔴 IL NE BLOQUE PAS, ET C'EST TOUT L'OBJET DE L'INTERFACE. Un appel HTTPS
// vers un site distant coûte de cent à cinq cents millisecondes quand il
// répond, et jusqu'à son délai de garde quand il ne répond pas. L'écran de
// connexion tourne dans la même boucle que le décor vivant : un appel bloquant
// y figerait la ville, le curseur et la saisie du joueur, et un serveur muet
// les figerait quinze secondes.
//
// ⚠ UNE SEULE REQUÊTE À LA FOIS. Le parcours est une SÉQUENCE — s'identifier,
// puis choisir un compte — et rien n'y demande deux réponses en même temps. Une
// file rendrait le transport plus général et la machine à états plus difficile
// à lire, pour un besoin qui n'existe pas.
//
// ⚠ ET IL EST ABSTRAIT POUR ÊTRE ESSAYABLE, pas par goût de l'abstraction : ce
// qui a des états, des refus et un jeton qui expire est la machine, pas le
// transport. Derrière cette interface, l'essai répond ce qu'il veut, quand il
// veut — y compris rien.

// UNE PARTIE D'UN FORMULAIRE EN PLUSIEURS MORCEAUX.
struct WebPart {
  std::string name;
  // Des OCTETS, pas du texte : une image y passe telle quelle.
  std::string content;
  // Non vides, la partie se présente comme un FICHIER — son nom et son type.
  std::string file_name;
  std::string content_type;
};

// UNE REQUÊTE, toujours en POST, sous l'une de deux formes.
//
// ⚠ LE SITE DU FORUM LIT DES CHAMPS ENCODÉS (`fields`) ; le service web du
// SERVEUR DE JEU ne lit que des parties de formulaire (`parts`), et refuse une
// requête qui lui présente ses champs autrement. Dès que `parts` n'est pas
// vide, c'est lui qui part, et `fields` est ignoré.
struct WebRequest {
  // Le chemin, avec sa barre de tête. L'origine vient du transport.
  std::string path;
  std::vector<std::pair<std::string, std::string>> fields;
  std::vector<WebPart> parts;
};

// LE CORPS D'UN FORMULAIRE EN PLUSIEURS MORCEAUX, et la frontière qui les
// sépare.
//
// ⚠ LA FRONTIÈRE NE DOIT APPARAÎTRE DANS AUCUNE PARTIE : une image peut porter
// n'importe quels octets. Elle s'allonge donc jusqu'à ne plus s'y trouver.
struct MultipartBody {
  std::string boundary;
  std::string body;
};
[[nodiscard]] MultipartBody BuildMultipartBody(const std::vector<WebPart>& parts);

// CE QUE LE TRANSPORT A RAPPORTÉ.
struct WebReply {
  // Le code HTTP, ou zéro quand rien n'est arrivé du tout.
  //
  // ⚠ IL COMPTE : l'API répond 429 quand le limiteur d'échecs a mordu, et 410
  // quand une session Discord a expiré. Le corps porte le motif dans les deux
  // cas, mais le code dit qu'il ne s'agit pas d'un refus ordinaire.
  int status = 0;
  std::string body;
  // Ce qui a empêché la requête d'aboutir, en clair, ou vide. ⚠ UN TRANSPORT
  // QUI ÉCHOUE N'EST PAS UN SERVEUR QUI REFUSE : le premier se dit « le site
  // est injoignable », le second porte un message écrit par le site.
  std::string error;

  [[nodiscard]] bool arrived() const noexcept { return error.empty() && status != 0; }
};

class WebTransport {
 public:
  WebTransport() = default;
  WebTransport(const WebTransport&) = delete;
  WebTransport& operator=(const WebTransport&) = delete;
  WebTransport(WebTransport&&) = delete;
  WebTransport& operator=(WebTransport&&) = delete;
  virtual ~WebTransport() = default;

  // Lance la requête. Sans effet si une autre est en vol.
  virtual void Begin(const WebRequest& request) = 0;

  // LA RÉPONSE, UNE FOIS, quand elle est là.
  //
  // ⚠ ELLE SE PREND ET S'EFFACE : un appelant qui la relirait à l'image
  // suivante traiterait deux fois la même réponse, et avancerait deux fois
  // dans un parcours qui n'a qu'un pas par réponse.
  [[nodiscard]] virtual std::optional<WebReply> Take() = 0;

  // Vrai tant qu'une requête est en vol.
  [[nodiscard]] virtual bool busy() const = 0;

  // Abandonne ce qui est en vol. ⚠ LA RÉPONSE PEUT ARRIVER QUAND MÊME, et
  // c'est au transport de la jeter : le parcours, lui, a tourné la page.
  virtual void Abandon() = 0;
};

}  // namespace moonclient::web
