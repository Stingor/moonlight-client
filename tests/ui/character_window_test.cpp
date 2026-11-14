#include "ro/ui/character_window.h"

#include <array>
#include <cstring>
#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::ui::CharacterWidthSteps;
using ro::ui::CostumeSlots;
using ro::ui::EquipSlot;
using ro::ui::EquipSlotLabel;
using ro::ui::EquipSlotView;
using ro::ui::kCharacterDollBlockPixels;
using ro::ui::kCostumeSlotCount;
using ro::ui::kEquipSlotCount;
using ro::ui::SnappedCharacterWidth;

TEST_CASE("Fiche : les onze cases portent onze libellés distincts", "[ui][fiche]") {
  // ⚠ UN LIBELLÉ EMPRUNTÉ NE SE VOIT PAS À LA RELECTURE et se voit très bien à
  // l'écran : deux cases vides qui s'annoncent pareil. C'est l'erreur que le
  // portage du sac a faite une fois, sur ses onglets.
  std::set<std::string> seen;
  for (std::size_t rank = 0; rank < kEquipSlotCount; ++rank) {
    const char* label = EquipSlotLabel(static_cast<EquipSlot>(rank));
    REQUIRE(label != nullptr);
    CHECK(std::strlen(label) > 0);
    seen.insert(label);
  }
  CHECK(seen.size() == kEquipSlotCount);
}

TEST_CASE("Fiche : les deux accessoires s'étiquettent en MIROIR du serveur", "[ui][fiche]") {
  // 🔴 CE N'EST PAS UNE ERREUR À RÉPARER. Le personnage se montre de face, donc
  // sa main droite est de notre côté gauche, et l'écran l'étiquette ainsi depuis
  // toujours. La case NOMMÉE d'après le bit gauche du serveur porte « Acc. R ».
  //
  // Sans cet essai, la prochaine relecture accordera les deux — et deux
  // accessoires différents changeront de place sans que rien ne le dise.
  CHECK(std::string(EquipSlotLabel(EquipSlot::kAccessoryLeft)) == "Acc. R");
  CHECK(std::string(EquipSlotLabel(EquipSlot::kAccessoryRight)) == "Acc. L");
}

TEST_CASE("Fiche : les trois coiffes ne s'annoncent pas pareil", "[ui][fiche]") {
  // Elles ne se distinguent que par leur dernier mot, et elles sont voisines sur
  // le mannequin : c'est la paire la plus facile à intervertir de tout l'écran.
  const std::string top = EquipSlotLabel(EquipSlot::kHeadTop);
  const std::string mid = EquipSlotLabel(EquipSlot::kHeadMid);
  const std::string low = EquipSlotLabel(EquipSlot::kHeadLow);
  CHECK(top == "Head\ntop");
  CHECK(mid == "Head\nmid");
  CHECK(low == "Head\nbot");
}

TEST_CASE("Fiche : le costume n'a que quatre places, et ce sont les bonnes", "[ui][fiche]") {
  const std::array<EquipSlot, kCostumeSlotCount> places = CostumeSlots();
  CHECK(places.size() == 4);
  // Colonne gauche, puis colonne droite : c'est l'ordre du dessin.
  CHECK(places[0] == EquipSlot::kHeadTop);
  CHECK(places[1] == EquipSlot::kHeadLow);
  CHECK(places[2] == EquipSlot::kHeadMid);
  CHECK(places[3] == EquipSlot::kGarment);

  // ⚠ NI LES MAINS NI L'ARMURE : un costume ne reprend pas ces places, et les y
  // mettre montrerait la vraie pièce comme si elle en était une d'apparat.
  for (const EquipSlot place : places) {
    CHECK(place != EquipSlot::kWeapon);
    CHECK(place != EquipSlot::kShield);
    CHECK(place != EquipSlot::kArmor);
    CHECK(place != EquipSlot::kAmmo);
  }
}

TEST_CASE("Fiche : le bloc du mannequin mesure ce que ses pièces font", "[ui][fiche]") {
  // Deux colonnes de cases, l'avatar, et deux jours. Le nombre n'est pas écrit à
  // la main : il se recompose, et c'est ce qui l'empêche de dériver quand une
  // des trois mesures change.
  CHECK(kCharacterDollBlockPixels == 44.0F + 6.0F + 130.0F + 6.0F + 44.0F);
}

TEST_CASE("Fiche : une case vide se reconnaît à sa PLACE dans le sac", "[ui][fiche]") {
  // ⚠ ET NON À SON NOM. Un objet peut très bien n'avoir aucun nom composé — la
  // table de l'installation peut l'ignorer — sans que sa case soit libre. La
  // place, elle, vaut zéro tant que rien n'y est.
  EquipSlotView empty;
  CHECK_FALSE(empty.filled());

  EquipSlotView nameless;
  nameless.client_slot = 7;
  CHECK(nameless.filled());
}

namespace {

// Trois crans plausibles : mannequin seul, + les nombres, + un volet a venir.
[[nodiscard]] CharacterWidthSteps Steps(float extra = 0.0F) {
  CharacterWidthSteps steps;
  steps.narrow = 300.0F;
  steps.wide = 560.0F;
  steps.extra = extra > 0.0F ? extra : steps.wide;
  steps.measured = true;
  return steps;
}

}  // namespace

