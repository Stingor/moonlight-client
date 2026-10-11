#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/gl_context.h"

// L'ÉCRAN OÙ LE JEU S'AFFICHE : comment le choix se garde, et comment il se
// retrouve parmi les écrans branchés.
//
// ⚠ QUE DES DONNÉES : la liste des écrans arrive toute faite. C'est ce qui
// permet à un essai de débrancher un moniteur sans en avoir un.
namespace moonclient {

// L'ÉCRAN CHOISI, tel que le fichier le garde.
//
// 🔴 PAR SON NOM, ET LE RANG EN DÉPARTAGE SEULEMENT. Le rang seul désignerait
// un autre moniteur dès que le système réordonne sa liste — un câble rebranché
// suffit. Le nom seul ne sépare pas deux moniteurs du même modèle.
//
// Nom vide = jamais choisi : la fenêtre va sur l'écran principal.
struct ScreenChoice {
  std::string name;
  int rank = -1;

  [[nodiscard]] bool operator==(const ScreenChoice& other) const = default;
};

// CE QUE LA LISTE « Écran » OFFRE, relu à chaque composition des lignes.
//
// ⚠ LES POINTEURS DE `choices` DÉSIGNENT `labels` : les deux se refont
// ensemble, par `OfferScreens`, jamais l'un sans l'autre.
struct ScreenOffer {
  std::vector<ro::render::ScreenInfo> screens;
  std::vector<std::string> labels;
  std::vector<const char*> choices;
};

// Le rang de l'écran à employer parmi `connected` :
//   • celui du même nom au même rang ;
//   • sinon celui du même nom au rang le plus proche ;
//   • sinon — jamais choisi, ou débranché — l'écran principal, et le premier
//     si aucun ne se dit principal.
// Moins un quand aucun écran ne répond.
[[nodiscard]] int PickScreen(const ScreenChoice& saved,
                             std::span<const ro::render::ScreenInfo> connected);

// Le libellé d'un écran, numéroté à partir de un : « 1 — DELL U2419H
// (1920×1080) ». ⚠ La taille disparaît quand l'écran ne la donne pas, et le
// nom quand le système n'en a pas.
[[nodiscard]] std::string ScreenLabel(int rank, const ro::render::ScreenInfo& screen);

// Refait l'offre à partir des écrans branchés.
void OfferScreens(std::span<const ro::render::ScreenInfo> connected, ScreenOffer& into);

// Ce que le rang choisi dans la liste désigne. ⚠ Un rang hors liste rend
// `kept` : mieux vaut ne rien changer que changer au hasard.
[[nodiscard]] ScreenChoice ScreenChoiceAt(const ScreenOffer& offer, int rank,
                                          const ScreenChoice& kept);

}  // namespace moonclient
