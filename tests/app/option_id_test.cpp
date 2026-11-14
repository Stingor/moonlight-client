// LES IDENTIFIANTS DE RANGÉE, ET CE QU'ILS NE DOIVENT JAMAIS NOMMER.
//
// 🔴 CE BANC GARDE UN BUG LIVRÉ, ET IL VAUT D'ÊTRE RACONTÉ. `GameOption` tient
// sur HUIT BITS ; les identifiants COMPOSÉS — ceux des jauges, des barres de
// raccourcis, des barres d'entité, des étiquettes — se comptent par milliers. Un
// `static_cast<GameOption>` d'un de ces milliers ne déborde pas : il TRONQUE, et
// retombe sur une option parfaitement valable.
//
// Le 2026-09-20, la ligne « Montrer pour cette espèce » du premier onglet des
// étiquettes portait le numéro 4000. Tronqué, 4000 vaut 160, qui est
// `kCaptureRecord` : cocher une case de réglage démarrait l'enregistrement d'un
// film, et la liste déroulante d'à côté — 4001, soit `kCapturePicture` —
// prenait une capture d'écran. Rien ne le disait ; c'est le joueur qui l'a vu.
//
// ⚠ ET LA COLLISION NE SE VOIT PAS EN LISANT : elle dépend du nombre d'entrées
// de l'énumération, qui grandit à chaque réglage ajouté. Ce banc la cherche
// mécaniquement, sur TOUTES les familles à la fois.

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "options/card_album_options.h"
#include "options/craft_atlas_options.h"
#include "options/game_option_ids.h"

using moonclient::EntityBarField;
using moonclient::EntityBarRow;
using moonclient::EntityNameField;
using moonclient::EntityNameRow;
using moonclient::GameOption;
using moonclient::NamedOption;
using moonclient::ShortcutBarField;
using moonclient::ShortcutBarRow;
using moonclient::StatBarField;
using moonclient::StatBarRow;

TEST_CASE("Options : un identifiant composé ne nomme aucune option", "[app][options]") {
  // ⚠ LE NOMBRE D'ESPÈCES ET DE BARRES EST ÉCRIT ICI plutôt qu'inclus : ce banc
  // garde une propriété des NUMÉROS, et il doit continuer de mordre même si une
  // famille change de taille.
  constexpr std::size_t kKinds = 6;
  constexpr std::size_t kBars = 8;

  for (std::size_t kind = 0; kind < kKinds; ++kind) {
    for (int field = 0; field < static_cast<int>(EntityNameField::kCount); ++field) {
      CHECK_FALSE(
          NamedOption(EntityNameRow(kind, static_cast<EntityNameField>(field))).has_value());
    }
    for (int field = 0; field < static_cast<int>(EntityBarField::kCount); ++field) {
      CHECK_FALSE(NamedOption(EntityBarRow(kind, static_cast<EntityBarField>(field))).has_value());
    }
  }
  for (std::size_t bar = 0; bar < kBars; ++bar) {
    for (int field = 0; field < static_cast<int>(StatBarField::kCount); ++field) {
      CHECK_FALSE(NamedOption(StatBarRow(bar, static_cast<StatBarField>(field))).has_value());
    }
    for (int field = 0; field < static_cast<int>(ShortcutBarField::kCount); ++field) {
      CHECK_FALSE(
          NamedOption(ShortcutBarRow(bar, static_cast<ShortcutBarField>(field))).has_value());
    }
  }
}

// ⚠ ET LA TRADUCTION RECONNAÎT QUAND MÊME LES VRAIES OPTIONS : une garde qui
// refuserait tout serait verte sur le banc du dessus et muette en jeu.
TEST_CASE("Options : un identifiant nommé se reconnaît", "[app][options]") {
  CHECK(NamedOption(static_cast<int>(GameOption::kMusic)) == GameOption::kMusic);
  CHECK(NamedOption(static_cast<int>(GameOption::kCaptureRecord)) == GameOption::kCaptureRecord);
  CHECK(NamedOption(static_cast<int>(GameOption::kCount) - 1).has_value());

  // Hors bornes des deux côtés.
  CHECK_FALSE(NamedOption(-1).has_value());
  CHECK_FALSE(NamedOption(static_cast<int>(GameOption::kCount)).has_value());
}

TEST_CASE("Options : deux pages ne se volent pas leurs lignes", "[app][options]") {
  // 🔴 L'ALBUM ET L'ATLAS ONT PARTAGÉ LA PLAGE 5700 : l'Atlas, interrogé le
  // premier, prenait les cases de l'album pour les siennes, et cocher « Sources
  // de la carte » réglait une case de l'Atlas.
  moonclient::CraftAtlasOptions atlas;
  ro::ui::CraftAtlasWindowState window;
  for (int row = 0; row < static_cast<int>(moonclient::CardAlbumRow::kCount); ++row) {
    ro::ui::OptionsWindowAction asked;
    asked.changed = moonclient::CardAlbumRowId(static_cast<moonclient::CardAlbumRow>(row));
    CHECK_FALSE(atlas.Change(asked, &window));
  }
}
