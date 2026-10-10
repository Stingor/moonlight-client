#pragma once

// CE QU'UN OBJET AU SOL FAIT DE LUI-MÊME : il TOMBE en arrivant, puis il
// SCINTILLE tant qu'il reste là.
//
// Deux mouvements sans rapport, que le client rejoue à chaque image pour
// chaque objet au sol :
//
//     t = (maintenant - apparition) / 24             l'âge, en TICS de 24 ms
//     éclat = (int)t % 92 >= 90                      le scintillement
//     si chute_en_cours :                            la chute, si elle court
//       hauteur = (0.083 * t + vitesse) * t + origine
//       posé dès que la hauteur passe sous le terrain, et la chute s'arrête
//
// La chute s'arme à la naissance, mais SEULEMENT quand le paquet la demande :
//
//     hauteur = sol - 15.0     l'objet naît en l'air
//     vitesse = -0.6           sa vitesse initiale, vers le HAUT
//     chute_en_cours = vrai
//     origine = sol - 15.0     l'origine de la parabole
//
// 🔴 CE QUI DISTINGUE L'OBJET QUI TOMBE DE CELUI QUI EST DÉJÀ LÀ EST LE PAQUET,
// et rien d'autre : `ZC_ITEM_FALL_ENTRY5 0x0ADD` arme la chute,
// `ZC_ITEM_ENTRY 0x009D` — l'objet déjà posé qu'on découvre en entrant dans la
// zone — ne l'arme pas. Le scintillement, lui, ne fait pas cette différence :
// il court pour les deux, depuis l'instant où l'objet est APPARU À L'ÉCRAN.
//
// ⚠ LE CLIENT NE DÉPLACE JAMAIS UN OBJET AU SOL HORIZONTALEMENT. Sa chute est
// strictement verticale — elle ne touche ni à X ni à Z. Le jaillissement en
// arc est un AJOUT, décrit à part : voir
// `ItemDropSettings`.

#include <array>
#include <cstdint>
#include <string_view>

