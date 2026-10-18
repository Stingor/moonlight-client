#include "ro/render/effect_track_burst.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_node.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

// LES TROIS DERNIERS MOUVEMENTS DU LOT 8, sortis de `effect_track.cpp` au
// plafond de huit cents lignes.
//
// La coupure est arbitraire — ce sont simplement les trois derniers arrivés —,
// et c'est assumé : il n'y a pas de famille commune entre un portail qui pulse,
// une colonne qui tourne et un carré qui tremble. Les ranger par « thème »
// aurait inventé une parenté que le client n'a pas.
//
// Ce qu'ils partagent vraiment tient dans `effect_track_angles.h`.

namespace ro::render {

void AdvanceRadialBurstTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT, toutes celles du client.
  //
  // LES MODES QUE LE CLIENT COMPARE, un a un et par egalite exacte. Tout ce qui
  // n'est aucun des quatre suit la branche ordinaire.
  constexpr std::int16_t kModeThatCloses = 3;
  constexpr std::int16_t kModeThatGlimmers = 4;
  constexpr std::int16_t kModeThatGlows = 5;
  constexpr std::int16_t kModeThatBursts = 1;

  constexpr float kRadiusShrinkPerTick = 0.25F;
  constexpr float kBurstRadiusGrowthPerTick = 0.5F;
  constexpr float kRadiusThreshold = 7.0F;
  // ⚠ LE MODE QUI SE REFERME RENAIT LOIN, les autres au centre. C'est ce qui
  // fait qu'un portail aspire au lieu de rayonner.
  constexpr float kRadiusAtRebirthOfClosing = 14.0F;

  constexpr std::int32_t kRiseTicksWhenClosing = 20;
  constexpr std::int32_t kRiseTicks = 10;
  constexpr std::int16_t kRiseWhenClosing = 12;
  constexpr std::int16_t kRiseWhenGlimmering = 4;
  constexpr std::int16_t kRise = 24;
  constexpr std::int16_t kCeilingWhenGlimmering = 40;
  constexpr std::int16_t kCeilingWhenGlowing = 120;
  constexpr std::int16_t kCeiling = 240;
  constexpr std::int16_t kFallWhenClosing = 7;
  constexpr std::int16_t kFallWhenGlimmering = 5;
  constexpr std::int16_t kFallWhenGlowing = 8;
  constexpr std::int16_t kFall = 15;

  // JUSQU'A QUEL AGE UNE PISTE A ENCORE LE DROIT DE RENAITRE.
  constexpr std::int32_t kShortLifeTicks = 120;
  constexpr std::int32_t kLongLifeTicks = 1400;

  // JUSQU'A QUEL TIC LE RUBAN S'OUVRE en un quart de sinus.
  constexpr std::int32_t kOpeningTicks = 10;

  EffectTrack& registre = tracks[kEffectRadialBurstRegisterTrack];
  const std::int16_t mode = registre.style;

