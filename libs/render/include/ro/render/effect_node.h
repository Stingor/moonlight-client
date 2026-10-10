#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "ro/render/effect_node_fwd.h"

namespace ro::render {

// L'état d'un effet PROCÉDURAL, et son avancement d'un tic au suivant.
//
// Le client a deux familles d'effets. La première se décrit par un fichier —
// c'est `effect_frame.h` qui la joue. La seconde n'a aucun fichier : chaque
// effet a du code, et ce code ne DESSINE pas. Il remplit une structure — le
// NŒUD — dont un dessinateur commun tire deux triangles.
//
// Il n'y a donc pas cent dix rendus à porter, mais UN objet et son horloge.
// Voir `docs/adr/0014` ; le quad se compose dans `effect_scene.h`.
//
// Description PURE : aucune texture, aucun contexte graphique, aucun tirage
// caché. Le nœud porte sa propre graine, si bien qu'un même nœud rejoue
// toujours le même mouvement — c'est ce qui rend le tout vérifiable au banc.

// Une DEMI-EXTENSION du quad, et comment elle grandit.
//
// Trois étages, comme un mouvement : une accélération pousse une croissance,
// qui pousse la taille. C'est ainsi qu'un halo enfle puis ralentit sans
// qu'aucune courbe ne soit stockée nulle part.
// LA SENTINELLE DE LA RÉSERVE : « ce champ n'a pas été posé ».
//
// ⚠ ELLE EST AU CLIENT, PAS À NOUS. Son constructeur laisse dix millièmes dans
// chaque champ de la seconde image-clé, et son mouvement ne recopie que ceux qui
// ont changé. Zéro ne pouvait pas servir : un pas nul est une valeur, celle d'un
// quad qui cesse de s'ouvrir.
inline constexpr float kEffectNodeKeyframeUnset = 0.0001F;

// OÙ LES EXEMPLAIRES D'UN PATRON SE POSENT, les uns par rapport aux autres.
//
// ⚠ DEUX FORMES MESURÉES, ET PAS UNE DE PLUS. Le client place ses grilles par
// une double boucle et ses couronnes par un angle tiré ; inventer une troisième
// disposition « au cas où » serait inventer un effet.
enum class EffectNodeInstancePlacement : std::uint8_t {
  // UN SEUL, au point de l'effet. Le cas de presque tous les patrons.
  kSingle,
  // UNE GRILLE CARRÉE centrée sur le point, au pas donné. Vingt-cinq pics de
  // terre y tiennent : cinq rangs de cinq, de moins dix à dix par pas de cinq.
  kSquareGrid,
  // UN CERCLE du rayon donné, chaque exemplaire à un angle TIRÉ — et non à un
  // angle régulier. Le client tire le sien, si bien que six pointes ne sont
  // jamais également écartées.
  kRing,
  // DEUX, DE PART ET D'AUTRE DU POINT, le long de la NORMALE du nœud — l'axe que
  // son cap fait tourner. Les quatre murs d'EF_GLASSWALL en font deux paires :
  // le bâtisseur boucle sur plus un et moins un, et multiplie une colonne de sa
  // matrice de lacet par l'écart. Deux paires tournées d'un quart de tour l'une
  // par rapport à l'autre ferment une boîte autour du porteur.
  kMirroredPair,
  // EN LIGNE SUR LA PROFONDEUR, centrée sur le point : l'exemplaire k se pose à
  // `(k − (compte − 1) / 2) × écart × cos(a)` en profondeur, `a` tiré sur le tour
  // pour chacun. Trois éclats du mur de glace se serrent ainsi dans l'épaisseur
  // de leur cellule, plus ou moins selon l'angle que chacun tire.
  kDrawnDepthLine,
};

// UNE IMAGE-CLÉ D'ALPHA : à l'âge dit, le nœud REPREND cet alpha et ce
// plafond, puis sa rampe continue de là. Un âge négatif : pas d'image-clé.
struct EffectAlphaKeyframe {
  std::int32_t tick = -1;
  float alpha = 0.0F;
  float alpha_max = 0.0F;
};
inline constexpr std::size_t kEffectAlphaKeyframeCapacity = 2;

struct EffectNodeExtent {
  // Du centre au bord. L'UNITÉ est celle du dessinateur : le billboard les
  // ajoute telles quelles aux coordonnées d'écran, donc en pixels ; d'autres,
  // qui projettent un plan, les prennent en unités du monde. Les valeurs le
  // disent — quarante pour une rune d'écran, un cinquième pour une particule
  // projetée.
  float half_size = 0.0F;
  float growth_per_tick = 0.0F;
  float growth_acceleration_per_tick = 0.0F;
};

// Ce que le nœud fait de particulier, tel que le client le code en drapeaux.
//
// Le champ d'origine en porte d'autres, que nous ne savons pas encore lire :
// n'ajouter ici que ce dont le sens est établi.
inline constexpr std::uint32_t kEffectNodeAnchorsAtTop = 0x1;
// ⚠ LE MÊME BIT, ET IL DIT AUTRE CHOSE (ADR 0017). Sur une COURONNE il la rend
// PLEINE — un disque du centre au bord — au lieu de la creuser jusqu'au rayon à
// l'ancre. Le socle d'un point de téléportation est le nœud qui le pose : sans
// lui sa largeur vaudrait zéro.
inline constexpr std::uint32_t kEffectNodeFillsRing = 0x1;
inline constexpr std::uint32_t kEffectNodeFreezesGrowth = 0x2;
// ⚠ LE BIT DEUX DIT AUTRE CHOSE SELON LE MOUVEMENT QUI LE LIT (ADR 0017). Pour
// les sous-types quinze et seize, il ARRÊTE LE NŒUD AU SOL : quand son ordonnée
// passe sous la hauteur du terrain, le pas tombe à zéro, l'âge saute à la vie
// moins vingt-cinq, le bit s'efface et le bâtisseur est prévenu. C'est ainsi
// qu'une flèche se plante et que son anneau d'impact naît.
inline constexpr std::uint32_t kEffectNodeStopsAtGround = 0x4;
inline constexpr std::uint32_t kEffectNodeWandersAngle = 0x8;
// LE QUAD SE DRESSE FACE À LA CAMÉRA : son mouvement bâtit alors sa matrice
// depuis le lacet de la caméra plus un quart de tour, et son inclinaison.
//
// ⚠ SANS CE BIT, LE QUAD S'ORIENTE PAR SES PROPRES ANGLES, comme toute autre
// figure — et c'est le cas ordinaire : les quatre murs d'EF_GLASSWALL portent
// 0x28 et sont quatre plans fixes du monde. Le tenir pour un billboard par
// défaut les posait tous dans le même sens.
inline constexpr std::uint32_t kEffectNodeFacesCamera = 0x40;
// LA HAUTEUR BAT : sous ce bit, le mouvement des sous-types quinze et seize
// retourne le signe du pas de demi-hauteur tous les `height_flip_period_ticks`.
// C'est ce qui fait PULSER le haut des murs de Safety Wall : un quart d'unité
// par tic pendant cinquante tics, dans un sens puis dans l'autre. Sans lui, le
// mur grimpait jusqu'au plafond de cent.
inline constexpr std::uint32_t kEffectNodeHeightGrowthFlips = 0x20;
inline constexpr std::uint32_t kEffectNodeAlphaBounces = 0x80;
// ⚠ LE BIT 0x200 DIT AUTRE CHOSE SELON LE MOUVEMENT QUI LE LIT (ADR 0017) : pour
// le dessin d'un sprite, il garde la teinte du calque ; pour le mouvement des
// sous-types quinze et seize, il penche le quad de l'altitude de la caméra —
// voir `kEffectOrientedQuadLeansByCameraAltitude`.
inline constexpr std::uint32_t kEffectNodeKeepsLayerColor = 0x200;
// L'ACTION DE CHAQUE QUART DE TOUR, pour un sprite qui montre sa direction par
// son image : voir `EffectAimQuadrantAction`. Celle de la flèche verte par défaut.
using EffectActionsByQuarter = std::array<std::int8_t, 4>;
inline constexpr EffectActionsByQuarter kEffectArrowActionsByQuarter = {0, 6, 4, 2};
// ⚠ LE BIT 0x800 DIT AUTRE CHOSE SELON LE MOUVEMENT QUI LE LIT (ADR 0017) : il
// fige une animation sur sa dernière image ailleurs, et pour les sous-types
// quinze et seize il fait TOURNER la teinte — voir `EffectNode::cycles_color`.
inline constexpr std::uint32_t kEffectNodeCyclesColor = 0x800;

// VRAI quand le quad du nœud se dresse face à la caméra — voir
// `kEffectNodeFacesCamera`.
[[nodiscard]] constexpr bool EffectNodeFacesCamera(std::uint32_t flags) {
  return (flags & kEffectNodeFacesCamera) != 0U;
}

// VRAI quand la couronne du nœud est PLEINE — voir `kEffectNodeFillsRing`.
[[nodiscard]] constexpr bool EffectNodeFillsRing(std::uint32_t flags) {
  return (flags & kEffectNodeFillsRing) != 0U;
}

// L'alpha et les couleurs du client vont de zéro à 255, en FLOTTANT : le nœud
// les fait monter par pas fractionnaires, et ce n'est qu'au moment de peindre
// qu'ils deviennent des octets.
inline constexpr float kEffectNodeChannelMax = 255.0F;

// LES DÉFAUTS SONT CEUX QUE LE CLIENT DONNE À UN NŒUD NEUF. Ce ne sont pas des
// valeurs de commodité : un effet qui ne pose pas un champ COMPTE sur celle-ci,
// et en choisir une autre change ce qu'il montre sans que rien ne le dise.
//
// Le plafond d'alpha s'arrête à 254 et non à 255, et c'est bien ce que le
// client écrit.
inline constexpr float kEffectNodeDefaultAlphaMax = 254.0F;
inline constexpr std::int32_t kEffectNodeDefaultLifeTicks = 100;

// CE QUE LE CLIENT ÉCRIT POUR « SANS FIN ».
//
// ⚠ ZÉRO N'EST PAS L'INFINI, C'EST LE CONTRAIRE. La queue du répartiteur de
// mouvement tue dès que l'âge DÉPASSE la vie, et un âge d'un dépasse une vie de
// zéro : un nœud sans vie meurt à son premier tic. L'infini s'écrit avec ce
// nombre-ci, et le client le pose en clair : deux cent trente-cinq effets le
// portent dans la table des durées.
inline constexpr std::int32_t kEffectLifeForever = 99999999;

// LE FONDU QUE L'INSTANCE IMPOSE N'EST PAS ENCORE JUGÉ : la mise à jour de
// l'alpha le jugera. Voir `EffectNode::owner_fade_at_age`.
inline constexpr std::int32_t kEffectNodeOwnerFadeUnjudged = -1;
// ET IL NE VIENDRA PAS : le semis l'a jugé, contre une instance dont la troupe
// ne lit pas le reste à chaque mise à jour.
inline constexpr std::int32_t kEffectNodeOwnerFadeNever = std::numeric_limits<std::int32_t>::max();
inline constexpr std::int32_t kEffectNodeDefaultFrameHoldTicks = 4;
inline constexpr float kEffectNodeDefaultHalfHeightCeiling = 100.0F;
inline constexpr float kEffectNodeDefaultRingStepDegrees = 36.0F;

// COMBIEN DE TICS AVANT LA FIN la PLONGÉE commence.
//
// Le client l'écrit en dur — `vie moins trente` — dans le seul mouvement qui
// s'en sert. Ce n'est donc pas un champ du nœud mais une constante du
// mouvement, et elle se lit ici plutôt que de se retrouver semée dans une
// table.
inline constexpr std::int32_t kEffectNodeSinkingLeadTicks = 30;

// LE CYCLE QUI RELANCE LE PAS, et ce qu'il pose à chacune de ses deux phases.
//
// ⚠ C'EST LE SEUL ÉCART ENTRE LE SOUS-TYPE 9 ET LE SOUS-TYPE 117, et les deux
// partagent tout le reste — même figure, même translation le long de l'axe, même
// accumulation. Là où la pointe de terre bascule UNE FOIS, à son tic d'image-clé,
// la pierre de gravitation REBASCULE tous les six tics : trois tics de descente
// à ce que sa réserve porte, trois tics de montée à ce que le cas écrit en dur.
// Le bilan est presque nul, et c'est ce qui la fait FLOTTER en vibrant.
//
// ⚠ LA RÉSERVE N'EST DONC PAS UNE IMAGE-CLÉ POUR CE MOUVEMENT-LÀ. C'est l'ADR
// 0017 dans le modèle du nœud : le cas 117 ne lit jamais le tic de bascule, et
// les deux champs de la réserve y sont le RETOUR du cycle. Les faire jouer
// comme une bascule les consommerait au douzième tic, après quoi le cycle ne
// relancerait plus rien.
inline constexpr std::int32_t kEffectNodeStepCycleTicks = 6;
inline constexpr std::int32_t kEffectNodeStepCycleReturnPhase = 2;
inline constexpr std::int32_t kEffectNodeStepCycleLaunchPhase = 5;
inline constexpr float kEffectNodeStepCycleLaunchStep = 1.18F;
inline constexpr float kEffectNodeStepCycleLaunchChange = 0.01F;
// LE DERNIER TIC OÙ LE CYCLE JOUE, et il en porte le retour une fois de plus.
//
// ⚠ AUCUN EFFET RELEVÉ NE L'ATTEINT : celui du sous-type 117 vit deux cent
// cinquante tics. Le transcrire quand même coûte une ligne, et le taire ferait
// croire que le cycle est sans fin.
inline constexpr std::int32_t kEffectNodeStepCycleLastTick = 550;

// Le mouvement d'errance tire deux nombres par secousse : de combien l'angle
// dévie, et dans combien de tics recommencer.
inline constexpr std::int32_t kEffectNodeWanderSpanDegrees = 16;
inline constexpr std::int32_t kEffectNodeWanderPeriodSpanTicks = 10;
inline constexpr std::int32_t kEffectNodeWanderPeriodFloorTicks = 10;

struct EffectNode {
  // La TAILLE du quad. `half_width` court le long de l'axe u de la texture,
  // `half_height` le long de v.
  //
  // Un nœud d'ANNEAU emploie `half_height` autrement : elle y devient l'ÉCART
  // entre les deux cercles de la bande. C'est le même champ dans le client, le
  // même intégrateur, et le dessinateur seul décide de ce qu'il en fait.
  EffectNodeExtent half_width;
  // LA SECONDE IMAGE-CLÉ, et le tic où elle remplace la première.
  //
  // ⚠ CE N'EST PAS UN PAS QUI CHANGE MAIS UNE RÉSERVE QUI REMPLACE. Au tic dit,
  // le client recopie ses champs vers les champs vivants — chacun seulement s'il
  // a été posé, ce que dit `kEffectNodeKeyframeUnset`. Un quad qui s'ouvre vite
  // puis ralentit se décrit ainsi, et non par une courbe.
  //
  // Un tic NÉGATIF veut dire qu'il n'y a pas de seconde image.
  std::int32_t second_keyframe_tick = -1;
  float second_half_width_growth = kEffectNodeKeyframeUnset;
  float second_half_width_acceleration = kEffectNodeKeyframeUnset;
  float second_half_height_growth = kEffectNodeKeyframeUnset;
  float second_half_height_acceleration = kEffectNodeKeyframeUnset;
  // ⚠ ET LE PAS EN EST, DEPUIS QU'IL A ÉTÉ MESURÉ. Le client remplace NEUF
  // champs à la bascule ; nous en portons SIX. Les deux derniers sont le pas du
  // nœud et sa variation — ce qui fait qu'une pointe de terre jaillit puis
  // REDESCEND, son pas passant de un à moins un et deux dixièmes. Sans eux, la
  // pointe montait et restait.
  float second_step_length = kEffectNodeKeyframeUnset;
  float second_step_length_change = kEffectNodeKeyframeUnset;
  EffectNodeExtent half_height;

