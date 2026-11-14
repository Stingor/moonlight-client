#include "ro/ui/npc_dialog_text.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

// LE TEXTE D'UN SCRIPT DE PNJ : ce qu'il montre, ce qu'il cache, et où il se
// replie.
//
// ⚠ LA MESURE EST UNE RÈGLE À NOUS : un pixel par octet. Un banc d'essai n'a ni
// police ni fenêtre, et les distances deviennent ainsi des nombres qu'on peut
// écrire dans un essai.

using ro::ui::DialogFragment;
using ro::ui::DialogLayout;
using ro::ui::DialogLink;
using ro::ui::DialogMedia;
using ro::ui::DialogMetrics;
using ro::ui::DialogRun;
using ro::ui::DialogTargetKind;
using ro::ui::DialogVocabulary;
using ro::ui::LayOutDialog;
using ro::ui::MonsterRank;
using ro::ui::PageMonsterPortrait;
using ro::ui::ParseDialogLine;

namespace {

constexpr float kBytePixels = 1.0F;
constexpr float kLinePixels = 10.0F;

[[nodiscard]] float ByteRuler(std::string_view piece, void* /*owner*/) {
  return static_cast<float>(piece.size()) * kBytePixels;
}

[[nodiscard]] DialogMetrics Ruler(float wrap) {
  DialogMetrics metrics;
  metrics.wrap_width = wrap;
  metrics.line_height = kLinePixels;
  metrics.font_size = kLinePixels;
  metrics.measure = ByteRuler;
  return metrics;
}

// Le texte affiché d'une ligne, morceaux recollés.
[[nodiscard]] std::string Shown(std::string_view line) {
  std::string shown;
  for (const DialogRun& run : ParseDialogLine(line)) {
    shown += run.text;
  }
  return shown;
}

}  // namespace

TEST_CASE("Texte de PNJ : la couleur, et ce que `^000000` veut dire", "[ui][npc]") {
  const std::vector<DialogRun> runs = ParseDialogLine("^1F81D4Agent^000000 de voyage");
  REQUIRE(runs.size() == 2);
  CHECK(runs[0].text == "Agent");
  CHECK(runs[0].color == IM_COL32(0x1F, 0x81, 0xD4, 255));
  CHECK(runs[1].text == " de voyage");
  // ⚠ `^000000` N'EST PAS DU NOIR : c'est le retour à la couleur par défaut, et
  // zéro est ce qui le dit. Le prendre à la lettre écrirait en noir sur un fond
  // sombre.
  CHECK(runs[1].color == 0);
}

TEST_CASE("Texte de PNJ : un accent circonflexe isolé reste du texte", "[ui][npc]") {
  // Il en faut SIX derrière : sans les six chiffres, c'est une lettre.
  CHECK(Shown("Un ^ tout seul") == "Un ^ tout seul");
  CHECK(Shown("^12345 trop court") == "^12345 trop court");
}

TEST_CASE("Texte de PNJ : les chevrons qui ne sont pas une balise s'affichent", "[ui][npc]") {
  // 🔴 C'EST LE DÉFAUT QUE LA LISTE BLANCHE CORRIGE. Un parseur qui avale tout ce
  // qui tient entre '<' et le premier '>' ne montrait de cette ligne que
  // « Entrez . », là où le client officiel rend la phrase entière.
  CHECK(Shown("Entrez ^6B9900<Wave Mode - Forest>^000000.") == "Entrez <Wave Mode - Forest>.");
  // Et une balise connue, elle, disparaît.
  CHECK(Shown("Texte <B>gras</B> ici") == "Texte gras ici");
}

