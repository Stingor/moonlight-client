#pragma once

// CE QUE CHAQUE CHARGEMENT ENCHAÎNE, ET CE QUE LE JOUEUR EN LIT.
//
// Un plan dit les étapes d'un chargement et leur durée ordinaire : c'est le
// RAPPORT des durées qui fait la fraction, et donc la vitesse apparente de la
// barre. Voir `ro::core::LoadProgress`.
//
// ⚠ LOGIQUE PURE : ni archive, ni fenêtre.

#include <span>
#include <string>
#include <vector>

#include "ro/core/load_progress.h"

namespace moonclient {

// LE DÉMARRAGE, après le montage des archives : la coquille et le son.
[[nodiscard]] std::span<const ro::core::LoadStepWeight> StartupPlan() noexcept;
// La garde-robe du décor de l'écran de connexion.
[[nodiscard]] std::span<const ro::core::LoadStepWeight> WardrobePlan() noexcept;
// LE PASSAGE À LA TABLÉE : les tables du monde.
[[nodiscard]] std::span<const ro::core::LoadStepWeight> TablesPlan() noexcept;

// UNE CARTE, lue puis téléversée : un warp, le décor de l'écran de connexion.
// Le sol arrondi pèse à lui seul autant que tout le reste.
[[nodiscard]] std::vector<ro::core::LoadStepWeight> MapPlan(bool rounded_ground);

// L'ENTRÉE EN JEU : la carte, puis les tables si la tablée ne les a pas déjà
// lues, puis les catalogues d'effets.
[[nodiscard]] std::vector<ro::core::LoadStepWeight> GameOpenPlan(bool rounded_ground,
                                                                 bool tables_pending);

// Le nom traduit de l'étape.
[[nodiscard]] const char* LoadStepLabel(ro::core::LoadStep step);

// LA LIGNE DE L'ÉTAPE OUVERTE : son nom, et son compte quand elle en a un —
// « Modèles 312 / 540 ». Vide entre deux étapes.
[[nodiscard]] std::string LoadStepLine(const ro::core::LoadProgress& progress);

// La part faite, en pour cent entier, à la façon de la langue : « 57 % ».
[[nodiscard]] std::string LoadPercentText(float fraction);

}  // namespace moonclient
