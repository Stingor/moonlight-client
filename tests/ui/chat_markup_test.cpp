// Le balisage d'une ligne du fil : gras, italique, souligné, barré, spoiler,
// code, couleurs, icônes, emotes et adresses.
//
// Ce qui se vérifie ici, ce sont les RÈGLES DE PAIRE : le chat est plein
// d'astérisques isolées et de deux-points, et une règle trop gourmande
// passerait la moitié d'une ligne en italique, ou ferait d'une heure une
// emote. Et les liens reçus, qui doivent traverser le balisage sans perdre un
// octet ni leur jeton.
//
// ⚠ LES ESSAIS ÉTIQUETÉS `[web_address]` dépendent du repérage des adresses de
// `ro/core/web_address.h`.

#include "ro/ui/chat_markup.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_emote_names.h"

using ro::ui::ApplyChatMarkup;
using ro::ui::ChatInlineKind;
using ro::ui::ChatLinkKind;
using ro::ui::ChatLinkSpan;
using ro::ui::ChatMarkupResult;
using ro::ui::ChatStyleRun;
using ro::ui::kChatBold;
using ro::ui::kChatCode;
using ro::ui::kChatItalic;
using ro::ui::kChatSpoiler;
using ro::ui::kChatStrike;
using ro::ui::kChatUnderline;
using ro::ui::kNewChatLink;
using ro::ui::MayCarryChatMarkup;

namespace {

constexpr ImU32 kRed = IM_COL32(0xFF, 0x00, 0x00, 0xFF);

// Le balisage d'un texte sans lien reçu ; il doit y avoir quelque chose à faire.
[[nodiscard]] ChatMarkupResult Marked(std::string_view text,
                                      std::span<const ChatLinkSpan> links = {}) {
  ChatMarkupResult made;
  REQUIRE(ApplyChatMarkup(text, links, &made));
  return made;
}

// Le texte de la suite, lu dans le texte rendu.
[[nodiscard]] std::string_view RunText(const ChatMarkupResult& made, const ChatStyleRun& run) {
  return std::string_view(made.text).substr(run.offset, run.length);
}

[[nodiscard]] ChatLinkSpan ItemLink(std::uint32_t offset, std::uint32_t length) {
  ChatLinkSpan link;
  link.offset = offset;
  link.length = length;
  link.kind = ChatLinkKind::kItem;
  link.item_id = 1101;
  link.token = 7;
  return link;
}

}  // namespace

TEST_CASE("Balisage du fil : une astérisque isolée reste du texte", "[ui][chat][markup]") {
  // Rien à faire : la ligne n'est pas touchée, et `out` non plus.
  ChatMarkupResult made;
  made.text = "garde";
  CHECK_FALSE(ApplyChatMarkup("3*4", {}, &made));
  CHECK_FALSE(ApplyChatMarkup("*soupir", {}, &made));
  CHECK_FALSE(ApplyChatMarkup("* mot*", {}, &made));
  CHECK_FALSE(ApplyChatMarkup("*mot *", {}, &made));
  CHECK_FALSE(ApplyChatMarkup("a_b __c", {}, &made));
  CHECK(made.text == "garde");
}

TEST_CASE("Balisage du fil : gras et italique, aux bons octets", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("**gras** et *ital*");
  CHECK(made.text == "gras et ital");
  REQUIRE(made.runs.size() == 2);
  CHECK(made.runs[0].offset == 0);
  CHECK(made.runs[0].length == 4);
  CHECK(made.runs[0].style == kChatBold);
  CHECK(made.runs[1].offset == 8);
  CHECK(made.runs[1].length == 4);
  CHECK(made.runs[1].style == kChatItalic);
  CHECK(made.links.empty());
  CHECK(made.images.empty());
}

TEST_CASE("Balisage du fil : trois astérisques, la première reste du texte", "[ui][chat][markup]") {
  // La première ne s'ouvre pas — elle est suivie d'une astérisque —, la paire
  // suivante si.
  const ChatMarkupResult made = Marked("***a***");
  CHECK(made.text == "*a*");
  REQUIRE(made.runs.size() == 1);
  CHECK(RunText(made, made.runs[0]) == "a");
  CHECK(made.runs[0].style == kChatBold);
}

