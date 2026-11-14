#include "ro/vfs/item_link.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::BuildItemLink;
using ro::vfs::FindItemLink;
using ro::vfs::ItemLink;
using ro::vfs::ItemLinkCardSlots;
using ro::vfs::ItemLinkNameFacts;
using ro::vfs::ItemLinkOption;
using ro::vfs::ParseItemLink;

namespace {

// Un couteau ordinaire, sans rien dessus : le plus court lien qui existe.
[[nodiscard]] ItemLink PlainKnife() {
  ItemLink link;
  link.item_id = 1201;
  link.equipable = true;
  link.wearable_at = 2;  // EQP_HAND_R
  return link;
}

// Relit ce qu'on vient d'écrire. L'aller-retour est la seule preuve qui tienne
// les deux sens d'accord.
[[nodiscard]] ItemLink RoundTrip(const ItemLink& link, std::uint8_t card_slots) {
  const std::string text = BuildItemLink(link, card_slots);
  ItemLink back;
  std::size_t after = 0;
  REQUIRE(ParseItemLink(text, &back, &after));
  CHECK(after == text.size());
  return back;
}

}  // namespace

TEST_CASE("Lien d'objet : la tête est un emplacement sur CINQ et un drapeau", "[vfs][lien]") {
  // 🔴 CE CADRAGE EST LE FORMAT. Les zéros de tête ne sont pas un ornement : sans
  // eux, l'autre bout lit un autre nombre — et c'est le nom d'un autre objet qui
  // s'affiche chez tous les joueurs de la carte.
  const std::string text = BuildItemLink(PlainKnife(), 0);
  REQUIRE(text.starts_with("<ITEML>"));
  REQUIRE(text.ends_with("</ITEML>"));
  // « 2 » cadré sur cinq, puis « 1 » parce qu'un couteau se décore.
  CHECK(text.substr(7, 6) == "000021");
}

