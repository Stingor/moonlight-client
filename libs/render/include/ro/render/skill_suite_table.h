#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ro::render {

// LA SUITE D'UNE COMPÉTENCE : tout ce que le client fait naître et fait
// entendre, dans l'ordre, quand elle frappe ou quand elle est annoncée.
//
// 🔴 UNE COMPÉTENCE N'A PAS UN EFFET PAR MOMENT, ELLE EN A UNE SUITE. Spear
// Boomerang lance son boomerang PUIS pose son éclat ; Moon Slasher fait naître
// quatre effets et un son ; Tetra Vortex joue trois sons ensemble. Une table qui
// ne garde qu'un effet par compétence montre une compétence tronquée, sans rien
// dire.
//
// ⚠ UNE ÉTAPE PEUT DÉPENDRE DE L'ARGUMENT du paquet — le niveau, ou pour
// certaines compétences un MONTANT. Heal choisit son effet selon ce qu'il
// soigne : l'étape porte l'intervalle de l'argument où elle joue.

// Le moment de la suite : le paquet qui la déclenche.
enum class SkillSuiteMoment : std::uint8_t {
  // Le coup porté : le répartiteur des dégâts, puis l'effet visé de la carte.
  kImpact,
  // La compétence annoncée sans dégâts.
  kAnnounced,
};

// Ce qu'une étape fait.
enum class SkillSuiteKind : std::uint8_t {
  kEffect,
  kSound,
  // RETIRE de son porteur l'effet `effect_id` qu'il porte déjà. Two-Hand
  // Quicken ôte son aura avant d'en semer une neuve : relancée, elle ne se
  // double pas.
  kRemoveEffect,
  // FAIT TREMBLER L'ÉCRAN pendant `quake_ms` : la commande 122 au mode de
  // jeu. Steel Body, Explosion Spirits, Asura Strike en envoient une.
  kScreenQuake,
  // FAIT MONTER LE MONTANT de l'annonce au-dessus de la cible, de la couleur
  // `number_argb` : le vert du soin, le bleu de la magie rendue.
  kSupportNumber,
  // UNE BOUFFÉE DE FUMÉE aux pieds du porteur, figée là où il se tenait : le
  // sprite `이팩트\smoke` joué une fois, une image toutes les vingt
  // millisecondes, même quand le porteur est caché — voir `kSmokePuffSprite`.
  kSmokePuff,
};

// Le sprite de la bouffée, et sa cadence en pas de vingt-quatre millisecondes.
inline constexpr std::string_view kSmokePuffSprite = "data\\sprite\\이팩트\\smoke";
inline constexpr float kSmokePuffFrameSteps = 1.0F / 1.2F;

// Ce qu'une instance vise. Les figures qui tendent un trait ou lancent un
// projectile en tirent leur second point.
enum class SkillSuiteAim : std::uint8_t {
  // Le client ne donne aucun point : l'autre bout — la cible pour une instance
  // née sur le lanceur, le lanceur sinon.
  kOtherEnd,
  // La place de la cible, ou celle du lanceur — même quand l'instance y est
  // née : Brandish Spear vise ce qu'il frappe, depuis ce qu'il frappe.
  kTarget,
  kCaster,
  // Un point de l'anneau centré sur le lanceur, l'angle tiré dans un arc.
  kCasterRing,
  // AU-DELÀ DE LA CIBLE, dans le sens du tir : la cible plus l'écart qui la
  // sépare du lanceur. Ce qui naît sur la cible pointe alors à l'opposé du
  // lanceur, comme un trait qui la traverse.
  kBeyondTarget,
};

// LE RÉGLAGE DES EFFETS ALLÉGÉS (`/mineffect`) choisit parfois l'effet : une
// compétence en pose une variante légère à la place de sa figure pleine.
enum class SkillSuiteSetting : std::uint8_t {
  kAlways,
  kFullEffects,
  kMinimalEffects,
};

// Où l'étape naît, ou d'où elle sonne.
enum class SkillSuiteCarrier : std::uint8_t {
  kCaster,
  kTarget,
  // Dans le MONDE, à un décalage de la cible, sans porteur qu'elle suive.
  kWorldNearTarget,
  // Née sur le lanceur et tendue vers la cible.
  kCasterTowardTarget,
  // Dans le MONDE, à la place du LANCEUR, sans porteur qu'elle suive : le client
  // passe la position du lanceur et aucun propriétaire.
  kWorldAtCaster,
  // Sur le JOUEUR DE LA CAMÉRA, ni lanceur ni cible : l'aura d'Intimidate naît
  // sur le personnage de qui regarde, et s'éteint avec lui quand le sort le
  // téléporte.
  kLocalPlayer,
};