TEST_CASE("Fiche : la largeur se cale sur le cran le plus proche", "[ui][fiche]") {
  // 🔴 C'EST CE QUI FAIT APPARAITRE LE VOLET DES NOMBRES D'UN SEUL GESTE. Sans
  // calage, le joueur tire la poignee et rien ne change tant qu'il n'a pas
  // franchi un seuil qu'aucun repere ne montre.
  const CharacterWidthSteps steps = Steps();
  CHECK(SnappedCharacterWidth(steps, 300.0F) == 300.0F);
  CHECK(SnappedCharacterWidth(steps, 560.0F) == 560.0F);
  // Juste au-dessus du cran etroit : on y retombe.
  CHECK(SnappedCharacterWidth(steps, 340.0F) == 300.0F);
  // Passe la moitie du chemin, on bascule.
  CHECK(SnappedCharacterWidth(steps, 500.0F) == 560.0F);
  // Et l'on ne depasse aucun des deux : tirer plus loin ne donne pas plus.
  CHECK(SnappedCharacterWidth(steps, 900.0F) == 560.0F);
  CHECK(SnappedCharacterWidth(steps, 10.0F) == 300.0F);
}

TEST_CASE("Fiche : le troisieme cran se confond avec le second tant qu'il est vide",
          "[ui][fiche]") {
  // ⚠ C'EST CE QUI RENDRA LE VOLET DU STAFF ADDITIF. Tant qu'aucun volet de plus
  // n'existe, `extra` vaut `wide` et le test s'annule de lui-meme : la bascule a
  // trois crans EST la bascule a deux crans, sans branche a ajouter le jour ou
  // le troisieme volet arrive.
  CHECK(SnappedCharacterWidth(Steps(), 700.0F) == 560.0F);

  // Et le jour ou il existe, le meme geste va plus loin.
  const CharacterWidthSteps steps_760 = Steps(760.0F);
  CHECK(SnappedCharacterWidth(steps_760, 700.0F) == 760.0F);
  CHECK(SnappedCharacterWidth(steps_760, 600.0F) == 560.0F);
}

TEST_CASE("Fiche : le cran le plus large SE REPOSE", "[ui][fiche]") {
  // 🔴 C'EST CE QUI REND LE VOLET DU STAFF GARDABLE. Le droit n'arrive qu'apres
  // l'entree sur la carte : les premieres images posent un plafond SANS volet,
  // et ImGui y rabote la fenetre restauree. Le souvenir du joueur vit donc dans
  // ses reglages, et cet ordre-ci repose le cran a l'instant du droit.
  CharacterWidthSteps steps = Steps(760.0F);
  steps.force_extra = true;
  CHECK(SnappedCharacterWidth(steps, 300.0F) == 760.0F);
  CHECK(SnappedCharacterWidth(steps, 560.0F) == 760.0F);

  // ⚠ ET IL PASSE DEVANT LE CRAN LARGE. Les deux ordres tombent sur la meme
  // image quand le grimoire est montre au moment ou le droit parait, et c'est
  // le plus large qui doit gagner : l'inverse rendrait le volet invisible une
  // image sur deux sans que rien ne le dise.
  steps.force_wide = true;
  CHECK(SnappedCharacterWidth(steps, 300.0F) == 760.0F);

  // Sans troisieme volet, l'ordre ne peut pas elargir au-dela de ce qui existe.
  CharacterWidthSteps forced = Steps();
  forced.force_extra = true;
  CHECK(SnappedCharacterWidth(forced, 300.0F) == 560.0F);
}

TEST_CASE("Fiche : un onglet pleine largeur interdit le cran etroit", "[ui][fiche]") {
  // Le grimoire etale sept colonnes d'icones : le replier couperait sa grille.
  CharacterWidthSteps steps = Steps();
  steps.force_wide = true;
  CHECK(SnappedCharacterWidth(steps, 300.0F) == 560.0F);
  CHECK(SnappedCharacterWidth(steps, 10.0F) == 560.0F);
}

TEST_CASE("Fiche : un onglet pleine largeur garde le volet du staff", "[ui][fiche]") {
  // 🔴 PASSER PAR LE GRIMOIRE NE REFERME RIEN. L'onglet interdit l'etroit, pas
  // le troisieme cran : le rabattre sur le large faisait disparaitre le volet
  // du staff sans que le joueur l'ait retreci.
  CharacterWidthSteps steps = Steps(760.0F);
  steps.force_wide = true;
  CHECK(SnappedCharacterWidth(steps, 760.0F) == 760.0F);
  CHECK(SnappedCharacterWidth(steps, 300.0F) == 560.0F);
}

TEST_CASE("Fiche : sans mesure, la largeur demandee passe telle quelle", "[ui][fiche]") {
  // ⚠ A LA PREMIERE IMAGE LES CRANS VALENT ZERO. Caler dessus collerait la
  // fenetre a rien avant qu'on ait mesure quoi que ce soit ; elle se cale une
  // image plus tard, ce qui ne se voit pas.
  const CharacterWidthSteps fresh;
  CHECK_FALSE(fresh.measured);
  CHECK(SnappedCharacterWidth(fresh, 480.0F) == 480.0F);
}
