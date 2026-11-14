// LES RÉGLAGES DES LIENS, DES IMAGES ET DES MENTIONS DU CHAT : défauts, clés,
// relecture, bornes de la taille des vignettes, listes de sites admis — leur
// plafond, leur nettoyage et leurs rangées « Retirer ».

#include "options/chat_web_options.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/web_address.h"
#include "ro/core/web_preview_policy.h"

#include "game/settings_file.h"
#include "support/temp_directory.h"

using moonclient::ChatWebOptions;
using moonclient::ChatWebRow;
using moonclient::ChatWebSetup;
using moonclient::kChatOpenHostRowBase;
using moonclient::kChatPreviewHostRowBase;
using moonclient::kChatWebRowBase;
using moonclient::SettingsFile;
using ro::test_support::TempDirectory;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

namespace {

[[nodiscard]] int IdOf(ChatWebRow row) { return kChatWebRowBase + static_cast<int>(row); }

[[nodiscard]] const OptionRowView* Row(std::span<const OptionRowView> rows, int id) {
  const auto found = std::ranges::find(rows, id, &OptionRowView::id);
  return found != rows.end() ? &*found : nullptr;
}

// Les sites que porte une rangée « Retirer » d'une liste, dans l'ordre.
[[nodiscard]] std::vector<std::string> ShownHosts(std::span<const OptionRowView> rows,
                                                  int first_row_id) {
  std::vector<std::string> hosts;
  for (const OptionRowView& row : rows) {
    if (row.id >= first_row_id &&
        row.id < first_row_id + static_cast<int>(ro::core::kMostUserHosts)) {
      CHECK(row.kind == OptionKind::kAction);
      CHECK(row.small_button);
      REQUIRE(row.object != nullptr);
      hosts.emplace_back(row.object);
    }
  }
  return hosts;
}

// Une page déclarée dans un fichier écrit à la main, puis relue.
void LoadInto(ChatWebOptions& page, const TempDirectory& folder, std::string_view text) {
  folder.WriteText("settings.yml", text);
  SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  file.Load();
}

}  // namespace

TEST_CASE("Liens du chat, réglages : les défauts", "[app][chat_web]") {
  ChatWebOptions page;
  const ChatWebSetup setup = page.setup();
  CHECK(setup.url_confirm);
  // 🔴 RIEN NE SE TÉLÉCHARGE SANS QU'ON L'AIT DEMANDÉ.
  CHECK_FALSE(setup.url_preview);
  CHECK_FALSE(setup.pictures);
  CHECK(setup.picture_pixels == 48);
  CHECK(setup.emote_images);
  CHECK(setup.item_icons);
  CHECK(setup.preview_hosts.empty());
  CHECK(setup.open_hosts.empty());
  CHECK(setup.mentions);
  CHECK(setup.mention_words.empty());
  CHECK_FALSE(setup.mention_sound);
  CHECK(setup.mention_flash);
}

TEST_CASE("Liens du chat, réglages : les douze clés se déclarent à leur défaut",
          "[app][chat_web]") {
  const TempDirectory folder;
  ChatWebOptions page;
  SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  const moonclient::SettingsValues values = file.Values();
  CHECK(values.size() == 12);
  const auto value_of = [&values](const char* key) {
    const auto found = values.find(key);
    REQUIRE(found != values.end());
    return found->second;
  };
  CHECK(value_of("url_confirm_chat") == "true");
  CHECK(value_of("url_preview_chat") == "false");
  CHECK(value_of("pictures_chat") == "false");
  CHECK(value_of("picture_size_chat") == "48");
  CHECK(value_of("emote_images_chat") == "true");
  CHECK(value_of("item_icons_chat") == "true");
  CHECK(value_of("mentions_chat") == "true");
  CHECK(value_of("mention_sound_chat") == "false");
  CHECK(value_of("mention_flash_chat") == "true");
  CHECK(values.contains("preview_hosts_chat"));
  CHECK(values.contains("open_hosts_chat"));
  CHECK(values.contains("mention_words_chat"));
}

