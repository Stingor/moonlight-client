// LA PAGE DE RÉGLAGES DU CARNET DE CHASSE MVP : défauts, clés, grisages, bornes
// du préavis, rangée « Tout remettre au carnet » et raison d'un son qui n'a pas
// sonné.

#include "options/mvp_tracker_options.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "game/settings_file.h"
#include "support/temp_directory.h"

using moonclient::kMvpTrackerRowBase;
using moonclient::MvpAlertTone;
using moonclient::MvpAlertTrouble;
using moonclient::MvpTrackerOptions;
using moonclient::MvpTrackerRow;
using moonclient::MvpTrackerRowId;
using moonclient::MvpTrackerSettings;
using moonclient::SettingsFile;
using ro::test_support::TempDirectory;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

namespace {

constexpr int kCustomTone = static_cast<int>(MvpAlertTone::kCustom);

[[nodiscard]] const OptionRowView* Row(std::span<const OptionRowView> rows, MvpTrackerRow row) {
  const auto found = std::ranges::find(rows, MvpTrackerRowId(row), &OptionRowView::id);
  return found != rows.end() ? &*found : nullptr;
}

// Une page déclarée dans un fichier écrit à la main, puis relue.
void LoadInto(MvpTrackerOptions& page, const TempDirectory& folder, std::string_view text) {
  folder.WriteText("settings.yml", text);
  SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  file.Load();
}

}  // namespace

TEST_CASE("Carnet MVP, réglages : les défauts de la page", "[app][mvp]") {
  const MvpTrackerOptions page;
  const MvpTrackerSettings hunt = page.settings();
  CHECK(hunt.enabled);
  CHECK(hunt.alert_sound);
  CHECK(hunt.alert_lead_min == 5);
  CHECK(hunt.show_tombs);
  CHECK(hunt.look.show_sprites);
  CHECK_FALSE(hunt.look.animate_sprites);
  CHECK_FALSE(hunt.look.show_filter);
  CHECK(hunt.look.line_show_sprite);
  CHECK(hunt.look.line_text_color == IM_COL32(245, 237, 209, 255));
  CHECK(hunt.look.line_background_color == IM_COL32(10, 10, 15, 140));
}

TEST_CASE("Carnet MVP, réglages : les dix clés anglaises se relisent", "[app][mvp]") {
  const TempDirectory folder;
  MvpTrackerOptions page;
  LoadInto(page, folder,
           "mvp_tracker_enabled: true\n"
           "mvp_tracker_alert_sound: false\n"
           "mvp_tracker_alert_lead_min: 12\n"
           "mvp_tracker_show_tombs: false\n"
           "mvp_tracker_show_sprites: false\n"
           "mvp_tracker_animate_sprites: true\n"
           "mvp_tracker_show_filter: true\n"
           "mvp_tracker_line_sprite: false\n"
           "mvp_tracker_line_text: 4278190335\n"
           "mvp_tracker_line_background: 305419896\n");
  const MvpTrackerSettings hunt = page.settings();
  CHECK(hunt.enabled);
  CHECK_FALSE(hunt.alert_sound);
  CHECK(hunt.alert_lead_min == 12);
  CHECK_FALSE(hunt.show_tombs);
  CHECK_FALSE(hunt.look.show_sprites);
  CHECK(hunt.look.animate_sprites);
  CHECK(hunt.look.show_filter);
  CHECK_FALSE(hunt.look.line_show_sprite);
  CHECK(hunt.look.line_text_color == 4278190335U);
  CHECK(hunt.look.line_background_color == 305419896U);
  CHECK(page.look().show_filter);
}

