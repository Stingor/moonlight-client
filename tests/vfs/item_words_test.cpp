#include "ro/vfs/item_words.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::core::Tr;
using ro::test_support::TempDirectory;
using ro::vfs::CardInsertText;
using ro::vfs::CardWord;
using ro::vfs::DroppedItemText;
using ro::vfs::EquipShownText;
using ro::vfs::FormatTakesOneInt;
using ro::vfs::FormatTakesStringThenInt;
using ro::vfs::GotItemText;
using ro::vfs::ItemPickupRefusal;
using ro::vfs::ItemWords;
using ro::vfs::ObtainedText;
using ro::vfs::PickupRefusalText;
using ro::vfs::ReadItemWords;
using ro::vfs::Vfs;

namespace {

// Même montage nu que pour la table des objets : sans archive, il faut un
// dossier `data` pour que le montage ait quelque chose à monter.
void MakeClient(const TempDirectory& client) { client.WriteText("data/marqueur.txt", ""); }

[[nodiscard]] ItemWords Read(const TempDirectory& client) {
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto words = ReadItemWords(*vfs);
  REQUIRE(words.has_value());
  return std::move(*words);
}

}  // namespace

TEST_CASE("Mots d'objets : les ornements du nom sont des textes de l'interface", "[vfs][mots]") {
  // Posés par la langue courante, jamais lus dans l'installation. Les espaces
  // de bord leur appartiennent : c'est eux qui séparent l'ornement du nom.
  TempDirectory client;
  MakeClient(client);
  const ItemWords words = Read(client);
  CHECK(words.nameless_creator == Tr("Sans nom"));
  CHECK(words.RepeatedWord(2) == Tr("Double"));
  CHECK(words.ForgeStrengthWord(5) == std::string(Tr("Très robuste")) + " ");
  CHECK(words.PossessiveWord(1101, 3) == std::string(Tr(": Feu")) + " ");
  // Une carte seule ne se multiplie pas, et seules 5, 10 et 15 étoiles parlent.
  CHECK(words.RepeatedWord(1).empty());
  CHECK(words.ForgeStrengthWord(6).empty());
}

TEST_CASE("Mots d'objets : la liste des suffixes DÉPLACE un mot, elle n'en donne aucun",
          "[vfs][mots]") {
  // 🔴 LA RÈGLE QUI A FAILLI PASSER À CÔTÉ. `card_postfixes.yml` ne porte aucun
  // mot : il nomme les cartes dont le mot déménage derrière le nom — et sur
  // l'installation réelle, cela concerne 509 mots. Les deux tables sont ce que
  // la conversion tire de « 4001#Lucky#\n4014#of Champion#\n4099##\n » et de
  // « 4014#\n ».
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/yaml_files/card_prefixes.yml",
                   "version: 1\ncards:\n  4001: Lucky\n  4014: of Champion\n  4099: ''\n");
  client.WriteText("data/yaml_files/card_postfixes.yml", "version: 1\ncards:\n- 4014\n");

  const ItemWords words = Read(client);
  REQUIRE(words.cards.size() == 3);

  const CardWord* lucky = words.FindCard(4001);
  REQUIRE(lucky != nullptr);
  CHECK(lucky->text == "Lucky");
  CHECK_FALSE(lucky->after_name);

  const CardWord* champion = words.FindCard(4014);
  REQUIRE(champion != nullptr);
  CHECK(champion->text == "of Champion");
  CHECK(champion->after_name);

  // Une carte CONNUE mais MUETTE n'est pas une absence : le client distingue les
  // deux, et c'est la présence qui décide si l'objet suit la voie des cartes.
  const CardWord* silent = words.FindCard(4099);
  REQUIRE(silent != nullptr);
  CHECK(silent->text.empty());
  CHECK(words.FindCard(4002) == nullptr);
}

TEST_CASE("Mots d'objets : une carte nommée en suffixe mais sans mot n'a rien à déplacer",
          "[vfs][mots]") {
  auto cards = ro::vfs::ParseCardWords("version: 1\ncards:\n  4001: Lucky\n");
  REQUIRE(cards.has_value());
  REQUIRE(ro::vfs::ParseCardWordSides("version: 1\ncards:\n- 4001\n- 4777\n", *cards));
  CHECK(cards->size() == 1);
  REQUIRE(cards->contains(4001));
  CHECK(cards->at(4001).after_name);
}

TEST_CASE("Mots d'objets : une installation sans ces fichiers se lit quand même", "[vfs][mots]") {
  // Un nom reste lisible sans les mots de cartes : il perd ses ornements, pas
  // son sens. Refuser de démarrer pour cela serait hors de proportion.
  TempDirectory client;
  MakeClient(client);
  const ItemWords words = Read(client);
  CHECK(words.cards.empty());
  CHECK(words.FindCard(4001) == nullptr);
  // Les mots de l'interface, eux, ne dépendent d'aucun fichier.
  CHECK(words.RepeatedWord(2) == Tr("Double"));
}

