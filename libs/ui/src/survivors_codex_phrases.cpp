// LES PHRASES DU CODEX, une par entrée hors des sorts et des cartes, désignées
// par leur clé de Codex. Une phrase dit ce que fait l'entrée dans le mode, sans
// chiffre : les chiffres sont dans la fiche, tirés des règles.
//
// Une clé numérique est la classe RO d'un monstre ou d'une plante ; son nom
// suit en commentaire. Une entrée qui entre aux règles sans sa phrase, ou une
// phrase dont l'entrée en sort, fait échouer l'essai des fiches du Codex.

#include <array>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/survivors_codex_window.h"

#include "survivors_text.h"

namespace ro::ui {
namespace {

using core::TrText;

constexpr std::array<SurvivorsCodexPhraseRow, 73> kPhrases{{
    // ── Les monstres communs ──
    {"1063", TrText("Le premier venu : petit, rapide et fragile.")},                 // Lunatic
    {"1007", TrText("Une larve lente, qui vient en nombre dès le départ.")},         // Fabre
    {"1002", TrText("Rebondit droit sur le héros ; sa carte est la plus connue.")},  // Poring
    {"1113", TrText("Un Poring de feu, plus coriace que ses cousins.")},             // Drops
    {"1011", TrText("Une mouche vive qui arrive en nuée avant ses vagues.")},        // Chonchon
    {"1010", TrText("Un arbre animé, plus solide qu'il n'en a l'air.")},             // Willow
    {"1008",
     TrText("Un cocon qui ne bouge ni ne frappe : un mur vivant, posé en anneau.")},  // Pupa
    {"1052", TrText("Un criquet qui encaisse et frappe sec.")},                       // Rocker
    {"1009", TrText("Un oiseau rapide qui fond sur le héros.")},                      // Condor
    {"1167", TrText("Un petit sauvageon, lent mais tenace.")},                        // Savage Babe
    {"1004", TrText("Un frelon rapide qui pique en essaim.")},                        // Hornet
    {"1005",
     TrText("Une chauve-souris qui survole la horde pour atteindre le héros.")},  // Familiar
    {"1051",
     TrText(
         "Ramasse les gemmes laissées au sol, et rend leur expérience à sa mort.")},  // Thief Bug
    {"1014", TrText("Un champignon robuste, posé en anneau autour du héros.")},       // Spore
    {"1031", TrText("Laisse en mourant une flaque de poison qui blesse.")},           // Poporing
    {"1056", TrText("Disparaît par moments : ni visé, ni touché, ni touchant.")},     // Smokie
    {"1076", TrText("Le premier des morts-vivants, qui annonce la nuit.")},           // Skeleton
    {"1015", TrText("Lent, mais il faut frapper longtemps pour l'abattre.")},         // Zombie
    {"1077", TrText("Un champignon empoisonné, plus dur que le premier.")},       // Poison Spore
    {"1013", TrText("Chasse en meute : quand l'un vient, les autres suivent.")},  // Wolf
    {"1057", TrText("Un singe agile qui se faufile dans la horde.")},             // Yoyo
    {"1023", TrText("Un guerrier solide, qui tient la ligne de front.")},         // Orc Warrior
    {"1166", TrText("Fonce en ligne droite après s'être arrêté un instant.")},    // Savage
    {"1122", TrText("Ne vient qu'en raid, et frappe fort.")},                     // Goblin
    {"1139", TrText("Un insecte cuirassé aux coups tranchants.")},                // Mantis
    {"1152",
     TrText("Un squelette d'orc, plus dur que les morts-vivants d'avant.")},  // Orc Skeleton
    {"1016", TrText("Se tient à distance et tire après avoir visé ; il ne touche pas au "
                    "contact.")},                                      // Archer Skeleton
    {"1026", TrText("Une revenante qui frappe dur au contact.")},      // Munak
    {"1040", TrText("Un colosse de pierre que rien ne repousse.")},    // Golem
    {"1060", TrText("Un ours massif, lent mais redoutable.")},         // Bigfoot
    {"1035", TrText("Une mouche chasseresse qui survole la horde.")},  // Hunter Fly
    {"1041", TrText("Une momie lente qui encaisse beaucoup.")},        // Mummy
    {"1036", TrText("Un mort affamé, qui finit par cerner le héros en anneau.")},  // Ghoul
    {"1102", TrText("Bondit vers le héros après une courte annonce.")},            // Bathory
    {"1170", TrText("Un esprit d'eau qui hante la fin de la nuit.")},              // Sohee
    {"1179", TrText("Un fantôme qui traverse les murs et survole la horde.")},     // Whisper
    {"1061", TrText("Un destrier spectral qui charge en ligne droite.")},          // Nightmare
    {"1192", TrText("Un spectre lourd parmi les derniers venus.")},                // Wraith
    {"1117", TrText("Un druide maudit, le plus robuste des communs.")},            // Evil Druid
    {"1101", TrText("Ne naît que des invocations de Baphomet.")},                  // Baphomet Jr.
    // ── Les mini-boss et le boss ──
    {"1090", TrText("Le premier mini-boss : il appelle des Porings à la rescousse.")},  // Mastering
    {"1093", TrText("Un lapin qui bondit vers le héros, escorté de Lunatics.")},        // Eclipse
    {"1091", TrText("Une libellule qui s'éloigne dès qu'on la touche.")},  // Dragon Fly
    {"1092", TrText("Son hurlement presse le pas de ses loups.")},         // Vagabond Wolf
    {"1039", TrText("Le seigneur de l'arène : charge, invoque, fait trembler le sol. Sa chute est "
                    "la victoire.")},  // Baphomet
    // ── Les plantes ──
    {"1078", TrText("Une plante qui lâche de la viande, pour reprendre des forces.")},  // Red Plant
    {"1081", TrText("Une plante qui lâche de l'or.")},                // Yellow Plant
    {"1080", TrText("Une plante qui lâche une potion de vitesse.")},  // Green Plant
    {"1079", TrText("Une plante qui lâche un manuel de combat.")},    // Blue Plant
    {"1083",
     TrText("Une plante rare qui lâche de quoi attirer toutes les gemmes.")},  // Shining Plant
    {"1082", TrText("La plus rare : elle lâche de l'eau bénite.")},            // White Plant
    // ── Les objets ──
    {"chest",
     TrText(
         "Lâché par les élites et les mini-boss : un ou plusieurs gains, parfois une évolution.")},
    {"meat", TrText("Rend des PV au héros.")},
    {"gold", TrText("Des Zeny gardés au carnet à la fin de la partie.")},
    {"manual", TrText("Augmente l'expérience gagnée un moment.")},
    {"haste", TrText("Le héros marche plus vite un moment.")},
    {"holy_water",
     TrText("Abat d'un coup les monstres communs autour du héros ; les boss y résistent.")},
    {"magnet_all", TrText("Attire au héros toutes les gemmes du terrain.")},
    {"blue_gem", TrText("La plus petite gemme d'expérience.")},
    {"yellow_gem", TrText("Une gemme qui vaut plus d'expérience.")},
    {"red_gem", TrText("Une gemme d'expérience précieuse.")},
    {"emperium_gem", TrText("La gemme d'expérience la plus riche.")},
    {"card",
     TrText(
         "Une carte tombée d'un monstre : elle reste au sol jusqu'à la fin, et rejoint l'Album.")},
    // ── Les événements ──
    {"swarm_1011", TrText("Une nuée de Chonchons fond sur le héros.")},
    {"ring_1008", TrText("Des Pupas se posent en anneau et enferment le héros.")},
    {"swarm_1004", TrText("Un essaim de Hornets arrive d'un même côté.")},
    {"group_1051", TrText("Une bande de Thief Bugs vient voler les gemmes.")},
    {"ring_1014", TrText("Des Spores se posent en anneau autour du héros.")},
    {"swarm_1005", TrText("Une nuée de Familiars traverse la horde.")},
    {"raid_1122", TrText("Un raid de Goblins charge le héros.")},
    {"ring_1036", TrText("Des Ghouls referment un anneau autour du héros.")},
    {"elites", TrText("Un monstre de la vague, bien plus robuste, qui lâche une Old Blue Box.")},
    {"night", TrText("Le ciel s'assombrit jusqu'à minuit, et les morts-vivants se lèvent.")},
}};

}  // namespace

const char* SurvivorsCodexPhrase(std::string_view codex_key) {
  for (const SurvivorsCodexPhraseRow& row : kPhrases) {
    if (codex_key == row.key) return SurvivorsText(row.french);
  }
  return "";
}

std::span<const SurvivorsCodexPhraseRow> SurvivorsCodexPhrases() { return kPhrases; }

}  // namespace ro::ui