TEST_CASE("Carnet MVP, réglages : un préavis hors bornes est ramené à la lecture", "[app][mvp]") {
  const TempDirectory folder;
  SECTION("trop grand") {
    MvpTrackerOptions page;
    LoadInto(page, folder, "mvp_tracker_alert_lead_min: 500\n");
    CHECK(page.settings().alert_lead_min == 60);
    page.Compose();
    const OptionRowView* lead = Row(page.rows(), MvpTrackerRow::kAlertLead);
    REQUIRE(lead != nullptr);
    CHECK(lead->value == 60.0F);
  }
  SECTION("négatif") {
    MvpTrackerOptions page;
    LoadInto(page, folder, "mvp_tracker_alert_lead_min: -3\n");
    CHECK(page.settings().alert_lead_min == 0);
    page.Compose();
    const OptionRowView* lead = Row(page.rows(), MvpTrackerRow::kAlertLead);
    REQUIRE(lead != nullptr);
    CHECK(lead->value == 0.0F);
  }
}

TEST_CASE("Carnet MVP, réglages : le curseur borne ce qu'il écrit", "[app][mvp]") {
  MvpTrackerOptions page;
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertLead), .value = 17.4F}));
  CHECK(page.settings().alert_lead_min == 17);
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertLead), .value = 99.0F}));
  CHECK(page.settings().alert_lead_min == 60);
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertLead), .value = -8.0F}));
  CHECK(page.settings().alert_lead_min == 0);
  // Les autres lignes posent leur champ.
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = true}));
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kShowTombs), .on = false}));
  CHECK(page.Change(
      {.changed = MvpTrackerRowId(MvpTrackerRow::kLineBackground), .color = IM_COL32(1, 2, 3, 4)}));
  CHECK(page.settings().enabled);
  CHECK_FALSE(page.settings().show_tombs);
  CHECK(page.look().line_background_color == IM_COL32(1, 2, 3, 4));
  // « Ouvrir » et « Tout remettre » sont à la page, mais ne changent rien.
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kOpen)}));
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kPutBackAll)}));
  // Une ligne d'une autre page n'est pas la sienne.
  CHECK_FALSE(page.Change({.changed = kMvpTrackerRowBase - 1, .on = true}));
  CHECK_FALSE(page.Change(
      {.changed = kMvpTrackerRowBase + static_cast<int>(MvpTrackerRow::kCount), .on = true}));
}

TEST_CASE("Carnet MVP, réglages : éteint, tout se grise sauf l'interrupteur", "[app][mvp]") {
  MvpTrackerOptions page;
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = false}));
  page.NoteLineCount(1);
  page.Compose();
  const std::span<const OptionRowView> rows = page.rows();
  // 🔴 « OUVRIR LE CARNET » COMPRIS : éteint, il n'ouvrirait rien, et seule la
  // case rallume.
  for (const OptionRowView& row : rows) {
    const bool master = row.id == MvpTrackerRowId(MvpTrackerRow::kEnabled);
    INFO("rangée " << row.id);
    CHECK(row.disabled == !master);
  }
  // L'ordre des rangées, et chaque forme. « Délai » suit le champ de filtre :
  // c'est l'allure de la table, pas celle des lignes détachées.
  constexpr std::array kOrder = {
      MvpTrackerRow::kEnabled,        MvpTrackerRow::kOpen,        MvpTrackerRow::kAlertSound,
      MvpTrackerRow::kAlertTone,      MvpTrackerRow::kAlertListen, MvpTrackerRow::kAlertFile,
      MvpTrackerRow::kAlertLead,      MvpTrackerRow::kShowSprites, MvpTrackerRow::kAnimateSprites,
      MvpTrackerRow::kShowFilter,     MvpTrackerRow::kShowDelay,   MvpTrackerRow::kShowTombs,
      MvpTrackerRow::kLinesNote,      MvpTrackerRow::kLineSprite,  MvpTrackerRow::kLineText,
      MvpTrackerRow::kLineBackground, MvpTrackerRow::kPutBackAll};
  // La raison d'un son qui n'a pas sonné n'y est pas : elle ne vient qu'après
  // un échec.
  static_assert(kOrder.size() + 1 == static_cast<std::size_t>(MvpTrackerRow::kCount));
  REQUIRE(rows.size() == kOrder.size());
  for (std::size_t rank = 0; rank < rows.size(); ++rank) {
    CHECK(rows[rank].id == MvpTrackerRowId(kOrder[rank]));
  }
  CHECK(Row(rows, MvpTrackerRow::kOpen)->kind == OptionKind::kAction);
  CHECK(Row(rows, MvpTrackerRow::kAlertLead)->kind == OptionKind::kSlider);
  CHECK(Row(rows, MvpTrackerRow::kLinesNote)->kind == OptionKind::kNote);
  CHECK(Row(rows, MvpTrackerRow::kLinesNote)->heading != nullptr);
  CHECK(Row(rows, MvpTrackerRow::kLineText)->kind == OptionKind::kColor);
  CHECK(Row(rows, MvpTrackerRow::kLineBackground)->kind == OptionKind::kColor);

  // Allumé, seule l'animation reste grisée tant que les sprites sont éteints —
  // et le fichier personnel tant qu'un autre son est choisi.
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = true}));
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kShowSprites), .on = false}));
  page.Compose();
  for (const OptionRowView& row : page.rows()) {
    INFO("rangée " << row.id);
    CHECK(row.disabled == (row.id == MvpTrackerRowId(MvpTrackerRow::kAnimateSprites) ||
                           row.id == MvpTrackerRowId(MvpTrackerRow::kAlertFile)));
  }
}

