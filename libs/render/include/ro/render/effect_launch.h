#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_sprite_draw_order.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {

// ── LE LANCER D'UNE PARTICULE ───────────────────────────────────────────────
//
// 🔴 UNE PARTICULE PART DANS UNE DIRECTION, ET CETTE DIRECTION VIENT DE DEUX
// ANGLES. Le client tourne une matrice par le CAP, dans le plan du sol, puis par
// l'INCLINAISON, et avance d'un pas le long de sa TROISIÈME COLONNE — tic après
// tic, en accumulant.
//
// Sans cela une nuée se pose à plat autour de son point et n'en part jamais.
// ⚠ Et cela ne ressemble pas à une nuée incomplète : cela ressemble à une nuée
// FIGÉE, ce qui envoie chercher la faute dans l'animation du sprite.
//
// ⚠ L'INCLINAISON ÉLÈVE, ELLE NE FAIT PAS TOURNER : elle se prend sur l'axe
// vertical, et les deux autres composantes en gardent le cosinus. La confondre
// avec un second cap ferait tourner la gerbe au lieu de l'ouvrir.
//
// ⚠ LE CAP EST RELATIF AU PORTEUR. Le client l'écrit comme l'orientation de
// l'acteur PLUS un tirage, et l'appelant seul sait où l'acteur regarde : la
// plage du patron ne porte que l'ÉCART, et la face s'y ajoute à l'émission.
// Sans quoi toute gerbe du jeu s'ouvrirait du même côté du monde.
//
// Trois choses courbent ensuite la trajectoire, et une seule est commune :
//
//   · LE FREINAGE. Le pas perd une part de lui-même à chaque tic, rapportée à
//     la vie du nœud — un demi pour les impacts. ⚠ IL EST BORNÉ À ZÉRO par un
//     `fmaxf` : sans la borne, une particule ralentie repartirait
//     EN ARRIÈRE, ce qui se voit tout de suite sur une gerbe.
//   · LA CHUTE. Une vitesse verticale avec sa propre accélération, ajoutée à
//     l'ordonnée. ⚠ C'EST ELLE, ET ELLE SEULE, qui sépare la famille qui
//     S'ÉCARTE de celle qui RETOMBE : les deux partagent leur sprite, leur
//     échelle et leur pas, et un patron qui ne la nomme pas les rend
//     indiscernables.
//   · LE REBOND sur le terrain, que nous ne portons pas encore : le client
//     divise la vitesse verticale par moins un et demi quand la particule
//     repasse au-dessus du sol.
//
// Ce modèle vit à part de la troupe parce qu'il ne parle que de mouvement : ni
// figure, ni texture, ni horloge.

// L'ANGLE DE BASE D'UN EFFET, celui dont partent le cap d'une particule ET
// l'orientation d'un anneau. `direction` est celle du serveur, zéro au nord.
//
// 🔴 LE CLIENT LE COMPOSE, IL NE LE LIT PAS : son bâtisseur écrit
// `180 − angle du corps du porteur`, et cet angle vaut lui-même
// `45 × (direction + 4)` — d'où, les cent quatre-vingts s'annulant, MOINS
// QUARANTE-CINQ DEGRÉS PAR DIRECTION.
//
// La chaîne se ferme entièrement DANS le client :
//
//   · l'orientation d'un acteur vers un point écrit l'angle du corps,
//     `atan2(Δx, −Δz) × 180/π` ramené dans [0, 360) ;
//   · la conversion du monde en cases donne le sens des axes :
//     `case = ⌊moitié + monde / pas⌋` sur les DEUX, donc une case croît avec
//     son axe. Les écarts `dirx`/`diry` du serveur y tombent sur
//     `45 × (direction + 4)`, la table entière ;
//   · la conversion d'un angle en huit directions rend `(angle + 180) / 45 % 8`,
//     et appliquée à cet angle elle rend EXACTEMENT la direction du serveur.
//     C'est la contre-épreuve : le client émet ce nombre-là quand l'acteur se
//     tourne, et c'est bien celui que le serveur y attend.
//
// ⚠ ET LE ZÉRO N'EST PAS UNE ABSENCE. Un porteur en direction zéro donne un
// angle de base nul : ne rien passer revient à dire « tourné vers le nord », et
// non « pas d'orientation ». Une gerbe sans porteur s'ouvre donc vers le haut
// de la carte, ce qui est un choix et non un défaut.
//
// ⚠ C'EST POURQUOI AUCUN PATRON NE LE PORTE. La valeur n'est pas dans le
// bâtisseur, elle
// est relue du PORTEUR à chaque semis. Un champ qui prétendrait la mesurer
// figerait toutes les gerbes du jeu du même côté du monde.
[[nodiscard]] constexpr float EffectOwnerFacingDegrees(std::uint8_t direction) {
  return -45.0F * static_cast<float>(direction);
}

