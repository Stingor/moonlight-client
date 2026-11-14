#include "ro/vfs/item_name.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

using ro::vfs::ComposeItemName;
using ro::vfs::ItemEntry;
using ro::vfs::ItemNameFacts;
using ro::vfs::ItemNameRole;
using ro::vfs::ItemNameSlots;
using ro::vfs::ItemNameText;
using ro::vfs::ItemTable;
using ro::vfs::ItemWords;

namespace {

// Les types que le serveur emploie, nommés là où ils servent.
constexpr std::int32_t kTypeHealing = 0;  // ne se porte pas
constexpr std::int32_t kTypeWeapon = 4;
constexpr std::int32_t kTypeArmor = 5;

constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kRedPotionId = 501;

// Les mots anglais du client d'origine, espaces compris : un essai de composition
// ne dépend pas de la langue de l'interface.
[[nodiscard]] ItemWords MakeWords() {
  ItemWords words;
  words.repeated[2] = "Double";
  words.repeated[3] = "Triple";
  words.repeated[4] = "Quadruple";
  words.possessive[0] = "'s ";
  words.possessive[1] = "'s Ice ";
  words.possessive[2] = "'s Earth ";
  words.possessive[3] = "'s Fire ";
  words.possessive[4] = "'s Wind ";
  words.wedding_possessive = "With Mr/Miss";
  words.forge_strength[5] = "Very Strong ";
  words.forge_strength[10] = "Very Very Strong ";
  words.forge_strength[15] = "Very Very Very Strong ";
  words.nameless_creator = "Nameless";
  words.beloved_pet = "Beloved ";
  words.cards[4001] = {.text = "Lucky", .after_name = false};
  words.cards[4002] = {.text = "Vital", .after_name = false};
  words.cards[4014] = {.text = "of Champion", .after_name = true};
  // Une carte CONNUE mais MUETTE : elle décide que l'objet suit la voie des
  // cartes, sans écrire elle-même un seul mot.
  words.cards[4099] = {.text = "", .after_name = false};
  return words;
}

[[nodiscard]] ItemTable MakeItems(std::uint8_t sword_slots = 0) {
  ItemTable items;
  items.Put(kSwordId, ItemEntry{.name = "Sword", .card_slots = sword_slots});
  items.Put(kRedPotionId, ItemEntry{.name = "Red Potion"});
  return items;
}

[[nodiscard]] std::string Compose(const ItemNameFacts& facts, const ItemTable& items,
                                  const ItemWords& words,
                                  ItemNameSlots slots = ItemNameSlots::kShown) {
  const auto segments = ComposeItemName(facts, items, words, slots);
  return ItemNameText(segments);
}

}  // namespace

TEST_CASE("Nom d'objet : nu, c'est celui de la table", "[vfs][nom]") {
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kRedPotionId, .type = kTypeHealing}, items, words) == "Red Potion");
}

TEST_CASE("Nom d'objet : un objet que la table ignore montre son numéro", "[vfs][nom]") {
  // Le client écrit une espace — un nom vide, que rien ne rattrape. Nous
  // montrons le numéro : c'est la seule chose qui permette encore d'agir.
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = 65'000, .type = kTypeHealing}, items, words) == "#65000");
}

TEST_CASE("Nom d'objet : raffinement et grade se posent devant", "[vfs][nom]") {
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .refine_level = 7}, items, words) ==
        "+7 Sword");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .grade = 4}, items, words) ==
        "[A] Sword");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .grade = 1}, items, words) ==
        "[D] Sword");
}

TEST_CASE("Nom d'objet : un grade hors barème écrit des crochets vides", "[vfs][nom]") {
  // 🔴 C'EST LE DÉFAUT DU CLIENT, ET ON LE REPRODUIT (ADR 0030). Son tampon de
  // deux octets reste à zéro et le « %s » rend une chaîne vide. Corriger ici
  // donnerait un affichage que personne d'autre ne produit, donc introuvable
  // pour qui compare les deux clients.
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .grade = 5}, items, words) ==
        "[] Sword");
}

TEST_CASE("Nom d'objet : les emplacements viennent de la TABLE, pas du fil", "[vfs][nom]") {
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon}, MakeItems(2), words) == "Sword [2]");
  // Zéro emplacement n'écrit pas « [0] » : le client saute le format entier.
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon}, MakeItems(0), words) == "Sword");
}

