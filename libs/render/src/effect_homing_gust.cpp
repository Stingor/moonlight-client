#include "ro/render/effect_homing_gust.h"

#include <algorithm>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"

#include "effect_ground_gust_motion.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LES DEUX MOUVEMENTS PARTAGENT.

// De combien le carré tourne par tic.
constexpr std::int32_t kTurnStepDegrees = 30;
// LE TIC OÙ LE PREMIER CAP SE PREND, depuis la place du nœud.
constexpr std::int32_t kHeadingTick = 1;
// Jusqu'à quel tic la bourrasque file TOUT DROIT : ni poursuite, ni retour, ni
// extinction avant le sixième.
constexpr std::int32_t kStraightUntilTick = 5;
// De combien le cap se corrige par tic quand il poursuit ou rentre, et à
// combien de degrés près il est tenu pour atteint.
constexpr std::int32_t kSteerStepDegrees = 5;
constexpr std::int32_t kAimToleranceDegrees = 3;
// De combien il tourne encore pendant qu'elle pâlit.
constexpr std::int32_t kFadingTurnDegrees = 1;
// De combien les quatre pistes pâlissent par tic, TOUTES ensemble.
constexpr std::int16_t kFadeStep = 5;

// LE 122.
//
// Le quart de tour dont elle part de côté.
constexpr std::int32_t kSteeredSideStartDegrees = 90;
// Son alpha monte de quinze jusqu'au vingtième tic, plafonné à deux cent
// cinquante.
constexpr std::int32_t kSteeredRiseUntilTick = 20;
constexpr std::int16_t kSteeredAlphaRise = 15;
constexpr std::int16_t kSteeredAlphaCeiling = 250;
// Quand elle renonce : après cent quarante tics, ou quatre-vingt-dix si elle
// est hâtive.
constexpr std::int32_t kSteeredLastTick = 140;
constexpr std::int32_t kSteeredEarlyLastTick = 90;
constexpr std::uint8_t kSteeredEarlyFade = 1;
// Combien de tics elle met à rentrer avant de s'éteindre.
constexpr std::int32_t kSteeredReturnLastTick = 50;

// LE 125.
//
// L'écart dont elle part : vingt-cinq degrés, plus un tirage sur vingt et un.
constexpr std::int32_t kSpreadBaseDegrees = 25;
constexpr std::int32_t kSpreadDrawSpan = 21;
// Son alpha monte de huit jusqu'au vingtième tic, plafonné à cent vingt ; au-delà, elle pâlit.
constexpr std::int32_t kSpreadRiseUntilTick = 20;
constexpr std::int16_t kSpreadAlphaRise = 8;
constexpr std::int16_t kSpreadAlphaCeiling = 120;

// LE CAP, dans le repère du CLIENT, d'un point de notre monde vers un autre.
//
// ⚠ LA PROFONDEUR SE RETOURNE : le client soustrait ses z, les nôtres sont
// opposés. Voir l'en-tête.
[[nodiscard]] std::int32_t ClientHeadingToward(glm::vec3 from, glm::vec3 to) {
  return HeadingOfVector(to.x - from.x, from.z - to.z);
}

// UN CAP D'UN REPÈRE À L'AUTRE : il se nie, dans les deux sens.
[[nodiscard]] std::int32_t ClientHeadingOf(const EffectTrack& track) {
  return WithinFullTurn(-static_cast<std::int32_t>(track.azimuth_degrees));
}

void SetClientHeading(EffectTrack& track, std::int32_t client_heading) {
  track.azimuth_degrees = static_cast<std::int16_t>(WithinFullTurn(-client_heading));
}

// UN VIRAGE, dans le repère du client : du côté que la marque dit.
void TurnClientHeading(EffectTrack& track, bool side, std::int32_t step_degrees) {
  SetClientHeading(track, ClientHeadingOf(track) + (side ? step_degrees : -step_degrees));
}

// LA CORRECTION DE CAP : cinq degrés du côté que la marque dit, sauf quand le
// cap est déjà à trois degrés du but.
//
// ⚠ LA FENÊTRE SE COMPARE SANS REPLI AU TOUR, comme le client la compare : un
// cap de 359 ne tient pas un but de 1 pour atteint. Vrai quand le but est
// atteint, et rien n'a alors tourné.
bool SteerTowards(EffectTrack& track, std::int32_t client_bearing, bool side) {
  const std::int32_t cap = ClientHeadingOf(track);
  if (cap >= client_bearing - kAimToleranceDegrees &&
      cap <= client_bearing + kAimToleranceDegrees) {
    return true;
  }
  TurnClientHeading(track, side, kSteerStepDegrees);
  return false;
}

// L'EXTINCTION : les QUATRE pistes pâlissent ensemble, et chacune meurt sous
// zéro — les échos compris, ce que le mouvement en ligne du 119 ne fait pas.
void FadeAllTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kFadeStep);
    if (effect_track.alpha < 0) {
      effect_track.alpha = 0;
      effect_track.active = false;
    }
  }
}

[[nodiscard]] std::int16_t RaisedAlpha(std::int16_t alpha, std::int16_t rise,
                                       std::int16_t ceiling) {
  return std::min<std::int16_t>(static_cast<std::int16_t>(alpha + rise), ceiling);
}

}  // namespace

void AdvanceHomingGustSteeredTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                    glm::vec3 node_position,
                                    const std::optional<glm::vec3>& target) {
  // ⚠ LA PISTE ZÉRO SEULE : la boucle du client s'arrête après elle, et les trois autres ne vivent
  // que par l'écho.
  EffectTrack& leader = tracks.front();
  if (!leader.active) {
    return;
  }
  ++leader.ticks;
  if (leader.ticks <= 0) {
    return;
  }
  std::uint8_t& state = leader.mark[kHomingGustMarkOfState];
  const bool cote = leader.mark[kHomingGustMarkOfSide] != 0;

  // L'ACTEUR INTROUVABLE : tant qu'elle poursuit, elle
  // RENTRE. ⚠ ET AU PREMIER TIC ELLE NE FAIT RIEN DU TOUT — ni rotation, ni
  // dérive : elle n'a pas encore de cap.
  if (!target.has_value() && state < kHomingGustFading) {
    state = kHomingGustReturning;
    if (leader.ticks == kHeadingTick) {
      return;
    }
  }

  leader.tilt_degrees =
      static_cast<std::int16_t>(WithinFullTurn(leader.tilt_degrees + kTurnStepDegrees));

  if (leader.ticks == kHeadingTick) {
    // ⚠ UN QUART DE TOUR DE CÔTÉ : elle part À ANGLE DROIT de sa cible, et
    // c'est la poursuite qui la ramène. `target` est là : sans lui, le tic un
    // est sorti plus haut.
    const std::int32_t vers = ClientHeadingToward(node_position, *target);
    SetClientHeading(leader, vers + (cote ? -kSteeredSideStartDegrees : kSteeredSideStartDegrees));
  } else if (leader.ticks > kSteeredRiseUntilTick) {
    const bool hative = leader.mark[kHomingGustMarkOfEarlyFade] == kSteeredEarlyFade;
    if (leader.ticks > kSteeredLastTick || (leader.ticks > kSteeredEarlyLastTick && hative)) {
      state = kHomingGustFading;
    }
  } else {
    leader.alpha = RaisedAlpha(leader.alpha, kSteeredAlphaRise, kSteeredAlphaCeiling);
  }

  if (leader.ticks > kStraightUntilTick) {
    if (state == kHomingGustChasing && target.has_value()) {
      // ⚠ LE BUT SE PREND DEPUIS L'ANCRE DE LA PISTE, non depuis le nœud : la
      // bourrasque vise l'acteur d'où ELLE est.
      static_cast<void>(SteerTowards(leader, ClientHeadingToward(leader.anchor, *target), cote));
    } else if (state == kHomingGustFading) {
      FadeAllTracks(tracks);
      TurnClientHeading(leader, cote, kFadingTurnDegrees);
    } else if (state == kHomingGustReturning) {
      // ELLE RENTRE VERS LA PLACE DU NŒUD, puis
      // renonce au cinquante et unième tic.
      static_cast<void>(
          SteerTowards(leader, ClientHeadingToward(leader.anchor, node_position), cote));
      if (leader.ticks > kSteeredReturnLastTick) {
        state = kHomingGustFading;
      }
    }
  }

  DriftGroundGustLeader(leader);
  EchoGroundGustTrail(tracks);
}

void AdvanceHomingGustSpreadTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   glm::vec3 node_position, const std::optional<glm::vec3>& target,
                                   std::uint32_t& generator) {
  EffectTrack& leader = tracks.front();
  if (!leader.active) {
    return;
  }
  ++leader.ticks;
  if (leader.ticks <= 0) {
    return;
  }
  std::uint8_t& state = leader.mark[kHomingGustMarkOfState];
  const bool cote = leader.mark[kHomingGustMarkOfSide] != 0;

  // L'ACTEUR INTROUVABLE : elle PÂLIT, quel que soit
  // son état — le 125 ne rentre jamais. Et au premier tic, rien ne bouge.
  if (!target.has_value()) {
    state = kHomingGustFading;
    if (leader.ticks == kHeadingTick) {
      return;
    }
  }

  leader.tilt_degrees =
      static_cast<std::int16_t>(WithinFullTurn(leader.tilt_degrees + kTurnStepDegrees));

  if (leader.ticks == kHeadingTick) {
    // ⚠ UN TIRAGE DANS LES DEUX BRANCHES : seul le signe de l'écart dépend du
    // côté. Ne tirer que d'un côté décalerait la suite de tous les nœuds
    // suivants.
    const std::int32_t offset =
        kSpreadBaseDegrees + static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                                       static_cast<std::uint32_t>(kSpreadDrawSpan));
    const std::int32_t vers = ClientHeadingToward(node_position, *target);
    SetClientHeading(leader, vers + (cote ? -offset : offset));
  } else if (leader.ticks > kSpreadRiseUntilTick) {
    state = kHomingGustFading;
  } else {
    leader.alpha = RaisedAlpha(leader.alpha, kSpreadAlphaRise, kSpreadAlphaCeiling);
  }

  if (leader.ticks > kStraightUntilTick) {
    if (state == kHomingGustChasing && target.has_value()) {
      // ⚠ ATTEINDRE LE BUT ÉTEINT LA BOURRASQUE : elle ne le
      // SUIT pas, elle le TROUVE et s'efface.
      if (SteerTowards(leader, ClientHeadingToward(leader.anchor, *target), cote)) {
        state = kHomingGustFading;
      }
    } else if (state == kHomingGustFading) {
      FadeAllTracks(tracks);
      TurnClientHeading(leader, cote, kFadingTurnDegrees);
    }
  }

  DriftGroundGustLeader(leader);
  EchoGroundGustTrail(tracks);
}

}  // namespace ro::render