TEST_CASE("Balisage du fil : souligné, barré, spoiler et code", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("x __a__ ~~b~~ ||c|| `d*e*`");
  CHECK(made.text == "x a b c d*e*");
  REQUIRE(made.runs.size() == 4);
  CHECK(RunText(made, made.runs[0]) == "a");
  CHECK(made.runs[0].style == kChatUnderline);
  CHECK(RunText(made, made.runs[1]) == "b");
  CHECK(made.runs[1].style == kChatStrike);
  CHECK(RunText(made, made.runs[2]) == "c");
  CHECK(made.runs[2].style == kChatSpoiler);
  CHECK(made.runs[2].spoiler == 1);
  // Le code se recopie tel quel : ses astérisques ne basculent rien.
  CHECK(RunText(made, made.runs[3]) == "d*e*");
  CHECK(made.runs[3].style == kChatCode);
}

TEST_CASE("Balisage du fil : les spoilers se comptent à partir de un", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("x ||a|| et ||b||");
  REQUIRE(made.runs.size() == 2);
  CHECK(made.runs[0].spoiler == 1);
  CHECK(made.runs[1].spoiler == 2);
}

TEST_CASE("Balisage du fil : le code ne porte ni emote ni balisage", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("`:smile: **a**`");
  CHECK(made.text == ":smile: **a**");
  CHECK(made.images.empty());
  REQUIRE(made.runs.size() == 1);
  CHECK(made.runs[0].style == kChatCode);
  CHECK(made.runs[0].length == made.text.size());

  // Une paire vide n'est pas du code.
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("a `` b", {}, &untouched));
}

TEST_CASE("Balisage du fil : une couleur, et le retour à celle de la ligne", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("^FF0000rouge^000000 fin");
  CHECK(made.text == "rouge fin");
  REQUIRE(made.runs.size() == 1);
  CHECK(RunText(made, made.runs[0]) == "rouge");
  CHECK(made.runs[0].color == kRed);
  CHECK(made.runs[0].style == 0);

  // L'hexadécimal se lit sans casse ; un code incomplet reste du texte.
  CHECK(Marked("^ff0000x").runs[0].color == kRed);
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("^FF00 non", {}, &untouched));
}

TEST_CASE("Balisage du fil : l'icône d'un objet, sans texte", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("^i[501]Potion");
  CHECK(made.text == "Potion");
  REQUIRE(made.images.size() == 1);
  CHECK(made.images[0].kind == ChatInlineKind::kItemIcon);
  CHECK(made.images[0].item_id == 501);
  CHECK(made.images[0].offset == 0);
  CHECK(made.images[0].length == 0);

  // Un identifiant nul fait disparaître la marque, sans icône.
  const ChatMarkupResult empty = Marked("a^I[0]b");
  CHECK(empty.text == "ab");
  CHECK(empty.images.empty());
}

TEST_CASE("Balisage du fil : une emote Discord garde son repli et pointe le CDN",
          "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("<:pog:123> !");
  CHECK(made.text == ":pog: !");
  REQUIRE(made.images.size() == 1);
  CHECK(made.images[0].kind == ChatInlineKind::kPicture);
  CHECK(made.images[0].offset == 0);
  CHECK(made.images[0].length == 5);
  CHECK(made.images[0].picture_key == 0);
  REQUIRE(made.picture_urls.size() == 1);
  CHECK(made.picture_urls[0] == "https://cdn.discordapp.com/emojis/123.png");

  const ChatMarkupResult animated = Marked("x <a:dance:45> <:pog:6>");
  CHECK(animated.text == "x :dance: :pog:");
  REQUIRE(animated.images.size() == 2);
  CHECK(animated.images[1].picture_key == 1);
  REQUIRE(animated.picture_urls.size() == 2);
  CHECK(animated.picture_urls[0] == "https://cdn.discordapp.com/emojis/45.gif");
  CHECK(animated.picture_urls[1] == "https://cdn.discordapp.com/emojis/6.png");

  // L'identifiant doit être un nombre, et le nom ne traverse pas un chevron.
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("<:a:b> <:x> y :12>", {}, &untouched));
}

TEST_CASE("Balisage du fil : une seule emote du jeu par ligne", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked(":smile: et :smile:");
  CHECK(made.text == ":smile: et :smile:");
  REQUIRE(made.images.size() == 1);
  CHECK(made.images[0].kind == ChatInlineKind::kGameEmote);
  CHECK(made.images[0].offset == 0);
  CHECK(made.images[0].length == 7);
  CHECK(made.images[0].emote_action == *ro::ui::ChatEmoteAction("smile"));
}

