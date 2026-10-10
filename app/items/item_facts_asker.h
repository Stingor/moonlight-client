#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <utility>

#include "ro/net/connection.h"
#include "ro/sim/item_facts.h"

namespace moonclient {

// CE QUE LE SERVEUR SEUL SAIT D'UN OBJET — ses sources, son script — et le
// rythme auquel on le lui demande.
//
// ⚠ LES SOURCES VIEILLISSENT : un taux bouge avec les événements, le VIP et
// l'équipement du joueur. Une réponse vaut quinze secondes, et se redemande à
// chaque RÉOUVERTURE de la description — on revient souvent sur un objet après
// avoir changé d'équipement. Le script, lui, ne bouge pas de la session. Une
// demande restée sans réponse se rejoue au bout de trois secondes : avant la
// poignée de main, le serveur l'ignore sans rien dire.
class ItemFactsAsker {
 public:
  enum class Kind : std::uint8_t { kOrdinary, kBoss, kScripts };

  // Les réponses arrivent ici.
  [[nodiscard]] ro::sim::ItemFacts& facts() noexcept { return facts_; }
  [[nodiscard]] const ro::sim::ItemFacts& facts() const noexcept { return facts_; }

  // À CHAQUE IMAGE, fenêtre fermée comprise : c'est ainsi que la réouverture se
  // voit. Les trois demandes partent dès que la fenêtre est ouverte sur un
  // objet, et non à l'ouverture d'un onglet : c'est ce qui laisse griser un
  // onglet vide avant qu'on ait cliqué dessus.
  void Ask(ro::net::Connection& link, bool window_open, std::uint32_t item_id,
           std::uint32_t now_ms);

  // « Rafraichir » : oublier la réponse, et redemander à l'image suivante.
  void Forget(std::uint32_t item_id, Kind kind);

  // Une demande est-elle partie pour ce couple ?
  [[nodiscard]] bool Asked(std::uint32_t item_id, Kind kind) const;

  // LE NOM D'UN FORGERON, voulu par une description. Il part au prochain `Ask`,
  // FENÊTRE FERMÉE COMPRISE : l'aperçu au survol le veut aussi. ⚠ UNE FOIS PAR
  // PERSONNAGE ET PAR SESSION — un nom ne change pas —, rejouée seulement si le
  // serveur n'a pas répondu dans les trois secondes.
  void WantCreator(std::uint32_t character_id);

 private:
  struct Sent {
    std::uint32_t at_ms = 0;
    // Une réponse est arrivée depuis : `at_ms` date alors sa fraîcheur.
    bool answered = false;
  };
  void AskOne(ro::net::Connection& link, std::uint32_t item_id, Kind kind, std::uint32_t now_ms);

  void AskCreators(ro::net::Connection& link, std::uint32_t now_ms);

  ro::sim::ItemFacts facts_;
  std::map<std::pair<std::uint32_t, Kind>, Sent> sent_;
  std::set<std::uint32_t> wanted_creators_;
  std::map<std::uint32_t, std::uint32_t> creators_asked_at_ms_;
  bool window_was_open_ = false;
};

// Les délais, publics pour les essais.
inline constexpr std::uint32_t kItemSourcesFreshMs = 15000;
inline constexpr std::uint32_t kItemFactsAnswerWaitMs = 3000;

}  // namespace moonclient
