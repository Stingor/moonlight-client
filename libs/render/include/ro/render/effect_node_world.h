#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_node_fwd.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/terrain_mesh.h"

namespace ro::render {

// Les dessinateurs de nœud qui travaillent DANS LE MONDE — le troisième chemin
// de composition.
//
// Les deux premiers ne projettent pas : `effect_scene.h` rend des quads en
// pixels autour d'un point déjà projeté, `effect_node_sprite.h` pose une image
// d'ACT. Ceux-ci font autre chose — ils bâtissent leurs sommets en coordonnées
// du monde et les projettent eux-mêmes. La phrase de l'ADR 0014 selon laquelle
// « le nœud ne projette pas » vaut pour le billboard, pas pour la famille. Voir
// l'ADR 0017.
//
// Chaque fonction dit sa figure au-dessus d'elle.
//
// Description PURE, comme les deux autres : aucune texture, aucun contexte
// graphique. Les données sont dans `scripts/client_effect_node_rings.json`.

// LA PROFONDEUR QU'UNE PRIMITIVE DEMANDE, primitive par primitive.
//
// ⚠ CE N'EST PAS LE SEAU. Le mot de rendu du nœud décide d'un LOT ; ceci
// décide d'un QUAD, et un même dessinateur en sort de deux sortes selon sa
// marque ou son style.
enum class EffectQuadDepth : std::uint8_t {
  // La profondeur projetée de ses coins : le cas ordinaire.
  kProjected,
  // APLATIE AU PREMIER PLAN : le quad passe devant tout, hors brouillard, et sa
  // texture se pose sans correction de perspective.
  kFrontmost,
  // AVANCÉE D'UN SOUFFLE vers l'œil : l'inverse de sa profondeur gagne
  // `kEffectQuadDepthNudge`. Il gagne ainsi un départage à profondeur égale,
  // sans rien changer à l'écran.
  kNudged,
  // RECENTRÉE À L'ÉCRAN PUIS APLATIE : le quad garde la hauteur projetée de
  // chaque coin, levée d'un seizième d'écran, mais se recentre sur l'abscisse
  // projetée de `EffectWorldQuad::screen_anchor`, à la largeur projetée de
  // chacun de ses deux triangles.
  kScreenRecentred,
};

// Ce que `kNudged` ajoute à l'inverse de la profondeur.
inline constexpr float kEffectQuadDepthNudge = 0.000045F;
// De combien `kScreenRecentred` lève le quad, en part de la hauteur de l'écran.
inline constexpr float kEffectQuadRecentredRiseScreenShare = 30.0F / 480.0F;

// LE POINT QUI DONNE SA PROFONDEUR À UN QUAD, quand la file le range parmi les
// autres de son seau.
//
// La file trie chaque triangle sur la profondeur de son PREMIER SOMMET, et
// c'est chaque dessinateur qui choisit lequel vient en premier. Une bande
// d'anneau émet ses deux triangles en tête par les deux coins de son PREMIER
// CERCLE, celui de l'ancre : ses segments se rangent donc au niveau du sol, et
// non à mi-hauteur. Pour trois colonnes emboîtées de hauteurs différentes, le
// centre rangerait les segments hauts de la plus haute devant l'avant des plus
// courtes, et la grande colonne passerait par-dessus les petites.
enum class EffectQuadSortPoint : std::uint8_t {
  // Le centre des quatre coins, faute de mieux mesuré.
  kCentre,
  // Le milieu des coins 0 et 1, que les deux triangles du quad prennent chacun
  // pour clé.
  kFirstEdge,
  // Le coin 0 seul : le quad porte UN triangle dont c'est le premier sommet. La
  // croix de pointes se range ainsi par un coin de sa base, au sol : une haute
  // pointe de glace ne passe pas devant les courtes pointes de pierre plantées
  // devant elle parce que son sommet est plus près de l'œil.
  kFirstCorner,
};

// Un quad d'effet POSÉ DANS LE MONDE.
//
// Il diffère d'`EffectQuad` sur le seul point qui compte : ses coins sont des
// points du monde et non des pixels autour d'une ancre. Les mêler ferait un
// type dont la moitié des champs serait toujours vide, et surtout un renderer
// qui devrait deviner lequel des deux repères il tient.
//
// Les coordonnées de texture sont données PAR COIN, comme pour un quad de sol :
// une bande d'anneau n'a pas une portion rectangulaire de texture à chaque
// segment, et un couple min/max ne saurait pas le dire.
struct EffectWorldQuad {
  std::array<glm::vec3, 4> corners = {};
  std::array<glm::vec2, 4> texture_uv = {};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // Les facteurs de mélange, dans la numérotation de Direct3D — celle du
  // fichier et celle du client.
  std::int32_t blend_source = 0;
  std::int32_t blend_destination = 0;
  // Le numéro de la texture dans la liste du nœud.
  std::size_t texture = 0;
  // Voir `EffectQuadDepth`.
  EffectQuadDepth depth = EffectQuadDepth::kProjected;
  // L'ORIGINE DU NŒUD, que seul `kScreenRecentred` lit.
  glm::vec3 screen_anchor{0.0F, 0.0F, 0.0F};
  // Voir `EffectQuadSortPoint`.
  EffectQuadSortPoint sort_point = EffectQuadSortPoint::kCentre;
};

// Le point du monde dont la profondeur range ce quad. Voir `EffectQuadSortPoint`.
[[nodiscard]] glm::vec3 EffectWorldQuadSortPoint(const EffectWorldQuad& quad);

// L'ORIENTATION d'un anneau : où vont ses trois axes locaux, et de combien son
// centre s'écarte de l'ancre.
//
// Quatre colonnes de trois, comme la matrice du nœud : les trois premières sont
// les images des axes locaux x, y et z, la quatrième une translation. Le client
// la refait à chaque tic à partir de deux angles ; nous ne portons pas encore
// ce mouvement, et l'anneau la reçoit donc en paramètre.
using EffectRingOrientation = glm::mat4x3;

// L'anneau COUCHÉ, celui d'un nœud dont le mouvement n'a rien tourné : son
// plan est celui du sol, sa hauteur monte.
inline constexpr EffectRingOrientation kEffectRingUpright{
    glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F},
    glm::vec3{0.0F, 0.0F, 0.0F}};

