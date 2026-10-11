#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "ro/render/actor_layers.h"
#include "ro/render/skill_held_pose.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

// CE QU'UN EFFET CHANGE SUR L'ACTEUR, au lieu de dessiner.
//
// Quatre-vingt-six identifiants ne posent aucune géométrie. Ils lèvent un
// DRAPEAU dans le mot de rendu de l'acteur, le TEIGNENT, CLONENT son sprite,
// changent son apparence, ou donnent un ordre à l'acteur ou à l'interface.
// C'est un huitième chemin, que le tableau des sept ne nomme pas.
//
// 🔴 UN DRAPEAU VIT EXACTEMENT TANT QU'UN EFFET VIVANT LE RELÈVE. Aucune
// écriture du mot de rendu n'en retire un bit, mais l'acteur REMET LE MOT À
// ZÉRO au premier pas qui suit chacun de ses dessins, et les instances d'effet,
// mises à jour après lui, le relèvent aussitôt. Un bit tombe donc le tic où
// son effet meurt, est retiré, ou passe la borne qui l'enferme — voir
// `RenderBitsAt`. Le PARAMÈTRE, lui, n'est jamais effacé : il garde la valeur
// de sa dernière écriture, et ne se lit que sous son bit.
//
// ⚠ ET UNE TEINTE REVIENT D'ELLE-MÊME. L'acteur ajoute huit à chacune des
// quatre composantes de sa couleur à chaque pas, jusqu'à 255 : la couleur dure
// tant que l'effet la réécrit, puis remonte vers le blanc en une trentaine de
// tics au plus. Voir `TintAfterEffectsAt`.

// La famille d'un effet qui change l'acteur : ce qu'il faut en faire, et donc
// qui s'en occupe.
enum class ActorEffectFamily : std::uint8_t {
  // Un bit du mot de rendu, et l'octet voisin qui en donne la mesure.
  kRenderFlag,
  // Une couleur sur l'acteur entier.
  kTint,
  // Une rémanence : une copie du sprite, posée dans la scène.
  kSpriteClone,
  // Le sprite ou le métier de l'acteur change.
  kAppearance,
  // Un ordre, à l'acteur par sa vtable ou à l'interface — la secousse d'écran.
  kOrder,
  // Il fait TOURNER l'acteur, en poussant son angle.
  kSpin,
};

// La composante d'une teinte que l'effet ne touche pas. Le client passe -1, et
// c'est ainsi que EF_CLOAKING ne change que l'alpha sans noircir le corps.
inline constexpr std::int16_t kTintUnchanged = -1;

// UNE COMPOSANTE QUE LE CLIENT TIRE AU SORT à chaque tic.
//
// ⚠ CE N'EST PAS « INCONNUE ». Le client calcule la composante au lieu d'y
// pousser un nombre, et ce calcul est un TIRAGE : `rand() % 255` sur chacune
// des trois. La lire comme « inchangée » laisserait le corps sans couleur, et
// l'effet muet.
//
// ⚠ ET LE TIRAGE NE SE FAIT PAS DANS `render`, qui doit rester sans hasard :
// la table dit QUE la composante est tirée, l'appelant tire. C'est la même
// frontière que pour le tremblement du corps.
inline constexpr std::int16_t kTintRandom = -2;

// Ce que porte command_tick quand l’ordre ne part pas à un tic précis : il se
// répète selon sa propre règle, et la vie de l’effet le borne.
inline constexpr std::int16_t kCommandRepeats = -1;

// Ce qu'un effet écrit dans un champ de l'acteur, et d'où la valeur vient.
//
// ⚠ LA VALEUR EST SOUVENT L'HORLOGE, pas une constante : le paramètre d'un
// fondu est le compteur de tics de l'effet, parfois multiplié ou retranché
// d'une base. C'est ce qui fait la rampe, et c'est pourquoi `value` ne suffit
// pas à décrire l'écriture.
// Ce que porte `cap` quand la rampe n'en a pas.
inline constexpr std::int32_t kNoCap = -1;

// Ce que porte `ActorFieldWrite::at_tick` quand l'écriture n'est sous aucune
// garde d'égalité : le client l'écrit à CHAQUE tic, pas à un seul.
inline constexpr std::int16_t kWriteEveryTick = -1;

