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

constexpr std::array<SurvivorsCodexPhraseRow, 163> kPhrases{{
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
    // ── Les monstres du désert ──
    {"1049", TrText("Un poussin du désert, le premier venu du sable.")},  // Picky
    {"1107", TrText("Un louveteau du désert, rapide et fragile.")},       // Baby Desert Wolf
    {"1047",
     TrText("Un œuf qui ne bouge ni ne frappe : un mur vivant, posé en anneau.")},  // Peco Peco Egg
    {"1097", TrText("Un œuf de fourmi, immobile, posé en anneau autour du héros.")},  // Ant Egg
    {"1019", TrText("Un grand oiseau coureur, solide pour son rang.")},               // Peco Peco
    {"1055", TrText("Un cactus en marche, lent mais coriace.")},                      // Muka
    {"1127", TrText("S'enterre par moments : ni visé, ni touché, ni touchant.")},     // Hode
    {"1001", TrText("Un scorpion vif qui arrive en essaim.")},                        // Scorpion
    {"1106", TrText("Chasse en meute : quand l'un vient, les autres suivent.")},      // Desert Wolf
    {"1095", TrText("Une fourmi ouvrière, que la reine appelle en nombre.")},         // Andre
    {"1160", TrText("Une fourmi soldat, plus vive que l'ouvrière.")},                 // Piere
    {"1105", TrText("La plus rapide des fourmis, qui vient en raid.")},               // Deniro
    {"1165", TrText("Un homme de sable, solide au cœur du désert.")},                 // Sandman
    {"1119", TrText("Un lézard à collerette, aux coups secs.")},                      // Frilldora
    {"1140", TrText("Un prêtre du feu que rien ne repousse.")},                       // Marduk
    {"1029", TrText("Une serpente des ruines ; le Pharaon l'appelle à la fin.")},     // Isis
    {"1032", TrText("Un chien mort-vivant, qui annonce la nuit de la pyramide.")},    // Verit
    {"1146", TrText("Chasse en meute dans le noir, et mord vite.")},                  // Matyr
    {"1164", TrText("Un mort lent qui frappe dur.")},                                 // Requiem
    {"1154", TrText("Un guerrier de feu, le plus robuste des communs du désert.")},   // Pasana
    // ── Les monstres communs de la neige ──
    {"1062", TrText("Un Poring en bonnet de Noël, le premier venu de la neige.")},  // Santa Poring
    {"1242", TrText("Un Poring d'eau, mou et lent.")},                              // Marin
    {"1265", TrText("Un biscuit vif, qui vient en raid.")},                         // Cookie
    {"1246", TrText("Un biscuit de Noël, plus solide que ses cousins.")},   // Christmas Cookie
    {"1776", TrText("Un petit glaçon vif, qui chasse en meute.")},          // Siroma
    {"1248", TrText("Un jouet à roulettes, lent et têtu.")},                // Cruiser
    {"1243", TrText("Une bête des neiges qui charge après une annonce.")},  // Sasquatch
    {"1778", TrText("Se tient à distance et lance des éclats de glace.")},  // Gazeti
    {"1782", TrText("Un loup du vent, qui chasse en meute.")},              // Roween
    {"1775", TrText("Un bloc de neige massif, aux coups lourds.")},         // Snowier
    {"1515", TrText("Le petit de Hatii, qui chasse en meute.")},            // Baby Hatii
    {"1777", TrText("Un titan de glace que rien ne repousse.")},            // Ice Titan
    {"1789", TrText("Un glaçon immobile, qui mure le passage.")},           // Iceicle
    // ── Les mini-boss et le boss ──
    {"1090", TrText("Le premier mini-boss : il appelle des Porings à la rescousse.")},  // Mastering
    {"1093", TrText("Un lapin qui bondit vers le héros, escorté de Lunatics.")},        // Eclipse
    {"1091", TrText("Une libellule qui s'éloigne dès qu'on la touche.")},  // Dragon Fly
    {"1092", TrText("Son hurlement presse le pas de ses loups.")},         // Vagabond Wolf
    {"1039", TrText("Le seigneur de l'arène : charge, invoque, fait trembler le sol. Sa chute est "
                    "la victoire.")},  // Baphomet
    // ── Les mini-boss et le boss du désert ──
    {"1159", TrText("Un poisson des sables qui charge après une annonce.")},       // Phreeoni
    {"1147", TrText("La reine de la fourmilière : elle appelle ses ouvrières.")},  // Maya
    {"1157",
     TrText("Le seigneur du désert : charge, invoque, fait trembler le sable, et une armure "
            "absorbe une part de chaque coup. Sa chute est la victoire.")},  // Pharaoh
    // ── Les mini-boss et le boss de la neige ──
    {"1250", TrText("La poupée de la fabrique : elle appelle des biscuits de Noël.")},  // Chepet
    {"1249", TrText("La boîte à surprises : elle lâche des biscuits.")},                // Myst Case
    {"1251", TrText("Il lève Storm Gust : ses cercles annoncés gèlent le héros qu'ils "
                    "touchent.")},                                         // Stormy Knight
    {"1319", TrText("Son hurlement presse le pas des petits de Hatii.")},  // Freezer
    {"1247",
     TrText("Il s'enfuit dès qu'on le touche ; rattrapé, il lâche une Old Blue Box.")},  // Antonio
    {"1252",
     TrText("Le seigneur des neiges : charge, appelle ses petits, et son souffle gèle le héros. "
            "Sa chute est la victoire.")},  // Hatii
    // ── Les monstres du labo de Lighthalzen ──
    {"1613",
     TrText("Un petit automate de métal, le premier venu des champs de Lighthalzen.")},  // Metaling
    {"1103", TrText("Un hérisson des champs, plus têtu que dangereux.")},                // Caramel
    {"1128", TrText("Un scarabée à corne qui fonce droit devant lui.")},                 // Horn
    {"1627", TrText("Un moustique qui vient en nuée.")},                                // Anopheles
    {"1628", TrText("Une taupe qui fouit sous le sol, puis ressurgit.")},               // Holden
    {"1215", TrText("Un ver qui se tient à distance et crache.")},                      // Stem Worm
    {"1680", TrText("Un oiseau de proie qui chasse en meute.")},                        // Hill Wind
    {"1682", TrText("Le nettoyeur du labo : son seau laisse une flaque à sa chute.")},  // Remover
    {"1652", TrText("Une chercheuse du labo, vive et brûlante.")},           // Egnigem Cenia
    {"1656", TrText("Un chercheur archer, qui décoche de loin.")},           // Kavach Icarus
    {"1655", TrText("Une prêtresse du labo, rapide au contact.")},           // Errende Ebecee
    {"1653", TrText("Un assassin du labo, qui bondit vers le héros.")},      // Wickebine Tres
    {"1654", TrText("Un forgeron massif que rien ne repousse.")},            // Armeyer Dinze
    {"1657", TrText("Une sorcière du labo, qui lance ses sorts de loin.")},  // Laurell Weinder
    {"1634", TrText("Un gardien chevalier qui charge après une annonce.")},  // Seyren Windsor
    {"1635",
     TrText("Un gardien assassin qui disparaît, puis reparaît au contact.")},   // Eremes Guile
    {"1636", TrText("Un gardien forgeron que rien ne repousse.")},              // Howard Alt-Eisen
    {"1638", TrText("Une gardienne archère, qui décoche de loin.")},            // Cecil Damon
    {"1639", TrText("Une gardienne sorcière, qui lance ses sorts de loin.")},   // Kathryne Keyron
    {"1681", TrText("Le robot du premier étage : il appelle des Metalings.")},  // Gemini-S58
    {"1658",
     TrText("La chercheuse devenue MVP : elle charge après une annonce.")},  // Egnigem Cenia MVP
    {"1642",
     TrText("Cart Termination : une longue charge que rien ne repousse.")},  // Whitesmith Howard
    {"1643", TrText("Elle presse le pas des prêtresses, et Magnus Exorcismus tombe en cercles "
                    "annoncés.")},  // High Priest Margaretha
    {"1641",
     TrText(
         "Il bondit vers le héros, et s'éloigne dès qu'on le touche.")},  // Assassin Cross Eremes
    {"1646", TrText("L'un des deux MVP du labo : charge, appelle ses chevaliers, et Bowling Bash "
                    "tombe en cercles. Sa chute est la victoire.")},  // Lord Knight Seyren
    {"1651", TrText("L'un des deux MVP du labo : Storm Gust gèle le héros, Meteor Storm tombe plus "
                    "loin et plus fort. Sa chute est la victoire.")},  // High Wizard Kathryne
    // ── Les plantes ──
    {"1078", TrText("Une plante qui lâche de la viande, pour reprendre des forces.")},  // Red Plant
    {"1081", TrText("Une plante qui lâche de l'or.")},                // Yellow Plant
    {"1080", TrText("Une plante qui lâche une potion de vitesse.")},  // Green Plant
    {"1079", TrText("Une plante qui lâche un manuel de combat.")},    // Blue Plant
    {"1083",
     TrText("Une plante rare qui lâche de quoi attirer toutes les gemmes.")},  // Shining Plant
    {"1082", TrText("La plus rare : elle lâche de l'eau bénite.")},            // White Plant
    {"1084", TrText("Un champignon que seule la carte Willow fait pousser : il lâche une "
                    "Yggdrasil Berry.")},  // Black Mushroom
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
    {"berry", TrText("Rend au héros tous ses PV.")},
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
    // ── Les événements du désert ──
    {"swarm_1107", TrText("Une meute de louveteaux traverse le sable.")},
    {"ring_1047", TrText("Des œufs de Peco Peco se posent en anneau et enferment le héros.")},
    {"swarm_1001", TrText("Un essaim de Scorpions arrive d'un même côté.")},
    {"ring_1097", TrText("Des œufs de fourmi se posent en anneau autour du héros.")},
    {"raid_1105", TrText("Un raid de Deniros charge le héros.")},
    {"raid_1106", TrText("Une meute de loups du désert charge le héros.")},
    {"ring_1041", TrText("Des momies referment un anneau autour du héros.")},
    // ── Les événements de la neige ──
    {"swarm_1062", TrText("Une nuée de Santa Porings dévale la neige.")},
    {"ring_1789", TrText("Des glaçons se dressent en anneau et enferment le héros.")},
    {"raid_1265", TrText("Un raid de biscuits charge le héros.")},
    {"swarm_1776", TrText("Une meute de Siromas arrive d'un même côté.")},
    {"raid_1515", TrText("Les petits de Hatii chargent le héros.")},
    {"ring_1777", TrText("Des titans de glace referment un anneau autour du héros.")},
    // ── Les événements du labo ──
    {"swarm_1627", TrText("Une nuée d'Anopheles traverse le labo.")},
    {"ring_1628", TrText("Des Holdens surgissent du sol en anneau autour du héros.")},
    {"raid_1613", TrText("Un raid de Metalings charge le héros.")},
    {"tanks_1652",
     TrText("Les cuves les plus proches cèdent : des chercheuses en sortent, dans l'acide.")},
    {"raid_1682", TrText("Les nettoyeurs chargent le héros, et laissent leurs flaques.")},
    {"tanks_1634", TrText("Les cuves cèdent encore : les gardiens chevaliers en sortent.")},
    {"ring_1635", TrText("Des gardiens assassins referment un anneau autour du héros.")},
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
