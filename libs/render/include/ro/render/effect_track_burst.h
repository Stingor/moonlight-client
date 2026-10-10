#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

// LES MOUVEMENTS DES CINQ DERNIÈRES FAMILLES DU LOT 8.
//
// Sortis d'`effect_track.h` au plafond de huit cents lignes, et l'en-tête suit
// la coupure que son `.cpp` avait déjà prise : ce sont exactement les mouvements
// de `effect_track_burst.cpp`.
//
// ⚠ LA COUPURE EST ARBITRAIRE, ET C'EST ASSUMÉ. Il n'y a pas de famille commune
// entre un portail qui pulse, une colonne qui tourne, un carré qui tremble, un
// mur qui s'ouvre et un essaim qui poursuit — ce sont simplement les cinq
// derniers arrivés. Les ranger par « thème » aurait inventé une parenté que le
// client n'a pas.

namespace ro::render {

// COMBIEN D'ANNEAUX le nœud du sous-type 63 porte, et où vit son REGISTRE.
//
// ⚠ TROIS PISTES SUR QUATRE SE DESSINENT. La quatrième n'est pas oubliée : son
// STYLE porte le mode de tout le nœud et son COMPTEUR l'âge global. Une piste
// qui ne paraît jamais et qui décide de tout.
inline constexpr std::size_t kEffectRadialBurstRingCount = 3;
inline constexpr std::size_t kEffectRadialBurstRegisterTrack = 3;

// LES ANNEAUX QUI PULSENT : le mouvement du sous-type 63.
//
// Tout y dépend du MODE que le registre porte, et aucun de ses nombres ne se
// déduit d'un autre :
//
//   - LE RAYON RESPIRE DANS LES DEUX SENS. Le mode trois le fait DÉCROÎTRE d'un
//     quart par tic, tous les autres le font croître d'un demi. Le seuil est
//     sept dans les deux cas, et c'est là que l'alpha commence à tomber ;
//   - L'ALPHA A QUATRE RYTHMES ET QUATRE PLAFONDS. Il monte de quatre, douze ou
//     vingt-quatre pendant dix tics — vingt pour le mode trois —, plafonne à
//     quarante, cent vingt ou deux cent quarante, puis tombe de cinq, huit,
//     quinze ou sept ;
//   - LA PISTE RENAÎT AU LIEU DE MOURIR, tant que l'âge global le permet : cent
//     vingt tics pour les modes un, quatre et cinq, mille quatre cents pour les
//     autres. C'est ce qui fait durer un portail ; les laisser mourir au premier
//     cycle éteindrait l'effet en deux secondes. Le mode trois renaît d'ailleurs
//     à un rayon de QUATORZE, et les autres à zéro : l'un se referme, les autres
//     s'ouvrent.
//
// ⚠ SEULE LA PREMIÈRE PISTE FAIT AVANCER L'ÂGE GLOBAL, et le client l'avance
// avant même de savoir si elle a le droit de bouger. Le faire avancer trois fois
// par tic userait les trois anneaux en un tiers du temps.
//
// ⚠ ET LES VINGT ET UNE CASES REÇOIVENT TOUTES LA CLÉ DE LA PISTE, modulée par
// le sinus de neuf fois le compteur pendant les dix premiers tics : le ruban
// s'OUVRE en un quart de sinus, puis reste plein. Le semer plein le ferait
// paraître d'un coup.
void AdvanceRadialBurstTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// AU BOUT DE COMBIEN DE TICS LA COLONNE S'OUVRE, et sur combien.
//
// ⚠ CENT TICS D'ATTENTE, ET CE N'EST PAS UN OUBLI. Avant le centième tic, les
// vingt et une cases valent ZÉRO : la colonne existe, tourne, porte son alpha,
// et ne se dessine pas. Son bâtisseur sème d'ailleurs le compteur entre soixante
// et quatre-vingt-dix-neuf, si bien que chaque piste attend de UN à QUARANTE
// tics — c'est un retard TIRÉ AU SORT, et c'est lui qui décale les quatre
// colonnes les unes des autres.
inline constexpr std::int32_t kEffectColumnOpensAtTick = 100;
inline constexpr std::int32_t kEffectColumnFullAtTick = 115;

// LA COLONNE QUI TOURNE : le mouvement du sous-type 137.
//
// Il tient en quatre gestes, et chaque piste vive les subit :
//
//   - le compteur avance, et l'AZIMUT avec lui — d'un degré par tic, ramené dans
//     le tour. C'est ce qui fait tourner la colonne ;
//   - passé le cent vingt-cinquième tic, l'alpha tombe de quinze et la CLÉ de
//     cinq, toutes deux bornées à zéro ;
//   - et les vingt et une cases prennent la clé, ouverte en un quart de sinus.
//
// ⚠ L'OUVERTURE EST UN QUART DE SINUS, et non une table à part : au centième
// tic la clé vaut le sinus de zéro, au cent quinzième celui de
// quatre-vingt-dix. La branche suivante rend la clé ENTIÈRE passé le cent
// quinzième tic, et le sinus de quatre-vingt-dix vaut justement un : les deux
// branches se REJOIGNENT sans saut.
void AdvanceSpiralColumnTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE CARRÉ QUI TREMBLE ET ORBITE : le mouvement du sous-type 32.
//
// Il ne fait vivre que la piste ZÉRO ; la piste UN ne sert qu'à porter le
// second angle de rotation.
//
// ⚠ SES DEUX ANGLES VIENNENT DE LA CAMÉRA, et c'est ce qui fait de cette figure
// un billboard : `turn_degrees` vaut TROIS CENT SOIXANTE moins un angle du mode
// de jeu, `tilt_degrees` QUATRE CENT CINQUANTE moins l'autre, ramené sous le
// tour. Le mot « billboard » porte donc sur le MOUVEMENT et non sur la figure —
// comme pour les sous-types quinze et seize, et pour la même raison. `render` ne
// connaît pas la caméra (CLAUDE.md) : l'appelant les donne.
//
// ⚠ LE TREMBLEMENT EST FAIT DE TROIS MARCHES VERS UNE CIBLE. Les cases zéro à
// cinq forment TROIS PAIRES — un angle courant et sa cible. Chaque courant
// avance d'UN degré par tic vers sa cible, et dès qu'il l'atteint une NOUVELLE
// cible est tirée. Les lire comme six angles indépendants supprimerait le
// tremblement, qui naît justement de la poursuite.
//
// ⚠ ET LE CHAMP DU NŒUD FAIT MONTER OU TOMBER LE MÊME EFFET. `dives` vient d'un
// champ du nœud : posé, le cap avance de QUINZE degrés au lieu d'un, la hauteur
// DESCEND jusqu'à moins trente et le rayon CROÎT jusqu'à dix ; absent, la
// hauteur est fixée à moins quinze et le rayon à six. Un seul champ, deux
// effets qui n'ont pas l'air parents.
void AdvanceOrbitJitterTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                              bool dives, std::uint32_t& generator);

