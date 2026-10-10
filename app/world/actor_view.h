#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_pick.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/creature_motion.h"
#include "ro/render/led_pose.h"
#include "ro/render/skill_held_pose.h"
#include "ro/render/skill_named_recipe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/strike_play.h"
#include "ro/render/tint_recovery_clock.h"
#include "ro/render/vanish_fade.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "world/actor_ghosts.h"
#include "world/actor_styles.h"
#include "world/actor_tint.h"
#include "world/actor_visibility.h"
#include "world/ailment_tints.h"
#include "world/deferred_walk.h"
#include "world/frozen_poses.h"
#include "world/item_drops.h"
#include "world/map_stage.h"

// Ce qu'un acteur MONTRE : la traduction de ce que le serveur en a dit —
// apparence, posture, marche, coups — vers ce que la garde-robe habille et
// compose. C'est ici que sim rencontre render, et nulle part ailleurs : render
// ne connaît pas sim, et c'est app qui recopie l'un dans l'autre.
namespace moonclient {

struct GroundUnitPick;

// Ce que le serveur a dit d'un acteur, réduit à ce qui choisit ses fichiers.
// Définie, comme `ComposedAsPlayer` et `LayoutOf`, dans `actor_look.cpp`.
[[nodiscard]] ro::render::ActorLook LookOf(const ro::sim::Actor& actor);

// LE MÉTIER QUE L'ACTEUR MONTRE : celui de son corps monté ou de son costume,
// que ses options et deux de ses états choisissent — voir
// `ro::render::ShownJobFor`. Tout ce qui se lit sur le corps se lit sur lui ;
// le métier exercé, lui, reste `look.job`. Le métier tel quel pour qui ne se
// compose pas comme un joueur.
[[nodiscard]] std::uint32_t ShownJobOf(const ro::sim::Actor& actor);

// Vrai quand l'acteur se COMPOSE comme un joueur : un joueur, ou un PNJ vêtu
// d'un métier joueur. C'est son corps qui décide, pas son genre : un tel PNJ
// en prend la disposition et tient, au repos, l'image de son regard.
[[nodiscard]] bool ComposedAsPlayer(const ro::sim::Actor& actor);

// La disposition du .act d'un acteur : celle d'un joueur pour qui se compose
// comme un joueur, celle d'une créature pour tout le reste.
[[nodiscard]] ro::render::SpriteLayout LayoutOf(const ro::sim::Actor& actor);

// CE QUI DÉCIDE DU RANG qu'un état pose à une créature : voir
// `ro::render::CreatureMotionFamily`.
[[nodiscard]] ro::render::CreatureMotionFamily CreatureFamilyOf(const ro::sim::Actor& actor);

// LE RANG qu'un état d'action pose à cet acteur, ou RIEN quand il garde ce
// qu'il jouait : la table des treize rangs pour qui se compose comme un joueur,
// `ro::render::CreatureActionAnimationFor` pour une créature.
[[nodiscard]] std::optional<std::size_t> ActionAnimationOf(const ro::sim::Actor& actor,
                                                           std::int8_t action_state,
                                                           ro::render::SpriteLayout layout);

// LE RANG QUE L'INCANTATION, L'EMPLOI OU LA REPRISE D'UN ENCHAÎNEMENT POSENT :
// celui d'`ActionAnimationOf`, sauf l'attaque et sa variante d'un joueur, que
// l'arme tenue décide — voir `ro::render::PosedActionAnimation`. `posed_ms`
// date le tirage d'un taekwon. Sans catalogue, la table seule.
[[nodiscard]] std::optional<std::size_t> PosedAnimationOf(const ro::render::SpriteCatalog* catalog,
                                                          const ro::sim::Actor& actor,
                                                          std::int8_t action_state,
                                                          ro::render::SpriteLayout layout,
                                                          std::uint32_t posed_ms);

// De combien un acteur RECULE quand plusieurs se disputent le pixel cliqué, en
// unités du monde : la marge que le client ajoute à la profondeur du rectangle
// de chacun selon son genre — cent pour un PNJ ou un compagnon, deux cents pour un monstre, quatre
// cents pour un joueur. Un joueur planté sur un marchand ne l'empêche donc pas de vendre, ni un
// monstre sur un joueur de se faire frapper. Rien pour un objet au sol : petit, il passe devant
// tout. Un monstre mort recule derrière tout le reste.
[[nodiscard]] float PickBiasOf(const ro::sim::Actor& actor);

// LE TREMBLEMENT D'UN CORPS à cette image, en pixels de sprite.
//
// La table dit l'AMPLITUDE, le tirage se fait ici : `render` doit rester sans
// hasard, sans quoi une image de référence ne se compare plus à rien. Le
// tirage dépend de l'acteur et de l'instant — deux corps ne tremblent pas
// ensemble, et la même image redonne le même tremblement.
//
// ⚠ LE CLIENT TIRE QUATRE NOMBRES, UN PAR BORD, et nous en tirons deux. Ses
// quatre bords bougeant séparément, son rectangle change un peu de taille en
// même temps qu'il se déplace ; le nôtre ne fait que se déplacer. Dire
// l'écart vaut mieux que le taire : l'exprimer demanderait une échelle par
// axe, que `DrawnActor` n'a pas.
[[nodiscard]] glm::vec2 BodyJitter(std::uint32_t actor_id, std::uint32_t now_ms,
                                   float amplitude_pixels);

// La motion qu'un acteur joue à un instant, prête à composer.
struct PlayedMotion {
  ro::render::Motion motion = ro::render::Motion::kIdle;
  std::size_t animation = 0;
  // Depuis le début de la motion ; pour une motion en boucle, depuis n'importe
  // quel instant fixe, l'origine lui étant égale.
  float elapsed_ms = 0.0F;
  // L'image sur laquelle la motion se TIENT, quand elle ne se déroule pas :
  // voir MotionAt.
  std::optional<std::size_t> held_frame;
  bool loops = true;
  // 🔴 VRAI QUAND LE PAS DU MONDE NE TOUCHE PLUS À CETTE MOTION : ni image, ni
  // action. Ce qu'on y écrit reste dessiné jusqu'à la motion suivante — voir
  // `GroundSkillFrozenImageAt`.
  bool frozen = false;
  // LA DIRECTION DU .ACT QUE L'IMAGE IMPOSE, à la place de celle que le regard
  // et la caméra donnent. Rien le plus souvent.
  std::optional<std::size_t> held_direction;
  // L'INSTANT OÙ LA MOTION S'EST FIGÉE, quand `frozen` : sa direction à
  // l'écran est celle de cet instant-là, que ni un regard ni la caméra ne
  // changent plus — voir `FrozenPoses`.
  std::uint32_t frozen_since_ms = 0;
  // 🔴 VRAI QUAND LA POSE FIGÉE GARDE L'ACTION ET L'IMAGE QUE L'ACTEUR MONTRAIT
  // à cet instant, au lieu d'en poser une : une créature dans un état figé.
  bool keeps_shown_image = false;
  // L'ÉTAT D'ACTION QUE LE COUP MONTRÉ A POSÉ — deux pour l'attaque, neuf pour
  // sa variante, sept pour l'emploi —, quand la motion est celle d'un coup.
  std::optional<std::int8_t> action_state;
};

// CE QUE LE JOUEUR RÈGLE POUR MIEUX VOIR UN GESTE, et qui s'écarte du client.
// Les deux ne touchent que l'AFFICHAGE : ni l'instant où un coup porte, ni la
// durée d'une barre, ni le rythme des effets n'en dépendent. Leur défaut est
// le client.
struct ReadableMotion {
  // Le geste d'un coup de compétence ne dure pas moins à l'écran, là où une
  // forte vitesse d'attaque le ferait passer trop vite pour être vu. Zéro :
  // la cadence du client. Voir `ro::render::FloorSkillGesture`.
  float skill_gesture_floor_ms = 0.0F;
  // L'incantation ordinaire d'un joueur déroule son geste UNE fois sur toute
  // sa barre, au lieu d'en tenir la première image comme le client.
  bool cast_spans_bar = false;
};

// Ce qu'un acteur joue à cet instant : mort ou assis d'après sa posture, en
// marche d'après son chemin, en garde ou en attente sinon (voir
// `RestMotionAt`) — et, devant l'assise et la marche mais jamais devant la
// mort, la blessure d'un coup reçu ou l'attaque d'un coup porté. Les coups
// demandent la garde-robe, qui sait comment qui frappe s'y prend ; sans elle,
// ils ne se jouent pas.
//
// `looping_elapsed_ms` est le temps qu'une motion en boucle reçoit.
//
// `readable` porte ce que le joueur règle pour mieux VOIR, sans rien changer à
// l'instant où les choses arrivent : voir `ReadableMotion`.
[[nodiscard]] PlayedMotion MotionAt(ro::render::ActorWardrobe* wardrobe,
                                    const ro::sim::World& world, const ro::sim::Actor& actor,
                                    ro::render::ActorOutfit& outfit, const ro::sim::WalkPath& path,
                                    std::uint32_t now_ms, float looping_elapsed_ms,
                                    std::span<const ro::render::ActorEffectPlay> played_effects,
                                    const ReadableMotion& readable = {});

// L'INCANTATION ORDINAIRE D'UN JOUEUR, ÉTALÉE SUR SA BARRE, quand le joueur le
// demande (`ReadableMotion::cast_spans_bar`) : son geste se déroule une fois, du
// début de la barre à sa fin, au lieu de tenir sa première image comme le fait
// le client. `played` est ce que `CastPostureAt` a rendu ; rien ne change hors de
// l'incantation d'état huit d'un joueur, pendant sa barre — ni ce qui la termine,
// ni les états qui mènent leur pose ou se figent.
void SpreadCastOverBar(const ro::sim::Actor& actor, ro::render::ActorOutfit& outfit,
                       ro::render::SpriteLayout layout, std::uint32_t now_ms, PlayedMotion& played);

// L'ATTAQUE QU'UN ENCHAÎNEMENT REJOUE à chaque frappe de son coup, ou rien.
//
// Le lanceur de CH_CHAINCRUSH repart dans sa pose d'attaque à l'instant où
// chaque frappe au-delà de la première touche — un pas de coup multiple après
// la précédente, comme ses nombres —, depuis la première image de l'attaque, à
// une image par tic de vingt-quatre millisecondes. Voir
// `ro::render::SwingReplaysOf`.
//
// ⚠ IL FAUT QUE LA CIBLE SOIT CONNUE : c'est l'impact de la compétence qui
// programme ces reprises, et il ne s'exécute que s'il trouve l'acteur visé.
// Un mort ne reprend rien, ni qui fait le mort. Défini dans `swing_replay.cpp`.
[[nodiscard]] std::optional<PlayedMotion> ReplayedSwingAt(
    ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, std::uint32_t now_ms);

// CE QUE `MotionAt` SAIT DES CHANGEMENTS DE MOTION qui ne se lisent pas sur
// l'acteur seul : l'instant où la blessure en cours a porté, et les effets
// qu'il porte. Un geste de table qui tourne s'arrête au premier venu.
//
// ⚠ LES ÂGES DES EFFETS SONT CEUX D'UN INSTANT, `effects_at_ms` : c'est lui qui
// date leurs tics, et il ne change pas quand on interroge la scène à un autre
// instant — à celui où un coup porte, par exemple.
struct MotionEvents {
  std::optional<std::uint32_t> hurt_at_ms;
  std::span<const ro::render::ActorEffectPlay> played_effects;
  std::uint32_t effects_at_ms = 0;
};

// LA FENÊTRE OÙ L'ON CHERCHE UN CHANGEMENT DE MOTION : après `since_ms`, et
// jusqu'à `until_ms` compris. L'état qu'un effet écrit n'y compte que sur
// demande : il ne repose pas d'action, et ne termine une pose menée que si
// elle est déjà échue.
struct ChangeWindow {
  std::uint32_t since_ms = 0;
  std::uint32_t until_ms = 0;
  bool counts_effects = true;
};

// 🔴 UN GESTE QUI TIENT S'ARRÊTE AU CHANGEMENT DE MOTION SUIVANT, quel qu'il
// soit : vrai quand l'un de ceux que la scène connaît tombe dans la fenêtre —
// une posture, une marche, un arrêt, un coup, une blessure, un ramassage, une
// incantation, un emploi, le sommeil d'une créature, un réveil, l'état qu'un
// effet écrit. Défini dans `actor_skill_posture.cpp`.
[[nodiscard]] bool MotionChangedSince(const ro::render::SpriteCatalog* catalog,
                                      const ro::sim::Actor& actor, ro::render::SpriteLayout layout,
                                      ro::render::Motion current, const MotionEvents& events,
                                      const ChangeWindow& window);

// LA POSTURE D'INCANTATION de qui porte une incantation, ou rien quand elle
// est finie ou ne change pas la posture. Une créature y prend le geste de sa
// table quand la table en nomme un. Sans catalogue, la table ne se lit pas.
// `current` est ce que l'acteur joue sans elle. Défini dans
// `actor_skill_posture.cpp`, comme les deux suivantes.
[[nodiscard]] std::optional<PlayedMotion> CastPostureAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, ro::render::Motion current,
    const MotionEvents& events, std::uint32_t now_ms);

