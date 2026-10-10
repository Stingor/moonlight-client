#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/render/effect_frame.h"

namespace ro::render {

// CE QUE LE CLIENT POSE QUAND UNE COMPÉTENCE TOMBE SUR UNE CASE.
//
// Le serveur l'annonce par ZC_NOTIFY_GROUNDSKILL, et le client en fait l'image
// par un répartiteur indexé sur la compétence, qui choisit l'un de trois gestes.
enum class GroundSkillVisualBearer : std::uint8_t {
  // Un LANCEUR sans corps posé sur la case, qui sème l'effet et le porte : à sa
  // mort, l'effet perd son porteur et meurt avec lui, nœuds compris.
  kLauncher,
  // L'effet posé sur la case, sans porteur.
  kCell,
  // L'effet posé SUR LE LANCEUR du sort, qui le porte.
  kCaster,
};

// Sous quelle bascule d'effets minimaux — `/mineffect` — une entrée se joue.
enum class GroundSkillVisualSetting : std::uint8_t {
  kAnySetting,
  // Bascule baissée, le réglage par défaut : les effets pleins.
  kFullEffects,
  // Bascule levée : les effets allégés qui les remplacent.
  kMinimalEffects,
};

// Vrai quand une entrée de ce réglage se joue sous cette bascule.
[[nodiscard]] constexpr bool GroundSkillSettingPlays(GroundSkillVisualSetting setting,
                                                     bool minimal_effects) noexcept {
  return setting == GroundSkillVisualSetting::kAnySetting ||
         (setting == GroundSkillVisualSetting::kMinimalEffects) == minimal_effects;
}

// Une entrée du répartiteur, pour une compétence, une bascule et une plage de
// niveaux.
struct GroundSkillVisualEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  GroundSkillVisualBearer bearer = GroundSkillVisualBearer::kCell;
  GroundSkillVisualSetting setting = GroundSkillVisualSetting::kAnySetting;
  // La plage de niveaux, bornes comprises.
  std::int32_t min_level = 0;
  std::int32_t max_level = 0;
  // LA VIE DU LANCEUR, en millisecondes : un pas de vingt-quatre par unité de
  // vie, et le lanceur meurt au premier contrôle qui la dépasse. Zéro hors du
  // geste du lanceur.
  //
  // ⚠ ELLE NE COUPE L'EFFET QUE S'IL LE PORTE : un effet que le gestionnaire
  // confie à une classe naît à part, sur la case, et vit sa propre vie. Voir
  // `GroundSkillLauncherCarries`.
  std::int32_t launcher_life_ms = 0;
  // Ce que le répartiteur ajoute à la HAUTEUR de la case, dans le repère du
  // client — l'axe vertical y descend. Zéro pour presque toutes.
  float client_height_offset = 0.0F;
  // VRAI QUAND L'EFFET VISE LA CASE : le répartiteur donne la case pour point
  // visé à l'instance qu'il vient de poser sur le lanceur. Une traînée qui vole
  // du lanceur vers ce point ne sème rien sans lui.
  bool aimed_at_cell = false;
  // LE DÉLAI DE DÉPART DE SON FICHIER, en millisecondes : le fichier ne montre
  // rien tant qu'il court, puis commence à sa première image.
  std::int32_t str_start_delay_ms = 0;
  // LA PART DE SON FICHIER QU'IL SAUTE À LA NAISSANCE : autant d'images que la
  // dernière image multipliée par cette part, tronquée, passent d'un coup.
  float str_skipped_share = 0.0F;
};

// UN SON QUE LE RÉPARTITEUR JOUE LUI-MÊME, à côté des effets qu'il pose.
//
// ⚠ À LA POSITION QU'IL POUSSE, et ce n'est pas la case : il écrit en dur un
// point du repère du client — centré sur la carte, l'axe vertical descendant —,
// l'origine le plus souvent. Le son s'entend donc depuis le centre de la carte.
struct GroundSkillSound {
  std::uint16_t skill_id = 0;
  GroundSkillVisualSetting setting = GroundSkillVisualSetting::kAnySetting;
  std::int32_t min_level = 0;
  std::int32_t max_level = 0;
  std::string_view file;
  std::array<float, 3> client_position{0.0F, 0.0F, 0.0F};
  // La portée, la proximité en deçà de laquelle il sonne plein, et son volume.
  float hearing_range = 0.0F;
  float near_distance = 0.0F;
  float volume = 0.0F;
};

