// La décoration riche d'une ligne du fil : balises, auteur relayé de Discord,
// balisage, adresses, icônes d'objet et vignettes.

#include "chat/chat_rich_decor.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/web_preview_policy.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/chat_emote_names.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/chat_markup.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

using moonclient::ChatBodyOffset;
using moonclient::ChatDecorTables;
using moonclient::ChatRichContext;
using moonclient::DecorateBubbleLine;
using moonclient::DecoratedChatLine;
using moonclient::DecorateRichChatLine;
using moonclient::ItemIconLookup;
using moonclient::MayNeedRichDecor;
using ro::ui::ChatInlineImage;
using ro::ui::ChatInlineKind;
using ro::ui::ChatLinkKind;
using ro::ui::ChatStyleRun;

namespace {

constexpr std::uint32_t kKnifeId = 1201;
// « ^FF0000 », lu par le balisage.
constexpr ImU32 kPureRed = IM_COL32(0xFF, 0x00, 0x00, 0xFF);

[[nodiscard]] ro::vfs::ItemTable SmallTable() {
  ro::vfs::ItemTable table;
  table.Put(kKnifeId, ro::vfs::ItemEntry{.name = "Knife", .card_slots = 3});
  return table;
}

[[nodiscard]] ro::vfs::ItemLink KnifeLink() {
  ro::vfs::ItemLink link;
  link.item_id = kKnifeId;
  link.equipable = true;
  link.wearable_at = 2;
  return link;
}

// Le texte d'un morceau de la ligne décorée.
[[nodiscard]] std::string_view Piece(const DecoratedChatLine& line, std::uint32_t offset,
                                     std::uint32_t length) {
  return std::string_view(line.text).substr(offset, length);
}

[[nodiscard]] std::size_t CountOf(const DecoratedChatLine& line, ChatInlineKind kind) {
  return static_cast<std::size_t>(std::ranges::count(line.images, kind, &ChatInlineImage::kind));
}

}  // namespace

TEST_CASE("Décoration riche : une ligne ordinaire ne coûte rien", "[app][chat][riche]") {
  // 🔴 LE FIL POSE LA QUESTION À CHAQUE LIGNE, À CHAQUE IMAGE : une ligne sans
  // balise ni balisage garde sa vue sur le texte du monde.
  CHECK_FALSE(MayNeedRichDecor("bonjour tout le monde", false));
  DecoratedChatLine out;
  out.text = "intact";
  CHECK_FALSE(DecorateRichChatLine("bonjour tout le monde", false, {}, {}, &out));
  CHECK(out.text == "intact");
  // Un « < » sans balise, sans les tables, ne décore rien non plus.
  CHECK_FALSE(DecorateRichChatLine("je t'aime <3", false, {}, {}, &out));
  CHECK(out.text == "intact");
}

TEST_CASE("Décoration riche : une adresse devient un lien qui porte ce qui est écrit",
          "[app][chat][riche]") {
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine("regarde https://example.com/a ici", false, {}, {}, &out));
  CHECK(out.text == "regarde https://example.com/a ici");
  REQUIRE(out.links.size() == 1);
  REQUIRE(out.carried.size() == 1);
  CHECK(out.links[0].kind == ChatLinkKind::kUrl);
  CHECK(Piece(out, out.links[0].offset, out.links[0].length) == "https://example.com/a");
  CHECK(out.carried[0].kind == ChatLinkKind::kUrl);
  CHECK(out.carried[0].url == "https://example.com/a");
  // ⚠ UNE ADRESSE QUELCONQUE N'EST PAS UNE VIGNETTE, même images permises.
  ChatRichContext pictures;
  pictures.pictures = true;
  REQUIRE(DecorateRichChatLine("regarde https://example.com/a.png", false, {}, pictures, &out));
  CHECK(out.images.empty());
}

TEST_CASE("Décoration riche : le balisage éteint ne retire rien", "[app][chat][riche]") {
  ChatRichContext plain;
  plain.markup = false;
  DecoratedChatLine out;
  CHECK_FALSE(DecorateRichChatLine("un **gras** et https://example.com", false, {}, plain, &out));
}