TEST_CASE("Balisage du fil : une heure n'est pas une emote", "[ui][chat][markup]") {
  ChatMarkupResult made;
  CHECK_FALSE(ApplyChatMarkup("13:10", {}, &made));
  CHECK_FALSE(ApplyChatMarkup("lui : ok", {}, &made));
  CHECK_FALSE(ApplyChatMarkup(":inconnue: :Smile:", {}, &made));
}

TEST_CASE("Balisage du fil : un lien d'objet portant une astérisque n'est pas touché",
          "[ui][chat][markup]") {
  // « <Ep*ee> » est le nom composé d'un lien : son astérisque ne ferme rien.
  const std::vector<ChatLinkSpan> links = {ItemLink(6, 7)};
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("Bob : <Ep*ee> 3*4", links, &untouched));
  CHECK_FALSE(ApplyChatMarkup("*vends <Ep*ee>", std::vector{ItemLink(7, 7)}, &untouched));

  // Une bascule qui l'entoure passe par-dessus, et le lien se recopie entier.
  const ChatMarkupResult made = Marked("*vends <Ep*ee> vite*", std::vector{ItemLink(7, 7)});
  CHECK(made.text == "vends <Ep*ee> vite");
  REQUIRE(made.links.size() == 1);
  CHECK(made.links[0].offset == 6);
  CHECK(made.links[0].length == 7);
  CHECK(made.links[0].item_id == 1101);
  CHECK(made.links[0].token == 7);
  REQUIRE(made.link_origin.size() == 1);
  CHECK(made.link_origin[0] == 0);
  REQUIRE(made.runs.size() == 1);
  CHECK(made.runs[0].style == kChatItalic);
  CHECK(made.runs[0].length == made.text.size());
}

TEST_CASE("Balisage du fil : les liens reçus se remappent, dans l'ordre du texte",
          "[ui][chat][markup]") {
  // Donnés dans le désordre : le rang d'origine suit chacun.
  const std::vector<ChatLinkSpan> links = {ItemLink(17, 8), ItemLink(6, 7)};
  const ChatMarkupResult made = Marked("**x** <Sword> et <Shield>", links);
  CHECK(made.text == "x <Sword> et <Shield>");
  REQUIRE(made.links.size() == 2);
  CHECK(made.links[0].offset == 2);
  CHECK(made.links[1].offset == 13);
  CHECK(made.links[1].length == 8);
  REQUIRE(made.link_origin.size() == 2);
  CHECK(made.link_origin[0] == 1);
  CHECK(made.link_origin[1] == 0);
}

TEST_CASE("Balisage du fil : une borne de lien fausse ne déplace rien", "[ui][chat][markup]") {
  // Un lien qui déborde du texte est écarté, pas tronqué.
  const ChatMarkupResult made = Marked("**a** b", std::vector{ItemLink(6, 40)});
  CHECK(made.text == "a b");
  CHECK(made.links.empty());
}

TEST_CASE("Balisage du fil : une ligne ordinaire ne porte rien", "[ui][chat][markup]") {
  const std::string plain = "Bob : salut, ça va ? 13:10 a_b 3^2";
  CHECK_FALSE(MayCarryChatMarkup(plain));

  CHECK(MayCarryChatMarkup("**a**"));
  CHECK(MayCarryChatMarkup("^i[501]"));
  CHECK(MayCarryChatMarkup("ok :smile:"));
  CHECK(MayCarryChatMarkup("<a:x:1>"));
  CHECK_FALSE(MayCarryChatMarkup("<après>"));
}

