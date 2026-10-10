#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {

// ── CE QU'UN COUP PORTÉ FAIT JOUER À QUI LE PORTE ────────────────────────────
//
// Un coup pose un ÉTAT D'ACTION sur son auteur : l'attaque pour un coup
// ordinaire, l'état d'emploi de la compétence pour un coup de compétence — la
// même table que l'emploi —, ou rien pour une liste de compétences qui gardent
// la motion en cours. Cet état choisit le rang joué, une passe, et laisse sur
// l'acteur une IMAGE DE FRAPPE : c'est elle, multipliée par la durée d'une
// image de ce rang, qui date le départ du projectile, la blessure de la cible
// et ses impacts.
//
//   · l'attaque (états deux et neuf) joue le rang d'attaque ; l'image de frappe
//     est celle de la table d'un joueur — reculée du vol de sa flèche et de son
//     âme — ou celle du marqueur du coup dans le .act d'une créature ;
//   · l'emploi ordinaire (état sept) joue le rang que l'état donne au métier —
//     l'incantation pour la plupart, la garde pour un barde, l'attaque pour un
//     Doram —, et pose l'image SIX. Assis ou terré, il ne touche à rien ;
//   · le repos (état zéro) et les autres états qui posent une action remettent
//     l'image de frappe : le coup touche AUSSITÔT ;
//   · les poses menées (états seize et au-delà) et les compétences qui gardent
//     la motion laissent l'image d'avant — celle de l'attaque qui les a
//     déclenchées, pour un enchaînement.
//
// Une CRÉATURE n'a que l'attaque : tout autre état la laisse telle qu'elle est,
// sauf l'homoncule et l'élémentaire, dont l'emploi joue le rang cinq ; le coup
// touche alors aussitôt. Le geste que sa table de gestes nomme remplace ensuite
// le rang affiché, sans changer l'instant du coup.

// LA MARQUE D'UNE IMAGE DE FRAPPE REMISE par une action : le coup touche
// aussitôt.
inline constexpr float kResetHitFrame = -1.0F;

// Les faits d'un coup, tels que le monde les tient. Rien de `sim` ici : c'est
// l'appelant qui les recopie.
struct StrikeFacts {
  // Zéro pour un coup ordinaire.
  std::uint16_t skill_id = 0;
  // La sorte du coup, telle que le serveur la numérote.
  std::uint8_t damage_type = 0;
  // La durée que le serveur donne à l'attaque ; zéro quand il ne dit rien.
  std::uint32_t attack_motion_ms = 0;
  // Le dixième tiré pour le taekwon et l'homoncule, de zéro à neuf.
  std::uint8_t chance_tenth = 0;
  // Vrai quand l'auteur tire une âme : l'état qui la donne est posé.
  bool shoots_souls = false;
  // Vrai quand l'auteur est assis ou terré : l'emploi ordinaire ne touche
  // alors à rien.
  bool keeps_posture = false;
  // Vrai pour un homoncule ou un élémentaire : leur emploi joue le rang cinq.
  bool skill_led_familiar = false;
  // L'ACTION QUE L'AUTEUR JOUE DÉJÀ — son image de frappe et la durée d'une de
  // ses images —, que lit seul un coup d'éclaboussure : voir
  // `SplashHitKeepsAction`. L'image remise quand il ne joue rien.
  float ongoing_hit_frame = kResetHitFrame;
  float ongoing_frame_interval_ms = 0.0F;
};

// Les états d'action qu'un coup pose.
inline constexpr std::int8_t kStrikeAttackState = 2;
inline constexpr std::int8_t kStrikeAttackVariantState = 9;
inline constexpr std::int8_t kStrikeUseState = 7;
inline constexpr std::int8_t kStrikeRestState = 0;
// LES ÉTATS QUI POSENT UNE ACTION, et remettent donc l'image de frappe : ceux
// de 0 à 9, et 11, 12, 13, 45, 52 à 54, 58, 59. Les autres — ceux qui mènent
// une pose, et ceux que le client laisse tels quels — gardent l'image d'avant.
[[nodiscard]] bool StatePosesAction(std::int8_t action_state) noexcept;

// L'image de frappe que l'emploi ordinaire pose.
inline constexpr float kUseStateHitFrame = 6.0F;

