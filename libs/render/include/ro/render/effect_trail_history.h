#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'HISTORIQUE DE TRAÎNÉE : le nœud tel qu'il était, un tic après l'autre.
//
// C'est le mécanisme de
// l'IMAGE RÉMANENTE : le client garde N copies du nœud, décale la file d'un cran
// à chaque tic et dessine les N. La plus ancienne est la plus pâle et la plus
// petite, ce qui donne le sillage.
//
// ⚠ CE N'EST PAS UNE FIGURE, C'EST UNE MÉCANIQUE, et elle sert TOUTE la famille
// des sprites à traînée — le sous-type 153 en est, et il ouvre quinze
// identifiants. Une figure de cette famille portée sans elle se dessinerait UNE
// fois : présente à l'écran, plausible, et privée de ce qui la fait reconnaître.
//
// ⚠ LA FILE SE DÉCALE DE LA FIN VERS LE DÉBUT. L'inverse recopierait l'état
// courant dans toutes les cases d'un seul tic, et la traînée deviendrait N
// copies superposées — le même piège que les cascades de pistes.

// UNE ÉTAPE : le nœud à un tic passé, déjà pâli et rétréci.
struct EffectTrailStep {
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  float sprite_scale = 1.0F;
  float angle_degrees = 0.0F;
  // De zéro au plafond du nœud. C'est une valeur FRACTIONNAIRE comme celle du
  // nœud, et non l'entier d'une piste : les deux coexistent.
  float alpha = 0.0F;
  // La teinte du nœud à ce tic, recopiée telle quelle : le client la range dans
  // l'étape avec l'alpha, en un seul mot.
  std::array<float, 3> color = {kEffectNodeChannelMax, kEffectNodeChannelMax,
                                kEffectNodeChannelMax};
};

// DE COMBIEN LA TAILLE S'ÉTEINT PAR RAPPORT À L'ALPHA.
//
// L'alpha d'une étape perd sa part entière du chemin parcouru, la taille n'en
// perd que la MOITIÉ : une traînée s'efface plus vite qu'elle ne rétrécit. Les
// égaliser donnerait un sillage qui disparaît en pointe au lieu de s'estomper.
inline constexpr float kEffectTrailScaleFadeShare = 0.5F;

// FAIT AVANCER L'HISTORIQUE D'UN TIC.
//
// `history` garde sa taille : c'est elle qui dit combien d'étapes la traînée
// porte, et le client la tient dans le nœud. Un historique VIDE est une
// réponse — un nœud sans traînée n'en paie rien.
//
// `ground_position` est le point du nœud à cet instant ; il entre en tête, et
// tout le reste recule d'un cran.
void AdvanceEffectTrailHistory(std::vector<EffectTrailStep>& history, const EffectNode& node,
                               glm::vec3 ground_position);

// FAIT NAÎTRE LA TRAÎNÉE REPLIÉE SUR LE NŒUD : `count` crans, tous au point de
// naissance, la tête à l'état courant et les autres déjà pâlis et rétrécis par
// rang — ce que la construction des sommets puis le premier décalage donnent
// chez le client. Un compte de un ou moins VIDE l'historique :
// le sprite se dessine alors une fois, sans rien payer.
void SeedEffectTrailHistory(std::vector<EffectTrailStep>& history, std::int32_t count,
                            const EffectNode& node, glm::vec3 birth_position);

// LE FACTEUR D'ÉCHELLE D'UN CRAN, rapporté au nœud.
//
// ⚠ L'ÉCHELLE D'UN CRAN EST ABSOLUE — celle du client, rampée par rang —,
// alors qu'une pose se dessine par un facteur PAR-DESSUS des quads qui portent
// déjà l'échelle du nœud. Passer le cran tel quel dessine la tête à l'échelle
// AU CARRÉ : deux et trois quarts deviendraient sept et demi pour une boule,
// six dixièmes un tiers pour un halo. Un pour la tête, la rampe du client pour la
// suite ; et un si le nœud n'a pas d'échelle, faute de quoi rien ne se verrait.
[[nodiscard]] float EffectTrailStepScaleFactor(const EffectTrailStep& step, const EffectNode& node);

// LE MOUVEMENT DE LA TRAÎNÉE QUI S'ALLUME PUIS S'ÉTEINT.
//
// C'est le mouvement du sous-type 153. Il est court et il fait trois choses : il monte l'alpha du
// NŒUD pendant ses premiers tics, le redescend pendant ses derniers, et fait
// reculer l'historique d'un cran.
//
// ⚠ L'ALPHA QU'IL MÈNE EST CELUI DU NŒUD, PAS CELUI D'UNE PISTE. Les deux
// coexistent dans la famille — l'un fractionnaire, l'autre entier — et ce
// mouvement-ci ne touche qu'au premier. Se tromper de compteur laisserait le
// sprite à l'alpha de sa naissance, c'est-à-dire éteint.
//
// ⚠ ET CE SONT LES TICS DES PISTES QUI LE MÈNENT, une par une : chaque piste
// vive avance son compteur et pousse l'alpha du nœud. Un nœud à quatre pistes
// vives monterait donc quatre fois plus vite — c'est ce que le client fait, et
// le lisser donnerait une montée quatre fois trop lente là où il en allume
// plusieurs.
void AdvanceFadingTrailNode(std::array<EffectTrack, kEffectTracksPerNode>& tracks, EffectNode& node,
                            std::vector<EffectTrailStep>& history, glm::vec3 ground_position);

}  // namespace ro::render
