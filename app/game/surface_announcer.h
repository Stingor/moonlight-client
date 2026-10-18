#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/ui_caps.h"

namespace moonclient {

// CE QUE CE CLIENT SAIT MONTRER, annoncé au serveur (`CZ 0x0F24`). Il y règle ce
// qu'il écrit : sans `kUiCapNpcDialog`, il retire d'un dialogue ses balises
// maison — les sprites `<MOBS>`, les portraits `<MOBP>` et les images `<IMG>`
// disparaissent ; sans `kUiCapChat`, ses commandes écrivent à plat ce qu'elles
// écriraient en liens.
//
// 🔴 LES DEUX BITS D'ALBUM ENSEMBLE, TOUJOURS : le second ne dit pas un réglage
// mais ce que ce client sait LIRE sur le fil.
//
// ⚠ LE CARNET MVP (0x04) SUIT SON INTERRUPTEUR MAÎTRE, pas sa fenêtre : éteint,
// le serveur cesse de nous diffuser les observations du groupe — nos kills
// continuent de l'alimenter, puisque c'est lui qui les enregistre.
//
// Et le Flag Graffiti, que ce client sait peindre au sol, le maître d'un
// monstre invoqué, les règles du serveur, les queues du carnet, la raison d'une
// fin de quête, le maître d'un compagnon, l'auteur de la parole, le relais
// Discord décrit et ce qu'apporte une pièce posée ou retirée, qu'il sait lire. ⚠ LES QUEUES DU
// CARNET S'ANNONCENT MÊME CARNET ÉTEINT, et le relais Discord même relais décoché : ces bits disent
// seulement ce que ce client sait lire, et n'abonnent à rien.
//
// Et le Marché, qu'un catalogue ouvre à la place de la recherche d'échoppes,
// l'habit du GM d'un administrateur, qu'il sait dessiner, et les éditions de
// carte que le serveur tient.
inline constexpr std::uint32_t kAlwaysShownSurfaces =
    ro::net::kUiCapNpcDialog | ro::net::kUiCapChat | ro::net::kUiCapCardAlbum |
    ro::net::kUiCapCardAlbumDropper | ro::net::kUiCapFlagGraffiti | ro::net::kUiCapUnitMaster |
    ro::net::kUiCapServerRules | ro::net::kUiCapMvpTrackerExt | ro::net::kUiCapQuestEnd |
    ro::net::kUiCapCompanionMaster | ro::net::kUiCapChatAuthor | ro::net::kUiCapDiscordRich |
    ro::net::kUiCapEquipAttributes | ro::net::kUiCapInstanceInfo | ro::net::kUiCapMarket |
    ro::net::kUiCapGmOutfit | ro::net::kUiCapMapEdits;
[[nodiscard]] constexpr std::uint32_t ShownSurfaces(bool mvp_tracker_enabled) noexcept {
  return kAlwaysShownSurfaces | (mvp_tracker_enabled ? ro::net::kUiCapMvpTracker : 0U);
}

// LE MASQUE SE RÉ-ANNONCE QUAND IL CHANGE, et à chaque poignée de main : le
// serveur l'oublie à chaque entrée en zone, et le remplace en entier à chaque
// annonce.
//
// 🔴 L'ANNONCEUR SURVIT À LA PARTIE, la poignée de main non : chaque nouvelle
// poignée de main le réarme (`Rearm`). Une image non vérifiée ne suffit pas —
// au changement de personnage, le serveur nous reconnaît d'office, et la
// partie suivante est vérifiée dès sa première image.
class SurfaceAnnouncer {
 public:
  // À CHAQUE IMAGE. Non vérifié : se réarme, rien n'est envoyé. Vérifié :
  // annonce si rien n'a été annoncé depuis le dernier réarmement, ou si `caps`
  // diffère du dernier masque ANNONCÉ.
  //
  // ⚠ L'ENVOI MET EN FILE ET NE REFUSE RIEN : seul l'encodage peut échouer,
  // ce qui n'arrive pas pour cette trame fixe. Un échec se journalise une fois
  // et compte comme annoncé — pas de nouvelle tentative avant le prochain
  // changement de masque ou la prochaine vérification.
  void Pump(ro::net::Connection& link, bool verified, std::uint32_t caps);
  // UNE NOUVELLE POIGNÉE DE MAIN : la prochaine image vérifiée annonce, quel que
  // soit le masque.
  void Rearm() noexcept { announced_ = false; }
  [[nodiscard]] bool announced() const noexcept { return announced_; }
  [[nodiscard]] std::uint32_t last_caps() const noexcept { return last_caps_; }

 private:
  bool announced_ = false;
  std::uint32_t last_caps_ = 0;
};

}  // namespace moonclient
