#include "ro/render/hat_effect_table.h"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"

using ro::render::HatEffect;
using ro::render::HatEffectTable;

namespace {

// LA TABLE EN MINIATURE, dans les deux formes qu'une fiche porte : un `.str`,
// ou un IDENTIFIANT d'effet. Les chiffres réels — 259 fiches, 149 fichiers,
// 111 identifiants — se vérifient par `roc-cli effect hat` sur une
// installation, pas ici.
//
// La fiche 11 est telle que la conversion l'écrit quand la table d'origine
// pose `isRenderBeforeCharacter` sans `hatEffectPos` : sans le drapeau, que le
// client ne consulterait pas.
constexpr std::string_view kMiniature = R"(
version: 1
hat_effects:
  9: {name: 'HAT_EF_DEUX', file: 'les_deux\deux.str', effect_id: 1240, offset: -11,
      render_before_character: true, ignore_riding: true, attached_head: true,
      effect_pair: true}
  1: {name: 'HAT_EF_BLOSSOM_FLUTTERING', file: 'efst_blossom_fluttering\sakura.str',
      offset: -4, offset_x: 0.5, adjust_position_when_shrunk: true,
      adjust_size_when_shrunk: true}
  5: {name: 'HAT_EF_CLOCKING', effect_id: 120}
  11: {file: 'sans_hauteur\x.str'}
  12: {file: '효과\x.str'}
)";

