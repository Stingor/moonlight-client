#pragma once

// L'ADOPTION DANS LA PARTIE : la demande qu'un couple nous adresse, posée dans
// une boîte qui attend la réponse, et les refus que le serveur dit à qui a
// demandé. La demande elle-même part du menu d'entité.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/sim/adoption.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// LA BOÎTE DE LA DEMANDE REÇUE, telle que l'étage d'interface la dessine.
struct AdoptionBox {
  // La question, DÉJÀ TRADUITE ; vide quand aucune demande n'attend.
  std::string question;
  // Une demande nouvelle vient d'arriver : la boîte s'ouvre.
  bool ask = false;
  // Ce que le joueur a répondu.
  std::optional<bool> answer;
};

// La question posée à l'adopté, DÉJÀ TRADUITE : ce qu'il accepte ne se défait
// pas.
[[nodiscard]] std::string AdoptionQuestion(std::string_view parent_name);

// Ce qu'un refus dit au parent, DÉJÀ TRADUIT.
[[nodiscard]] const char* AdoptionRefusalText(ro::sim::AdoptionRefusal refusal);

class AdoptionStage {
 public:
  // Ce que les trames du serveur remplissent.
  [[nodiscard]] ro::sim::AdoptionDesk& desk() noexcept { return desk_; }

  // Une partie commence : aucune demande ne survit à la précédente.
  void StartSession();

  // L'IMAGE : la réponse donnée part, les refus se disent, puis la boîte se
  // compose pour la suivante.
  void Advance(AdoptionBox& box, ro::net::Connection& link, ro::sim::World& world,
               std::uint32_t now_ms);

 private:
  ro::sim::AdoptionDesk desk_;
  // La dernière demande dont la boîte s'est ouverte.
  std::uint32_t asked_seq_ = 0;
};

}  // namespace moonclient