struct ActorFieldWrite {
  // Le décalage du champ dans l'acteur. Zéro pour une écriture absente.
  std::uint16_t field = 0;
  // La constante écrite, quand il y en a une.
  std::int32_t value = 0;
  // Vrai quand la valeur vient de l'HORLOGE de l'effet et non d'une constante.
  bool from_clock = false;
  // Vrai quand elle en vient par un calcul que la table ne décrit pas — une
  // rampe, un plafond. Le champ bouge avec le temps, et `value` ne le dit pas.
  bool computed = false;
  // LA RAMPE, quand la valeur est affine en l'âge de l'effet :
  //
  //     valeur = min(cap, slope × tics + offset)
  //
  // ⚠ CE N'EST JAMAIS LE TIC NU. `EF_BABYBODY_BACK` écrit `45 − 3 × tic`,
  // `EF_UNDEADBODY_DEL` `200 − 5 × tic`, `EF_JUMPBODY` `tic − 35`. Porter le
  // bit sans sa rampe montre une image figée à sa valeur d'arrivée — le semis
  // manquant de l'ADR 0021, transposé au corps.
  std::int32_t slope = 0;
  std::int32_t offset = 0;
  // Le plafond, ou kNoCap. Il vient d'un `cmov`, pas d'un calcul.
  std::int32_t cap = 0;
  // À QUEL TIC cette écriture tombe, ou kWriteEveryTick.
  //
  // ⚠ ZÉRO EST UN TIC. Et c’est ce champ qui sépare DEUX écritures du même
  // champ : `EF_JUMPKICK` pose l’état vingt-sept au cinquième tic, l’état deux
  // au trente-cinquième. Sans lui, les deux se réunissaient en une seule — la
  // dernière —, et le coup de pied ne montrait que sa fin.
  std::int16_t at_tick = kWriteEveryTick;
  // VRAI QUAND L'EFFET ÉCRIT CE CHAMP AVANT DE COMPARER SON ÂGE À SA VIE : il
  // l'écrit alors tant que son instance vit, et l'âge ne se fige pas à la vie.
  bool outlives_life = false;

  [[nodiscard]] friend bool operator==(const ActorFieldWrite&, const ActorFieldWrite&) = default;
};

// La valeur qu'une écriture pose après `age_ticks` tics d'effet.
//
// ⚠ L'ÂGE SE FIGE AU DERNIER TIC DE LA VIE DE L'EFFET. Le client cesse d'écrire
// au-delà de sa borne, et le champ garde donc la valeur de sa dernière écriture
// — il ne revient pas à zéro et ne continue pas de monter. Passer l'âge brut
// ferait descendre un corps bien après qu'il a fini de descendre. Sauf pour une
// écriture `outlives_life`, qui suit l'horloge tant qu'on la lit.
[[nodiscard]] std::int32_t ValueAt(const ActorFieldWrite& write, std::uint32_t age_ticks,
                                   std::uint16_t life_ticks) noexcept;

// Combien de champs un même effet pousse au plus. Deux : `EF_LANDBODY` écrit
// sa rampe de fondu ET son décalage. Le générateur s'arrête si une source en
// apporte un troisième, plutôt que de le perdre en silence.
inline constexpr std::size_t kActorFieldWritesPerEffect = 2;

// Ce que porte `EffectLedPose::tick` quand l'effet ne mène aucune pose.
inline constexpr std::int16_t kNoLedPoseTick = -1;

// LA POSE QU'UN EFFET MÈNE LUI-MÊME sur son acteur, à un tic de sa vie.
//
// Deux effets — le coup de pied sauté et son jumeau — ne se contentent pas
// d'écrire l'état de l'acteur : au même tic, ils lui posent une pose menée,
// comme une compétence à l'annonce (voir `SkillHeldPose`). Les images
// [first_frame, last_frame] du rang `play_rank`, puis l'image `hold_frame` du
// rang `hold_rank`, minutée à `hold_ms`.
//
// ⚠ L'ÉCHÉANCE NE REND PAS L'ACTEUR À SON ANIMATION. Passé `hold_ms`, le client
// ne fait plus avancer l'image et la borne au rang : l'acteur GARDE l'image
// tenue jusqu'à sa prochaine motion. Et tant que la pose est minutée, une
// blessure qui porte n'a pas lieu, comme sous toute pose menée.
struct EffectLedPose {
  std::int16_t tick = kNoLedPoseTick;
  std::uint8_t play_rank = 0;
  std::uint8_t first_frame = 0;
  std::uint8_t last_frame = 0;
  std::uint8_t hold_rank = 0;
  std::uint8_t hold_frame = 0;
  std::uint16_t hold_ms = 0;

  [[nodiscard]] friend bool operator==(const EffectLedPose&, const EffectLedPose&) = default;
};

