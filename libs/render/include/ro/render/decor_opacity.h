#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/mapedit/map_edit.h"
#include "ro/render/model_mesh.h"
#include "ro/render/model_veil.h"

namespace ro::render {

// L'OPACITÉ DU DÉCOR DANS UNE IMAGE : celle que le document d'édition donne à
// un modèle, et le voile qui efface ce qui cache le joueur.
//
// 🔴 LES DEUX SE MULTIPLIENT. Le voile est RELATIF : il ôte à une pose les deux
// tiers de ce qu'elle montre, qu'elle soit pleine ou déjà translucide, et lui
// rend exactement ce que le document lui donne quand il se lève. Un minimum
// figerait au contraire tout modèle réglé sous le plancher du voile : passer
// devant lui ne changerait rien, et le joueur ne saurait plus s'il est caché.
//
// Une pose pleine des deux côtés reste dans la passe opaque ; sans aucun modèle
// translucide dans le document, rien ne se compose.

// Une opacité du document, de 0 à 1, en octet du voile.
[[nodiscard]] std::uint8_t OpacityByte(float opacity) noexcept;

// Le produit de deux opacités en octets, arrondi : le plein est neutre.
[[nodiscard]] std::uint8_t CombinedOpacity(std::uint8_t veil, std::uint8_t document) noexcept;

// Le rang d'une pose dans l'opacité de son modèle : les figées dans leur ordre,
// puis les animées, groupe par groupe — l'ordre de `ModelOpacities`.
[[nodiscard]] std::size_t OpacitySlot(const ModelInstances& model, std::optional<std::size_t> group,
                                      std::size_t index);

// LES OPACITÉS QUE LE DOCUMENT POSE, pose par pose : `by_rank` porte celles des
// rangs du RSW translucides, croissants (`MapScene::model_opacities`). Vide
// quand aucun n'est translucide ; sinon une entrée par modèle de `models`.
void DocumentOpacities(std::span<const ModelInstances> models,
                       std::span<const mapedit::ModelOpacity> by_rank, ModelOpacities& into);

// CE QUE LE DESSIN DU DÉCOR REÇOIT : les opacités de la passe opaque, qui en
// écarte les poses non pleines, et les poses que la passe translucide peint.
struct DecorOpacity {
  ModelOpacities opacities;
  std::vector<VeiledPose> translucent;
};

// MÊLE LE DOCUMENT AU VOILE. `veil` est nul quand le voile est éteint ;
// `veiled` porte alors rien. Une pose que le voile efface mais que la vue ne
// montre pas reste hors des deux passes, comme sans document ; une pose que le
// document seul rend translucide se peint où qu'elle soit, ses faces hors de
// l'écran écartées. Une pose d'opacité nulle ne se peint pas du tout.
void ComposeDecorOpacity(std::span<const ModelInstances> models, const ModelOpacities& document,
                         const ModelOpacities* veil, std::span<const VeiledPose> veiled,
                         DecorOpacity& into);

// 🔴 L'OMBRE D'UNE POSE TRANSLUCIDE : sous la moitié de son opacité, elle n'en
// porte plus ; au-dessus, elle en porte une pleine — une carte d'ombre ne sait
// pas peindre une ombre à demi. Seul le document en décide : le voile, qui suit
// la caméra, ferait clignoter l'ombre chaque fois que le joueur passe derrière.
inline constexpr std::uint8_t kShadowlessBelowOpacity = 128;

// Les poses qui portent ombre, pleines, et les autres à zéro, rangées comme
// `document` : de quoi écarter celles-ci de la carte d'ombre. Vide quand toutes
// en portent.
void ShadowCastingOpacities(const ModelOpacities& document, ModelOpacities& into);

}  // namespace ro::render
