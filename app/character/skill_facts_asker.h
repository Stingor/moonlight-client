#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/ui/description_window.h"

namespace moonclient {

// CE QUE LES ONGLETS TECHNIQUES D'UNE COMPÉTENCE DEMANDENT AU SERVEUR : le
// temps d'incantation, et l'estimation des dégâts contre une cible.
//
// 🔴 LA FENÊTRE REDEMANDE À CHAQUE IMAGE où son onglet est ouvert, et c'est
// voulu : elle ne sait pas ce que le monde porte déjà — `ui` ne possède rien.
// C'est donc ICI qu'on décide de ne pas harceler le serveur, et nulle part
// ailleurs. Sans ce filtre, ce serait soixante paquets par seconde.
//
// ⚠ LA COMPÉTENCE ET LA CIBLE FONT PARTIE DE LA CLÉ : changer de compétence ou
// de cible, c'est une AUTRE question, et la réponse d'avant n'y répond pas.
//
// ⚠ ON NE REDEMANDE PAS PARCE QUE LE TEMPS PASSE : ces chiffres ne changent
// qu'avec l'équipement et les états, et rien ne nous en avertit. C'est le bouton
// du joueur qui rouvre la question — d'où `refresh`.
class SkillFactsAsker {
 public:
  // Envoie ce que la fenêtre veut et qu'on n'a pas déjà demandé. Un envoi qui
  // échoue se journalise : la fenêtre garde les chiffres précédents.
  void Ask(const ro::ui::SkillDescriptionAction& asked, ro::net::Connection& link);

 private:
  std::uint16_t asked_cast_skill_id_ = 0;
  std::uint16_t asked_damage_skill_id_ = 0;
  std::uint32_t asked_damage_target_id_ = 0;
};

}  // namespace moonclient