TEST_CASE("Carnet MVP, réglages : le son de l'alerte, sous la case qu'il précise", "[app][mvp]") {
  MvpTrackerOptions page;
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = true}));
  page.Compose();
  const OptionRowView* tone = Row(page.rows(), MvpTrackerRow::kAlertTone);
  const OptionRowView* listen = Row(page.rows(), MvpTrackerRow::kAlertListen);
  const OptionRowView* file = Row(page.rows(), MvpTrackerRow::kAlertFile);
  REQUIRE(tone != nullptr);
  REQUIRE(listen != nullptr);
  REQUIRE(file != nullptr);
  CHECK(tone->kind == OptionKind::kChoice);
  CHECK(tone->choices.size() == moonclient::kMvpAlertToneCount);
  CHECK(tone->chosen == static_cast<int>(MvpAlertTone::kSystem));
  CHECK(tone->indented);
  // « Écouter » est un bouton, à côté du choix.
  CHECK(listen->kind == OptionKind::kAction);
  CHECK(listen->same_line);
  CHECK(file->kind == OptionKind::kText);
  CHECK_FALSE(tone->disabled);
  CHECK_FALSE(listen->disabled);
  CHECK(file->disabled);

  // Le fichier personnel choisi, son champ s'ouvre et porte le nom tapé.
  CHECK(
      page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertTone), .chosen = kCustomTone}));
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertFile), .text = "alerte.wav"}));
  CHECK(page.settings().alert_tone == kCustomTone);
  CHECK(page.settings().alert_wav == "alerte.wav");
  page.Compose();
  file = Row(page.rows(), MvpTrackerRow::kAlertFile);
  REQUIRE(file != nullptr);
  CHECK_FALSE(file->disabled);
  CHECK(std::string(file->text) == "alerte.wav");

  // SANS ALERTE SONORE, le son ne se choisit ni ne s'écoute.
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertSound), .on = false}));
  page.Compose();
  for (const MvpTrackerRow muted :
       {MvpTrackerRow::kAlertTone, MvpTrackerRow::kAlertListen, MvpTrackerRow::kAlertFile}) {
    INFO("rangée " << MvpTrackerRowId(muted));
    CHECK(Row(page.rows(), muted)->disabled);
  }
  // « Écouter » est à la page, mais ne change rien.
  const MvpTrackerSettings before = page.settings();
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertListen)}));
  CHECK(page.settings().alert_tone == before.alert_tone);
  CHECK(page.settings().alert_wav == before.alert_wav);
}

