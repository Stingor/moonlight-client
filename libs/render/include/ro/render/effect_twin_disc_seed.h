#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES DEUX DISQUES : le semis et le mouvement du sous-type 28.
//
// CINQ identifiants — 201, 398, 650, 680 et 881 —, DEUX bâtisseurs qui sont le
// même code à trois nombres près : l'un prend un octet de site, l'autre non.
//
// ⚠ SA FIGURE EST DÉJÀ PORTÉE : c'est le carré qui respire du sous-type 39,
// `ComposeBreathingQuad`. Ce qui lui est propre n'est pas un dessinateur mais
// un MOUVEMENT.
//
// ⚠ IL Y A BIEN DEUX DISQUES. Le bâtisseur
// n'allume que les pistes ZÉRO et UN, et les pose à cent quatre-vingts degrés
// d'inclinaison l'une de l'autre — deux carrés qui se font face.
//
// ⚠ LE PREMIER BÂTISSEUR EST CELUI DU SOUS-TYPE 161 à trois écritures près :
// même switch sur l'octet, mêmes styles,
// même rayon. Voir `effect_fading_twin_disc_seed.h`.
//
// CE QUE LES BÂTISSEURS ÉCRIVENT ET QUE CE PORTAGE NE PORTE PAS, sciemment :
//   · la remise à zéro de l'âge à 99999998 (vie reposée à 99999999, premier
//     bâtisseur seulement) : l'effet perpétuel
//     rebâtit un couple quand son âge atteint ce nombre de tics, soit plus de
//     dix-huit jours de jeu à seize millisecondes le tic. Le nœud porté vit la
//     vie de l'effet ;
//   · le compteur d'images du mouvement : il boucle sur le nombre d'images du
//     nœud, que le constructeur pose à un et que ces bâtisseurs ne touchent
//     pas. Une seule image, un seul rang : sans effet.

// COMBIEN DE PISTES VIVENT : les deux premières allumées, les deux dernières
// éteintes. L'inclinaison de la seconde vaut 180 — la première part de zéro.
inline constexpr std::size_t kTwinDiscLiveTracks = 2;
inline constexpr std::int16_t kTwinDiscSecondTilt = 180;

// LA HAUTEUR DES DEUX DISQUES, que la figure lit dans la première case.
//
// ⚠ MOINS UN, ET LE REPÈRE DU CLIENT DESCEND : le disque flotte un rien
// AU-DESSUS de son nœud. Recopier le signe au lieu du sens l'enfoncerait.
inline constexpr float kTwinDiscHeight = -1.0F;

// UN COUPLE DE DISQUES qu'un effet pose : ce que son bâtisseur écrit dans le
// nœud pour ce site.
//
// ⚠ LE STYLE N'EST PAS LE MOT DE RENDU. Le premier bâtisseur pose TREIZE au mot
// de rendu et un FLOTTANT de style à part ; c'est ce dernier que le dessinateur
// relit pour choisir sa teinte. Prendre le mot pour le style rendrait blanc ce
// que le client peint en vert (680), en lilas et en bleu (881).
struct EffectTwinDiscPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // LE STYLE DU NŒUD, que la figure relit comme un flottant : quinze, dix-sept
  // ou dix-huit selon l'octet du site, quatre sinon — et quatre pour le
  // bâtisseur à un argument.
  std::int32_t node_style = 4;
  // LE RAYON D'ANCRE des deux pistes, le même pour l'une et l'autre.
  float radius = 15.0F;
  // L'ALPHA DE NAISSANCE des deux pistes.
  std::int16_t alpha = 200;
  // LE MOT DE RENDU : treize pour le premier bâtisseur, cinq pour le
  // second. Voir `effect_render_word.h`.
  std::uint32_t render_word = kEffectRenderWordOfConstructor;
  // L'OCTET QUE LE SITE PASSE au bâtisseur à deux arguments, ou rien pour celui
  // qui n'en prend qu'un. Il a déjà choisi le style et le rayon ci-dessus : il
  // n'est gardé que pour se relire contre le sous-type 161, qui est le même code.
  std::optional<std::uint8_t> builder_argument;
};

[[nodiscard]] std::span<const EffectTwinDiscPattern> EffectTwinDiscPatternTable();

[[nodiscard]] std::span<const EffectTwinDiscPattern> EffectTwinDiscPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud — mais DEUX seulement sont vives.
//
// ⚠ AUCUN TIRAGE : les deux bâtisseurs posent tout en dur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTwinDiscTracks(
    const EffectTwinDiscPattern& pattern);

[[nodiscard]] PooledEffectNode SowEffectTwinDiscNode(const EffectTwinDiscPattern& pattern,
                                                     glm::vec3 anchor,
                                                     std::int32_t effect_life_ticks);

// ---------------------------------------------------------------------------
// LE MOUVEMENT
// ---------------------------------------------------------------------------

// DE COMBIEN L'INCLINAISON TOURNE par tic, sur les deux pistes vives.
//
// ⚠ C'EST L'INCLINAISON QUI MÈNE LE SOUFFLE de cette figure, pas un compteur —
// voir `effect_breathing_quad.h`. La faire tourner, c'est faire respirer les
// deux disques ; l'oublier les figerait à leur rayon de départ.
inline constexpr std::int16_t kTwinDiscTiltPerTick = 3;

// LE MOUVEMENT DU SOUS-TYPE 28.
//
// ⚠ IL NE TOUCHE QUE LES DEUX PREMIÈRES PISTES, et les touche même éteintes :
// le client ne teste rien avant d'ajouter. Les deux dernières ne sont jamais
// allumées par cette famille, si bien que cela ne se voit pas — mais borner la
// boucle aux pistes vives changerait le code sans changer l'image, et masquerait
// ce que le client fait vraiment.
//
// ⚠ ET LE NŒUD SUIT SON PORTEUR PAR L'INSTANCE : le mouvement recopie la place
// de l'instance dans le nœud, et le bâtisseur, rappelé à chaque tic, recale
// l'instance sur son porteur AVANT sa garde d'âge. C'est le pas que la
// troupe applique à `ground_position` (`MoveAnchored`) : le mouvement ne le
// touche pas.
void AdvanceTwinDiscTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