// LA POSTURE D'EMPLOI de la dernière compétence employée, ou rien quand elle
// laisse l'acteur tel qu'il est — `current` est ce qu'il joue sans elle.
[[nodiscard]] std::optional<PlayedMotion> SkillUsePostureAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, ro::render::Motion current,
    const MotionEvents& events, std::uint32_t now_ms);

// LA POSE MENÉE QUI TIENT L'ACTEUR à cet instant, ou rien : la pose d'une
// compétence à l'annonce ou à l'impact, l'emploi ordinaire de seize métiers,
// l'incantation d'un état qui mène sa pose (`ro::render::StateLeadsPose`). La
// dernière posée gagne, et la motion suivante que
// `events` et l'acteur font connaître la termine. Voir `ro/render/led_pose.h`.
[[nodiscard]] std::optional<ro::render::HeldPoseFrame> LedPoseAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::SpriteLayout layout, ro::render::Motion current, const MotionEvents& events,
    std::uint32_t at_ms);

// LA POSE DU MOINE QUI TIENT UNE LAME, ou rien : l'image que
// `ro::render::kBladeStopHeldAnimation` nomme, menée sans échéance tant que la
// prise dure, puis GARDÉE après le relâchement jusqu'à la motion suivante —
// l'échéance d'une pose menée n'ôte pas l'image tenue. Tant que la prise dure,
// la blessure n'a pas lieu : voir `LedPoseHoldsAt`.
[[nodiscard]] std::optional<ro::render::HeldPoseFrame> BladeStopPoseAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::SpriteLayout layout, ro::render::Motion current, const MotionEvents& events,
    std::uint32_t at_ms);

