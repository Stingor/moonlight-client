// LES SPHÈRES ÉLÉMENTAIRES d'un acteur : la part du monde qui les note.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes.

#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// L'EFFET QUE MONTRE CHAQUE ÉLÉMENT — eau, terre, feu, vent —, puis celui du
// cinquième type, la sphère du sort. Tout autre type ne montre rien.
constexpr std::array<std::uint32_t, 4> kSpiritCharmEffects = {737, 738, 735, 736};
constexpr std::uint32_t kFifthSpiritCharmEffect = 228;
constexpr std::int16_t kFirstSpiritCharmElement = 1;
constexpr std::int16_t kFifthSpiritCharmElement = 5;

// LES SPHÈRES SPIRITUELLES : un effet par classe, le même dessin en cercle
// autour du personnage, dont le NOMBRE est le paramètre. Toute classe que la
// table ne nomme pas prend celui du Monk.
constexpr std::uint32_t kMonkSpheresEffect = 228;
struct SphereEffectOfJob {
  std::uint16_t job = 0;
  std::uint32_t effect = 0;
};
constexpr std::array<SphereEffectOfJob, 6> kSphereEffectOfJob = {{
    {24, 629},    // Gunslinger : des pièces
    {4228, 629},  // Rebellion
    {4229, 629},
    {4038, 504},  // Baby Monk
    {4215, 1135},
    {4306, 1135},
}};

// LES DEUX QUATRIÈMES MÉTIERS QUI MONTRENT AUTRE CHOSE : les armes de servant
// du Dragon Knight, monté ou non, et les sphères d'ombre de l'Abyss Chaser.
constexpr std::array<std::uint16_t, 3> kServantWeaponJobs = {4252, 4265, 4280};
constexpr std::uint32_t kServantWeaponsEffect = 1694;
constexpr std::uint16_t kShadowSpheresJob = 4260;
constexpr std::uint32_t kShadowSpheresEffect = 2109;

[[nodiscard]] std::uint32_t SphereEffectFor(std::uint16_t job) {
  for (const SphereEffectOfJob& entry : kSphereEffectOfJob) {
    if (entry.job == job) {
      return entry.effect;
    }
  }
  return kMonkSpheresEffect;
}

}  // namespace

bool World::RecordSpiritSpheres(std::uint32_t id, std::int16_t count, std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  const std::uint16_t job = found->second.look.job;
  const bool hidden = (found->second.option & (kOptionHide | kOptionCloak)) != 0;
  // LES ARMES DE SERVANT ne partent jamais avec le compte : l'instance en fait
  // partir une à une jusqu'au nouveau nombre, zéro compris. Caché, le porteur
  // les perd toutes d'un coup.
  if (std::ranges::find(kServantWeaponJobs, job) != kServantWeaponJobs.end()) {
    if (hidden) {
      RemoveEffect(id, kServantWeaponsEffect);
    } else {
      RecordEffect(id, kServantWeaponsEffect, now_ms, count);
    }
    return true;
  }
  // LES SPHÈRES D'OMBRE partent toutes à zéro ; sinon l'instance retrouvée
  // reçoit le nouveau nombre. Caché, le porteur les reprend à neuf.
  if (job == kShadowSpheresJob) {
    if (count == 0 || hidden) {
      RemoveEffect(id, kShadowSpheresEffect);
    }
    if (count != 0) {
      RecordEffect(id, kShadowSpheresEffect, now_ms, count);
    }
    return true;
  }
  // L'EFFET DE SA CLASSE PART D'ABORD, puis revient avec le nouveau compte :
  // c'est ce qui fait passer de trois sphères à quatre sans en laisser traîner.
  const std::uint32_t effect = SphereEffectFor(found->second.look.job);
  RemoveEffect(id, effect);
  if (count > 0) {
    RecordEffect(id, effect, now_ms, count);
  }
  return true;
}

bool World::RecordSpiritCharm(std::uint32_t id, std::int16_t element, std::int16_t count,
                              std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  // ⚠ LES QUATRE PARTENT D'ABORD, quel que soit le compte — et seulement eux :
  // l'effet des autres éléments n'est pas retiré.
  for (const std::uint32_t effect : kSpiritCharmEffects) {
    RemoveEffect(id, effect);
  }
  Actor& actor = found->second;
  // UN COMPTE NUL, OU UN ACTEUR CACHÉ, NE MONTRE RIEN, et ne change pas
  // l'élément retenu.
  if (count <= 0 || (actor.option & (kOptionHide | kOptionCloak)) != 0) {
    return true;
  }
  actor.spirit_charm_element = element;
  // Le NOMBRE de sphères est le paramètre de l'effet.
  const auto rang = static_cast<std::uint32_t>(element - kFirstSpiritCharmElement);
  if (rang < kSpiritCharmEffects.size()) {
    RecordEffect(id, kSpiritCharmEffects[rang], now_ms, count);
  } else if (element == kFifthSpiritCharmElement) {
    // ⚠ LE MÊME NUMÉRO QUE LES SPHÈRES DU MONK, MAIS PAS LE MÊME DESSIN : ici
    // c'est la classe des orbes qui le fait naître, et non la gerbe.
    RecordEffect(id, kFifthSpiritCharmEffect, now_ms, count);
    for (PlayedEffect& played : actor.effects) {
      if (played.id == kFifthSpiritCharmEffect && played.since_ms == now_ms) {
        played.born_of_class = true;
      }
    }
  }
  return true;
}

}  // namespace ro::sim