// Une entrée de la table : ce qu'un identifiant change sur l'acteur.
struct ActorEffectChange {
  std::uint32_t effect_id = 0;
  ActorEffectFamily family = ActorEffectFamily::kRenderFlag;
  // Les bits levés dans le mot de rendu, réunis. Zéro s'il n'en lève aucun.
  std::uint32_t render_bits = 0;
  // Alpha, rouge, vert, bleu — dans l'ordre des arguments du client.
  // kTintUnchanged pour ce qui ne change pas ; les quatre à kTintUnchanged pour
  // un effet qui ne teint rien.
  std::array<std::int16_t, 4> tint = {kTintUnchanged, kTintUnchanged, kTintUnchanged,
                                      kTintUnchanged};
  std::array<ActorFieldWrite, kActorFieldWritesPerEffect> writes;
  // La vie de l'effet en TICS, telle que le client la borne, ou zéro quand il
  // ne la borne pas : le PREMIER tic où il n'agit plus. Un tic vaut
  // kEffectTickMs.
  std::uint16_t life_ticks = 0;
  // Le numéro de l'ordre, pour la famille kOrder. La commande 0x7A est la
  // secousse d'écran.
  std::uint16_t command = 0;
  // À QUEL TIC cet ordre part, ou `kCommandRepeats` s'il ne part pas à un tic
  // précis.
  //
  // ⚠ ZÉRO EST UN TIC, pas une absence. `EF_DAMAGE1` donne son ordre au premier
  // tic, `EF_FALCONASSAULT` au trentième — une demi-seconde plus tard, quand
  // son coup touche. Confondre les deux ferait trembler l'écran avant le coup.
  std::int16_t command_tick = kCommandRepeats;
  // LE NOMBRE QUE CET ORDRE FAIT PARAÎTRE au-dessus de l'acteur, et sa COULEUR
  // en alpha-rouge-vert-bleu. Zéro pour un ordre qui n'en fait paraître aucun.
  //
  // ⚠ LA COULEUR EST ICI EN CLAIR, contrairement à celles que le client
  // empaquette pour les contours : `0xFFFF0000` est ROUGE. C'est la même
  // convention que dans le dispatcher de messages de l'acteur, qui la passe
  // telle quelle.
  std::int32_t command_value = 0;
  std::uint32_t command_color = 0;
  // LA CLASSE QUE CET EFFET FAIT PORTER À L'ACTEUR, ou zéro.
  //
  // Cinq effets posent une PLANTE là où l'acteur se tient — l'hallucination —,
  // chacune d'une espèce différente. Le numéro est la classe que le client
  // donne à l'acteur, et c'est une classe de MONSTRE : la garde-robe
  // sait l'habiller comme n'importe quelle autre.
  //
  // ⚠ CES CINQ-LÀ PARTAGENT UNE SEULE CASE, et se départagent en comparant leur
  // propre numéro. Lue en bloc, elle rendait le même métier aux cinq.
  std::uint16_t appearance_job = 0;
  // LA FONCTION DU CLIENT qui fait le travail, quand le cas le délègue.
  //
  // ⚠ SANS ELLE, UN CLONE NE DIT RIEN. Les huit rémanences ne posent ni bit ni
  // écriture : tout est dans la fonction appelée, et deux d'entre elles n'ont
  // même pas de couleur constante. Le nom est ce qui les distingue, et ce qui
  // dit à `app` quel mécanisme porter. Vide quand le cas fait tout lui-même.
  std::string_view client_function;
  // TOUS LES COMBIEN DE TICS cet effet sème une COPIE du sprite de l'acteur.
  //
  // Zéro pour un effet qui n'en sème aucune. Un pour un effet qui en sème à
  // chaque tic — c'est le cas de Kagemusha, qui en dispose une par angle.
  //
  // ⚠ LA COPIE VIT UN TOUR D'ANIMATION, pas une durée. Le constructeur de clone
  // pose un compteur de tours, et l'animation cesse de la dessiner quand ce
  // compteur s'épuise : ce n'est pas un fondu, c'est une boucle qui finit.
  std::uint16_t clone_period_ticks = 0;
  // La teinte des tics impairs tant que l'effet vit — de la seconde moitié de
  // chaque `blink_period_ticks` : le corps clignote entre elle et `tint`. Toute
  // à kTintUnchanged pour un effet qui ne clignote pas.
  std::array<std::int16_t, 4> odd_tick_tint = {kTintUnchanged, kTintUnchanged, kTintUnchanged,
                                               kTintUnchanged};
  // LE PREMIER TIC où l'effet agit sur l'acteur : avant, le client sort du cas
  // sans le toucher — ni teinte, ni bit. Zéro pour un effet qui agit dès son
  // départ.
  //
  // ⚠ C'EST AUSSI CE QUI RETIENT UNE RAMPE. `EF_JUMPBODY` écrit `tic − 35` à
  // partir du trente-sixième tic : lue plus tôt, la rampe est négative et
  // enfonce le corps dans le sol avant qu'il ne saute.
  std::uint16_t first_tick = 0;
  // SUR COMBIEN DE TICS le corps clignote : `tint` la première moitié de
  // chaque période, `odd_tick_tint` la seconde. Deux pour un tic sur deux ;
  // un effet alterne trois tics gris et trois tics presque blancs.
  std::uint8_t blink_period_ticks = 2;
  // LA RAMPE D'UNE TEINTE, en unités par tic de l'effet, par composante — alpha,
  // rouge, vert, bleu comme `tint`. Une composante à pente non nulle vaut
  // `tint + pente × âge`, l'âge borné à la vie comme pour le reste : le corps
  // garde la couleur de son dernier tic. Tout à zéro : la teinte est fixe.
  std::array<std::int16_t, 4> tint_slope_per_tick = {0, 0, 0, 0};
  // LES BITS QUE L'EFFET LÈVE AVANT DE COMPARER SON ÂGE À SA VIE : ils ne
  // tombent qu'avec l'instance, quand les autres tombent à `life_ticks`.
  //
  // ⚠ LA VIE DE LA TABLE PEUT N'ÊTRE QUE CELLE DU PARAMÈTRE. `EF_BABYBODY2`
  // lève le bit de sa réduction à chaque tic, puis n'écrit son compteur que
  // jusqu'au quarante-cinquième : le corps reste petit tant que l'instance vit,
  // le compteur figé à sa dernière valeur.
  std::uint32_t unbounded_render_bits = 0;
  // LA POSE QUE L'EFFET MÈNE au tic où il écrit l'état de l'acteur, ou aucune.
  EffectLedPose led_pose;
  // LA COULEUR DE LA COPIE, alpha-rouge-vert-bleu, quand l'effet teint la copie
  // qu'il sème et non son porteur — l'ombre de Kagemusha est grise et
  // translucide, son porteur reste tel quel. kTintUnchanged pour ce qu'il ne
  // change pas ; les quatre pour une copie qui garde la couleur du porteur.
  std::array<std::int16_t, 4> clone_tint = {kTintUnchanged, kTintUnchanged, kTintUnchanged,
                                            kTintUnchanged};
  // DE COMBIEN D'UNITÉS DU MONDE la copie naît DERRIÈRE son porteur, sur l'axe
  // de son corps. Zéro pour une copie posée à sa place.
  float clone_behind_units = 0.0F;
  // TOUS LES COMBIEN DE TICS l'effet lève ses bits : au tic dont l'âge est un
  // multiple, et à aucun autre. Le mot de rendu retombant à zéro à chaque
  // image, un voile levé un tic sur quatre CLIGNOTE. Un pour chaque tic.
  std::uint8_t render_bits_period_ticks = 1;
};

