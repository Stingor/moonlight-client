// LES ESSAIMS : les noeuds des sous-types 155 et 170.
//
// 5 identifiants, 18 appels : un site en fait plusieurs, et chacun pose
// son propre noeud. Deux batisseurs, qui sont un seul semis a trois
// nombres pres - voir effect_swarm_seed.h.
//
// Les identifiants 1584 et 1594 ont DEUX lignes par image : leur site
// choisit son dossier par une bascule de reglage, que nous ne
// modelisons pas. La fenetre ne seme que les images qui se lisent.
//
// ENGENDRE par scripts/gen_client_effect_swarms.py - ne pas modifier a
// la main.

#include <array>

#include "ro/render/effect_swarm_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSwarmPattern, 18> kSwarms = {{
    {854, "Effect_InitDirectedSprayEmitter", "data\\texture\\effect\\melody_a.bmp",
     EffectSwarmKind::kNarrow, 0},
    {854, "Effect_InitDirectedSprayEmitter", "data\\texture\\effect\\melody_b.bmp",
     EffectSwarmKind::kNarrow, 0},
    {858, "Effect_InitDirectedSprayEmitter", "data\\texture\\effect\\melody_a.bmp",
     EffectSwarmKind::kNarrow, 1},
    {858, "Effect_InitDirectedSprayEmitter", "data\\texture\\effect\\melody_b.bmp",
     EffectSwarmKind::kNarrow, 1},
    {969, "Effect_InitTexturedParticles4", "data\\texture\\effect\\melody_a.bmp",
     EffectSwarmKind::kWide, 1},
    {969, "Effect_InitTexturedParticles4", "data\\texture\\effect\\melody_b.bmp",
     EffectSwarmKind::kWide, 1},
    {969, "Effect_InitTexturedParticles4", "data\\texture\\effect\\melody_a.bmp",
     EffectSwarmKind::kWide, 1},
    {969, "Effect_InitTexturedParticles4", "data\\texture\\effect\\melody_b.bmp",
     EffectSwarmKind::kWide, 1},
    {1584, "Effect_InitDirectedSprayEmitter",
     "data\\texture\\effect\\mineffect\\new_metalicsound\\new_metalicsound_icon\\met_particle_1."
     "tga",
     EffectSwarmKind::kNarrow, 2},  // bascule vraie
    {1584, "Effect_InitDirectedSprayEmitter",
     "data\\texture\\effect\\mineffect\\new_metalicsound\\new_metalicsound_icon\\met_particle_2."
     "tga",
     EffectSwarmKind::kNarrow, 2},  // bascule vraie
    {1584, "Effect_InitDirectedSprayEmitter",
     "data\\texture\\effect\\mineffect\\new_metalicsound\\new_metalicsound_icon\\met_particle_3."
     "tga",
     EffectSwarmKind::kNarrow, 2},  // bascule vraie
    {1584, "Effect_InitDirectedSprayEmitter",
     "data\\texture\\effect\\new_metalicsound\\new_metalicsound_icon\\met_particle_1.tga",
     EffectSwarmKind::kNarrow, 2},  // bascule fausse
    {1584, "Effect_InitDirectedSprayEmitter",
     "data\\texture\\effect\\new_metalicsound\\new_metalicsound_icon\\met_particle_2.tga",
     EffectSwarmKind::kNarrow, 2},  // bascule fausse
    {1584, "Effect_InitDirectedSprayEmitter",
     "data\\texture\\effect\\new_metalicsound\\new_metalicsound_icon\\met_particle_3.tga",
     EffectSwarmKind::kNarrow, 2},  // bascule fausse
    {1594, "Effect_InitTexturedParticles4",
     "data\\texture\\effect\\mineffect\\new_reverberation\\rev_note_01.tga", EffectSwarmKind::kWide,
     3},  // bascule vraie
    {1594, "Effect_InitTexturedParticles4",
     "data\\texture\\effect\\mineffect\\new_reverberation\\rev_note_02.tga", EffectSwarmKind::kWide,
     3},  // bascule vraie
    {1594, "Effect_InitTexturedParticles4",
     "data\\texture\\effect\\new_reverberation\\rev_note_01.tga", EffectSwarmKind::kWide,
     3},  // bascule fausse
    {1594, "Effect_InitTexturedParticles4",
     "data\\texture\\effect\\new_reverberation\\rev_note_02.tga", EffectSwarmKind::kWide,
     3},  // bascule fausse
}};

}  // namespace

std::span<const EffectSwarmPattern> EffectSwarmPatternTable() { return kSwarms; }

}  // namespace ro::render
