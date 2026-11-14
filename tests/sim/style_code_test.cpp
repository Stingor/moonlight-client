// LE CODE D'UN STYLE : la chaîne qu'un joueur copie, colle et partage.
//
// 🔴 ÉPROUVÉ SUR DES CHAÎNES LITTÉRALES, ET C'EST LE SEUL ESSAI QUI VAILLE.
// Un aller-retour encode/décode passerait avec n'importe quel format inventé ;
// ce qu'il faut tenir, c'est que nos octets soient EXACTEMENT ceux de l'autre
// client et du serveur, parce que les joueurs de ce serveur échangent leurs codes
// entre les deux clients et qu'une base de données en garde.

#include "ro/sim/style_code.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/style.h"

using ro::sim::BuildStyleTag;
using ro::sim::DecodeStyleCode;
using ro::sim::EncodeStyleCode;
using ro::sim::FindStyleTag;
using ro::sim::kStyleRampCount;
using ro::sim::ParseStyleTag;
using ro::sim::StyleAdjust;
using ro::sim::StyleCode;
using ro::sim::StyleRecipe;
using ro::sim::StyleTag;
using ro::sim::StyleTagLabel;
using ro::sim::StyleTagRead;

namespace {

// Quatre-vingts chiffres à zéro : huit rampes de cinq octets, sans réglage.
const std::string kNoAdjustments(kStyleRampCount* ro::sim::kStyleCodeRampBytes * 2, '0');

}  // namespace

TEST_CASE("Code de style : une recette vide s'écrit à l'octet près", "[sim][style]") {
  // Version sept, aucun corps, trois sentinelles, et rien d'autre.
  CHECK(EncodeStyleCode(StyleRecipe{}) == "7:00000000:-1:-1:-1:" + kNoAdjustments);
}

TEST_CASE("Code de style : les octets d'une rampe sont ceux du réseau", "[sim][style]") {
  // ⚠ PETIT-BOUTISTE SUR LA TEINTE, et le signe passe par le complément à
  // deux : 180 s'écrit `b400`, −50 s'écrit `ce`. Un encodage « lisible » de
  // ces deux-là — gros-boutiste, ou un signe en clair — donnerait un code que
  // l'autre client relirait de travers sans jamais s'en plaindre.
  StyleRecipe recipe;
  recipe.body_palette = 42;
  recipe.hair_palette = 7;
  recipe.hair_style = 12;
  recipe.ramps[0] = StyleAdjust{.hue = 180, .saturation = -50, .value = 25, .absolute = true};

  const std::string code = EncodeStyleCode(recipe, 0x1F87791DU);
  const std::string rest(7 * ro::sim::kStyleCodeRampBytes * 2, '0');
  CHECK(code == "7:1f87791d:42:7:12:b400ce1901" + rest);
}

TEST_CASE("Code de style : ce qui a été écrit se relit à l'identique", "[sim][style]") {
  StyleRecipe recipe;
  recipe.body_palette = 553;
  recipe.hair_palette = 251;
  recipe.hair_style = 80;
  for (std::size_t rank = 0; rank < kStyleRampCount; ++rank) {
    const auto step = static_cast<int>(rank);
    recipe.ramps[rank] = StyleAdjust{.hue = static_cast<std::int16_t>(-359 + 40 * step),
                                     .saturation = static_cast<std::int8_t>(-100 + 20 * step),
                                     .value = static_cast<std::int8_t>(100 - 10 * step),
                                     .absolute = (rank % 2) == 0};
  }
  constexpr std::uint32_t kBody = 0xDEADBEEFU;

  const std::optional<StyleCode> read = DecodeStyleCode(EncodeStyleCode(recipe, kBody));
  REQUIRE(read.has_value());
  CHECK(read->body_key == kBody);
  CHECK(read->recipe.body_palette == 553);
  CHECK(read->recipe.hair_palette == 251);
  CHECK(read->recipe.hair_style == 80);
  for (std::size_t rank = 0; rank < kStyleRampCount; ++rank) {
    CHECK(read->recipe.ramps[rank].hue == recipe.ramps[rank].hue);
    CHECK(read->recipe.ramps[rank].saturation == recipe.ramps[rank].saturation);
    CHECK(read->recipe.ramps[rank].value == recipe.ramps[rank].value);
    CHECK(read->recipe.ramps[rank].absolute == recipe.ramps[rank].absolute);
  }
}

