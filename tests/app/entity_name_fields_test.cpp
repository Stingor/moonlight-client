// QUEL RÉGLAGE D'ÉTIQUETTE EXISTE SUR QUELLE ESPÈCE.
//
// 🔴 CE BANC GARDE UNE PAGE DE RÉGLAGES INCOHÉRENTE, ET C'EST LE JOUEUR QUI
// L'AVAIT VUE : « des réglages spécifiques à un type d'entité sont présents dans
// toutes les autres ». L'insigne de boss se proposait sous l'onglet des objets
// au sol, la couleur d'écart de niveau sous celui des joueurs — grisés, mais là,
// et un réglage grisé promet qu'il servira un jour.
//
// ⚠ CE QUE CE TABLEAU FIXE VIENT DU PROTOCOLE, PAS D'UN GOÛT. Chaque case
// « non » ci-dessous est une chaîne que le serveur n'envoie jamais pour cette
// espèce-là : voir `entity_name_fields.h`, qui porte le paquet de chacune.
//
// ⚠ ET IL NE PROUVE PAS QUE LE PANNEAU S'EN SERT : `BuildEntityNameRows` ne se
// monte pas hors du jeu — ses lignes vivent dans l'exécutable, avec `main`. Ce
// qui tient la règle là-bas est que TOUTE rangée passe par un seul entonnoir,
// qui écrit aussi son identifiant : une ligne qui contournerait ce tableau
// n'aurait pas de numéro.

#include "options/entity_name_fields.h"

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/entity_names.h"

using moonclient::EntityNameField;
using moonclient::EntityNameFieldApplies;
using ro::render::NamePlateKind;

namespace {

// LES SIX ESPÈCES DANS L'ORDRE DES ONGLETS, et ce que chacune porte.
//
// ⚠ ÉCRIT EN TOUTES LETTRES plutôt que calculé : un tableau attendu qui se
// déduirait de la fonction éprouvée dirait toujours oui.
struct KindRow {
  NamePlateKind kind;
  // Dans l'ordre de `EntityNameField` : montrer, quand, titre, parenthèse,
  // seconde ligne, insigne, couleur, écart de niveau, ombre, couleur d'ombre,
  // écart, opacité.
  bool has[static_cast<std::size_t>(EntityNameField::kCount)];
};

constexpr KindRow kExpected[] = {
    {NamePlateKind::kOwn, {1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1}},
    {NamePlateKind::kPlayer, {1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1}},
    // Le PNJ perd la parenthèse et la seconde ligne : son paquet de nom ne
    // porte que son nom et son titre.
    {NamePlateKind::kNonPlayer, {1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1}},
    // Le compagnon a le même paquet de nom qu'un PNJ.
    {NamePlateKind::kCompanion, {1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1}},
    {NamePlateKind::kMonster, {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1}},
    // Le boss est la seule espèce à porter les douze.
    {NamePlateKind::kBoss, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    // L'objet au sol perd le titre et la seconde ligne : c'est nous qui
    // composons son nom, et sa parenthèse est la quantité.
    {NamePlateKind::kGroundItem, {1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1}},
};

}  // namespace

TEST_CASE("Réglages d'étiquette : ce que chaque espèce porte", "[app][noms][options]") {
  for (const KindRow& expected : kExpected) {
    for (int rank = 0; rank < static_cast<int>(EntityNameField::kCount); ++rank) {
      const auto field = static_cast<EntityNameField>(rank);
      CHECK(EntityNameFieldApplies(expected.kind, field) ==
            expected.has[static_cast<std::size_t>(rank)]);
    }
  }
}

// 🔴 L'INSIGNE EST À UNE SEULE ESPÈCE, L'ÉCART DE NIVEAU À DEUX. Le compte est
// dit à part du tableau : c'est lui qui mord si quelqu'un rouvre un champ « pour
// que la page se ressemble d'un onglet à l'autre ».
TEST_CASE("Réglages d'étiquette : les deux réglages qui n'appartiennent pas à tous",
          "[app][noms][options]") {
  int badge = 0;
  int level = 0;
  for (std::size_t rank = 0; rank < ro::render::kNamePlateKindCount; ++rank) {
    const auto kind = static_cast<NamePlateKind>(rank);
    badge += EntityNameFieldApplies(kind, EntityNameField::kBossBadge) ? 1 : 0;
    level += EntityNameFieldApplies(kind, EntityNameField::kLevelGap) ? 1 : 0;
  }
  CHECK(badge == 1);
  CHECK(level == 2);
  CHECK(EntityNameFieldApplies(NamePlateKind::kBoss, EntityNameField::kBossBadge));
  CHECK(EntityNameFieldApplies(NamePlateKind::kMonster, EntityNameField::kLevelGap));
  CHECK(EntityNameFieldApplies(NamePlateKind::kBoss, EntityNameField::kLevelGap));
}

// ⚠ UN ONGLET NE SE VIDE JAMAIS : ce qui décide qu'une étiquette paraît, sa
// couleur et sa place valent pour les six. Une espèce qui perdrait son maître
// n'aurait plus de moyen de s'éteindre.
TEST_CASE("Réglages d'étiquette : ce qui vaut pour les six espèces", "[app][noms][options]") {
  constexpr EntityNameField kEverywhere[] = {
      EntityNameField::kShown,  EntityNameField::kWhen,        EntityNameField::kColor,
      EntityNameField::kShadow, EntityNameField::kShadowColor, EntityNameField::kOffset,
      EntityNameField::kOpacity};
  for (std::size_t rank = 0; rank < ro::render::kNamePlateKindCount; ++rank) {
    const auto kind = static_cast<NamePlateKind>(rank);
    for (const EntityNameField field : kEverywhere) {
      CHECK(EntityNameFieldApplies(kind, field));
    }
    // ⚠ ET LA BORNE DE L'ÉNUMÉRATION N'EST PAS UN RÉGLAGE : une boucle qui
    // monterait jusqu'à elle ne doit pas composer une treizième ligne.
    CHECK_FALSE(EntityNameFieldApplies(kind, EntityNameField::kCount));
  }
}

// 🔴 SEUL LE BOSS PORTE LES DOUZE, ET AUCUN ONGLET NE DESCEND SOUS HUIT. Le
// compte par onglet est ce qu'on lit à l'écran, et c'est la seule forme du
// tableau qu'un joueur puisse vérifier lui-même, en comptant ses lignes.
TEST_CASE("Réglages d'étiquette : combien de lignes par onglet", "[app][noms][options]") {
  const auto counted = [](NamePlateKind kind) {
    int rows = 0;
    for (int rank = 0; rank < static_cast<int>(EntityNameField::kCount); ++rank) {
      rows += EntityNameFieldApplies(kind, static_cast<EntityNameField>(rank)) ? 1 : 0;
    }
    return rows;
  };
  CHECK(counted(NamePlateKind::kOwn) == 10);
  CHECK(counted(NamePlateKind::kPlayer) == 10);
  CHECK(counted(NamePlateKind::kNonPlayer) == 8);
  CHECK(counted(NamePlateKind::kCompanion) == 8);
  CHECK(counted(NamePlateKind::kMonster) == 11);
  CHECK(counted(NamePlateKind::kBoss) == 12);
  CHECK(counted(NamePlateKind::kGroundItem) == 8);
}