// LE MUR QUI S'OUVRE : le mouvement du sous-type 75.
//
// Le cas 75 du répartiteur. Pour chaque piste vive :
//
//   - le RAYON INTÉRIEUR gagne une unité par tic, et passé DIX l'alpha se met à
//     tomber de deux. Le mur s'éloigne donc du centre et s'éteint en chemin ;
//   - l'ÉPAISSEUR et la HAUTEUR croissent, mais à deux rythmes selon la marque :
//     la marque UN gagne une unité d'épaisseur et deux dixièmes de hauteur, et
//     plafonne à deux ; toutes les autres gagnent DEUX et QUATRE dixièmes, et
//     plafonnent à quatre ;
//   - et l'alpha MONTE de cinq tant que le compteur ne dépasse pas son seuil —
//     CINQ tics pour la marque un, SEPT pour les autres.
//
// ⚠ LA MARQUE CHANGE QUATRE NOMBRES À LA FOIS, et aucun ne se déduit d'un
// autre : deux pas de croissance, un plafond, un seuil de tics. La lire comme un
// simple interrupteur ferait un mur qui s'ouvre deux fois trop vite ou deux fois
// trop lentement, selon le sens de l'erreur.
//
// ⚠ ET LA PISTE MEURT PAR SON ALPHA, pas par son âge : quand la chute le fait
// passer sous zéro, le client l'ÉTEINT. La laisser vivre à alpha nul ferait
// grandir un mur invisible jusqu'à la fin de l'effet.
void AdvanceTripleWallTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LES DEUX RÉGLAGES DE L'ESSAIM, un par sous-type.
//
// ⚠ LES DEUX MOUVEMENTS DU CLIENT NE DIFFÈRENT QUE PAR TROIS NOMBRES — l'essaim
// étroit pour le 155, le large pour le 170 —, et c'est pourquoi un seul les
// porte. Les transcrire séparément aurait doublé quatre-vingts lignes pour
// trois valeurs.
//
// ⚠ ET « LARGE » NE PARLE PAS DE LA LARGEUR D'UN QUAD mais de la DISPERSION du
// cap : l'essaim large vise n'importe où sur le tour, l'étroit reste à
// quarante- cinq degrés de sa référence. Il vit aussi cinq fois plus longtemps.
struct EffectSwarmTiming {
  // De combien de degrés une cible de cap peut s'écarter de sa référence, et de
  // combien on recule avant de tirer. L'étroit tire sur quatre-vingt-onze et
  // recule de quarante-cinq ; le large tire sur le tour entier sans reculer.
  std::int32_t heading_spread_degrees = 91;
  float heading_bias_degrees = 45.0F;
  // Jusqu'à quel tic l'alpha monte, et à partir duquel il tombe.
  std::int32_t alpha_rises_until_tick = 10;
  std::int32_t alpha_falls_after_tick = 20;
};

