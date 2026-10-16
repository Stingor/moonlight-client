#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient {

// LA CONFIGURATION DE CE CLIENT : à qui il parle.
//
// Elle vit dans `moonclient.yml`, À CÔTÉ DE L'EXÉCUTABLE, et c'est le seul
// endroit où une adresse est écrite : le programme n'en connaît aucune, si bien
// qu'on change de serveur sans recompiler. Le patcher la livre avec
// l'exécutable ; le modèle est `dist/moonclient.yml`.
//
// ⚠ CE N'EST NI `clientinfo.xml` NI `ExternalSettings_kr.lub`. Le client
// d'origine lit ces deux-là dans ses archives, et ils appartiennent à SON
// installation : les partager aurait fait dépendre ce client de fichiers qu'un
// autre modifie. Un seul fichier, à nous, et aucun repli : s'il manque, le
// client le dit et s'arrête.
//
//   servers:
//     - name: Moonlight-Destiny
//       address: moonlight-destiny.fr
//       port: 16900
//   web: moonlight-destiny.fr:8888
//   server_time_zone: Europe/Paris
//   archives:
//     - moonlight.grf
//     - data.grf
//   patcher:
//     mirrors:
//       - name: Moon Patch Server
//         list: https://moonlight-destiny.fr/moonpatcher/plist.txt
//         archives: https://moonlight-destiny.fr/moonpatcher/data/
//     default_grf: moonlight.grf
//     require_hashes: false

// Un serveur de connexion, tel que le fichier le déclare.
struct ServerEntry {
  std::string name;
  std::string address;
  std::uint16_t port = 0;
};

// UN SERVEUR DE PATCHS : où lire la liste, où prendre les archives.
//
// ⚠ LES DEUX ADRESSES SONT EN HTTPS, et le fichier qui en déclare une autre est
// refusé. Une archive de patch écrit dans le dossier du jeu, exécutables
// compris : la servir en clair, c'est laisser n'importe qui sur le chemin
// choisir ce que le joueur lancera.
struct PatchMirror {
  std::string name;
  // La liste des patchs : une ligne `<index> <archive> [<sha256>]` par patch.
  std::string list_url;
  // Le dossier des archives, TOUJOURS terminé par `/` : le nom d'une archive
  // s'y ajoute tel quel.
  std::string archive_url;
};

struct PatcherConfig {
  // Dans l'ordre du fichier : le premier qui répond sert.
  std::vector<PatchMirror> mirrors;
  // L'archive qu'enrichit un patch qui ne nomme pas la sienne.
  std::string default_grf;
  // Vrai : un patch sans empreinte dans la liste est refusé. Une empreinte
  // PRÉSENTE est vérifiée dans tous les cas.
  bool require_hashes = false;
  // Vrai : un patch qui vise une archive absente la crée. ⚠ FAUX PAR DÉFAUT :
  // une archive qui manque par accident renaîtrait avec le seul contenu du
  // patch, et l'installation se croirait à jour.
  bool create_grf = false;
};

// Celui du serveur Moonlight.
inline constexpr std::string_view kDefaultServerTimeZone = "Europe/Paris";

struct ClientConfig {
  // Dans l'ordre du fichier : le PREMIER est celui qu'un lancement sans
  // argument rejoint.
  std::vector<ServerEntry> servers;
  // L'adresse du service web du serveur — emblèmes de guilde —, telle
  // qu'écrite : `hôte:port`, avec ou sans schéma. Vide : pas de service web.
  std::string web;
  // LE FUSEAU HORAIRE DU SERVEUR, au nom de la base des fuseaux : le cycle jour
  // et nuit se cale sur l'heure qu'il fait LÀ-BAS. Le serveur n'envoie que des
  // instants, jamais son fuseau. Absent du fichier : `kDefaultServerTimeZone`.
  std::string server_time_zone = std::string(kDefaultServerTimeZone);
  // LES ARCHIVES DU JEU, par priorité DÉCROISSANTE : la première l'emporte.
  // Des noms relatifs à l'installation.
  //
  // 🔴 C'EST LA SEULE LISTE QUE LE CLIENT MONTE : aucun autre fichier de
  // l'installation ne décide de ce qui est monté.
  std::vector<std::string> archives;
  // Absent : le client ne se patche pas — c'est le cas d'un arbre de
  // développement, qui pointe sur un serveur local.
  std::optional<PatcherConfig> patcher;
};

// Le nom du fichier, à la racine de l'installation.
inline constexpr std::string_view kClientConfigFileName = "moonclient.yml";

// Lit le texte du fichier.
//
// ⚠ UN FICHIER SANS SERVEUR EST UNE ERREUR, et non un fichier vide : un client
// qui ne sait à qui parler ne peut rien faire, et le dire tout de suite vaut
// mieux qu'un écran de connexion qui n'aboutit jamais. De même d'un serveur
// sans adresse ou dont le port n'en est pas un — l'erreur nomme lequel.
//
// ⚠ ET UN FICHIER SANS ARCHIVE AUSSI : sans elles, il n'y a rien à dessiner.
// Un nom d'archive qui sortirait de l'installation — absolu, ou par `..` —
// est refusé.
[[nodiscard]] ro::core::Result<ClientConfig> ParseClientConfig(std::string_view text);

}  // namespace moonclient
