#include "chat/emoji_catalog.h"

#include <array>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// ── 🔴 POURQUOI DES ÉCHAPPEMENTS `\U…` ET PAS LES EMOJI EN CLAIR ──────
//
// Les sources de ce dépôt sont en UTF-8, et MSVC les lit dans la page de codes
// du système tant qu'on ne lui passe pas `/utf-8`. Les accents des commentaires
// y survivent par chance — chaque octet d'un « é » existe aussi en 1252, donc il
// traverse inchangé.
//
// LES EMOJI, EUX, N'ONT PAS CETTE CHANCE. « \U0001F60D » s'encode F0 9F 98 8D, et
// l'octet 0x8D n'est assigné À AUCUN caractère en 1252 : le compilateur le
// remplace par « ? », avec un avertissement noyé dans le bruit du build.
// L'emoji écrit en clair arriverait donc corrompu dans le binaire, et le losange
// à l'écran enverrait chercher le défaut du côté de la police — là où il n'est pas.
//
// L'échappement, lui, ne dépend d'aucun encodage de fichier. Le nom en fin de
// ligne remplace la lisibilité perdue.
//
// ⚠ DEUX RÈGLES POUR AJOUTER UNE ENTRÉE :
//   1. PAS de sélecteur de variante U+FE0F, même si les tables Unicode l'écrivent.
//      La couleur vient de la POLICE, pas de lui, et il occuperait la largeur
//      d'un emoji entier.
//   2. PAS de séquence à plusieurs points de code — drapeaux (deux indicateurs
//      régionaux), familles et métiers assemblés au ZWJ, teintes de peau. ImGui ne
//      fait pas de façonnage : elles s'afficheraient en morceaux.

constexpr std::array<std::string_view, 80> kFaces = {
    "\U0001F600",  // souriant
    "\U0001F603",  // bouche ouverte
    "\U0001F604",  // yeux rieurs
    "\U0001F601",  // dents serrees
    "\U0001F606",  // yeux fermes
    "\U0001F605",  // sueur froide
    "\U0001F923",  // roule par terre
    "\U0001F602",  // larmes de joie
    "\U0001F642",  // leger sourire
    "\U0001F643",  // tete en bas
    "\U0001F609",  // clin d'oeil
    "\U0001F60A",  // yeux rieurs
    "\U0001F607",  // aureole
    "\U0001F970",  // trois coeurs
    "\U0001F60D",  // yeux en coeur
    "\U0001F929",  // yeux en etoile
    "\U0001F618",  // envoie un baiser
    "\U0001F61A",  // baiser yeux fermes
    "\U0001F60B",  // se regale
    "\U0001F61B",  // tire la langue
    "\U0001F61C",  // langue et clin d'oeil
    "\U0001F92A",  // fou
    "\U0001F61D",  // langue yeux fermes
    "\U0001F911",  // yeux en dollars
    "\U0001F917",  // fait un calin
    "\U0001F92D",  // main sur la bouche
    "\U0001F914",  // pensif
    "\U0001F910",  // bouche fermeture eclair
    "\U0001F928",  // sourcil leve
    "\U0001F610",  // neutre
    "\U0001F636",  // sans bouche
    "\U0001F60F",  // narquois
    "\U0001F612",  // pas amuse
    "\U0001F644",  // leve les yeux
    "\U0001F62C",  // grimace
    "\U0001F634",  // dort
    "\U0001F60C",  // soulage
    "\U0001F614",  // songeur
    "\U0001F62A",  // somnolent
    "\U0001F924",  // bave
    "\U0001F637",  // masque
    "\U0001F912",  // thermometre
    "\U0001F922",  // nausee
    "\U0001F92E",  // vomit
    "\U0001F975",  // a chaud
    "\U0001F60E",  // lunettes de soleil
    "\U0001F913",  // binoclard
    "\U0001F9D0",  // monocle
    "\U0001F615",  // confus
    "\U0001F61F",  // inquiet
    "\U0001F641",  // leger froncement
    "\U0001F62E",  // bouche ouverte
    "\U0001F632",  // stupefait
    "\U0001F633",  // rougit
    "\U0001F97A",  // yeux suppliants
    "\U0001F628",  // apeure
    "\U0001F630",  // sueur froide
    "\U0001F625",  // decu mais soulage
    "\U0001F622",  // pleure
    "\U0001F62D",  // pleure a chaudes larmes
    "\U0001F631",  // hurle de peur
    "\U0001F616",  // confondu
    "\U0001F61E",  // decu
    "\U0001F613",  // sueur froide
    "\U0001F629",  // las
    "\U0001F62B",  // fatigue
    "\U0001F971",  // baille
    "\U0001F624",  // triomphant
    "\U0001F621",  // furieux
    "\U0001F620",  // en colere
    "\U0001F92C",  // jure
    "\U0001F608",  // cornes
    "\U0001F47F",  // diablotin
    "\U0001F480",  // crane
    "\U00002620",  // crane et os
    "\U0001F4A9",  // crotte
    "\U0001F921",  // clown
    "\U0001F47B",  // fantome
    "\U0001F47D",  // extraterrestre
    "\U0001F916",  // robot
};

