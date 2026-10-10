#include "ro/render/effect_track.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE SOUS-TYPE 30 POSE EN DUR. Chaque valeur est une constante du
// client ; aucune n'est un réglage à notre main.
constexpr float kRadiusGrowthPerTick = 0.07F;
constexpr float kRadiusThatFadesTheRing = 4.0F;
constexpr float kRadiusAtRelight = 3.37F;
constexpr float kUndulationAmplitude = 0.3F;
constexpr std::int16_t kTiltFallPerTick = 1;
constexpr std::int16_t kTiltFloorDegrees = 10;
constexpr std::int16_t kAlphaFallPerTick = 3;
// Le style qui prend l'autre branche partout. Le client le compare sans jamais
// s'en servir autrement : lui donner un nom serait le deviner.
constexpr std::int16_t kStyleThatRisesSlowly = 22;
constexpr std::int16_t kAlphaRiseForThatStyle = 5;
constexpr std::int16_t kAlphaRisePerTick = 10;
constexpr std::int16_t kTiltAtRelightForThatStyle = 64;
constexpr std::int16_t kTiltAtRelightDegrees = 74;
// L'anneau ne se rallume que s'il lui reste au moins cela à vivre : un anneau
// rallumé au dernier tic n'aurait pas le temps de s'ouvrir, et l'on verrait une
// figure naître et disparaître dans la même image.
constexpr std::int32_t kTicksNeededToRelight = 30;
// L'écart d'azimut que le client ajoute d'une piste à la suivante, et le seuil
// de rang au-delà duquel il change de formule de phase.
constexpr std::int32_t kPhaseStepPerRankDegrees = 90;
constexpr std::int32_t kRankStepDegrees = 45;
constexpr std::int32_t kRankThresholdDegrees = 90;
constexpr std::int32_t kFastPhaseFactor = 2;

// La phase de l'onde à ce tic, pour cette piste.
//
// ⚠ DEUX FORMULES, ET LE RANG CHOISIT. Voir l'avertissement de l'en-tête : le
// client compare le rang à un seuil, et la seconde formule double la vitesse.
[[nodiscard]] std::int32_t UndulationPhaseDegrees(std::size_t rank, std::int32_t ticks) {
  const std::int32_t rang = static_cast<std::int32_t>(rank);
  if (rang * kRankStepDegrees < kRankThresholdDegrees) {
    return WithinFullTurn(ticks + rang * kPhaseStepPerRankDegrees);
  }
  return WithinFullTurn((ticks + rang * kRankStepDegrees) * kFastPhaseFactor);
}

// CE QUE LE SOUS-TYPE 139 AJOUTE, et lui seul : l'échelle du nœud RESPIRE.
//
// ⚠ QUATRE VALEURS À QUATRE INSTANTS D'UN CYCLE DE CENT TICS, et entre deux
// instants l'échelle ne bouge pas. Ce n'est donc pas une rampe : la lisser
// ferait gonfler l'anneau sans à-coup, là où le client le fait sauter d'un cran.
constexpr std::int32_t kScaleCycleTicks = 100;
constexpr std::int32_t kScaleQuarterTicks = kScaleCycleTicks / 4;
constexpr std::uint8_t kScaleAtCycleStart = 4;
constexpr std::uint8_t kScaleAtFirstQuarter = 6;
constexpr std::uint8_t kScaleAtHalf = 7;
constexpr std::uint8_t kScaleAtLastQuarter = 8;

// L'échelle du nœud à ce tic, ou rien entre deux marches.
[[nodiscard]] std::optional<std::uint8_t> CycleScale(std::int32_t ticks) {
  const std::int32_t cycle_tick = ticks % kScaleCycleTicks;
  if (cycle_tick == 0) {
    return kScaleAtCycleStart;
  }
  if (cycle_tick == kScaleQuarterTicks) {
    return kScaleAtFirstQuarter;
  }
  if (cycle_tick == 2 * kScaleQuarterTicks) {
    return kScaleAtHalf;
  }
  if (cycle_tick == 3 * kScaleQuarterTicks) {
    return kScaleAtLastQuarter;
  }
  return std::nullopt;
}