  // Le PLAFOND de la demi-hauteur : l'intégrateur s'y arrête, il ne le franchit
  // pas. Cent par défaut, comme dans le client — assez haut pour qu'un nœud qui
  // ne s'en sert pas ne le rencontre jamais.
  float half_height_ceiling = kEffectNodeDefaultHalfHeightCeiling;

  // Les DEUX RAYONS d'un anneau, chacun avec ses trois étages comme les
  // extensions : celui du cercle posé au plan du nœud, et celui du cercle posé
  // à `half_height` de là. Un tronc de cône quand ils diffèrent, un cylindre
  // quand ils sont égaux, un disque quand la hauteur est nulle.
  //
  // ⚠ `radius_at_anchor` sert AUSSI de PLAFOND à la largeur d'une couronne
  // plate, et c'est le client qui le veut ainsi : le mouvement de la couronne
  // écrit sa largeur comme le plus petit de ce rayon et de `ring_radius`. Un
  // champ, deux emplois, selon le dessinateur — comme `half_height`.
  EffectNodeExtent radius_at_anchor;
  EffectNodeExtent radius_at_height;

  // Le RAYON D'UNE COURONNE PLATE, qui s'étend par ses trois étages : c'est
  // l'onde de choc de Magnum Break, qui part du lanceur et s'écarte de
  // presque deux unités par tic.
  //
  // Distinct des deux précédents parce que le client le range ailleurs et le
  // fait intégrer par un autre mouvement. Les confondre mêlerait la largeur
  // d'une couronne au rayon de son cercle.
  EffectNodeExtent ring_radius;