// Vrai quand l'effet agit sur l'acteur à cet âge : passé son premier tic, et
// avant sa vie quand il en a une.
[[nodiscard]] bool ActsAt(const ActorEffectChange& change, std::uint32_t age_ticks) noexcept;

// LES EFFETS DONT L'INSTANCE REMBOBINE SON ÂGE un tic avant sa borne : ils ne
// meurent jamais d'eux-mêmes, et seul un retrait les éteint. Cinq le font à un
// âge écrit en dur, deux — 421 et 1131 — à leur propre durée moins un.
inline constexpr std::array<std::uint32_t, 7> kAgeRewindingActorEffects = {166, 421, 617, 752,
                                                                           792, 925, 1131};

// Vrai tant que l'INSTANCE de cet effet vit à cet âge : avant la durée que le
// client lui donne, ou toujours quand cette durée est infinie, inconnue, ou
// que l'instance rembobine son âge. Un effet retiré n'est plus là pour poser la
// question.
[[nodiscard]] bool InstanceAliveAt(std::uint32_t effect_id, std::uint32_t age_ticks);

// LES BITS DU MOT DE RENDU QUE L'EFFET RELÈVE À CET ÂGE : aucun avant son
// premier tic ni après la mort de son instance, et passé sa vie, seulement
// `unbounded_render_bits`.
[[nodiscard]] std::uint32_t RenderBitsAt(const ActorEffectChange& change, std::uint32_t age_ticks);

// La table entière, triée par identifiant d'effet.
[[nodiscard]] std::span<const ActorEffectChange> ActorEffectChangeTable();

// Ce qu'un effet change sur l'acteur, ou nullptr s'il n'en change rien.
[[nodiscard]] const ActorEffectChange* ActorEffectChangeFor(std::uint32_t effect_id);

// Vrai pour une teinte qui change au moins une composante.
[[nodiscard]] bool TintsActor(const ActorEffectChange& change) noexcept;

// UN EFFET JOUÉ, tel que `app` le rapporte : son numéro et son ÂGE en tics.
//
// L'âge est ce que la teinte n'a pas besoin de connaître et que la pose exige :
// un corps qui saute est à mi-chemin au dixième tic et parti au vingtième.
struct ActorEffectPlay {
  std::uint32_t effect_id = 0;
  std::uint32_t age_ticks = 0;
};