TEST_CASE("Mots d'objets : le suffixe du bandeau d'obtention garde son espace de tête",
          "[vfs][mots]") {
  // L'espace de TÊTE fait partie du suffixe : c'est elle qui sépare le nom du
  // reste. Une clé de traduction n'en porte pas ; le remplissage la remet.
  TempDirectory client;
  MakeClient(client);
  const ItemWords words = Read(client);
  CHECK(words.obtained_suffix == std::string(" ") + Tr("- %d obtenu."));
  CHECK(ObtainedText(words, 1).starts_with(" "));
  CHECK(ObtainedText(words, 1).find('1') != std::string::npos);
}

TEST_CASE("Mots d'objets : un gabarit de bandeau n'est pris qu'à UNE conversion entière",
          "[vfs][mots]") {
  // 🔴 LE GABARIT VIENT D'UN CATALOGUE DE LANGUE, qu'un joueur retouche : un « %s »
  // qui s'y glisserait lirait un pointeur pris sur la pile. Ce crible n'est pas
  // une politesse de style.
  CHECK(FormatTakesOneInt(" - %d obtained."));
  CHECK(FormatTakesOneInt("%i obtenus"));
  // « %% » est un pour-cent littéral : il n'attend aucun argument.
  CHECK(FormatTakesOneInt("100%% %d"));
  CHECK_FALSE(FormatTakesOneInt("%s"));
  CHECK_FALSE(FormatTakesOneInt("%d %d"));
  CHECK_FALSE(FormatTakesOneInt("%n"));
  CHECK_FALSE(FormatTakesOneInt("obtenu"));
  CHECK_FALSE(FormatTakesOneInt(""));
  // Un « % » en dernier caractère n'ouvre rien : le gabarit est tronqué.
  CHECK_FALSE(FormatTakesOneInt("obtenu %"));
}

TEST_CASE("Mots d'objets : un gabarit de bandeau vérolé ne donne aucun texte", "[vfs][mots]") {
  // L'agrégat se remplit à la main : ces deux fonctions ne lisent aucun fichier.
  ItemWords words;
  words.obtained_suffix = " x %d ramasses.";
  CHECK(ObtainedText(words, 12) == " x 12 ramasses.");

  words.obtained_suffix = " - %s obtained.";
  CHECK(ObtainedText(words, 12).empty());

  // Un champ vide n'est pas un gabarit : aucun libellé en dur ne le remplace.
  words.obtained_suffix.clear();
  CHECK(ObtainedText(words, 1).empty());
}

TEST_CASE("Mots d'objets : les six textes d'un ramassage ont chacun leur champ", "[vfs][mots]") {
  TempDirectory client;
  MakeClient(client);
  const ItemWords words = Read(client);
  CHECK(words.pickup.cannot_get == Tr("Vous ne pouvez pas ramasser cet item."));
  CHECK(words.pickup.too_heavy ==
        Tr("Vous ne pouvez plus porter d'items : vous êtes en surcharge."));
  CHECK(words.pickup.over_weight_limit ==
        Tr("Vous ne pouvez pas prendre cet item : la limite de poids serait dépassée."));
  CHECK(words.pickup.one_kind_limit ==
        Tr("Vous ne pouvez pas porter plus de 30 000 items d'un même type."));
  CHECK(words.pickup.same_item_limit == Tr("Vous avez dépassé la quantité maximale du même item."));
  CHECK(words.pickup.got_item == Tr("Vous avez obtenu %s (%d)."));
  // Le gabarit du gain a la forme qu'on sait imprimer.
  CHECK(FormatTakesStringThenInt(words.pickup.got_item));
}

TEST_CASE("Mots d'objets : le gabarit du gain veut une CHAÎNE PUIS un entier", "[vfs][mots]") {
  CHECK(FormatTakesStringThenInt("You got %s (%d)."));
  CHECK(FormatTakesStringThenInt("%s %d"));
  // « %% » est un pour-cent littéral : il n'attend aucun argument.
  CHECK(FormatTakesStringThenInt("100%% %s %d"));

  // 🔴 L'ORDRE INVERSE NE SE RÉORDONNE PAS, IL SE REFUSE : il passerait un
  // entier là où la libc attend un pointeur, qu'elle déréférencerait.
  CHECK_FALSE(FormatTakesStringThenInt("%d %s"));

  CHECK_FALSE(FormatTakesStringThenInt("%s"));
  CHECK_FALSE(FormatTakesStringThenInt("%d"));
  CHECK_FALSE(FormatTakesStringThenInt("%s %s %d"));
  CHECK_FALSE(FormatTakesStringThenInt("%s %d %d"));
  CHECK_FALSE(FormatTakesStringThenInt("%n"));
  CHECK_FALSE(FormatTakesStringThenInt("obtenu"));
  CHECK_FALSE(FormatTakesStringThenInt(""));
  // Un « % » en dernier caractère n'ouvre rien : le gabarit est tronqué.
  CHECK_FALSE(FormatTakesStringThenInt("You got %s (%d) %"));
}