TEST_CASE("Texte de PNJ : un lien d'objet porte sa charge sans la montrer", "[ui][npc]") {
  const std::vector<DialogRun> runs =
      ParseDialogLine("Apporte <ITEM>une Potion rouge<INFO>501</INFO></ITEM> demain");
  REQUIRE(runs.size() == 3);
  CHECK(runs[0].text == "Apporte ");
  CHECK(runs[0].link == DialogLink::kNone);
  CHECK(runs[1].text == "une Potion rouge");
  CHECK(runs[1].link == DialogLink::kItem);
  // ⚠ LA CHARGE NE S'AFFICHE PAS, et elle reste attachée au morceau qu'elle
  // décrit : vider le morceau à `<INFO>` couperait le libellé en deux liens.
  CHECK(runs[1].payload == "501");
  CHECK(runs[2].text == " demain");
  CHECK(runs[2].link == DialogLink::kNone);
}

TEST_CASE("Texte de PNJ : une adresse et un lieu", "[ui][npc]") {
  const std::vector<DialogRun> link_runs =
      ParseDialogLine("<URL>le site<INFO>https://exemple.test/a?b=1</INFO></URL>");
  REQUIRE(link_runs.size() == 1);
  CHECK(link_runs[0].link == DialogLink::kAddress);
  // Le deux-points et l'interrogation de l'adresse traversent intacts.
  CHECK(link_runs[0].payload == "https://exemple.test/a?b=1");

  const std::vector<DialogRun> lieu =
      ParseDialogLine("<NAVI>Prontera<INFO>prontera,150,150</INFO></NAVI>");
  REQUIRE(lieu.size() == 1);
  CHECK(lieu[0].link == DialogLink::kPlace);
  CHECK(lieu[0].payload == "prontera,150,150");
}

TEST_CASE("Texte de PNJ : l'icône d'un objet est un morceau à elle seule", "[ui][npc]") {
  const std::vector<DialogRun> runs = ParseDialogLine("Il faut ^i[501] une potion");
  REQUIRE(runs.size() == 3);
  CHECK(runs[1].icon_item == 501);
  CHECK(runs[1].text.empty());
  CHECK(runs[2].text == " une potion");
  // Sans crochet fermant, ce n'est pas une icône : le texte reste.
  CHECK(Shown("^i[501 sans fin") == "^i[501 sans fin");
}

TEST_CASE("Texte de PNJ : le lien d'un monstre porte son rang et son nom", "[ui][npc]") {
  const std::vector<DialogRun> runs = ParseDialogLine("Cherche <MOBL>1002:0:Poring</MOBL> dehors");
  REQUIRE(runs.size() == 3);
  CHECK(runs[1].target.kind == DialogTargetKind::kMonster);
  CHECK(runs[1].target.id == 1002);
  CHECK(runs[1].target.rank == MonsterRank::kOrdinary);
  // ⚠ LE NOM VOYAGE : le client ne sait pas nommer un monstre, et une balise qui
  // ne porterait que l'identifiant afficherait un numéro.
  CHECK(runs[1].target.label == "Poring");
  CHECK(runs[1].text == "<[Mob] Poring>");

  const std::vector<DialogRun> mvp = ParseDialogLine("<MOBL>1086:2:Golden Thief Bug</MOBL>");
  REQUIRE(mvp.size() == 1);
  CHECK(mvp[0].target.rank == MonsterRank::kChampion);
  // Le nom garde ses espaces : c'est le DERNIER champ, et il prend tout.
  CHECK(mvp[0].target.label == "Golden Thief Bug");
}

TEST_CASE("Texte de PNJ : un lien d'objet de la maison porte son icône", "[ui][npc]") {
  const std::vector<DialogRun> runs = ParseDialogLine("<ITMR>501:Potion rouge</ITMR>");
  // L'icône d'abord, le libellé ensuite : c'est ce qui rend une liste de
  // composants lisible d'un coup d'œil.
  REQUIRE(runs.size() == 2);
  CHECK(runs[0].icon_item == 501);
  CHECK(runs[1].target.kind == DialogTargetKind::kItem);
  CHECK(runs[1].text == "<Potion rouge>");
}

