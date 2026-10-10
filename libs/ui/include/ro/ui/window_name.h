#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace ro::ui {

// LE NOM D'UNE FENÊTRE : un titre qui se traduit, une identité qui ne bouge pas.
//
// ImGui veut les deux en UNE chaîne — « Journal###log » —, ce qui les
// oppose : le titre change avec la langue, l'identité ne doit jamais changer
// sous peine de perdre la place et la taille que le joueur avait données.
//
// ⚠ ET LA RECOMPOSER À CHAQUE IMAGE COÛTERAIT UNE ALLOCATION PAR FENÊTRE ET PAR
// IMAGE, pour un texte qui ne bouge qu'au réglage. Cet objet la refait quand la
// langue change, et pas une fois de plus : il compare le compte de changements
// que `core::LanguageSerial` tient.
class WindowName {
 public:
  // `french` doit être un littéral : il vit aussi longtemps que le programme,
  // et c'est la clé du catalogue.
  //
  // ⚠ L'IDENTITÉ, ELLE, SE COMPOSE : une fenêtre qui existe en plusieurs
  // exemplaires — les conversations détachées — a besoin d'une identité par
  // exemplaire, sinon toutes partagent la même place à l'écran. On la garde
  // donc par valeur.
  WindowName(const char* french, std::string id) : french_(french), id_(std::move(id)) {}

  // Le nom complet, traduit dans la langue courante.
  [[nodiscard]] const char* Get();

 private:
  const char* french_;
  std::string id_;
  std::string composed_;
  // ⚠ UN COMPTE QUI NE PEUT PAS ÊTRE CELUI D'UNE LANGUE : il force la première
  // composition. Zéro serait le compte du français au démarrage, et le nom
  // resterait vide jusqu'au premier changement de langue.
  std::size_t composed_for_ = static_cast<std::size_t>(-1);
};

}  // namespace ro::ui
