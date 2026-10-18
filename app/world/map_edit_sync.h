#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>

#include "ro/mapedit/map_edit.h"
#include "ro/net/connection.h"
#include "ro/net/map_edits.h"
#include "ro/net/world_update.h"
#include "ro/vfs/vfs.h"

#include "world/map_edit_store.h"

namespace moonclient {

// LES ÉDITIONS DE CARTE QUE LE SERVEUR TIENT : voir docs/adr/0124.
//
// Le serveur est la source de vérité. À l'entrée sur une carte, il annonce la
// révision qu'il détient ; une autre que la nôtre se demande. Un document servi
// se valide par le lecteur strict, s'écrit dans le dossier du client, et
// s'applique à chaud — sauf sur une carte où le staff a des retouches non
// enregistrées : il s'écrit alors sans remplacer les siennes. « Enregistrer »
// envoie le document, fondé sur la révision qu'on tient ; un envoi périmé
// attend qu'on écrase ou qu'on recharge.
class MapEditSync {
 public:
  // Ce que l'image demande au jeu.
  struct Report {
    // Le document de la carte affichée a changé : le décor se repose, et
    // l'historique de l'éditeur ne vaut plus.
    bool replaced = false;
    // La nouvelle à montrer dans la barre d'état de l'éditeur ; vide : aucune.
    std::string status;
  };

  // Une nouvelle liaison : rien de ce que l'ancienne a dit ne vaut plus.
  void StartSession();
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame) {
    return inbox_.Apply(frame);
  }
  // Ce que le serveur a dit depuis l'image précédente, `shown_map` affichée.
  [[nodiscard]] Report Serve(std::string_view shown_map, MapEditStore& store, ro::vfs::Vfs& vfs,
                             ro::net::Connection& link);
  // ENVOIE le document de `map_name` que l'éditeur vient d'enregistrer, si le
  // serveur tient les éditions et permet de publier. La nouvelle à montrer.
  [[nodiscard]] std::string Publish(std::string_view map_name, const MapEditStore& store,
                                    ro::net::Connection& link);
  // ÉCRASER : renvoyer le document, fondé sur la révision qui l'a devancé.
  [[nodiscard]] std::string Overwrite(std::string_view map_name, const MapEditStore& store,
                                      ro::net::Connection& link);
  // RECHARGER : adopter la révision du serveur ; les retouches non enregistrées
  // se perdent.
  [[nodiscard]] Report Reload(std::string_view map_name, std::string_view shown_map,
                              MapEditStore& store, ro::net::Connection& link);
  // Un envoi de `map_name` a été dit périmé, et attend qu'on écrase ou recharge.
  [[nodiscard]] bool Conflicted(std::string_view map_name) const {
    return stale_.contains(map_name);
  }

 private:
  // Un document servi que des retouches non enregistrées masquent.
  struct Masked {
    ro::mapedit::MapEdit edit;
    std::uint32_t revision = 0;
    std::string author;
  };

  void Judge(const ro::net::MapEditVerdict& verdict, MapEditStore& store, ro::net::Connection& link,
             Report& report);
  void Receive(ro::net::ServedMapEdit served, std::string_view shown_map, MapEditStore& store,
               ro::vfs::Vfs& vfs, Report& report);
  // Adopte `edit` sous `revision`, et le dit.
  void Adopt(const std::string& map_name, ro::mapedit::MapEdit edit, std::uint32_t revision,
             const std::string& author, std::string_view shown_map, MapEditStore& store,
             Report& report);

  ro::net::MapEditInbox inbox_;
  // Ce que le serveur a annoncé, par carte.
  std::map<std::string, ro::net::MapEditAnnounce, std::less<>> announced_;
  // La révision qui a devancé un envoi, par carte.
  std::map<std::string, std::uint32_t, std::less<>> stale_;
  std::map<std::string, Masked, std::less<>> masked_;
  // Les cartes dont le prochain document servi s'adopte malgré les retouches
  // non enregistrées : le staff a demandé à recharger.
  std::set<std::string, std::less<>> reloading_;
};

}  // namespace moonclient
