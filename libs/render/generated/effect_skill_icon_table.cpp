// LES ICONES DE COMPETENCE qu'un effet montre, et le chemin de chacune.
//
// Relevees dans l'INSTALLATION du joueur, et non dans l'executable : le
// client compose ce nom a l'execution — le Lua lui rend « MG_FIREBOLT », il
// le met dans son format et charge le fichier. Un releve de desassemblage
// n'en voit rien.
//
// 26 icones, et CHACUNE a ete confrontee a l'archive : le releve n'ecrit une
// entree que s'il a trouve le fichier. Un nom juste et une image absente
// donneraient un effet muet, et rien ne le dirait.
//
// ENGENDRE par scripts/gen_client_skill_icons.py depuis le releve
// scripts/client_skill_icons.json - ne pas modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_skill_icon.h"

namespace ro::render {
namespace {

// Triee par identifiant de competence : la recherche est dichotomique.
constexpr std::array<EffectSkillIcon, 26> kSkillIcons = {{
    {9, "MG_SRECOVERY", "data\\texture\\유저인터페이스\\item\\MG_SRECOVERY.bmp"},
    {10, "MG_SIGHT", "data\\texture\\유저인터페이스\\item\\MG_SIGHT.bmp"},
    {11, "MG_NAPALMBEAT", "data\\texture\\유저인터페이스\\item\\MG_NAPALMBEAT.bmp"},
    {12, "MG_SAFETYWALL", "data\\texture\\유저인터페이스\\item\\MG_SAFETYWALL.bmp"},
    {13, "MG_SOULSTRIKE", "data\\texture\\유저인터페이스\\item\\MG_SOULSTRIKE.bmp"},
    {14, "MG_COLDBOLT", "data\\texture\\유저인터페이스\\item\\MG_COLDBOLT.bmp"},
    {15, "MG_FROSTDIVER", "data\\texture\\유저인터페이스\\item\\MG_FROSTDIVER.bmp"},
    {16, "MG_STONECURSE", "data\\texture\\유저인터페이스\\item\\MG_STONECURSE.bmp"},
    {17, "MG_FIREBALL", "data\\texture\\유저인터페이스\\item\\MG_FIREBALL.bmp"},
    {18, "MG_FIREWALL", "data\\texture\\유저인터페이스\\item\\MG_FIREWALL.bmp"},
    {19, "MG_FIREBOLT", "data\\texture\\유저인터페이스\\item\\MG_FIREBOLT.bmp"},
    {20, "MG_LIGHTNINGBOLT", "data\\texture\\유저인터페이스\\item\\MG_LIGHTNINGBOLT.bmp"},
    {80, "WZ_FIREPILLAR", "data\\texture\\유저인터페이스\\item\\WZ_FIREPILLAR.bmp"},
    {81, "WZ_SIGHTRASHER", "data\\texture\\유저인터페이스\\item\\WZ_SIGHTRASHER.bmp"},
    {82, "WZ_FIREIVY", "data\\texture\\유저인터페이스\\item\\WZ_FIREIVY.bmp"},
    {83, "WZ_METEOR", "data\\texture\\유저인터페이스\\item\\WZ_METEOR.bmp"},
    {84, "WZ_JUPITEL", "data\\texture\\유저인터페이스\\item\\WZ_JUPITEL.bmp"},
    {85, "WZ_VERMILION", "data\\texture\\유저인터페이스\\item\\WZ_VERMILION.bmp"},
    {86, "WZ_WATERBALL", "data\\texture\\유저인터페이스\\item\\WZ_WATERBALL.bmp"},
    {87, "WZ_ICEWALL", "data\\texture\\유저인터페이스\\item\\WZ_ICEWALL.bmp"},
    {88, "WZ_FROSTNOVA", "data\\texture\\유저인터페이스\\item\\WZ_FROSTNOVA.bmp"},
    {89, "WZ_STORMGUST", "data\\texture\\유저인터페이스\\item\\WZ_STORMGUST.bmp"},
    {90, "WZ_EARTHSPIKE", "data\\texture\\유저인터페이스\\item\\WZ_EARTHSPIKE.bmp"},
    {91, "WZ_HEAVENDRIVE", "data\\texture\\유저인터페이스\\item\\WZ_HEAVENDRIVE.bmp"},
    {92, "WZ_QUAGMIRE", "data\\texture\\유저인터페이스\\item\\WZ_QUAGMIRE.bmp"},
    {2285, "SC_REPRODUCE", "data\\texture\\유저인터페이스\\item\\SC_REPRODUCE.bmp"},
}};

}  // namespace

std::span<const EffectSkillIcon> EffectSkillIconTable() { return kSkillIcons; }

std::string_view EffectSkillIconFor(std::uint32_t skill_id) {
  const auto found =
      std::ranges::lower_bound(kSkillIcons, skill_id, {}, &EffectSkillIcon::skill_id);
  if (found == kSkillIcons.end() || found->skill_id != skill_id) {
    return {};
  }
  return found->texture;
}

}  // namespace ro::render