// VRAI QUAND UNE POSE MENÉE TIENT L'ACTEUR à cet instant — celle d'une
// compétence, ou celle qu'un de ses effets mène. Le client n'y laisse pas
// porter la blessure : elle n'a alors pas lieu du tout.
[[nodiscard]] bool LedPoseHoldsAt(const ro::render::SpriteCatalog* catalog,
                                  const ro::sim::Actor& actor, ro::render::SpriteLayout layout,
                                  ro::render::Motion current, const MotionEvents& events,
                                  std::uint32_t at_ms);

// VRAI QUAND LA POSE MENÉE DU COUP que l'acteur porte a déclaré son geste fini
// à cet instant : l'échéance d'une pose minutée — celle de l'impact, du
// métier ou de l'état —, sous un état que la fin ramène au repos. Le reste de
// la passe du coup ne se joue alors pas, et l'acteur se repose. `current` est
// ce qu'il joue sans le coup. Définie dans `actor_led_pose.cpp`, comme les
// poses menées et les trois suivantes.
[[nodiscard]] bool StrikeFinishedByLedPose(const ro::sim::Actor& actor,
                                           ro::render::SpriteLayout layout,
                                           ro::render::Motion current, std::uint32_t now_ms);

// De même pour la pose que mène l'emploi d'état `use_state` : le paladin tient
// l'image zéro de l'incantation quatre cents millisecondes, puis se repose
// sans jouer la fin du rang.
[[nodiscard]] bool SkillUseFinishedByLedPose(const ro::sim::Actor& actor,
                                             ro::render::SpriteLayout layout, std::int8_t use_state,
                                             std::uint32_t now_ms);