TEST_CASE("Carnet MVP, réglages : le son de l'alerte se relit, borné", "[app][mvp]") {
  const TempDirectory folder;
  SECTION("le fichier personnel") {
    MvpTrackerOptions page;
    LoadInto(page, folder,
             "mvp_tracker_alert_tone: 3\n"
             "mvp_tracker_alert_wav: sons\\alerte.wav\n");
    CHECK(page.settings().alert_tone == kCustomTone);
    CHECK(page.settings().alert_wav == "sons\\alerte.wav");
  }
  SECTION("hors plage : le son du système, pas la borne") {
    MvpTrackerOptions page;
    LoadInto(page, folder, "mvp_tracker_alert_tone: 7\n");
    CHECK(page.settings().alert_tone == static_cast<int>(MvpAlertTone::kSystem));
    page.Compose();
    CHECK(Row(page.rows(), MvpTrackerRow::kAlertTone)->chosen == 0);
  }
  SECTION("une liste déroulante hors plage aussi") {
    MvpTrackerOptions page;
    CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertTone), .chosen = -2}));
    CHECK(page.settings().alert_tone == static_cast<int>(MvpAlertTone::kSystem));
  }
}

TEST_CASE("Carnet MVP, réglages : un choix neuf oublie l'échec déjà dit", "[app][mvp]") {
  MvpTrackerOptions page;
  page.alert_failure_reported() = "absent";
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertFile), .text = "neuf.wav"}));
  CHECK(page.alert_failure_reported().empty());
  page.alert_failure_reported() = "absent";
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertTone), .chosen = 1}));
  CHECK(page.alert_failure_reported().empty());
}

namespace {

// Le rang de la raison dans la page, juste après celui de `after` — ou faux.
[[nodiscard]] bool TroubleFollows(std::span<const OptionRowView> rows, MvpTrackerRow after) {
  const auto found = std::ranges::find(rows, MvpTrackerRowId(after), &OptionRowView::id);
  return found != rows.end() && std::next(found) != rows.end() &&
         std::next(found)->id == MvpTrackerRowId(MvpTrackerRow::kAlertTrouble);
}

// La page allumée, sur ce son, vient d'échouer pour cette raison. ⚠ PAR
// RÉFÉRENCE : ses rangées pointent dans ses propres chaînes.
void FailOn(MvpTrackerOptions& page, int tone, MvpAlertTrouble trouble) {
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = true}));
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertTone), .chosen = tone}));
  page.alert_trouble() = trouble;
  page.Compose();
}

}  // namespace

TEST_CASE("Carnet MVP, réglages : la raison d'un son muet, sous le réglage en cause",
          "[app][mvp]") {
  struct Case {
    int tone;
    MvpAlertTrouble trouble;
    MvpTrackerRow under;
    std::string_view text;
  };
  constexpr int kNewMessage = static_cast<int>(MvpAlertTone::kNewMessage);
  const std::array cases = {
      Case{kCustomTone, MvpAlertTrouble::kFileMissing, MvpTrackerRow::kAlertFile,
           "Fichier introuvable dans savedata."},
      Case{kCustomTone, MvpAlertTrouble::kFileUnreadable, MvpTrackerRow::kAlertFile,
           "Fichier illisible : ce n'est pas un .wav lisible."},
      Case{kCustomTone, MvpAlertTrouble::kNameRefused, MvpTrackerRow::kAlertFile,
           "Nom refusé : le fichier doit rester dans savedata."},
      Case{kCustomTone, MvpAlertTrouble::kNameEmpty, MvpTrackerRow::kAlertFile,
           "Aucun fichier nommé."},
      // Coupés, ce n'est pas le fichier : la raison va sous le choix du son.
      Case{kCustomTone, MvpAlertTrouble::kMuted, MvpTrackerRow::kAlertListen,
           "Les bruitages du jeu sont coupés : la case « Bruitages » les rallume."},
      Case{kNewMessage, MvpAlertTrouble::kFileMissing, MvpTrackerRow::kAlertListen,
           "Son du jeu introuvable dans l'installation."},
      Case{kNewMessage, MvpAlertTrouble::kFileUnreadable, MvpTrackerRow::kAlertListen,
           "Son du jeu illisible."},
  };
  for (const Case& failed : cases) {
    INFO(failed.text);
    MvpTrackerOptions page;
    FailOn(page, failed.tone, failed.trouble);
    const OptionRowView* note = Row(page.rows(), MvpTrackerRow::kAlertTrouble);
    REQUIRE(note != nullptr);
    CHECK(std::string_view(note->label) == failed.text);
    CHECK(note->kind == OptionKind::kNote);
    CHECK(note->warning);
    CHECK(note->indented);
    CHECK(TroubleFollows(page.rows(), failed.under));
  }
  // RIEN À DIRE : pas de rangée.
  MvpTrackerOptions page;
  FailOn(page, kCustomTone, MvpAlertTrouble::kNone);
  CHECK(Row(page.rows(), MvpTrackerRow::kAlertTrouble) == nullptr);
  // L'ALERTE SONORE ÉTEINTE, la raison se tait avec elle.
  FailOn(page, kCustomTone, MvpAlertTrouble::kFileMissing);
  REQUIRE(Row(page.rows(), MvpTrackerRow::kAlertTrouble) != nullptr);
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertSound), .on = false}));
  page.Compose();
  CHECK(Row(page.rows(), MvpTrackerRow::kAlertTrouble) == nullptr);
}