// L'orientation d'un nœud qui FAIT FACE À LA CAMÉRA.
//
// ⚠ ELLE N'EST PAS OPTIONNELLE pour le quad orienté, et c'est ce qui le rendait
// invisible : le mouvement des sous-types 15 et 16 bâtit la seconde matrice du
// nœud À PARTIR DE LA CAMÉRA du mode de jeu avant d'y appliquer les angles. Ce
// qui fait face à la caméra est donc son MOUVEMENT, pas sa figure — et lui
// passer l'identité poserait son quad dans un plan vertical arbitraire, vu par
// la tranche selon l'azimut.
//
// Les trois axes viennent du même repère que celui d'un sprite : l'axe local de
// la hauteur va vers le haut de l'écran, celui de la largeur vers sa GAUCHE, et
// la normale vers la caméra. C'est la matrice que le client bâtit de sa caméra,
// conjuguée par `S` : les coins du quad y passent par `S`, comme sous les angles
// du nœud.
//
// 🔴 L'INCLINAISON DU NŒUD S'APPLIQUE EN PREMIER aux coordonnées locales : le
// client pose la rotation autour de x avec elle — qui ÉCRASE la matrice —, puis
// multiplie l'altitude de sa caméra autour de z, puis son lacet plus un quart de
// tour autour de y. `pitch_degrees` est l'inclinaison du CLIENT, lue dans sa
// table comme pour `EffectNodeOrientedAsClient` ; `S` la laisse intacte
// (`S·Rx(p)·S = Rx(p)`).
//
// 🔴 LES DEUX ANGLES DE LA CAMÉRA PASSENT AUSSI PAR LA TABLE, tronqués au degré :
// l'altitude du client (l'opposé de notre tangage) et son lacet (l'opposé du
// nôtre) PLUS le quart de tour, la somme tronquée et non le lacet seul. Un
// argument négatif s'y lit un degré de travers — l'altitude l'est dès que la
// caméra plonge, si bien que le quad penche d'un degré de moins que l'œil.
// Seuls les ANGLES entrent : ni la distance, ni le plancher qui remonte l'œil
// au-dessus du terrain, ni la borne du tangage ne changent cette matrice.
[[nodiscard]] EffectRingOrientation EffectNodeFacingCamera(const Camera& camera,
                                                           float pitch_degrees = 0.0F);