TEST_CASE("Liens du chat, réglages : un fichier écrit à la main se relit", "[app][chat_web]") {
  const TempDirectory folder;
  ChatWebOptions page;
  LoadInto(page, folder,
           "url_confirm_chat: false\n"
           "url_preview_chat: true\n"
           "pictures_chat: true\n"
           "picture_size_chat: 96\n"
           "emote_images_chat: false\n"
           "item_icons_chat: false\n"
           "preview_hosts_chat: \"a.com;b.net\"\n"
           "open_hosts_chat: \"wiki.example\"\n"
           "mentions_chat: false\n"
           "mention_words_chat: \"mvp;Baphomet\"\n"
           "mention_sound_chat: true\n"
           "mention_flash_chat: false\n");
  const ChatWebSetup setup = page.setup();
  CHECK_FALSE(setup.url_confirm);
  CHECK(setup.url_preview);
  CHECK(setup.pictures);
  CHECK(setup.picture_pixels == 96);
  CHECK_FALSE(setup.emote_images);
  CHECK_FALSE(setup.item_icons);
  CHECK(setup.preview_hosts == "a.com;b.net");
  CHECK(setup.open_hosts == "wiki.example");
  CHECK_FALSE(setup.mentions);
  CHECK(setup.mention_words == "mvp;Baphomet");
  CHECK(setup.mention_sound);
  CHECK_FALSE(setup.mention_flash);
}

TEST_CASE("Liens du chat, réglages : ce qui s'écrit se relit à l'identique", "[app][chat_web]") {
  const TempDirectory folder;
  const std::filesystem::path path = folder.path() / "settings.yml";
  {
    ChatWebOptions page;
    SettingsFile file(path);
    page.Declare(file);
    CHECK(page.Change({.changed = IdOf(ChatWebRow::kUrlPreview), .on = true}));
    CHECK(page.Change({.changed = IdOf(ChatWebRow::kPictureSize), .value = 72.0F}));
    CHECK(page.Change({.changed = IdOf(ChatWebRow::kMentionWords), .text = "Ana;\"mvp\""}));
    CHECK(page.AddPreviewHost("pics.example"));
    CHECK(page.AddOpenHost("wiki.example"));
    file.Touch();
    file.Flush();
  }
  ChatWebOptions reread;
  SettingsFile file(path);
  reread.Declare(file);
  file.Load();
  const ChatWebSetup setup = reread.setup();
  CHECK(setup.url_preview);
  CHECK(setup.picture_pixels == 72);
  CHECK(setup.mention_words == "Ana;\"mvp\"");
  CHECK(setup.preview_hosts == "pics.example");
  CHECK(setup.open_hosts == "wiki.example");
}

TEST_CASE("Liens du chat, réglages : la taille des vignettes reste entre 24 et 128",
          "[app][chat_web]") {
  const TempDirectory folder;
  SECTION("trop grande dans le fichier") {
    ChatWebOptions page;
    LoadInto(page, folder, "picture_size_chat: 500\n");
    CHECK(page.setup().picture_pixels == 128);
    std::vector<OptionRowView> rows;
    page.ComposeInto(rows);
    const OptionRowView* size = Row(rows, IdOf(ChatWebRow::kPictureSize));
    REQUIRE(size != nullptr);
    CHECK(size->kind == OptionKind::kSlider);
    CHECK(size->value == 128.0F);
    CHECK(size->lowest == 24.0F);
    CHECK(size->highest == 128.0F);
  }
  SECTION("trop petite dans le fichier") {
    ChatWebOptions page;
    LoadInto(page, folder, "picture_size_chat: -7\n");
    CHECK(page.setup().picture_pixels == 24);
  }
  SECTION("le curseur borne ce qu'il écrit") {
    ChatWebOptions page;
    CHECK(page.Change({.changed = IdOf(ChatWebRow::kPictureSize), .value = 10.0F}));
    CHECK(page.setup().picture_pixels == 24);
    CHECK(page.Change({.changed = IdOf(ChatWebRow::kPictureSize), .value = 300.0F}));
    CHECK(page.setup().picture_pixels == 128);
    CHECK(page.Change({.changed = IdOf(ChatWebRow::kPictureSize), .value = 64.4F}));
    CHECK(page.setup().picture_pixels == 64);
  }
}

