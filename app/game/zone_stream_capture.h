#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>

#include "ro/net/connection.h"

namespace moonclient {

// ENREGISTRE LES OCTETS REÇUS DU SERVEUR DE ZONE, tels qu'ils arrivent.
//
// Quand le découpage bute, ce qu'il faut relire est précisément ce qu'il a
// refusé — les trames, elles, s'arrêtent avant. Le fichier porte donc le FLUX,
// pas des paquets : `roc-cli net frames` le redécoupe ensuite, hors du jeu et
// autant de fois qu'il le faut. C'est aussi la matière du rejeu sans fenêtre.
//
// 🔴 UN FICHIER PAR LIAISON DE ZONE. Chaque liaison — l'entrée en jeu, chaque
// changement de serveur de zone, chaque retour par la sélection du personnage —
// repart d'un flux neuf, ouvert par son identifiant de compte nu : deux
// liaisons mises bout à bout dans un même fichier rompraient le découpage à la
// jonction. La première garde le nom demandé, les suivantes prennent un numéro
// avant l'extension : `flux.bin`, `flux.2.bin`, `flux.3.bin`…
//
// ⚠ LE FICHIER NE S'OUVRE QU'AU PREMIER OCTET REÇU : une liaison qui n'aboutit
// jamais — un écran de connexion refermé — ne laisse ni fichier vide ni trou
// dans la numérotation.
class ZoneStreamCapture {
 public:
  explicit ZoneStreamCapture(std::filesystem::path first_file);

  // L'observateur d'UNE liaison neuve, à poser avant sa première lecture.
  [[nodiscard]] ro::net::ReceiveObserver ObserveNewLink();

 private:
  struct Shared {
    std::filesystem::path first_file;
    std::size_t links_opened = 0;
  };
  std::shared_ptr<Shared> shared_;
};

}  // namespace moonclient