TEST_CASE("Décoration riche : l'auteur relayé de Discord se clique", "[app][chat][riche]") {
  const std::string_view raw = "[#gonryun][Ana] salut **toi**";
  CHECK(MayNeedRichDecor("[#gonryun][Ana] salut", true));
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine(raw, true, {}, {}, &out));
  CHECK(out.text == "[#gonryun][Ana] salut toi");
  REQUIRE_FALSE(out.links.empty());
  CHECK(out.links[0].kind == ChatLinkKind::kDiscordName);
  CHECK(Piece(out, out.links[0].offset, out.links[0].length) == "Ana");
  CHECK(out.carried[0].kind == ChatLinkKind::kDiscordName);
  CHECK(out.carried[0].discord_name == "Ana");
  // Ce que la ligne dit commence après le pseudo et son crochet.
  CHECK(out.text.substr(out.body_offset) == "salut toi");
  REQUIRE(out.styles.size() == 1);
  CHECK(Piece(out, out.styles[0].offset, out.styles[0].length) == "toi");

  // Sans balisage ni balise, l'auteur suffit à décorer.
  REQUIRE(DecorateRichChatLine("[#gonryun][Bob] coucou", true, {}, {}, &out));
  REQUIRE(out.links.size() == 1);
  CHECK(out.carried[0].discord_name == "Bob");
  CHECK(out.text.substr(out.body_offset) == "coucou");

  // ⚠ LA MÊME FORME HORS DU RELAI N'EST PAS UN AUTEUR : un joueur peut taper
  // des crochets.
  CHECK_FALSE(DecorateRichChatLine("[#gonryun][Bob] coucou", false, {}, {}, &out));
}

TEST_CASE("Décoration riche : une emote du jeu suit sa case", "[app][chat][riche]") {
  const std::string raw = "bonjour :" + std::string(ro::ui::ChatEmoteName(1)) + ":";
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine(raw, false, {}, {}, &out));
  CHECK(CountOf(out, ChatInlineKind::kGameEmote) == 1);
  // Le repli reste dans le texte : éteinte, l'emote se lit encore.
  CHECK(out.text == raw);
  ChatRichContext without;
  without.emote_images = false;
  REQUIRE(DecorateRichChatLine(raw, false, {}, without, &out));
  CHECK(out.images.empty());
  CHECK(out.text == raw);
}

TEST_CASE("Décoration riche : une icône devant chaque objet lié", "[app][chat][riche]") {
  const ro::vfs::ItemTable items = SmallTable();
  const ro::vfs::ItemWords words;
  const ChatDecorTables tables{.items = &items, .words = &words};
  const std::string raw = "vends " + ro::vfs::BuildItemLink(KnifeLink(), 3) + " **vite**";
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine(raw, false, tables, {}, &out));
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ChatLinkKind::kItem);
  CHECK(out.carried[0].item.item_id == kKnifeId);
  REQUIRE(CountOf(out, ChatInlineKind::kItemIcon) == 1);
  const ChatInlineImage& icon = out.images[0];
  // ⚠ DEVANT LE CHEVRON, sans rien remplacer : le nom reste le lien.
  CHECK(icon.length == 0);
  CHECK(icon.offset + 1 == out.links[0].offset);
  CHECK(out.text[icon.offset] == '<');
  CHECK(icon.item_id == kKnifeId);
  // Le balisage s'est appliqué autour du lien.
  CHECK(out.text.ends_with(" vite"));

  ChatRichContext without;
  without.item_icons = false;
  REQUIRE(DecorateRichChatLine(raw, false, tables, without, &out));
  CHECK(out.images.empty());
}

