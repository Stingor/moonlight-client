#include "ro/render/ailment_sprites.h"

namespace ro::render {

CurseMarkPlacement CurseMarkFor(std::size_t screen_direction) noexcept {
  const bool in_front = screen_direction >= kCurseMarkFrontFirstDirection &&
                        screen_direction <= kCurseMarkFrontLastDirection;
  const float lead = in_front ? kCurseMarkLeadLayers : -kCurseMarkLeadLayers;
  return CurseMarkPlacement{screen_direction, lead * kLayerDepthStep};
}

AilmentReactions ReactToAilments(const AilmentChange& change) {
  AilmentReactions reactions;
  reactions.blind_seen = change.blind_seen;
  reactions.poison_seen = change.poison_seen;

  if (change.body_before != change.body_after) {
    // CE QUE LE CORPS QUITTE, d'abord : l'éclat, son bruit et les retraits
    // partent même hors du direct.
    switch (change.body_before) {
      case kAilmentBodyStone:
        reactions.sounds.push_back(kStoneBreakSound);
        break;
      case kAilmentBodyFrozen:
        reactions.shatter = true;
        reactions.sounds.push_back(kThawSound);
        break;
      case kAilmentBodyBurning:
        reactions.lifted_effects.push_back(kBurningEffect);
        break;
      case kAilmentBodyImprisoned:
        reactions.lifted_effects.push_back(kImprisonEffect);
        break;
      default:
        break;
    }
    // PUIS CE QU'IL PREND : les effets toujours, les bruits en direct seulement.
    switch (change.body_after) {
      case kAilmentBodyFrozen:
        if (change.live) {
          reactions.sounds.push_back(kFreezeSound);
        }
        break;
      case kAilmentBodyStunned:
        if (change.live) {
          reactions.sounds.push_back(kStunSound);
        }
        break;
      case kAilmentBodyBurning:
        reactions.posed_effects.push_back(kBurningEffect);
        break;
      case kAilmentBodyImprisoned:
        if (!change.imprison_carried) {
          reactions.posed_effects.push_back(kImprisonEffect);
        }
        break;
      default:
        break;
    }
  }

  const auto carries = [&change](std::uint16_t bit) { return (change.health_after & bit) != 0; };

  // LE POISON, mortel ou non : le bruit en direct, la brume sur le joueur.
  if (carries(kAilmentHealthDeadlyPoison) || carries(kAilmentHealthPoison)) {
    if (change.own_player) {
      reactions.poison_seen = true;
    }
    if (change.live) {
      reactions.sounds.push_back(kPoisonSound);
      if (change.own_player && !change.poison_mist_carried) {
        reactions.posed_effects.push_back(kPoisonMistEffect);
      }
    }
  } else if (change.own_player) {
    if (change.poison_seen) {
      reactions.lifted_effects.push_back(kPoisonMistEffect);
    }
    reactions.poison_seen = false;
  }

  if (change.live && carries(kAilmentHealthCurse)) {
    reactions.sounds.push_back(kCurseSound);
  }
  if (change.live && carries(kAilmentHealthConfusion)) {
    reactions.sounds.push_back(kConfusionSound);
  }

  if (carries(kAilmentHealthBlind)) {
    if (change.own_player) {
      reactions.blind_seen = true;
    }
    if (change.live) {
      reactions.sounds.push_back(kBlindSound);
      if (change.own_player && !change.darkness_carried) {
        reactions.posed_effects.push_back(kBlindDarknessEffect);
      }
    }
  } else if (change.own_player && !change.deep_blind_held) {
    if (change.blind_seen) {
      reactions.lifted_effects.push_back(kBlindDarknessEffect);
    }
    reactions.blind_seen = false;
  }
  return reactions;
}

std::optional<std::array<std::uint8_t, 4>> AilmentTint(std::uint16_t body_state,
                                                       std::uint16_t health_state,
                                                       bool poison_tint_yields) noexcept {
  if ((health_state & kAilmentHealthCurse) != 0) {
    return kCurseTint;
  }
  if ((health_state & kAilmentHealthDeadlyPoison) != 0) {
    return kDeadlyPoisonTint;
  }
  if ((health_state & kAilmentHealthPoison) != 0 && !poison_tint_yields) {
    return kPoisonTint;
  }
  switch (body_state) {
    case kAilmentBodyStone:
      return kStoneTint;
    case kAilmentBodyPetrifying:
      return kPetrifyingTint;
    case kAilmentBodyFrozen:
      return kFrozenTint;
    default:
      return std::nullopt;
  }
}

void LatchStateChange(AilmentTintLatch& latch, std::uint16_t body_before, std::uint16_t body_after,
                      std::uint16_t health_state, bool poison_tint_yields) noexcept {
  if (body_before != body_after) {
    // LE DÉGEL BLANCHIT, et lui seul parmi les corps quittés.
    if (body_before == kAilmentBodyFrozen) {
      latch.tint = kUntinted;
    }
    switch (body_after) {
      case kAilmentBodyStone:
        latch.tint = kStoneTint;
        break;
      case kAilmentBodyFrozen:
        latch.tint = kFrozenTint;
        break;
      case kAilmentBodyPetrifying:
        latch.tint = kPetrifyingTint;
        break;
      default:
        break;
    }
  }
  const auto carries = [health_state](std::uint16_t bit) { return (health_state & bit) != 0; };
  if (carries(kAilmentHealthDeadlyPoison)) {
    latch.tint = kDeadlyPoisonTint;
  } else if (carries(kAilmentHealthPoison) && !poison_tint_yields) {
    latch.tint = kPoisonTint;
  }
  if (carries(kAilmentHealthCurse)) {
    latch.tint = kCurseTint;
  }
  // 🔴 BLANCHIR N'A LIEU QUE SI RIEN NE TIENT. Le poison ordinaire compte même
  // quand il n'a rien écrit.
  const bool body_holds = body_after == kAilmentBodyStone || body_after == kAilmentBodyFrozen ||
                          body_after == kAilmentBodyPetrifying;
  const bool health_holds = carries(kAilmentHealthPoison) || carries(kAilmentHealthCurse) ||
                            carries(kAilmentHealthDeadlyPoison);
  bool handicap_holds = false;
  for (const bool held : latch.handicaps) {
    handicap_holds = handicap_holds || held;
  }
  if (!body_holds && !health_holds && !latch.blade_stop && !handicap_holds) {
    latch.tint = kUntinted;
  }
}

void LatchHandicap(AilmentTintLatch& latch, std::size_t slot, bool active) noexcept {
  if (slot >= kTintingHandicaps) {
    return;
  }
  latch.handicaps[slot] = active;
  if (active) {
    latch.tint = kTintingHandicapTints[slot];
  }
}

void LatchBladeStop(AilmentTintLatch& latch, bool active) noexcept {
  latch.blade_stop = active;
  latch.tint = active ? kBladeStopTint : kUntinted;
}

std::optional<std::array<std::uint8_t, 4>> LatchedTint(const AilmentTintLatch& latch) noexcept {
  if (latch.tint == kUntinted) {
    return std::nullopt;
  }
  return latch.tint;
}

}  // namespace ro::render