// Le quart de tour que le quad face caméra ajoute au lacet du client avant de le
// tronquer, en degrés.
inline constexpr float kEffectFacingCameraYawQuarterTurnDegrees = 90.0F;

// 🔴 LE LACET DU QUAD ORIENTÉ SUIT L'ANGLE DU CORPS DU NŒUD sous ce bit : à
// chaque tic, son mouvement recopie l'angle du corps dans le lacet, puis y
// ajoute le pas de lacet du tic. Le lacet ne cumule donc rien d'un tic à l'autre.
//
// ⚠ C'EST LE BIT QUI TUE LE NŒUD À SON ARRIVÉE — voir `kEffectNodeDiesAtTarget` :
// un seul bit, deux effets du même mouvement.
//
// ⚠ IL NE COMPTE QUE SUR LA BRANCHE DES ANGLES DU NŒUD : sous
// `kEffectNodeFacesCamera` le lacet est réécrit depuis la caméra avant la
// matrice, et sous `kEffectOrientedQuadLeansByCameraAltitude` il n'est pas lu.
inline constexpr std::uint32_t kEffectOrientedQuadYawFollowsBody = 0x400;

// LE LACET, DANS NOTRE REPÈRE, dont la branche des angles du nœud bâtit la
// matrice du quad orienté. `node_yaw_degrees` et `node_yaw_step_degrees` sont
// dans notre repère, `body_degrees` dans celui du client. Hors
// `kEffectOrientedQuadYawFollowsBody`, c'est le lacet du nœud tel quel.
[[nodiscard]] float EffectOrientedQuadYawDegrees(std::uint32_t flags, float node_yaw_degrees,
                                                 float body_degrees, float node_yaw_step_degrees);

// LE QUAD ORIENTÉ PENCHE DE L'ALTITUDE DE LA CAMÉRA ET TOURNE DE L'ANGLE DU CORPS
// DU NŒUD : c'est ce que dit le bit 0x200 au mouvement des sous-types quinze et
// seize.
//
// ⚠ UN DRAPEAU NE VEUT DIRE QUELQUE CHOSE QUE POUR SON MOUVEMENT (ADR 0017) :
// pour le dessin d'un sprite, ce même bit garde la teinte du calque — voir
// `kEffectNodeKeepsLayerColor`. Les deux sens coexistent, chacun lu par sa
// figure.
//
// 🔴 IL L'EMPORTE SUR `kEffectNodeFacesCamera` : le mouvement l'éprouve
// d'abord, et un nœud qui porte les deux suit cette règle-ci, sans inclinaison
// ni quart de tour.
inline constexpr std::uint32_t kEffectOrientedQuadLeansByCameraAltitude = 0x200;

// VRAI quand le quad orienté suit l'altitude de la caméra — voir
// `kEffectOrientedQuadLeansByCameraAltitude`.
[[nodiscard]] constexpr bool EffectOrientedQuadLeansByCameraAltitude(std::uint32_t flags) {
  return (flags & kEffectOrientedQuadLeansByCameraAltitude) != 0U;
}

// L'ORIENTATION D'UN QUAD ORIENTÉ sous `kEffectOrientedQuadLeansByCameraAltitude`.
//
// Le client pose une rotation autour de z de l'altitude de sa caméra — qui
// écrase la matrice —, puis multiplie une rotation autour de y de l'angle du
// corps du nœud, lu dans sa table.
//
// 🔴 L'ALTITUDE ENTRE EN DEGRÉS LÀ OÙ LA ROTATION ATTEND DES RADIANS, et elle
// n'est ni tronquée ni convertie : quarante-cinq degrés de tangage tournent de
// quarante-cinq RADIANS. C'est le comportement du client, et il se reproduit
// tel quel. Ramenée dans notre repère par `S`, la rotation autour de z prend
// notre tangage (l'opposé de son altitude) et celle autour de y l'opposé de
// l'angle du corps.
[[nodiscard]] EffectRingOrientation EffectNodeLeaningByCameraAltitude(const Camera& camera,
                                                                      float body_degrees);