inline constexpr EffectSwarmTiming kEffectNarrowSwarm{91, 45.0F, 10, 20};
inline constexpr EffectSwarmTiming kEffectWideSwarm{360, 0.0F, 50, 60};

// L'ESSAIM QUI POURSUIT : le mouvement des sous-types 155 et 170.
//
// ⚠ DIX CASES EN DEUX GROUPES DE CINQ, un par POSITION : une référence de cap,
// une cible, un cap courant, une cible d'inclinaison, une inclinaison courante.
// Les lire comme dix valeurs indépendantes supprimerait la poursuite, qui est
// tout le mouvement.
//
// ⚠ LE CAP PREND LE CHEMIN LE PLUS COURT. Le client compare la différence à CENT
// QUATRE-VINGTS ET à MOINS CENT QUATRE-VINGTS pour décider du sens, puis avance
// de deux degrés. Avancer dans le sens du signe ferait faire le tour long à une
// mouche qui n'a qu'un pas à faire.
//
// ⚠ ET LES DEUX POSITIONS MONTENT EN SENS OPPOSÉS : celle de la queue SOUSTRAIT
// le sinus de son inclinaison, celle de l'ancre l'AJOUTE. C'est l'essaim — deux
// nuées qui se séparent. Leur donner le même signe les collerait l'une à
// l'autre, et la figure n'aurait plus qu'une position.
//
// Les deux angles de rotation viennent de la CAMÉRA, comme pour le sous-type 32,
// et l'appelant les donne.
void AdvanceBillboardSwarmTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 const EffectSwarmTiming& timing, std::int16_t camera_turn_degrees,
                                 std::int16_t camera_tilt_degrees, std::uint32_t& generator);

// DE COMBIEN LE BOUCLIER TOURNE PAR RAPPORT À CELUI QUI LE PORTE, et quelle
// marque le fait tourner sur lui-même.
//
// ⚠ L'AZIMUT EST RECALCULÉ À CHAQUE TIC À PARTIR DU PORTEUR, et non avancé :
// le client lit un angle du porteur, lui ajoute TROIS QUARTS DE TOUR et écrase
// l'azimut avec. Le bouclier fait donc face à la même direction que celui qui
// le porte, et pivote quand il pivote. L'avancer au lieu de l'écraser le ferait
// dériver dès le premier demi-tour du personnage.
//
// ⚠ ET LA MARQUE UN EST LA SEULE EXCEPTION, mais AUCUN des cinq identifiants ne
// la porte : le bâtisseur ne sait poser que zéro, deux, trois, quatre et cinq.
// Elle est transcrite parce que le client l'a, pas parce qu'elle sert ; la
// retirer ferait croire un jour que le client n'en a jamais eu.
inline constexpr std::int16_t kSphereShellFacingOffsetDegrees = 270;
inline constexpr std::uint8_t kSphereShellSpinningMark = 1;
inline constexpr std::int16_t kSphereShellSpinPerTick = 10;