// L'IMAGE QUE MÈNE L'ÉTAT D'INCANTATION `cast_state`, sur le rang qu'il pose,
// et si sa durée court encore ; rien quand l'état ne mène pas de pose. Voir
// `ro::render::StateLedFrameAt`.
[[nodiscard]] std::optional<ro::render::StateLedFrame> CastLedFrameAt(
    const ro::sim::Actor& actor, ro::render::SpriteLayout layout, std::int8_t cast_state,
    std::uint32_t at_ms);

// De même pour l'état que pose l'emploi de l'acteur : le taekwon qui prend sa
// garde (état 23) tient l'image 3 de son rang d'incantation.
[[nodiscard]] std::optional<ro::render::StateLedFrame> SkillUseLedFrameAt(
    const ro::sim::Actor& actor, ro::render::SpriteLayout layout, std::uint32_t at_ms);

// L'ÉTAT QU'UN COUP PORTÉ fait prendre : l'attaque pour un coup ordinaire,
// l'état d'emploi de la compétence pour un coup de compétence — le client
// résout l'un comme l'autre par la même table —, ou rien pour les compétences
// dont le coup laisse la motion en place. Définie dans
// `actor_skill_posture.cpp`, comme la suivante.
[[nodiscard]] std::optional<std::int8_t> StrikeState(const ro::sim::Strike& strike);

// L'INSTANT D'UN TIC d'un effet joué, dans l'horloge de la scène : son âge est
// celui de `events.effects_at_ms`.
[[nodiscard]] std::uint32_t TickInstantMs(const ro::render::ActorEffectPlay& play,
                                          std::uint32_t tick, const MotionEvents& events);

// CE QUE L'ACTEUR JOUAIT à un instant passé, avec les effets qu'il portait
// alors et l'âge qu'ils y avaient.
using MotionAtInstant = std::function<PlayedMotion(
    std::uint32_t at_ms, std::span<const ro::render::ActorEffectPlay> played_effects)>;

// L'IMAGE QU'UNE COMPÉTENCE DE CASE QUI LAISSE LA MOTION impose, ou rien —
// voir `ro::sim::GroundSkillKeepsCasterMotion`. Le client écrit la dernière
// image de `ro::render::kGroundSkillHeldImageAnimation` à la place de celle de
// l'acteur ; une motion qui déroule son image l'efface au pas suivant, et seule
// en garde trace une motion qui TIENT la sienne, bornée à ce qu'elle porte :
// l'attente et l'assise d'un joueur, jusqu'au regard suivant, et l'incantation
// ordinaire. Il faut que `played` soit déjà ce qu'il jouait quand la
// compétence est arrivée, et que rien n'ait changé la motion depuis.
[[nodiscard]] std::optional<std::size_t> GroundSkillHeldFrameAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, const PlayedMotion& played,
    const MotionEvents& events, std::uint32_t now_ms, const MotionAtInstant& motion_at);