TEST_CASE("Lien d'objet : la balise entière, contre le témoin du SERVEUR", "[vfs][lien]") {
  // 🔴 CE TÉMOIN N'EST PAS DE MOI. Il est recomposé par l'algorithme de rAthena
  // — `base62_encode` puis `string_left_pad`, champ par champ, dans l'ordre de
  // `ItemDatabase::create_item_link` —, qui est une SECONDE implémentation du
  // même format. Une constante que j'aurais recopiée de ma propre sortie ne
  // prouverait rien : elle dirait seulement que le code fait ce qu'il fait.
  //
  // ⚠ DEUX ÉCARTS ASSUMÉS Y SONT VISIBLES, tranchés en faveur du CLIENT parce
  // que c'est son texte que les autres joueurs reçoivent : l'apparence est
  // écrite bien que le serveur la réserve aux équipables, et la charge cassée
  // `!01` n'est dans aucun des deux formats.
  ItemLink link;
  link.item_id = 1201;
  link.wearable_at = 2;
  link.equipable = true;
  link.refine_level = 7;
  link.view_id = 42;
  link.grade = 3;
  link.cards = {4'001, 0, 0, 0};
  link.options[0] = ItemLinkOption{.id = 2, .param = 1, .value = 15};
  link.option_count = 1;
  link.damaged = true;

  CHECK(BuildItemLink(link, 4) == "<ITEML>000021jn%07&0G'03)12x)00)00)00+02,01-0f!01</ITEML>");
}

TEST_CASE("Lien d'objet : le raffinement ne s'écrit que s'il y en a un", "[vfs][lien]") {
  // Le client saute le champ à zéro, et le serveur aussi. L'écrire quand même
  // serait quatre octets de bruit sur chaque lien de consommable.
  ItemLink link = PlainKnife();
  CHECK(BuildItemLink(link, 0).find('%') == std::string::npos);

  link.refine_level = 7;
  const std::string refined = BuildItemLink(link, 0);
  REQUIRE(refined.find("%07") != std::string::npos);
  CHECK(RoundTrip(link, 0).refine_level == 7);
}

TEST_CASE("Lien d'objet : l'apparence s'écrit MÊME sur ce qui ne se porte pas", "[vfs][lien]") {
  // 🔴 LE CLIENT ET LE SERVEUR NE SONT PAS D'ACCORD, et c'est le CLIENT qui
  // tranche : le serveur garde ce champ pour les objets équipables, le client
  // l'écrit toujours. C'est le texte du client que les autres joueurs reçoivent.
  ItemLink potion;
  potion.item_id = 501;
  potion.equipable = false;
  const std::string text = BuildItemLink(potion, 0);
  CHECK(text.find('&') != std::string::npos);
  // Rien à porter — cinq zéros —, et le drapeau à « 0 ».
  CHECK(text.substr(7, 6) == "000000");
}

TEST_CASE("Lien d'objet : les cartes partent par emplacements, quatre dès qu'il y en a une",
          "[vfs][lien]") {
  // 🔴 ET CE SECOND CAS N'EST PAS UNE COQUETTERIE : sur un objet FORGÉ, ces
  // quatre mots portent l'identité du forgeron. Les tronquer à zéro emplacement
  // effacerait le nom du créateur du lien.
  ItemLink link = PlainKnife();
  CHECK(ItemLinkCardSlots(link, 2) == 2);
  // Zéro emplacement connu vaut quatre : c'est ce que fait le client.
  CHECK(ItemLinkCardSlots(link, 0) == 4);
  link.cards[2] = 12'345;
  CHECK(ItemLinkCardSlots(link, 2) == 4);
  // Et jamais plus de quatre, quoi que la table raconte.
  CHECK(ItemLinkCardSlots(link, 9) == 4);
}

TEST_CASE("Lien d'objet : une option part en TROIS morceaux, et la valeur la clôt", "[vfs][lien]") {
  // ⚠ COMPTER SUR LA VALEUR, c'est ne jamais enregistrer une option à moitié lue.
  ItemLink link = PlainKnife();
  link.options[0] = ItemLinkOption{.id = 2, .param = 0, .value = 15};
  link.options[1] = ItemLinkOption{.id = 5, .param = 1, .value = 300};
  link.option_count = 2;

  const ItemLink back = RoundTrip(link, 0);
  REQUIRE(back.option_count == 2);
  CHECK(back.options[0].id == 2);
  CHECK(back.options[0].value == 15);
  CHECK(back.options[1].id == 5);
  CHECK(back.options[1].param == 1);
  CHECK(back.options[1].value == 300);
}

TEST_CASE("Lien d'objet : un identifiant d'option nul arrête la suite", "[vfs][lien]") {
  // Les deux bouts font pareil : le client n'affiche rien au-delà, donc écrire la
  // suite serait du bruit — et la relire donnerait une option fantôme.
  ItemLink link = PlainKnife();
  link.options[0] = ItemLinkOption{.id = 0, .param = 0, .value = 0};
  link.options[1] = ItemLinkOption{.id = 5, .param = 1, .value = 300};
  link.option_count = 2;

  CHECK(BuildItemLink(link, 0).find('+') == std::string::npos);
}

TEST_CASE("Lien d'objet : l'aller-retour rend tout ce qu'on y a mis", "[vfs][lien]") {
  ItemLink link;
  link.item_id = 1'299;
  link.wearable_at = 0x0002;
  link.equipable = true;
  link.refine_level = 10;
  link.view_id = 42;
  link.grade = 3;
  link.cards = {4'001, 4'002, 0, 0};
  link.damaged = true;

  const ItemLink back = RoundTrip(link, 4);
  CHECK(back.item_id == 1'299);
  CHECK(back.wearable_at == 0x0002);
  CHECK(back.equipable);
  CHECK(back.refine_level == 10);
  CHECK(back.view_id == 42);
  CHECK(back.grade == 3);
  CHECK(back.cards[0] == 4'001);
  CHECK(back.cards[1] == 4'002);
  CHECK(back.damaged);
}

TEST_CASE("Lien d'objet : l'objet cassé voyage derrière un séparateur que rien ne lit",
          "[vfs][lien]") {
  // 🔴 CE CHAMP N'EST PAS DANS LE FORMAT, et c'est pour cela qu'il est posé en
  // DERNIER, derrière le séparateur de rang zéro : un client qui l'ignore range
  // sa charge dans une case qu'il ne lit jamais, et tout le standard qui précède
  // lui reste intact.
  ItemLink link = PlainKnife();
  link.damaged = true;
  const std::string text = BuildItemLink(link, 0);
  const std::size_t mark = text.find('!');
  REQUIRE(mark != std::string::npos);
  // Il ne reste que la fermante derrière lui, et sa charge sur deux chiffres.
  CHECK(text.substr(mark) == "!01</ITEML>");
}

TEST_CASE("Lien d'objet : sans identifiant, rien du tout", "[vfs][lien]") {
  // 🔴 UNE OUVRANTE SANS SA FERMANTE FERAIT UNE LIGNE ILLISIBLE CHEZ TOUT LE
  // MONDE. Mieux vaut n'envoyer aucun lien qu'un lien tronqué.
  CHECK(BuildItemLink(ItemLink{}, 4).empty());

  ItemLink back;
  CHECK_FALSE(ParseItemLink("<ITEML>0000010</ITEML>", &back, nullptr));
  CHECK_FALSE(ParseItemLink("bonjour", &back, nullptr));
  CHECK_FALSE(ParseItemLink("<ITEML>", &back, nullptr));
}

TEST_CASE("Lien d'objet : une balise tronquée se lit sans sortir du texte", "[vfs][lien]") {
  // Le serveur coupe les lignes trop longues. Une balise sans fermante doit se
  // lire jusqu'au bout du texte, et pas au-delà.
  const std::string text = "<ITEML>0000218b%07";
  ItemLink back;
  std::size_t after = 0;
  REQUIRE(ParseItemLink(text, &back, &after));
  CHECK(after == text.size());
  CHECK(back.refine_level == 7);
}

TEST_CASE("Lien d'objet : un séparateur inconnu saute sa charge, sans décaler le reste",
          "[vfs][lien]") {
  // ⚠ C'EST CE QUI LAISSE UN CLIENT PLUS RÉCENT AJOUTER UN CHAMP. Sans cela, sa
  // charge serait relue comme un séparateur, et tout ce qui suit partirait de
  // travers — un grade deviendrait une carte.
  ItemLink link = PlainKnife();
  link.grade = 3;
  std::string text = BuildItemLink(link, 0);
  const std::size_t close = text.find("</ITEML>");
  text.insert(close, "/0z");

  ItemLink back;
  REQUIRE(ParseItemLink(text, &back, nullptr));
  CHECK(back.item_id == 1201);
  CHECK(back.grade == 3);
}

TEST_CASE("Lien d'objet : on le trouve au milieu d'une phrase", "[vfs][lien]") {
  const std::string text = "regarde " + BuildItemLink(PlainKnife(), 0) + " il est joli";
  const std::size_t at = FindItemLink(text, 0);
  REQUIRE(at != std::string::npos);
  CHECK(at == 8);

  ItemLink back;
  std::size_t after = 0;
  REQUIRE(ParseItemLink(std::string_view(text).substr(at), &back, &after));
  CHECK(back.item_id == 1201);
  // Ce qui suit la fermante, pour que l'appelant reprenne la phrase.
  CHECK(std::string_view(text).substr(at + after) == " il est joli");
  CHECK(FindItemLink(text, at + after) == std::string::npos);
}

TEST_CASE("Lien d'objet : le drapeau du sixième caractère décide de la décoration", "[vfs][lien]") {
  // 🔴 LA BALISE NE PORTE PAS LE TYPE, elle porte le PRÉDICAT. Composer le nom
  // demande pourtant un type : on en pose un dedans ou dehors, et la composition
  // prend exactement la même décision que le client.
  ItemLink link = PlainKnife();
  CHECK(ro::vfs::ItemTypeIsDecorated(ItemLinkNameFacts(link).type));
  link.equipable = false;
  CHECK_FALSE(ro::vfs::ItemTypeIsDecorated(ItemLinkNameFacts(link).type));
}

TEST_CASE("Lien d'objet : les faits du nom reprennent tout ce qui le décore", "[vfs][lien]") {
  ItemLink link = PlainKnife();
  link.refine_level = 7;
  link.grade = 2;
  link.cards = {4'001, 4'001, 0, 0};

  const ro::vfs::ItemNameFacts facts = ItemLinkNameFacts(link);
  CHECK(facts.item_id == 1201);
  CHECK(facts.refine_level == 7);
  CHECK(facts.grade == 2);
  CHECK(facts.cards[0] == 4'001);
  // ⚠ IDENTIFIÉ, TOUJOURS : un objet non identifié ne se lie pas, et composer
  // sous le nom de base perdrait tout ce que la balise transporte.
  CHECK(facts.identified);
  // ⚠ ET SANS NOM DE CRÉATEUR : la balise ne porte qu'un identifiant de
  // personnage, et le traduire demande une question au serveur.
  CHECK(facts.creator_name.empty());
}

TEST_CASE("Lien d'objet : un identifiant au-delà de 32 bits ne désigne personne", "[vfs][lien]") {
  // 🔴 REPLIÉ, IL DÉSIGNAIT UN AUTRE OBJET : une balise forgée dans le chat en
  // montrait un que personne n'avait lié. Sept chiffres base62 dépassent 32 bits.
  const std::string forged = "<ITEML>000021zzzzzzz</ITEML>";
  ItemLink back;
  std::size_t after = 0;
  if (ParseItemLink(forged, &back, &after)) {
    CHECK(back.item_id == 0);
  }
}