TEST_CASE("Liens du chat, réglages : un site admis est nettoyé, et admis une fois",
          "[app][chat_web]") {
  ChatWebOptions page;
  CHECK(page.AddPreviewHost("  Pics.EXAMPLE "));
  CHECK_FALSE(page.AddPreviewHost("pics.example"));
  CHECK(page.setup().preview_hosts == "pics.example");
  // ⚠ UN SEUL SITE PAR GESTE : un séparateur en ferait deux.
  CHECK_FALSE(page.AddPreviewHost("a.example;b.example"));
  // Un hôte inutilisable n'entre pas.
  CHECK_FALSE(page.AddPreviewHost(""));
  CHECK_FALSE(page.AddPreviewHost("user@evil.example"));
  CHECK(page.setup().preview_hosts == "pics.example");
  // Les deux listes ne se mêlent pas.
  CHECK(page.AddOpenHost("wiki.example"));
  CHECK(page.setup().open_hosts == "wiki.example");
  CHECK(page.setup().preview_hosts == "pics.example");
}

TEST_CASE("Liens du chat, réglages : le fil n'admet qu'un hôte exact", "[app][chat_web]") {
  // 🔴 UNE RÈGLE DE SUFFIXE ADMETTRAIT TOUT UN DOMAINE d'un seul clic : elle ne
  // s'écrit qu'à la main, dans le fichier.
  ChatWebOptions page;
  for (const std::string_view refused :
       {".com", ".x.fr", "a..b", "localhost", "x.fr.", ".cdn.example", "a.-b.fr"}) {
    CAPTURE(refused);
    CHECK_FALSE(moonclient::IsAdmissibleChatHost(refused));
    CHECK_FALSE(page.AddPreviewHost(refused));
    CHECK_FALSE(page.AddOpenHost(refused));
  }
  CHECK(page.setup().preview_hosts.empty());
  CHECK(page.setup().open_hosts.empty());
  CHECK(moonclient::IsAdmissibleChatHost("cdn.example"));
  CHECK(moonclient::IsAdmissibleChatHost("a.b.example"));
  // Une règle de suffixe écrite à la main dans le fichier reste lue.
  const TempDirectory folder;
  ChatWebOptions loaded;
  LoadInto(loaded, folder, "preview_hosts_chat: \".cdn.example\"\n");
  CHECK(loaded.setup().preview_hosts == ".cdn.example");
}