// LES TROIS ÂGES DU BOUCLIER, en tics.
//
// ⚠ IL Y A UN PALIER ENTRE LES DEUX, et il dure vingt et un tics : avant le
// dixième l'alpha monte, après le vingt-sixième il tombe, et entre les deux le
// client ne touche à RIEN. Coller les deux seuils supprimerait le temps pendant
// lequel le bouclier se tient.
inline constexpr std::int32_t kSphereShellAlphaRisesBeforeTick = 10;
inline constexpr std::int32_t kSphereShellFadesAfterTick = 26;

// ⚠ SON ALPHA PLAFONNE À CENT, pas à deux cent cinquante-cinq. Le bouclier
// n'est jamais opaque — c'est une vitre, et le plafonner plus haut en ferait un
// mur. Il monte de vingt et tombe de SIX, ce qui lui donne une extinction trois
// fois plus longue que son apparition.
inline constexpr std::int16_t kSphereShellAlphaCeiling = 100;
inline constexpr std::int16_t kSphereShellAlphaRise = 20;
inline constexpr std::int16_t kSphereShellAlphaFall = 6;

// DE COMBIEN LA COQUILLE EXTÉRIEURE ENFLE, une fois la chute commencée.
inline constexpr float kSphereShellGrowthPerTick = 0.1F;

// LE BOUCLIER QUI FAIT FACE : le mouvement du sous-type 61.
//
// Transcrit le cas 61 du répartiteur. Pour chaque piste vive, dans cet ordre :
// le compteur avance, l'azimut se recale sur le porteur, l'alpha monte ou tombe
// selon l'âge, et la case zéro enfle en même temps que l'alpha tombe.
//
// ⚠ L'ENFLURE ET LA CHUTE SONT LE MÊME GESTE : le client les fait dans la même
// branche, si bien que la coquille extérieure ne s'écarte QUE pendant qu'elle
// s'éteint. Les séparer ferait grandir un bouclier plein.
//
// ⚠ IL PARCOURT QUATRE PISTES ALORS QUE LE SEMIS N'EN ALLUME QU'UNE, et c'est
// le client qui fait ainsi. Les trois autres sont éteintes et ne coûtent rien ;
// n'en parcourir qu'une serait supposer qu'aucun autre bâtisseur n'arrivera.
//
// `owner_facing_degrees` est l'angle du porteur, en degrés. `render` ne connaît
// pas l'état de jeu (CLAUDE.md) : l'appelant le donne, comme il donne les deux
// angles de caméra aux billboards.
void AdvanceSphereShellTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              float owner_facing_degrees);

// DE COMBIEN LA TENTURE RESPIRE, et à quelle cadence.
//
// ⚠ SA HAUTEUR N'EST PAS UN ÉTAT QUI CROÎT, C'EST UN SINUS : le mouvement la
// RECALCULE à chaque tic à partir d'un angle qui tourne, entre DIX et TRENTE.
// L'écrire comme une croissance la ferait monter sans jamais redescendre, et le
// battement — qui est tout ce que cette figure fait — disparaîtrait.
inline constexpr float kGroundCurtainHeightAmplitude = 10.0F;
inline constexpr float kGroundCurtainHeightMiddle = 20.0F;

// ⚠ ET UN SEUL STYLE BAT DEUX FOIS PLUS VITE. Le client compare la case zéro à
// UN et avance alors de QUATRE degrés par tic au lieu de deux. Leur donner la
// même cadence ferait respirer ensemble des marques au sol que le client
// distingue.
inline constexpr float kGroundCurtainFastStyle = 1.0F;
inline constexpr std::int16_t kGroundCurtainFastStepDegrees = 4;
inline constexpr std::int16_t kGroundCurtainStepDegrees = 2;

