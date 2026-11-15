#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "survivors/survivors_sightings.h"

namespace moonclient {

// CE QU'UNE OFFRE DU CHOIX DE NIVEAU FAIT ÉVOLUER. Un sort offert nomme
// l'évolution qu'il ouvre avec sa carte, et l'union qu'il forme avec un second
// sort ; une carte offerte nomme le sort qu'elle fait évoluer. Chaque recette
// dit son autre ingrédient et l'évolué qu'elle donne.
//
// 🔴 LA RÈGLE DU CODEX : ce que le carnet n'a pas vu ne se nomme pas. La
// fenêtre en montre la silhouette, sans dire ce que c'est.
struct SurvivorsOfferRecipe {
  // L'autre ingrédient : la carte d'une évolution, le second sort d'une union,
  // ou le sort de base que la carte offerte fait évoluer.
  std::uint16_t partner_id = 0;
  bool partner_is_skill = false;
  std::uint16_t evolved_skill_id = 0;
  bool union_of_two = false;
  // Le héros qui reçoit l'offre a déjà l'autre ingrédient : l'offre la complète.
  bool completes = false;
  bool partner_seen = false;
  bool evolved_seen = false;
  [[nodiscard]] bool operator==(const SurvivorsOfferRecipe&) const = default;
};

// Les recettes de `offer` pour le héros `hero`, dans l'ordre des règles. Une
// recette dont le héros tient déjà l'évolué est faite : elle ne paraît plus.
// Rien pour un soin ni des zenys. `found_cards` : les cartes de l'Album, par
// objet de carte, vues comme au Codex.
[[nodiscard]] std::vector<SurvivorsOfferRecipe> SurvivorsOfferRecipesOf(
    const ro::sim::SurvivorsRules& rules, const ro::sim::SurvivorsHero& hero,
    const ro::sim::SurvivorsOffer& offer, const SurvivorsCodexSeen& seen,
    std::span<const std::uint16_t> found_cards);

}  // namespace moonclient