// LE MÊME CAP, DANS NOTRE REPÈRE.
//
// 🔴 NOTRE MONDE N'EST PAS CELUI DU CLIENT : il a l'ordonnée ET la profondeur
// opposées aux siennes, et `terrain_mesh.cpp` le dit sur les deux axes — « l'axe
// vertical du monde monte, celui du fichier descend », et la rangée de cubes
// s'inverse de même, si bien qu'une case CROÎT quand notre Z décroît.
//
// Deux axes retournés composent une rotation d'un demi-tour autour du premier.
// Un cap y tourne donc À L'ENVERS : tout angle lu chez le client passe par ici
// avant d'atteindre une figure.
//
// ⚠ CE N'EST PAS UNE CORRECTION D'ORIENTATION, C'EST UN CHANGEMENT DE REPÈRE.
// La distinction décide de ce qu'on cherche quand une figure paraît de travers :
// un angle faux se corrige dans le patron, un repère faux se corrige ICI, pour
// tout le monde à la fois.
[[nodiscard]] constexpr float EffectClientYawInWorld(float client_yaw_degrees) {
  return -client_yaw_degrees;
}

// LA MÊME PLACE, DANS NOTRE REPÈRE, et c'est le MÊME changement que ci-dessus :
// l'ordonnée et la profondeur du client sont les nôtres au signe près.
//
// ⚠ L'ORDONNÉE EST CELLE QUI SE VOIT. L'anneau bleu d'un impact porte moins dix,
// ce qui le MONTE de deux cellules chez le client ; posé tel quel chez nous il
// s'enfonce d'autant et naît aux pieds au lieu de la hauteur du corps.
[[nodiscard]] inline glm::vec3 EffectClientOffsetInWorld(float x, float y, float z) {
  return glm::vec3{x, -y, -z};
}

// CE QU'UN LANCER TIRE, une fois pour toute la vie de la particule.
struct EffectLaunchDraw {
  // Unitaire, ou NULLE quand rien ne part — le cas de tout ce qui n'est pas une
  // particule lancée.
  glm::vec3 direction{0.0F, 0.0F, 0.0F};
  // À ajouter au point de l'effet pour placer la particule : la distance le
  // long de la direction, plus la descente du départ.
  glm::vec3 offset{0.0F, 0.0F, 0.0F};
  // Négatif : ce que le pas perd à chaque tic.
  float step_decay = 0.0F;
  float fall_speed = 0.0F;
  float fall_acceleration = 0.0F;
  // LE BALANCEMENT du sous-type 8, dans les unités du client : négatif pour
  // un écart vers le haut, comme sa vitesse verticale. Voir
  // `PooledEffectNode::swing_amount`.
  float swing_speed = 0.0F;
  float swing_acceleration = 0.0F;
};

struct EffectNodeSeed;
struct PooledEffectNode;

// LE LANCER D'UNE PARTICULE, tirages faits. `facing_degrees` est l'orientation
// du PORTEUR, à laquelle l'écart de cap s'ajoute.
//
// ⚠ RIEN N'EST TIRÉ QUAND LA PLAGE DE PAS EST VIDE. Un nœud immobile ne doit pas
// consommer de tirage : le générateur avancerait, et tous les nœuds suivants
// d'une troupe changeraient de taille pour une raison invisible.
[[nodiscard]] EffectLaunchDraw DrawEffectLaunch(const EffectNodeSeed& seed, float facing_degrees,
                                                std::int32_t life_ticks, float step_length,
                                                std::uint32_t& generator);

// LES DEUX BOUTS D'UN VOL : où le nœud naît, et ce qu'il vise — rien quand il
// ne vise rien.
struct EffectFlightEnds {
  glm::vec3 birth{0.0F, 0.0F, 0.0F};
  std::optional<glm::vec3> aim;
};