namespace ro::render {

// Le tic du client, en millisecondes : le client divise l'âge d'un objet au
// sol par cette valeur avant toute autre chose, et les deux mouvements
// comptent dans cette unité.
inline constexpr float kItemTickMs = 24.0F;

// ── LA CHUTE ────────────────────────────────────────────────────────────────

// De combien l'objet naît AU-DESSUS de sa case, en unités du monde. Une case
// en fait cinq (`kWorldUnitsPerCell`) : l'objet naît donc trois cases plus
// haut.
inline constexpr float kItemFallBirthUnits = 15.0F;

// Sa vitesse initiale, en unités par tic. ⚠ ELLE EST DIRIGÉE VERS LE HAUT —
// chez le client l'ordonnée DESCEND, et la constante y vaut -0.6. L'objet
// commence donc par s'élever un peu avant de retomber.
inline constexpr float kItemFallRiseUnitsPerTick = 0.6F;

// Ce qui le ramène, en unités par tic au carré.
inline constexpr float kItemFallPullUnitsPerTick2 = 0.083F;

// De combien l'objet est AU-DESSUS du sol de sa case, `elapsed_ms` après sa
// naissance. Zéro une fois posé — la chute ne repart jamais.
//
// ⚠ LE SIGNE EST LE NÔTRE : le client compte vers le bas, cette fonction vers
// le haut. Un appelant qui ajouterait la valeur du client à une ordonnée
// enfoncerait l'objet sous le terrain.
[[nodiscard]] float ItemFallUpUnits(std::uint32_t elapsed_ms) noexcept;

// Vrai quand la chute est finie et que l'objet repose sur sa case.
[[nodiscard]] bool ItemFallIsOver(std::uint32_t elapsed_ms) noexcept;

// ── LE SCINTILLEMENT ────────────────────────────────────────────────────────

// La période du scintillement et le premier tic ALLUMÉ, tous deux en tics :
// `(int)t % 92 >= 90` laisse deux tics allumés sur quatre-vingt-douze, soit
// 48 ms de couleur toutes les 2208 ms.
//
// 🔴 CE N'EST PAS UNE DURÉE DE VIE QUI S'ÉCOULE. La période est FIXE et le
// client ne connaît pas l'échéance de l'objet — `flooritem_lifetime` est un
// réglage du SERVEUR, que rien n'annonce sur le fil. L'éclat bat au même
// rythme à la première seconde qu'à la dernière.
inline constexpr std::uint32_t kItemSparklePeriodTicks = 92;
inline constexpr std::uint32_t kItemSparkleFirstLitTick = 90;

// LA COULEUR DE L'ÉCLAT : le jaune opaque que le client pose une fois pour
// toutes à la création de l'objet — 0xFFFFFF00, lu alpha, rouge, vert, bleu.
//
// ⚠ ELLE REMPLACE, ELLE NE MULTIPLIE PAS. Le client écrit ce mot ENTIER dans
// la couleur du quad au lieu de composer celle du
// calque avec celle de l'acteur. Sur un objet au sol les deux reviennent au
// même — sa teinte d'acteur est blanche —, mais un calque coloré par son .act
// perdrait sa couleur chez le client et la garderait chez qui multiplierait.
inline constexpr std::array<std::uint8_t, 4> kItemSparkleTint = {255, 255, 0, 255};

// Vrai aux tics où l'objet porte la couleur de l'éclat.
[[nodiscard]] bool ItemSparklesAt(std::uint32_t elapsed_ms) noexcept;

// ── LE JAILLISSEMENT ────────────────────────────────────────────────────────

// CE QUI REMPLACE LA CHUTE VERTICALE : un objet qui part de CELUI QUI L'A
// LÂCHÉ, s'élève, retombe sur sa case et rebondit deux fois.
//
// 🔴 CE N'EST PAS DU CLIENT, et c'est assumé : le natif ne fait jaillir
// personne. Ses quatre réglages sont ci-dessous, avec leurs défauts réglés à
// l'œil en jeu.
//
// ⚠ IL VIT ICI ET NON DANS `app`, avec les formules qui le lisent : le panneau
// de réglages doit pouvoir le déclarer à son fichier sans traîner derrière lui
// le monde et la carte.
struct ItemDropSettings {
  // Le jaillissement, contre la chute verticale du client. ⚠ ALLUMÉ PAR DÉFAUT :
  // c'est un embellissement sans effet de jeu, sans
  // touche à soi et sans réseau.
  bool arc = true;
  // La crête de l'arc, en unités du monde. Vingt : le double du saut d'un
  // personnage, et un tiers de plus que la hauteur de naissance du natif — ce
  // qui se lit comme un jaillissement plutôt que comme une chute.
  float height_units = 20.0F;
  // De combien l'objet part À CÔTÉ de sa case quand personne ne l'a lâché à
  // portée. Même unité que la hauteur : l'unité du monde, PAS une fraction de
  // case. À zéro, l'objet monte et retombe droit.
  float kick_units = 12.0F;
  // OÙ, SUR LE CORPS DE QUI L'A LÂCHÉ, le butin naît : zéro aux pieds, un au
  // sommet de la tête, un demi à mi-hauteur.
  //
  // 🔴 UNE FRACTION ET NON UNE DISTANCE. Le client lui-même fait naître son
  // butin À CÔTÉ du corps : il le lâche quinze unités au-dessus de sa CASE,
  // sans rien savoir de qui l'a lâché. Partir des PIEDS de celui-ci ne vaut pas
  // mieux, et une hauteur fixe ne vaut pour personne —
  // quarante-neuf pixels passent au-dessus de la tête d'un Poring et sous les
  // genoux d'un Baphomet. La taille du sprite est la seule mesure qui vaille
  // pour tous.
  //
  // ⚠ ELLE DÉCROÎT AVEC L'AVANCE, sans quoi l'objet finirait en l'air : à
  // l'arrivée il ne reste que le sol de la case.
  float source_height_fraction = 0.5F;
  // La durée du vol, rebonds NON compris. Le natif met environ 420 ms à tomber
  // tout droit.
  std::uint32_t duration_ms = 750;
};

// Les deux rebonds, en fractions de la durée du vol et de la crête de l'arc.
// Un objet qui retombe et s'arrête net a l'air collé au sol ; deux rebonds
// courts suffisent à le poser. Ils se font SUR PLACE — la case d'arrivée est
// déjà atteinte.
inline constexpr float kItemBounce1Duration = 0.32F;
inline constexpr float kItemBounce1Height = 0.22F;
inline constexpr float kItemBounce2Duration = 0.18F;
inline constexpr float kItemBounce2Height = 0.05F;

// La durée de vol minimale : en deçà l'arc n'est qu'un saut d'une image, et
// diviser par la durée n'a plus de sens.
inline constexpr std::uint32_t kItemArcMinDurationMs = 60;

// CE QU'UN CURSEUR DE RÉGLAGE OFFRE. ⚠ CE NE SONT PAS DES BORNES DE VALIDITÉ :
// la formule accepte tout, et c'est le réglage seul qu'on borne. Mal réglé,
// l'arc donne un butin qui décolle ou qui ne bouge plus — ces bornes sont ce
// au-delà de quoi le résultat cesse de ressembler à du butin qui tombe.
inline constexpr float kItemArcLowestHeightUnits = 0.0F;
inline constexpr float kItemArcHighestHeightUnits = 60.0F;
inline constexpr float kItemArcHighestKickUnits = 40.0F;
inline constexpr float kItemArcHighestSourceFraction = 1.0F;

// LA HAUTEUR QU'ON PRÊTE À UN CORPS QU'ON N'A PAS ENCORE VU, en pixels de
// sprite. Un lâcheur qui n'a jamais été composé — un objet qui tombe à l'instant
// où l'on arrive — n'a pas de taille connue ; celle d'un personnage ordinaire
// vaut mieux que zéro, qui ferait sortir le butin du sol.
inline constexpr float kItemArcDefaultBodyPixels = 90.0F;
inline constexpr std::uint32_t kItemArcHighestDurationMs = 2000;

// OÙ EN EST L'ARC À UN INSTANT.
struct ItemArcStep {
  // L'AVANCE du départ vers l'arrivée : zéro au point de lâcher, un sur la
  // case. Elle décélère — l'objet est éjecté vite puis se pose.
  float advance = 1.0F;
  // De combien l'objet est au-dessus du sol, en unités du monde.
  float up_units = 0.0F;
  // Vrai quand l'arc et ses rebonds sont finis : l'objet repose sur sa case, et
  // il n'y a plus rien à calculer pour lui.
  bool over = false;
};

// Vrai quand l'arc et ses rebonds sont finis.
//
// 🔴 ELLE NE DEMANDE PAS LE DÉPART, ET C'EST TOUT SON OBJET. La fin d'un arc ne
// dépend que du temps ; la faire lire dans `ItemArcAt` obligerait à connaître
// d'abord d'où le butin est parti — donc à CHERCHER son lâcheur pour un butin
// posé depuis une minute, à chaque image, sur chaque butin de la carte.
[[nodiscard]] bool ItemArcIsOver(const ItemDropSettings& settings,
                                 std::uint32_t elapsed_ms) noexcept;

// Où en est l'arc `elapsed_ms` après la naissance de l'objet.
//
// `source_up_units` est la hauteur à laquelle le butin naît sur le corps de qui
// l'a lâché, déjà convertie : c'est l'APPELANT qui connaît la taille de ce
// corps, et la fraction seule ne dirait rien ici.
[[nodiscard]] ItemArcStep ItemArcAt(const ItemDropSettings& settings, std::uint32_t elapsed_ms,
                                    float source_up_units) noexcept;

// ── LE PILIER ───────────────────────────────────────────────────────────────

// CE QUE LE SERVEUR PEUT FAIRE JAILLIR AVEC UN BUTIN : une colonne de lumière
// colorée, pour qu'un objet rare se voie de loin.
//
// `ZC_ITEM_FALL_ENTRY5 0x0ADD` la demande par deux champs, `showdropeffect` et
// `dropeffectmode`, et le client la traduit en identifiant d'effet par un
// switch de cinq cas.
//
// 🔴 CES IDENTIFIANTS NE SONT DANS AUCUNE DE NOS DEUX TABLES D'EFFETS, et c'est
// ce qui les rendrait invisibles : ils ne passent NI par le résolveur de
// chemins des effets, NI par le répartiteur procédural. Le client les inscrit
// sous une classe d'effet à part, qui nomme ses fichiers en dur. Chercher un
// pilier dans les tables d'effets ne peut que conclure « identifiant
// inemployé ».
//
// ⚠ LES NOMS DU SERVEUR S'ACCORDENT, MAIS SEULEMENT SOUS SON PACKETVER.
// `e_item_drop_effect` (`src/map/itemdb.hpp`) est découpée par des `#if
// PACKETVER < 20200304` : avant cette date elle offre `WHITE` et `ORANGE`, après
// elle les retire au profit de `GREEN` et `RED`. Sous les 20250716 de ce
// serveur, les modes 1 à 5 — `dropEffect - 1` — sont donc BLUE, YELLOW, PURPLE,
// GREEN, RED, exactement ce que le client charge.
//
// 🔴 LIRE CETTE ÉNUMÉRATION SANS SES `#if` DONNE CINQ NOMS DÉCALÉS, et le
// décalage ne se voit pas : un pilier paraît quand même, d'une autre couleur.
// C'est pourquoi l'appariement est éprouvé par les NOMBRES du switch du client,
// et non par les noms du serveur.

// Combien de modes le serveur peut demander — donc combien de couleurs.
inline constexpr std::uint16_t kItemDropPillarCount = 5;

// Les cinq identifiants d'effet, dans l'ordre des modes 1 à 5 du serveur.
inline constexpr std::uint16_t kItemDropPillarBlue = 1869;
inline constexpr std::uint16_t kItemDropPillarYellow = 1189;
inline constexpr std::uint16_t kItemDropPillarPurple = 1190;
inline constexpr std::uint16_t kItemDropPillarGreen = 1870;
inline constexpr std::uint16_t kItemDropPillarRed = 1871;

// CE QU'UN PILIER DEMANDE À L'INSTALLATION : deux fichiers d'effet et un son.
//
// ⚠ DEUX FICHIERS, ET LES DEUX SONT NÉCESSAIRES. La classe en charge un pour la
// colonne et un pour la flaque à son pied (« _bottom ») ; n'en jouer qu'un
// laisse la colonne flotter au-dessus d'un sol nu.
struct ItemDropPillar {
  // Relatifs à `kEffectFolder`, écrits comme dans l'archive.
  std::string_view top;
  std::string_view bottom;
  // Relatif à `kSoundFolder`.
  std::string_view sound;
};

// L'identifiant d'effet d'un MODE du serveur, ou ZÉRO hors de 1 à 5 — le client
// garde alors son butin muet.
[[nodiscard]] std::uint16_t ItemDropPillarEffectId(std::uint16_t mode) noexcept;

// Les ressources d'un identifiant de pilier.
//
// ⚠ UN IDENTIFIANT INCONNU REND LE ROSE, et ce n'est pas une commodité : c'est
// le DÉFAUT de la classe du client, qui n'a aucun chemin d'échec. Ce rose-là ne
// s'atteint pas par un drop — les cinq modes couvrent les cinq couleurs —, mais
// par les autres identifiants que le client inscrit sous la même classe.
[[nodiscard]] ItemDropPillar ItemDropPillarOf(std::uint16_t effect_id) noexcept;

}  // namespace ro::render
