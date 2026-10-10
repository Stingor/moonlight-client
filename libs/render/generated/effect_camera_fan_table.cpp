// LES EVENTAILS TOURNES VERS LA CAMERA : les noeuds du sous-type 68.
//
// 3 identifiants, 6 noeuds : chaque batisseur en pose DEUX par effet, a
// vingt tics d'ecart, et une trainee par image dans chacun.
//
// ENGENDRE par scripts/gen_client_effect_camera_fans.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_camera_fan_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectCameraFanPattern, 6> kCameraFans = {{
    {328,
     "EffectUpdate_AsuraStrike",
     {"data\\texture\\effect\\asura3.tga", "data\\texture\\effect\\asura2.tga",
      "data\\texture\\effect\\asura1.tga"},
     {-40, 20, -6.0F, -12.0F, 18.0F, 0},
     {1, 1, 1},  // EF_BEGINASURA, variante 0, tic 1
     3},
    {328,
     "EffectUpdate_AsuraStrike",
     {"data\\texture\\effect\\asura4.tga", "data\\texture\\effect\\asura5.tga",
      "data\\texture\\effect\\asura6.tga"},
     {-40, -20, 6.0F, 12.0F, 18.0F, 0},
     {21, 21, 1},  // EF_BEGINASURA, variante 0, tic 21
     3},
    {503,
     "EffectUpdate_SoulLinkFan",
     {"data\\texture\\effect\\soul_l.tga", "data\\texture\\effect\\soul_u.tga",
      "data\\texture\\effect\\soul_o.tga", "data\\texture\\effect\\soul_s.tga"},
     {-60, 20, -4.0F, -6.0F, 5.0F, 2},
     {1, 1, 1},  // EF_SOULLINK, variante 0, tic 1
     4},
    {503,
     "EffectUpdate_SoulLinkFan",
     {"data\\texture\\effect\\soul_k.tga", "data\\texture\\effect\\soul_n.tga",
      "data\\texture\\effect\\soul_i.tga", "data\\texture\\effect\\soul_l.tga"},
     {-120, 20, 22.0F, -6.0F, 5.0F, 2},
     {21, 21, 1},  // EF_SOULLINK, variante 0, tic 21
     4},
    {510,
     "EffectUpdate_AsuraStrike",
     {"data\\texture\\effect\\asura13.tga", "data\\texture\\effect\\asura12.tga",
      "data\\texture\\effect\\asura11.tga"},
     {-40, 20, -8.0F, -16.0F, 24.0F, 3},
     {1, 1, 1},  // EF_BEGINASURA11, variante 1, tic 1
     3},
    {510,
     "EffectUpdate_AsuraStrike",
     {"data\\texture\\effect\\asura14.tga", "data\\texture\\effect\\asura15.tga",
      "data\\texture\\effect\\asura16.tga"},
     {-40, -20, 8.0F, 16.0F, 24.0F, 3},
     {21, 21, 1},  // EF_BEGINASURA11, variante 1, tic 21
     3},
}};

}  // namespace

std::span<const EffectCameraFanPattern> EffectCameraFanPatternTable() { return kCameraFans; }

}  // namespace ro::render
