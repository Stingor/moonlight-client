// LES NŒUDS DE PARTICULES QU'UN EFFET SÈME, par identifiant d'effet.
//
// 31 nœuds pour 9 effets — le 770 en sème huit à lui seul, avec huit
// textures différentes.
//
// Les marques employées sont 1, 2, 6, 7, 8, 9.
//
// ENGENDRÉ par scripts/gen_client_effect_rising_particles.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_particle_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRisingParticlePattern, 31> kRisingParticles = {{
    {274, "Effect_Build_RisingScatter2b", "data\\texture\\effect\\coin_a.bmp", 3.5F, 1, 1.5F, 30},
    {495, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\shield.bmp", 5.0F, 2,
     1.0F, 1},
    {495, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\sword.bmp", 9.0F, 2,
     1.0F, 1},
    {627, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\골드룩스.bmp", 5.0F, 2, 1.0F, 1},
    {627, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\더개리슨.bmp", 5.0F, 2, 1.0F, 1},
    {627, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\더블샷건.bmp", 5.0F, 2, 1.0F, 1},
    {627, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\더스크.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\루드라의활.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\아트로스의흉기.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\모닝스타.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\바스타드소드.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\글레이브.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\블레이드휩.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\드래곤슬레이어.bmp", 5.0F, 2, 1.0F, 1},
    {770, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\배틀액스.bmp", 5.0F, 2, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\갈색반지.bmp", 5.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\금반지.bmp", 5.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\금방울.bmp", 5.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\꽃반지.bmp", 5.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\낡은펜던트.bmp", 5.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\눈구슬반지.bmp", 5.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\유저인터페이스\\item\\상자.bmp",
     4.0F, 6, 1.0F, 1},
    {820, "Effect_Build_RisingScatter2bFromParam",
     "data\\texture\\유저인터페이스\\item\\수제초콜렛.bmp", 5.0F, 6, 1.0F, 1},
    {869, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\melody_a.bmp", 6.0F, 6,
     1.0F, 1},
    {869, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\melody_b.bmp", 6.0F, 6,
     1.0F, 1},
    {907, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\shield.bmp", 5.0F, 7,
     1.0F, 1},
    {907, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\sword.bmp", 9.0F, 7,
     1.0F, 1},
    {2436, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\shield.bmp", 5.0F, 8,
     1.0F, 1},
    {2436, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\sword.bmp", 9.0F, 8,
     1.0F, 1},
    {2437, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\shield.bmp", 5.0F, 9,
     1.0F, 1},
    {2437, "Effect_Build_RisingScatter2bFromParam", "data\\texture\\effect\\sword.bmp", 9.0F, 9,
     1.0F, 1},
}};

}  // namespace

std::span<const EffectRisingParticlePattern> EffectRisingParticlePatternTable() {
  return kRisingParticles;
}

std::span<const EffectRisingParticlePattern> EffectRisingParticlePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] = std::ranges::equal_range(kRisingParticles, effect_id, {},
                                                      &EffectRisingParticlePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
