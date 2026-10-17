#pragma once

#include "ro/net/connection.h"

namespace moonclient {

// L'HABIT DU GM DE NOTRE ADMINISTRATEUR, dit au serveur (`CZ 0x0F43`), qui le
// retient avec le personnage et le montre aux autres joueurs.
//
// 🔴 REDIT À CHAQUE POIGNÉE DE MAIN, comme nos surfaces : le choix a pu changer
// pendant que la liaison était coupée, et le serveur ne l'apprend que de nous.
// Un choix inchangé ne coûte rien de son côté : il ne le rediffuse pas.
class GmOutfitAnnouncer {
 public:
  // À CHAQUE IMAGE. Non vérifié : se réarme, rien n'est envoyé. Sans le droit
  // d'administrateur, rien non plus : le serveur refuserait. Sinon, dit `worn`
  // si rien n'a été dit depuis le dernier réarmement, ou s'il a changé.
  void Pump(ro::net::Connection& link, bool verified, bool admin, bool worn);
  // UNE NOUVELLE POIGNÉE DE MAIN : la prochaine image vérifiée le redit.
  void Rearm() noexcept { told_ = false; }
  [[nodiscard]] bool told() const noexcept { return told_; }

 private:
  bool told_ = false;
  bool last_worn_ = false;
};

}  // namespace moonclient