// L'ORIENTATION D'UN NŒUD, BÂTIE DE SES DEUX ANGLES.
//
// 🔴 UN ANNEAU N'EST PAS TOUJOURS COUCHÉ. Le client bâtit la matrice du nœud en
// tournant d'abord de l'INCLINAISON, puis du CAP, et les figures du monde la
// lisent telle quelle. Sans elle, tout anneau reste à plat sur le sol :
// l'anneau bleu d'un impact, que le client dresse d'un quart de tour, se voyait
// par la tranche.
//
// ⚠ L'ORDRE COMPTE ET IL EST CELUI-LÀ : l'inclinaison bascule le plan, le cap le
// fait tourner ensuite autour de la verticale. L'inverser ferait pencher la
// figure dans une direction qui dépend du cap.
// LE QUART DE TOUR QUE LE TRONC DE CÔNE PORTE DÉJÀ.
//
// 🔴 LE CLIENT ÉCRIT LES DEUX CERCLES DE CETTE FIGURE DANS UN PLAN COUCHÉ et les
// redresse par une rotation de quatre-vingt-dix degrés autour de l'abscisse.
// Nous les bâtissons DEBOUT : deux cercles horizontaux, l'un au plan du nœud,
// l'autre à sa demi-hauteur. L'inclinaison du patron les recoucherait, et
// l'anneau deviendrait un cercle vertical dont la moitié passe sous le sol.
//
// ⚠ LES ONZE PATRONS DE CETTE FIGURE PORTENT LA MÊME VALEUR, sans exception :
// c'est une convention de repère, non un choix par effet. Elle se replie donc
// ici, comme celle des pointes en croix, et la table garde sa valeur intacte.
inline constexpr float kEffectRingCylinderUprightDegrees = 90.0F;

[[nodiscard]] EffectRingOrientation EffectNodeOriented(float yaw_degrees, float pitch_degrees);

// LA MÊME, AVEC LE TROISIÈME ANGLE — le roulis.
//
// 🔴 IL S'APPLIQUE EN PREMIER aux coordonnées locales : le client pose la
// rotation autour de z avec lui, PUIS multiplie l'inclinaison, PUIS le cap. Le
// composer en dernier donnerait une figure juste tant qu'aucun cap ne la tourne.
[[nodiscard]] EffectRingOrientation EffectNodeOriented(float yaw_degrees, float pitch_degrees,
                                                       float roll_degrees);

// L'ANGLE QU'UNE MATRICE DE NŒUD LIT VRAIMENT chez le client, en degrés de son
// repère : tronqué à l'entier, puis lu dans sa table des radians — un degré de
// travers quand il est négatif, moins quatre-vingt-dix comme deux cent
// soixante et onze. Voir `NativeTableRadians`.
[[nodiscard]] float EffectClientMatrixDegrees(float client_degrees);

// 🔴 L'ORIENTATION D'UN NŒUD TELLE QUE LE CLIENT LA BÂTIT : ses trois angles
// passent par ses tables avant de faire la matrice — le cap, donné dans NOTRE
// repère, est ramené au sien pour cela. `pitch_fold_degrees` se retranche
// APRÈS la lecture : c'est une convention de notre figure (voir
// `kEffectRingCylinderUprightDegrees`), et la lire dans la table ajouterait
// un degré de travers que le client n'a pas.
[[nodiscard]] EffectRingOrientation EffectNodeOrientedAsClient(float yaw_degrees,
                                                               float pitch_degrees,
                                                               float roll_degrees,
                                                               float pitch_fold_degrees = 0.0F);

