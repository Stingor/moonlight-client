#include "ro/formats/named_effects.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/bson_builder.h"

using ro::core::ErrorCode;
using ro::formats::NamedEffect;
using ro::formats::NamedEffectTable;
using ro::test_support::BsonBuilder;

namespace {

// Une fiche telle que le fichier du jeu les écrit : onze champs, toujours les
// mêmes, dans l'ordre alphabétique de leurs clés.
BsonBuilder EffectEntry(std::string_view file, std::string_view sound, double pos_x, double pos_y,
                        std::int32_t start_delay_ms, std::int32_t quake_delay, bool infinite,
                        bool floor, bool above_ui, bool sound_loop, bool to_ground) {
  BsonBuilder record;
  record.Text("FilePath", file)
      .Flag("IsAboveUI", above_ui)
      .Flag("IsFloor", floor)
      .Flag("IsInfinite", infinite)
      .Flag("IsSoundLoop", sound_loop)
      .Flag("IsToGround", to_ground)
      .Number("PosX", pos_x)
      .Number("PosY", pos_y)
      .Integer("QuakeDelay", quake_delay)
      .Text("SoundPath", sound)
      .Integer("StartDelayTime", start_delay_ms);
  return record;
}

// Deux fiches réelles, telles que le fichier de l'installation les porte.
std::vector<std::uint8_t> TwoEntries() {
  BsonBuilder table;
  table
      .Document("jack_frost_nova_loop_bottom",
                EffectEntry("HYPER_NOVICE\\HN_JACK_FROST_NOVA\\jack_frost_nova\\"
                            "jack_frost_nova_loop_bottom.str",
                            "effect\\HYPER_NOVICE\\hn_jack_frost_nova01.wav", 0.0, 12.0, 496, -1,
                            true, true, false, true, false))
      .Document("kunaiarea_01_start",
                EffectEntry("SHINKIRO_SHIRANUI\\NPC_KINAI\\kunaiarea_01_start.str",
                            "effect\\SHINKIRO_SHIRANUI\\SS_KUNAIAREA_1.wav", 0.0, 9.0, 0, -1, false,
                            false, false, false, false));

  BsonBuilder root;
  root.Document("EZ2STREffect", table);
  return root.Build();
}

}  // namespace

TEST_CASE("Effets nommés : une fiche se relit champ par champ", "[formats][named_effects]") {
  const std::vector<std::uint8_t> bytes = TwoEntries();
  const auto table = NamedEffectTable::Parse(bytes);
  REQUIRE(table.has_value());
  REQUIRE(table->entries().size() == 2);

  const NamedEffect* loop = table->Find("jack_frost_nova_loop_bottom");
  REQUIRE(loop != nullptr);
  CHECK(loop->file ==
        "HYPER_NOVICE\\HN_JACK_FROST_NOVA\\jack_frost_nova\\jack_frost_nova_loop_bottom.str");
  CHECK(loop->sound == "effect\\HYPER_NOVICE\\hn_jack_frost_nova01.wav");
  CHECK(loop->offset_x == 0.0F);
  CHECK(loop->offset_y == 12.0F);
  // Le délai est en MILLISECONDES : c'est le client qui le divise par seize
  // pour en faire des tics. Le ranger déjà divisé perdrait les fiches dont il
  // ne tombe pas juste — 391 en est une.
  CHECK(loop->start_delay_ms == 496);
  CHECK(loop->quake_delay_ticks == -1);
  CHECK(loop->is_infinite);
  CHECK(loop->is_floor);
  CHECK(loop->is_sound_loop);
  CHECK_FALSE(loop->is_above_ui);
  CHECK_FALSE(loop->is_to_ground);

  const NamedEffect* start = table->Find("kunaiarea_01_start");
  REQUIRE(start != nullptr);
  CHECK(start->offset_y == 9.0F);
  CHECK(start->start_delay_ms == 0);
  CHECK_FALSE(start->is_infinite);
}

TEST_CASE("Effets nommés : la recherche abaisse la casse, comme le client",
          "[formats][named_effects]") {
  const std::vector<std::uint8_t> bytes = TwoEntries();
  const auto table = NamedEffectTable::Parse(bytes);
  REQUIRE(table.has_value());

  // Le client abaisse la clé avant de la ranger, et la recherche subit la même
  // transformation que le rangement : c'est ce qui fait tenir la
  // correspondance, quelle que soit la casse du nom que le code demande.
  CHECK(table->Find("KUNAIAREA_01_START") != nullptr);
  CHECK(table->Find("Kunaiarea_01_Start") == table->Find("kunaiarea_01_start"));
  CHECK(table->Find("kunaiarea_00_start") == nullptr);
}

