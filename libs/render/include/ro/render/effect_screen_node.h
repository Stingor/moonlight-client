#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_screen_quad.h"

namespace ro::render {

// L'ÉTAT D'UN NŒUD DE LA VOIE D'ÉCRAN, et ce qu'un tic en fait.
//
// Porte le mouvement du sous-type 0 et celui du sous-type 1, que le premier
// appelle après avoir fait le sien. Les porter comme deux mouvements
// séparés en écrirait un deux fois.
//
// ⚠ SES LONGUEURS SONT DES PIXELS, pas des unités du monde. Voir l'ADR 0022 :
// les sommets de cette voie sortent déjà transformés.
//
// Les défauts ci-dessous sont ceux du CONSTRUCTEUR du client, pas
// des zéros : un champ que le bâtisseur ne pose pas garde celui-ci. Un alpha
// omis vaut 254, pas 0, et une vie omise vaut cent tics.

// ⚠ LA SENTINELLE QUI VEUT DIRE « NE CHANGE PAS ». Au tic du second pas, six
// champs « d'après » remplacent six champs courants — mais seulement ceux qui ne
// valent pas ce dix-millième. Le prendre pour une valeur poserait un pas de
// croissance minuscule au lieu de n'en poser aucun.
inline constexpr float kEffectScreenUnchanged = 0.0001F;

// LE PAS ANGULAIRE DU CONSTRUCTEUR : trente-six degrés, soit dix
// bandes qui ferment exactement le tour. Voir `EffectScreenNode::angle_degrees`.
inline constexpr float kEffectScreenDefaultStepDegrees = 36.0F;

// CE QU'UN NŒUD D'ÉCRAN DESSINE.
//
// ⚠ LA FORME LIE LA FIGURE ET LE MOUVEMENT, comme dans la troupe du monde :
// les trois sous-types de cette voie ne bougent pas de la même façon, et les
// faire battre par le même code donnerait à l'un le rythme de l'autre.
enum class EffectScreenShape : std::uint8_t {
  // UN QUAD TOURNÉ, sous-types 0 et 1 — la figure d'origine de cette voie.
  kQuad,
  // UNE COURONNE balayée par bandes, sous-type 4.
  kRing,
  // UN TRIANGLE ISOCÈLE pointe au centre, sous-type 3.
  kWedge,
  // UN TRAIT FENDU, sous-type 2 : deux triangles qui partagent une arête, leurs
  // pointes de part et d'autre. Voir `effect_split_beam.h`.
  //
  // ⚠ IL BOUGE COMME UN QUAD : le répartiteur des mouvements range les
  // sous-types 0 et 2 dans le même cas. Seule sa forme le
  // sépare du quad, pas son rythme — au contraire de la couronne et de l'éclat,
  // qui ont chacun le leur.
  kSplitBeam,
};

// LES DRAPEAUX DU NŒUD, tels que les fonctions les lisent.
//
// ⚠ LE DRAPEAU UN DIT TROIS CHOSES DIFFÉRENTES, une par forme, et rien dans
// le champ ne le signale (ADR 0017). Pour le quad il fait PENDRE la figure
// sous son point ; pour la couronne il pousse sa bande jusqu'au CENTRE, ce
// qui en fait un disque plein ; pour l'éclat il FIGE la croissance dès que le
// rayon s'annule. Lui donner un sens unique le ferait pendre là où il doit
// remplir.
enum class EffectScreenFlag : std::uint32_t {
  // Le quad PEND sous son point au lieu d'être centré dessus. Pour la
  // couronne, sa bande va jusqu'au centre ; pour l'éclat, la croissance se
  // fige à rayon nul.
  kHangsBelowAnchor = 1U << 0U,
  // Passé l'âge que porte `growth_stop_tick`, les quatre pas de taille tombent
  // à zéro d'un coup.
  kStopsGrowing = 1U << 1U,
  // La position d'écran est ABSOLUE : le décalage EST la position, au lieu de
  // s'ajouter à celle du porteur.
  kScreenAbsolute = 1U << 2U,
  // Le cap dérive au sort, par périodes.
  kZigzags = 1U << 3U,
  // L'alpha fait un VA-ET-VIENT entre ses bornes au lieu de monter une fois.
  kSwingsAlpha = 1U << 7U,
};

[[nodiscard]] constexpr bool HasEffectScreenFlag(std::uint32_t flags, EffectScreenFlag flag) {
  return (flags & static_cast<std::uint32_t>(flag)) != 0U;
}

struct EffectScreenNode {
  EffectScreenShape shape = EffectScreenShape::kQuad;