// LE CORPS COMMUN DES SOUS-TYPES 30, 139 ET 144. Tous battent autour de leur CLÉ : les
// seuils, le rallumage, les deux formules de phase sont le même code aux mêmes
// constantes.
void AdvanceWavingRing(EffectTrack& track, std::size_t rank, std::int32_t life_ticks) {
  if (!track.active) {
    return;
  }
  ++track.ticks;
  // UN TIC NÉGATIF EST UNE ATTENTE, pas une erreur : le bâtisseur recule le
  // départ des pistes pour les échelonner, et elles ne bougent qu'une fois leur
  // compte revenu à zéro.
  if (track.ticks <= 0) {
    return;
  }

  track.radius_at_anchor += kRadiusGrowthPerTick;

  track.tilt_degrees = static_cast<std::int16_t>(track.tilt_degrees - kTiltFallPerTick);
  if (track.tilt_degrees < kTiltFloorDegrees) {
    track.tilt_degrees = kTiltFloorDegrees;
    track.alpha = 0;
  }

  if (track.radius_at_anchor >= kRadiusThatFadesTheRing) {
    track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFallPerTick);
    if (track.alpha <= 0) {
      track.alpha = 0;
      if (track.ticks < life_ticks - kTicksNeededToRelight) {
        track.radius_at_anchor = kRadiusAtRelight;
        track.tilt_degrees = track.style == kStyleThatRisesSlowly ? kTiltAtRelightForThatStyle
                                                                  : kTiltAtRelightDegrees;
      }
    }
  } else {
    const std::int16_t rise =
        track.style == kStyleThatRisesSlowly ? kAlphaRiseForThatStyle : kAlphaRisePerTick;
    track.alpha = static_cast<std::int16_t>(track.alpha + rise);
  }

  // Chaque segment prend la longueur de sa base, ondulée par le produit de deux
  // sinus : l'un court LE LONG du ruban — un demi-tour réparti sur les segments
  // —, l'autre dans le TEMPS. Le premier s'annule aux deux bouts, si bien que
  // l'onde ne casse pas la jointure de l'anneau.
  const std::int32_t phase = UndulationPhaseDegrees(rank, track.ticks);
  const float le_long_du_temps = SinOfDegrees(phase);
  const float base = track.lift_key;
  // CE MOUVEMENT LIT LES CASES DE SA PISTE COMME DES SEGMENTS, et il les écrit
  // toutes. Un autre mouvement n'en écrira que trois, et pour autre chose.
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    const std::int32_t along_ribbon =
        static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
    const float ondulation =
        base * kUndulationAmplitude * SinOfDegrees(along_ribbon) * le_long_du_temps;
    track.value[segment] = base + ondulation;
  }
}

// CE QUE LE SOUS-TYPE 108 POSE EN DUR.
constexpr float kSpreadKeyAmplitude = 2.5F;
constexpr std::int32_t kSpreadKeyPeriodTicks = 90;
constexpr std::int32_t kSpreadKeyDegreesPerTick = 4;
// La case de QUEUE où le client range la base de la clé. Voir l'en-tête.
constexpr std::size_t kSpreadKeyBaseSlot = kRadialSpreadKeyBaseTailSlot;
// Passé ce tic, l'alpha redescend ; avant celui-là, il monte.
constexpr std::int32_t kSpreadAlphaFallsAfterTick = 110;
constexpr std::int32_t kSpreadAlphaRisesBeforeTick = 20;
constexpr std::int16_t kSpreadAlphaRisePerTick = 5;
constexpr std::int16_t kSpreadAlphaCeiling = 180;
// Le tic au-delà duquel le segment ne dépend plus du temps.
constexpr std::int32_t kSpreadTimeFactorLastTick = 90;
// La marque qui dit qu'un segment est FIGÉ à sa pleine longueur.
constexpr std::uint8_t kSpreadSlotLocked = 1;