struct StrikePlay {
  // L'état posé, ou rien quand le coup laisse la motion.
  std::optional<std::int8_t> state;
  // Vrai quand l'auteur montre une motion pour ce coup : le rang ci-dessous,
  // depuis sa première image, une passe de `duration_ms`.
  bool plays_motion = false;
  // Vrai quand cette motion est l'ATTAQUE : un joueur n'y bronche pas.
  bool attack = false;
  std::size_t animation = 0;
  // Le facteur de cadence du coup, qui multiplie la durée de chaque image.
  float factor = 1.0F;
  float duration_ms = 0.0F;
  // L'IMAGE DE FRAPPE que le coup laisse sur l'auteur, le vol de sa flèche
  // compris ; `kResetHitFrame` quand l'action l'a remise.
  float hit_frame = kResetHitFrame;
  // L'instant, depuis le coup, où il TOUCHE : la cible bronche, ses dégâts
  // paraissent. Le rattrapage par espèce d'une créature s'y ajoute.
  float lands_at_ms = 0.0F;
};

// L'IMAGE DE FRAPPE D'UN RANG D'ATTAQUE : la table d'un joueur, le marqueur du
// coup d'une créature.
[[nodiscard]] float StrikeFrameOf(const ActorLook& look, const ActorOutfit& outfit,
                                  std::size_t attack_animation);

// LE GESTE D'UN COUP DE COMPÉTENCE, étiré pour durer au moins `floor_ms` à
// l'écran — un réglage du joueur, zéro par défaut. La vitesse d'attaque que le
// serveur donne l'accélère, et un sort instantané sous une forte ASPD passait
// trop vite pour être vu. Seules la cadence et la durée MONTRÉES changent ; ni
// une attaque ordinaire ni l'instant où le coup porte n'en dépendent.
void FloorSkillGesture(StrikePlay& play, float floor_ms) noexcept;

// Ce que ce coup fait jouer à cet auteur, habillé de cette tenue.
//
// 🔴 UN COUP D'ÉCLABOUSSURE NE JOUE RIEN : il touche à l'image de frappe de
// l'action en cours, que les faits portent — et que ne rend pas sa propre
// cadence. Une action qui l'a remise le fait toucher aussitôt.
[[nodiscard]] StrikePlay StrikePlayOf(const SpriteCatalog& catalog, const ActorOutfit& outfit,
                                      const ActorLook& look, const StrikeFacts& facts);

// CE QUE L'EMPLOI D'UNE COMPÉTENCE FAIT JOUER : le rang que son état pose,
// une passe à la cadence du fichier — personne ne règle le facteur sur ce
// chemin —, et l'image de frappe que l'état laisse. L'attaque et sa variante
// prennent celle de la table d'un joueur, la flèche comprise pour la
// première ; l'emploi ordinaire l'image six ; une créature n'a que l'attaque,
// à son marqueur. Tout autre état la remet.
[[nodiscard]] StrikePlay UsePlayOf(const SpriteCatalog& catalog, const ActorOutfit& outfit,
                                   const ActorLook& look, std::int8_t action_state,
                                   std::uint8_t chance_tenth, bool shoots_souls);

// ── LE RANG QU'UN ÉTAT POSÉ DONNE À UN JOUEUR ───────────────────────────────
//
// 🔴 L'ATTAQUE ET SA VARIANTE NE SE LISENT PAS DANS LA TABLE DES ÉTATS. Quand
// l'incantation, l'emploi, le coup ou la reprise d'un enchaînement posent
// l'état deux ou l'état neuf, le client les résout par l'ARME tenue : l'état
// deux tire au sort le rang d'un taekwon et prend sinon celui que l'arme
// décide, l'état neuf prend ce rang sans tirer. La table, elle, donne toujours
// le rang dix à l'un et le rang onze à l'autre.
//
// ⚠ C'EST L'ARME QUI DISPARAÎT QUAND ON SE TROMPE : un fichier d'arme n'a
// d'images qu'aux rangs de ses propres attaques. Le katar d'un assassin n'en a
// qu'au rang onze ; jouer le dix de la table le laisse les mains vides.
//
// Tout autre état prend le rang de la table ; une créature, rien.
[[nodiscard]] std::optional<std::size_t> PosedActionAnimation(const SpriteCatalog& catalog,
                                                              const ActorLook& look,
                                                              std::int8_t action_state,
                                                              std::uint8_t chance_tenth);

// Le dixième que le tirage d'un taekwon donne à cet instant, de zéro à neuf.
inline constexpr std::uint32_t kChanceTenthMs = 100;
inline constexpr std::uint32_t kChanceTenths = 10;
[[nodiscard]] constexpr std::uint8_t ChanceTenthAt(std::uint32_t at_ms) {
  return static_cast<std::uint8_t>((at_ms / kChanceTenthMs) % kChanceTenths);
}

}  // namespace ro::render