  // L'ÂGE ET LA VIE, en tics. La vie omise vaut cent.
  std::int32_t age_ticks = 0;
  std::int32_t life_ticks = 100;
  // Le tic où l'alpha se met à tomber vers zéro. MOINS UN veut dire « jamais ».
  std::int32_t fade_out_tick = -1;
  // ⚠ ET LE TIC OÙ SIX CHAMPS EN REMPLACENT SIX AUTRES. Moins un veut dire
  // « jamais » ; le client compare par ÉGALITÉ, si bien qu'un tic manqué ne
  // rattrape pas.
  std::int32_t second_step_tick = -1;

  // OÙ LE QUAD SE POSE, en pixels, par rapport à son porteur — ou dans l'absolu
  // quand le drapeau le dit.
  glm::vec2 screen_offset = {0.0F, 0.0F};
  // Le déplacement CUMULÉ depuis la naissance, que le cap fait avancer.
  glm::vec2 travel = {0.0F, 0.0F};

  // LA TAILLE ET SA CROISSANCE, en pixels par tic.
  //
  // ⚠ LE RELEVÉ NOMME 864 « hauteur » ET LES EFFETS L'APPELLENT « demi_hauteur ».
  // C'est la seconde qui a raison pour cette figure : le dessinateur l'étend de
  // part et d'autre du point. Voir l'ADR 0017.
  float half_width = 0.0F;
  float half_width_step = 0.0F;
  float half_width_acceleration = 0.0F;
  float half_height = 0.0F;
  float half_height_step = 0.0F;
  float half_height_acceleration = 0.0F;
  // Ce que ces quatre-là deviennent au tic du second pas. La sentinelle y vaut
  // « ne change pas », et c'est leur défaut.
  float half_width_step_after = kEffectScreenUnchanged;
  float half_height_step_after = kEffectScreenUnchanged;
  float half_width_acceleration_after = kEffectScreenUnchanged;
  float half_height_acceleration_after = kEffectScreenUnchanged;

  // LE DÉPLACEMENT : de combien de pixels par tic, et dans quelle direction.
  float step = 0.0F;
  float step_change = 0.0F;
  float step_after = kEffectScreenUnchanged;
  float step_change_after = kEffectScreenUnchanged;
  // Le CAP, en degrés d'écran.
  float heading_degrees = 0.0F;

  // L'ANGLE DU QUAD, en degrés d'écran, et ce qui le fait tourner.
  float turn_degrees = 0.0F;
  float turn_speed = 0.0F;
  float turn_acceleration = 0.0F;

  // L'ALPHA, DE ZÉRO À 255, et ses bornes. Un alpha omis vaut 254.
  float alpha = 254.0F;
  float alpha_step = 0.0F;
  float alpha_max = 254.0F;
  float alpha_min = 0.0F;
  // Le facteur qui accélère ou ralentit le fondu de sortie.
  float fade_factor = 1.0F;

  // LES TROIS CANAUX DE COULEUR et leurs pas. Ils valent 255 par défaut.
  std::array<std::int32_t, 3> color = {255, 255, 255};
  std::array<std::int32_t, 3> color_step = {0, 0, 0};

  // L'IMAGE COURANTE de l'animation, et son rythme.
  std::int32_t frame = 0;
  std::int32_t ticks_per_frame = 4;
  std::int32_t texture_count = 1;
  // VRAI quand l'animation BOUCLE ; faux, le nœud MEURT en atteignant la
  // dernière image.
  //
  // 🔴 LE CONSTRUCTEUR DU CLIENT LE POSE À VRAI. Un bâtisseur qui n'y touche
  // pas hérite donc du BOUCLAGE, et son nœud meurt de sa VIE. Un défaut à faux
  // tuerait au contraire tout nœud à une seule image au quatrième tic — la
  // cadence par défaut —, quelle que soit la vie que son bâtisseur a écrite.
  // Une vie de vingt tics servant en plus de diviseur à trois pentes, la
  // contredire raccourcirait l'effet des quatre cinquièmes.
  bool frame_loops = true;

  // ⚠ LE MÊME CHAMP SERT À DEUX DRAPEAUX QUI S'EXCLUENT : borne d'arrêt de
  // croissance pour l'un, période du zigzag pour l'autre.
  std::int32_t growth_stop_tick = 0;

  // UN ANGLE EN DEGRÉS DONT LA FIGURE DÉCIDE DU SENS, et les deux qui le
  // lisent ne s'accordent pas (ADR 0017) : la COURONNE y voit le PAS de son
  // balayage, l'ÉCLAT la DEMI-OUVERTURE de son triangle.
  //
  // 🔴 SON DÉFAUT EST TRENTE-SIX, pas zéro : le constructeur du nœud écrit
  // `0x42100000` (36.0F), et trois bâtisseurs de couronne
  // sur quatre ne le posent jamais. Ils héritent donc de DIX bandes qui
  // FERMENT le tour — le balayage s'arrête à `360 − pas`, chez le client comme
  // ici. Trente-cinq donnerait dix bandes sur trois cent cinquante degrés, et
  // le disque de Bash montrerait un secteur vide. À zéro, le balayage ne
  // s'arrêterait pas.
  //
  // ⚠ ET CE QUI LE FAIT CHANGER EST `half_width_step`, que l'éclat détourne :
  // sa demi-ouverture croît du pas de DEMI-LARGEUR, un champ que sa figure
  // n'emploie à rien d'autre. Lui donner un pas à lui inventerait un champ que
  // le client n'a pas.
  float angle_degrees = kEffectScreenDefaultStepDegrees;

