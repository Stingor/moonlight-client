// LES RÉGLAGES DES ÉTIQUETTES D'ENTITÉ : qui paraît, et de quelle couleur.
//
// 🔴 CE BANC EXISTE PARCE QUE LE FILTRE VIT CHEZ `render` ET NON DANS LA BOUCLE
// DE DESSIN. « Quand un nom paraît » se vérifie ici sans monde, sans fenêtre et
// sans carte graphique — et c'est toute la raison d'avoir séparé les deux.

#include "ro/render/entity_names.h"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/name_plate.h"

using ro::render::DefaultEntityNames;
using ro::render::EntityNamesLook;
using ro::render::EntityNameView;
using ro::render::NamePlateKind;
using ro::render::NamePlateWhen;
using ro::render::NamePlateWhenOf;
using ro::render::PackedColor;
using ro::render::ShowsEntityName;
using ro::render::UnpackedColor;

namespace {

// Toutes les espèces montrées et TOUJOURS : un banc qui part du défaut
// testerait le défaut, pas le filtre.
[[nodiscard]] EntityNamesLook AlwaysShown() {
  EntityNamesLook look;
  for (ro::render::EntityNameLook& one : look.kinds) {
    one.shown = true;
    one.when = NamePlateWhen::kAlways;
  }
  return look;
}

[[nodiscard]] EntityNameView Monster(bool hovered, bool targeted) {
  return EntityNameView{.kind = NamePlateKind::kMonster, .hovered = hovered, .targeted = targeted};
}

}  // namespace

TEST_CASE("Étiquettes : le maître éteint tout", "[render][noms]") {
  EntityNamesLook look = AlwaysShown();
  CHECK(ShowsEntityName(look, Monster(false, false)));
  look.shown = false;
  CHECK_FALSE(ShowsEntityName(look, Monster(true, true)));
}

TEST_CASE("Étiquettes : chaque espèce décide de la sienne", "[render][noms]") {
  EntityNamesLook look = AlwaysShown();
  look.kinds[static_cast<std::size_t>(NamePlateKind::kMonster)].shown = false;
  CHECK_FALSE(ShowsEntityName(look, Monster(true, true)));
  // ⚠ ET SEULEMENT DE LA SIENNE : c'est tout l'objet des onglets.
  CHECK(ShowsEntityName(look, EntityNameView{.kind = NamePlateKind::kBoss}));
}

TEST_CASE("Étiquettes : les quatre moments", "[render][noms]") {
  EntityNamesLook look = AlwaysShown();
  ro::render::EntityNameLook& monster =
      look.kinds[static_cast<std::size_t>(NamePlateKind::kMonster)];

  monster.when = NamePlateWhen::kAlways;
  CHECK(ShowsEntityName(look, Monster(false, false)));

  monster.when = NamePlateWhen::kHovered;
  CHECK(ShowsEntityName(look, Monster(true, false)));
  CHECK_FALSE(ShowsEntityName(look, Monster(false, true)));

  monster.when = NamePlateWhen::kTarget;
  CHECK(ShowsEntityName(look, Monster(false, true)));
  CHECK_FALSE(ShowsEntityName(look, Monster(true, false)));

  monster.when = NamePlateWhen::kHoveredOrTarget;
  CHECK(ShowsEntityName(look, Monster(true, false)));
  CHECK(ShowsEntityName(look, Monster(false, true)));
  CHECK_FALSE(ShowsEntityName(look, Monster(false, false)));
}

