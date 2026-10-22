#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/actor.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// L'ÉCHOPPE D'UN MARCHAND LOINTAIN : on marche jusqu'à elle, puis on l'ouvre.
//
// Le serveur ne traite avec une échoppe qu'à la portée du JEU
// (`SessionSettings::GameAreaCells`), en cases de Tchebychev : il n'ouvre
// l'échoppe d'achat, ne lui vend, et n'achète dans celle de vente qu'à cette
// distance du marchand. Il ne lui accorde PAS la case de grâce qu'il donne à un
// PNJ. moonclient voit plus loin : au-delà, le clic s'approche comme d'un PNJ
// lointain — par étapes s'il le faut, voir `PendingAction` —, et la demande
// d'échoppe part à l'arrivée.
//
// ⚠ LA LISTE D'UN VENDEUR, ELLE, S'OBTIENT DE N'IMPORTE OÙ. Mais l'achat qui la
// suit échouerait sans un mot : on s'approche donc des deux échoppes de même.

// La marge prise sous la portée du jeu pour s'arrêter, en cases.
//
// UNE pour la tolérance du test de portée (`kRangeToleranceCells`), UNE pour la
// case du serveur, qui retarde d'un pas au plus sur la position dessinée : la
// demande part à l'image où l'on se voit à portée, et le serveur la mesure
// depuis sa case à lui.
inline constexpr int kStoreReachMarginCells = 2;

// La portée d'approche d'une échoppe, en cases : la portée du jeu moins la
// marge, et une case au moins.
[[nodiscard]] std::uint16_t StoreApproachRangeCells(const World& world);

// Vrai quand le personnage traite d'ici avec ce marchand : la case qu'il a
// quittée comme celle où il va sont à la portée du jeu. Sans personnage, faux.
[[nodiscard]] bool StoreWithinReach(const World& world, const Actor& merchant);

// Ce qu'un clic sur l'échoppe d'un marchand trop loin INSCRIT. Rien quand
// l'échoppe se traite d'ici, ou que l'acteur ne tient pas d'échoppe : la
// demande part alors telle quelle.
[[nodiscard]] std::optional<PendingAction> ArmFarStoreVisit(const World& world,
                                                            const Actor& merchant);

// UNE APPROCHE QUI N'EST PAS UN COMBAT s'abandonne-t-elle ici ? La parole à un
// PNJ, le ramassage et la visite d'une échoppe lointains : Échap les défait,
// comme un autre clic. Rend vrai, et efface l'action, quand c'en était une ;
// une poursuite d'attaque reste.
[[nodiscard]] bool DropErrandApproach(std::optional<PendingAction>& action) noexcept;

}  // namespace ro::sim
