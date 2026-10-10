#pragma once

#include <array>
#include <limits>

#include <glm/glm.hpp>

namespace ro::render {

// Caméra de type Ragnarok : elle orbite autour d'un point de visée.
//
// Aucune dépendance à OpenGL — ce ne sont que des matrices. Les erreurs qui
// coûtent cher ici (axe inversé, champ de vision, dégénérescence au zénith) se
// vérifient donc par des assertions numériques.
struct Camera {
  glm::vec3 look_at{0.0F, 0.0F, 0.0F};
  float distance = 200.0F;

  // Rotation autour de l'axe vertical, en degrés.
  float yaw_degrees = 0.0F;

  // Élévation au-dessus de l'horizon, en degrés. Bornée à ±89 par Position() :
  // à 90 la direction de visée devient colinéaire au vecteur « haut » et la
  // matrice de vue dégénère — l'image devient noire sans la moindre erreur.
  float pitch_degrees = 45.0F;

  // Le client officiel emploie un champ ÉTROIT, et c'est ce qui donne à RO son
  // aspect presque orthographique. 45° est le réglage « éditeur », plus large
  // et plus commode pour inspecter une carte.
  float fov_degrees = 15.0F;

  float z_near = 1.0F;
  float z_far = 99999.0F;

  // Posée par le jeu, qui connaît le terrain : un œil plus bas y est remonté.
  float eye_floor_height = std::numeric_limits<float>::lowest();