// UNE POSE QUE LE RÉPARTITEUR MÈNE SUR LE LANCEUR : les images [first_frame,
// last_frame] du rang `play_rank`, puis l'image `hold_frame` du rang
// `hold_rank`, minutée à `hold_ms`.
struct GroundSkillPose {
  std::uint16_t skill_id = 0;
  std::int32_t min_level = 0;
  std::int32_t max_level = 0;
  std::uint8_t play_rank = 0;
  std::uint8_t first_frame = 0;
  std::uint8_t last_frame = 0;
  std::uint8_t hold_rank = 0;
  std::uint8_t hold_frame = 0;
  std::uint16_t hold_ms = 0;
};

// ── CE QUE LE GESTIONNAIRE DU PAQUET POSE LUI-MÊME ──────────────────────────
//
// Après avoir envoyé la compétence au répartiteur ci-dessus, le gestionnaire de
// ZC_NOTIFY_GROUNDSKILL fait encore ses propres gestes pour une vingtaine de
// compétences, puis passe au registre des actions — les effets nommés au sol —,
// sauf quand il en sort plus tôt.

// Qui a lancé : une CRÉATURE ou un PNJ (un métier affiché de créature, voir
// `IsMonsterOrNpcClass`) ne suit pas le même chemin qu'un joueur — ni bannière,
// ni registre, et deux compétences seulement.
enum class GroundSkillPacketCaster : std::uint8_t {
  kPlayer,
  kCreature,
  // UN LANCEUR HORS DE VUE : le chemin du joueur, puisqu'aucun métier ne dit le
  // contraire, mais sans le répartiteur — le message ne part qu'à un acteur
  // connu — ni rien de ce qui naîtrait sur lui. Voir `PlanGroundSkillPacket`.
  // Aucune étape de la table ne le nomme.
  kUnseen,
};

enum class GroundSkillPacketGesture : std::uint8_t {
  // Un son, à la position du repère du client qu'il pousse.
  kSound,
  // Un effet posé sur le lanceur, qui le porte.
  kOnCaster,
  // Un effet posé sur la case, sans porteur — ou autour d'elle, à un rayon tiré.
  kOnCell,
  // Un lanceur sans corps posé sur la case : voir `GroundSkillVisualBearer::kLauncher`.
  kLauncher,
  // Le tremblement d'écran, pendant `quake_ms`.
  kScreenQuake,
};

// UNE ÉTAPE DU CHEMIN DE LA CASE, dans l'ordre où le client la joue.
//
// ⚠ UN CHAMP AJOUTÉ VA EN QUEUE, et la table engendrée nomme chaque champ.
struct GroundSkillPacketStep {
  std::uint16_t skill_id = 0;
  GroundSkillPacketCaster caster = GroundSkillPacketCaster::kPlayer;
  GroundSkillVisualSetting setting = GroundSkillVisualSetting::kAnySetting;
  std::int32_t min_level = 0;
  std::int32_t max_level = 0;
  GroundSkillPacketGesture gesture = GroundSkillPacketGesture::kOnCell;
  std::uint16_t effect_id = 0;
  // Voir `GroundSkillVisualEntry` : la vie du lanceur, la hauteur ajoutée à la
  // case, le point visé, le délai et la part sautée du fichier.
  std::int32_t launcher_life_ms = 0;
  float client_height_offset = 0.0F;
  bool aimed_at_cell = false;
  std::int32_t str_start_delay_ms = 0;
  float str_skipped_share = 0.0F;
  // L'ÉCHELLE DU FICHIER à l'écran, autour de son point projeté.
  float str_scale = 1.0F;
  // AUTOUR DE LA CASE : un rayon de `rand() % ring_draw_modulo +
  // ring_draw_base`, le long de `client_ring_direction` — x puis z, dans le
  // repère du client. Les étapes de même `ring_draw_rank` partagent leur
  // tirage. Sur la case même quand le modulo est nul.
  std::int32_t ring_draw_modulo = 0;
  float ring_draw_base = 0.0F;
  std::array<float, 2> client_ring_direction{0.0F, 0.0F};
  std::uint8_t ring_draw_rank = 0;
  // LE SECTEUR DE L'ANGLE hors duquel l'étape ne se joue pas : le lacet de la
  // caméra plus le regard du lanceur, en degrés, tels que le client les tient —
  // voir `GroundSkillViewAngleDegrees`. Tout angle quand `by_view_angle` est faux.
  bool by_view_angle = false;
  float view_angle_min_degrees = 0.0F;
  bool view_angle_min_inclusive = true;
  float view_angle_max_degrees = 0.0F;
  bool view_angle_max_inclusive = false;
  // Le son : voir `GroundSkillSound`.
  std::string_view sound;
  std::array<float, 3> sound_client_position{0.0F, 0.0F, 0.0F};
  float hearing_range = 0.0F;
  float near_distance = 0.0F;
  float volume = 0.0F;
  std::uint32_t quake_ms = 0;
};