  for (std::size_t rang = 0; rang < kEffectRadialBurstRingCount; ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ L'AGE GLOBAL AVANCE AVEC LA PREMIERE PISTE SEULE, et il avance avant
    // meme qu'on sache si elle a le droit de bouger. Le faire avancer une fois
    // par piste userait les trois anneaux en un tiers du temps.
    if (rang == 0) {
      ++registre.ticks;
    }
    if (track.ticks <= 0) {
      continue;
    }

    if (mode == kModeThatCloses) {
      track.radius_at_anchor -= kRadiusShrinkPerTick;
      if (track.radius_at_anchor < kRadiusThreshold) {
        if (track.radius_at_anchor < 0.0F) {
          track.radius_at_anchor = 0.0F;
        }
        track.alpha = static_cast<std::int16_t>(track.alpha - kFallWhenClosing);
        if (track.alpha < 0) {
          track.alpha = 0;
          if (registre.ticks <= kLongLifeTicks) {
            track.ticks = 0;
            track.radius_at_anchor = kRadiusAtRebirthOfClosing;
          } else {
            track.active = false;
          }
        }
      }
    } else {
      track.radius_at_anchor += kBurstRadiusGrowthPerTick;
      if (track.radius_at_anchor > kRadiusThreshold) {
        const std::int16_t chute = mode == kModeThatGlimmers ? kFallWhenGlimmering
                                   : mode == kModeThatGlows  ? kFallWhenGlowing
                                                             : kFall;
        track.alpha = static_cast<std::int16_t>(track.alpha - chute);
        if (track.alpha < 0) {
          track.alpha = 0;
          // ⚠ TROIS MODES VIVENT DEUX SECONDES, LES AUTRES VINGT-TROIS. Les
          // traiter pareil eteindrait un portail au premier cycle, ou ferait
          // durer un eclat dix fois trop longtemps.
          const bool short_life =
              mode == kModeThatBursts || mode == kModeThatGlimmers || mode == kModeThatGlows;
          if ((short_life && registre.ticks > kShortLifeTicks) || registre.ticks > kLongLifeTicks) {
            track.active = false;
          } else {
            track.ticks = 0;
            track.radius_at_anchor = 0.0F;
          }
        }
      }
    }

    // ⚠ LA MONTEE LIT LE COMPTEUR D'APRES LA RENAISSANCE, et non celui d'avant :
    // une piste qui vient de renaitre remonte son alpha des ce tic. Lire le
    // compteur avant laisserait un tic noir a chaque cycle.
    if (mode == kModeThatCloses) {
      if (track.ticks < kRiseTicksWhenClosing) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kRiseWhenClosing);
        track.alpha = std::min<std::int16_t>(track.alpha, kCeiling);
      }
    } else if (track.ticks < kRiseTicks) {
      if (mode == kModeThatGlimmers) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kRiseWhenGlimmering);
        track.alpha = std::min<std::int16_t>(track.alpha, kCeilingWhenGlimmering);
      } else {
        track.alpha = static_cast<std::int16_t>(track.alpha + kRise);
        track.alpha = std::min<std::int16_t>(
            track.alpha, mode == kModeThatGlows ? kCeilingWhenGlowing : kCeiling);
      }
    }

    // ⚠ LES VINGT ET UNE CASES RECOIVENT LA CLE DE LA PISTE, celle-la meme que
    // la jupe compare pour decider de s'elever : une case, deux sens.
    const float wave = track.ticks <= kOpeningTicks
                           ? SinOfDegrees(track.ticks * kEffectTrackUndulationStepDegrees)
                           : 1.0F;
    track.value.fill(track.lift_key * wave);
  }
}

void AdvanceSpiralColumnTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int32_t kSpinPerTick = 1;
  constexpr std::int32_t kFadeBeginsAtTick = 125;
  constexpr std::int16_t kAlphaFall = 15;
  constexpr float kKeyFall = 5.0F;
  // ⚠ SIX DEGRES PAR TIC : seize tics du centieme au cent quinzieme portent le
  // quart de sinus entier, et le dernier tombe pile sur quatre-vingt-dix.
  constexpr std::int32_t kOpeningStepDegrees = 6;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    track.azimuth_degrees =
        static_cast<std::int16_t>(WithinFullTurn(track.azimuth_degrees + kSpinPerTick));

    if (track.ticks >= kFadeBeginsAtTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFall);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
      track.lift_key -= kKeyFall;
      if (track.lift_key < 0.0F) {
        track.lift_key = 0.0F;
      }
    }

    // ⚠ ET LA CLE N'EST PAS UNE CLE ICI MAIS UNE HAUTEUR. Le meme champ que la
    // jupe du sous-type 63 compare a des flottants exacts porte, pour cette
    // figure, la longueur du segment — jusqu'a cent vingt. ADR 0017 sur un seul
    // flottant, et pour la seconde fois.
    float length = 0.0F;
    if (track.ticks > kEffectColumnFullAtTick) {
      length = track.lift_key;
    } else if (track.ticks >= kEffectColumnOpensAtTick) {
      length = track.lift_key *
               SinOfDegrees((track.ticks - kEffectColumnOpensAtTick) * kOpeningStepDegrees);
    }
    // Le client borne chaque case a zero apres l'avoir ecrite : une cle devenue
    // negative ne creuserait pas la colonne vers le bas.
    track.value.fill(std::max(length, 0.0F));
  }
}

void AdvanceOrbitJitterTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                              bool dives, std::uint32_t& generator) {
  // LES CONSTANTES DU MOUVEMENT.
  constexpr std::int32_t kAlphaRisesUntilTick = 200;
  // Les trois paires (courant, cible) du tremblement, et la borne haute d'un
  // angle : trois cent cinquante-neuf, jamais trois cent soixante.
  constexpr std::size_t kJitterPairs = 3;
  constexpr float kLastDegree = 359.0F;
  // Le cap de l'orbite, la hauteur et le rayon vivent dans trois cases.
  constexpr std::size_t kSlotOfOrbit = 6;
  constexpr std::size_t kSlotOfHeight = 7;
  constexpr std::size_t kSlotOfFactorKey = 10;
  // La case HUIT est celle que la figure lit comme son angle de souffle : le
  // mouvement la fait tourner, la figure en tire la respiration du rayon.
  constexpr std::size_t kSlotOfBreath = 8;
  constexpr float kFullTurn = 360.0F;
  // Les deux facons d'avancer le cap, et les deux facons de vivre.
  constexpr float kDivingOrbitStep = 15.0F;
  constexpr float kOrbitStep = 1.0F;
  constexpr float kDivingHeightFall = 0.1F;
  constexpr float kDivingHeightFloor = -30.0F;
  constexpr float kRestingHeight = -15.0F;
  constexpr float kDivingRadiusGain = 0.02F;
  constexpr float kDivingRadiusCeiling = 10.0F;
  constexpr float kRestingRadius = 6.0F;
  // ⚠ LE FACTEUR SE LIT DANS UNE CASE COMPAREE A UN FLOTTANT EXACT, comme la
  // cle de levee de la jupe d'anneau. Un de plus dans cette famille.
  constexpr float kFactorKey = 1.0F;
  constexpr float kKeyedJitter = 0.7F;
  constexpr float kKeyedOrbit = 5.5F;
  constexpr float kMarkedJitter = 0.5F;
  constexpr float kPlainJitter = 1.0F;
  constexpr float kOrbitRadius = 7.0F;
  // La case du souffle avance d'un tirage entre cinq et neuf.
  constexpr std::int32_t kSpinSpread = 5;
  constexpr std::int32_t kSpinFloor = 5;
  constexpr std::uint8_t kFirstMarkedStyle = 1;
  constexpr std::uint8_t kLastMarkedStyle = 19;
  constexpr std::uint8_t kFirstDivingMark = 10;

  // ⚠ LES DEUX ANGLES SE POSENT MEME QUAND LA PISTE DORT : le client les ecrit
  // avant de tester si elle est vive. Les poser apres laisserait la figure
  // tourner avec ceux de l'image precedente le jour ou la piste s'allume.
  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);

  EffectTrack& track = tracks[0];
  if (!track.active) {
    return;
  }
  ++track.ticks;
  if (track.ticks <= kAlphaRisesUntilTick) {
    ++track.alpha;
  }

  // LE TREMBLEMENT : trois courants qui poursuivent trois cibles.
  for (std::size_t pair = 0; pair < kJitterPairs; ++pair) {
    float& current = track.value[2 * pair];
    float& target = track.value[2 * pair + 1];
    if (target <= current) {
      current = std::max(current - 1.0F, 0.0F);
      if (target >= current) {
        const auto borne = static_cast<std::int32_t>(current);
        const std::int32_t drawn =
            borne != 0 ? static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                                   static_cast<std::uint32_t>(borne))
                       : 0;
        target = static_cast<float>(static_cast<std::int32_t>(kLastDegree) - drawn);
      }
    } else {
      current = std::min(current + 1.0F, kLastDegree);
      if (current > target) {
        const auto borne = static_cast<std::int32_t>(current);
        target = borne != 0 ? static_cast<float>(NextEffectNodeDraw(generator) %
                                                 static_cast<std::uint32_t>(borne))
                            : 0.0F;
      }
    }
  }

  const std::uint8_t mark = track.mark[0];
  const bool marquee = mark >= kFirstMarkedStyle && mark <= kLastMarkedStyle;
  bool heading_advances = true;
  if (marquee && mark >= kFirstDivingMark) {
    // ⚠ LE CAP N'AVANCE QU'UN TIC SUR (MARQUE MOINS NEUF) : plus la marque est
    // haute, plus l'orbite est lente. Le faire avancer a chaque tic ferait
    // tourner les marques hautes dix fois trop vite.
    const auto period = static_cast<std::int32_t>(mark) - 9;
    heading_advances = period > 0 && track.ticks % period == 0;
    if (dives) {
      if (heading_advances) {
        track.value[kSlotOfOrbit] += kDivingOrbitStep;
      }
      track.value[kSlotOfHeight] =
          std::max(track.value[kSlotOfHeight] - kDivingHeightFall, kDivingHeightFloor);
      track.radius_at_anchor =
          std::min(track.radius_at_anchor + kDivingRadiusGain, kDivingRadiusCeiling);
    } else {
      if (heading_advances) {
        track.value[kSlotOfOrbit] += kOrbitStep;
      }
      track.radius_at_anchor = kRestingRadius;
      track.value[kSlotOfHeight] = kRestingHeight;
    }
  } else {
    // ⚠ LES MARQUES BASSES N'AVANCENT QU'UN TIC SUR DEUX, et le client le lit
    // sur le BIT DE POIDS FAIBLE du compteur.
    if (!marquee || track.ticks % 2 == 0) {
      track.value[kSlotOfOrbit] += kOrbitStep;
    }
  }
  if (track.value[kSlotOfOrbit] >= kFullTurn) {
    track.value[kSlotOfOrbit] -= kFullTurn;
  }

  // LA POSITION : le tremblement, plus l'orbite, plus la hauteur.
  const bool keyed = track.value[kSlotOfFactorKey] == kFactorKey;
  const float tremblement = keyed ? kKeyedJitter : marquee ? kMarkedJitter : kPlainJitter;
  const float orbite = keyed ? kKeyedOrbit : kOrbitRadius;
  const auto cap = static_cast<std::int32_t>(track.value[kSlotOfOrbit]);
  track.anchor.x = SinOfDegrees(static_cast<std::int32_t>(track.value[0])) * tremblement +
                   SinOfDegrees(cap) * orbite;
  track.anchor.z = SinOfDegrees(static_cast<std::int32_t>(track.value[2])) * tremblement +
                   CosOfDegrees(cap) * orbite;
  track.anchor.y = SinOfDegrees(static_cast<std::int32_t>(track.value[4])) * tremblement +
                   track.value[kSlotOfHeight];

  track.value[kSlotOfBreath] +=
      static_cast<float>(NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kSpinSpread)) +
      static_cast<float>(kSpinFloor);
  if (track.value[kSlotOfBreath] >= kFullTurn) {
    track.value[kSlotOfBreath] -= kFullTurn;
  }
}

void AdvanceTripleWallTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LES CONSTANTES DU MOUVEMENT, celles du cas 75 du repartiteur.
  constexpr float kInnerRadiusGain = 1.0F;
  constexpr float kFadeBeginsBeyondRadius = 10.0F;
  constexpr std::int16_t kAlphaFall = 2;
  constexpr std::int16_t kAlphaRise = 5;
  // ⚠ LA MARQUE UN CHANGE QUATRE NOMBRES A LA FOIS, et aucun ne se deduit d'un
  // autre : deux pas de croissance, un plafond, un seuil de tics.
  constexpr std::uint8_t kThinMark = 1;
  constexpr float kThinThicknessGain = 1.0F;
  constexpr float kThinRiseGain = 0.2F;
  constexpr float kThinRiseCeiling = 2.0F;
  constexpr std::int32_t kThinRisesUntilTick = 5;
  constexpr float kThickThicknessGain = 2.0F;
  constexpr float kThickRiseGain = 0.4F;
  constexpr float kThickRiseCeiling = 4.0F;
  constexpr std::int32_t kThickRisesUntilTick = 7;

  constexpr std::size_t kSlotOfInnerRadius = 0;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    // LE RAYON INTERIEUR S'ELOIGNE, et l'alpha tombe des qu'il passe dix.
    track.value[kSlotOfInnerRadius] += kInnerRadiusGain;
    if (track.value[kSlotOfInnerRadius] > kFadeBeginsBeyondRadius) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFall);
      if (track.alpha < 0) {
        // ⚠ LA PISTE MEURT PAR SON ALPHA, pas par son age. La laisser vivre
        // ferait grandir un mur invisible jusqu'a la fin de l'effet.
        track.alpha = 0;
        track.active = false;
        continue;
      }
    }

    const bool mince = track.mark[0] == kThinMark;
    const float ceiling = mince ? kThinRiseCeiling : kThickRiseCeiling;
    track.radius_at_anchor += mince ? kThinThicknessGain : kThickThicknessGain;
    // LE PLAFOND SE TESTE AVANT D'AJOUTER, comme le client le fait.
    //
    // ⚠ ET LES DEUX PAS DIVISENT EXACTEMENT LEUR PLAFOND : deux dixiemes dix
    // fois font deux, quatre dixiemes dix fois font quatre. La cle tombe donc
    // PILE dessus, et le "teste avant" ne se voit pas ici. Il se verrait au
    // premier reglage qui casserait cette division — c'est pour ce jour-la que
    // l'ordre est transcrit tel quel plutot que borne apres coup.
    if (ceiling > track.lift_key) {
      track.lift_key += mince ? kThinRiseGain : kThickRiseGain;
    }

    if (track.ticks <= (mince ? kThinRisesUntilTick : kThickRisesUntilTick)) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRise);
    }
  }
}

void AdvanceBillboardSwarmTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 const EffectSwarmTiming& timing, std::int16_t camera_turn_degrees,
                                 std::int16_t camera_tilt_degrees, std::uint32_t& generator) {
  constexpr float kHeadingStep = 2.0F;
  constexpr float kTiltStep = 1.0F;
  constexpr float kHalfTurn = 180.0F;
  constexpr float kFullTurn = 360.0F;
  // LA CIBLE D'INCLINAISON se tire autour de l'HORIZONTALE : de cent
  // soixante-quinze a cent quatre-vingt-cinq degres.
  constexpr std::int32_t kTiltSpread = 11;
  constexpr float kTiltFloor = 175.0F;
  constexpr float kAlphaRise = 25.0F;
  constexpr float kAlphaFall = 3.0F;
  constexpr float kAlphaCeiling = 255.0F;
  // LES DEUX GROUPES DE CINQ CASES, et les deux alphas qui les suivent.
  constexpr std::size_t kGroups = 2;
  constexpr std::size_t kSlotsPerGroup = 5;
  constexpr std::size_t kSlotOfFirstAlpha = 12;
  // OU LES DEUX POSITIONS VIVENT : la queue pour l'une, l'ancre pour l'autre.
  constexpr std::size_t kSlotOfTailX = 3;
  constexpr std::size_t kSlotOfTailY = 4;
  constexpr std::size_t kSlotOfTailZ = 5;

  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    for (std::size_t groupe = 0; groupe < kGroups; ++groupe) {
      const std::size_t base = groupe * kSlotsPerGroup;
      float& reference = track.value[base];
      float& heading_target = track.value[base + 1];
      float& cap = track.value[base + 2];
      float& tilt_target = track.value[base + 3];
      float& tilt = track.value[base + 4];

      // ⚠ LE CHEMIN LE PLUS COURT, et c'est ce que disent les deux bornes : une
      // difference dans [0, 180[ se remonte en DESCENDANT, une difference sous
      // moins cent quatre-vingts en MONTANT. Avancer dans le sens du signe
      // ferait faire le tour long.
      const float offset = cap - heading_target;
      bool atteint = false;
      if (offset >= 0.0F && offset < kHalfTurn) {
        cap -= kHeadingStep;
        atteint = heading_target >= cap;
      } else if (offset >= -kHalfTurn) {
        cap += kHeadingStep;
        atteint = cap >= heading_target;
      } else {
        cap -= kHeadingStep;
        atteint = cap <= heading_target - kFullTurn;
      }
      if (atteint) {
        heading_target =
            reference - timing.heading_bias_degrees +
            static_cast<float>(NextEffectNodeDraw(generator) %
                               static_cast<std::uint32_t>(timing.heading_spread_degrees));
      }

      const float tilt_gap = tilt - tilt_target;
      bool tilt_reached = false;
      if (tilt_gap >= 0.0F && tilt_gap < kHalfTurn) {
        tilt -= kTiltStep;
        tilt_reached = tilt_target >= tilt;
      } else if (tilt_gap >= -kHalfTurn) {
        tilt += kTiltStep;
        tilt_reached = tilt >= tilt_target;
      } else {
        tilt -= kTiltStep;
        tilt_reached = tilt <= tilt_target - kFullTurn;
      }
      if (tilt_reached) {
        tilt_target = static_cast<float>(NextEffectNodeDraw(generator) %
                                         static_cast<std::uint32_t>(kTiltSpread)) +
                      kTiltFloor;
      }
    }

    // Les dix cases se ramenent dans le tour, negatives comprises.
    for (std::size_t rang = 0; rang < kGroups * kSlotsPerGroup; ++rang) {
      if (track.value[rang] < 0.0F) {
        track.value[rang] += kFullTurn;
      } else if (track.value[rang] >= kFullTurn) {
        track.value[rang] -= kFullTurn;
      }
    }

    // LES DEUX ALPHAS, et ils vivent le meme rythme.
    for (std::size_t rang = 0; rang < kGroups; ++rang) {
      float& alpha = track.value[kSlotOfFirstAlpha + rang];
      if (track.ticks < timing.alpha_rises_until_tick) {
        alpha = std::min(alpha + kAlphaRise, kAlphaCeiling);
      } else if (track.ticks > timing.alpha_falls_after_tick) {
        alpha -= kAlphaFall;
        if (alpha < 0.0F) {
          // ⚠ LA PISTE MEURT PAR SES ALPHAS, pas par son age.
          track.active = false;
          alpha = 0.0F;
        }
      }
    }

    // LE DEPLACEMENT DES DEUX POSITIONS, en sens vertical OPPOSES.
    //
    // ⚠ LES CAPS ET LES INCLINAISONS SONT CEUX DU CLIENT, et les deux positions
    // vivent dans NOTRE repere, ou la figure les lit et ou elles naissent : le
    // pas passe par `S`, hauteur et profondeur niees. La queue MONTE chez nous
    // de ce que le client lui retranche, l'ancre descend de ce qu'il lui ajoute.
    const float speed = track.lift_key;
    const auto cap_de_la_queue = static_cast<std::int32_t>(track.value[2]);
    const auto tail_tilt = static_cast<std::int32_t>(track.value[4]);
    track.tail[kSlotOfTailX] += speed * CosOfDegrees(cap_de_la_queue);
    track.tail[kSlotOfTailZ] -= speed * SinOfDegrees(cap_de_la_queue);
    track.tail[kSlotOfTailY] += speed * SinOfDegrees(tail_tilt);

    const auto anchor_heading = static_cast<std::int32_t>(track.value[7]);
    const auto anchor_tilt = static_cast<std::int32_t>(track.value[9]);
    track.anchor.x += speed * CosOfDegrees(anchor_heading);
    track.anchor.z -= speed * SinOfDegrees(anchor_heading);
    track.anchor.y -= speed * SinOfDegrees(anchor_tilt);
  }
}

void AdvanceSphereShellTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              float owner_facing_degrees) {
  // LA CASE QUE LA COQUILLE EXTERIEURE LIT comme son enflure.
  constexpr std::size_t kSlotOfOuterGrowth = 0;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    if (track.ticks > 1 && track.mark[0] == kSphereShellSpinningMark) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees + kSphereShellSpinPerTick);
    } else {
      // ⚠ LE CLIENT TRONQUE L'ANGLE APRES l'addition, pas avant : il ajoute les
      // trois quarts de tour au flottant du porteur et convertit le total.
      track.azimuth_degrees = static_cast<std::int16_t>(static_cast<std::int32_t>(
          owner_facing_degrees + static_cast<float>(kSphereShellFacingOffsetDegrees)));
    }
    // ⚠ UNE SEULE SOUSTRACTION, ET PAS DE MODULO : le client retranche un tour
    // quand l'azimut l'atteint, une fois. Un angle de porteur hors du tour
    // ressortirait donc hors du tour, et c'est ce qu'il fait.
    DropOneTurn(track.azimuth_degrees);

    if (track.ticks < kSphereShellAlphaRisesBeforeTick) {
      if (track.alpha < kSphereShellAlphaCeiling) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kSphereShellAlphaRise);
      }
    } else if (track.ticks > kSphereShellFadesAfterTick) {
      track.value[kSlotOfOuterGrowth] += kSphereShellGrowthPerTick;
      if (track.alpha > 0) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kSphereShellAlphaFall);
      }
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }
  }
}

void AdvanceGroundCurtainTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LA CASE QUI PORTE LE STYLE, la meme que la figure lit pour sa teinte.
  constexpr std::size_t kSlotOfStyle = 0;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    const std::int16_t step = track.value[kSlotOfStyle] == kGroundCurtainFastStyle
                                  ? kGroundCurtainFastStepDegrees
                                  : kGroundCurtainStepDegrees;
    // ⚠ UNE SEULE SOUSTRACTION, comme le client : il retranche un tour quand
    // l'angle l'atteint, et n'y revient pas.
    TurnOnward(track.degrees, step);
    track.lift_key =
        kGroundCurtainHeightAmplitude * SinOfDegrees(track.degrees) + kGroundCurtainHeightMiddle;
  }
}

void AdvanceSparkTubeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            glm::vec3 owner_position) {
  // LES DEUX CASES DE QUEUE que cette famille lit, les memes que sa figure.
  constexpr std::size_t kSlotOfPointCount = 1;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ TANT QUE LE COMPTEUR EST NEGATIF, LA PISTE NE FAIT RIEN D'AUTRE : ni
    // alpha, ni angle, ni glissement, ni meme suivre son porteur. C'est le
    // retard tire au semis.
    if (track.ticks <= 0) {
      continue;
    }
    if (track.ticks > kSparkTubeFadesAfterTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kSparkTubeAlphaFall);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }
    track.value[0] += kSparkTubeAngleStepDegrees;
    if (track.value[0] > kSparkTubeAngleCeilingDegrees) {
      track.value[0] = kSparkTubeAngleCeilingDegrees;
    }
    // ⚠ LES CASES GLISSENT VERS LE HAUT, de la derniere vers la premiere :
    // dans l'autre sens, toutes prendraient la valeur de la tete et la courbe
    // s'ecraserait en un point.
    const auto count = static_cast<std::int32_t>(track.tail[kSlotOfPointCount]);
    for (std::int32_t rang = count - 1; rang >= 0; --rang) {
      const auto source = static_cast<std::size_t>(rang);
      if (source + 1 < track.value.size()) {
        track.value[source + 1] = track.value[source];
        track.mark[source + 1] = track.mark[source];
      }
    }
    track.anchor = owner_position;
  }
}

void AdvanceGlidingSlashTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               bool aims_at_target, glm::vec3 toward_target,
                               std::uint32_t& generator) {
  // LA CASE QUI PORTE LA VITESSE, et la marque qui resserre le tir - la MEME
  // que la figure lit pour sa teinte.
  constexpr std::size_t kSlotOfSpeed = 1;
  constexpr std::size_t kSlotOfPaintMark = 1;
  constexpr std::uint8_t kMarkThatNarrows = 1;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    const std::int32_t before = track.ticks;
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }
    if (before == 0) {
      // ⚠ LE PREMIER TIC NE FAIT NI ALPHA NI VITESSE : le client range la
      // visee et le reste dans des branches EXCLUSIVES.
      if (aims_at_target) {
        const bool narrow = track.mark[kSlotOfPaintMark] == kMarkThatNarrows;
        const std::int32_t dispersion =
            narrow ? kGlidingSlashNarrowAimSpread : kGlidingSlashWideAimSpread;
        const std::int32_t recul = narrow ? kGlidingSlashNarrowAimBias : kGlidingSlashWideAimBias;
        // ⚠ L'ÉCART TIRÉ SE RETRANCHE. Ce mouvement tourne dans NOTRE repère,
        // dont la profondeur est l'opposée de celle du client : le cap vers la
        // cible y est l'opposé du sien, et l'écart que le client AJOUTE s'y
        // retranche pour que la taillade parte du même côté de la cible.
        const auto offset = static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                                      static_cast<std::uint32_t>(dispersion)) -
                            recul;
        std::int32_t cap = HeadingOfVector(toward_target.x, toward_target.z) - offset;
        // ⚠ UNE SEULE SOUSTRACTION PUIS UNE SEULE ADDITION, comme le client :
        // il retranche un tour s'il deborde, puis en ajoute un s'il est passe
        // sous zero. Un modulo donnerait le meme resultat ici, et cesserait de
        // le donner le jour ou la dispersion depasserait le tour.
        DropOneTurn(cap);
        if (cap < 0) {
          cap += kEffectTrackFullTurnDegrees;
        }
        track.azimuth_degrees = static_cast<std::int16_t>(cap);
      }
    } else if (before < kGlidingSlashRisesUntilTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kGlidingSlashAlphaRise);
      track.value[kSlotOfSpeed] *= kGlidingSlashSpeedGainWhileRising;
    } else {
      track.alpha = static_cast<std::int16_t>(track.alpha - kGlidingSlashAlphaFall);
      track.value[kSlotOfSpeed] *= kGlidingSlashSpeedGainWhileFading;
      if (track.alpha < 0) {
        // ⚠ LA PISTE MEURT PAR SON ALPHA, pas par son age.
        track.alpha = 0;
        track.active = false;
      }
    }
    const std::int32_t cap = WithinFullTurn(track.azimuth_degrees);
    track.anchor.x += track.value[kSlotOfSpeed] * CosOfDegrees(cap);
    track.anchor.z += track.value[kSlotOfSpeed] * SinOfDegrees(cap);
  }
}

void AdvanceFlyingShardTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // LA CASE QUE CETTE FAMILLE FAIT CROITRE, la meme que sa figure lit.
  constexpr std::size_t kSlotOfGrowth = 0;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ LE RAYON NE GRANDIT QUE PASSE LE RETARD ; l'alpha, lui, bouge tout de
    // suite. Les deux gardes ne sont pas la meme.
    if (track.ticks > 0) {
      track.radius_at_anchor += track.value[kSlotOfGrowth];
    }
    if (track.ticks >= kFlyingShardLightsAtTick && track.ticks <= kFlyingShardDimsAfterTick) {
      // ⚠ UN PAS UNIQUE, ET C'EST UN CHOIX : la bascule de reglage 0xE5 en
      // donne un second — HUIT au lieu de quinze — pour la marque vingt-quatre
      // seulement. Nous ne modelisons pas ce reglage et prenons la branche
      // pleine. Voir kFlyingShardReducedRise.
      track.alpha = static_cast<std::int16_t>(track.alpha + kFlyingShardAlphaRise);
      continue;
    }
    track.alpha = static_cast<std::int16_t>(track.alpha - kFlyingShardAlphaFall);
    if (track.alpha <= 0) {
      track.alpha = 0;
    }
  }
}

}  // namespace ro::render
