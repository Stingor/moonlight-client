#pragma once

namespace moonclient {

// Le volume, et toute part, se GARDE de zéro à un — l'unité du moteur — et se
// MONTRE en pourcentage, qui est ce qu'un joueur lit.
//
// ⚠ UN SEUL FOYER, dans un en-tête qui ne tire rien : les rangées le lisent, et
// la bibliothèque des dégâts aussi, sans la fenêtre d'options. Deux copies
// d'une même unité divergent le jour où l'une est touchée, et rien ne le dit.
inline constexpr float kPercent = 100.0F;

}  // namespace moonclient