[[nodiscard]] std::span<const GroundSkillPacketStep> GroundSkillPacketStepTable();
// Les compétences dont le gestionnaire sort, pour un joueur, avant le registre.
[[nodiscard]] std::span<const std::uint16_t> GroundSkillPacketRegistrySkipTable();

// LES EFFETS SUR LE LANCEUR DONT LE GESTIONNAIRE ATTEND LA NAISSANCE : il les
// vise ensuite vers la case, et sort sans rien faire d'autre quand ils ne
// naissent pas — ce qui arrive faute de lanceur. 2417 perd alors son
// tremblement et son son avec son 863 ; 2422 a déjà sonné avant son 858.
inline constexpr std::array<std::uint16_t, 2> kCasterEffectsTheHandlerWaitsFor = {858, 863};

// L'ANGLE QUE LE CLIENT DÉCOUPE pour choisir une étape : le lacet de sa caméra
// — l'opposé du nôtre, ramené dans le tour — plus le regard du lanceur, sans
// autre réduction que la sienne, faite par les secteurs.
[[nodiscard]] float GroundSkillViewAngleDegrees(float camera_yaw_degrees,
                                                float client_body_degrees) noexcept;

// Vrai quand l'étape se joue sous cet angle.
[[nodiscard]] bool GroundSkillStepFitsViewAngle(const GroundSkillPacketStep& step,
                                                float view_angle_degrees) noexcept;

// Vrai quand l'étape pose un lanceur qui porte son effet, et le coupe donc en
// mourant — voir `GroundSkillLauncherCarries`.
[[nodiscard]] bool GroundSkillStepLauncherCarries(const GroundSkillPacketStep& step) noexcept;

// L'étape d'une entrée du répartiteur, et celle d'un de ses sons.
[[nodiscard]] GroundSkillPacketStep GroundSkillStepOf(const GroundSkillVisualEntry& entry);
[[nodiscard]] GroundSkillPacketStep GroundSkillStepOf(const GroundSkillSound& sound);

// Les tables que lit le plan : les engendrées par défaut, d'autres pour
// l'essayer sur une compétence qu'aucune ne porte encore.
struct GroundSkillPacketTables {
  std::span<const GroundSkillVisualEntry> dispatcher;
  std::span<const GroundSkillSound> dispatcher_sounds;
  std::span<const GroundSkillPacketStep> handler;
  std::span<const std::uint16_t> registry_skips;
};
[[nodiscard]] GroundSkillPacketTables GeneratedGroundSkillPacketTables();

// CE QUE LE CHEMIN DE LA CASE JOUE, dans l'ordre du client : les sons puis les
// effets du répartiteur (le message au lanceur part en premier), puis les
// étapes du gestionnaire, puis — `reaches_registry` — les effets nommés au sol.
//
// ⚠ LE RÉPARTITEUR NE CÈDE PAS AUX EFFETS NOMMÉS : le client lui envoie la
// compétence avant même d'arriver au registre.
//
// ⚠ L'EFFET DES TABLES FONDUES n'est qu'un REPLI, posé sur la case quand ni le
// répartiteur, ni le gestionnaire, ni le registre ne connaissent la compétence
// (`has_named_effects` dit si ce dernier la connaît).
//
// 🔴 SANS LANCEUR (`kUnseen`), LA CASE SE MONTRE QUAND MÊME : ni répartiteur,
// ni repli, mais les étapes du joueur sur la case — sons, effets, lanceurs sans
// corps — et le registre. Ce qui naîtrait sur le lanceur ne naît pas, les
// étapes qui lisent son regard non plus ; et quand le client attend un effet
// sur lui pour continuer — voir `kCasterEffectsTheHandlerWaitsFor` —, le reste
// de la compétence s'éteint avec lui.
struct GroundSkillPacketPlan {
  std::vector<GroundSkillPacketStep> steps;
  bool reaches_registry = true;
};
[[nodiscard]] GroundSkillPacketPlan PlanGroundSkillPacket(
    const GroundSkillPacketTables& tables, std::uint32_t skill_id, std::uint16_t table_effect_id,
    std::int32_t skill_level, bool minimal_effects, GroundSkillPacketCaster caster,
    bool has_named_effects);