TEST_CASE("Texte de PNJ : un réglage se nomme chez le LECTEUR", "[ui][npc]") {
  // 🔴 LE LIBELLÉ DU SCRIPT N'EST QU'UN REPLI. Chaque client range ses réglages
  // à sa façon et les nomme dans sa langue : le PNJ écrit une CLÉ, et c'est le
  // lecteur qui dit où cela mène et comment cela s'appelle chez lui.
  DialogVocabulary words;
  words.setting_label = [](std::string_view key, void* /*owner*/) -> std::string {
    return key == "chat" ? "Conversation" : std::string();
  };

  const std::vector<DialogRun> connu = ParseDialogLine("<SETL>chat:Chat settings</SETL>", words);
  REQUIRE(connu.size() == 1);
  CHECK(connu[0].target.kind == DialogTargetKind::kSetting);
  CHECK(connu[0].target.key == "chat");
  // Le nom du LECTEUR, et non celui que le script transporte.
  CHECK(connu[0].text.find("Conversation") != std::string::npos);

  // ⚠ UNE DESTINATION INCONNUE NE FORME AUCUN LIEN : le libellé transporté
  // reste, en texte simple. Un lien qui n'ouvre rien vaut moins que pas de lien.
  const std::vector<DialogRun> unknown = ParseDialogLine("<SETL>minimap:Minimap</SETL>", words);
  REQUIRE(unknown.size() == 1);
  CHECK_FALSE(unknown[0].target.valid());
  CHECK(unknown[0].text == "Minimap");
}

TEST_CASE("Texte de PNJ : un réglage a son menu, et son libellé tel quel", "[ui][npc]") {
  // Le libellé arrive composé par le lecteur : le dialogue n'y ajoute rien.
  DialogVocabulary words;
  words.setting_label = [](std::string_view key, void* /*owner*/) -> std::string {
    return key == "sheet_titles" ? "[Fenêtre: Personnage › Titres]" : std::string();
  };
  const std::vector<DialogRun> runs =
      ParseDialogLine("<SETL>sheet_titles:Personnage › Titres</SETL>", words);
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].text == "[Fenêtre: Personnage › Titres]");
  // ⚠ LE MENU EST CELUI D'UN RÉGLAGE, jamais celui d'un objet.
  CHECK(ro::ui::DialogTargetMenuKind(DialogTargetKind::kSetting) == ro::ui::ChatLinkKind::kSetting);
  CHECK(ro::ui::DialogTargetMenuKind(DialogTargetKind::kItem) == ro::ui::ChatLinkKind::kItem);
  CHECK_FALSE(ro::ui::DialogTargetMenuKind(DialogTargetKind::kMonster).has_value());
}

TEST_CASE("Texte de PNJ : un état se nomme chez le LECTEUR", "[ui][npc]") {
  // Même règle, et pour la même raison : le serveur envoie un INDEX, et chaque
  // installation nomme ses états dans sa langue, depuis ses propres tables.
  DialogVocabulary words;
  words.status_label = [](std::uint32_t effect_id, void* /*owner*/) -> std::string {
    return effect_id == 33 ? "Impositio Manus" : std::string();
  };

  const std::vector<DialogRun> connu = ParseDialogLine("<STAL>33:Blessing</STAL>", words);
  REQUIRE(connu.size() == 1);
  CHECK(connu[0].target.kind == DialogTargetKind::kStatus);
  CHECK(connu[0].target.id == 33);
  CHECK(connu[0].text.find("Impositio Manus") != std::string::npos);

  // Un état que CETTE installation n'a pas : pas de lien, et le libellé reste.
  const std::vector<DialogRun> absent = ParseDialogLine("<STAL>999:Inconnu</STAL>", words);
  REQUIRE(absent.size() == 1);
  CHECK_FALSE(absent[0].target.valid());
  CHECK(absent[0].text == "Inconnu");
}

TEST_CASE("Texte de PNJ : sans vocabulaire, les deux restent lisibles", "[ui][npc]") {
  // Un appelant qui n'a pas de tables — un banc d'essai, un écran de démarrage —
  // ne doit pas faire disparaître le texte.
  CHECK(Shown("<SETL>chat:Reglages du chat</SETL>") == "Reglages du chat");
  CHECK(Shown("<STAL>33:Benediction</STAL>") == "Benediction");
}