TEST_CASE("Décoration riche : une image du miroir devient sa vignette", "[app][chat][riche]") {
  const std::string address = std::string(ro::core::kMirrorPicturePrefix) + "abc.png";
  const std::string raw = "regarde " + address;
  ChatRichContext pictures;
  pictures.pictures = true;
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine(raw, false, {}, pictures, &out));
  REQUIRE(out.images.size() == 1);
  const ChatInlineImage& picture = out.images[0];
  CHECK(picture.kind == ChatInlineKind::kPicture);
  // Elle recouvre l'adresse, qui reste le lien qu'on clique.
  REQUIRE(out.links.size() == 1);
  CHECK(picture.offset == out.links[0].offset);
  CHECK(picture.length == out.links[0].length);
  REQUIRE(out.picture_urls.size() == 1);
  CHECK(picture.picture_key == 0);
  CHECK(out.picture_urls[0] == address);

  // Vignettes éteintes : l'adresse seule.
  REQUIRE(DecorateRichChatLine(raw, false, {}, {}, &out));
  CHECK(out.images.empty());
  CHECK(out.picture_urls.empty());
  CHECK(out.links.size() == 1);
}

TEST_CASE("Décoration riche : une emote de Discord ne se télécharge qu'images permises",
          "[app][chat][riche]") {
  const std::string_view raw = "[#gonryun][Ana] <:pepe:123>";
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine(raw, true, {}, {}, &out));
  CHECK(out.text == "[#gonryun][Ana] :pepe:");
  CHECK(out.images.empty());
  CHECK(out.picture_urls.empty());

  ChatRichContext pictures;
  pictures.pictures = true;
  REQUIRE(DecorateRichChatLine(raw, true, {}, pictures, &out));
  REQUIRE(out.images.size() == 1);
  CHECK(out.images[0].kind == ChatInlineKind::kPicture);
  REQUIRE(out.picture_urls.size() == 1);
  CHECK(out.picture_urls[0] == "https://cdn.discordapp.com/emojis/123.png");
}

TEST_CASE("Décoration riche : les images vont dans l'ordre du texte", "[app][chat][riche]") {
  // 🔴 LE FIL LES LIT DANS L'ORDRE : une image dont l'origine est dépassée ne
  // se pose pas. L'icône du lien, ajoutée après le balisage, se range donc
  // entre l'icône tapée et l'emote.
  const ro::vfs::ItemTable items = SmallTable();
  const ro::vfs::ItemWords words;
  const ChatDecorTables tables{.items = &items, .words = &words};
  const std::string raw = "^i[501] et " + ro::vfs::BuildItemLink(KnifeLink(), 3) + " :" +
                          std::string(ro::ui::ChatEmoteName(1)) + ":";
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine(raw, false, tables, {}, &out));
  REQUIRE(out.images.size() == 3);
  CHECK(out.images[0].kind == ChatInlineKind::kItemIcon);
  CHECK(out.images[0].item_id == 501);
  CHECK(out.images[1].kind == ChatInlineKind::kItemIcon);
  CHECK(out.images[1].item_id == kKnifeId);
  CHECK(out.images[2].kind == ChatInlineKind::kGameEmote);
  CHECK(std::ranges::is_sorted(out.images, {}, &ChatInlineImage::offset));
}

TEST_CASE("Décoration riche : la phrase d'un chuchotement se décale derrière son en-tête",
          "[app][chat][riche]") {
  const std::string address = std::string(ro::core::kMirrorPicturePrefix) + "a.png";
  ChatRichContext pictures;
  pictures.pictures = true;
  DecoratedChatLine body;
  REQUIRE(DecorateRichChatLine("**vu** " + address, false, {}, pictures, &body));
  REQUIRE(body.styles.size() == 1);
  REQUIRE(body.links.size() == 1);
  REQUIRE(body.images.size() == 1);

  DecoratedChatLine head;
  head.text = "(De Ana) : ";
  const auto body_at = static_cast<std::uint32_t>(head.text.size());
  const DecoratedChatLine kept = body;
  moonclient::AppendRichBody(head, std::move(body));
  CHECK(head.text == "(De Ana) : " + kept.text);
  // Chaque morceau désigne encore le même texte, à sa nouvelle place.
  REQUIRE(head.styles.size() == 1);
  CHECK(head.styles[0].offset == kept.styles[0].offset + body_at);
  CHECK(Piece(head, head.styles[0].offset, head.styles[0].length) == "vu");
  REQUIRE(head.links.size() == 1);
  REQUIRE(head.carried.size() == 1);
  CHECK(head.links[0].offset == kept.links[0].offset + body_at);
  CHECK(Piece(head, head.links[0].offset, head.links[0].length) == address);
  REQUIRE(head.images.size() == 1);
  CHECK(head.images[0].offset == kept.images[0].offset + body_at);
  CHECK(head.images[0].picture_key == 0);
  REQUIRE(head.picture_urls.size() == 1);
  CHECK(head.picture_urls[0] == address);

  // UNE SECONDE PHRASE se numérote après la première vignette.
  DecoratedChatLine second;
  REQUIRE(DecorateRichChatLine(address, false, {}, pictures, &second));
  moonclient::AppendRichBody(head, std::move(second));
  REQUIRE(head.images.size() == 2);
  CHECK(head.images[1].picture_key == 1);
  CHECK(head.picture_urls.size() == 2);
}