  // De combien l'anneau avance à chaque segment, en degrés : le tour se découpe
  // en `360 / ring_step_degrees` quads. TRENTE-SIX par défaut — dix segments —,
  // et c'est le constructeur du client qui le pose, pas ses effets : quatre
  // seulement le changent.
  float ring_step_degrees = kEffectNodeDefaultRingStepDegrees;

  // La TRANCHE de texture qu'un quad orienté prend en ordonnée. L'image entière
  // par défaut ; une tranche plus étroite permet d'empiler plusieurs motifs
  // dans une même planche.
  float texture_v_min = 0.0F;
  float texture_v_max = 1.0F;

  // La ROTATION du quad, elle aussi à trois étages : un quad peut donc partir
  // lentement et s'emballer.
  float spin_degrees = 0.0F;
  // VRAI QUAND LE SPRITE SE DESSINE TOURNÉ de `spin_degrees` dans l'écran, en
  // plus de l'angle que porte chacun de ses calques : un roulis, une rotation
  // propre qui avance à chaque tic, ou la FLÈCHE d'arc, dont l'unique image
  // prend l'angle que son vol fait avec la caméra. Faux, seul l'angle des
  // calques compte — voir `ComposeEffectNodeSprite`.
  bool draws_rotated = false;
  // L'ANGLE DE CHAQUE CALQUE DU `.act` tourne-t-il son quad ? Oui pour un nœud
  // semé par le patron d'un effet : le dessinateur des sprites d'effet ajoute
  // l'angle du calque à celui du nœud. Non ailleurs : un PROJECTILE tiré vers
  // sa cible — une flèche, une balle de monstre — est un acteur, calques
  // redressés, et son image ne prend que l'angle de son vol.
  bool draws_layer_angles = false;
  // DE COMBIEN LA FIGURE PENCHE, et vers où. Tirés une fois, au semis.
  //
  // ⚠ LE PENCHANT EST L'ÉCART AU QUART DE TOUR que la figure a déjà replié —
  // voir `EffectRingPattern::lean_degrees`. Y mettre l'angle brut du client
  // coucherait la figure une seconde fois.
  float lean_degrees = 0.0F;
  float yaw_degrees = 0.0F;
  float spin_speed_per_tick = 0.0F;
  float spin_acceleration_per_tick = 0.0F;