TEST_CASE("Nom d'objet : le bandeau d'obtention TAIT les emplacements", "[vfs][nom]") {
  // ⚠ C'EST LE CLIENT QUI LES TAIT : son name-builder ne compose « [N] » que si
  // son dernier argument vaut 1, et le chemin du bandeau passe 0. Sans cela le
  // bandeau dirait « Sword [3] - 1 obtained. » là où le client dit « Sword - 1
  // obtained. ».
  const ItemWords words = MakeWords();
  const ItemTable items = MakeItems(3);
  const ItemNameFacts sword{.item_id = kSwordId, .type = kTypeWeapon, .refine_level = 7};
  CHECK(Compose(sword, items, words) == "+7 Sword [3]");
  CHECK(Compose(sword, items, words, ItemNameSlots::kHidden) == "+7 Sword");

  // La voie non décorée compte ses emplacements à un AUTRE endroit : elle se
  // tait aussi, sinon la moitié des objets ramassés garderait son suffixe.
  ItemTable pouches;
  pouches.Put(kRedPotionId, ItemEntry{.name = "Red Potion", .card_slots = 1});
  const ItemNameFacts potion{.item_id = kRedPotionId, .type = kTypeHealing};
  CHECK(Compose(potion, pouches, words) == "Red Potion [1]");
  CHECK(Compose(potion, pouches, words, ItemNameSlots::kHidden) == "Red Potion");

  // Les sorties anticipées n'en composaient déjà aucun : les taire n'y change
  // rien.
  const ItemNameFacts forged{.item_id = kSwordId,
                             .type = kTypeWeapon,
                             .cards = {0x00FF, 0x0A03, 0, 0},
                             .creator_name = "Bob"};
  CHECK(Compose(forged, items, words, ItemNameSlots::kHidden) == Compose(forged, items, words));
}

TEST_CASE("Nom d'objet : un mot de carte va devant ou derrière selon la LISTE", "[vfs][nom]") {
  // 🔴 LA RÈGLE QUE CET ESSAI TIENT. `card_postfixes.yml` ne porte aucun
  // mot : il NOMME les cartes dont le mot déménage derrière le nom. Lu
  // naïvement, cette table semble vide et 509 mots se poseraient du mauvais côté.
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4001, 0, 0, 0}}, items,
                words) == "Lucky Sword");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4014, 0, 0, 0}}, items,
                words) == "Sword of Champion");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4001, 4014, 0, 0}}, items,
                words) == "Lucky Sword of Champion");
}

TEST_CASE("Nom d'objet : des cartes identiques se comptent et se consomment", "[vfs][nom]") {
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4001, 4001, 0, 0}}, items,
                words) == "Double Lucky Sword");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4001, 4001, 4001, 0}}, items,
                words) == "Triple Lucky Sword");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4001, 4001, 4001, 4001}},
                items, words) == "Quadruple Lucky Sword");
  // Deux cartes DIFFÉRENTES ne se multiplient pas : deux mots, chacun une fois.
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4001, 4002, 0, 0}}, items,
                words) == "Lucky Vital Sword");
  // Le mot de multiplicité se pose APRÈS le mot quand celui-ci est derrière.
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4014, 4014, 0, 0}}, items,
                words) == "Sword of Champion Double");
}

TEST_CASE("Nom d'objet : une carte connue mais muette n'écrit rien", "[vfs][nom]") {
  // Elle décide quand même que l'objet suit la voie des cartes — et c'est ce qui
  // évite qu'un objet portant une carte muette ET une carte parlante perde les
  // deux.
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4099, 0, 0, 0}}, items,
                words) == "Sword");
  CHECK(Compose({.item_id = kSwordId, .type = kTypeWeapon, .cards = {4099, 4001, 0, 0}}, items,
                words) == "Lucky Sword");
}

TEST_CASE("Nom d'objet : une arme forgée nomme son forgeron", "[vfs][nom]") {
  const ItemTable items = MakeItems(2);
  const ItemWords words = MakeWords();
  // card[0] = marque de forge ; card[1] = étoiles en octet haut, élément en bas.
  const ItemNameFacts forged{.item_id = kSwordId,
                             .type = kTypeWeapon,
                             .cards = {0x00FF, 0x0A03, 0, 0},
                             .creator_name = "Bob"};
  // ⚠ PAS DE « [2] » : le client sort de sa fonction avant d'y arriver, alors
  // même que la table déclare deux emplacements.
  CHECK(Compose(forged, items, words) == "Very Very Strong Bob's Fire Sword");
}

TEST_CASE("Nom d'objet : sans étoiles ni élément, il ne reste que le possessif", "[vfs][nom]") {
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  const ItemNameFacts created{
      .item_id = kSwordId, .type = kTypeWeapon, .cards = {0x00FE, 0, 0, 0}, .creator_name = "Bob"};
  CHECK(Compose(created, items, words) == "Bob's Sword");
}

TEST_CASE("Nom d'objet : un forgeron inconnu se dit, et se distingue", "[vfs][nom]") {
  // Le fil ne porte qu'un identifiant de personnage ; tant que le serveur n'a
  // pas répondu, le nom manque. Le rôle du morceau le dit, pour que la couleur
  // puisse le dire à son tour.
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  const auto segments = ComposeItemName(
      {.item_id = kSwordId, .type = kTypeWeapon, .cards = {0x00FE, 0, 0, 0}}, items, words);
  CHECK(ItemNameText(segments) == "Nameless's Sword");
  bool saw_unknown = false;
  for (const auto& segment : segments) {
    if (segment.role == ItemNameRole::kUnknownCreator) {
      saw_unknown = true;
      CHECK(segment.text == "Nameless");
    }
  }
  CHECK(saw_unknown);
}