// 🔴 LE FICHIER DE RÉGLAGES S'OUVRE DANS UN ÉDITEUR DE TEXTE, et un rang hors
// bornes rendu tel quel serait un comportement indéfini — pas une valeur
// bizarre. Le repli est le comportement du client : au survol.
TEST_CASE("Étiquettes : un rang illisible se replie sur le client", "[render][noms]") {
  CHECK(NamePlateWhenOf(0) == NamePlateWhen::kAlways);
  CHECK(NamePlateWhenOf(3) == NamePlateWhen::kHoveredOrTarget);
  CHECK(NamePlateWhenOf(-1) == NamePlateWhen::kHovered);
  CHECK(NamePlateWhenOf(4) == NamePlateWhen::kHovered);
  CHECK(NamePlateWhenOf(9'999) == NamePlateWhen::kHovered);
}

// 🔴 L'ORDRE DES OCTETS EST CELUI D'`IM_COL32` — LE ROUGE EN POIDS FAIBLE — et
// ce banc le fixe. Le nuancier du panneau travaille dans ce format ; une
// inversion ici rendrait bleu ce que le client peint en rouge, et c'est
// exactement l'erreur qui a vécu des mois dans `name_plate.h`.
TEST_CASE("Étiquettes : les couleurs s'empaquettent dans l'ordre du nuancier", "[render][noms]") {
  constexpr std::array<std::uint8_t, 4> kRed = {0xFF, 0x00, 0x00, 0xFF};
  CHECK(PackedColor(kRed) == 0xFF0000FFU);
  CHECK(UnpackedColor(0xFF0000FFU) == kRed);

  // L'aller-retour ne perd rien, alpha compris.
  constexpr std::array<std::uint8_t, 4> kOmbre = {0x10, 0x20, 0x30, 0xC8};
  CHECK(UnpackedColor(PackedColor(kOmbre)) == kOmbre);
}

// ⚠ LES DÉFAUTS SONT UNE DÉCISION, pas un agrégat vide : ce banc dit lesquels
// s'écartent du commun, et pourquoi il faudrait y regarder à deux fois avant de
// les changer.
TEST_CASE("Étiquettes : ce qui s'écarte du défaut commun", "[render][noms]") {
  const auto names = DefaultEntityNames();

  // La mienne est ÉTEINTE : le client n'écrit pas mon nom sous mes pieds.
  CHECK_FALSE(names[static_cast<std::size_t>(NamePlateKind::kOwn)].shown);

  // Les autres suivent le client : au survol.
  CHECK(names[static_cast<std::size_t>(NamePlateKind::kNonPlayer)].when == NamePlateWhen::kHovered);
  CHECK(names[static_cast<std::size_t>(NamePlateKind::kGroundItem)].when ==
        NamePlateWhen::kHovered);
  CHECK(names[static_cast<std::size_t>(NamePlateKind::kPlayer)].when == NamePlateWhen::kHovered);
  CHECK(names[static_cast<std::size_t>(NamePlateKind::kMonster)].when == NamePlateWhen::kHovered);

  // L'ÉCART DE NIVEAU N'APPARTIENT QU'AUX MONSTRES ET AUX BOSS : ailleurs, le
  // client ne l'applique pas, et le niveau ne s'écrit pas par ce chemin.
  CHECK(names[static_cast<std::size_t>(NamePlateKind::kMonster)].level_gap);
  CHECK(names[static_cast<std::size_t>(NamePlateKind::kBoss)].level_gap);
  CHECK_FALSE(names[static_cast<std::size_t>(NamePlateKind::kPlayer)].level_gap);
  CHECK_FALSE(names[static_cast<std::size_t>(NamePlateKind::kNonPlayer)].level_gap);

  // ET LES COULEURS SONT CELLES DU CLIENT : rose clair sur un monstre, blanc
  // sur un joueur. Voir `name_plate.h` — ces octets se lisent à l'envers d'une
  // couleur COLORREF.
  CHECK(UnpackedColor(names[static_cast<std::size_t>(NamePlateKind::kMonster)].color) ==
        ro::render::kMonsterNameColor);
  CHECK(UnpackedColor(names[static_cast<std::size_t>(NamePlateKind::kPlayer)].color) ==
        ro::render::kPlayerNameColor);
  CHECK(UnpackedColor(names[static_cast<std::size_t>(NamePlateKind::kOwn)].color) ==
        ro::render::kOwnNameColor);
}