// DE COMBIEN UN QUAD COUCHÉ SE DÉGAGE DU TERRAIN, en unités du monde.
//
// 🔴 C'EST NOTRE CONTRAINTE ET NON CELLE DU CLIENT, et il faut le dire, sans
// quoi quelqu'un ira chercher ce nombre chez le client d'origine et ne le
// trouvera pas. Celui-ci ne lève RIEN : ni son récepteur, ni son bâtisseur, ni
// son dessinateur n'ajoutent de décalage vertical à la position du nœud.
// L'anneau du classement y est donc exactement au sol.
//
// ⚠ CE QUI DIFFÈRE EST L'ÉPREUVE DE PROFONDEUR. Nous éprouvons ces quads contre
// le terrain — délibérément : sans cela les vingt-cinq pointes de terre
// recouvraient entièrement le personnage —, et un quad EXACTEMENT coplanaire au
// sol perd cette épreuve. Il s'enfonce, par morceaux, selon la pente.
//
// Un dixième de cellule : assez pour franchir la précision du tampon, trop peu
// pour qu'on voie l'anneau flotter. C'est un nombre qui se règle contre une
// capture, et non une mesure.
inline constexpr float kEffectFlatQuadClearance = kWorldUnitsPerCell / 10.0F;

// L'ORIENTATION D'UN QUAD ORIENTÉ — les sous-types quinze et seize.
//
// 🔴 FACE À LA CAMÉRA SOUS SON DRAPEAU SEULEMENT. Son mouvement ne bâtit sa
// matrice depuis la caméra que si les drapeaux du nœud portent
// `kEffectNodeFacesCamera` ; sans lui, ce sont les angles du nœud — lacet et
// pente — qui la font, comme pour toute autre figure. Les quatre murs
// d'EF_GLASSWALL portent 0x28 : quatre plans FIXES du monde, tournés d'un quart
// de tour deux à deux, qui ferment une boîte. Tenus pour des billboards, ils se
// posaient tous dans le même sens.
//
// ⚠ ET `kEffectOrientedQuadLeansByCameraAltitude` PASSE AVANT : sous lui, ni les
// angles du nœud ni le bit 0x40 ne comptent, seul l'angle du corps. Sur la
// branche des angles du nœud, le lacet est celui d'`EffectOrientedQuadYawDegrees`.
[[nodiscard]] EffectRingOrientation EffectOrientedQuadFacing(
    std::uint32_t flags, const Camera& camera, float yaw_degrees, float pitch_degrees,
    float roll_degrees, float body_degrees = 0.0F, float yaw_step_degrees = 0.0F);

// Combien de segments la texture couvre avant de se répéter.
//
// Le client avance son abscisse de texture d'un quart par segment et la remet à
// zéro dès qu'elle atteint un : la texture fait donc le tour QUATRE FOIS par
// dix segments, et non une fois par tour.
inline constexpr int kEffectRingSegmentsPerTextureSpan = 4;

// Un tour complet, en degrés. Le dessinateur s'arrête au dernier segment qui
// commence avant le tour : c'est ainsi que l'anneau se ferme sans chevaucher.
inline constexpr float kEffectRingFullTurnDegrees = 360.0F;

// Les quads d'un nœud dessiné en ANNEAU, en points du monde.
//
// La figure n'est pas une bande plate mais un TRONC DE CÔNE debout. Un cercle
// de rayon `radius_at_anchor` est posé au plan du nœud, un second de rayon
// `radius_at_height` à `half_height` de là, et une bande de quads les joint.
// Les deux rayons sont égaux dans la plupart des effets — c'est alors un
// cylindre, comme la colonne de lumière de Heal.
//
// ⚠ LA HAUTEUR MONTE CHEZ NOUS ET DESCEND CHEZ LUI. Le client la compte en
// ordonnée négative parce que son repère descend ; le nôtre monte, comme le
// terrain que nous bâtissons. Reprendre son signe enfoncerait chaque colonne
// sous le sol.
//
// L'ordonnée de texture vaut UN sur le cercle de l'ancre et ZÉRO sur celui de
// la hauteur : la texture se lit donc à l'envers de ce qu'on attendrait, et
// c'est le client qui le veut ainsi.
//
// Rend une liste vide quand l'anneau n'a pas de quoi se dessiner — pas
// angulaire nul ou négatif, qui ferait un tour sans fin.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectNodeRingBand(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation);