  [[nodiscard]] glm::vec3 Position() const;
  [[nodiscard]] glm::mat4 ViewMatrix() const;
  [[nodiscard]] glm::mat4 ProjectionMatrix(float aspect_ratio) const;
};

// Élévation maximale acceptée, en degrés.
inline constexpr float kMaxPitchDegrees = 89.0F;

// Bornes de distance de la caméra, en unités du monde. Sans la borne basse, un
// cran de trop la place au centre exact et l'image devient noire.
//
// ⚠ ELLES VIVAIENT AVEC LE CLIC, et n'avaient rien à y faire : ce sont des
// faits de CAMÉRA. Ce qui les a déplacées est l'écran de veille, qui recule la
// caméra jusqu'à ce plafond sans jamais toucher à la souris — et qui ne peut
// pas connaître le monde.
inline constexpr float kMinCameraDistance = 10.0F;
inline constexpr float kMaxCameraDistance = 100000.0F;

// La caméra du JEU, mesurée contre une capture du client.
//
// Les trois valeurs vont ensemble. Le champ étroit est ce qui donne à RO son
// aspect presque orthographique : un arbre au bord de l'écran y a la même taille
// qu'au centre. Le rendre avec le champ large de la visionneuse enfle les objets
// proches du bord et laisse voir le flanc des bâtiments — la scène est juste, et
// pourtant elle ne ressemble pas au jeu. Le champ et le recul ne se règlent pas
// non plus séparément : ce qui fixe la taille apparente au centre est leur
// PRODUIT, distance × tan(champ / 2).
//
// COMMENT ELLES ONT ÉTÉ MESURÉES
//
// En cherchant, sur une grille de zones réparties dans l'image, le décalage
// local qui superpose notre rendu à une capture du client, puis en ajustant une
// droite « décalage = origine + pente × (position − centre) ».
//
// Un décalage identique partout n'est qu'un cadrage : la même scène, vue d'un
// pouce à côté. Une PENTE, elle, dit que l'écart grandit à mesure qu'on
// s'éloigne du centre — c'est une différence de caméra. Et les lire séparément
// en horizontal et en vertical SÉPARE les deux réglages : l'élévation comprime
// la verticale sans toucher l'horizontale, le recul agit sur les deux.
//
// Pentes mesurées sur gonryun, horizontale puis verticale :
//
//     élévation 45   +0,013   −0,041     <- ce que l'on croyait
//     élévation 48   +0,012   −0,010
//     élévation 50   +0,013   +0,011
//     élévation 52   +0,013   +0,032
//
// Elles s'accordent à cinquante degrés ; le recul absorbe ensuite le pour cent
// qui leur reste commun. À cinquante degrés et mille deux cent vingt-quatre
// unités, les deux pentes tombent à +0,001 et 0,000 : l'image se superpose
// PARTOUT, et non plus seulement en son centre.
inline constexpr float kGameFovDegrees = 15.0F;
inline constexpr float kGameElevationDegrees = 50.0F;
inline constexpr float kGameViewDistance = 1224.0F;

// COMBIEN UN OBJET DU MONDE PARAÎT PLUS GRAND QU'AU RECUL DU JEU.
//
// Un à `kGameViewDistance`, deux quand on a zoomé jusqu'à la moitié de ce
// recul, un demi quand on a reculé du double.
//
// 🔴 CE QUI FIXE LA TAILLE APPARENTE EST LE PRODUIT `distance × tan(champ / 2)`,
// et non le recul seul — c'est écrit plus haut, et c'est pourquoi les deux se
// lisent ensemble ici. Le champ ne bouge pas en jeu ; l'écrire quand même est
// gratuit et rend la réponse juste le jour où il bougera.
//
// ⚠ ELLE NE DIT RIEN DE LA RÉSOLUTION. Un objet occupe plus de PIXELS sur une
// fenêtre plus haute, mais ce que cette échelle sert à suivre est le ZOOM : ce
// qui doit suivre la fenêtre est l'échelle de l'interface, qui est un réglage à
// part.
[[nodiscard]] float ApparentScale(const Camera& camera);

// L'OREILLE DROITE DE L'AUDITEUR, dans notre repère, comme le client la tourne.
//
// 🔴 LE CLIENT LA TIRE DU LACET DU JEU, TRONQUÉ AU DEGRÉ : l'angle
// `a = 180 − lacet du jeu` se lit dans ses tables — dont un angle négatif se lit
// un degré de travers —, la face de l'auditeur est `(sin a, 0, −cos a)` dans son
// repère et sa droite `(−cos a, 0, −sin a)`, soit `(−cos a, 0, sin a)` dans le
// nôtre. Le lacet du jeu est l'opposé de `yaw_degrees`.
[[nodiscard]] glm::vec3 ClientListenerRight(const Camera& camera);

// ── CE QUI S'ANCRE SUR UN ACTEUR : LA MÉTHODE DU CLIENT ─────────────────────
//
// 🔴 L'ÉCART SUIT LA TAILLE APPARENTE DU SPRITE, ET LA TAILLE NE SUIT RIEN.
// Le client repose la jauge d'un acteur à chaque image, et son ordonnée vaut
// exactement :
//
//     y_des_pieds + écran_480(12) × échelle_de_dessin
//
// où `écran_480(v)` vaut `v × hauteur_de_la_fenêtre / 480`.
//
// 🔴 ET L'ÉCHELLE DE DESSIN EST CELLE DU SPRITE À L'ÉCRAN, PERSPECTIVE
// COMPRISE :
//
//     échelle_de_dessin = profondeur_vers_écran(p) × échelle_du_corps / 1,34
//
// où `profondeur_vers_écran(p)` vaut `facteur_d_écran × p / 7`.
// Elle porte donc le RECUL DE LA CAMÉRA autant que la taille du corps : c'est
// ce qui garde l'étiquette collée au sprite à tous les zooms. La lire comme la
// seule « taille du corps » laisse l'étiquette décrocher dès qu'on zoome.
//
// 🔴 La taille du widget — soixante sur cinq — n'est mise à l'échelle par
// rien. Seule l'ORDONNÉE passe par là.

// La hauteur d'écran à laquelle le client rapporte tout ce qu'il place.
inline constexpr float kClientReferenceHeight = 480.0F;

// L'ÉCHELLE D'UN ANCRAGE POSÉ SUR UN ACTEUR.
//
// Les trois parts du client, dans l'ordre où il les compose :
//
//   `viewport_height`  la fenêtre, rapportée à 480 ;
//   `apparent_scale`   ce que le RECUL fait à la taille — voir `ApparentScale`,
//                      qui tient lieu du passage profondeur → écran du client ;
//   `body_scale`       ce dont l'acteur ENTIER est grandi à cette image, voir
//                      `DrawnActor::scale`.
//
// ⚠ AUCUNE BORNE, ET C'EST VOULU : une borne casse la proportionnalité aux
// extrêmes, c'est-à-dire exactement là où l'ancrage se voit décrocher. Le client
// n'en a pas.
//
// ⚠ ELLE NE S'APPLIQUE QU'À CE QUI SE MESURE DEPUIS L'ACTEUR — un écart, une
// hauteur de pile. Jamais à ce qui se mesure en soi : une largeur de barre, une
// taille de police.
[[nodiscard]] float ScreenAnchorScale(float viewport_height, float apparent_scale,
                                      float body_scale);

// Recule la caméra jusqu'à ce que l'emprise donnée tienne entièrement dans le
// champ, et la vise en son centre.
//
// C'est ce qui permet de rendre une carte entière sans réglage manuel — donc de
// comparer des images automatiquement.
[[nodiscard]] Camera FrameBounds(const std::array<float, 3>& bounds_min,
                                 const std::array<float, 3>& bounds_max, float aspect_ratio,
                                 float fov_degrees = 45.0F, float pitch_degrees = 45.0F,
                                 float yaw_degrees = 0.0F);

}  // namespace ro::render