struct SkillSuiteStep {
  std::uint16_t skill_id = 0;
  SkillSuiteMoment moment = SkillSuiteMoment::kImpact;
  SkillSuiteKind kind = SkillSuiteKind::kEffect;
  SkillSuiteCarrier carrier = SkillSuiteCarrier::kTarget;
  // L'effet d'une étape d'effet ; zéro pour un son.
  std::uint32_t effect_id = 0;
  // Le fichier d'une étape de son, relatif au dossier des sons.
  std::string_view sound;
  // L'intervalle de l'argument du paquet où l'étape joue, bornes comprises.
  std::int32_t argument_min = std::numeric_limits<std::int32_t>::min();
  std::int32_t argument_max = std::numeric_limits<std::int32_t>::max();
  // Le décalage à la cible d'une étape née dans le monde, en unités du monde du
  // client — l'ordonnée et la profondeur y sont celles du client.
  float offset_x = 0.0F;
  float offset_z = 0.0F;
  // VRAI QUAND LE CLIENT DONNE À CETTE INSTANCE LE PARAMÈTRE DU COUP — le
  // nombre de coups, dont Soul Strike tire son nombre de boules. Faux, elle
  // garde un.
  bool receives_param = false;
  // Pour un son différé, en tics de l'effet.
  std::uint16_t delay_ticks = 0;
  // LE POINT QUE L'INSTANCE VISE, quand le client le lui donne (message 14).
  SkillSuiteAim aim = SkillSuiteAim::kOtherEnd;
  // Pour `kCasterRing` : le rayon, en unités du monde, et l'arc où l'angle se
  // tire, en degrés — premier degré compris, largeur de l'arc.
  float aim_ring_radius = 0.0F;
  std::int16_t aim_arc_first_degrees = 0;
  std::int16_t aim_arc_span_degrees = 0;
  // Selon le réglage des effets allégés (`/mineffect`) du joueur.
  SkillSuiteSetting effects_setting = SkillSuiteSetting::kAlways;
  // SELON QU'UN ACTEUR PORTE DÉJÀ UN EFFET : l'étape ne joue que si
  // `condition_carrier` porte `condition_effect_id` (ou ne le porte pas, quand
  // `condition_carried` est faux). Zéro : pas de condition.
  std::uint32_t condition_effect_id = 0;
  SkillSuiteCarrier condition_carrier = SkillSuiteCarrier::kCaster;
  bool condition_carried = false;
  // LE DÉLAI AVANT QUE LE FICHIER DE L'EFFET NE DÉMARRE, en millisecondes :
  // Arrow Storm tire ses trois volées à 600, 800 et 1 000. Le client le décompte
  // de seize par tic et ne fait rien vivre tant qu'il reste positif.
  std::uint16_t start_delay_ms = 0;
  // La durée d'une étape de tremblement, en millisecondes.
  std::uint16_t quake_ms = 0;
  // La couleur d'une étape de montant, en 0xAARRGGBB.
  std::uint32_t number_argb = 0;
  // UNE SECONDE CONDITION, sur le même porteur que la première, quand l'étape en
  // demande deux : 411 ne pose 442 que si son lanceur ne porte ni 443 ni 442.
  std::uint32_t condition2_effect_id = 0;
  bool condition2_carried = false;
  // UN POINT ET UN DÉLAI TIRÉS, pour une étape née dans le monde près de la
  // cible : une distance entière dans [scatter_radius_min, scatter_radius_max],
  // un angle multiple de scatter_angle_step_degrees, et un délai de
  // delay_step_ms fois un entier tiré sous delay_steps, ajouté au délai fixe.
  // Judex fait tomber douze paires d'éclats ainsi.
  std::uint8_t scatter_radius_min = 0;
  std::uint8_t scatter_radius_max = 0;
  std::uint8_t scatter_angle_step_degrees = 0;
  std::uint16_t delay_step_ms = 0;
  std::uint8_t delay_steps = 0;
  // Vrai quand l'étape reprend le point et le délai tirés pour la précédente :
  // les deux éclats d'une paire tombent ensemble.
  bool reuse_previous_draw = false;
  // LES SORTES DE COUP où l'étape joue, un bit par sorte ; zéro pour toutes.
  // Un son d'impact propre à une compétence se tait sous les sortes où le
  // client ne le joue pas.
  std::uint16_t damage_types = 0;
  // ⚠ EN QUEUE, comme tout ce qui s'ajoute : la table se bâtit par position.
  //
  // VRAI QUAND LE RETARD SUIT LA CADENCE DU LANCEUR au lieu de `start_delay_ms` :
  // le client le calcule au coup — voir `SkillSuiteCadenceDelayMs`.
  bool delay_from_caster_cadence = false;
  // CE QUE L'ARGUMENT DU PAQUET AJOUTE AU DÉCALAGE, par unité d'argument : une
  // grille qui s'écarte avec le niveau. Même repère que `offset_x`/`offset_z`.
  float offset_x_per_argument = 0.0F;
  float offset_z_per_argument = 0.0F;
  // L'ÉCHELLE DE L'IMAGE d'un effet en fichier, autour de son point du sol —
  // relèvement compris. Voir `DrawnEffect::scale`.
  float scale = 1.0F;
  // VRAI QUAND L'IMAGE SE DESSINE TOURNÉE d'un angle tiré à la naissance :
  // un multiple de dix degrés, de zéro à trois cent cinquante, fixe à l'écran.
  // Voir `SkillSuiteDrawSpinDegrees`.
  bool spin_drawn = false;
  // VRAI QUAND L'ÉTAPE NE JOUE QUE SI LE LANCEUR EST LE JOUEUR LOCAL : la
  // plaisanterie du barde et le cri de la danseuse font PARLER celui qui les
  // lance, et le client ne fait parler que son propre personnage. Lancées par
  // un autre, ces étapes ne jouent pas.
  bool cast_by_local_player_only = false;
};

