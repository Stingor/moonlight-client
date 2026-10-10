#include "ro/net/handshake.h"

#include <chrono>
#include <thread>

namespace ro::net {
namespace {

// Le rythme auquel on repasse. Le réseau ne va pas plus vite parce qu'on le
// harcèle, et une milliseconde rend la main au système.
constexpr int kPollIntervalMs = 1;

// Combien de codes PIN au maximum on propose en une session.
//
// Le serveur COMPTE les échecs et, passé sa propre limite, prévient le serveur
// de connexion — ce qui peut verrouiller le compte. Insister n'est donc pas
// seulement inutile, c'est nuisible.
constexpr int kMaxPincodeAttempts = 3;

}  // namespace

core::Result<PhaseState> AwaitCharacterList(CharacterPhase& phase, const PincodeProvider& pincode,
                                            std::uint32_t timeout_ms) {
  using Step = CharacterPhase::Step;

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  int attempts = 0;

  while (std::chrono::steady_clock::now() < deadline && phase.step() != Step::kAwaitingSelection) {
    if (phase.step() == Step::kAwaitingPincode) {
      if (!pincode) {
        return std::unexpected(core::Error{core::ErrorCode::kUnsupportedVersion,
                                           "le serveur demande un code PIN, et rien n'est prévu "
                                           "pour en fournir un"});
      }
      if (attempts >= kMaxPincodeAttempts) {
        return std::unexpected(core::Error{core::ErrorCode::kNotFound,
                                           "code PIN refusé ; le serveur verrouille le compte au "
                                           "bout de quelques essais, on s'arrête là"});
      }
      const std::optional<std::string> digits = pincode(phase.pincode_refused());
      if (!digits.has_value()) {
        return std::unexpected(
            core::Error{core::ErrorCode::kNotFound, "aucun code PIN fourni, connexion abandonnée"});
      }
      ++attempts;
      if (auto sent = phase.SubmitPincode(*digits); !sent.has_value()) {
        return std::unexpected(sent.error());
      }
    }

    const PhaseState state = phase.Advance();
    if (state != PhaseState::kWaiting) {
      return state;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
  }

  return phase.step() == Step::kAwaitingSelection ? PhaseState::kDone : PhaseState::kWaiting;
}

}  // namespace ro::net
