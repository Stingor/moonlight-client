#pragma once

#include <array>
#include <cstdint>

#include <imgui.h>

namespace moonclient {

// CE QUE LA PARTIE PEINT PAR-DESSUS TOUT LE MONDE ET SOUS LES FENÊTRES : un
// rappel de dessin, glissé dans la liste de FOND d'ImGui juste après les barres
// d'entité et les bulles, qui passent donc dessous, et avant toute fenêtre.
// Nul : rien à peindre. Voir `OverWorldLayer`.
struct OverWorldPaint {
  ImDrawCallback paint = nullptr;
  void* painter = nullptr;
};

// CE QUE LE RAPPEL PEINT, dans l'ordre où il le peint.
enum class OverWorldPass : std::uint8_t {
  kDarkVeil,  // le voile de ténèbres, au-dessus du monde
  kGameHud,   // le HUD de jeu, au-dessus du voile
};

// L'ORDRE DES PASSES : le voile, puis le HUD de jeu par-dessus lui — le
// classement PvP et l'étiquette de ce qui est armé restent lisibles sous la
// nuit.
inline constexpr std::array<OverWorldPass, 2> kOverWorldPasses = {OverWorldPass::kDarkVeil,
                                                                  OverWorldPass::kGameHud};

}  // namespace moonclient