  // LE RAYON DE LA COURONNE, et ce qui le fait croître.
  float outer_radius = 0.0F;
  float outer_radius_step = 0.0F;
  float outer_radius_acceleration = 0.0F;
  // LA LARGEUR DE BANDE VOULUE, et celle que le mouvement calcule.
  //
  // ⚠ LA SECONDE NE SE SÈME PAS. Le mouvement la recalcule à chaque tic — le
  // rayon entier quand le drapeau un est mis, sinon le plus petit de la
  // voulue et du rayon —, et la poser au semis ne servirait qu'au premier
  // tic, avant d'être écrasée.
  float band_width_target = 0.0F;
  float band_width = 1.0F;

  // CE QUE LA ROTATION DE L'ÉCLAT DEVIENT au tic du second pas.
  //
  // ⚠ LE SECOND PAS DE L'ÉCLAT NE REMPLACE QUE DEUX CHAMPS, là où celui du
  // quad en remplace six, et il n'emploie AUCUNE sentinelle : les deux
  // valeurs s'écrivent telles quelles. Leur prêter la sentinelle du quad
  // laisserait la rotation décélérer au lieu de se figer.
  float turn_speed_after = 0.0F;
  float turn_acceleration_after = 0.0F;

  std::uint32_t flags = 0;
  // ⚠ ET LE MODE DE MÉLANGE NE SERT QUE SI UN AUTRE CHAMP LE DIT. Le
  // dessinateur teste d'abord `uses_blend_mode` : à faux, il passe UN à la
  // primitive et ignore le mode. Poser le mode sans le drapeau ne changerait
  // donc rien, et le lire sans lui donnerait un mélange que le client n'emploie
  // pas.
  std::int32_t blend_mode = 0;
  bool uses_blend_mode = false;

  // ⚠ MARQUE QUE LE FONDU CALÉ SUR LE PORTEUR EST DÉJÀ ENGAGÉ. Il ne s'engage
  // qu'une fois ; sans cette marque il se réarmerait à chaque tic et l'alpha ne
  // baisserait jamais.
  bool owner_fade_engaged = false;
  // COMBIEN DE TICS IL RESTE À L'INSTANCE QUI L'A SEMÉ, tel que cette mise à
  // jour le lit : son âge a déjà crû de ce passage. La troupe d'écran le pose
  // avant chaque mise à jour.
  //
  // ⚠ ZÉRO VEUT DIRE « JE NE SAIS PAS », et la branche ne s'engage alors pas.
  std::int32_t owner_remaining_ticks = 0;
};

// UN TIC DE CE NŒUD. Rend FAUX quand il vient de mourir — l'animation a dépassé
// sa dernière image sans boucler.
//
// ⚠ LE CANAL ROUGE REBOUCLE LÀ OÙ LES DEUX AUTRES SATURENT. Vert et bleu
// s'écrêtent normalement ; le rouge fait l'inverse aux deux bouts — au-delà de
// 255 il retombe à zéro, en deçà de zéro il remonte à 255. Ce n'est pas une
// faute de lecture : les deux bornes sont inversées ensemble, dans le même bloc
// que les deux autres canaux écrits normalement. Donner le même écrêtage aux
// trois ferait un dégradé là où le client fait clignoter.
//
// ⚠ ET L'ORDONNÉE D'ÉCRAN SE SOUSTRAIT : le déplacement gagne le cosinus du cap
// en abscisse et PERD son sinus en ordonnée. Recopier le signe du monde ferait
// tomber ce qui doit s'élever.
//
// `generator` sert au zigzag, qui tire son cap au sort.
[[nodiscard]] bool AdvanceEffectScreenNode(EffectScreenNode& node, std::uint32_t& generator);

// CE QUE LE NŒUD DEMANDE À SA FIGURE, à ce tic.
//
// `owner_screen_position` est la position d'écran du porteur, DÉJÀ PROJETÉE.
// Elle est ignorée quand le nœud se pose dans l'absolu.
[[nodiscard]] EffectScreenQuadRequest EffectScreenQuadRequestOf(const EffectScreenNode& node,
                                                                glm::vec2 owner_screen_position = {
                                                                    0.0F, 0.0F});

}  // namespace ro::render
