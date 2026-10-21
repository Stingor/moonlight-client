#pragma once

#include <array>

#include <glm/glm.hpp>

#include "ro/render/effect_node.h"

// UN NŒUD PROCÉDURAL PEINT ENTRE DEUX TICS : sa place, son cap, son angle, son
// échelle, ses étendues et sa couleur glissent de leur état au tic PRÉCÉDENT à
// celui du tic COURANT, au poids que rend `TickBlendWeight`.
//
// ⚠ LE MOUVEMENT NE CHANGE PAS : la troupe garde son pas de seize millisecondes
// et la même suite de mises à jour. Ce qui est discret — l'image d'un sprite,
// son action, les marques des pistes — reste celui du tic.
//
// 🔴 TOUTES LES FORMES NE S'Y PRÊTENT PAS, et c'est `EffectNodeBlendsBetweenTicks`
// qui le dit. Les seules qui glissent se dessinent du nœud seul — le sprite et
// son halo, le tronc de cône, les plans croisés, le quad orienté. Une figure
// bâtie de ses quatre pistes, ou d'une traînée de crans pris tic après tic,
// reste peinte au tic : sa tête glisserait sans sa traînée, et les deux se
// décrocheraient.
namespace ro::render {

struct PooledEffectNode;

// CE QUE LE DESSIN D'UN NŒUD LIT ET QUE SON MOUVEMENT FAIT GLISSER, à un tic.
struct EffectNodeKinematics {
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  // Le cap de la figure, en degrés du client : voir
  // `PooledEffectNode::node_yaw_degrees`.
  float yaw_degrees = 0.0F;
  float spin_degrees = 0.0F;
  float sprite_scale = 1.0F;
  float alpha = 0.0F;
  std::array<float, 3> color = {kEffectNodeChannelMax, kEffectNodeChannelMax,
                                kEffectNodeChannelMax};
  // Les demi-étendues du nœud, chacune dans son unité : voir `EffectNodeExtent`.
  float half_width = 0.0F;
  float half_height = 0.0F;
  float radius_at_anchor = 0.0F;
  float radius_at_height = 0.0F;
  float ring_radius = 0.0F;
};

// VRAI quand la forme de ce nœud se peint entre deux tics : voir l'en-tête.
// ⚠ UN SPRITE À TRAÎNÉE N'EN EST PAS, ni l'éventail, ni le sprite qui s'éteint
// sur ses pistes : leurs crans sont des places prises tic après tic.
[[nodiscard]] bool EffectNodeBlendsBetweenTicks(const PooledEffectNode& pooled) noexcept;

// L'état cinématique du nœud tel qu'il est.
[[nodiscard]] EffectNodeKinematics EffectNodeKinematicsOf(const PooledEffectNode& pooled) noexcept;

// RETIENT L'ÉTAT DU NŒUD AVANT SA MISE À JOUR : c'est l'état du tic précédent.
//
// ⚠ UN NŒUD QUI BAT À CHAQUE IMAGE (`beats_every_frame`) n'en retient aucun :
// il avance déjà au rythme du dessin, et le mélanger le ferait traîner. Une
// forme qui ne glisse pas n'en retient aucun non plus.
void RememberEffectNodeTick(PooledEffectNode& pooled, bool beats_every_frame);

// OÙ LE NŒUD SE PEINT, au poids `weight` du tic courant. Sa place courante, au
// bit près, sans état retenu ou au poids un — un nœud né depuis le dernier tic
// se peint donc où il est.
[[nodiscard]] glm::vec3 EffectNodeDrawnPosition(const PooledEffectNode& pooled,
                                                float weight) noexcept;

// LE CAP DE LA FIGURE, au même poids, par le plus court chemin.
[[nodiscard]] float EffectNodeDrawnYawDegrees(const PooledEffectNode& pooled,
                                              float weight) noexcept;

// LE NŒUD TEL QU'IL SE PEINT, au même poids : une copie dont seuls les champs
// de `EffectNodeKinematics` ont glissé.
[[nodiscard]] EffectNode EffectNodeDrawnBetweenTicks(const PooledEffectNode& pooled, float weight);

}  // namespace ro::render
