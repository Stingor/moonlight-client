#pragma once

#include <cstdint>

namespace ro::render {

struct PooledEffectNode;
struct EffectScatterWorld;

// CHAQUE FORME A SON MOUVEMENT, et c'est la forme qui le choisit.
//
// ⚠ SORTI DE `effect_node_pool.cpp`, QUI ÉTAIT À HUIT CENTS LIGNES PILE et ne
// pouvait plus recevoir une famille de plus. La coupure suit une frontière
// réelle : d'un côté la TROUPE — ajouter un nœud, l'avancer, le retirer —, de
// l'autre ce CATALOGUE d'un cas par forme, où l'on cherche un mouvement sans
// jamais lire de bout en bout.
//
// L'en-tête est INTERNE à `src/` : personne hors de la troupe n'a à choisir un
// mouvement, et l'exposer inviterait à le faire.
void AdvanceTracksOf(PooledEffectNode& pooled, const EffectScatterWorld& world,
                     std::uint32_t& generator);

// LES FORMES DONT LE MOUVEMENT NE TIRE RIEN ET NE LIT PAS LE MONDE : les
// panneaux qui rangent leurs angles de caméra, et les deux formes qui ne
// peignent rien. `AdvanceTracksOf` leur délègue ; hors d'elles, rien ne se passe.
void AdvancePanelTracksOf(PooledEffectNode& pooled);

// LES ANNEAUX QUI SUIVENT LEUR PORTEUR — le quatuor, la gerbe, l'anneau
// tournant : la même jupe, trois mouvements. `AdvanceTracksOf` leur délègue.
void AdvanceFollowingRingTracksOf(PooledEffectNode& pooled);

}  // namespace ro::render