// La table entière, triée par compétence.
[[nodiscard]] std::span<const GroundSkillVisualEntry> GroundSkillVisualTable();
[[nodiscard]] std::span<const GroundSkillSound> GroundSkillSoundTable();
[[nodiscard]] std::span<const GroundSkillPose> GroundSkillPoseTable();
// Les compétences qui envoient le faucon du lanceur vers la case, triées.
[[nodiscard]] std::span<const std::uint16_t> GroundSkillFalconFlightTable();

// Les sons que le répartiteur joue pour une compétence, à un niveau et sous une
// bascule.
[[nodiscard]] std::vector<GroundSkillSound> GroundSkillSoundsToPlay(std::uint32_t skill_id,
                                                                    std::int32_t skill_level,
                                                                    bool minimal_effects);

// L'ACTION DONT UNE COMPÉTENCE DE CASE QUI LAISSE LA MOTION impose la dernière
// image : le rang onze du corps, vu de la direction zéro, quelle que soit la
// direction où l'acteur regarde. L'image se borne ensuite à ce que porte
// l'action qu'il joue.
inline constexpr std::size_t kGroundSkillHeldImageAnimation = 11;
inline constexpr std::size_t kGroundSkillHeldImageDirection = 0;

// La pose que le répartiteur mène pour une compétence à un niveau, ou rien.
[[nodiscard]] const GroundSkillPose* GroundSkillPoseOf(std::uint32_t skill_id,
                                                       std::int32_t skill_level);

// Vrai quand la compétence envoie le faucon de son lanceur vers la case.
[[nodiscard]] bool GroundSkillSendsFalcon(std::uint32_t skill_id);

// LE NOMBRE D'IMAGES QU'UN FICHIER SAUTE, pour sa dernière image et la part
// donnée : le produit en flottant, tronqué.
[[nodiscard]] std::int32_t StrSkippedFrames(std::int32_t last_frame, float share) noexcept;

// Les entrées d'une compétence, vides quand le répartiteur ne la connaît pas.
[[nodiscard]] std::span<const GroundSkillVisualEntry> GroundSkillVisualsOf(std::uint32_t skill_id);

// Vrai quand le lanceur porte l'effet, et donc le coupe en mourant.
[[nodiscard]] bool GroundSkillLauncherCarries(const GroundSkillVisualEntry& entry) noexcept;

// LE TIC DE L'EFFET OÙ SON PORTEUR N'EST PLUS, pour un porteur qui meurt à
// `bearer_life_ms` : le premier dont l'heure la dépasse — le porteur vit tant
// que son âge ne l'excède pas. Zéro pour zéro, qui ne coupe rien.
[[nodiscard]] constexpr std::int32_t BearerDeathTick(std::uint32_t bearer_life_ms) noexcept {
  return bearer_life_ms == 0 ? 0
                             : static_cast<std::int32_t>(
                                   bearer_life_ms / static_cast<std::uint32_t>(kEffectTickMs)) +
                                   1;
}

// CE QUE LE CHEMIN DE LA CASE POSE, pour une compétence à un niveau.
//
// Quand le répartiteur connaît la compétence, les entrées de la variante que
// la bascule `/mineffect` et le niveau choisissent — toutes : il en pose
// parfois plusieurs d'un coup. Quand il ne la connaît pas, `table_effect_id`,
// l'effet que les tables fondues lui donnent, revient seul, posé sur la case ;
// rien quand il vaut zéro.
[[nodiscard]] std::vector<GroundSkillVisualEntry> GroundSkillVisualsToPlay(
    std::uint32_t skill_id, std::uint16_t table_effect_id, std::int32_t skill_level,
    bool minimal_effects);

}  // namespace ro::render