// Les quads d'un nœud dessiné en COURONNE PLATE, en points du monde.
//
// La figure n'est PAS un cylindre. Le client multiplie par zéro les trois
// colonnes qui porteraient la troisième coordonnée locale, si bien que les
// quatre coins sont dans le même plan. C'est un disque percé — l'onde de choc
// de Magnum Break, qui s'écarte du lanceur.
//
// La boucle est la MÊME que celle de la bande : deux cercles joints par des
// quads, l'ordonnée de texture valant un sur le premier et zéro sur le second.
// Seules changent les entrées — ici les deux cercles sont au même plan et leurs
// rayons diffèrent, là ils partageaient leur rayon et différaient de hauteur.
//
// LA LARGEUR DE LA COURONNE se déduit, elle ne se stocke pas : le client écrit
// à chaque tic le plus petit de `radius_at_anchor` et de `ring_radius`. Une
// couronne naissante est donc pleine — un disque —, et elle se creuse à mesure
// que son rayon dépasse le plafond.
//
// ⚠ UNE SEULE TRACE. Le client garde un historique de traînée et pose une
// couronne par trace, l'alpha et la largeur décroissant avec le rang. Aucun des
// effets connus n'en demande plus d'une — le constructeur en pose une, et pas
// un ne l'augmente —, et l'historique attendra celui qui le réclamera.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectNodeRingCylinder(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation);

// Les DEUX QUADS PERPENDICULAIRES d'un nœud, en points du monde.
//
// Rien n'y est projeté à l'écran par avance, et il n'y a pas UN plan mais DEUX.
// Le client bâtit huit points locaux — quatre dans le plan de l'ordonnée nulle,
// quatre dans celui de l'abscisse nulle — et en tire deux quads qui se croisent
// le long de leur axe commun.
//
// À QUOI ÇA SERT. Une croix de deux plans se voit sous presque tous les angles,
// là où un seul plan disparaît quand on le regarde par la tranche. C'est ainsi
// que le client fait une étincelle qui ne s'éteint pas quand la caméra tourne —
// les particules d'Increase AGI, de Decrease AGI et de Blitz Beat.
//
// ⚠ LES DEUX DEMI-EXTENSIONS SONT CROISÉES par rapport à leurs noms : la
// demi-HAUTEUR porte l'extension le long du premier axe, et la demi-LARGEUR
// celle de l'axe qu'ils partagent. C'est ce que donnent les huit points, pas ce
// que les noms suggèrent.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectNodeCrossedPlanes(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation);

// Le quad POSÉ AU-DESSUS de son ancre plutôt que centré dessus.
//
// ⚠ LE MÊME BIT VEUT DIRE AUTRE CHOSE AILLEURS : 0x8 fait errer l'angle d'un
// nœud de billboard (`kEffectNodeWandersAngle`). Ce n'est pas une confusion de
// notre part mais celle du client, qui lit ses drapeaux selon le dessinateur ;
// deux noms pour un bit valent mieux qu'un nom qui mentirait à moitié.
inline constexpr std::uint32_t kEffectNodeQuadSitsAboveAnchor = 0x8;

// Le quad PENDU par son bord haut, obtenu en doublant sa demi-hauteur.
inline constexpr std::uint32_t kEffectNodeQuadHangsFromAnchor = 0x80;

// LE QUAD ORIENTÉ d'un nœud, en points du monde.
//
// Ce n'est pas un billboard — rien n'y fait face à la caméra — et ce n'est pas
// une croix. C'est UN SEUL QUAD, quatre points dans le plan de l'abscisse
// locale nulle. Le plus simple des quatre dessinateurs du monde, et celui qui
// porte le plus d'effets : Magic Circle, Shockwave Trap, Thunder Plasma Blast,
// Glass Wall.
//
// ⚠ IL EMPLOIE LA SECONDE MATRICE du nœud, et non la première. Le nœud en porte
// deux, et les confondre orienterait le quad par ce qui tourne l'anneau. C'est
// pourquoi l'orientation lui est passée comme aux autres : ni l'une ni l'autre
// n'est encore alimentée.
//
// L'ORDONNÉE DE TEXTURE EST UNE TRANCHE, entre `texture_v_min` et
// `texture_v_max` : le quad ne prend pas toute son image, ce qui permet
// d'empiler plusieurs motifs dans une même planche. Zéro et un par défaut,
// c'est-à-dire l'image entière.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectNodeOrientedQuad(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation);