  // La LONGUEUR DU PAS D'ORBITE, en pixels. Ce n'est PAS la taille du quad —
  // les demi-extensions s'en chargent. Plusieurs dessinateurs s'en servent
  // aussi comme facteur, ce que nous ne portons pas encore.
  //
  // ⚠ NULLE PAR DÉFAUT, et il le faut : un nœud tire souvent son angle
  // d'orbite au hasard sans poser de pas, et un pas non nul le ferait alors
  // dériver dans une direction quelconque — une nuée qui part en étoile au
  // lieu de tenir en place.
  float step_length = 0.0F;
  float step_length_change_per_tick = 0.0F;

  // LE PAS DÉPLACE LE NŒUD LE LONG DE SON PROPRE AXE, et non sur son orbite.
  //
  // ⚠ CE SONT DEUX MOUVEMENTS DU CLIENT, PAS DEUX EMPLOIS D'UN SEUL. Un nœud
  // passe par l'un OU par l'autre, jamais par les deux : le répartiteur en
  // choisit un par sous-type. Celui du sous-type 9 multiplie son pas par la
  // TROISIÈME LIGNE de la matrice du nœud et l'accumule — la pointe de terre
  // jaillit dans la direction où elle penche, celle-là même où pointe son
  // sommet. Les mouvements d'orbite, eux, avancent dans le plan.
  //
  // `axial_translation` est ce que le pas a cumulé ; c'est au composeur de
  // l'appliquer le long de l'axe, qu'il est seul à connaître.
  bool translates_along_axis = false;
  float axial_translation = 0.0F;

