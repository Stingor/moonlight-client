#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "web/secret_store.h"

namespace moonclient::web {

// CE QUE LA PORTE MOONLIGHT RETIENT D'UN LANCEMENT À L'AUTRE.
//
// 🔴 DEUX MÉMOIRES ET NON UNE, PARCE QU'ELLES NE COÛTENT PAS LE MÊME PRIX.
// L'identifiant du site n'est qu'un nom — il s'écrit en clair, comme le compte
// de jeu s'écrit déjà dans les réglages. Le mot de passe, lui, ouvre le
// compte : il ne s'écrit que scellé par le système, et seulement si le joueur
// l'a demandé. Les fondre en une case ferait retenir le second à qui ne
// voulait que le premier.
//
// ⚠ À PART DES RÉGLAGES DU JEU, et délibérément : `interface.ini` et le
// fichier de réglages sont des fichiers qu'on montre, qu'on copie d'une
// machine à l'autre et qu'on colle dans une conversation de dépannage. Ce
// fichier-ci n'a rien à y faire.
//
// ⚠ ET LE MOT DE PASSE DU JEU N'EST PAS ICI, ni nulle part : celui-là ne se
// retient dans aucun fichier. Seul le mot de passe du SITE a une case, parce
// que lui ne traverse jamais le protocole du jeu.

// Le fichier, dans le dossier du joueur.
inline constexpr std::string_view kWebCredentialsFileName = "moonlight_login.yml";

// LA PLUS GRANDE TAILLE QU'ON ACCEPTE DE ROUVRIR, en octets du scellé.
//
// ⚠ UNE BORNE SUR UN FICHIER QU'ON NE CONTRÔLE PLUS : il traîne chez le
// joueur, et rien n'empêche qu'il ait été tronqué, rempli, ou remplacé. Un
// secret scellé par le système pèse quelques centaines d'octets ; au-delà, ce
// n'est plus le nôtre.
inline constexpr std::size_t kSealedSecretMaxBytes = 8192;

struct WebCredentials {
  std::string member;
  // ⚠ EN MÉMOIRE SEULEMENT, ET EN CLAIR : c'est ce que le champ affiche et ce
  // qui part au site. Sur le disque il n'existe que scellé.
  std::string password;
  bool remember_member = false;
  bool remember_password = false;
};

[[nodiscard]] std::filesystem::path WebCredentialsPath(const std::filesystem::path& client_root);

// Lit ce qui est retenu. Un fichier absent, illisible ou venu d'ailleurs rend
// une mémoire VIDE : ce n'est pas une panne, c'est un premier lancement.
//
// ⚠ LES DEUX DRAPEAUX SE DÉDUISENT DE CE QU'ON A TROUVÉ, ils ne s'écrivent
// pas. Une case cochée dont le secret ne s'ouvre plus — l'installation a
// changé de compte Windows — ferait promettre à l'écran une mémoire qui n'est
// plus là.
[[nodiscard]] WebCredentials LoadWebCredentials(const std::filesystem::path& client_root,
                                                SecretStore& vault);

// Écrit, ou efface, selon les deux drapeaux. Rend faux si le disque refuse.
//
// ⚠ UNE SEULE ÉCRITURE POUR LES DEUX, et c'est ce qui les garde cohérents :
// deux fichiers laisseraient un mot de passe scellé sans son identifiant le
// jour où la seconde écriture échoue.
bool SaveWebCredentials(const std::filesystem::path& client_root, SecretStore& vault,
                        const WebCredentials& wanted);

}  // namespace moonclient::web