TEST_CASE("Décoration riche : les clés des vignettes se suivent d'une ligne à l'autre",
          "[app][chat][riche]") {
  const std::string first = std::string(ro::core::kMirrorPicturePrefix) + "1.png";
  const std::string second = std::string(ro::core::kMirrorPicturePrefix) + "2.png";
  ChatRichContext pictures;
  pictures.pictures = true;
  DecoratedChatLine one;
  DecoratedChatLine two;
  REQUIRE(DecorateRichChatLine(first + " " + second, false, {}, pictures, &one));
  REQUIRE(DecorateRichChatLine(second, false, {}, pictures, &two));
  REQUIRE(one.images.size() == 2);
  REQUIRE(two.images.size() == 1);
  // 🔴 DEUX LIGNES ONT CHACUNE LEUR VIGNETTE DE RANG ZÉRO : numérotées dans
  // l'image, elles ne se confondent plus.
  CHECK(two.images[0].picture_key == 0);
  moonclient::NumberPictureKeys(one, 0);
  moonclient::NumberPictureKeys(two, static_cast<std::uint32_t>(one.picture_urls.size()));
  CHECK(one.images[0].picture_key == 0);
  CHECK(one.images[1].picture_key == 1);
  CHECK(two.images[0].picture_key == 2);
  CHECK(two.images[0].state != ro::ui::ChatPictureState::kFailed);

  // Une clé sans adresse met sa vignette en échec.
  DecoratedChatLine orphan;
  orphan.images.push_back(ChatInlineImage{.kind = ChatInlineKind::kPicture, .picture_key = 3});
  moonclient::NumberPictureKeys(orphan, 5);
  CHECK(orphan.images[0].state == ro::ui::ChatPictureState::kFailed);
  CHECK(orphan.images[0].picture_key == 3);
}

TEST_CASE("Décoration riche : ce que dit la ligne commence après son auteur",
          "[app][chat][riche]") {
  CHECK(ChatBodyOffset("Ana : salut", false) == 6);
  CHECK(ChatBodyOffset("Vous avez obtenu un objet", false) == 0);
  CHECK(ChatBodyOffset("[#gonryun][Ana]   salut", true) == 18);
  // Sans la forme du relai, rien n'est coupé.
  CHECK(ChatBodyOffset("pas de crochet", true) == 0);
  // ⚠ DANS LE TEXTE RENDU, balisage retiré.
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine("**Ana** : `salut`", false, {}, {}, &out));
  CHECK(out.text == "Ana : salut");
  CHECK(out.body_offset == 6);
}