TEST_CASE("Nom d'objet : un objet non identifié n'a QUE son nom", "[vfs][nom]") {
  const ItemTable items = MakeItems(2);
  const ItemWords words = MakeWords();
  const ItemNameFacts hidden{.item_id = kSwordId,
                             .type = kTypeWeapon,
                             .refine_level = 7,
                             .cards = {4001, 0, 0, 0},
                             .identified = false};
  CHECK(Compose(hidden, items, words) == "Sword");
}

TEST_CASE("Nom d'objet : ce qui ne se porte pas ne se décore pas", "[vfs][nom]") {
  // Une potion ne prend ni raffinement ni mot de carte, même si on lui en passe.
  const ItemTable items = MakeItems();
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = kRedPotionId,
                 .type = kTypeHealing,
                 .refine_level = 7,
                 .cards = {4001, 0, 0, 0}},
                items, words) == "Red Potion");
}

TEST_CASE("Nom d'objet : une protection se décore comme une arme", "[vfs][nom]") {
  ItemTable items;
  items.Put(2301, ItemEntry{.name = "Cotton Shirt", .card_slots = 1});
  const ItemWords words = MakeWords();
  CHECK(Compose({.item_id = 2301, .type = kTypeArmor, .refine_level = 4, .cards = {4001, 0, 0, 0}},
                items, words) == "+4 Lucky Cotton Shirt [1]");
}

TEST_CASE("Nom d'objet : tout ensemble, dans l'ordre du client", "[vfs][nom]") {
  const ItemTable items = MakeItems(2);
  const ItemWords words = MakeWords();
  const ItemNameFacts full{.item_id = kSwordId,
                           .type = kTypeWeapon,
                           .refine_level = 7,
                           .grade = 2,
                           .cards = {4001, 4001, 0, 0}};
  CHECK(Compose(full, items, words) == "+7 [C] Double Lucky Sword [2]");
}

TEST_CASE("Nom d'objet : un œuf de familier renommé se préfixe", "[vfs][nom]") {
  ItemTable items;
  items.Put(9001, ItemEntry{.name = "Poring Egg"});
  const ItemWords words = MakeWords();
  // La quatrième carte à 1 est la marque du renommage ; hors de la plage des
  // familiers, elle ne veut rien dire.
  CHECK(Compose({.item_id = 9001, .type = kTypeWeapon, .cards = {0, 0, 0, 1}}, items, words) ==
        "Beloved Poring Egg");
  items.Put(1101, ItemEntry{.name = "Sword"});
  CHECK(Compose({.item_id = 1101, .type = kTypeWeapon, .cards = {0, 0, 0, 1}}, items, words) ==
        "Sword");
}

TEST_CASE("Nom d'objet : les anneaux de mariage remplacent le possessif", "[vfs][nom]") {
  ItemTable items;
  items.Put(2634, ItemEntry{.name = "Wedding Ring"});
  const ItemWords words = MakeWords();
  const ItemNameFacts ring{
      .item_id = 2634, .type = 5, .cards = {0x00FE, 0, 0, 0}, .creator_name = "Bob"};
  // ⚠ ET IL SE SOUDE AU NOM, parce que ce mot-là est le SEUL des six à ne pas
  // porter d'espace final : « 's Fire » en a un, « With Mr/Miss » non. Le client
  // d'origine affiche donc exactement cette bouillie, et nous la reproduisons
  // (ADR 0030) — corriger ici donnerait un texte que personne d'autre ne montre.
  CHECK(Compose(ring, items, words) == "BobWith Mr/MissWedding Ring");
}

TEST_CASE("Nom d'objet : les morceaux se fondent sauf celui du forgeron", "[vfs][nom]") {
  // Deux morceaux ordinaires de suite se dessinent de la même couleur : les
  // garder séparés ne dirait rien et ferait deux allocations.
  const ItemTable items = MakeItems(2);
  const ItemWords words = MakeWords();
  const auto plain = ComposeItemName(
      {.item_id = kSwordId, .type = kTypeWeapon, .refine_level = 7, .cards = {4001, 0, 0, 0}},
      items, words);
  CHECK(plain.size() == 1);

  const auto forged = ComposeItemName({.item_id = kSwordId,
                                       .type = kTypeWeapon,
                                       .cards = {0x00FF, 0x0503, 0, 0},
                                       .creator_name = "Bob"},
                                      items, words);
  REQUIRE(forged.size() == 3);
  CHECK(forged[0].text == "Very Strong ");
  CHECK(forged[0].role == ItemNameRole::kOrdinary);
  CHECK(forged[1].text == "Bob");
  CHECK(forged[1].role == ItemNameRole::kCreator);
  CHECK(forged[2].text == "'s Fire Sword");
}