  // LE PAS SE RELANCE TOUS LES SIX TICS au lieu de basculer une fois : c'est le
  // sous-type 117, et lui seul. Voir `kEffectNodeStepCycleTicks`.
  //
  // ⚠ IL ÉTEINT LA SECONDE IMAGE-CLÉ, et il le faut : les deux se disputeraient
  // la même réserve, et la bascule la consommerait la première.
  bool step_cycles = false;

  // LE TIC OÙ LE MOUVEMENT S'ARRÊTE NET : le pas ET sa variation tombent à
  // zéro. Négatif quand rien ne l'arrête.
  //
  // ⚠ IL N'EST PAS LE GEL DE LA CROISSANCE. Celui-ci fige les extensions — ce
  // que la figure MESURE ; celui-là fige l'avance — où elle EST. Le client les
  // porte dans deux champs distincts, et les confondre arrêterait une pointe
  // qui grandit au lieu d'une pointe qui monte.
  std::int32_t motion_stop_tick = -1;

  // LA TROISIÈME IMAGE-CLÉ, celle de la PLONGÉE.
  //
  // ⚠ ELLE SE COMPTE DEPUIS LA FIN, pas depuis la naissance : le client la joue
  // à `vie moins trente`. Une pointe dont la vie est TIRÉE plonge donc plus tard
  // qu'une autre, et toujours trente tics avant de s'éteindre. Un tic fixe
  // aurait fait plonger les plus longues bien avant leur fin.
  //
  // Sans elle, la pointe s'éteint sur place ; le client la fait RENTRER SOUS
  // TERRE d'abord, et c'est la comparaison en jeu qui l'a demandée.
  float sinking_step_length = kEffectNodeKeyframeUnset;
  float sinking_step_length_change = kEffectNodeKeyframeUnset;