TEST_CASE("Décoration de bulle : le balisage devient des styles, jamais du texte",
          "[app][chat][riche]") {
  const std::string raw =
      "Ana : **fort** ^FF0000rouge ||secret|| :" + std::string(ro::ui::ChatEmoteName(1)) + ":";
  DecoratedChatLine out;
  REQUIRE(DecorateBubbleLine(raw, {}, {}, &out));
  // 🔴 CE QUE LE JOUEUR LIT AU-DESSUS D'UNE TÊTE n'est plus la marque brute.
  CHECK(out.text.find("**") == std::string::npos);
  CHECK(out.text.find("||") == std::string::npos);
  CHECK(out.text.find('^') == std::string::npos);
  // La suite qui couvre le premier octet du mot : une couleur court jusqu'à la
  // fin de la phrase, et sa suite déborde du mot.
  const auto styled = [&out](std::string_view word) {
    const std::size_t at = out.text.find(word);
    return std::ranges::find_if(out.styles, [&](const ChatStyleRun& run) {
      return at != std::string::npos && run.offset <= at && at < run.offset + run.length;
    });
  };
  REQUIRE(styled("fort") != out.styles.end());
  CHECK((styled("fort")->style & ro::ui::kChatBold) != 0);
  REQUIRE(styled("rouge") != out.styles.end());
  CHECK(styled("rouge")->color == kPureRed);
  REQUIRE(styled("secret") != out.styles.end());
  CHECK((styled("secret")->style & ro::ui::kChatSpoiler) != 0);
  // L'emote du jeu se peint ; son nom reste en repli.
  CHECK(CountOf(out, ChatInlineKind::kGameEmote) == 1);
}

TEST_CASE("Décoration de bulle : l'icône d'un objet lié, jamais de vignette",
          "[app][chat][riche]") {
  const ro::vfs::ItemTable items = SmallTable();
  const ro::vfs::ItemWords words;
  const ChatDecorTables tables{.items = &items, .words = &words};
  const std::string raw = "vends " + ro::vfs::BuildItemLink(KnifeLink(), 3) + " <:pepe:123>";
  constexpr ImTextureID kKnifeIcon = 0xC0DE;
  const ItemIconLookup icon_of = [](std::uint32_t item_id) -> ImTextureID {
    return item_id == kKnifeId ? kKnifeIcon : 0;
  };
  DecoratedChatLine out;
  REQUIRE(DecorateBubbleLine(raw, tables, icon_of, &out));
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ChatLinkKind::kItem);
  // 🔴 COMME DANS LE FIL : l'icône devant le nom, texture déjà posée.
  REQUIRE(CountOf(out, ChatInlineKind::kItemIcon) == 1);
  const auto icon =
      std::ranges::find(out.images, ChatInlineKind::kItemIcon, &ChatInlineImage::kind);
  CHECK(icon->item_id == kKnifeId);
  CHECK(icon->texture == kKnifeIcon);
  // ⚠ UNE BULLE NE TÉLÉCHARGE RIEN.
  CHECK(CountOf(out, ChatInlineKind::kPicture) == 0);
  CHECK(out.picture_urls.empty());

  // Sans source d'icônes, aucune ne se pose.
  DecoratedChatLine bare;
  REQUIRE(DecorateBubbleLine(raw, tables, {}, &bare));
  CHECK(CountOf(bare, ChatInlineKind::kItemIcon) == 0);
}

TEST_CASE("Décoration de bulle : la durée se compte sur le texte montré", "[app][chat][riche]") {
  const std::string emote = ":" + std::string(ro::ui::ChatEmoteName(1)) + ":";
  const std::string raw = "**fort** ||lu|| ^FF0000\xC3\xA9 " + emote;
  DecoratedChatLine out;
  REQUIRE(DecorateBubbleLine(raw, {}, {}, &out));
  REQUIRE(out.text == "fort lu \xC3\xA9 " + emote);
  // « fort lu é » et son espace font dix caractères — « é » en compte UN —, et
  // l'emote un seul : ni les marques retirées, ni le nom qu'elle recouvre.
  constexpr std::size_t kShown = 11;
  CHECK(ro::ui::ChatBubbleShownCharacters(out.text, out.images) == kShown);
  const ro::ui::ChatBubbleLook look{
      .client_timing = false, .base_ms = 1000.0F, .per_character_ms = 100.0F};
  CHECK(ro::ui::ChatBubbleLifeMs(look, out.text, out.images) == 2100.0F);
  // La phrase brute aurait duré plus longtemps.
  CHECK(ro::ui::ChatBubbleLifeMs(look, raw) > ro::ui::ChatBubbleLifeMs(look, out.text, out.images));

  // Une icône d'objet, insérée devant son nom, ne rallonge rien.
  const ro::vfs::ItemTable items = SmallTable();
  const ro::vfs::ItemWords words;
  DecoratedChatLine linked;
  REQUIRE(DecorateBubbleLine(
      ro::vfs::BuildItemLink(KnifeLink(), 3), ChatDecorTables{.items = &items, .words = &words},
      [](std::uint32_t) -> ImTextureID { return 1; }, &linked));
  REQUIRE(CountOf(linked, ChatInlineKind::kItemIcon) == 1);
  CHECK(ro::ui::ChatBubbleShownCharacters(linked.text, linked.images) == linked.text.size());
}