// L'ANGLE TIRÉ d'une étape `spin_drawn`, en degrés : le reste de `draw` par
// trente-six, fois dix. Zéro la laisse droite.
[[nodiscard]] constexpr float SkillSuiteDrawSpinDegrees(std::uint32_t draw) {
  constexpr std::uint32_t kSpinSlots = 36;
  constexpr float kSpinSlotDegrees = 10.0F;
  return static_cast<float>(draw % kSpinSlots) * kSpinSlotDegrees;
}

// LE RETARD QUE LA CADENCE DU LANCEUR DONNE : cent millisecondes par pas de
// vingt-quatre de la durée d'image de son action en cours, plus cent.
[[nodiscard]] constexpr std::int32_t SkillSuiteCadenceDelayMs(float cadence_steps) {
  constexpr float kPerStep = 100.0F;
  constexpr float kBase = 100.0F;
  return static_cast<std::int32_t>(cadence_steps * kPerStep + kBase);
}

// LE DÉCALAGE D'UNE ÉTAPE pour cet argument.
[[nodiscard]] constexpr glm::vec2 SkillSuiteOffsetFor(const SkillSuiteStep& step,
                                                      std::int32_t argument) {
  const auto facteur = static_cast<float>(argument);
  return {step.offset_x + step.offset_x_per_argument * facteur,
          step.offset_z + step.offset_z_per_argument * facteur};
}

// Vrai si cette étape joue pour un coup de cette sorte.
[[nodiscard]] constexpr bool SkillSuiteStepFitsDamageType(const SkillSuiteStep& step,
                                                          std::uint8_t damage_type) {
  constexpr std::uint8_t kDamageTypeBits = 16;
  return step.damage_types == 0 ||
         (damage_type < kDamageTypeBits && ((step.damage_types >> damage_type) & 1U) != 0);
}

// Vrai si cette étape joue sous ce réglage des effets allégés.
[[nodiscard]] constexpr bool SkillSuiteStepShows(const SkillSuiteStep& step, bool minimal_effects) {
  return step.effects_setting == SkillSuiteSetting::kAlways ||
         (step.effects_setting == SkillSuiteSetting::kMinimalEffects) == minimal_effects;
}

// La table entière, triée par compétence, puis par moment, puis dans l'ordre du
// client.
[[nodiscard]] std::span<const SkillSuiteStep> SkillSuiteTable();

// La suite d'une compétence à ce moment, dans l'ordre du client — vide quand le
// client n'y fait rien.
[[nodiscard]] std::span<const SkillSuiteStep> SkillSuiteOf(std::uint16_t skill_id,
                                                           SkillSuiteMoment moment);

// Vrai si cette étape joue pour cet argument du paquet.
[[nodiscard]] constexpr bool SkillSuiteStepPlays(const SkillSuiteStep& step,
                                                 std::int32_t argument) {
  return argument >= step.argument_min && argument <= step.argument_max;
}

// Vrai quand l'étape joue pour ce lanceur : celles que seul le joueur local
// sème se taisent quand un autre les lance.
[[nodiscard]] constexpr bool SkillSuiteStepPlaysForCaster(const SkillSuiteStep& step,
                                                          bool caster_is_local_player) {
  return !step.cast_by_local_player_only || caster_is_local_player;
}

// LE POINT QUE VISE L'INSTANCE D'UNE ÉTAPE, dans notre repère. `other_end` est
// l'autre bout, `target` et `caster` les places au sol des deux acteurs (vide
// quand l'acteur est introuvable). `generator` tire l'angle d'un anneau.
[[nodiscard]] std::optional<glm::vec3> SkillSuiteAimPoint(const SkillSuiteStep& step,
                                                          std::optional<glm::vec3> other_end,
                                                          glm::vec3 target,
                                                          std::optional<glm::vec3> caster,
                                                          std::uint32_t& generator);

// UN POINT ET UN DÉLAI TIRÉS pour une étape dispersée : l'écart au point de
// naissance, dans notre repère, et le délai en millisecondes. Nul pour une
// étape qui ne se disperse pas.
struct SkillSuiteScatter {
  glm::vec3 offset{0.0F, 0.0F, 0.0F};
  std::uint32_t delay_ms = 0;
};
[[nodiscard]] SkillSuiteScatter SkillSuiteDrawScatter(const SkillSuiteStep& step,
                                                      std::uint32_t& generator);

}  // namespace ro::render