constexpr std::array<std::string_view, 20> kHands = {
    "\U0001F44D",  // pouce en l'air
    "\U0001F44E",  // pouce en bas
    "\U0001F44C",  // ok
    "\U0000270C",  // victoire
    "\U0001F91E",  // doigts croises
    "\U0001F91D",  // poignee de main
    "\U0001F44F",  // applaudit
    "\U0001F64C",  // deux mains levees
    "\U0001F64F",  // mains jointes
    "\U0001F4AA",  // biceps
    "\U0001F44B",  // salue
    "\U0000270B",  // main levee
    "\U0001F91A",  // dos de la main
    "\U0001F446",  // pointe en haut
    "\U0001F447",  // pointe en bas
    "\U0001F448",  // pointe a gauche
    "\U0001F449",  // pointe a droite
    "\U0000261D",  // index leve
    "\U0001F919",  // appelle-moi
    "\U0001F590",  // main ouverte
};

constexpr std::array<std::string_view, 24> kSymbols = {
    "\U00002764",  // coeur rouge
    "\U0001F9E1",  // coeur orange
    "\U0001F49B",  // coeur jaune
    "\U0001F49A",  // coeur vert
    "\U0001F499",  // coeur bleu
    "\U0001F49C",  // coeur violet
    "\U0001F5A4",  // coeur noir
    "\U0001F90D",  // coeur blanc
    "\U0001F494",  // coeur brise
    "\U0001F495",  // deux coeurs
    "\U0001F4AF",  // cent points
    "\U00002728",  // etincelles
    "\U00002B50",  // etoile
    "\U0001F31F",  // etoile brillante
    "\U0001F4AB",  // etourdi
    "\U000026A1",  // haute tension
    "\U0001F525",  // feu
    "\U0001F4A5",  // collision
    "\U00002757",  // exclamation
    "\U00002753",  // interrogation
    "\U00002705",  // coche
    "\U0000274C",  // croix
    "\U000026A0",  // avertissement
    "\U0001F6AB",  // sens interdit
};

constexpr std::array<std::string_view, 24> kAdventure = {
    "\U00002694",  // epees croisees
    "\U0001F6E1",  // bouclier
    "\U0001F3F9",  // arc et fleche
    "\U0001F5E1",  // dague
    "\U0001F4B0",  // sac d'argent
    "\U0001F48E",  // gemme
    "\U0001F9EA",  // eprouvette
    "\U0001F4DC",  // parchemin
    "\U0001F5DD",  // vieille cle
    "\U0001F451",  // couronne
    "\U0001F3AF",  // dans le mille
    "\U0001F3B2",  // de
    "\U0001F3C6",  // trophee
    "\U0001F947",  // medaille d'or
    "\U0001F381",  // cadeau
    "\U0001F514",  // cloche
    "\U0000231B",  // sablier
    "\U0001F9D9",  // mage
    "\U0001F409",  // dragon
    "\U0001F9B4",  // os
    "\U00002697",  // alambic
    "\U0001F52E",  // boule de cristal
    "\U0001FA84",  // baguette magique
    "\U0001F9DA",  // fee
};

