// LES CLÉS DES DÉGÂTS FLOTTANTS, ET CE QUI LES DÉCLARE.
//
// ⚠ À PART DU CATALOGUE DES CLÉS, pour le plafond, et parce qu'elles forment un
// tout : une cinquantaine de réglages d'un seul objet, `HitPopupLook`. Les
// règles qui les valent sont celles du catalogue — voir `game_option_keys.cpp`.

#include <array>
#include <cstddef>
#include <string>

#include "options/game_options.h"

namespace moonclient {
namespace {

// ⚠ GRAVÉES : les changer rend à tous les joueurs les réglages d'origine, sans
// que rien ne le dise. PRÉFIXE `damage_`, et des MOTS : « damage_color_self »
// se relit dans un fichier ouvert à la main, « damage_7 » non. Les rangs des
// listes — le partage, les allures — s'écrivent en nombre, et c'est pourquoi
// l'ordre de leurs énumérations est gravé.
constexpr const char* kDamageShownKey = "damage_shown";
constexpr const char* kDamageOwnDealtKey = "damage_my_hits";
constexpr const char* kDamageOwnTakenKey = "damage_hits_received";
constexpr const char* kDamageOthersKey = "damage_hits_of_others";
constexpr const char* kDamageMissKey = "damage_miss";
constexpr const char* kDamageLuckyKey = "damage_lucky";
constexpr const char* kDamageCriticalStarKey = "damage_star_critical";
constexpr const char* kDamageHealsKey = "damage_heals";
constexpr const char* kDamageEffectNumbersKey = "damage_numbers_of_effects";
// ⚠ UNE CLÉ SE RENOMME quand son défaut change — le partage l'a été, les
// chemins aussi —, pour qu'un fichier écrit sous l'ancien défaut ne le garde
// pas sous le nouveau nom : une clé inconnue est ignorée, une clé connue est lue.
constexpr const char* kDamageSplitKey = "damage_sharing_hits_multiple";
constexpr const char* kDamageHitIntervalKey = "damage_interval_hits";
constexpr const char* kDamageScaleKey = "damage_size";
constexpr const char* kDamageDigitPitchKey = "damage_gap_digits";
constexpr const char* kDamageOpacityKey = "damage_opacity";
constexpr const char* kDamageAbbreviateKey = "damage_short";
constexpr const char* kDamageColorOwnDealtKey = "damage_color_my_hits";
constexpr const char* kDamageColorMonsterKey = "damage_color_monster";
constexpr const char* kDamageColorPlayerKey = "damage_color_player";
constexpr const char* kDamageColorSelfKey = "damage_color_self";
constexpr const char* kDamageColorCriticalKey = "damage_color_critical";
constexpr const char* kDamageColorTotalKey = "damage_color_stack";
constexpr const char* kDamageTotalCarriesKey = "damage_total_continuous";
constexpr const char* kDamageTotalForgetKey = "damage_total_forget_ms";
constexpr const char* kDamageColorSpKey = "damage_color_magic";
constexpr const char* kDamageColorHealKey = "damage_color_heal";
constexpr const char* kDamageColorSpRecoveryKey = "damage_color_magic_rendered";
constexpr const char* kDamageFontTextKey = "damage_font";
constexpr const char* kDamageFontOutlineKey = "damage_font_outline";
constexpr const char* kDamageFontNameKey = "damage_font_name";
constexpr const char* kDamageFontBoldKey = "damage_font_bold";
// ⚠ L'INTERVALLE DES COUPS FACTICES S'ENREGISTRE, lui : ce n'est pas un coup
// qui survit à la session, mais la vitesse à laquelle on aime régler.
constexpr const char* kDamageRehearsalStepKey = "damage_preview_interval_ms";
constexpr const char* kDamagePreviewFloorKey = "damage_preview_ground_px";
constexpr const char* kDamagePreviewGridKey = "damage_preview_grid_px";

// LA VIE ET LE CHEMIN DE CHAQUE TYPE : « damage_type_<type>_<field> ». ⚠ AU RANG
// DE `DamagePath`.
//
// 🔴 DES NOMS NEUFS, PARCE QUE LE SENS A CHANGÉ : un chemin s'ajoutait à
// l'allure du client, il en porte maintenant tout le mouvement, et son défaut
// n'est plus zéro. Lus sous les anciens noms, les zéros d'un ancien fichier
// effaceraient la cloche du client.
constexpr std::array<const char*, kDamagePathCount> kDamagePathKeyWords = {
    "hit_monster", "skill_monster", "player", "critical", "stack", "heal", "words"};
constexpr const char* kDamagePathKeyPrefix = "damage_type_";

}  // namespace

// ⚠ L'ADRESSE DU CHAMP, comme partout : la lecture et l'écriture parcourent la
// même table, et la valeur présente ici est le défaut — celui de
// `HitPopupLook`, qui est le client.
void GameOptions::DeclareDamage(SettingsFile& into) {
  into.Declare(kDamageShownKey, &damage_.shown);
  into.Declare(kDamageOwnDealtKey, &damage_.own_dealt);
  into.Declare(kDamageOwnTakenKey, &damage_.own_taken);
  into.Declare(kDamageOthersKey, &damage_.others);
  into.Declare(kDamageMissKey, &damage_.miss);
  into.Declare(kDamageLuckyKey, &damage_.lucky);
  into.Declare(kDamageCriticalStarKey, &damage_.critical_star);
  into.Declare(kDamageHealsKey, &damage_.heals);
  into.Declare(kDamageEffectNumbersKey, &damage_.effect_numbers);
  into.Declare(kDamageSplitKey, &damage_.split);
  into.Declare(kDamageHitIntervalKey, &damage_.hit_interval_ms);
  into.Declare(kDamageScaleKey, &damage_.motion.scale);
  into.Declare(kDamageDigitPitchKey, &damage_.motion.digit_pitch_pixels);
  into.Declare(kDamageOpacityKey, &damage_.motion.opacity);
  into.Declare(kDamageAbbreviateKey, &damage_.motion.abbreviate);
  into.Declare(kDamageColorOwnDealtKey, &damage_.color_own_dealt);
  into.Declare(kDamageColorMonsterKey, &damage_.color_on_monster);
  into.Declare(kDamageColorPlayerKey, &damage_.color_on_player);
  into.Declare(kDamageColorSelfKey, &damage_.color_on_self);
  into.Declare(kDamageColorCriticalKey, &damage_.color_critical);
  into.Declare(kDamageColorTotalKey, &damage_.color_total);
  into.Declare(kDamageTotalCarriesKey, &damage_.total_carries);
  into.Declare(kDamageTotalForgetKey, &damage_.total_forget_ms);
  // ⚠ PAS L'APERÇU : un coup factice ne survit pas à la session.
  into.Declare(kDamageColorSpKey, &damage_.color_sp);
  into.Declare(kDamageColorHealKey, &damage_.color_heal);
  into.Declare(kDamageColorSpRecoveryKey, &damage_.color_sp_recovery);
  into.Declare(kDamageFontTextKey, &damage_.font_text);
  into.Declare(kDamageFontOutlineKey, &damage_.font_outline_pixels);
  into.Declare(kDamageFontNameKey, &damage_.font_name);
  into.Declare(kDamageFontBoldKey, &damage_.font_bold);
  into.Declare(kDamageRehearsalStepKey, &damage_.rehearsal_interval_ms);
  into.Declare(kDamagePreviewFloorKey, &damage_.preview_floor_pixels);
  into.Declare(kDamagePreviewGridKey, &damage_.preview_grid_pixels);

  for (std::size_t rank = 0; rank < kDamagePathCount; ++rank) {
    NumberPath& path = damage_.paths[rank];
    const std::string prefix = std::string(kDamagePathKeyPrefix) + kDamagePathKeyWords[rank] + "_";
    into.Declare(prefix + "start_x", &path.start_x_pixels);
    into.Declare(prefix + "start_y", &path.lift_pixels);
    into.Declare(prefix + "arrival_x", &path.arrival_x_pixels);
    into.Declare(prefix + "arrival_y", &path.arrival_y_pixels);
    into.Declare(prefix + "handle_1_x", &path.bend_one_x_pixels);
    into.Declare(prefix + "handle_1_y", &path.bend_one_y_pixels);
    into.Declare(prefix + "handle_2_x", &path.bend_two_x_pixels);
    into.Declare(prefix + "handle_2_y", &path.bend_two_y_pixels);
    into.Declare(prefix + "spread", &path.scatter);
    into.Declare(prefix + "spread_x", &path.scatter_x_pixels);
    into.Declare(prefix + "spread_y", &path.scatter_y_pixels);
    into.Declare(prefix + "drift", &path.facing_drift);
    // ⚠ LES MOTS N'ONT PAS DE VIE RÉGLABLE : leur animation tient la leur.
    if (static_cast<DamagePath>(rank) == DamagePath::kWords) {
      continue;
    }
    ro::render::NumberLife& life = damage_.lives[rank];
    into.Declare(prefix + "birth", &life.birth_scale);
    into.Declare(prefix + "return_ms", &life.birth_settle_ms);
    into.Declare(prefix + "fade_ms", &life.fade_start_ms);
    into.Declare(prefix + "life_ms", &life.life_ms);
  }
}

}  // namespace moonclient