// OÙ LE CORPS SE POSE, et s'il s'estompe — ce que les effets en font.
//
// La hauteur est en PIXELS DE SPRITE, vers le HAUT : c'est l'unité dans
// laquelle le client déplace ses quads, et kSpritePixelsPerWorldUnit la
// convertit.
//
// ⚠ LE CLIENT DÉCALE EN COORDONNÉES D'ÉCRAN, NOUS EN COORDONNÉES DU MONDE, et
// pour la MONTÉE cela revient au même en mieux : un corps qui saute monte, et
// la caméra de RO regarde toujours du même angle. Ce qui demande un décalage
// LATÉRAL passe par `DrawnActor::screen_offset_pixels`, que le renderer sait
// poser dans le repère du panneau.
struct ActorBodyPose {
  float up_pixels = 0.0F;
  std::uint8_t alpha = 255;
  // Le corps n'est pas à l'écran du tout. Le client l'envoie très loin sous le
  // sol plutôt que de sauter son dessin ; le résultat visible est le même, et
  // un drapeau le dit mieux qu'un décalage de vingt-quatre mille.
  bool hidden = false;
  // DE COMBIEN LE CORPS EST SOULEVÉ, en FRACTION de sa propre hauteur.
  //
  // ⚠ CE N'EST PAS `up_pixels`, qui compte en pixels de sprite. Le client
  // déplace ici les deux bords du quad d'une part de leur ÉCART : un grand
  // corps monte plus haut qu'un petit, et le dire en pixels le ferait sauter
  // pareil. C'est à l'appelant de multiplier par la hauteur qu'il connaît.
  float lift_fraction = 0.0F;
  // DE COMBIEN LE CORPS EST ÉCRASÉ EN HAUTEUR. Un pour un corps debout.
  //
  // ⚠ L'ÉCRASEMENT EST ANCRÉ AUX PIEDS, et cela tombe juste tout seul : le
  // client rapproche le bord HAUT du bord bas, et nos coins de quad sont déjà
  // comptés depuis le sol. Multiplier leur ordonnée suffit.
  float height_scale = 1.0F;
  // LES CALQUES QUI SE COMPOSENT EN ADDITION au lieu de se poser par-dessus.
  //
  // ⚠ CE N'EST PAS UN « MODE 2 » À INTERPRÉTER, c'est un FACTEUR DE MÉLANGE
  // Direct3D. Le client soumet le calque avec cinq et deux — ou six — comme
  // facteurs source et destination, passés TELS QUELS sous les états 19 et 20,
  // `D3DRS_SRCBLEND` et `D3DRS_DESTBLEND`. Deux est `D3DBLEND_ONE`, six
  // `D3DBLEND_INVSRCALPHA` : l'un ajoute au décor, l'autre le recouvre.
  //
  // ⚠ ET LE CLIENT CHOISIT PAR CALQUE. Le bit `0x1` rend additifs les calques
  // zéro à quatre — corps, tête, trois coiffes — ET LA CAPE, que le client
  // peint par un autre chemin sous le numéro du corps ; l'arme, sa traînée et
  // le bouclier recouvrent toujours. Le bit `0x2` ne rend additive que l'ARME.
  ActorPartSet additive_parts = 0;
  // L'ARME SE PEINT UNE SECONDE FOIS, en addition — le bit `0x4`. Le client ne
  // la double qu'une image sur deux : c'est à l'appelant, qui compte les images,
  // de n'en retenir qu'une sur deux.
  bool doubles_weapon = false;
  // OÙ LA TÊTE EN EST DE SON CERCLE, en pixels de sprite d'ÉCRAN : x vers la
  // droite, y vers le haut. Zéro pour une tête qui ne tourne pas.
  //
  // 🔴 LE CORPS NE QUITTE PAS SA PLACE : il tourne sur lui-même — voir
  // `layer_spin_degrees` —, et ses calques tournent autour de lui, la tête sur
  // un cercle de quarante pixels, les autres sur un cercle de dix. Un monstre,
  // qui n'a qu'un corps, fait la culbute sur son pivot.
  //
  // ⚠ CE N'EST PAS `up_pixels`, et les deux ne se confondent pas : la montée
  // d'un saut se fait dans le MONDE, ce balancement dans le repère du panneau.
  std::array<float, 2> swing_pixels = {0.0F, 0.0F};
  // Le même cercle pour les calques qui ne sont ni le corps ni la tête.
  std::array<float, 2> layer_swing_pixels = {0.0F, 0.0F};
  // DE COMBIEN LE CORPS TREMBLE, en pixels de sprite ; zéro s'il ne tremble
  // pas. C'est une AMPLITUDE, pas un décalage : le client tire un nombre au
  // sort pour chacun des quatre bords à chaque image, et `render` reste sans
  // hasard — c'est l'appelant qui tire, et la table qui dit de combien.
  float jitter_pixels = 0.0F;
  // DE COMBIEN CHAQUE CALQUE TOURNE SUR LUI-MÊME, en degrés dans le plan de
  // l'écran, sens des aiguilles d'une montre. Le même tour que le cercle de la
  // tête : le corps fait un tour complet, la tête passe dessous à mi-course.
  // Il REMPLACE l'angle du calque, et retombe à zéro le tour fini.
  float layer_spin_degrees = 0.0F;
};

// LES CALQUES QUE LE CLIENT TRAITE COMME LE CORPS dans ce que les effets font à
// l'acteur : les calques zéro à quatre — corps, tête, trois coiffes — et la
// cape, qu'il peint par un autre chemin sous le numéro du corps. L'arme, sa
// traînée et le bouclier n'en sont pas.
inline constexpr ActorPartSet kBodyAndHeadwearParts =
    PartBit(ActorPart::kBody) | PartBit(ActorPart::kHead) | PartBit(ActorPart::kHeadgearLow) |
    PartBit(ActorPart::kHeadgearMiddle) | PartBit(ActorPart::kHeadgearTop) |
    PartBit(ActorPart::kGarment);