TEST_CASE("Code de style : une autre version est REFUSÉE, jamais migrée", "[sim][style]") {
  // 🔴 LA VERSION SIX A CHANGÉ LE CLASSEMENT DES RAMPES : le rang trois d'hier
  // ne désigne plus la même pièce du costume. La relire repeindrait les bottes
  // en couleur de cape, avec l'aplomb d'un format valide.
  CHECK_FALSE(DecodeStyleCode("6:00000000:-1:-1:-1:" + kNoAdjustments).has_value());
  CHECK_FALSE(DecodeStyleCode("8:00000000:-1:-1:-1:" + kNoAdjustments).has_value());
  CHECK(DecodeStyleCode("7:00000000:-1:-1:-1:" + kNoAdjustments).has_value());
}

TEST_CASE("Code de style : un code abîmé ne rend rien", "[sim][style]") {
  // Une queue trop courte, une trop longue, un chiffre qui n'en est pas un,
  // un champ de corps de la mauvaise largeur, un champ manquant.
  CHECK_FALSE(DecodeStyleCode("7:00000000:-1:-1:-1:" + kNoAdjustments.substr(1)).has_value());
  CHECK_FALSE(DecodeStyleCode("7:00000000:-1:-1:-1:" + kNoAdjustments + "0").has_value());
  CHECK_FALSE(DecodeStyleCode("7:00000000:-1:-1:-1:z" + kNoAdjustments.substr(1)).has_value());
  CHECK_FALSE(DecodeStyleCode("7:0000000:-1:-1:-1:" + kNoAdjustments).has_value());
  CHECK_FALSE(DecodeStyleCode("7:00000000:-1:-1:" + kNoAdjustments).has_value());
  CHECK_FALSE(DecodeStyleCode("").has_value());
  CHECK_FALSE(DecodeStyleCode("   ").has_value());
  // Un champ qui n'est pas un nombre : la référence s'en tirerait avec un
  // zéro silencieux, ce qui poserait une teinte que personne n'a demandée.
  CHECK_FALSE(DecodeStyleCode("7:00000000:rouge:-1:-1:" + kNoAdjustments).has_value());
}

TEST_CASE("Code de style : les espaces autour se pardonnent", "[sim][style]") {
  // ⚠ UN CODE COLLÉ DEPUIS UN CHAT OU UN FORUM EN TRAÎNE PRESQUE TOUJOURS, et
  // refuser pour cela serait gratuitement pénible.
  const std::string code = "7:00000000:-1:-1:-1:" + kNoAdjustments;
  CHECK(DecodeStyleCode("  " + code + "\r\n").has_value());
  CHECK(DecodeStyleCode("\t" + code).has_value());
}

TEST_CASE("Code de style : les majuscules se lisent, mais ne s'écrivent pas", "[sim][style]") {
  // 🔴 DEUX CLIENTS QUI N'ÉCRIRAIENT PAS LA MÊME CASSE produiraient deux
  // chaînes pour un même style, et la comparaison « ce réglage a-t-il changé
  // depuis la validation ? » répondrait faux. On écrit donc en minuscules, et
  // l'on relit les deux.
  StyleRecipe recipe;
  recipe.ramps[0] = StyleAdjust{.hue = 180};
  const std::string code = EncodeStyleCode(recipe, 0xABCDEF01U);
  CHECK(code.find("ABCDEF01") == std::string::npos);
  CHECK(code.find("abcdef01") != std::string::npos);

  std::string shouted = code;
  for (char& letter : shouted) {
    if (letter >= 'a' && letter <= 'f') {
      letter = static_cast<char>(letter - 'a' + 'A');
    }
  }
  const std::optional<StyleCode> read = DecodeStyleCode(shouted);
  REQUIRE(read.has_value());
  CHECK(read->body_key == 0xABCDEF01U);
  CHECK(read->recipe.ramps[0].hue == 180);
}

// ── LA BALISE QUI PORTE UN CODE DANS UNE LIGNE DE CONVERSATION ──────────────

TEST_CASE("Balise de style : elle s'écrit à l'octet près", "[sim][style]") {
  // 🔴 À L'OCTET PRÈS, et pour la même raison que le code : les joueurs de ce
  // serveur échangent leurs styles entre les deux clients, et celui qui n'a
  // pas le greffon voit la balise BRUTE. L'étiquette vient donc en tête, pour
  // que le début de la ligne reste lisible.
  const std::string code = EncodeStyleCode(StyleRecipe{});
  CHECK(BuildStyleTag("Lune", code) == "<STYL>Lune:" + code + "</STYL>");
  // Un code vide ne fait pas de balise : il n'y aurait rien à transporter.
  CHECK(BuildStyleTag("Lune", "").empty());
}