// LES QUADS D'UN LOT, DE L'ARRIÈRE VERS L'AVANT : l'ordre où les peindre.
//
// ⚠ SANS LUI, UNE FIGURE FERMÉE SE RETOURNE. Les quads se peignent sans épreuve
// de profondeur — voir `PaintQuads` —, si bien que le dernier peint recouvre.
// Les quatre faces d'une croix de pointes sortaient donc dans l'ordre où la
// figure les avait composées, et les faces ARRIÈRE recouvraient les avant. Cela
// s'est vu en jeu sur le pic de terre, et pas avant : toutes les figures portées
// jusque-là sont OUVERTES — un ruban, une jupe, un carré — et ne se recouvrent
// pas elles-mêmes.
//
// ⚠ LE CLIENT NE L'A PAS PARCE QU'IL TRIE, primitive par primitive, et passe
// les mélangées en dernier. Le renderer triait déjà ses LOTS de la même façon ;
// ce tri-ci est le même, un cran plus bas.
//
// La profondeur d'un quad est celle de son point de tri, le CENTRE sauf quand
// sa figure en choisit un autre (`EffectWorldQuadSortPoint`). Pour une figure
// convexe — une pyramide en est une — l'ordre par le centre est exact.
//
// ⚠ ET LE TRI EST STABLE : deux quads à la même profondeur, les segments
// coplanaires d'un ruban par exemple, gardent l'ordre que leur figure a voulu.
// Un tri instable les échangerait d'une image à l'autre, et le ruban
// scintillerait.
//
// Rend les RANGS, et non les quads : l'appelant tient d'autres tableaux
// parallèles — la texture, le mélange — qu'il doit réordonner de même.
[[nodiscard]] std::vector<std::size_t> EffectWorldQuadsBackToFront(
    std::span<const EffectWorldQuad> quads, glm::vec3 eye);

// Le même ordre, rangé dans `order` ; `distances` lui sert de brouillon. Les
// deux sont vidés d'abord et gardent leur mémoire : un appelant qui trie à
// chaque image n'alloue plus rien une fois ses tampons à la taille.
void EffectWorldQuadsBackToFront(std::span<const EffectWorldQuad> quads, glm::vec3 eye,
                                 std::vector<std::size_t>& order, std::vector<float>& distances);

// UNE PRIMITIVE À PEINDRE : son quad, le lot qui porte sa texture et son mot, et
// le seau où la file la range. `view_depth` est rempli par le tri.
struct EffectPaintedQuad {
  const EffectWorldQuad* quad = nullptr;
  std::size_t lot = 0;
  EffectBucketRank bucket = EffectBucketRank::kSorted;
  float view_depth = 0.0F;
};

// L'ORDRE OÙ PEINDRE DES PRIMITIVES DE PLUSIEURS LOTS : seau par seau, et dans
// un seau, de l'arrière vers l'avant, PRIMITIVE PAR PRIMITIVE.
//
// 🔴 PAS LOT PAR LOT. La file range chaque primitive dans son seau avec sa
// propre profondeur, quel que soit l'effet qui l'a posée. Trier les lots par
// leur point d'appui peindrait d'un bloc les vingt-cinq pointes d'une grille, à
// la profondeur de son centre : les pointes d'Ice Wall voisines passeraient
// toutes dessous ou toutes dessus, au lieu de s'entremêler.
//
// La profondeur d'une primitive est celle de son point de tri
// (`EffectWorldQuadSortPoint`), mesurée le long de l'axe de visée
// `view_forward` (unitaire), depuis l'œil : c'est l'inverse de la coordonnée
// homogène projetée qui range la file, soit la profondeur de vue, et non la
// distance à l'œil. Deux points à la même profondeur, l'un au centre de l'écran
// et l'autre au bord, sont à égalité ; la distance mettrait le second derrière.
//
// ⚠ LE TRI EST STABLE : à égalité, l'ordre d'entrée — les lots de l'arrière
// vers l'avant, puis l'ordre de leur figure.
void OrderEffectPaintedQuads(std::vector<EffectPaintedQuad>& painted, glm::vec3 eye,
                             glm::vec3 view_forward);