TEST_CASE("Effets nommés : la table est TRIÉE par nom", "[formats][named_effects]") {
  // Le rang d'une fiche dans cet ordre est ce dont le client fait un
  // identifiant, en lui ajoutant dix mille. Rendre les fiches dans l'ordre du
  // fichier au lieu de l'ordre trié perdrait cette correspondance.
  BsonBuilder table;
  table
      .Document("zzz", EffectEntry("z.str", "", 0.0, 0.0, 0, -1, false, false, false, false, false))
      .Document("aaa", EffectEntry("a.str", "", 0.0, 0.0, 0, -1, false, false, false, false, false))
      .Document("mmm",
                EffectEntry("m.str", "", 0.0, 0.0, 0, -1, false, false, false, false, false));
  BsonBuilder root;
  root.Document("EZ2STREffect", table);

  const auto lu = NamedEffectTable::Parse(root.Build());
  REQUIRE(lu.has_value());
  REQUIRE(lu->entries().size() == 3);
  CHECK(lu->entries()[0].name == "aaa");
  CHECK(lu->entries()[1].name == "mmm");
  CHECK(lu->entries()[2].name == "zzz");
}

TEST_CASE("Effets nommés : une fiche sans fichier se lit quand même", "[formats][named_effects]") {
  // Le fichier du jeu en porte une : une ligne MODÈLE laissée par ses auteurs,
  // nommée en coréen et sans aucun chemin. Refuser le fichier entier pour elle
  // priverait le joueur des deux cent vingt-deux autres.
  BsonBuilder table;
  table.Document("modele", EffectEntry("", "", 0.0, 0.0, 0, -1, false, false, false, false, false))
      .Document("vrai",
                EffectEntry("v.str", "", 0.0, 7.0, 0, -1, false, false, false, false, false));
  BsonBuilder root;
  root.Document("EZ2STREffect", table);

  const auto lu = NamedEffectTable::Parse(root.Build());
  REQUIRE(lu.has_value());
  REQUIRE(lu->entries().size() == 2);
  REQUIRE(lu->Find("modele") != nullptr);
  CHECK(lu->Find("modele")->file.empty());
  CHECK(lu->Find("vrai")->offset_y == 7.0F);
}

TEST_CASE("Effets nommés : un champ manquant prend son défaut", "[formats][named_effects]") {
  // QuakeDelay absent ne vaut pas ZÉRO : zéro ferait trembler l'écran à chaque
  // pose. Le défaut est celui que portent les 223 fiches du jeu, -1, qui veut
  // dire AUCUNE secousse.
  BsonBuilder maigre;
  maigre.Text("FilePath", "seul.str");
  BsonBuilder table;
  table.Document("maigre", maigre);
  BsonBuilder root;
  root.Document("EZ2STREffect", table);

  const auto lu = NamedEffectTable::Parse(root.Build());
  REQUIRE(lu.has_value());
  const NamedEffect* record = lu->Find("maigre");
  REQUIRE(record != nullptr);
  CHECK(record->file == "seul.str");
  CHECK(record->sound.empty());
  CHECK(record->quake_delay_ticks == -1);
  CHECK(record->start_delay_ms == 0);
  CHECK_FALSE(record->is_infinite);
}

TEST_CASE("Effets nommés : un fichier qui n'est pas celui-là est refusé",
          "[formats][named_effects]") {
  SECTION("la racine ne porte pas EZ2STREffect") {
    BsonBuilder root;
    root.Integer("autre", 1);
    const auto lu = NamedEffectTable::Parse(root.Build());
    REQUIRE_FALSE(lu.has_value());
    CHECK(lu.error().code == ErrorCode::kCorruptData);
  }
  SECTION("une fiche n'est pas un document") {
    BsonBuilder table;
    table.Integer("pas_une_fiche", 1);
    BsonBuilder root;
    root.Document("EZ2STREffect", table);
    const auto lu = NamedEffectTable::Parse(root.Build());
    REQUIRE_FALSE(lu.has_value());
    CHECK(lu.error().code == ErrorCode::kCorruptData);
  }
  SECTION("la table est vide") {
    const BsonBuilder table;
    BsonBuilder root;
    root.Document("EZ2STREffect", table);
    const auto lu = NamedEffectTable::Parse(root.Build());
    REQUIRE_FALSE(lu.has_value());
    CHECK(lu.error().code == ErrorCode::kCorruptData);
  }
  SECTION("ce ne sont pas des octets BSON") {
    const std::vector<std::uint8_t> named_sound(32, 0xAB);
    const auto lu = NamedEffectTable::Parse(named_sound);
    REQUIRE_FALSE(lu.has_value());
  }
}