[[nodiscard]] HatEffectTable Lue() {
  auto table = HatEffectTable::Parse(kMiniature);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("Effets de chapeau : les DEUX formes d'une fiche", "[render][hateffect]") {
  const HatEffectTable table = Lue();
  REQUIRE(table.entries().size() == 5);
  // Triées par numéro, quel que soit l'ordre du fichier.
  CHECK(table.entries().front().hat_effect_id == 1);
  CHECK(table.entries().back().hat_effect_id == 12);

  // ⚠ LES DEUX FORMES SONT LE FAIT CENTRAL DE CETTE FAMILLE. Le plan l'annonçait
  // comme un chemin de rendu NOUVEAU, à porter après les dessinateurs ; elle ne
  // demande en réalité aucune machinerie de plus, parce qu'une fiche désigne
  // soit un `.str`, soit un identifiant d'effet — les deux familles que nous
  // peignons déjà. Un lecteur qui n'en verrait qu'une laisserait la moitié de
  // la table muette sans que rien ne le dise.
  const HatEffect* file = table.Find(1);
  REQUIRE(file != nullptr);
  CHECK(file->file == "efst_blossom_fluttering\\sakura.str");
  CHECK(file->effect_id == 0);
  CHECK(file->name == "HAT_EF_BLOSSOM_FLUTTERING");

  const HatEffect* effect_id = table.Find(5);
  REQUIRE(effect_id != nullptr);
  CHECK(effect_id->file.empty());
  CHECK(effect_id->effect_id == 120);

  // Une fiche peut porter les deux. L'installation n'en compte qu'une, et c'est
  // assez pour qu'un lecteur qui choisirait l'une OU l'autre se trompe sur elle.
  const HatEffect* both = table.Find(9);
  REQUIRE(both != nullptr);
  CHECK_FALSE(both->file.empty());
  CHECK(both->effect_id == 1240);
}

TEST_CASE("Effets de chapeau : un drapeau ABSENT vaut faux", "[render][hateffect]") {
  const HatEffectTable table = Lue();
  // ⚠ C'EST AINSI QUE LA TABLE EST ÉCRITE : aucune fiche ne porte `false`, elles
  // omettent le champ. Les 259 fiches de l'installation portent respectivement
  // 148, 148, 61, 45 et 2 drapeaux vrais ; les lire autrement les rendrait
  // toutes pareilles.
  const HatEffect* file = table.Find(1);
  REQUIRE(file != nullptr);
  CHECK(file->adjust_position_when_shrunk);
  CHECK(file->adjust_size_when_shrunk);
  CHECK_FALSE(file->render_before_character);
  CHECK_FALSE(file->ignore_riding);
  CHECK_FALSE(file->attached_head);

  const HatEffect* both = table.Find(9);
  REQUIRE(both != nullptr);
  CHECK(both->render_before_character);
  CHECK(both->ignore_riding);
  CHECK(both->attached_head);
  CHECK_FALSE(both->adjust_position_when_shrunk);
  // Sans décalage vertical, le client ne consulte pas le drapeau : la
  // conversion ne l'écrit pas, et la fiche le lit faux.
  const HatEffect* no_height = table.Find(11);
  REQUIRE(no_height != nullptr);
  CHECK_FALSE(no_height->render_before_character);
  // La paire dit que l'identifiant se joue AUSSI, à côté du fichier.
  CHECK(both->effect_pair);
  CHECK_FALSE(file->effect_pair);
}

TEST_CASE("Effets de chapeau : les décalages gardent leur unité", "[render][hateffect]") {
  const HatEffectTable table = Lue();
  const HatEffect* file = table.Find(1);
  REQUIRE(file != nullptr);
  // ⚠ LES DEUX DÉCALAGES N'ONT PAS LA MÊME UNITÉ, et c'est ce qui interdit de
  // les traiter ensemble : le vertical ne prend que des valeurs ENTIÈRES sur
  // l'installation (quatorze, de -17 à 4), l'autre est fractionnaire (-0,5,
  // -0,4, -0,3). Ils sont portés tels quels plutôt que traduits au jugé.
  CHECK(file->offset == -4.0F);
  CHECK(file->offset_x == 0.5F);
}

TEST_CASE("Effets de chapeau : un numéro sans fiche n'est pas une erreur", "[render][hateffect]") {
  const HatEffectTable table = Lue();
  // `HatEFID` nomme 292 effets, la table n'en décrit que 259 : un numéro nommé
  // et non décrit est le cas normal, pas une lecture ratée.
  CHECK(table.Find(2) == nullptr);
  CHECK(table.Find(999) == nullptr);
}

TEST_CASE("Effets de chapeau : une table absente est un ÉCHEC", "[render][hateffect]") {
  // Sans la table attendue, la lecture ne rend pas une table VIDE : une table
  // vide se lirait « cette installation n'a pas d'effets de chapeau », ce qui
  // est faux et se découvrirait en jeu.
  const auto table = HatEffectTable::Parse("version: 1\nautre_chose: [1, 2, 3]\n");
  REQUIRE_FALSE(table.has_value());
  CHECK(table.error().code == ro::core::ErrorCode::kCorruptData);
  CHECK_FALSE(HatEffectTable::Parse("version: 2\nhat_effects: {}\n").has_value());
  CHECK_FALSE(HatEffectTable::Parse("version: 1\nhat_effects: {1: 3}\n").has_value());
  CHECK_FALSE(HatEffectTable::Parse("version: 1\nhat_effects: {70000: {}}\n").has_value());
  CHECK_FALSE(HatEffectTable::Parse("version: 1\nhat_effects: {1: {effect_id: -1}}\n").has_value());
  CHECK_FALSE(HatEffectTable::Parse("[").has_value());
}

TEST_CASE("Effets de chapeau : un fichier se cherche en CP949", "[render][hateffect]") {
  // Le YAML est en UTF-8 ; le VFS range les noms coréens en CP949.
  const HatEffectTable table = Lue();
  const HatEffect* korean = table.Find(12);
  REQUIRE(korean != nullptr);
  CHECK(korean->file == "\xC8\xBF\xB0\xFA\\x.str");
}

TEST_CASE("Effets de chapeau : au-delà de 2442 c'est le Lua qui résout, et il se TROMPE",
          "[render][hateffect]") {
  const HatEffectTable table = Lue();

  // ⚠ CE TEST GARDE UN DÉFAUT DU CLIENT QUE NOUS REPRODUISONS, et c'est la
  // décision la moins évidente de cette famille. Voir ADR 0030.
  //
  // Le client ne résout par sa table de fichiers que SOUS 2442 ; au-delà il
  // appelle un global Lua avec `identifiant - 2442`, soit un ORDINAL. Or il
  // inscrit chaque fiche sous la clé `ordinal + 2442`, tandis qu'il lance
  // l'effet sous l'identifiant CONCRET de la fiche. Les deux plages se recouvrant, la recherche
  // aboutit — sur la MAUVAISE fiche.
  //
  // Sur l'installation, une seule fiche y tombe : l'ordinal 277 porte
  // l'identifiant 2443, donc 2443 - 2442 = 1, et le joueur voit le `sakura.str`
  // de l'ordinal 1. Montrer « ce qu'il faudrait » afficherait autre chose que
  // tous les autres écrans ; la PARITÉ est le but.
  const HatEffect* vu = table.FindByConcreteId(ro::render::kHatEffectIdBase + 1);
  REQUIRE(vu != nullptr);
  CHECK(vu->hat_effect_id == 1);
  CHECK(vu->file == "efst_blossom_fluttering\\sakura.str");

  // En deçà du seuil, cette branche ne répond pas : la table de fichiers du
  // client est seule, et c'est `ro::render::EffectFile` qui le porte.
  CHECK(table.FindByConcreteId(ro::render::kHatEffectIdBase - 1) == nullptr);
  CHECK(table.FindByConcreteId(1) == nullptr);
  // Au-delà du seuil mais sur un ordinal que la table ne décrit pas : rien, et
  // le client d'origine ne montre rien non plus.
  CHECK(table.FindByConcreteId(ro::render::kHatEffectIdBase + 2) == nullptr);
}