// LES BOUTS DU VOL D'UN NŒUD, selon sa graine : le point de l'effet et sa
// cible, ou l'inverse quand le vol est RETOURNÉ — voir
// `EffectNodePattern::flies_from_target`. Sans cible, rien ne se retourne : le
// nœud naît au point de l'effet et ne vise rien.
[[nodiscard]] EffectFlightEnds EffectNodeFlightEnds(const EffectNodeSeed& seed,
                                                    glm::vec3 effect_point,
                                                    const std::optional<glm::vec3>& target);

// COMBIEN DE TICS UN NŒUD PLANTÉ RESTE AVANT DE S'EFFACER : au sol, son âge
// saute à sa vie moins ceci — c'est le mouvement des sous-types quinze et
// seize.
inline constexpr std::int32_t kEffectNodeRestAfterImpactTicks = 25;

// UN SON QUE LA TROUPE FAIT ENTENDRE : le nom à jouer et l'endroit où il se
// joue. Deux sources, à des instants que seule la troupe connaît : le REBOND
// d'une pièce, que la mise à jour du nœud déclenche, et le SEMIS d'une boule,
// que son bâtisseur sonne dans sa garde — voir `EffectNodePattern::sound_at_sowing`.
//
// ⚠ C'EST UNE DONNÉE, PAS UN APPEL. `render` ne connaît pas l'audio ; la troupe
// range ses sons du tic, l'appelant les emporte et les joue. C'est le même
// chemin que les unités semées et les effets lancés.
struct EffectNodeSound {
  // Le nom FORMÉ, variante choisie : plus aucun `%d` n'y reste.
  std::string sound;
  glm::vec3 position{0.0F, 0.0F, 0.0F};
};

// DE COMBIEN UN REBOND DIVISE LA VITESSE VERTICALE, en la retournant :
// `vitesse /= -1,5`. La particule repart vers le haut, plus lentement.
inline constexpr float kEffectNodeBounceDivisor = -1.5F;

// AVANCE UNE PARTICULE LANCÉE d'un tic. Sans effet sur ce qui n'est pas lancé.
// Un nœud qui s'arrête au sol s'y plante — voir `PooledEffectNode::stops_at_floor` —,
// un nœud qui REBONDIT y retourne sa vitesse — voir `bounces_at_floor`.
//
// ⚠ LE MONDE RÉPOND POUR LE SOL, comme chez le client : il relit la hauteur du
// terrain SOUS LA PARTICULE à chaque tic, et non celle
// de son point de naissance. Sur une pente, les deux diffèrent. Un monde sans
// réponse laisse le nœud passer : voir `EffectScatterWorld::ground_height`.
void AdvanceLaunchedNode(PooledEffectNode& pooled, const EffectScatterWorld& world);

// LA VOIE RADIALE ACCÉLÉRÉE d'un nœud, dans notre repère : sa vitesse, ce qui
// s'y ajoute à chaque tic, et ce qui les REMPLACE toutes deux à la bascule. Voir
// `EffectNodePattern::radial_speed`.
//
// ⚠ CE N'EST PAS UN LANCER : un lancer avance d'un pas le long d'une direction
// unitaire, et son pas ne fait que freiner. Ici c'est un VECTEUR qui s'intègre,
// et sa dérive l'écarte de son cap — la particule tourne après la bascule.
//
// Tout nul et une bascule à moins un : pas de voie, le cas de presque tous.
struct EffectRadialLane {
  glm::vec3 velocity{0.0F, 0.0F, 0.0F};
  glm::vec3 acceleration{0.0F, 0.0F, 0.0F};
  glm::vec3 velocity_after_switch{0.0F, 0.0F, 0.0F};
  glm::vec3 acceleration_after_switch{0.0F, 0.0F, 0.0F};
  // Combien de tics avant la bascule ; moins un quand elle ne viendra pas.
  std::int32_t ticks_to_switch = -1;
};