TEST_CASE("Balise de style : deux-points et chevron deviennent un tiret", "[sim][style]") {
  // 🔴 ON REMPLACE AU LIEU DE REFUSER. L'étiquette peut être un nom de
  // préréglage, donc tapé par le joueur : « Rouge : v2 » est parfaitement
  // raisonnable, et refuser donnerait un geste qui ne produit rien et
  // n'explique rien. Les deux caractères découperaient la balise de travers.
  CHECK(StyleTagLabel("Rouge : v2") == "Rouge - v2");
  CHECK(StyleTagLabel("a<b") == "a-b");
  // Un pseudo ordinaire n'en porte aucun : pour lui, rien ne se voit.
  CHECK(StyleTagLabel("Lune") == "Lune");
}

TEST_CASE("Balise de style : elle se relit, étiquette et code séparés", "[sim][style]") {
  StyleRecipe recipe;
  recipe.hair_style = 12;
  const std::string code = EncodeStyleCode(recipe);
  const std::string line = "tiens " + BuildStyleTag("Lune", code) + " !";

  const std::size_t at = FindStyleTag(line, 0);
  REQUIRE(at == 6);
  StyleTag tag;
  std::size_t after = 0;
  REQUIRE(ParseStyleTag(line.substr(at), &tag, &after) == StyleTagRead::kOk);
  CHECK(tag.label == "Lune");
  // ⚠ LE CODE EST RENDU TEL QU'IL A VOYAGÉ, jamais ré-encodé : un aller-retour
  // perdrait ce qu'une version future y ajouterait.
  CHECK(tag.code == code);
  CHECK(tag.read.recipe.hair_style == 12);
  CHECK(line.substr(at + after) == " !");
}

TEST_CASE("Balise de style : le PREMIER deux-points sépare, et lui seul", "[sim][style]") {
  // 🔴 LE CODE EN PORTE CINQ — un après chacun des cinq champs. Chercher le
  // dernier découperait la balise au milieu de la recette, et l'étiquette
  // avalerait tout le code sauf sa queue hexadécimale — sans qu'aucune
  // longueur ne s'en plaigne.
  const std::string code = EncodeStyleCode(StyleRecipe{});
  REQUIRE(std::count(code.begin(), code.end(), ':') == 5);
  StyleTag tag;
  std::size_t after = 0;
  REQUIRE(ParseStyleTag(BuildStyleTag("Lune", code), &tag, &after) == StyleTagRead::kOk);
  CHECK(tag.label == "Lune");
  CHECK(tag.code == code);
}

TEST_CASE("Balise de style : un code illisible rend quand même l'étiquette", "[sim][style]") {
  // 🔴 C'EST CE QUI RESTE À MONTRER. Cent dix caractères d'hexadécimal
  // n'apprennent rien à personne ; le nom, si. Et le refus se décide ICI, à
  // l'analyse, jamais au clic : un lien qui échoue au moment où on l'ouvre est
  // une promesse trahie.
  StyleTag tag;
  std::size_t after = 0;
  const std::string line = "<STYL>Lune:6:00000000:-1:-1:-1:</STYL>";
  CHECK(ParseStyleTag(line, &tag, &after) == StyleTagRead::kUnreadable);
  CHECK(tag.label == "Lune");
  // ⚠ ET IL Y A QUAND MÊME QUELQUE CHOSE À SAUTER : sans cela l'appelant
  // repartirait en arrière sur la même balise, indéfiniment.
  CHECK(after == line.size());
}

TEST_CASE("Balise de style : une fermante absente ne fait rien sauter", "[sim][style]") {
  // 🔴 C'EST UN GEL QU'ON ÉVITE, pas un défaut d'affichage — même règle que
  // pour une balise d'objet tronquée par le serveur. Zéro dit « rien à
  // sauter », et l'appelant laisse le reste de la ligne tel quel.
  StyleTag tag;
  std::size_t after = 7;
  CHECK(ParseStyleTag("<STYL>Lune:7:0000", &tag, &after) == StyleTagRead::kNotATag);
  CHECK(after == 0);
  CHECK(ParseStyleTag("bonjour", &tag, &after) == StyleTagRead::kNotATag);
  CHECK(FindStyleTag("bonjour", 0) == std::string_view::npos);
}
