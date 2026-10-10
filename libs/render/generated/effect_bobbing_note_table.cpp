// LA NOTE QUI FLOTTE : qui la pose.
//
// Le sous-type 156, qui lit de son cas une image et un octet.
//
// 3 site(s). Colonnes : identifiant, fonction, image, image que
// /mineffect lui substitue (vide sans variante), octet, vie de l'instance.
// Tout le reste est ecrit en dur par le batisseur : voir
// effect_bobbing_note.h.
//
// ENGENDRE par scripts/gen_client_effect_bobbing_notes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_bobbing_note.h"

namespace ro::render {
namespace {

constexpr std::array<EffectBobbingNotePattern, 3> kBobbingNotes = {{
    {856, "Effect_InitSpriteAtPos", "data\\texture\\effect\\melody_a.bmp", "", 0, 29999},
    {860, "Effect_InitSpriteAtPos", "data\\texture\\effect\\melody_a.bmp", "", 1, 29999},
    {1569, "Effect_InitSpriteAtPos", "data\\texture\\effect\\new_reverberation\\melody_a.bmp",
     "data\\texture\\effect\\mineffect\\new_reverberation\\melody_a.bmp", 2, 150},
}};

}  // namespace

std::span<const EffectBobbingNotePattern> EffectBobbingNotePatternTable() { return kBobbingNotes; }

}  // namespace ro::render