TEST_CASE("Carnet MVP, réglages : la raison s'efface quand le son ou le fichier change",
          "[app][mvp]") {
  MvpTrackerOptions page;
  page.alert_trouble() = MvpAlertTrouble::kFileMissing;
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertFile), .text = "neuf.wav"}));
  CHECK(page.alert_trouble() == MvpAlertTrouble::kNone);
  page.alert_trouble() = MvpAlertTrouble::kFileUnreadable;
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertTone), .chosen = 1}));
  CHECK(page.alert_trouble() == MvpAlertTrouble::kNone);
  // « Écouter » lui-même ne l'efface pas : c'est le son joué qui le fait.
  page.alert_trouble() = MvpAlertTrouble::kFileMissing;
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kAlertListen)}));
  CHECK(page.alert_trouble() == MvpAlertTrouble::kFileMissing);
}

TEST_CASE("Carnet MVP, réglages : « coupés » s'efface quand les bruitages reviennent",
          "[app][mvp]") {
  MvpTrackerOptions page;
  page.alert_trouble() = MvpAlertTrouble::kMuted;
  page.NoteSoundAudible(false);
  CHECK(page.alert_trouble() == MvpAlertTrouble::kMuted);
  page.NoteSoundAudible(true);
  CHECK(page.alert_trouble() == MvpAlertTrouble::kNone);
  // Une autre raison ne tient pas au volume : elle reste.
  page.alert_trouble() = MvpAlertTrouble::kFileMissing;
  page.NoteSoundAudible(true);
  CHECK(page.alert_trouble() == MvpAlertTrouble::kFileMissing);
}

TEST_CASE("Carnet MVP, réglages : l'aide du champ de filtre met en garde sur les colonnes",
          "[app][mvp]") {
  MvpTrackerOptions page;
  page.Compose();
  const OptionRowView* filter = Row(page.rows(), MvpTrackerRow::kShowFilter);
  REQUIRE(filter != nullptr);
  REQUIRE(filter->help != nullptr);
  REQUIRE(filter->help_warning != nullptr);
  CHECK(std::string_view(filter->help_warning).starts_with("Les COLONNES"));
  // Le paragraphe n'est dit qu'une fois : derrière le signe, plus dans l'aide.
  CHECK(std::string_view(filter->help).find("COLONNES") == std::string_view::npos);
}

