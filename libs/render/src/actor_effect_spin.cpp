#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/render/actor_effect_table.h"

namespace ro::render {
namespace {

// CE QUE FAIT TOURNER `EF_SPINMOVE` : l'ANGLE DE FACE de l'acteur.
//
// C'est un angle en degrés, continu, celui que le client écrit quand un acteur
// se tourne vers un point, et qu'il arrondit au huitième de tour pour choisir
// l'image. L'effet l'AVANCE, et l'acteur paraît pivoter sur lui-même.
//
// ⚠ CE N'EST PAS LE BIT `0x40`, qui fait tourner le corps sur un CERCLE autour
// de sa place. Celui-ci ne le déplace pas : il le fait se retourner.

// LES PALIERS DE VITESSE, en degrés par tic, et jusqu'à quel tic chacun tient.
//
// Le client accélère puis ralentit, et les deux moitiés sont symétriques : douze
// degrés par tic, puis dix-huit, vingt-quatre, quarante-huit au plus vite, puis
// les mêmes en redescendant. Passé le quarante-cinquième tic il n'ajoute plus
// rien.
struct SpinStep {
  std::uint32_t through_tick = 0;
  float degrees_per_tick = 0.0F;
};

constexpr std::array<SpinStep, 7> kSpinSteps = {
    {{7, 12.0F}, {15, 18.0F}, {20, 24.0F}, {25, 48.0F}, {32, 24.0F}, {37, 18.0F}, {45, 12.0F}}};

// Le dernier tic où l'effet ajoute quelque chose.
constexpr std::uint32_t kLastSpinningTick = 45;

// La toupie : sa vie et son pas.
constexpr std::uint32_t kSpinningTopTicks = 60;
constexpr float kSpinningTopDegreesPerTick = 12.0F;

// Les kunai en étoile : leur pas et le nombre de tics où ils le donnent.
constexpr std::uint32_t kKunaiStarTurningTicks = 19;
constexpr float kKunaiStarDegreesPerTick = 45.0F;

// Ce que l'effet ajoute à CE tic-là, ou zéro passé la fin.
[[nodiscard]] float DegreesAtTick(std::uint32_t tick) {
  for (const SpinStep& step : kSpinSteps) {
    if (tick <= step.through_tick) {
      return step.degrees_per_tick;
    }
  }
  return 0.0F;
}

}  // namespace

float SpinDegreesAfterEffects(std::span<const ActorEffectPlay> played) {
  float degrees = 0.0F;
  for (const ActorEffectPlay& play : played) {
    // LA TOUPIE avance de douze degrés à chacun de ses soixante tics, âge zéro
    // compris ; finie, elle a fait deux tours exacts et l'acteur a retrouvé son
    // cap : elle n'ajoute plus rien.
    if (play.effect_id == kSpinningTopEffect) {
      if (play.age_ticks < kSpinningTopTicks) {
        degrees += kSpinningTopDegreesPerTick * static_cast<float>(play.age_ticks + 1);
      }
      continue;
    }
    // LES KUNAI EN ÉTOILE ajoutent leur pas à chacun de leurs dix-neuf tics, âge
    // zéro compris, et l'acteur garde l'angle atteint.
    if (play.effect_id == kKunaiStarEffect) {
      degrees += kKunaiStarDegreesPerTick *
                 static_cast<float>(std::min(play.age_ticks + 1, kKunaiStarTurningTicks));
      continue;
    }
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    // ⚠ LA FAMILLE SUFFIT, et c'est voulu : la table dit lesquels font tourner,
    // et nommer un identifiant ici en ferait un second lieu de vérité.
    if (change == nullptr || change->family != ActorEffectFamily::kSpin) {
      continue;
    }
    // L'angle à cet âge est la SOMME de ce qui a été ajouté à chaque tic depuis
    // le départ : le client avance son angle, il ne le pose pas.
    //
    // ⚠ ET IL NE REVIENT PAS. Le client replie l'angle sous trois cent soixante
    // degrés à chaque tic, ce qui ne change rien à la direction montrée ; ce
    // qui compte est qu'il RESTE là où il s'est arrêté. Un acteur qui a tourné
    // de neuf cent cinquante-quatre degrés garde le reste — deux cent
    // trente-quatre — jusqu'à ce qu'il se tourne lui-même vers autre chose.
    const std::uint32_t last = std::min(play.age_ticks, kLastSpinningTick);
    for (std::uint32_t tick = 0; tick <= last; ++tick) {
      degrees += DegreesAtTick(tick);
    }
  }
  return degrees;
}

}  // namespace ro::render
