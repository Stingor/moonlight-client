#include "ro/render/item_icons.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/item_table.h"

using ro::render::CardIllustrationPath;
using ro::render::ItemCollectionPath;
using ro::render::ItemCollectionPathOfResource;
using ro::render::ItemIconPath;
using ro::vfs::CardIllustrations;
using ro::vfs::ItemEntry;
using ro::vfs::ItemTable;

namespace {

// « 빨간포션 » en CP949 : le nom de ressource de la potion rouge, tel que la
// table le donne et tel que l'archive l'indexe.
constexpr std::string_view kRedPotionCp949 = "\xBB\xA1\xB0\xA3\xC6\xF7\xBC\xC7";

// « 포링카드 » en CP949 : le nom du DESSIN de la carte Poring, celui que
// `card_illustrations.yml` donne — et non « 이름없는카드 », la ressource que
// l'objet 4001 porte comme toutes les autres cartes.
constexpr std::string_view kPoringCardCp949 = "\xC6\xF7\xB8\xB5\xC4\xAB\xB5\xE5";

}  // namespace

TEST_CASE("Icône d'objet : la ressource entre dans le chemin SANS retouche", "[render][icone]") {
  // 🔴 C'EST LA RÈGLE QUE CET ESSAI TIENT. La ressource sort de la table en
  // CP949 — c'est un nom de fichier d'archive. La repasser en UTF-8 pour que le
  // VFS la retranscrive serait deux conversions pour rien, et une occasion de
  // perdre un octet sur les dix-neuf noms que le fichier a mal fusionnés.
  ItemTable table;
  table.Put(501, ItemEntry{.resource = std::string(kRedPotionCp949)});

  const std::string path = ItemIconPath(table, 501);
  REQUIRE_FALSE(path.empty());
  CHECK(path.ends_with(std::string(kRedPotionCp949) + ".bmp"));
  // Le dossier vient devant, et il est lui aussi en octets d'archive : aucune
  // séquence UTF-8 ne doit s'y trouver.
  CHECK(path.starts_with("data\\texture\\"));
  CHECK(path.find("유저인터페이스") == std::string::npos);
}

TEST_CASE("Illustration de carte : le dossier change, et la CLÉ aussi", "[render][icone]") {
  // 🔴 CE N'EST PAS LA RESSOURCE DE L'OBJET. Les mille cinq cents cartes en
  // partagent une seule — le dos de carte générique de la case de sac —, et
  // composer le chemin sur elle montrerait le même dessin pour toutes. Seule la
  // table des illustrations nomme celui de chacune.
  CardIllustrations cards;
  cards.by_id[4001] = std::string(kPoringCardCp949);

  const std::string path = CardIllustrationPath(cards, 4001);
  REQUIRE_FALSE(path.empty());
  CHECK(path.ends_with(std::string(kPoringCardCp949) + ".bmp"));
  CHECK(path.starts_with("data\\texture\\"));
  // ⚠ SOUS LA RACINE D'INTERFACE, comme les icônes et les illustrations de
  // collection — et non sous `data\texture\cardbmp`, où l'installation ne range
  // qu'UN fichier.
  CHECK(path.find("\\cardbmp\\") != std::string::npos);
}

TEST_CASE("Illustration de carte : ce que la table ignore n'en a pas", "[render][icone]") {
  // C'est le test d'appartenance : un chemin vide veut dire « pas une carte », et
  // l'appelant se rabat sur l'illustration de collection.
  CardIllustrations cards;
  cards.by_id[4001] = std::string(kPoringCardCp949);

  CHECK(CardIllustrationPath(cards, 617).empty());
  CHECK(CardIllustrationPath(CardIllustrations{}, 4001).empty());
}

TEST_CASE("Icône d'objet : sans ressource, pas de chemin", "[render][icone]") {
  // Un chemin composé sur une ressource vide désignerait le DOSSIER, et la
  // lecture rendrait autre chose qu'une absence franche.
  ItemTable table;
  table.Put(1, ItemEntry{.name = "Objet sans image"});

  CHECK(ItemIconPath(table, 1).empty());
  CHECK(ItemIconPath(table, 12'345).empty());
}

TEST_CASE("Collection d'une ressource : le chemin de l'objet qui la porte", "[render][icone]") {
  // L'aperçu de la fiche éditée compose le chemin sur la ressource SAISIE : il
  // doit tomber sur le fichier que la table donnerait, une fois la fiche posée.
  ItemTable table;
  table.Put(501, ItemEntry{.resource = std::string(kRedPotionCp949)});

  const std::string path = ItemCollectionPathOfResource(kRedPotionCp949);
  REQUIRE_FALSE(path.empty());
  CHECK(path == ItemCollectionPath(table, 501));
  CHECK(path != ItemIconPath(table, 501));
  CHECK(path.ends_with(std::string(kRedPotionCp949) + ".bmp"));
  CHECK(ItemCollectionPathOfResource("").empty());
}