// La pose du corps après les effets en cours.
//
// `0x10000` le fait monter en s'effaçant — le saut —, `0x20000` le fait
// redescendre en réapparaissant — la réception —, `0x100000` l'envoie hors de
// l'écran ; `0x1`, `0x2` et `0x4` choisissent le mélange de certains calques
// (voir `ActorBodyPose::additive_parts`). Les autres bits du mot de rendu
// demandent chacun la lecture de leur propre lecteur, et rien ici ne les
// devine : un bit non porté ne change simplement rien.
[[nodiscard]] ActorBodyPose BodyPoseAfterEffects(std::span<const ActorEffectPlay> played);

// L'ÉCHELLE DU CORPS après les effets en cours, à multiplier à celle de sa
// classe.
//
// Deux bits, `0x10` vers un demi et `0x400` vers un et demi, et pour chacun un
// compteur d'octet que l'effet fait avancer. Le serveur n'a pas d'autre moyen
// d'annoncer la taille d'un corps : il envoie un EFFET, à chaque changement
// comme à chaque apparition.
//
// ⚠ CETTE RÈGLE ÉTAIT ÉCRITE DEUX FOIS. `net` déduisait une taille du numéro
// d'effet et la posait dans le monde, pendant que la table disait la même
// chose ; deux endroits décidaient de ce qu'un identifiant veut dire, et l'un
// des deux ignorait le RETOUR à la taille normale. La table décide seule.
[[nodiscard]] float BodyScaleAfterEffects(std::span<const ActorEffectPlay> played);

// LA SEULE RÉDUCTION du corps après les effets en cours, de un à un demi : ce
// qu'un sprite posé sur lui reprend, sans l'agrandissement.
[[nodiscard]] float BodyShrinkAfterEffects(std::span<const ActorEffectPlay> played);

// DE COMBIEN L'ACTEUR A PIVOTÉ SUR LUI-MÊME, en degrés, après les effets en
// cours.
//
// `EF_SPINMOVE` avance l'angle de face de l'acteur — celui dont le client tire
// le huitième de tour qui choisit l'image — de douze degrés par
// tic, puis dix-huit, vingt-quatre, quarante-huit au plus vite, puis les mêmes
// en redescendant. Neuf cent cinquante-quatre degrés en quarante-cinq tics, et
// il reste où il s'est arrêté.
//
// ⚠ CE N'EST PAS LE BIT `0x40`, qui promène le corps sur un CERCLE autour de sa
// place sans le retourner. Celui-ci ne le déplace pas.
//
// LA TOUPIE (837) tourne autrement : douze degrés par tic pendant soixante
// tics — deux tours exacts —, puis l'acteur revient au cap qu'il avait avant la
// première toupie. Plusieurs toupies en cours s'additionnent.
[[nodiscard]] float SpinDegreesAfterEffects(std::span<const ActorEffectPlay> played);

// L'EFFET DE LA TOUPIE, qu'aucune table ne décrit : il ne dessine rien et fait
// pivoter son porteur.
inline constexpr std::uint32_t kSpinningTopEffect = 837;

// LES KUNAI EN ÉTOILE (981) font tourner leur LANCEUR de quarante-cinq degrés à
// chaque tic où leur première tête se voit encore, et il garde l'angle atteint.
// Sans acteur lié — la compétence vise son lanceur —, le vol fait huit unités à
// un pas et demi : sept tics de vol et douze de fondu, soit dix-neuf tics.
inline constexpr std::uint32_t kKunaiStarEffect = 981;

// L'ÉTAT D'ACTION QU'UN EFFET A ÉCRIT sur son acteur, et le tic — compté depuis
// la naissance de l'effet — où il l'a écrit.
struct EffectMotionWrite {
  std::uint32_t tick = 0;
  std::int8_t state = 0;
};

// LA DERNIÈRE ÉCRITURE D'ÉTAT d'un effet à cet âge, ou rien s'il n'en a pas
// encore fait.
//
// 🔴 ÉCRIRE L'ÉTAT NE POSE AUCUNE ANIMATION. Deux effets du lot le font — le
// coup de pied sauté et son jumeau — : l'état vingt-sept à leur cinquième tic,
// l'état deux au trente-cinquième. L'acteur ne change pas d'image pour autant,
// il ne la change que sous la pose que l'effet mène au même tic (`led_pose`).
// Ce que l'état change, c'est ce qui vient APRÈS : l'état deux, vu fini par la
// mise à jour suivante, ramène l'acteur au repos — et c'est un repos quitté
// depuis l'attaque, donc la GARDE. C'est aussi un changement de motion comme
// un autre : un geste qui tourne s'y arrête.
[[nodiscard]] std::optional<EffectMotionWrite> LastMotionWrite(const ActorEffectPlay& played);

// CE QUE LA POSE MENÉE D'UN EFFET MONTRE `elapsed_ms` après son tic : ses images
// une à une, puis l'image tenue — que l'échéance ne retire pas. `tick_ms` est
// la durée d'une avance de l'acteur.
[[nodiscard]] HeldPoseFrame EffectLedPoseFrameAt(const EffectLedPose& pose,
                                                 std::uint32_t elapsed_ms, std::uint32_t tick_ms);

// LES DEUX EMPREINTES qu'un coureur laisse, en alternance.
inline constexpr std::array<std::uint32_t, 2> kFootprintEffects = {447, 448};

