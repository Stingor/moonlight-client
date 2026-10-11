#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace ro::render {

// CE QUE LE LECTEUR DE `.str` FAIT DE PLUS POUR CERTAINS IDENTIFIANTS, à leur
// naissance : le mot de rendu de son image, sa boucle, sa vie, son échelle, les
// tremblements qu'il met en file. Un identifiant absent n'a rien de plus que
// son fichier, joué une fois. Ses bruits, eux, sont dans la table des sons
// d'effet, rangés sous le même identifiant.

// LE TREMBLEMENT QU'UN EFFET MET EN FILE : la commande 122 au mode de jeu, sans
// argument — la durée ordinaire, en millisecondes.
inline constexpr std::uint32_t kEffectQueuedQuakeMs = 650;

// LE MOT DE RENDU que la création de l'image lui donne : translucide, au
// mélange propre de chaque couche. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kStrRenderWordOfCreation = 5;

// Une plage de niveaux et l'échelle qu'elle donne, bornes comprises.
struct StrLevelScale {
  std::int32_t level_min = 0;
  std::int32_t level_max = 0;
  float scale = 1.0F;
};

struct StrEffectSetup {
  std::uint32_t effect_id = 0;
  // LE MOT DE RENDU DE L'IMAGE, passé tel quel à la file de rendu. Deux bits
  // comptent pour un `.str`, dont les couches gardent toujours leur mélange :
  // 0x8 le peint dans la file du sol, avant les corps — une fissure, un
  // cratère —, 0x200 par-dessus tout, en dernier. Voir `StrPaintsBehindActors`.
  std::uint32_t render_word = kStrRenderWordOfCreation;
  // LE FICHIER REBOUCLE au lieu de s'arrêter à sa dernière image.
  bool loops = false;
  // UNE SEULE INSTANCE PAR PORTEUR : le redemander sur le même porteur rend
  // celle qui joue déjà, sans rien recommencer — ni fichier, ni bruit.
  bool unique_on_owner = false;
  // IL VIT TANT QUE SON PORTEUR EST LÀ. Avec `loops`, il reboucle jusqu'à ce que
  // le porteur s'en aille ; sans, il joue son fichier une fois comme les autres.
  bool lives_with_owner = false;
  // L'échelle fixe de l'image, et celle qui dépend du niveau reçu à la
  // naissance — hors de toute plage, aucune.
  float scale = 1.0F;
  std::span<const StrLevelScale> level_scales;
  // LES TICS DE L'INSTANCE où l'écran tremble, de la durée ordinaire.
  std::span<const std::uint16_t> quake_ticks;
  // COMBIEN DE FOIS LE FICHIER SE REJOUE à sa fin, quand il ne boucle pas.
  std::uint8_t replays = 0;
  // SA ROTATION À L'ÉCRAN SE CALCULE À LA NAISSANCE, vers le point qu'il vise.
  bool turns_toward_target = false;
  // SA VIE EST BORNÉE PAR LA TABLE DES DURÉES d'effet, en plus de son fichier.
  bool life_from_duration_table = false;
};

// Ce que le lecteur fait de plus pour cet identifiant, ou rien.
[[nodiscard]] const StrEffectSetup* StrEffectSetupFor(std::uint32_t effect_id);

// La table entière, triée par identifiant. Engendrée.
[[nodiscard]] std::span<const StrEffectSetup> StrEffectSetupTable();

// L'échelle de cet identifiant au niveau reçu : la fixe, fois celle de la plage
// qui contient le niveau.
[[nodiscard]] float StrEffectLevelScale(const StrEffectSetup& setup, std::int32_t level);

// Vrai quand l'image se peint dans la file du sol, avant les corps.
[[nodiscard]] bool StrPaintsBehindActors(const StrEffectSetup& setup);

// Vrai quand l'image se peint par-dessus tout, après tous les autres effets.
[[nodiscard]] bool StrPaintsOverEverything(const StrEffectSetup& setup);

// VRAI QUAND LE FICHIER MEURT AVEC L'ACTEUR QUI LE PORTE, au tic où celui-ci
// quitte le monde — mort ou sorti de la vue. C'est le cas d'un effet PRIMITIF,
// que l'acteur emporte en disparaissant, et du lecteur de `.str` quand il vit
// avec son porteur (`lives_with_owner`). Un autre lecteur de `.str` finit sa
// vie à la dernière place de son porteur.
[[nodiscard]] bool StrDiesWithBearer(std::uint32_t effect_id);

// ⚠ UN ÉCART VOULU AU CLIENT D'ORIGINE, à la demande du joueur : LA RÉTICULE DE
// BULL'S EYE.
//
// Le client relève l'ancre d'un `.str` de quatre-vingts pixels d'ÉCRAN, que le
// zoom ne grandit pas ; ce client-ci la relève de quatre-vingts pixels de
// sprite, qui grandissent avec lui, et le joueur garde ce fonctionnement. Au
// zoom ordinaire l'ancre se tient alors plus haut que chez le client, et la
// réticule de Bull's Eye (649) passait au-dessus de la tête au lieu de tomber
// dessus. Cinquante-huit pixels l'y posent, mesurés sur une capture côte à côte.
inline constexpr std::uint32_t kBullseyeReticleEffect = 649;
inline constexpr float kBullseyeReticleLiftPixels = 58.0F;

// Le relèvement propre à un effet, ou rien pour celui de tous.
[[nodiscard]] constexpr std::optional<float> StrAnchorLiftFor(std::uint32_t effect_id) {
  if (effect_id == kBullseyeReticleEffect) {
    return kBullseyeReticleLiftPixels;
  }
  return std::nullopt;
}

}  // namespace ro::render
