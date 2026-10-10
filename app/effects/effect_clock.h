#pragma once

#include <cstdint>
#include <optional>

#include <glm/vec3.hpp>

#include "ro/render/effect_sprite_draw_order.h"

namespace moonclient {

// UN EFFET EN COURS, ET SON HORLOGE.
//
// ⚠ LE CLIENT RAPPELLE SON BÂTISSEUR À CHAQUE TIC, et chaque semis est gardé
// par une condition sur le compteur : Heal pose sa colonne au tic zéro et
// lâche une étincelle tous les neuf tics jusqu'au centième. Tout semer d'un
// coup fait naître ensemble ce qui devait couler — cent quatorze identifiants
// relevés sont dans ce cas.
struct EffectClock {
  std::uint32_t effect_id = 0;
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  std::int32_t age_ticks = 0;
  // OÙ EST LA CIBLE, gardée à la naissance comme la face du porteur — le
  // client, lui, relit l'acteur visé à chaque semis. Vide sans cible.
  std::optional<glm::vec3> target_position;
  // Le dernier tic où l'un de ses patrons peut encore semer. L'effet se
  // retire au-delà : sans cette borne, une période sèmerait sans fin.
  std::int32_t last_tick = 0;
  // L'angle de base du porteur, GARDÉ À LA NAISSANCE.
  //
  // ⚠ LE CLIENT, LUI, LE RELIT VIF : son bâtisseur relit l'angle de l'acteur à
  // chaque semis, si bien qu'un porteur qui tourne pendant que son effet coule oriente les nœuds
  // semés ensuite. L'horloge ne tient pas ce pointeur ; c'est un écart NOMMÉ, et il ne se voit que
  // sur les effets qui sèment longtemps.
  float owner_facing_degrees = 0.0F;
  // L'ACTEUR QUI PORTE L'EFFET, ou zéro. Voir `FollowActor`.
  std::uint32_t attached_actor_id = 0;
  // LE PARAMÈTRE DE L'INSTANCE : le nombre de coups d'un coup
  // multi-coups, un pour tout le reste. Seuls les semis qui le suivent le
  // lisent — voir `EffectNodePattern::sowings_follow_param`.
  std::int32_t param = 1;
  // LA VIE MINIMALE DES NŒUDS À VENIR, portée par l'horloge et non par le
  // seul premier semis : un effet qui coule sème jusqu'à son dernier tic, et
  // les derniers nés doivent tenir aussi longtemps que les premiers. Voir
  // `EffectNodePool::LiveAtLeast`.
  std::int32_t life_floor_ticks = 0;
  // L'ÉCHELLE VENUE DE L'OBJET DU RSW, gardée pour les
  // semis à venir : un effet qui coule doit poser ses derniers nœuds à la
  // taille de son semeur comme les premiers. Voir
  // `EffectNodePattern::sprite_scale_from_instance`.
  float instance_scale = 0.0F;
  // OÙ SE TENAIT LE PORTEUR à la dernière image. Ce n'est pas la place de
  // l'effet : celle-ci ne le rejoint qu'aux tics de son horaire — voir
  // `EffectBearerFollow`. Elle vaut la place de naissance tant que personne ne
  // la déplace.
  glm::vec3 bearer_ground_position{0.0F, 0.0F, 0.0F};
  // LE SECOND PARAMÈTRE DE L'OBJET DU RSW, gardé comme l'échelle : l'eau de
  // 838 y lit son cap.
  float instance_second = 0.0F;
  // LE NUMÉRO DE CETTE INSTANCE pour ses projectiles, zéro tant qu'elle n'en a
  // semé aucun ; et les angles du DERNIER qu'elle a semé. Voir
  // `EffectNodeSupplement::records_projectile`.
  std::uint32_t projectile_serial = 0;
  std::optional<ro::render::EffectProjectileAngles> last_projectile;
  // LE TIC OÙ L'INSTANCE PERD SON PORTEUR, zéro quand rien ne l'y oblige : elle
  // ne sème plus au-delà, et ce qu'elle sème d'ici là meurt avec elle. Voir
  // `EffectNodePool::DieWithBearerAt`.
  std::int32_t bearer_death_tick = 0;
  // L'INSTANCE EST NÉE D'UN CHAPEAU, et tout ce qu'elle sème en hérite. Voir
  // `EffectNodePool::SowAsHat`.
  bool from_hat = false;
};

}  // namespace moonclient