// TOUS LES COMBIEN il en laisse une.
//
// ⚠ LE CLIENT COMPTE EN IMAGES, NOUS EN TICS, comme pour la respiration des
// halos : sept de ses images, sept de nos tics. Ses pas sont donc un peu plus
// serrés que les nôtres, et c'est dit plutôt que caché — le nombre d'images par
// seconde n'est pas un fait du client mais de la machine.
inline constexpr std::uint32_t kFootprintPeriodTicks = 7;

// LES DEUX EMPREINTES DE CHASE WALK, et leur période : l'option du marcheur fait
// tourner le même semeur, plus lentement, sans attendre aucune horloge.
inline constexpr std::array<std::uint32_t, 2> kChaseWalkFootprintEffects = {326, 327};
inline constexpr std::uint32_t kChaseWalkFootprintPeriodTicks = 12;

// CE QU'UN COUREUR LAISSE DERRIÈRE LUI, quand il en laisse.
//
// Ce qui varie — l'endroit, l'alternance — n'appartient pas à la table mais à
// qui suit l'acteur d'une image à l'autre. La table dit QUE la course en laisse,
// et si son horloge le permet déjà.
struct ActorTrail {
  // ⚠ LE SEMEUR TOURNE DÈS QUE LE BIT EST LEVÉ, mais ne pose rien tant que
  // l'horloge de la course ne dépasse pas sa période : une empreinte posée au
  // premier tic tomberait sous le pied qui n'a pas encore bougé. Et l'horloge
  // vit dans un OCTET de l'acteur : elle repasse par zéro tous les deux cent
  // cinquante-six tics, et la course se tait alors huit tics.
  bool clock_past_period = false;
};

// Rien quand aucun effet en cours ne fait laisser d'empreintes.
[[nodiscard]] std::optional<ActorTrail> TrailAfterEffects(std::span<const ActorEffectPlay> played);

// L'ANGLE D'UNE EMPREINTE, en degrés du client, pour une marche de `dx` et
// `dz` dans SON repère : le premier degré entier dont le cosinus ne dépasse plus
// la part de `dx`, retourné quand `dz` est négatif, puis tourné d'un quart de
// tour et demi. Rien pour une marche nulle.
[[nodiscard]] std::optional<std::int32_t> FootprintAzimuthDegrees(float dx, float dz);

// UNE COPIE DU CORPS qu'un effet ajoute autour de lui.
//
// Le client soumet ces quads-là AVANT le corps, et ils ne sont ni des acteurs
// ni des effets : ce sont le même sprite, à une échelle et une couleur près.
// C'est ce qui fait la lueur d'`EF_ASSUMPTIO` et le halo d'`EF_UNDEADBODY`.
struct ActorBodyCopy {
  // Multiplie l'échelle du corps. Un halo est légèrement plus grand que lui.
  float scale = 1.0F;
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // DE COMBIEN CETTE COPIE EST DÉCALÉE À L'ÉCRAN, en pixels de sprite : x vers
  // la droite, y vers le haut. Un halo reste sur son porteur et vaut zéro ; la
  // croix d'`EF_4WAYBODY`, elle, écarte ses quatre copies.
  std::array<float, 2> offset_pixels = {0.0F, 0.0F};
  // LES CALQUES QUE LA COPIE REPREND. Le client pose sa copie calque par calque,
  // et pas sur tous : la lueur qui respire ne reprend ni l'arme ni le bouclier,
  // le voile ne reprend pas la cape.
  ActorPartSet parts = kAllActorParts;
  // LA COPIE S'AJOUTE au décor au lieu de le recouvrir. Voir
  // `ActorBodyPose::additive_parts` pour ce que ce mélange veut dire.
  bool additive = false;
};

// Combien de copies un acteur peut porter à la fois. QUATRE par bit au plus —
// la croix et les anneaux en posent quatre chacun —, et deux bits au plus se
// rencontrent ; la fonction s'arrête au bout de la place qu'on lui donne
// plutôt que d'en perdre en silence.
inline constexpr std::size_t kMaxBodyCopies = 8;

// Les VOILES que les effets en cours posent PAR-DESSUS le corps, écrits dans
// `out`. Rendu : combien ont été écrits.
//
// Un voile est le même sprite à la même taille, teinté et translucide, que le
// client soumet après le corps.
//
// ⚠ TROIS BITS, DEUX COULEURS, UNE SEULE PIÈCE : `0x4000` teint en BLEU,
// `0x40000` en ROUGE, `0x100` en blanc. Le client passe ces composantes EN
// CLAIR à la fonction, et c'est ce qui prouve l'ordre dans lequel il empaquette
// ses couleurs ailleurs — six effets nommés bleus d'un côté, trois nommés
// rouges de l'autre.
//
// ⚠ UN VOILE S'AJOUTE, il ne recouvre pas, et il ne reprend pas la cape : le
// client le soumet en addition, calque par calque, hors du chemin qui peint la
// cape.
[[nodiscard]] std::size_t BodyVeilsAfterEffects(std::span<const ActorEffectPlay> played,
                                                std::span<ActorBodyCopy> out);