TEST_CASE("Liens du chat : quand un clic ouvre sans la boîte", "[app][chat_web]") {
  using moonclient::OffersAlwaysFor;
  using moonclient::OpensWithoutPrompt;
  const auto judge = [](std::string_view openable, bool url_confirm, bool listed_host) {
    return OpensWithoutPrompt(ro::core::ParseWebAddress(openable),
                              ro::core::WebAddressWarnings(openable), url_confirm, listed_host);
  };
  // Confirmation cochée : seul un site admis, en https propre, s'ouvre seul.
  CHECK_FALSE(judge("https://wiki.example/a", true, false));
  CHECK(judge("https://wiki.example/a", true, true));
  CHECK_FALSE(judge("http://wiki.example/a", true, true));
  CHECK_FALSE(judge("https://x@wiki.example/a", true, true));
  // Confirmation décochée : le http en clair passe, rien d'autre de suspect.
  CHECK(judge("https://wiki.example/a", false, false));
  CHECK(judge("http://wiki.example/a", false, false));
  // 🔴 « @ », punycode, hors ASCII et illisible forcent la boîte.
  CHECK_FALSE(judge("https://discord.com@evil.example/", false, false));
  CHECK_FALSE(judge("https://xn--pple-43d.com/", false, false));
  CHECK_FALSE(judge("https://\xC3\xA9vil.example/", false, false));
  CHECK_FALSE(judge("https://a.example\\@evil.example/", false, false));
  CHECK_FALSE(judge("https://a..example/", false, true));
  // « Toujours » : une adresse lisible, en https, sans avertissement, sur un
  // hôte que le fil peut admettre.
  const auto offers = [](std::string_view openable) {
    return OffersAlwaysFor(ro::core::ParseWebAddress(openable),
                           ro::core::WebAddressWarnings(openable));
  };
  CHECK(offers("https://wiki.example/a"));
  CHECK_FALSE(offers("http://wiki.example/a"));
  CHECK_FALSE(offers("https://localhost/a"));
  CHECK_FALSE(offers("https://x@wiki.example/a"));
  CHECK_FALSE(offers("https://a..example/"));
}

TEST_CASE("Liens du chat, réglages : soixante-quatre sites au plus par liste", "[app][chat_web]") {
  ChatWebOptions page;
  constexpr std::size_t kTried = ro::core::kMostUserHosts + 6;
  for (std::size_t rank = 0; rank < kTried; ++rank) {
    const std::string host = "site" + std::to_string(rank) + ".example";
    CHECK(page.AddPreviewHost(host) == (rank < ro::core::kMostUserHosts));
    CHECK(page.AddOpenHost(host) == (rank < ro::core::kMostUserHosts));
  }
  const std::vector<std::string> preview = ro::core::SplitHostList(page.setup().preview_hosts);
  REQUIRE(preview.size() == ro::core::kMostUserHosts);
  CHECK(preview.front() == "site0.example");
  CHECK(preview.back() == "site63.example");
  CHECK(ro::core::SplitHostList(page.setup().open_hosts).size() == ro::core::kMostUserHosts);

  // Un fichier qui en porte davantage n'en montre pas plus : les rangées
  // déborderaient sur la plage voisine.
  const TempDirectory folder;
  std::string listed;
  for (std::size_t rank = 0; rank < kTried; ++rank) {
    listed += "host" + std::to_string(rank) + ".example;";
  }
  ChatWebOptions loaded;
  LoadInto(loaded, folder, "preview_hosts_chat: \"" + listed + "\"\n");
  std::vector<OptionRowView> rows;
  loaded.ComposeInto(rows);
  CHECK(ShownHosts(rows, kChatPreviewHostRowBase).size() == ro::core::kMostUserHosts);
}

TEST_CASE("Liens du chat, réglages : « Retirer » reprend un site admis", "[app][chat_web]") {
  ChatWebOptions page;
  CHECK(page.AddPreviewHost("a.example"));
  CHECK(page.AddPreviewHost("b.example"));
  CHECK(page.AddPreviewHost("c.example"));
  CHECK(page.AddOpenHost("wiki.example"));
  std::vector<OptionRowView> rows;
  page.ComposeInto(rows);
  REQUIRE(ShownHosts(rows, kChatPreviewHostRowBase) ==
          std::vector<std::string>{"a.example", "b.example", "c.example"});
  REQUIRE(ShownHosts(rows, kChatOpenHostRowBase) == std::vector<std::string>{"wiki.example"});
  // Chaque liste a son intitulé.
  CHECK(Row(rows, IdOf(ChatWebRow::kPreviewHostsTitle)) != nullptr);
  CHECK(Row(rows, IdOf(ChatWebRow::kOpenHostsTitle)) != nullptr);

  // ⚠ LE RANG DÉSIGNE LE SITE MONTRÉ : un site admis entre la composition et le
  // clic ne décale rien.
  CHECK(page.AddPreviewHost("0.example"));
  CHECK(page.Change({.changed = kChatPreviewHostRowBase + 1}));
  CHECK(page.setup().preview_hosts == "a.example;c.example;0.example");
  CHECK(page.Change({.changed = kChatOpenHostRowBase}));
  CHECK(page.setup().open_hosts.empty());
  // Une rangée de la plage sans site montré est à la page, et ne retire rien.
  CHECK(page.Change({.changed = kChatPreviewHostRowBase + 40}));
  CHECK(page.setup().preview_hosts == "a.example;c.example;0.example");

  // Une liste vide ne montre ni intitulé ni rangée.
  rows.clear();
  page.ComposeInto(rows);
  CHECK(Row(rows, IdOf(ChatWebRow::kOpenHostsTitle)) == nullptr);
  CHECK(ShownHosts(rows, kChatOpenHostRowBase).empty());
}