TEST_CASE("Mots d'objets : la ligne du gain se compose, et un gabarit vérolé la vide",
          "[vfs][mots]") {
  // L'agrégat se remplit à la main : ces fonctions ne lisent aucun fichier.
  ItemWords words;
  words.pickup.got_item = "You got %s (%d).";
  CHECK(GotItemText(words, "Red Potion", 5) == "You got Red Potion (5).");

  words.pickup.got_item = "%d x %s ramasses";
  CHECK(GotItemText(words, "Red Potion", 5).empty());

  // Un champ vide n'est pas un gabarit : aucun libellé en dur ne le remplace.
  words.pickup.got_item.clear();
  CHECK(GotItemText(words, "Red Potion", 1).empty());

  // Le jet suit le même contrat.
  words.pickup.dropped_item = "You dropped %s (%d).";
  CHECK(DroppedItemText(words, "Red Potion", 5) == "You dropped Red Potion (5).");
  words.pickup.dropped_item = "%d x %s";
  CHECK(DroppedItemText(words, "Red Potion", 5).empty());
}

TEST_CASE("Mots d'objets : un nom d'objet n'est pas un gabarit", "[vfs][mots]") {
  // 🔴 LE CAS LE PLUS VICIEUX. Le nom vient lui aussi de l'installation, et il
  // est un ARGUMENT : ses « % » ne s'interprètent pas. Les relire comme un
  // gabarit ferait lire des arguments que personne n'a poussés.
  ItemWords words;
  words.pickup.got_item = "You got %s (%d).";
  CHECK(GotItemText(words, "50% Off Ticket", 2) == "You got 50% Off Ticket (2).");
  CHECK(GotItemText(words, "%s %d %n", 3) == "You got %s %d %n (3).");

  // Et le nom composé n'a pas de longueur bornée — quatre mots de cartes et le
  // nom d'un forgeron s'y ajoutent —, donc rien ne le tronque.
  const std::string long_name(400, 'A');
  CHECK(GotItemText(words, long_name, 7) == "You got " + long_name + " (7).");
}

TEST_CASE("Mots d'objets : un refus de ramassage dit son champ, ou reste vide", "[vfs][mots]") {
  // Un agrégat qu'aucun remplissage n'a posé ne ramène aucun libellé en dur.
  const ItemWords absent;
  CHECK(PickupRefusalText(absent, ItemPickupRefusal::kCannotGet).empty());
  CHECK(PickupRefusalText(absent, ItemPickupRefusal::kTooHeavy).empty());
  CHECK(PickupRefusalText(absent, ItemPickupRefusal::kOverWeightLimit).empty());
  CHECK(PickupRefusalText(absent, ItemPickupRefusal::kOneKindLimit).empty());
  CHECK(PickupRefusalText(absent, ItemPickupRefusal::kSameItemLimit).empty());
  ItemWords words;
  words.pickup.too_heavy = "Sac trop lourd.";
  CHECK(PickupRefusalText(words, ItemPickupRefusal::kTooHeavy) == "Sac trop lourd.");
  CHECK(PickupRefusalText(words, ItemPickupRefusal::kCannotGet).empty());
}

TEST_CASE("Mots d'objets : un sertissage dit sa réussite et son refus", "[vfs][mots][sertissage]") {
  const ItemWords absent;
  CHECK(CardInsertText(absent, true).empty());
  CHECK(CardInsertText(absent, false).empty());
  TempDirectory client;
  MakeClient(client);
  const ItemWords words = Read(client);
  CHECK(CardInsertText(words, true) == Tr("L'insertion de la carte a réussi."));
  CHECK(CardInsertText(words, false) == Tr("L'insertion de la carte a échoué."));
}

TEST_CASE("Mots d'objets : l'annonce de notre équipement, montré ou caché",
          "[vfs][mots][show_equip]") {
  const ItemWords absent;
  CHECK(EquipShownText(absent, true).empty());
  CHECK(EquipShownText(absent, false).empty());
  TempDirectory client;
  MakeClient(client);
  const ItemWords words = Read(client);
  CHECK(EquipShownText(words, true) == Tr("Informations d'équipement publiques."));
  CHECK(EquipShownText(words, false) == Tr("Informations d'équipement non publiques."));
}

TEST_CASE("Mots d'objets : la phrase des compétences de couple, ou rien", "[vfs][mots]") {
  TempDirectory client;
  MakeClient(client);
  CHECK(ro::vfs::LoveSkillText(Read(client)) == Tr("Je t'aime."));
  // Un agrégat qu'aucun remplissage n'a posé reste vide : aucun libellé en dur.
  CHECK(ro::vfs::LoveSkillText(ItemWords{}).empty());
}

TEST_CASE("Mots d'objets : une table de mots mal formée est refusée", "[vfs][mots]") {
  CHECK_FALSE(ro::vfs::ParseCardWords("version: 1\ncards: {sans identifiant: Lucky}\n"));
  CHECK_FALSE(ro::vfs::ParseCardWords("version: 1\ncards: [4001]\n"));
  std::unordered_map<std::uint32_t, CardWord> cards;
  CHECK_FALSE(ro::vfs::ParseCardWordSides("version: 1\ncards: {4001: x}\n", cards));
  CHECK_FALSE(ro::vfs::ParseCardWordSides("version: 2\ncards: []\n", cards));
}