// Les copies que les effets en cours demandent, écrites dans `out`.
//
// `body_height_pixels` est la hauteur du corps en pixels de sprite : le client
// ÉLARGIT le quad d'un nombre de pixels, et c'est cette hauteur qui convertit
// l'élargissement en échelle. Un corps de hauteur nulle n'a pas de halo.
//
// `0x8` pose deux copies blanches qui respirent, `0x800` et `0x80000` deux
// copies teintées dont l'alpha est le paramètre de l'effet — toutes quatre EN
// ADDITION —, `0x200` et `0x1000` les anneaux et la croix. Rendu : combien de
// copies ont été écrites.
[[nodiscard]] std::size_t BodyCopiesAfterEffects(std::span<const ActorEffectPlay> played,
                                                 float body_height_pixels,
                                                 std::span<ActorBodyCopy> out);

// La teinte d'un acteur une fois posés les effets qu'il porte, en rouge, vert,
// bleu, alpha — l'ordre de DrawnActor, et non celui de la table.
//
// `base` est ce que l'acteur montrerait sans effet. Les identifiants viennent
// dans l'ordre où le serveur les a joués ; LE DERNIER GAGNE, comme chez le
// client où chaque tic repose la couleur. Une composante que l'effet
// laisse à kTintUnchanged garde celle de `base` — c'est ainsi que EF_CLOAKING
// rend un corps transparent et non un corps noir.
//
// ⚠ AUCUNE HORLOGE N'ENTRE ICI : c'est la couleur que chaque effet ÉCRIT, sans
// sa vie ni le retour au blanc. Voir `TintAfterEffectsAt` pour la couleur
// qu'un acteur MONTRE.
[[nodiscard]] std::array<std::uint8_t, 4> TintAfterEffects(
    std::array<std::uint8_t, 4> base, std::span<const std::uint32_t> effect_ids);

// LE TIRAGE D'UNE COMPOSANTE `kTintRandom` : le rang de l'effet dans la liste
// jouée, le tic de l'écriture, et la composante dans l'ordre de la table
// (alpha, rouge, vert, bleu). `render` ne tire rien lui-même.
using TintDraw =
    std::function<std::uint8_t(std::size_t rank, std::uint32_t tick, std::size_t channel)>;

// LA COULEUR QUE L'ACTEUR MONTRE, à l'âge de chaque effet.
//
// Par composante, la DERNIÈRE écriture l'emporte — à tic égal, celle de
// l'effet le plus récent de la liste. Un effet écrit de son premier tic
// jusqu'à sa vie, ou jusqu'à la mort de son instance si elle vient avant ; un
// effet qui clignote écrit `odd_tick_tint` à ses tics impairs.
//
// ⚠ PUIS LA COULEUR REVIENT D'ELLE-MÊME : huit de plus par tic sur chaque
// composante écrite, jusqu'à 255. Un corps ne garde pas la teinte d'un effet
// qui a cessé de l'écrire ; seul `base` tient les composantes que personne
// n'a jamais écrites.
[[nodiscard]] std::array<std::uint8_t, 4> TintAfterEffectsAt(
    std::array<std::uint8_t, 4> base, std::span<const ActorEffectPlay> played,
    const TintDraw& draw = {});

// UNE COULEUR QU'UN EFFET DE LA LISTE ÉCRIT À CHAQUE TIC sans que la table la
// porte : celle de la traînée colorée, que l'état qui la pose choisit. Elle
// s'écrit au RANG de cet effet dans la liste jouée, si bien qu'un effet posé
// après elle la recouvre, et qu'elle recouvre ceux d'avant.
struct TintEveryTickWrite {
  std::size_t rank = 0;
  // Alpha, rouge, vert, bleu — l'ordre de la table ; `kTintUnchanged` saute.
  std::array<std::int16_t, 4> tint = {kTintUnchanged, kTintUnchanged, kTintUnchanged,
                                      kTintUnchanged};
};

// LES IMAGES SANS TIC passées depuis une écriture vieille de `ticks_since`
// tics : voir `TintRecoveryClock`.
using TintIdleFramesSince = std::function<std::uint32_t(std::uint32_t ticks_since)>;

// Ce que la couleur d'un acteur lit en plus des effets qu'il porte.
struct TintContext {
  TintDraw draw;
  std::optional<TintEveryTickWrite> every_tick_write;
  // 🔴 LE RETOUR AU BLANC SE PAIE AUSSI PAR IMAGE : chaque image sans tic
  // ajoute ses huit, et la teinte qu'un effet réécrit à chaque tic PAPILLOTE
  // au-delà de soixante images par seconde. Vide, aucune image n'est comptée.
  TintIdleFramesSince idle_frames_since;
};
[[nodiscard]] std::array<std::uint8_t, 4> TintAfterEffectsWith(
    std::array<std::uint8_t, 4> base, std::span<const ActorEffectPlay> played,
    const TintContext& context);

}  // namespace ro::render