TEST_CASE("Liens du chat, réglages : la liste se montre aperçu éteint", "[app][chat_web]") {
  ChatWebOptions page;
  CHECK(page.AddPreviewHost("a.example"));
  REQUIRE_FALSE(page.setup().url_preview);
  std::vector<OptionRowView> rows;
  page.ComposeInto(rows);
  const std::vector<std::string> shown = ShownHosts(rows, kChatPreviewHostRowBase);
  REQUIRE(shown.size() == 1);
  const OptionRowView* remove = Row(rows, kChatPreviewHostRowBase);
  REQUIRE(remove != nullptr);
  CHECK_FALSE(remove->disabled);
}

TEST_CASE("Liens du chat, réglages : chaque rangée pose son champ", "[app][chat_web]") {
  ChatWebOptions page;
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kUrlConfirm), .on = false}));
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kPictures), .on = true}));
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kEmoteImages), .on = false}));
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kItemIcons), .on = false}));
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kMentionWords), .text = "Ana;mvp"}));
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kMentionSound), .on = true}));
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kMentionFlash), .on = false}));
  const ChatWebSetup setup = page.setup();
  CHECK_FALSE(setup.url_confirm);
  CHECK(setup.pictures);
  CHECK_FALSE(setup.emote_images);
  CHECK_FALSE(setup.item_icons);
  CHECK(setup.mention_words == "Ana;mvp");
  CHECK(setup.mention_sound);
  CHECK_FALSE(setup.mention_flash);
  // Une ligne d'une autre page n'est pas la sienne.
  CHECK_FALSE(page.Change({.changed = kChatWebRowBase - 1, .on = true}));
  CHECK_FALSE(page.Change({.changed = kChatPreviewHostRowBase - 1, .on = true}));
  CHECK_FALSE(
      page.Change({.changed = kChatOpenHostRowBase + static_cast<int>(ro::core::kMostUserHosts)}));
}

TEST_CASE("Liens du chat, réglages : mentions éteintes, leurs sous-réglages se grisent",
          "[app][chat_web]") {
  ChatWebOptions page;
  CHECK(page.Change({.changed = IdOf(ChatWebRow::kMentions), .on = false}));
  std::vector<OptionRowView> rows;
  page.ComposeInto(rows);
  const OptionRowView* master = Row(rows, IdOf(ChatWebRow::kMentions));
  REQUIRE(master != nullptr);
  CHECK_FALSE(master->disabled);
  for (const ChatWebRow sub :
       {ChatWebRow::kMentionWords, ChatWebRow::kMentionSound, ChatWebRow::kMentionFlash}) {
    const OptionRowView* row = Row(rows, IdOf(sub));
    REQUIRE(row != nullptr);
    CHECK(row->disabled);
    CHECK(row->indented);
  }
  // Les images éteintes, leur taille se grise aussi, et reste visible.
  const OptionRowView* size = Row(rows, IdOf(ChatWebRow::kPictureSize));
  REQUIRE(size != nullptr);
  CHECK(size->disabled);
}