TEST_CASE("Texte de PNJ : une balise maison mal formée s'affiche telle quelle", "[ui][npc]") {
  // C'est le bon échec : un script fautif se VOIT, là où l'escamotage se cherche
  // une heure.
  CHECK(Shown("<MOBL>1002:0</MOBL>") == "<MOBL>1002:0</MOBL>");
  CHECK(Shown("<MOBL>1002:0:Poring") == "<MOBL>1002:0:Poring");
  CHECK(Shown("<ITMR>0:Rien</ITMR>") == "<ITMR>0:Rien</ITMR>");
}

TEST_CASE("Texte de PNJ : une image, avec ou sans taille imposée", "[ui][npc]") {
  const std::vector<DialogRun> simple = ParseDialogLine("<IMG>ressource\\carte.bmp</IMG>");
  REQUIRE(simple.size() == 1);
  CHECK(simple[0].media == DialogMedia::kImage);
  CHECK(simple[0].source == "ressource\\carte.bmp");
  CHECK(simple[0].wanted_width == 0.0F);

  const std::vector<DialogRun> mesuree = ParseDialogLine("<IMG>64:32:ressource\\carte.bmp</IMG>");
  REQUIRE(mesuree.size() == 1);
  CHECK(mesuree[0].wanted_width == 64.0F);
  CHECK(mesuree[0].wanted_height == 32.0F);

  // ⚠ LE « https: » D'UNE ADRESSE NE PASSE PAS POUR UNE TAILLE : les deux
  // premiers champs doivent être des NOMBRES.
  const std::vector<DialogRun> web = ParseDialogLine("<IMG>https://exemple.test/a.png</IMG>");
  REQUIRE(web.size() == 1);
  CHECK(web[0].source == "https://exemple.test/a.png");
  CHECK(web[0].wanted_width == 0.0F);
}

TEST_CASE("Texte de PNJ : le portrait vaut pour la page et ne s'affiche pas", "[ui][npc]") {
  const std::vector<std::string> lines = {"<MOBP>1002</MOBP>Bonjour.", "Autre chose ?",
                                          "<MOBP>1188</MOBP>"};
  // Le DERNIER gagne : un script qui en pose deux a changé d'avis en route.
  CHECK(PageMonsterPortrait(lines) == 1188);
  // Et il ne laisse pas ses chevrons dans le texte.
  CHECK(Shown("<MOBP>1002</MOBP>Bonjour.") == "Bonjour.");
}

TEST_CASE("Mise en page : les mots se replient sur la largeur donnée", "[ui][npc]") {
  // Quatre mots de quatre octets : deux tiennent par ligne sur neuf pixels.
  const std::vector<std::string> lines = {"abcd abcd abcd"};
  const DialogLayout laid = LayOutDialog(lines, Ruler(9.0F));
  REQUIRE(laid.fragments.size() == 3);
  CHECK(laid.fragments[0].top == 0.0F);
  CHECK(laid.fragments[1].top == 0.0F);
  // Le troisième ne tient plus : il descend.
  CHECK(laid.fragments[2].top == kLinePixels);
  CHECK(laid.fragments[2].left == 0.0F);
  CHECK(laid.height == 2.0F * kLinePixels);
}

TEST_CASE("Mise en page : une ligne vide garde sa hauteur", "[ui][npc]") {
  // ⚠ UNE LIGNE VIDE EST UN SAUT QUE LE SCRIPT A ÉCRIT : l'avaler serrerait un
  // dialogue que son auteur a fait respirer.
  const std::vector<std::string> lines = {"abc", "", "def"};
  const DialogLayout laid = LayOutDialog(lines, Ruler(100.0F));
  REQUIRE(laid.fragments.size() == 2);
  CHECK(laid.fragments[1].top == 2.0F * kLinePixels);
  CHECK(laid.height == 3.0F * kLinePixels);
}