// 🔴 L'IMAGE QU'UNE COMPÉTENCE DE CASE QUI LAISSE LA MOTION écrit sur une motion
// FIGÉE, ou rien : le pas du monde ne la réécrivant pas, c'est l'action à part
// elle-même qui se dessine — la dernière image du rang onze, vue de la
// direction zéro — jusqu'à la motion suivante. Il faut que `played` soit déjà
// ce qu'il jouait quand la compétence est arrivée. Défini, comme les suivantes,
// dans `actor_held_image.cpp`.
[[nodiscard]] std::optional<PlayedMotion> GroundSkillFrozenImageAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, const PlayedMotion& played,
    const MotionEvents& events, std::uint32_t now_ms, const MotionAtInstant& motion_at);

// Vrai quand l'attente de cet acteur TIENT l'image de son regard au lieu de
// dérouler ses trois images : un joueur, ou qui se compose comme lui — sauf cinq
// comptes à part, dont le client laisse l'attente se dérouler. Voir
// `ComposedAsPlayer`.
[[nodiscard]] bool IdleHoldsLookImage(const ro::sim::Actor& actor);

// L'IMAGE DU REGARD DROIT : la première des trois orientations du regard.
inline constexpr std::size_t kLookAheadImage = 0;

// L'IMAGE QU'UNE VIGNETTE DE CET ACTEUR TIENT pour cette animation, ou rien
// pour une animation qui se déroule.
//
// 🔴 LA RÈGLE DE LA PARTIE, POUR UN PORTRAIT : au repos et assis, les trois
// images d'un joueur sont les orientations du regard — voir
// `IdleHoldsLookImage`. Les dérouler fait dire non au pantin. Un portrait n'a
// pas de regard à suivre : il regarde droit.
//
// ⚠ CE QU'IL PORTE CONTINUE DE VIVRE : une coiffe animée a sa propre horloge,
// que l'image tenue n'arrête pas. Voir `ro::render::ActorOutfit::Composed`.
[[nodiscard]] std::optional<std::size_t> VignetteHeldFrame(const ro::sim::Actor& actor,
                                                           std::size_t animation);

// Vrai quand le sommeil FIGE cet acteur : une créature endormie, hors PNJ.
[[nodiscard]] bool SleepFreezesMotion(const ro::sim::Actor& actor, ro::render::SpriteLayout layout);

// 🔴 UNE CRÉATURE QUI DORT se fige sur la première image de son attente, du
// sommeil jusqu'à la motion suivante — le réveil compris. Rien pour un joueur,
// que le sommeil laisse tel qu'il était, ni pour un PNJ, que le client remet à
// l'attente ordinaire.
[[nodiscard]] std::optional<PlayedMotion> AsleepCreatureMotionAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::SpriteLayout layout, const MotionEvents& events, std::uint32_t now_ms);

// LE REPOS D'UN ACTEUR à cet instant : la GARDE ou l'ATTENTE, selon l'état que
// son dernier geste lui a fait quitter, et son mode PK. Voir
// `ro::sim::GuardsLeaving` pour la règle, et `actor_skill_posture.cpp` pour
// l'état de chaque geste — coup porté, coup encaissé, incantation, emploi,
// ramassage, marche, posture, et l'état qu'un effet écrit.
[[nodiscard]] ro::render::Motion RestMotionAt(const ro::sim::Actor& actor,
                                              const MotionEvents& events, std::uint32_t now_ms);

// Vrai quand l'état d'emploi laisse la posture en place parce que l'acteur est
// ASSIS ou TERRÉ : le client n'y touche alors ni à l'action, ni à la cadence,
// ni à l'image du coup.
[[nodiscard]] bool SkillUseKeepsPosture(const ro::sim::Actor& actor, ro::render::Motion current,
                                        std::int8_t use_state);

// QUI FAIT LE MORT se couche comme un mort — la chute, puis la dernière image —,
// et le client ne change plus sa pose tant que l'état tient : ni coup, ni
// blessure, ni incantation. Rien pour qui ne le fait pas.
[[nodiscard]] std::optional<PlayedMotion> TrickDeadMotionAt(const ro::sim::Actor& actor,
                                                            ro::render::SpriteLayout layout,
                                                            std::uint32_t now_ms);

