#pragma once

#include <array>
#include <optional>

#include "ro/render/number_look.h"

#include "world/hit_popup_look.h"

namespace moonclient {

// ── LA VIE ET LA TRAJECTOIRE DE CHAQUE TYPE DE DÉGÂT ────────────────────────
//
// Chaque nombre porte son type — voir `DamagePath` —, et c'est lui qui choisit
// sa vie et sa trajectoire. Des fonctions pures, que le jeu et le cadre d'essai
// appellent de la même façon : ce que l'aperçu montre est ce que le jeu fera.

// La trajectoire de ce type. Un rang hors de la liste est le coup ordinaire.
[[nodiscard]] const NumberPath& PathOf(const HitPopupLook& look, DamagePath path) noexcept;
[[nodiscard]] NumberPath& PathOf(HitPopupLook& look, DamagePath path) noexcept;

// SA VIE, ou rien pour les mots : ils gardent celle du client, que leur
// animation tient.
[[nodiscard]] std::optional<ro::render::NumberLife> PathLifeOf(const HitPopupLook& look,
                                                               DamagePath path) noexcept;
[[nodiscard]] ro::render::NumberLife& PathLifeOf(HitPopupLook& look, DamagePath path) noexcept;

// Le réglage commun, plié sur ce type : son chemin, son arrivée décalée de
// `scatter` — l'écart au hasard de CE nombre —, sa courbe et sa vie.
[[nodiscard]] ro::render::NumberLook OnPath(const ro::render::NumberLook& motion,
                                            const NumberPath& path,
                                            const std::optional<ro::render::NumberLife>& life,
                                            const std::array<float, 2>& scatter) noexcept;

}  // namespace moonclient