// LA TENTURE QUI RESPIRE : le mouvement du sous-type 48.
//
// Transcrit le cas 48 du répartiteur. Pour chaque piste vive : le compteur
// avance, l'angle tourne, et la hauteur suit son sinus.
//
// ⚠ IL NE TOUCHE NI À L'ALPHA NI À LA GÉOMÉTRIE AU SOL, et la piste ne meurt
// jamais d'elle-même : c'est la VIE DE L'EFFET qui la borne. Y ajouter une
// extinction raccourcirait des marques au sol que le client laisse tant que le
// chant dure.
//
// ⚠ ET LE COMPTEUR NE SERT À RIEN ICI. Le client l'avance quand même ; le
// retirer ferait diverger l'état de la piste de celui du client sans que rien
// ne se voie à l'écran — jusqu'au jour où une autre pièce le lirait.
void AdvanceGroundCurtainTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// DE COMBIEN L'ANGLE DE L'ÉTINCELLE MONTE par tic, et où il s'arrête.
//
// ⚠ IL PLAFONNE À CENT QUATRE-VINGTS, et c'est ce qui termine l'arc : au-delà
// le sinus redeviendrait positif et l'étincelle repartirait vers le haut. Le
// laisser courir la ferait osciller au lieu de retomber.
inline constexpr float kSparkTubeAngleStepDegrees = 6.0F;
inline constexpr float kSparkTubeAngleCeilingDegrees = 180.0F;

// À PARTIR DE QUEL TIC L'ÉTINCELLE S'ÉTEINT, et de combien.
inline constexpr std::int32_t kSparkTubeFadesAfterTick = 35;
inline constexpr std::int16_t kSparkTubeAlphaFall = 5;

// L'ÉTINCELLE QUI MONTE : le mouvement du sous-type 100.
//
// Transcrit le cas 100 du répartiteur. Pour chaque piste vive :
//
//   - le compteur avance, et tant qu'il est NÉGATIF la piste ne fait RIEN
//     d'autre. C'est le retard que le bâtisseur a tiré au sort, et c'est lui
//     qui échelonne les quatre étincelles d'un même nœud ;
//   - passé le trente-cinquième tic, l'alpha tombe de cinq, borné à zéro ;
//   - l'angle de tête monte de six degrés, plafonné à cent quatre-vingts ;
//   - et les cases GLISSENT d'un rang : la courbe est un HISTORIQUE.
//
// ⚠ C'EST UN TAMPON CIRCULAIRE, ET C'EST TOUTE LA FIGURE. La case zéro porte
// l'angle courant, la case un celui du tic précédent, et ainsi de suite sur
// autant de cases que le bâtisseur en a demandé. Sans le glissement, les
// points de la courbe seraient tous égaux et le tube aurait une longueur
// nulle.
//
// ⚠ ET L'ANCRE SUIT LE PORTEUR, piste par piste. Le dessinateur pose ses coins
// en ABSOLU : sans cette recopie, l'étincelle resterait à l'origine du monde.
void AdvanceSparkTubeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            glm::vec3 owner_position);

// LES DEUX RYTHMES DE LA TAILLADE, et le tic qui les sépare.
//
// ⚠ SA VITESSE CROÎT PLUS VITE EN MOURANT QU'EN NAISSANT : deux pour cent par
// tic tant que l'alpha monte, QUATRE ensuite. La taillade accélère donc en
// s'effaçant, ce qui la fait filer hors de vue au lieu de s'arrêter. Prendre
// le même facteur pour les deux la laisserait traîner.
inline constexpr std::int32_t kGlidingSlashRisesUntilTick = 10;
inline constexpr std::int16_t kGlidingSlashAlphaRise = 10;
inline constexpr std::int16_t kGlidingSlashAlphaFall = 5;
inline constexpr float kGlidingSlashSpeedGainWhileRising = 1.02F;
inline constexpr float kGlidingSlashSpeedGainWhileFading = 1.04F;

// DE COMBIEN LE CAP S'ÉCARTE DE LA CIBLE, et la marque qui resserre le tir.
//
// ⚠ LA MÊME MARQUE QUI ANIME L'IMAGE RESSERRE LE CAP : cinq degrés de part et
// d'autre au lieu de quinze. Un octet, deux conséquences sans rapport
// apparent.
inline constexpr std::int32_t kGlidingSlashNarrowAimSpread = 11;
inline constexpr std::int32_t kGlidingSlashNarrowAimBias = 5;
inline constexpr std::int32_t kGlidingSlashWideAimSpread = 31;
inline constexpr std::int32_t kGlidingSlashWideAimBias = 15;