  // La rampe d'alpha. `alpha_step_per_tick` change de signe au lieu de
  // s'arrêter quand le nœud rebondit.
  float alpha = 0.0F;
  float alpha_step_per_tick = 0.0F;
  float alpha_max = kEffectNodeDefaultAlphaMax;
  float alpha_min = 0.0F;

  // L'ORBITE, qui n'est PAS la rotation du quad : à chaque tic le nœud avance
  // d'un pas dans la direction de `orbit_degrees`, et ce déplacement
  // s'ACCUMULE. L'ordonnée descend, comme à l'écran, d'où la soustraction du
  // sinus.
  float orbit_degrees = 0.0F;
  float orbit_x = 0.0F;
  float orbit_y = 0.0F;

  // Rouge, vert, bleu, et leur variation par tic.
  std::array<float, 3> color = {kEffectNodeChannelMax, kEffectNodeChannelMax,
                                kEffectNodeChannelMax};
  std::array<float, 3> color_step_per_tick = {};

  std::int32_t age_ticks = 0;
  std::int32_t life_ticks = kEffectNodeDefaultLifeTicks;
  // Le tic où la sortie commence — la vie moins la durée du fondu.
  std::int32_t fade_out_tick = 0;
  // UN SEUL champ pour deux usages, et c'est ainsi dans le client : il dit
  // quand figer la croissance, ou tous les combien secouer l'angle d'orbite,
  // selon le drapeau posé. Les deux ne se combinent pas.
  std::int32_t motion_period_ticks = 1;
  // TOUS LES COMBIEN DE TICS LE PAS DE DEMI-HAUTEUR SE RETOURNE, sous
  // `kEffectNodeHeightGrowthFlips`, pour les sous-types quinze et seize. Zéro
  // veut dire « jamais ».
  //
  // ⚠ À PART DE `motion_period_ticks`, et il le faut : l'errance d'angle
  // RETIRE cette période-là au sort à chaque secousse, et les murs de Safety
  // Wall portent les deux bits. Partager le champ ferait battre leur hauteur
  // au rythme d'un tirage.
  std::int32_t height_flip_period_ticks = 0;

  // Ce que le nœud montre quand il est dessiné en SPRITE : son échelle propre,
  // et l'ACTION du fichier d'animation. `frame` sert alors d'image dans cette
  // action, comme il sert d'index de texture pour un nœud en quad.
  float sprite_scale = 1.0F;
  // Et de combien elle change à chaque tic — `792 += 796` chez le client. Voir
  // `EffectNodePattern::sprite_scale_step`.
  float sprite_scale_step = 0.0F;
  std::int32_t action = 0;

  // L'avance d'image : le nœud change de texture tous les `frame_hold_ticks`.
  // ZÉRO veut dire qu'il n'en change jamais.
  //
  // ⚠ IL BOUCLE PAR DÉFAUT, et c'est ce qui rend le défaut de quatre tics
  // vivable : avec une seule texture et sans boucle, le nœud aurait épuisé
  // ses images au premier tic et serait mort aussitôt.
  std::int32_t frame_hold_ticks = kEffectNodeDefaultFrameHoldTicks;
  // LA PÉRIODE D'IMAGE FRACTIONNAIRE, en tics, pour un sprite d'ACTEUR joué
  // comme un nœud : le client avance ses images à la cadence déclarée du
  // `.act`, en tics de vingt-quatre millisecondes — trois de nos deux —, si
  // bien qu'une cadence impaire ne tient pas dans un compte entier. Positive,
  // elle remplace `frame_hold_ticks` : l'image est l'ÂGE divisé par la période,
  // comme l'écrit l'attaque de la Mandragore. Zéro veut dire « la retenue
  // entière ».
  float frame_period_ticks = 0.0F;
  std::int32_t frame = 0;
  std::size_t texture_count = 1;
  bool loops_frames = true;
  // UNE ANIMATION QUI NE BOUCLE PAS A DEUX FINS, et non une. Le client la tue à
  // sa dernière image — sauf si le bit 0x800 de ses drapeaux est levé : il l'y
  // FIGE alors, et le nœud vit jusqu'au bout de sa vie. Sans effet quand il
  // boucle.
  bool holds_last_frame = false;
  // OU BIEN IL REVIENT À LA PREMIÈRE ET S'Y TIENT : voir
  // `EffectNodeSupplement::rests_on_first_frame`. `rested_on_first_frame` dit
  // qu'il y est arrivé, et que l'image n'avance plus.
  bool rests_on_first_frame = false;
  bool rested_on_first_frame = false;
  // LE MOT DE RENDU : voir `effect_render_word.h`. VIDE pour les familles dont
  // le semis ne le porte pas encore ; leurs figures gardent alors leur propre
  // mélange et l'épreuve de profondeur.
  std::optional<std::uint32_t> render_word;