// OÙ SE POSE L'EXEMPLAIRE DE RANG DONNÉ, par rapport au point de l'effet.
//
// ⚠ SEULEMENT EN ABSCISSE ET EN PROFONDEUR. Le client écrit aussi une ordonnée
// — dix pour la grille, vingt pour la couronne —, et nous ne la portons PAS :
// rien ne dit encore QUI la lit. Les deux autres, elles, sont certaines : la
// double boucle du bâtisseur de la grille les écrit l'une et l'autre sans
// détour. Poser une élévation qu'on n'a pas mesurée enfoncerait ou soulèverait
// vingt-cinq pointes sur une supposition.
//
// Le générateur n'avance QUE pour une couronne, dont l'angle ET le rayon se
// tirent. ⚠ UNE BORNE HAUTE NULLE VEUT DIRE « RAYON FIXE », et elle ne consomme
// alors aucun tirage : deux couronnes voisines gardent la même suite, qu'elles
// tirent leur rayon ou non.
//
// Le cap est celui du nœud DANS NOTRE MONDE, changement de repère fait : seule
// la paire en miroir le lit, pour se poser le long de la normale que ce cap
// tourne. Les autres dispositions sont fixes dans le plan du sol.
[[nodiscard]] glm::vec3 EffectNodeInstanceOffset(EffectNodeInstancePlacement placement,
                                                 std::uint16_t count, std::uint16_t index,
                                                 float spacing, float spacing_high,
                                                 float yaw_degrees, std::uint32_t& generator,
                                                 std::int32_t spacing_values = 0);

// OÙ NAÎT LE NŒUD SEMÉ À CE TIC quand sa naissance BALAIE un cercle : l'angle est
// le produit du tic par le pas, tronqué au degré comme la table du client, et
// le point se pose dans NOTRE repère. Nul rayon, nulle place.
//
// Voir `EffectNodePattern::birth_sweep_radius`.
[[nodiscard]] glm::vec3 EffectBirthSweepOffset(float radius, float degrees_per_tick,
                                               std::int32_t tick);

// L'ORIENTATION d'une figure PENCHÉE : un lacet autour de la verticale, puis un
// penchant qui l'écarte.
//
// ⚠ LE PENCHANT EST L'ÉCART AU QUART DE TOUR que la figure a déjà replié — voir
// `EffectRingPattern::lean_degrees`.
//
// ⚠ ET L'ORDRE DES DEUX ROTATIONS N'EST PAS CELUI DU CLIENT. Il compose son
// inclinaison puis son lacet sur la même matrice ; nous tournons d'abord autour
// de la verticale. Pour un lacet TIRÉ SUR LE TOUR ENTIER les deux donnent la
// même chose à l'œil — une pointe penchée d'autant, dans une direction
// quelconque —, et c'est le cas de tous les sites connus. Le jour où un
// bâtisseur posera un lacet FIXE, il faudra y revenir.
[[nodiscard]] EffectRingOrientation EffectNodeLeaningOrientation(float lean_degrees,
                                                                 float yaw_degrees);

// L'AXE d'une figure penchée : la verticale, une fois penchée puis lacée.
//
// C'est la direction où pointe le SOMMET d'une croix de pointes, et celle où
// son pas la déplace — le client emploie la même ligne de matrice pour les
// deux. Rendu à part parce qu'un nœud qui avance n'a besoin que de lui, sans la
// matrice entière ; et c'est `EffectNodeLeaningOrientation` qui s'en sert, pour
// que les deux ne puissent pas diverger.
//
// Le vecteur est UNITAIRE : le pas porte la distance, l'axe la direction.
[[nodiscard]] glm::vec3 EffectNodeLeanAxis(float lean_degrees, float yaw_degrees);

}  // namespace ro::render