namespace {

// L'auteur de « [#ally] Carol : … » : l'alias, une espace, le nom, « : ».
[[nodiscard]] ro::sim::ChatAuthor Carol() {
  ro::sim::ChatAuthor author;
  author.name = "Carol";
  author.name_offset = 8;
  author.name_length = 5;
  author.said_offset = 16;
  return author;
}

}  // namespace

TEST_CASE("Décoration riche : l'auteur décrit devient un pseudo cliquable", "[app][chat][riche]") {
  const std::string_view raw = "[#ally] Carol : *vite* ici";
  DecoratedChatLine out;
  REQUIRE(moonclient::DecorateAuthoredLine(raw, Carol(), "Moi", {}, {}, &out));
  CHECK(out.text == "[#ally] Carol : vite ici");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ChatLinkKind::kPlayer);
  CHECK(Piece(out, out.links[0].offset, out.links[0].length) == "Carol");
  REQUIRE(out.carried.size() == 1);
  CHECK(out.carried[0].player == "Carol");
  // La coupe est celle du serveur, et le style de la phrase vient derrière.
  CHECK(out.body_offset == 16);
  REQUIRE(out.styles.size() == 1);
  CHECK(Piece(out, out.styles[0].offset, out.styles[0].length) == "vite");
}

TEST_CASE("Décoration riche : son propre nom ne se clique pas", "[app][chat][riche]") {
  DecoratedChatLine out;
  REQUIRE(
      moonclient::DecorateAuthoredLine("[#ally] Carol : salut", Carol(), "Carol", {}, {}, &out));
  CHECK(out.text == "[#ally] Carol : salut");
  CHECK(out.links.empty());
  CHECK(out.body_offset == 16);
}

TEST_CASE("Décoration riche : un auteur qui ne s'accorde pas au texte n'en fait rien",
          "[app][chat][riche]") {
  DecoratedChatLine out;
  // Le nom n'est pas là où l'auteur le dit.
  CHECK_FALSE(moonclient::DecorateAuthoredLine("[#ally] Carla : salut", Carol(), "", {}, {}, &out));
  // La phrase commencerait au-delà du texte.
  CHECK_FALSE(moonclient::DecorateAuthoredLine("[#ally] Carol", Carol(), "", {}, {}, &out));
  CHECK(out.text.empty());
}

TEST_CASE("Décoration riche : l'avatar se pose devant, à la hauteur d'une rangée",
          "[app][chat][riche]") {
  ChatRichContext pictures;
  pictures.pictures = true;
  DecoratedChatLine out;
  REQUIRE(DecorateRichChatLine("[#gonryun][Ana] <:pepe:123>", true, {}, pictures, &out));
  moonclient::PlaceLineAvatar(out, "https://cdn.discordapp.com/embed/avatars/1.png");
  REQUIRE(out.images.size() == 2);
  CHECK(out.images[0].kind == ChatInlineKind::kPicture);
  CHECK(out.images[0].offset == 0);
  CHECK(out.images[0].length == 0);
  CHECK(out.images[0].line_high);
  CHECK_FALSE(out.images[1].line_high);
  // Chaque vignette garde l'adresse de son rang.
  REQUIRE(out.picture_urls.size() == 2);
  CHECK(out.picture_urls[out.images[0].picture_key] ==
        "https://cdn.discordapp.com/embed/avatars/1.png");
  CHECK(out.picture_urls[out.images[1].picture_key] == "https://cdn.discordapp.com/emojis/123.png");
}