// 🔴 L'ANGLE QUE LES TABLES TRIGONOMÉTRIQUES DU CLIENT LISENT VRAIMENT, pour
// un angle entier de [−360, 360]. Un angle négatif s'y lit UN DEGRÉ À CÔTÉ :
// sa table des négatifs est indexée en arrière depuis la fin de celle des
// positifs, qui compte trois cent soixante et UNE entrées. Moins quarante-cinq
// s'y lit donc comme trois cent seize, et non trois cent quinze.
[[nodiscard]] constexpr std::int32_t EffectClientTableDegrees(std::int32_t degrees) {
  constexpr std::int32_t kTableEntries = 361;
  return degrees < 0 ? degrees + kTableEntries : degrees;
}

// LE CAP, DANS NOTRE REPÈRE, D'UNE DIRECTION QUE LE BÂTISSEUR ÉCRIT
// `(sin a, 0, −cos a)` depuis un angle entier `a` — les deux lectures de table
// du client, avec leur degré de travers. Notre bras de dispersion tiré à ce cap
// pointe là où la particule du client part.
[[nodiscard]] constexpr float EffectClientSinCosHeadingInWorld(std::int32_t client_degrees) {
  constexpr float kQuarterTurnDegrees = 90.0F;
  return kQuarterTurnDegrees - static_cast<float>(EffectClientTableDegrees(client_degrees));
}

// 🔴 LE CAP TIRÉ SUR LA GRILLE 7×7 DU CLIENT, et la direction qu'il donne dans
// notre repère. Le bâtisseur tire la PROFONDEUR puis l'ABSCISSE d'un point
// `rand() % 7 − 3` autour de l'instance, tourne le nœud vers lui
// (`atan2(dx, −dz)` en degrés flottants, ramené sur le tour), pose le cap
// OPPOSÉ, le TRONQUE à l'entier et le lit dans ses tables — un degré de travers
// pour un négatif. La direction est la troisième ligne de la rotation,
// `(sin, 0, cos)` dans son repère.
//
// ⚠ QUARANTE-HUIT DIRECTIONS ET UN CENTRE. Le point central, sans direction,
// laisse le nœud tourné vers zéro : son cap est nul, une fois sur quarante-neuf.
// ⚠ ET LA TRONCATURE SE JOUE AU CHEVEU sur les vingt-quatre cases dont l'angle
// tombe juste — axes et diagonales : le quotient par l'approximation de π que
// le client emploie les pousse un rien au-delà de l'entier, et le cap d'un
// angle ramené sur le tour y perd un degré. Le calcul se fait donc en
// flottants, dans l'ordre du client.
[[nodiscard]] glm::vec3 DrawEffectClientGridHeading(std::uint32_t& generator);

// LA DIRECTION DE VOL D'UN NŒUD TOURNÉ VERS UN POINT, dans notre repère.
//
// Le nœud prend la face qui le tourne vers `toward` (vecteur du monde, depuis
// lui), la face reçoit `face_offset_degrees` et se ramène sur le tour, puis le
// cap est l'OPPOSÉ de la face, tronqué et lu dans la table du client. La ligne
// de la rotation ainsi bâtie tourne le dos au point : c'est le pas NÉGATIF que
// le client écrit qui ramène le vol vers lui, et la direction rendue ici est
// donc déjà retournée.
//
// ⚠ UN VECTEUR NUL laisse la face à zéro, avant l'écart.
[[nodiscard]] glm::vec3 EffectClientAimedHeading(glm::vec3 toward, float face_offset_degrees);

// LES ANGLES D'UNE FLÈCHE QUI TOMBE SUR SON POINT, dans le repère du client. Elle
// naît à `(start_x, −height, start_z)` de l'instance et vise `(aim_x, aim_z)` :
// son cap est l'OPPOSÉ de la face tournée vers le point (`atan2(Δx, −Δz)` en
// degrés, ramené sur le tour, π à six décimales), son inclinaison
// `atan2(height, distance au sol de sa naissance)` — la distance à
// l'instance, et non au point visé.
[[nodiscard]] EffectProjectileAngles EffectClientArrowAngles(float start_x, float start_z,
                                                             float height, float aim_x,
                                                             float aim_z);