TEST_CASE("Carnet MVP, réglages : « Tout remettre au carnet » compte les lignes", "[app][mvp]") {
  MvpTrackerOptions page;
  page.Compose();
  CHECK(Row(page.rows(), MvpTrackerRow::kPutBackAll) == nullptr);
  page.NoteLineCount(2);
  page.Compose();
  const OptionRowView* put_back = Row(page.rows(), MvpTrackerRow::kPutBackAll);
  REQUIRE(put_back != nullptr);
  CHECK(std::string(put_back->label) == "Tout remettre au carnet (2)");
  CHECK(put_back->kind == OptionKind::kAction);
}

TEST_CASE("Carnet MVP, réglages : « Tout remettre au carnet » demande confirmation, compte compris",
          "[app][mvp]") {
  MvpTrackerOptions page;
  page.NoteLineCount(3);
  page.Compose();
  const OptionRowView* put_back = Row(page.rows(), MvpTrackerRow::kPutBackAll);
  REQUIRE(put_back != nullptr);
  REQUIRE(put_back->confirm != nullptr);
  CHECK(std::string(put_back->confirm) == "Remettre au carnet les 3 lignes détachées ?");
  page.NoteLineCount(1);
  page.Compose();
  put_back = Row(page.rows(), MvpTrackerRow::kPutBackAll);
  REQUIRE(put_back != nullptr);
  REQUIRE(put_back->confirm != nullptr);
  CHECK(std::string(put_back->confirm) == "Remettre au carnet la ligne détachée ?");
}

TEST_CASE("Carnet MVP, réglages : la rangée « Délai » bascule la colonne et se relit",
          "[app][mvp]") {
  MvpTrackerOptions page;
  CHECK_FALSE(page.look().show_delay);
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = true}));
  page.Compose();
  const OptionRowView* delay = Row(page.rows(), MvpTrackerRow::kShowDelay);
  REQUIRE(delay != nullptr);
  CHECK(delay->kind == OptionKind::kToggle);
  CHECK_FALSE(delay->on);
  CHECK(page.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kShowDelay), .on = true}));
  CHECK(page.look().show_delay);
  page.Compose();
  delay = Row(page.rows(), MvpTrackerRow::kShowDelay);
  REQUIRE(delay != nullptr);
  CHECK(delay->on);
  // Le clic droit de l'en-tête dit ce qui a changé, et seulement cela.
  CHECK_FALSE(page.SetShowDelay(true));
  CHECK(page.SetShowDelay(false));
  CHECK_FALSE(page.look().show_delay);

  const TempDirectory folder;
  MvpTrackerOptions read_back;
  LoadInto(read_back, folder, "mvp_tracker_show_delay: true\n");
  CHECK(read_back.look().show_delay);
}

TEST_CASE("Carnet MVP, réglages : la porte dit la touche, ou qu'il en faut une", "[app][mvp]") {
  MvpTrackerOptions page;
  page.Compose();
  const OptionRowView* open = Row(page.rows(), MvpTrackerRow::kOpen);
  REQUIRE(open != nullptr);
  CHECK(open->object == nullptr);
  REQUIRE(open->object_note != nullptr);
  CHECK(std::string(open->object_note) == "ou une touche à lier : « Carnet de chasse MVP »");
  page.NoteOpenKey("Ctrl+M");
  page.Compose();
  open = Row(page.rows(), MvpTrackerRow::kOpen);
  REQUIRE(open != nullptr);
  CHECK(std::string(open->object_note) == "ou la touche Ctrl+M");
}

TEST_CASE("Carnet MVP, réglages : allumé par défaut, et l'aide dit ce qui continue", "[app][mvp]") {
  MvpTrackerOptions page;
  CHECK(page.settings().enabled);
  page.Compose();
  const OptionRowView* master = Row(page.rows(), MvpTrackerRow::kEnabled);
  REQUIRE(master != nullptr);
  REQUIRE(master->help != nullptr);
  // ÉTEINT N'EST PAS « HORS DU GROUPE » : le serveur enregistre nos kills pour
  // tous les membres, et seul le départ du groupe cesse le partage.
  const std::string help = master->help;
  CHECK(help.find("Seule cette case le rallume") != std::string::npos);
  CHECK(help.find("quitte le groupe") != std::string::npos);
}
