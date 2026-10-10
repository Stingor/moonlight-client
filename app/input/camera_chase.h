#pragma once

#include <cmath>
#include <cstdint>

#include "ro/render/terrain_mesh.h"
#include "ro/sim/walk_path.h"

// COMMENT LA CAMÉRA RATTRAPE LE PERSONNAGE, et à partir de quand elle renonce
// à le rattraper pour se poser sur lui.
//
// La règle vit à part de l'objet qui la porte — CameraTracking, dans
// camera_aim.h — parce qu'elle se CALCULE : son seuil se dérive de la vitesse
// la plus grande que le serveur autorise, et cette dérivation est ce qu'un
// essai doit pouvoir vérifier sans fenêtre ni carte graphique.
namespace moonclient {

// DE COMBIEN LA CAMÉRA RATTRAPE SON RETARD À CHAQUE IMAGE DU CLIENT : un
// DIXIÈME.
//
// ⚠ ET CE N'EST PAS LE NOMBRE À EMPLOYER TEL QUEL. Le lissage du client tourne
// dans sa boucle de rendu, sans horloge : « un dixième par image » ne veut dire
// quelque chose qu'à SA cadence. La nôtre ne règle aucun intervalle de
// présentation et tourne bien plus vite ; appliquer le même dixième y rattrape
// plusieurs fois plus vite, et la caméra redevient presque collée — ce qui
// annule exactement ce qu'on cherchait.
//
// On convertit donc en CONSTANTE DE TEMPS, calibrée sur soixante images par
// seconde. ⚠ Cette cadence est une hypothèse, la plus ordinaire du client ; elle
// se remesure si le flottement paraît encore faux.
inline constexpr float kCatchUpPerClientFrame = 0.1F;
inline constexpr float kClientFrameMs = 1000.0F / 60.0F;

// Ce qui RESTE du retard après ce temps-là, si l'on en retirait un dixième à
// chaque image du client.
[[nodiscard]] inline float RemainingLagAfter(float elapsed_ms) noexcept {
  return std::pow(1.0F - kCatchUpPerClientFrame, elapsed_ms / kClientFrameMs);
}

// LE TEMPS QUE MET CE LISSAGE, en millisecondes : derrière une cible qui avance
// à vitesse constante, le retard d'équilibre vaut AU PLUS cette durée-ci
// multipliée par la vitesse.
//
// C'est la constante de temps de l'exponentielle ci-dessus, prise à la limite
// des images courtes. ⚠ AU PLUS, ET NON EXACTEMENT : le retard d'équilibre
// dépend un peu de la cadence — cent cinquante millisecondes à soixante images
// par seconde, cent trente-cinq à vingt — et c'est cette limite-là qui les
// majore toutes. Un seuil veut le MAJORANT ; l'égalité serait fausse, et un
// essai qui l'affirme échoue.
[[nodiscard]] inline float CatchUpTimeMs() noexcept {
  return kClientFrameMs / -std::log1p(-kCatchUpPerClientFrame);
}

// LA MARCHE LA PLUS RAPIDE, en unités du monde par milliseconde.
inline constexpr float kFastestWalkUnitsPerMs =
    ro::render::kWorldUnitsPerCell / static_cast<float>(ro::sim::kFastestStepMs);

// LE RETARD QUE LA POURSUITE ACCUMULE DERRIÈRE CETTE MARCHE-LÀ, en unités du
// monde. Rien de ce qu'un personnage fait en MARCHANT ne va au-delà.
[[nodiscard]] inline float ChaseLagUnits() noexcept {
  return kFastestWalkUnitsPerMs * CatchUpTimeMs();
}

// AU-DELÀ DE COMBIEN LA CAMÉRA SE POSE au lieu de rattraper, en unités du monde.
//
// Une téléportation ne se rattrape pas : la caméra traverserait la carte en
// glissant, en montrant tout ce qu'il y a entre les deux. Ce seuil est NOTRE
// décision — le client, lui, remet sa caméra ailleurs, au chargement de carte.
//
// 🔴 IL SE DÉRIVE DU RETARD DE POURSUITE, ET C'EST TOUT L'ENJEU. Un seuil posé
// « à trois cellules » sépare bien les deux cas à la vitesse ORDINAIRE, et se
// retourne à la vitesse maximale : la poursuite y laisse à elle seule huit
// cellules de retard, si bien que la caméra se croit téléportée à chaque image.
// Elle se pose, relisse, reprend son retard, se repose — et tout l'écran
// tremble à une douzaine de fois par seconde. Voir ro::sim::kFastestStepMs.
//
// La marge double le retard de poursuite : elle absorbe une image longue, où
// le retard dépasse passagèrement sa valeur d'équilibre.
inline constexpr float kChaseLagMargin = 2.0F;

[[nodiscard]] inline float TeleportUnits() noexcept { return ChaseLagUnits() * kChaseLagMargin; }

}  // namespace moonclient