// L'ANGLE D'ÉCRAN D'UN PROJECTILE D'ACTEUR — la flèche d'un arc, la balle d'un
// monstre —, en degrés entiers dans [0, 360), tel que le client le recompose à
// chaque image : la FACE de son vol vue de la caméra, du LACET seul.
//
// La face est `atan2(Δx, −Δz)` du vol dans le repère du client, en degrés
// flottants ramenés sur le tour, π à six décimales ; l'angle est
// `−lacet de la caméra − face`, TRONQUÉ à l'entier puis ramené sur le tour. Le
// dessinateur des sprites tourne alors le quad de cet angle dans le sens des
// aiguilles d'une montre, l'ordonnée d'écran descendant — la rotation même de
// `ComposeEffectNodeSprite`, tables du client comprises.
//
// 🔴 LE LACET SEUL, ET NON LA PROJECTION. Le client ne tient aucun compte de la
// plongée de la caméra : un vol en diagonale se dessine à quarante-cinq degrés
// exactement, un peu à côté de sa trajectoire projetée, et c'est ce que le
// joueur voit. `ScreenRotationOfDirection`, elle, suit la projection : ce n'est
// pas le même angle, et une flèche ne passe pas par elle.
//
// 🔴 ET L'IMAGE POINTE VERS LE BAS AU REPOS. La flèche générique porte son
// empennage en haut et sa pointe en bas : au cap zéro devant la caméra par
// défaut — un vol qui vient vers le joueur — elle se dessine telle quelle, et
// c'est la pointe qui mène. Une image prise pointant vers le haut la faisait
// voler à l'envers.
//
// `flight_direction` est dans notre repère, sa hauteur ignorée ; le lacet est
// `Camera::yaw_degrees`, l'opposé de celui du client. Un vol nul laisse la face
// à zéro, comme chez le client dont la face garde alors sa valeur de naissance.
[[nodiscard]] std::int32_t EffectProjectileScreenAngleDegrees(glm::vec3 flight_direction,
                                                              float camera_yaw_degrees);

// LE MÊME ANGLE EN DEGRÉS RÉELS, ni tronqué ni ramené sur le tour : ce que le
// dessinateur de particules reçoit pour la munition en sprite d'un monstre,
// dont il tourne le quad d'un angle flottant. Sa face se calcule à l'opposé
// de celle de la flèche — `atan2(−Δx, −Δz)` chez le client — et se retranche
// du lacet au lieu de s'y ajouter : le même tour, au signe près, et le même
// angle sur le tour. `EffectProjectileScreenAngleDegrees` en est la
// troncature.
[[nodiscard]] float EffectProjectileScreenAngle(glm::vec3 flight_direction,
                                                float camera_yaw_degrees);

// LA TROISIÈME LIGNE DE `RotX(inclinaison) · RotY(cap)`, les deux angles
// tronqués et lus dans les tables du client, dans NOTRE repère : la direction
// le long de laquelle un pas positif fait avancer le nœud.
[[nodiscard]] glm::vec3 EffectClientPitchedHeading(float yaw_degrees, float pitch_degrees);

// L'ACTION D'UN SPRITE QUI MONTRE SA DIRECTION PAR SON IMAGE, selon le point
// visé vu de la caméra.
//
// Le client prend l'angle entier du point visé vers l'instance, dans son
// repère, lui ajoute l'angle de la caméra — le complément de
// `billboard_turn_degrees` — et le ramène sur le tour. Un quart de tour par
// action, dans l'ordre de `actions` : pour la flèche verte, 0 de 0 à 89, 6 de
// 90 à 179, 4 de 180 à 269, 2 au-delà.
[[nodiscard]] std::int32_t EffectAimQuadrantAction(
    glm::vec3 instance, glm::vec3 aim, std::int16_t billboard_turn_degrees,
    const EffectActionsByQuarter& actions = kEffectArrowActionsByQuarter);

// LA VOIE D'UN NŒUD PARTI AU CAP DONNÉ, en degrés — le même que celui de son bras
// de dispersion, si bien qu'il file droit hors de sa rosace. Sans vitesse, elle
// est vide.
[[nodiscard]] EffectRadialLane StartEffectRadialLane(const EffectNodeSeed& seed,
                                                     float heading_degrees);

// AVANCE D'UN TIC UN NŒUD SUR SA VOIE RADIALE, dans l'ordre du client : la
// bascule d'abord si c'est son tic, puis la vitesse gagne son accélération,
// puis la place gagne sa vitesse — dès le premier tic. Sans effet sur un nœud
// qui n'a pas de voie.
void AdvanceRadialNode(PooledEffectNode& pooled);

}  // namespace ro::render