// Le recensement d'une image : ce que la scène portait, et ce qui n'y a pas
// été dessiné, par CAUSE.
//
// Voir ComposeScene, qui le remplit.
//
// Un acteur qu'on ne voit pas ne dit pas pourquoi : absent du monde, sans
// sprite dans l'installation, caché par un état, ou habillé sans qu'aucun
// calque n'en sorte — tout cela se ressemble à l'œil. Ces comptes-là les
// séparent, et c'est la seule façon de trancher sans deviner.
struct SceneCensus {
  std::size_t actors = 0;
  std::size_t drawn = 0;
  // Ni personnage ni objet au sol : une zone de compétence n'a rien à montrer.
  std::size_t without_sprite = 0;
  // Caché, camouflé, invisible.
  std::size_t hidden = 0;
  // Corps introuvable dans l'installation : rien ne s'ancre, on ne dessine pas.
  std::size_t without_body = 0;
  // Habillé, mais la composition n'a rendu aucun calque.
  //
  // ⚠ LE PLUS HAUT ATTEINT, et non le compte de cette image : voir
  // `ReportCensus`. Ce compte-là suit l'image d'animation, et une trame vide
  // le ferait osciller à chaque image.
  std::size_t without_image = 0;

  [[nodiscard]] friend bool operator==(const SceneCensus&, const SceneCensus&) = default;
};

// Dit les sprites que la garde-robe n'a pas trouvés, et chacun une seule fois.
//
// `already_reported` porte combien l'ont déjà été : la garde-robe ne fait
// qu'ajouter à sa liste, et la relire en entier à chaque image répéterait la
// même ligne jusqu'à noyer le journal. C'est le pendant de ReportCensus — ce
// que la composition n'a pas trouvé, dit une fois.
void ReportMissingSprites(ro::render::ActorWardrobe* wardrobe, std::size_t& already_reported);

// Écrit le recensement sur la sortie d'erreur, et seulement s'il a CHANGÉ.
//
// Une ligne par image noierait le journal et cacherait ce qu'on y cherche ;
// c'est le changement qui apprend quelque chose. `last_reported` porte ce qui
// a déjà été dit.
void ReportCensus(const SceneCensus& census, SceneCensus& last_reported);

// CE QU'UN COUP FAIT JOUER À QUI LE PORTE — le rang, sa cadence, sa durée,
// l'image de frappe et l'instant où il touche —, d'après la tenue de l'auteur
// et ce que le monde sait de lui : son âme, sa posture, son genre de familier.
// Voir `ro::render::StrikePlayOf`. Rien sans tenue.
//
// 🔴 UN COUP D'ÉCLABOUSSURE NE JOUE RIEN, et touche à l'image de frappe de
// l'action que l'auteur joue encore : voir `ro::render::SplashHitKeepsAction`.
// C'est le monde qui dit laquelle.
[[nodiscard]] std::optional<ro::render::StrikePlay> StrikePlayFor(
    ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world,
    const ro::sim::Actor& attacker, const ro::sim::Strike& strike);

// LE COUP DONT L'ACTEUR MONTRE LE GESTE : son dernier, sauf quand c'est une
// éclaboussure, qui ne pose rien — c'est alors celui d'avant. Rien sans coup.
[[nodiscard]] const ro::sim::Strike* PosingStrikeOf(const ro::sim::World& world,
                                                    const ro::sim::Actor& actor);

// L'instant, depuis le début d'un coup, où il TOUCHE : l'image de frappe que
// le coup laisse sur son auteur, à la cadence du rang qu'il joue, plus le
// rattrapage d'un projectile lent et le retard propre à la compétence. C'est
// là que la cible bronche et que ses dégâts s'affichent. Zéro si l'attaquant
// n'est plus là ou n'a pas de tenue.
[[nodiscard]] float HitLandsAtMs(ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world,
                                 const ro::sim::Strike& strike);

// L'ALLURE D'UN ACTEUR QUI EMPLOIE UNE COMPÉTENCE : l'action que son état
// d'emploi lui fait prendre, ce que dure une de ses images en pas de
// vingt-quatre millisecondes — grandie du facteur du coup quand le paquet en
// annonce un —, et l'image où son coup porte. Un emploi qui laisse la posture
// laisse aussi l'allure : c'est alors celle de l'action que l'acteur tient.
// Rien sans tenue. Voir `ro::render::NamedEffectPaceOf`.
[[nodiscard]] std::optional<ro::render::NamedEffectPaceOf> NamedPaceOf(
    ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world, const ro::sim::Actor& actor,
    const ro::sim::WalkPath& path, std::uint16_t skill_id,
    std::optional<std::uint32_t> attack_motion_ms, std::uint32_t at_ms);

// Vrai quand une créature morte a fini son agonie et son cadavre, qui dure
// `corpse_fade_ms` : le client l'efface de lui-même, le serveur ne le fera
// pas. Un joueur mort reste.
[[nodiscard]] bool HasVanished(const ro::sim::Actor& actor, ro::render::ActorOutfit& outfit,
                               std::uint32_t now_ms, std::uint32_t corpse_fade_ms);