TEST_CASE("Mise en page : le soulignement d'un lien couvre ses deux mots", "[ui][npc]") {
  // « <[Mob] Poring> » fait deux mots, et sans cela le soulignement du second
  // repartirait de la marge gauche.
  const std::vector<std::string> lines = {"<MOBL>1002:0:Poring</MOBL>"};
  const DialogLayout laid = LayOutDialog(lines, Ruler(100.0F));
  REQUIRE(laid.fragments.size() == 2);
  CHECK(laid.fragments[0].text == "<[Mob]");
  CHECK(laid.fragments[1].text == "Poring>");
  // Le second commence APRÈS l'espace ; son trait, lui, repart de l'espace.
  CHECK(laid.fragments[1].underline_left < laid.fragments[1].left);
  // Les deux désignent la MÊME cible : c'est la zone entière qui est cliquable.
  REQUIRE(laid.targets.size() == 1);
  CHECK(laid.fragments[0].target == 0);
  CHECK(laid.fragments[1].target == 0);
}

TEST_CASE("Mise en page : une page sans mesure ne place rien", "[ui][npc]") {
  // Un appelant sans police ne doit pas produire une page à zéro pixel qu'on
  // croirait vide : il n'en produit aucune.
  DialogMetrics metrics;
  metrics.wrap_width = 100.0F;
  metrics.line_height = kLinePixels;
  const std::vector<std::string> lines = {"abc"};
  CHECK(LayOutDialog(lines, metrics).fragments.empty());
}

namespace {

// UNE IMAGE QUI N'EST PAS ENCORE LÀ, puis qui arrive : sa taille vit chez
// l'appelant, comme celle d'un téléchargement.
struct LateImage {
  ro::ui::DialogMediaSize size;
};

[[nodiscard]] ro::ui::DialogMediaSize MeasureLate(const DialogRun& /*media*/,
                                                  float /*available_width*/, void* owner) {
  return static_cast<const LateImage*>(owner)->size;
}

// Une icône dont le rapport change quand son image se charge.
[[nodiscard]] float AspectLate(std::uint32_t /*item_id*/, void* owner) {
  return *static_cast<const float*>(owner);
}

}  // namespace

TEST_CASE("Mise en page : une image arrivée après la page la fait refaire", "[ui][npc]") {
  // 🔴 LA PAGE NE SE REFAISAIT QU'AU CHANGEMENT DE PAGE, DE LARGEUR OU DE
  // POLICE : une image mesurée nulle au premier passage restait absente.
  LateImage image;
  DialogMetrics metrics = Ruler(100.0F);
  metrics.media_measure = MeasureLate;
  metrics.media_owner = &image;
  const std::vector<std::string> lines = {"<IMG>ressource\\carte.bmp</IMG>"};

  const DialogLayout before = LayOutDialog(lines, metrics);
  CHECK(before.fragments.empty());
  CHECK_FALSE(ro::ui::DialogMeasuresChanged(before, metrics));

  image.size = ro::ui::DialogMediaSize{.width = 40.0F, .height = 20.0F};
  CHECK(ro::ui::DialogMeasuresChanged(before, metrics));
  const DialogLayout after = LayOutDialog(lines, metrics);
  REQUIRE(after.fragments.size() == 1);
  CHECK(after.fragments[0].width == 40.0F);
  CHECK_FALSE(ro::ui::DialogMeasuresChanged(after, metrics));
}

TEST_CASE("Mise en page : une icône chargée après la page la fait refaire", "[ui][npc]") {
  float aspect = 0.0F;
  DialogMetrics metrics = Ruler(100.0F);
  metrics.icon_aspect = AspectLate;
  metrics.icon_owner = &aspect;
  const std::vector<std::string> lines = {"^i[501]"};
  const DialogLayout before = LayOutDialog(lines, metrics);
  CHECK_FALSE(ro::ui::DialogMeasuresChanged(before, metrics));
  aspect = 1.0F;
  CHECK(ro::ui::DialogMeasuresChanged(before, metrics));
}