  std::uint32_t flags = 0;
  // La graine de l'errance. Zéro est une graine comme une autre.
  std::uint32_t wander_seed = 0;
  // 🔴 LA TEINTE QUI TOURNE — le bit 0x800 lu par le mouvement des sous-types
  // quinze et seize, et par lui seul : les trois canaux BOUCLENT sur [0, 254]
  // (passé 254, zéro ; sous zéro, 254) au lieu de l'asymétrie ordinaire. Et
  // tant qu'un canal vaut 254, les pas attendent : le compteur monte d'un par
  // tic et ils ne s'appliquent qu'au tic où il atteint `color_hold_ticks`,
  // puis il repart de zéro. C'est ce qui fait PULSER le réticule de visée,
  // rouge puis presque blanc, au lieu de virer au rouge et d'y rester.
  bool cycles_color = false;
  std::int32_t color_hold_ticks = 0;
  std::int32_t color_hold_count = 0;
  // SOUS CET ALPHA, LE NŒUD S'ÉTEINT D'UN COUP : sitôt le pas du tic appliqué,
  // ce qui reste en deçà tombe à zéro. Zéro : pas de seuil — le cas de tous
  // sauf les pétales qui dérivent.
  float alpha_zero_below = 0.0F;
  // LA SECONDE IMAGE-CLÉ DU RAYON D'UNE COURONNE PLATE : au tic de bascule, la
  // croissance et l'accélération de `ring_radius` sont remplacées, comme les
  // extensions — c'est le rayon, et non le pas, que ce mouvement-là relance.
  float second_ring_radius_growth = kEffectNodeKeyframeUnset;
  float second_ring_radius_acceleration = kEffectNodeKeyframeUnset;
  // LES IMAGES-CLÉS D'ALPHA, dans l'ordre de leurs âges ; elles jouent AVANT le
  // pas du tic. Voir `EffectAlphaKeyframe`.
  std::array<EffectAlphaKeyframe, kEffectAlphaKeyframeCapacity> alpha_keyframes{};
  // 🔴 LA PREMIÈRE IMAGE SE TIENT CE NOMBRE DE TICS avant que l'avance ne
  // commence ; la retenue ordinaire joue ensuite. La flèche d'un arc et les
  // munitions de sa classe restent sur leur première image huit mises à jour,
  // puis avancent d'une par mise à jour jusqu'à la dernière. Zéro : l'avance
  // commence au premier tic. Sans effet sur une période fractionnaire.
  std::int32_t first_frame_hold_ticks = 0;
  // 🔴 L'ANGLE D'ÉCRAN SE PREND TEL QUEL, en degrés réels, au lieu d'être
  // tronqué au degré et lu dans la table du client : c'est ce que le
  // dessinateur de particules fait de la munition en sprite d'un monstre, là
  // où celui des acteurs tronque. Un degré au plus les sépare, et c'est le
  // degré que le joueur voit sur une longue hampe.
  bool spins_by_exact_degrees = false;
  // LE FONDU QUE L'INSTANCE IMPOSE à un nœud qui lui survivrait : à cet âge du
  // nœud, le pas d'alpha devient `−alpha / owner_fade_over_ticks`, et le
  // va-et-vient s'arrête. Il ne s'engage qu'une fois. Tant qu'il vaut
  // `kEffectNodeOwnerFadeUnjudged`, chaque mise à jour le juge : voir
  // `fades_with_its_instance`.
  //
  // ⚠ IL NE TOUCHE NI À LA VIE NI À LA SORTIE DU NŒUD : le client ne change que
  // le pas. La sortie propre (`fade_out_tick`) reprend donc la main si elle
  // tombe au même tic ou après, et refait son pas sur la vie du nœud. C'est la
  // troupe qui retire le nœud quand son instance meurt.
  std::int32_t owner_fade_at_age = kEffectNodeOwnerFadeUnjudged;
  std::int32_t owner_fade_over_ticks = 0;
  // VRAI QUAND SON MOUVEMENT PASSE PAR LA MISE À JOUR DE COULEUR qui juge ce
  // fondu. Elle compare à chaque passage ce qui reste au nœud (`vie − âge`) à
  // ce qui reste à son instance plus un ; dès que le premier dépasse, le pas
  // devient `−alpha / reste de l'instance`.
  bool fades_with_its_instance = false;
  // CE QUI RESTE À SON INSTANCE, TEL QUE CETTE MISE À JOUR LE LIT : l'âge de
  // l'instance a déjà crû de ce passage quand ses nœuds sont mis à jour. La
  // troupe le pose avant chaque mise à jour ; zéro : inconnu, rien n'est jugé.
  std::int32_t instance_ticks_left = 0;
};

// 🔴 CE NŒUD PLONGE-T-IL ? — ET LA RÉPONSE DÉCIDE AUSSI DE SON ALPHA.
//
// Le client range les deux dans le MÊME chemin. Sous le bit deux des drapeaux,
// le mouvement pose la plongée à l'âge `vie − 30` puis S'ARRÊTE ; la rampe de
// couleur ne tourne que sur l'autre branche, quand le bit est ABSENT. Un nœud
// qui plonge n'a donc NI fondu de sortie, NI va-et-vient d'alpha, NI fondu avec
// son instance : sa couleur reste le `0xFFFFFFFF` d'un nœud neuf. C'est ce qui
// le fait rentrer sous terre PLEIN, au lieu de s'éteindre en chemin.
//
// ⚠ LE TÉMOIN EST LA PLONGÉE POSÉE, et non un bit relu au dessin :
// `kEffectNodeKeyframeUnset` dit « le bâtisseur ne l'a pas écrite », ce qui est
// exactement l'absence du bit. Et un pas de plongée NUL est une plongée POSÉE —
// la pointe centrale de l'effet 79 lève le bit sans écrire la valeur.
[[nodiscard]] constexpr bool EffectNodeDives(const EffectNode& node) {
  return node.sinking_step_length != kEffectNodeKeyframeUnset ||
         node.sinking_step_length_change != kEffectNodeKeyframeUnset;
}

// Fait avancer le nœud d'UN tic, et dit s'il vit encore.
//
// ⚠ C'EST ICI QUE VIVENT LES MOUVEMENTS DES SOUS-TYPES À CHAMPS DE NŒUD, et
// nulle part ailleurs. Les mouvements des sous-types 12 et 13 n'ont pas de
// transcription à eux : ce qu'ils font — intégrer les rayons par leur pas, la
// hauteur par le sien, la demi-largeur, le lacet et le penchant, puis la rampe
// de couleur — est exactement ce que fait cette fonction pour TOUS les nœuds.
// Leur donner chacun un fichier écrirait trois fois la même chose.
//
// DEUX fins, et la première venue l'emporte : le nœud a montré sa dernière
// texture sans boucler, ou il a atteint sa durée de vie. La seconde ne
// s'ajoute pas à ce que fait le client, elle s'en DÉDUIT : le pas de sortie
// est calculé pour annuler l'alpha à la fin de vie, donc un nœud ne peut pas
// survivre à la sienne. Boucler ne concerne que les images ; un nœud dont la
// vie n'est pas posée meurt au premier tic, ce qui rend l'oubli visible tout
// de suite plutôt qu'à l'écran.
//
// ⚠ LA COMPOSANTE ROUGE S'ENROULE là où le vert et le bleu se bornent : passé
// 255 elle retombe à zéro, sous zéro elle remonte à 255. Cette asymétrie est
// celle du client et ressemble fort à une inversion de branches restée en place
// ; la reproduire est ce qui garde la parité.
[[nodiscard]] bool AdvanceEffectNode(EffectNode& node);

// Le plus grand tirage que le générateur du nœud rend.
inline constexpr std::uint32_t kEffectNodeDrawSpan = 0x7FFFU;

// Le tirage suivant, entre zéro et `kEffectNodeDrawSpan`, bornes comprises.
//
// Le nœud porte SA suite au lieu d'appeler un générateur global : c'est ce qui
// rend son errance et les semis qui le fabriquent également REJOUABLES. Le
// client tire avec `rand()`, dont la suite ne se reproduit pas d'une
// implémentation à l'autre — et le rendu n'en dépend pas.
[[nodiscard]] std::uint32_t NextEffectNodeDraw(std::uint32_t& generator);

// L'alpha du nœud une fois borné et arrondi, prêt à teinter un quad.
//
// La rampe déborde ses bornes d'un pas au plus — le client compare APRÈS avoir
// ajouté — et un alpha négatif ou au-delà de 255 ne veut rien dire au moment de
// peindre.
[[nodiscard]] std::uint8_t EffectNodeAlpha(const EffectNode& node);

}  // namespace ro::render
