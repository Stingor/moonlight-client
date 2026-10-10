#pragma once

#include <array>

namespace ro::audio {

// D'OÙ vient un son : ce que l'auditeur en entend, et de quel côté.
//
// Ce calcul est PUR. Le mélangeur sait placer un son dans l'espace, mais son
// modèle d'atténuation n'est pas réglable et n'est pas documenté ; le nôtre
// suit les nombres du jeu, et se vérifie par un essai numérique plutôt qu'à
// l'oreille.

// La portée d'un son, en unités du monde, et la distance en deçà de laquelle il
// est à plein volume.
//
// Ce sont les deux nombres que le client donne à chaque son placé, partout et
// sans exception : deux cent cinquante et quarante. Une cellule vaut cinq
// unités du monde — la portée couvre donc cinquante cases, la proximité huit.
inline constexpr float kHearingRange = 250.0F;
inline constexpr float kNearDistance = 40.0F;

// Ce qu'un son donne au mélangeur.
struct SoundPlacement {
  // De zéro — inaudible — à un.
  float gain = 0.0F;
  // De moins un, tout à gauche, à un, tout à droite.
  float pan = 0.0F;
};

// Où placer un son joué à `source`, pour un auditeur en `listener` dont l'axe
// DROIT est `listener_right`.
//
// L'auditeur est le PERSONNAGE, pas le point de vue ; qui l'est exactement se
// décide plus haut, mais l'atténuation ci-dessous n'a de sens que mesurée
// depuis lui.
//
// L'axe droit, lui, vient de la caméra : c'est ce qui fait qu'un son passe
// d'une oreille à l'autre quand on fait pivoter la vue, et non quand le monde
// bouge. Il est supposé unitaire ; un axe nul ne panoramique pas, ce qui vaut
// mieux que de diviser par zéro.
//
// La HAUTEUR ne compte pas dans la distance : le client la mesure au sol et
// passe zéro pour l'axe vertical. Sur une carte en pente, deux cases d'écart
// peuvent valoir trente unités de dénivelé, et un bruit tout proche n'a pas à
// s'éteindre parce que son auteur est en contrebas.
//
// L'extinction est en INVERSE de la distance : au-delà de la proximité, le
// volume vaut proximité / distance. Deux fois plus loin que la proximité,
// moitié moins fort ; quatre fois plus loin, le quart. C'est beaucoup plus
// rapide qu'une droite, et c'est ce que fait le client dans les deux chemins
// qu'il connaît — le sien, et celui du moteur sonore.
//
// À la portée, la coupure est FRANCHE : le volume y vaudrait encore seize pour
// cent, et il tombe à zéro d'un coup. C'est ce que fait le client, et c'est ce
// qui garde les voix pour les sons qu'on entend vraiment.
[[nodiscard]] SoundPlacement PlaceSound(const std::array<float, 3>& listener,
                                        const std::array<float, 3>& listener_right,
                                        const std::array<float, 3>& source);

// La même chose, pour un son qui donne SA portée et SA proximité. Seules les
// sources de son d'une carte le font : chacune déclare jusqu'où elle s'entend,
// et le client prend le sixième de cette portée pour proximité.
[[nodiscard]] SoundPlacement PlaceSound(const std::array<float, 3>& listener,
                                        const std::array<float, 3>& listener_right,
                                        const std::array<float, 3>& source, float hearing_range,
                                        float near_distance);

}  // namespace ro::audio