// L'OPACITÉ D'UN CORPS QUI S'EN VA : pleine tant que l'agonie se joue, puis
// décroissante jusqu'à l'effacement. Pleine aussi pour qui ne disparaît pas.
//
// Elle MULTIPLIE l'alpha déjà en place plutôt que de le remplacer : un corps
// que la mort prend camouflé reste camouflé en s'effaçant.
[[nodiscard]] std::uint8_t VanishAlpha(const ro::sim::Actor& actor, ro::render::ActorOutfit& outfit,
                                       std::uint32_t now_ms, std::uint32_t corpse_fade_ms);

struct ComposedScene;

// LA MÊME, multipliée par celle d'une créature que le serveur vient de faire
// APPARAÎTRE : elle monte de la transparence. Voir `ro/render/vanish_fade.h`.
[[nodiscard]] std::uint8_t ArrivalAndDepartureAlpha(const ro::sim::Actor& actor,
                                                    ro::render::ActorOutfit& outfit,
                                                    std::uint32_t now_ms,
                                                    const ComposedScene& scene);

// CE QU'UNE IMAGE MONTRE DES ACTEURS : les corps, et ce qui se peint autour.
//
// ⚠ LES QUATRE LISTES NE SONT PAS INTERCHANGEABLES, et c'est pourquoi elles
// voyagent ensemble plutôt qu'une à une :
//
//   * `drawn` et `ids` sortent APPARIÉS — jauges, noms, émoticônes, sons et
//     désignation s'appuient sur cet appariement, et une entrée de plus dans
//     l'une seule donnerait à un acteur l'identifiant de son voisin ;
//   * `behind` porte les HALOS, qui se peignent AVANT les corps — l'ordre du
//     client, qui soumet ses halos avant le sprite qu'ils entourent ;
//   * `front` porte les VOILES, qui se peignent APRÈS — un voile peint avant
//     serait entièrement recouvert par le corps qu'il colore.
//
// Ni `behind` ni `front` n'ont d'identifiant : ce ne sont pas des acteurs, on
// ne les clique pas, ils ne portent ni nom ni jauge.
struct ComposedScene {
  std::vector<ro::render::DrawnActor> drawn;
  std::vector<std::uint32_t> ids;
  std::vector<ro::render::DrawnActor> behind;
  std::vector<ro::render::DrawnActor> front;
  // COMBIEN D'IMAGES ONT ÉTÉ COMPOSÉES, depuis toujours : `Clear` ne le remet
  // pas à zéro. L'arme doublée ne paraît qu'une image sur deux, et c'est ce
  // compte qui le dit.
  //
  // ⚠ LE CLIENT TIENT SA BASCULE PAR ACTEUR, nous par scène : deux acteurs
  // doublés battent ici en phase, chez lui chacun à la sienne. La cadence est la
  // même.
  std::uint32_t composed_frames = 0;
  // LES IMAGES QUI NE DOIVENT AUCUN TIC, qui font papilloter une teinte
  // réécrite à chaque tic : voir `ro::render::TintRecoveryClock`. Comme le
  // compte ci-dessus, `Clear` n'y touche pas.
  ro::render::TintRecoveryClock tint_clock;
  // LES ACTEURS DONT LA DISPARITION EST FINIE : ils ne se sont pas dessinés, et
  // c'est `SettleWorld` qui les retire du monde, hors du dessin.
  std::vector<std::uint32_t> vanished;
  // LES UNITÉS AU SOL QUE LA SOURIS PEUT DÉSIGNER, chacune au point de sa case :
  // voir `PickGroundUnit`. Tenues par les unités au sol elles-mêmes.
  std::span<const GroundUnitPick> ground_units;
  // Et le plancher de leur emprise, en pixels : voir `ro::render::PickFloorPixels`.
  float ground_unit_floor_pixels = ro::render::kGroundPickFloorFactor;
  // CE QUE LE PERSONNAGE DU JOUEUR JOUAIT à cette image, s'il s'est composé :
  // un coup, un ramassage ou une assise retiennent sa marche — voir
  // `DeferredWalk`.
  std::optional<PlayerGesture> player_gesture;
  // CE QUE DURE LE CADAVRE d'une créature, réglé par le joueur : voir
  // `ro/render/vanish_fade.h`. `Clear` n'y touche pas.
  std::uint32_t corpse_fade_ms = ro::render::kClientCorpseFadeMs;
  // Et ce que durent le fondu d'une créature qui apparaît et celui d'un acteur
  // qui sort de vue, réglés par le joueur.
  std::uint32_t appear_fade_ms = ro::render::kClientAppearFadeMs;
  std::uint32_t leave_fade_ms = ro::render::kClientLeaveFadeMs;
  // Et ce que le joueur règle pour mieux voir les gestes : voir `MotionAt`.
  ReadableMotion readable_motion;
  // LES ACTEURS SORTIS DE VUE qui s'effacent encore : des copies, que le monde
  // a déjà oubliées. `Clear` n'y touche pas ; un changement de carte, si.
  std::vector<ro::sim::FadingActor> fading;

