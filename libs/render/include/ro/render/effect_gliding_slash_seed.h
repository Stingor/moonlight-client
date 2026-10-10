#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA TAILLADE QUI GLISSE, sous-types 86 et 92.
//
// DEUX bâtisseurs — l'un pour le 86, l'autre pour le 92 — et ils écrivent les
// mêmes champs. Ils ne diffèrent que par trois choses : les images, la
// façon de retarder leurs quatre pistes, et ce qu'ils mettent dans l'azimut.
//
// ⚠ UN OCTET SUR TROIS CHARGE QUATRE IMAGES, LES AUTRES UNE SEULE. Le premier
// bâtisseur compare son octet à UN : dans ce cas il charge les quatre
// `sonic_wave`, sinon la seule `purpleslash`. Additionner les deux branches en
// donnerait CINQ, et l'index d'image aurait une case que le client n'a pas.

// LES IMAGES, par branche.
inline constexpr std::array<std::string_view, 4> kGlidingSlashWaveTextures = {
    "data\\texture\\effect\\sonic_wave1.tga", "data\\texture\\effect\\sonic_wave2.tga",
    "data\\texture\\effect\\sonic_wave3.tga", "data\\texture\\effect\\sonic_wave4.tga"};
inline constexpr std::string_view kGlidingSlashPlainTexture =
    "data\\texture\\effect\\purpleslash.tga";

// LEQUEL DES DEUX BÂTISSEURS un site appelle.
enum class EffectGlidingSlashBuilder : std::uint8_t {
  // Sous-type 86 : il VISE, et son octet choisit les images.
  kSonicWave,
  // Sous-type 92 : il reçoit son azimut du site et ne vise rien.
  kPurpleSlash,
};

// CE QUE LES DEUX POSENT EN DUR.
//
// ⚠ LE CARRÉ NAÎT À VINGT D'UN CÔTÉ ET NE CHANGE JAMAIS DE TAILLE : aucun des
// deux mouvements ne touche au rayon. Le chercher dans le mouvement ferait
// croire à un semis incomplet.
inline constexpr float kGlidingSlashRadius = 20.0F;
inline constexpr float kGlidingSlashSpeed = 0.8F;

// DE COMBIEN LES PISTES NAISSENT AU-DESSUS DU SOL, et de combien elles se
// dispersent.
//
// ⚠ LE CLIENT RETRANCHE DIX, ET SON REPÈRE DESCEND : les quatre carrés
// naissent DIX unités plus HAUT que le point de l'effet, moins un tirage de
// cinq. Recopier le signe les enterrerait.
inline constexpr float kGlidingSlashBirthHeight = 10.0F;
inline constexpr std::int32_t kGlidingSlashHeightSpread = 5;

// LE RETARD DES QUATRE PISTES.
//
// ⚠ VINGT-HUIT TICS AVANT DE PARAÎTRE, et un tic d'écart entre les quatre.
// C'est long, et c'est voulu : la taillade arrive après le coup. Le semer à
// zéro la montrerait une seconde trop tôt.
//
// ⚠ ET LE SECOND BÂTISSEUR TIRE SON RETARD EN PLUS, une fois pour les quatre :
// deux taillades lancées ensemble ne partent donc pas au même tic.
inline constexpr std::int32_t kGlidingSlashDelayTicks = 28;
inline constexpr std::int32_t kGlidingSlashExtraDelaySpread = 6;

// UNE TAILLADE qu'un effet pose.
struct EffectGlidingSlashPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  EffectGlidingSlashBuilder builder = EffectGlidingSlashBuilder::kSonicWave;
  // L'OCTET DU SITE pour le premier bâtisseur : il choisit les images, la
  // teinte ET la dispersion du cap. Le second n'en a pas.
  std::uint8_t mark = 0;
  // L'AZIMUT QUE LE SITE DONNE au second bâtisseur. Le premier le met à zéro
  // et le mouvement le remplace au premier tic.
  std::int16_t azimuth_degrees = 0;
  // 🔴 COMBIEN DE NŒUDS LE SITE SÈME, et de combien l'azimut avance d'un nœud au
  // suivant. Le site de la taillade violette boucle HUIT fois et passe son
  // compteur — zéro, quarante-cinq, …, trois cent quinze — comme azimut : huit
  // taillades en étoile, et non une seule vers l'est. ⚠ EN QUEUE.
  std::uint16_t copies = 1;
  std::int16_t azimuth_step_per_copy_degrees = 0;
};

[[nodiscard]] std::span<const EffectGlidingSlashPattern> EffectGlidingSlashPatternTable();

[[nodiscard]] std::span<const EffectGlidingSlashPattern> EffectGlidingSlashPatternsFor(
    std::uint32_t effect_id);

// LES IMAGES qu'un patron charge, dans l'ordre où son bâtisseur les pose.
[[nodiscard]] std::vector<std::string_view> EffectGlidingSlashTexturesOf(
    const EffectGlidingSlashPattern& pattern);

// LES QUATRE PISTES d'un nœud, toutes vives et toutes retardées.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGlidingSlashTracks(
    const EffectGlidingSlashPattern& pattern, glm::vec3 anchor, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGlidingSlashNode(const EffectGlidingSlashPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
