#include "ro/render/attack_sound.h"

#include <algorithm>
#include <cstddef>
#include <ranges>

#include "ro/formats/lua_table.h"
#include "ro/render/class_hit_sound_table.h"

#include "sprite_catalog_internal.h"

namespace ro::render {
namespace {

// Le bruit d'un jeu de quatre, choisi par le tirage.
std::string OneOf(const std::array<std::string_view, 4>& sounds, std::uint32_t draw) {
  return std::string(sounds[draw % sounds.size()]);
}

}  // namespace

std::string WeaponHitSound(const SpriteCatalog& catalog, std::int32_t weapon_item,
                           std::uint32_t draw) {
  // 🔴 SOURCE INTROUVABLE : le client garde sa classe d'arme à MOINS UN et rend
  // les quatre bruits sourds. C'est le cas de TOUTE unité posée au sol, qui ne
  // vit pas dans sa liste d'acteurs — et l'abandonner rendait chaque coup de
  // Storm Gust muet.
  if (weapon_item < 0) {
    return OneOf(kEnemyHitSounds, draw);
  }
  // 🔴 DEUX TRADUCTIONS AVANT LA TABLE, et les sauter rend le coup MUET. Ce que
  // le serveur envoie dans l'apparence est l'IDENTIFIANT DE L'OBJET ; le client
  // en tire la classe d'arme, puis le SEAU d'animation de cette classe, et
  // c'est par ce seau qu'il indexe sa table. Un bâton porte l'objet 1601, la
  // table n'a que trente entrées, et `table[1601]` ne rend rien du tout.
  const int weapon_class =
      catalog_internal::WeaponClassOf(catalog, static_cast<std::uint32_t>(weapon_item));
  // Une classe hors table — un bouclier, une créature — sonne des quatre bruits
  // sourds : le seau vaut alors moins un.
  if (weapon_class < 0) {
    return OneOf(kEnemyHitSounds, draw);
  }
  const std::uint32_t bucket =
      catalog_internal::WeaponFamilyClass(catalog, static_cast<std::uint32_t>(weapon_class));
  if (bucket == kBareHandsWeapon) {
    return OneOf(kFistHitSounds, draw);
  }
  const formats::LuaValue* table = catalog.globals.Find(std::string(kWeaponHitTableGlobal));
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable || !table->table) {
    return {};
  }
  const formats::LuaValue* sound = table->table->Find(static_cast<double>(bucket));
  if (sound == nullptr || sound->kind != formats::LuaValue::Kind::kString) {
    return {};
  }
  return sound->text;
}

bool ClassHasHitSound(std::int32_t job) noexcept {
  // Les quatre plages que le client écarte, dans son ordre et avec ses bornes.
  if (job < 0) {
    return false;
  }
  if (job > kLastBasicClass && job < kNoviceHighClass) {
    return false;
  }
  if (job > kNoviceHighClass && job < kFirstAdvancedClass) {
    return false;
  }
  return job < kFirstShownOnlyClass || job >= kFirstShownOnlyClass + kShownOnlyClassCount;
}

std::string_view ClassHitSound(std::int32_t job) {
  if (!ClassHasHitSound(job)) {
    return {};
  }
  // ⚠ DEUX INDEXATIONS, ET LE SEUIL N'EST PAS CELUI DES BORNES : le client
  // indexe par la classe jusqu'à 3950, puis par `classe − 3950`. Les classes
  // avancées, qui commencent à 3999, tombent donc vers le haut du vecteur.
  const std::int32_t index = job <= kAdvancedClassIndexBase ? job : job - kAdvancedClassIndexBase;
  const std::span<const ClassHitSoundEntry> table = ClassHitSoundTable();
  const auto found = std::lower_bound(
      table.begin(), table.end(), index,
      [](const ClassHitSoundEntry& entry, std::int32_t wanted) { return entry.index < wanted; });
  if (found != table.end() && found->index == index) {
    return found->sound;
  }
  // Tout le reste garde ce dont le vecteur est rempli.
  return kDefaultClassHitSound;
}

std::string AttackSound(const SpriteCatalog& catalog, bool target_is_player,
                        std::int32_t target_job, std::int32_t attacker_weapon, std::uint32_t draw) {
  // ⚠ C'EST LA CIBLE QUI DÉCIDE, ET NON QUI FRAPPE. Le client lit le métier
  // d'affichage de la CIBLE : joueur, il prend le bruit de sa CLASSE ; autre
  // chose, le bruit de l'ARME qui la touche. Lire l'attaquant donnait le bruit
  // d'arme quand un monstre nous frappait.
  if (target_is_player) {
    const std::string_view of_class = ClassHitSound(target_job);
    if (!of_class.empty()) {
      return std::string(of_class);
    }
    // Hors des bornes de la table des classes, le client retombe sur celle des
    // ARMES — et sur les quatre bruits sourds quand elle ne répond rien.
    return WeaponHitSound(catalog, attacker_weapon, draw);
  }
  return WeaponHitSound(catalog, attacker_weapon, draw);
}

bool SkillBlowUsesEnemySounds(std::uint8_t damage_type, std::uint16_t announced_hits) noexcept {
  // LE CUMUL D'ABORD : un coup multiple porte son cumul, et le message qui le
  // porte devient 111 — donc ni 96 ni 97, donc la cible rend son métier.
  // ⚠ LE COMPTE NE SE LIT PAS À TRAVERS LA SORTE ICI : le répartiteur des
  // compétences borne le compte brut à [1, 36] et compare à un, sans regarder
  // la sorte, contrairement à ce que fait le choix du NOMBRE affiché.
  if (announced_hits != 1 && announced_hits <= kLargestHitCount) {
    return false;
  }
  return std::ranges::find(kPlainSoundingDamageTypes, damage_type) ==
         kPlainSoundingDamageTypes.end();
}

std::string SkillBlowSound(std::uint32_t draw) { return OneOf(kEnemyHitSounds, draw); }

}  // namespace ro::render