  void Clear() {
    drawn.clear();
    ids.clear();
    behind.clear();
    front.clear();
    vanished.clear();
    ground_units = {};
    player_gesture.reset();
  }
};

// ÉTEINT LES EFFETS QU'UN AUTRE EFFET ANNULE.
//
// Un effet peut demander au client de RETIRER un autre effet de la liste de son
// acteur : `EF_NPC_STOP2_DEL` passe ainsi le numéro de `EF_NPC_STOP2` au
// gestionnaire de liste de l'acteur. Retiré, l'effet cesse de relever ses
// drapeaux, qui tombent aussitôt — voir `ro::render::RenderBitsAt`.
//
// L'effet qui annule s'éteint AVEC sa cible : sans cela il redemanderait le
// retrait à chaque image, et resterait dans la liste pour rien.
void ApplyEffectCancellations(ro::sim::World& world);

// CE QUE LA DERNIÈRE COMPOSITION A CONSTATÉ, RENDU AU MONDE : les corps dont la
// disparition est finie s'en vont, puis les effets annulés s'éteignent, et les
// effets finis à `now_ms` quittent la liste de leur acteur.
//
// 🔴 HORS DU DESSIN, et c'est la règle : le dessin lit le monde, il ne le
// change pas. Une sortie précoce du dessin sautait sinon ce qui devait se
// faire à chaque tour. L'appeler après les trames du tour, avant de composer.
void SettleWorld(ro::sim::World& world, const ComposedScene& previous, std::uint32_t now_ms);

// Compose TOUS les acteurs du monde pour une image, et NOTE ceux dont la
// disparition est finie — voir `SettleWorld`.
//
// `wardrobe` habille chacun d'après ce que le serveur a dit de lui. Sans elle —
// une installation dont les tables ne se lisent pas —, personne ne se dessine.
//
// `drawn` et `ids` sortent APPARIÉS : le quoi et le qui, dans le même ordre.
// Tout ce qui se peint par-dessus la scène — jauges, noms, bulles — s'appuie
// sur cet appariement.
//
// `copies` reçoit les HALOS que certains effets posent autour d'un corps : le
// même sprite, à une échelle et une couleur près.
//
// ⚠ ELLES NE VONT PAS DANS `drawn`, ET C'EST UNE CONTRAINTE, PAS UN RANGEMENT.
// `drawn` et `ids` sortent APPARIÉS, et cinq choses s'appuient sur cet
// appariement — jauges, noms, émoticônes, sons, désignation. Une copie glissée
// dans `drawn` se verrait attribuer l'identifiant de son voisin : deux jauges
// de vie sur le même monstre, un son joué deux fois, un clic qui tombe sur un
// halo. Elles se dessinent donc à part, et AVANT les corps — c'est l'ordre du
// client, qui soumet ses halos avant le sprite qu'ils entourent.
//
// `styles` fait le lien entre les recettes que le serveur diffuse et les
// palettes que la garde-robe peint — voir `world/actor_styles.h`. Nul pour le
// décor de l'écran de connexion : chacun garde alors son apparence native.
// LA LUMIÈRE DE LA CARTE SUR LES CORPS. Sans sol, aucune : la teinte de chacun
// reste la sienne. `cold` suit la lumière froide du sol.
struct ActorLighting {
  const ro::render::GroundLight* ground = nullptr;
  bool cold = false;
  // LA TEINTE QUE LES ALTÉRATIONS ONT ÉCRITE, et qui reste : voir
  // `AilmentTints`. Nulle hors du jeu.
  const AilmentTints* ailment_tints = nullptr;
};

[[nodiscard]] SceneCensus ComposeScene(
    ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world, ro::sim::WalkPlanner& planner,
    const MapStage& stage, float camera_yaw_degrees, std::uint32_t now_ms, float looping_elapsed_ms,
    ActorStyles* styles, ComposedScene& scene, ActorGhosts& ghosts, ItemDrops& drops,
    FrozenPoses& frozen_poses, const ActorLighting& lighting = {});

// LE MÉLANGE DE CHAQUE CALQUE DU CORPS, d'après ce que les effets en font :
// quels calques s'ajoutent au décor, et si l'arme se double à cette image —
// `weapon_echo_frame` dit si c'en est une des deux.
inline void BlendLayers(const ro::render::ActorBodyPose& pose, bool weapon_echo_frame,
                        ro::render::DrawnActor& body) {
  body.additive_parts = pose.additive_parts;
  body.doubles_weapon = pose.doubles_weapon && weapon_echo_frame;
}

// LES CALQUES QU'UNE COPIE DU CORPS reprend, et comment elle s'y mélange.
inline void CopyLayers(const ro::render::ActorBodyCopy& copy, ro::render::DrawnActor& drawn) {
  drawn.shown_parts = copy.parts;
  drawn.additive_parts = copy.additive ? copy.parts : ro::render::ActorPartSet{0};
}

}  // namespace moonclient