TEST_CASE("Balisage du fil : une marque doublée est bordée de l'extérieur", "[ui][chat][markup]") {
  // Les identifiants et les pseudos portent des tirets bas et des barres : rien
  // de cela ne se met en forme.
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("my__var__name", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("__init__", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("__Ana__ : salut", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("||Zed|| : yo", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("a __b__c", {}, &untouched));

  const ChatMarkupResult made = Marked("a __b__ c");
  CHECK(made.text == "a b c");
  REQUIRE(made.runs.size() == 1);
  CHECK(RunText(made, made.runs[0]) == "b");
  CHECK(made.runs[0].style == kChatUnderline);

  // Une ponctuation borde aussi.
  const ChatMarkupResult framed = Marked("(~~x~~).");
  CHECK(framed.text == "(x).");
  REQUIRE(framed.runs.size() == 1);
  CHECK(framed.runs[0].style == kChatStrike);
}

TEST_CASE("Balisage du fil : une fermante dans du code ne ferme rien", "[ui][chat][markup]") {
  const ChatMarkupResult made = Marked("*a `b*` c");
  CHECK(made.text == "*a b* c");
  REQUIRE(made.runs.size() == 1);
  CHECK(RunText(made, made.runs[0]) == "b*");
  CHECK(made.runs[0].style == kChatCode);
}

TEST_CASE("Balisage du fil : une icône d'objet ne porte que des chiffres", "[ui][chat][markup]") {
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("^i[abc]", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("^i[12a] x", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("^i[] x", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("^i[ 12]", {}, &untouched));
  CHECK_FALSE(MayCarryChatMarkup("^i[abc]"));
}

TEST_CASE("Balisage du fil : un nom d'emote Discord ne porte que ce que Discord admet",
          "[ui][chat][markup]") {
  CHECK(Marked("<:Pog_2:1>").text == ":Pog_2:");
  ChatMarkupResult untouched;
  CHECK_FALSE(ApplyChatMarkup("<:a-b:1>", {}, &untouched));
  CHECK_FALSE(ApplyChatMarkup("<:caf\xC3\xA9:1>", {}, &untouched));
}

TEST_CASE("Balisage du fil : une adresse devient un lien neuf", "[ui][chat][markup][web_address]") {
  const ChatMarkupResult made = Marked("voir http://a.com ici");
  CHECK(made.text == "voir http://a.com ici");
  REQUIRE(made.links.size() == 1);
  CHECK(made.links[0].kind == ChatLinkKind::kUrl);
  CHECK(made.links[0].offset == 5);
  CHECK(made.links[0].length == 12);
  REQUIRE(made.link_origin.size() == 1);
  CHECK(made.link_origin[0] == kNewChatLink);
  CHECK(MayCarryChatMarkup("voir www.a.com"));
}

TEST_CASE("Balisage du fil : une adresse suit un lien reçu sans le recouvrir",
          "[ui][chat][markup][web_address]") {
  const ChatMarkupResult made =
      Marked("<Sword> https://x.fr/a et **b**", std::vector{ItemLink(0, 7)});
  CHECK(made.text == "<Sword> https://x.fr/a et b");
  REQUIRE(made.links.size() == 2);
  CHECK(made.links[0].kind == ChatLinkKind::kItem);
  CHECK(made.links[1].kind == ChatLinkKind::kUrl);
  CHECK(made.links[1].offset == 8);
  CHECK(made.links[1].length == 14);
  REQUIRE(made.link_origin.size() == 2);
  CHECK(made.link_origin[0] == 0);
  CHECK(made.link_origin[1] == kNewChatLink);
}

TEST_CASE("Balisage du fil : une adresse collée à une bascule n'est pas en début de mot",
          "[ui][chat][markup][web_address]") {
  // L'octet précédent se lit BRUT : « * » n'ouvre pas un mot.
  const ChatMarkupResult made = Marked("**http://a.com**");
  CHECK(made.text == "http://a.com");
  CHECK(made.links.empty());
  REQUIRE(made.runs.size() == 1);
  CHECK(made.runs[0].style == kChatBold);
}

TEST_CASE("Balisage du fil : une adresse peut commencer derrière une ouvrante doublée",
          "[ui][chat][markup][web_address]") {
  // `||lien||` est l'usage même du spoiler : l'adresse y commence, et s'arrête à
  // la fermante au lieu de l'avaler.
  const ChatMarkupResult made = Marked("voir ||http://a.com|| ici");
  CHECK(made.text == "voir http://a.com ici");
  REQUIRE(made.links.size() == 1);
  CHECK(made.links[0].kind == ChatLinkKind::kUrl);
  CHECK(made.links[0].offset == 5);
  CHECK(made.links[0].length == 12);
  REQUIRE(made.runs.size() == 1);
  CHECK(made.runs[0].style == kChatSpoiler);
  CHECK(RunText(made, made.runs[0]) == "http://a.com");
}
