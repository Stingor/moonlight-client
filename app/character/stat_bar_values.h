#pragma once

#include <cstdint>

#include "ro/sim/world_fwd.h"
#include "ro/ui/stat_bars.h"

namespace moonclient {

// CE QUE LES SEPT JAUGES MONTRENT, croisé depuis le monde.
//
// 🔴 ET C'EST `app` QUI CROISE, parce qu'il est le seul à voir les deux sources.
// La VIE vit sur l'`Actor` — c'est elle qui dessine les barres au-dessus des
// têtes, et le joueur n'y fait pas exception ; tout le reste vit dans son
// `PlayerStatus`. Les dédoubler ferait diverger sa jauge de celle des autres le
// jour où l'une des deux voies manquerait un paquet.
//
// ⚠ CE QUE LE SERVEUR N'A PAS DIT RESTE INCONNU. `kUnknownStat` vaut -1 : le
// recopier tel quel donnerait des jauges négatives, et le forcer à zéro
// annoncerait une bourse vide ou une mort. Chaque jauge porte donc son `known`.
// ⚠ `zeny_reference` EST CE QUE « BOURSE PLEINE » VEUT DIRE, et il vient des
// réglages : une bourse n'a pas de maximum, seulement un repère choisi.
// ⚠ `own_cast` ARRIVE TOUT FAIT : la règle de l'incantation montrée est celle
// des barres au-dessus des têtes, et elle vit avec elles — voir
// `world/cast_bar_view.h`.
[[nodiscard]] ro::ui::StatBarValues StatBarValuesOf(const ro::sim::World& world,
                                                    std::int64_t zeny_reference,
                                                    ro::ui::StatBarValue own_cast = {});

// LES VALEURS DU MODE RÉGLAGE : chaque jauge ondule du vide au plein, à sa
// propre période pour qu'elles ne battent pas ensemble. ⚠ FACTICES : voir
// `StatBarsLook::tuning`.
//
// La bourse se remplit contre le repère du joueur, comme la vraie ; la jauge
// d'incantation rejoue un sort de trois secondes.
[[nodiscard]] ro::ui::StatBarValues TuningStatBarValues(std::uint32_t now_ms,
                                                        std::int64_t zeny_reference);

// LE SEUIL DU MODE RÉGLAGE : celui du serveur quand il l'a dit, sinon celui que
// rAthena annonce d'ordinaire — il faut un seuil pour voir la bascule.
[[nodiscard]] ro::ui::StatBarWarnings TuningStatBarWarnings(const ro::sim::World& world);

// LE SEUIL DE SURCHARGE QUE LE SERVEUR ANNONCE, ou zéro s'il ne l'a pas dit.
[[nodiscard]] ro::ui::StatBarWarnings StatBarWarningsOf(const ro::sim::World& world);

}  // namespace moonclient