// LA TAILLADE QUI GLISSE : le mouvement des sous-types 86 ET 92.
//
// Transcrit les cas 86 et 92 du répartiteur. ⚠ CE SONT LE MÊME CODE À UNE
// BRANCHE PRÈS, et c'est pourquoi une seule fonction les porte : le 86 VISE au
// premier tic, le 92 garde l'azimut que son site lui a donné. Tout le reste —
// l'alpha, la vitesse, la glissade — est identique.
//
// Pour chaque piste vive :
//
//   - le compteur avance, et tant qu'il est négatif la piste ne fait RIEN ;
//   - AU PREMIER TIC SEULEMENT, le 86 pose son cap : la direction de la cible,
//     plus ou moins cinq ou quinze degrés selon sa marque ;
//   - aux tics SUIVANTS, l'alpha monte de dix puis tombe de cinq, et la
//     vitesse gagne deux puis quatre pour cent ;
//   - et l'ancre GLISSE de sa vitesse dans la direction de son azimut.
//
// ⚠ LE PREMIER TIC NE FAIT NI ALPHA NI VITESSE, même pour le 92 qui n'a rien à
// viser : le client range les deux dans des branches exclusives. Les fondre
// donnerait une taillade d'un tic d'avance sur celle du client.
//
// ⚠ ET LA PISTE MEURT PAR SON ALPHA, pas par son âge : quand la chute le fait
// passer sous zéro, le client l'ÉTEINT.
//
// `toward_target` est le vecteur du nœud vers la cible du coup. `render` ne la
// connaît pas (CLAUDE.md) : l'appelant la donne.
void AdvanceGlidingSlashTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               bool aims_at_target, glm::vec3 toward_target,
                               std::uint32_t& generator);

// LA FENÊTRE OÙ L'ÉCLAT S'ALLUME, et les deux pas de son alpha.
//
// ⚠ IL TOMBE AVANT DE MONTER. Du premier au dixième tic le client RETRANCHE
// cinq à un alpha déjà nul — sans effet, puisqu'il est borné —, puis il
// AJOUTE quinze du onzième au vingtième, puis il retranche de nouveau. Écrire
// une montée franche dès le premier tic allumerait l'éclat dix tics trop tôt.
inline constexpr std::int32_t kFlyingShardLightsAtTick = 11;
inline constexpr std::int32_t kFlyingShardDimsAfterTick = 20;
inline constexpr std::int16_t kFlyingShardAlphaRise = 15;
inline constexpr std::int16_t kFlyingShardAlphaFall = 5;

// ⚠ ET UNE MARQUE SUR DEUX MONTE MOINS VITE QUAND LE JOUEUR A RÉDUIT LES
// EFFETS. Le client interroge la bascule de réglage 0xE5 pour la marque
// VINGT-QUATRE seulement, et monte alors de HUIT au lieu de quinze. Nous ne
// modélisons pas ce réglage : nous prenons la branche pleine, celle du joueur
// qui n'a rien réduit, comme pour les images des autres familles.
inline constexpr std::uint8_t kFlyingShardMarkThatDims = 24;
inline constexpr std::int16_t kFlyingShardReducedRise = 8;

// L'ÉCLAT QUI FUIT : le mouvement du sous-type 44.
//
// Transcrit le cas 44 du répartiteur. Pour chaque piste vive :
//
//   - le compteur avance, et le RAYON grandit de la case zéro dès qu'il est
//     positif — c'est la fuite, et c'est tout le déplacement ;
//   - l'alpha tombe de cinq, monte de quinze entre le onzième et le vingtième
//     tic, puis retombe. Il est borné à zéro par le bas.
//
// ⚠ LE RAYON NE GRANDIT QUE PASSÉ LE RETARD, mais l'ALPHA BOUGE TOUT DE SUITE.
// Les deux gardes ne sont pas la même, et les fondre ferait tomber l'alpha
// d'un éclat qui n'a pas encore commencé — sans effet ici, puisqu'il naît nul,
// et faux le jour où un bâtisseur le sèmerait plein.
void AdvanceFlyingShardTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