constexpr std::array<std::string_view, 16> kNature = {
    "\U0001F431",  // chat
    "\U0001F436",  // chien
    "\U0001F43A",  // loup
    "\U0001F98A",  // renard
    "\U0001F43B",  // ours
    "\U0001F414",  // poule
    "\U0001F987",  // chauve-souris
    "\U0001F577",  // araignee
    "\U0001F338",  // fleur de cerisier
    "\U0001F339",  // rose
    "\U0001F33B",  // tournesol
    "\U0001F340",  // trefle
    "\U0001F319",  // croissant de lune
    "\U00002600",  // soleil
    "\U0001F308",  // arc-en-ciel
    "\U00002744",  // flocon
};

constexpr std::array<std::string_view, 12> kFood = {
    "\U0001F34E",  // pomme
    "\U0001F347",  // raisin
    "\U0001F356",  // viande
    "\U0001F357",  // cuisse de volaille
    "\U0001F35E",  // pain
    "\U0001F9C0",  // fromage
    "\U0001F37A",  // biere
    "\U0001F37B",  // chopes trinquees
    "\U0001F377",  // verre de vin
    "\U00002615",  // boisson chaude
    "\U0001F370",  // gateau
    "\U0001F36C",  // bonbon
};

constexpr std::array<std::string_view, 16> kObjects = {
    "\U0001F389",  // cotillon
    "\U0001F38A",  // confettis
    "\U0001F3B5",  // note de musique
    "\U0001F3B6",  // notes de musique
    "\U0001F4A4",  // ronfle
    "\U0001F4AC",  // bulle
    "\U0001F440",  // yeux
    "\U0001F9E0",  // cerveau
    "\U0001F680",  // fusee
    "\U0001F6E0",  // marteau et cle
    "\U0001F527",  // cle plate
    "\U0001F4E6",  // colis
    "\U0001F4BB",  // ordinateur
    "\U0001F4F1",  // telephone
    "\U0001F579",  // manette
    "\U0001F3AE",  // jeu video
};

}  // namespace

std::span<const EmojiCategory> EmojiCategories() {
  // 🔴 LES INTERTITRES SE TRADUISENT À CHAQUE APPEL, et la table se
  // refait donc ici. Posés à la définition, ils seraient évalués AVANT que le
  // catalogue ne soit lu, et resteraient français pour toujours.
  //
  // ⚠ DES LITTÉRAUX, ET C'EST OBLIGATOIRE : `Tr` sur autre chose n'entre dans
  // aucun catalogue, et `scripts/check_translations.py` le refuse. Un fichier
  // qui TRANSMET des libellés marqués peut être dispensé de la règle, mais
  // seulement s'il ne porte AUCUN littéral à lui — sinon ses propres textes
  // disparaîtraient du catalogue avec la dispense.
  static std::array<EmojiCategory, 7> shown{};
  shown[0] = EmojiCategory{.label = ro::core::Tr("Visages"), .items = kFaces};
  shown[1] = EmojiCategory{.label = ro::core::Tr("Mains"), .items = kHands};
  shown[2] = EmojiCategory{.label = ro::core::Tr("Symboles"), .items = kSymbols};
  shown[3] = EmojiCategory{.label = ro::core::Tr("Aventure"), .items = kAdventure};
  shown[4] = EmojiCategory{.label = ro::core::Tr("Nature"), .items = kNature};
  shown[5] = EmojiCategory{.label = ro::core::Tr("Nourriture"), .items = kFood};
  shown[6] = EmojiCategory{.label = ro::core::Tr("Objets"), .items = kObjects};
  return shown;
}

void RelabelEmojiGroups(std::span<ro::ui::ChatEmojiGroup> groups) {
  const std::span<const EmojiCategory> categories = EmojiCategories();
  for (std::size_t group = 0; group < groups.size() && group < categories.size(); ++group) {
    groups[group].label = categories[group].label;
  }
}

}  // namespace moonclient