// CE QUE LE SOUS-TYPE 118 POSE EN DUR.
//
// ⚠ LA QUATRIÈME PISTE A SES PROPRES DEUX CONSTANTES, et le client la distingue
// par une comparaison sur le rang. Lui donner celles de ses sœurs ferait quatre
// spirales identiques là où le client en montre trois plus une.
constexpr std::size_t kSpiralSingularRank = 3;
constexpr float kSpiralKeyGrowth = 1.005F;
constexpr float kSpiralKeyGrowthForSingularRank = 1.025F;
constexpr std::int16_t kSpiralDriftDegrees = 4;
constexpr std::uint32_t kSpiralDriftDraw = 3;
constexpr std::int16_t kSpiralDriftForSingularRank = 2;
constexpr std::uint32_t kSpiralDriftDrawForSingularRank = 2;
constexpr std::int32_t kSpiralAlphaRisesBeforeTick = 20;
constexpr std::int32_t kSpiralAlphaFallsAfterTick = 40;
constexpr std::int16_t kSpiralAlphaRisePerTick = 5;
constexpr std::int16_t kSpiralAlphaCeiling = 80;
constexpr std::int16_t kSpiralAlphaFallPerTick = 15;
constexpr std::int32_t kSpiralFrozenAfterTick = 90;

// CE QUE LE SOUS-TYPE 24 POSE EN DUR.
constexpr std::int16_t kArchTurnForEvenRanks = 1;
constexpr std::int16_t kArchTurnForOddRanks = 2;
constexpr std::int32_t kArchAlphaRisesBeforeTick = 100;
constexpr std::int16_t kArchAlphaRisePerTick = 1;
constexpr std::int16_t kArchAlphaCeiling = 80;
constexpr std::int16_t kArchAlphaFallPerTick = 5;
// De combien de tics avant sa fin le nœud commence à s'éteindre.
constexpr std::int32_t kArchFadeStartsBeforeDeath = 20;

}  // namespace

void AdvanceTurningArchTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int32_t life_ticks) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    const std::int16_t tour = rang % 2 == 0 ? kArchTurnForEvenRanks : kArchTurnForOddRanks;
    track.azimuth_degrees = static_cast<std::int16_t>(track.azimuth_degrees + tour);
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    if (track.ticks >= life_ticks - kArchFadeStartsBeforeDeath) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kArchAlphaFallPerTick);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    } else if (track.ticks < kArchAlphaRisesBeforeTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kArchAlphaRisePerTick);
      if (track.alpha > kArchAlphaCeiling) {
        track.alpha = kArchAlphaCeiling;
      }
    }

    const std::int32_t phase = UndulationPhaseDegrees(rang, track.ticks);
    const float respiration = 1.0F + (kUndulationAmplitude * SinOfDegrees(phase));
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      const std::int32_t along_ribbon =
          static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
      // LA HAUTEUR EN DEMI-ARCHE, que la respiration multiplie.
      const float arche = track.lift_key * SinOfDegrees(along_ribbon);
      track.value[segment] = arche * respiration;
    }
  }
}

void AdvanceSpiralParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::uint32_t& generator) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    const bool a_part = rang == kSpiralSingularRank;
    const std::uint32_t draw = a_part ? kSpiralDriftDrawForSingularRank : kSpiralDriftDraw;
    const std::int16_t step = a_part ? kSpiralDriftForSingularRank : kSpiralDriftDegrees;
    track.azimuth_degrees = static_cast<std::int16_t>(
        track.azimuth_degrees + static_cast<std::int16_t>(NextEffectNodeDraw(generator) % draw) +
        step);
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    if (track.ticks < kSpiralAlphaRisesBeforeTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kSpiralAlphaRisePerTick);
      if (track.alpha > kSpiralAlphaCeiling) {
        track.alpha = kSpiralAlphaCeiling;
      }
    } else if (track.ticks >= kSpiralAlphaFallsAfterTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kSpiralAlphaFallPerTick);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }

    track.lift_key *= a_part ? kSpiralKeyGrowthForSingularRank : kSpiralKeyGrowth;

    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      const std::int32_t along_ribbon =
          static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
      const float full = SinOfDegrees(along_ribbon) * track.lift_key;
      // ⚠ PASSÉ LE QUATRE-VINGT-DIXIÈME TIC, LE SEGMENT N'EST PLUS ÉCRIT : il
      // garde la longueur qu'il avait, et sa marque le dit. Le recalculer le
      // ferait redescendre avec le sinus du temps.
      if (track.ticks > kSpiralFrozenAfterTick) {
        track.mark[segment] = kSpreadSlotLocked;
      } else {
        track.value[segment] = full * SinOfDegrees(track.ticks);
      }
      if (track.value[segment] > full) {
        track.mark[segment] = kSpreadSlotLocked;
      }
      if (track.value[segment] < 0.0F) {
        track.value[segment] = 0.0F;
      }
    }
  }
}

void AdvanceRadialSpreadParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    track.azimuth_degrees = static_cast<std::int16_t>(WithinFullTurn(track.azimuth_degrees));
    track.tilt_degrees = static_cast<std::int16_t>(WithinFullTurn(track.tilt_degrees));
    // LA CLÉ SE RECALCULE À CHAQUE TIC, autour de ce que la queue porte.
    const std::int32_t battement = kSpreadKeyDegreesPerTick * (track.ticks % kSpreadKeyPeriodTicks);
    track.lift_key =
        (SinOfDegrees(battement) * kSpreadKeyAmplitude) + track.tail[kSpreadKeyBaseSlot];

    if (track.ticks > kSpreadAlphaFallsAfterTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha - 1);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    } else if (track.ticks < kSpreadAlphaRisesBeforeTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kSpreadAlphaRisePerTick);
      if (track.alpha > kSpreadAlphaCeiling) {
        track.alpha = kSpreadAlphaCeiling;
      }
    }

    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      // ⚠ UN SEGMENT FIGÉ NE SE RELIT PAS. Sa marque le dit, et c'est la seule
      // mémoire de cette figure.
      if (track.mark[segment] != 0) {
        continue;
      }
      const std::int32_t along_ribbon =
          static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
      const float full = track.lift_key * SinOfDegrees(along_ribbon);
      float length = full;
      if (track.ticks <= kSpreadTimeFactorLastTick) {
        length = full * SinOfDegrees(track.ticks);
      }
      if (length > full) {
        length = full;
        track.mark[segment] = kSpreadSlotLocked;
      }
      track.value[segment] = length < 0.0F ? 0.0F : length;
    }
  }
}

void AdvanceOscillatingRadiusParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                            std::int32_t life_ticks, std::uint8_t& node_scale) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    // ⚠ L'ÉCHELLE SE POSE AVANT LE RESTE, et sur le tic QUI VIENT : le client
    // incrémente le compteur puis lit son reste. La poser après la ferait
    // dépendre d'un tic de retard.
    if (const std::optional<std::uint8_t> scale = CycleScale(track.ticks + 1); scale.has_value()) {
      node_scale = *scale;
    }
    AdvanceWavingRing(track, rang, life_ticks);
  }
}

void AdvanceGravityWaveTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int32_t life_ticks) {
  // ⚠ C'EST LE CENT TRENTE-NEUF SANS SON ÉCHELLE. Le 144 reprend le corps
  // commun constante pour constante — croissance de sept centièmes, seuil de
  // quatre, rallumage à 3,37 avec une inclinaison de soixante-quatorze, chute
  // d'alpha de trois, plancher d'inclinaison à dix, l'onde battant autour de la
  // CLÉ — et la seule chose qu'il ne fait pas est de faire respirer l'échelle du
  // nœud. Lui prêter cette respiration ferait enfler un anneau que le client
  // laisse à sa taille.
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    AdvanceWavingRing(tracks[rang], rang, life_ticks);
  }
}

void AdvanceUndulatingRingTrack(EffectTrack& track, std::size_t rank, std::int32_t life_ticks) {
  // 🔴 LE SOUS-TYPE 30 ONDULE AUTOUR DE SA CLÉ, pas de son rayon. Sur le rayon,
  // les rubans du cercle d'incantation feraient quatre unités de haut au lieu
  // de vingt.
  AdvanceWavingRing(track, rank, life_ticks);
}

void AdvanceScrollingBandTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (track.active) {
      ++track.ticks;
    }
  }
}

void AdvanceDarkVeilTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.alpha < kEffectTrackAlphaMax) {
      ++track.alpha;
    }
  }
}

void AdvanceSpinningRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::int32_t node_life_ticks) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int16_t kSpinPerTick = 3;
  constexpr std::int16_t kRibbonAlphaRisePerTick = 3;
  constexpr std::int16_t kAlphaCeiling = 100;
  constexpr std::int32_t kRiseEndsAtTick = 30;
  constexpr std::int16_t kRibbonAlphaFallPerTick = 2;
  constexpr std::int32_t kFallBeginsBeforeEnd = 46;
  // L'ONDE : son amplitude, en part de la clé, et le pas angulaire qui la fait
  // serpenter d'une case à l'autre.
  constexpr float kWaveAmplitude = 0.3F;
  constexpr std::int32_t kWaveDegreesPerSlot = 9;
  // LA PHASE DE CHAQUE PISTE, et elle ne suit pas une règle unique. Les deux
  // premières ajoutent quatre-vingt-dix degrés par rang au compteur ; les deux
  // dernières ajoutent QUARANTE-CINQ par rang, puis DOUBLENT le tout — compteur
  // compris, si bien qu'elles ondulent deux fois plus vite.
  //
  // Le client tient ces deux pas séparément, et choisit selon le second. Les confondre ferait
  // onduler les quatre pistes en phase : un seul ruban épais au lieu de quatre.
  constexpr std::int32_t kPhasePerTrack = 90;
  constexpr std::int32_t kDoubledPhasePerTrack = 45;
  constexpr std::size_t kFirstDoublingTrack = 2;

  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ UNE PISTE RETARDÉE NE BOUGE PAS. Son compteur naît négatif et doit
    // d'abord remonter ; la faire vivre d'emblée la ferait onduler avant l'heure.
    if (track.ticks <= 0) {
      continue;
    }

    track.azimuth_degrees = static_cast<std::int16_t>(track.azimuth_degrees + kSpinPerTick);
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }
    if (track.ticks < kRiseEndsAtTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kRibbonAlphaRisePerTick);
      if (track.alpha > kAlphaCeiling) {
        track.alpha = kAlphaCeiling;
      }
    }
    if (track.ticks > node_life_ticks - kFallBeginsBeforeEnd) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kRibbonAlphaFallPerTick);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }

    const auto rank_int = static_cast<std::int32_t>(rang);
    const std::int32_t phase = rang >= kFirstDoublingTrack
                                   ? 2 * (kDoubledPhasePerTrack * rank_int + track.ticks)
                                   : kPhasePerTrack * rank_int + track.ticks;
    const float wave = std::sin(DegreesToRadians(
        static_cast<float>(((phase % kEffectTrackFullTurnDegrees) + kEffectTrackFullTurnDegrees) %
                           kEffectTrackFullTurnDegrees)));
    for (std::size_t case_ = 0; case_ < kEffectTrackSlots; ++case_) {
      const float le_long = std::sin(DegreesToRadians(
          static_cast<float>(kWaveDegreesPerSlot * static_cast<std::int32_t>(case_))));
      track.value[case_] = track.lift_key + track.lift_key * kWaveAmplitude * le_long * wave;
    }
  }
}

void AdvancePentagonTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int32_t kAlphaRisesUntilTick = 40;
  constexpr std::int32_t kAlphaFallsAfterTick = 140;
  constexpr std::int16_t kAlphaStep = 1;
  // LES DEUX FAMILLES DE MARQUES qui mènent l'alpha. La treize ne fait que
  // monter ; à partir de onze, on monte PUIS on descend. Sous onze, rien.
  constexpr std::uint8_t kMarkThatOnlyRises = 13;
  constexpr std::uint8_t kLowestMarkThatBreathes = 11;
  // LA RESPIRATION DU RAYON : son amplitude, et la durée d'un cycle entier.
  constexpr float kRadiusSwing = 0.5F;
  constexpr std::int32_t kBreathCycleTicks = 720;
  constexpr std::size_t kRadiusRestSlot = 0;

  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ L'AZIMUT SE RAMÈNE, IL NE S'INCRÉMENTE PAS. Lui donner un pas ferait
    // tourner une figure que le client laisse immobile.
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    const std::uint8_t mark = track.mark[0];
    if (mark == kMarkThatOnlyRises) {
      if (track.ticks <= kAlphaRisesUntilTick) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaStep);
      }
    } else if (mark >= kLowestMarkThatBreathes) {
      if (track.ticks <= kAlphaRisesUntilTick) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaStep);
      } else if (track.ticks > kAlphaFallsAfterTick) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaStep);
      }
    }
    if (track.alpha < 0) {
      track.alpha = 0;
    }

    // ⚠ UNE PISTE SUR DEUX SEULEMENT, et ce sont les rangs IMPAIRS. Les deux
    // autres gardent le rayon que le semis leur a donné, ce qui fixe l'échelle
    // du tube pendant que les impaires respirent autour.
    if (rang % 2 == 1) {
      const std::int32_t cycle_tick =
          ((track.ticks % kBreathCycleTicks) + kBreathCycleTicks) % kBreathCycleTicks;
      const float breath = std::sin(DegreesToRadians(static_cast<float>(cycle_tick / 2)));
      track.lift_key = track.value[kRadiusRestSlot] + kRadiusSwing * breath;
    }
  }
}

void AdvancePaletteBillboardTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   std::uint32_t& generator) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr float kKeyGrowthPerTick = 1.02F;
  constexpr float kKeyThatStartsTheFade = 20.0F;
  constexpr float kKeyAtRebirth = 10.0F;
  constexpr std::int16_t kBillboardAlphaFallPerTick = 3;
  constexpr std::int16_t kSweepPerTick = 1;
  // LES DEUX ALPHAS DE RENAISSANCE, et ils diffèrent : la piste qui ne mourra
  // jamais renaît deux fois plus vive que celle qui a un terme.
  constexpr std::int16_t kAlphaAtEndlessRebirth = 200;
  constexpr std::int16_t kAlphaAtMortalRebirth = 100;
  // ⚠ LA PALETTE DÉCIDE SI LA PISTE PEUT MOURIR, et c'est l'ADR 0017 : ce que la
  // figure lit comme une palette, le mouvement le lit comme un terme.
  constexpr float kPaletteThatCanDie = 2.0F;
  constexpr std::int32_t kLastTickThatRevives = 300;
  // LA TAILLE : une part fixe de la clé, et une part qui respire avec le
  // balayage.
  constexpr float kSizeFromKey = 0.9F;
  constexpr float kSizeFromSweep = 0.1F;
  constexpr std::size_t kSlotOfSize = 2;
  constexpr std::size_t kSlotOfPalette = 5;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ UNE PISTE RETARDÉE NE BOUGE PAS : son compteur naît négatif, et c'est ce
    // qui échelonne les quatre éclats d'un même nœud.
    if (track.ticks <= 0) {
      continue;
    }

    track.degrees = static_cast<std::int16_t>(track.degrees + kSweepPerTick);
    if (track.degrees >= kEffectTrackFullTurnDegrees) {
      track.degrees = static_cast<std::int16_t>(track.degrees - kEffectTrackFullTurnDegrees);
    }
    track.lift_key *= kKeyGrowthPerTick;

    if (track.lift_key > kKeyThatStartsTheFade) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kBillboardAlphaFallPerTick);
      if (track.alpha <= 0) {
        const bool mortelle = track.value[kSlotOfPalette] >= kPaletteThatCanDie;
        if (mortelle && track.ticks > kLastTickThatRevives) {
          track.alpha = 0;
          track.active = false;
        } else {
          track.lift_key = kKeyAtRebirth;
          track.azimuth_degrees =
              static_cast<std::int16_t>(NextEffectNodeDraw(generator) %
                                        static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
          track.alpha = mortelle ? kAlphaAtMortalRebirth : kAlphaAtEndlessRebirth;
        }
      }
    }

    // ⚠ LA TAILLE SE CALCULE À CHAQUE TIC, même quand la piste vient de renaître
    // ou de mourir : le client la pose après la branche, pas dedans.
    const float breath = std::sin(DegreesToRadians(static_cast<float>(track.degrees)));
    const float size = track.lift_key * kSizeFromKey + track.lift_key * breath * kSizeFromSweep;
    track.value[kSlotOfSize] = size < 0.0F ? 0.0F : size;
  }
}

void AdvanceSpiralStackTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::uint32_t& generator) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int32_t kSlowestSpin = 3;
  constexpr std::int32_t kSpinSpread = 3;
  constexpr std::int32_t kRiseEndsAtTick = 40;
  constexpr std::int32_t kOrdinaryFadeTick = 60;
  constexpr std::int32_t kLateFadeTick = 120;
  constexpr std::int16_t kAlphaStep = 1;
  // LA SIXIÈME MARQUE PORTE LE RYTHME, et ses trois valeurs ne se déduisent pas
  // du style : c'est le bâtisseur qui les pose une à une.
  constexpr std::size_t kMarkOfRhythm = 5;
  constexpr std::uint8_t kRhythmLate = 1;
  constexpr std::uint8_t kRhythmNeverFades = 10;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }

    // ⚠ TROIS À CINQ DEGRÉS, TIRÉS : le client prend un reste sur trois et y
    // ajoute trois. Un pas fixe donnerait un carrousel.
    const auto step = static_cast<std::int16_t>(
        kSlowestSpin + static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                                 static_cast<std::uint32_t>(kSpinSpread)));
    track.azimuth_degrees = static_cast<std::int16_t>(track.azimuth_degrees + step);
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    const std::uint8_t rythme = track.mark[kMarkOfRhythm];
    if (rythme != kRhythmNeverFades) {
      const std::int32_t seuil = rythme == kRhythmLate ? kLateFadeTick : kOrdinaryFadeTick;
      if (track.ticks > seuil) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaStep);
      }
    }
    // ⚠ ET LA MONTÉE SE TESTE À PART, pas en `else` : le client enchaîne les deux
    // conditions. Entre quarante et soixante tics, aucune ne s'applique — c'est
    // le palier.
    if (track.ticks <= kRiseEndsAtTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaStep);
    }
    if (track.alpha < 0) {
      track.alpha = 0;
    }
  }
}

void AdvanceBreathingQuadTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::size_t kTracksThatMove = 2;
  constexpr std::int16_t kFastSpin = 6;
  constexpr std::int16_t kSlowSpin = 3;
  constexpr std::int16_t kSlowTilt = 1;
  constexpr std::int16_t kOrdinaryTilt = 3;
  constexpr float kRadiusSwing = 0.3F;
  constexpr std::size_t kSlotOfVariant = 4;
  // LES VALEURS DE VARIANTE QUE LE CLIENT COMPARE, une à une. Ce ne sont pas des
  // seuils : il teste l'égalité exacte de flottants.
  constexpr float kVariantThatSpinsFast = 4.0F;
  constexpr float kVariantThatBreathes = 10.0F;
  constexpr float kVariantEleven = 11.0F;
  constexpr float kVariantTwelve = 12.0F;
  constexpr float kVariantThatTiltsSlowly = 2.0F;

  // ⚠ DEUX PISTES, PAS QUATRE : le compteur de boucle du client part à deux.
  const std::size_t count = std::min(kTracksThatMove, tracks.size());
  for (std::size_t rang = 0; rang < count; ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    const float variant = track.value[kSlotOfVariant];

    if (variant == kVariantThatSpinsFast) {
      track.azimuth_degrees = static_cast<std::int16_t>(track.azimuth_degrees + kFastSpin);
    } else if (variant == kVariantThatBreathes || variant == kVariantEleven ||
               variant == kVariantTwelve) {
      track.azimuth_degrees = static_cast<std::int16_t>(track.azimuth_degrees + kSlowSpin);
    }
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    if (variant == kVariantThatTiltsSlowly) {
      track.tilt_degrees = static_cast<std::int16_t>(track.tilt_degrees + kSlowTilt);
    } else if (variant == kVariantThatBreathes) {
      // ⚠ LA SEULE VARIANTE DONT LE RAYON RESPIRE, et le sens dépend du côté où
      // l'azimut se trouve : il gagne au-delà du demi-tour, il perd en deçà.
      // C'est une respiration, pas une croissance.
      track.radius_at_anchor +=
          track.azimuth_degrees > kEffectTrackHalfTurnDegrees ? kRadiusSwing : -kRadiusSwing;
    } else if (variant != kVariantEleven && variant != kVariantTwelve) {
      track.tilt_degrees = static_cast<std::int16_t>(track.tilt_degrees + kOrdinaryTilt);
    }
    if (track.tilt_degrees >= kEffectTrackFullTurnDegrees) {
      track.tilt_degrees =
          static_cast<std::int16_t>(track.tilt_degrees - kEffectTrackFullTurnDegrees);
    }
  }
}

void AdvanceConeWedgeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int32_t kRiseEndsAtTick = 10;
  constexpr std::int16_t kWideRise = 10;
  constexpr std::int16_t kWideFall = 3;
  constexpr std::int32_t kWideFallBeginsAfter = 11;
  constexpr std::int16_t kTightRise = 6;
  constexpr std::int16_t kTightFall = 1;
  constexpr std::int16_t kBurstRise = 20;
  constexpr std::int16_t kBurstFall = 15;
  constexpr std::int32_t kBurstFallBeginsAfter = 12;
  // LES DEUX FAÇONS DE CROÎTRE, et elles ne se ressemblent pas.
  constexpr float kRadiusGain = 3.0F;
  constexpr float kRadiusFactor = 1.15F;
  constexpr std::size_t kSlotOfVariant = 4;
  // LES VARIANTES QUE LE CLIENT COMPARE, une à une et par égalité exacte.
  constexpr float kVariantTwo = 2.0F;
  constexpr float kVariantSix = 6.0F;
  constexpr float kVariantThree = 3.0F;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }
    const float variant = track.value[kSlotOfVariant];

    if (variant == kVariantTwo || variant == kVariantSix) {
      if (track.ticks <= kRiseEndsAtTick) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kWideRise);
      } else if (track.ticks > kWideFallBeginsAfter) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kWideFall);
      }
      track.radius_at_anchor += kRadiusGain;
    } else if (variant == kVariantThree) {
      if (track.ticks <= kRiseEndsAtTick) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kTightRise);
      } else if (track.ticks > kWideFallBeginsAfter) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kTightFall);
      }
      track.radius_at_anchor += kRadiusGain;
    } else {
      if (track.ticks <= kRiseEndsAtTick) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kBurstRise);
      } else if (track.ticks > kBurstFallBeginsAfter) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kBurstFall);
      }
      // ⚠ MULTIPLIÉ, ET MÊME PENDANT LE PALIER : le client pose cette croissance
      // hors des branches d'alpha, si bien que le coin continue de s'ouvrir
      // pendant les deux tics où sa lumière ne bouge plus.
      track.radius_at_anchor *= kRadiusFactor;
    }
    if (track.alpha < 0) {
      track.alpha = 0;
    }
  }
}

void AdvanceGroundRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int32_t kRiseEndsAtTick = 10;
  constexpr std::int32_t kFallBeginsAfterTick = 20;
  constexpr std::int16_t kSlowRise = 12;
  constexpr std::int16_t kFastRise = 20;
  constexpr std::int16_t kFastFall = 4;
  constexpr std::int16_t kSlowFall = 2;
  // LE PAS QUI S'ACCÉLÈRE, et la borne où le glissement se referme.
  constexpr float kStepGrowth = 0.05F;
  constexpr float kSlideWrap = 200.0F;
  constexpr std::uint8_t kMarkThatAccelerates = 10;
  constexpr std::size_t kSlotOfNearEnd = 1;
  constexpr std::size_t kSlotOfStep = 3;
  constexpr std::size_t kMarkOfStyle = 0;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    const std::uint8_t mark = track.mark[kMarkOfStyle];
    const bool fast = mark >= kMarkThatAccelerates;

    if (track.ticks <= kRiseEndsAtTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + (fast ? kFastRise : kSlowRise));
    }
    // ⚠ ET LE PAS CROÎT AVANT D'ÊTRE AJOUTÉ, pour les marques rapides : le client
    // l'augmente puis s'en sert au même tic. L'ordre inverse retarderait le
    // glissement d'un tic à chaque fois.
    if (fast) {
      track.value[kSlotOfStep] += kStepGrowth;
    }
    track.value[kSlotOfNearEnd] += track.value[kSlotOfStep];
    if (track.value[kSlotOfNearEnd] >= kSlideWrap) {
      track.value[kSlotOfNearEnd] -= kSlideWrap;
    }

    if (track.ticks > kFallBeginsAfterTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha - (fast ? kSlowFall : kFastFall));
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }
  }
}

}  // namespace ro::render
