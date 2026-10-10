// LES EFFETS PRIMITIFS DONT LE FICHIER REPART À SA FIN — voir
// ro/render/primitive_str_loop.h.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_primitive_str_loops.py depuis le
// relevé scripts/client_effect_dispatch.json, pas écrit à la main.

#include <array>

#include "ro/render/primitive_str_loop.h"

namespace ro::render {
namespace {

constexpr std::array<std::uint16_t, 2> kPrimitiveStrLoops = {145, 926};

}  // namespace

std::span<const std::uint16_t> PrimitiveStrLoopTable() { return kPrimitiveStrLoops; }

}  // namespace ro::render
