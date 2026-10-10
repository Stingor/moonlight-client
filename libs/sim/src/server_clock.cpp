// Le calage d'horloge et la mesure d'aller-retour. Voir l'en-tête pour la
// raison pour laquelle le paquet ne porte pas notre heure, et pour celle du
// calcul modulo 2³².

#include "ro/sim/server_clock.h"

namespace ro::sim {
namespace {

// Vrai si `left` précède `right` sur le cercle des entiers 32 bits : leur
// écart, lu en signé, est négatif. C'est la seule comparaison qui survive au
// passage par zéro d'une des deux horloges.
[[nodiscard]] constexpr bool Before(std::uint32_t left, std::uint32_t right) noexcept {
  return static_cast<std::int32_t>(left - right) < 0;
}

}  // namespace

void ServerClock::NoteTickSent(std::uint32_t local_now_ms) noexcept {
  tick_sent_ms_ = local_now_ms;
  tick_pending_ = true;
}

void ServerClock::NoteServerTick(std::uint32_t server_tick, std::uint32_t local_now_ms) {
  // ⚠ SEULEMENT SI UNE DEMANDE ATTEND. Sans ce drapeau, une réponse en retard
  // arrivée après un changement de carte se mesurerait depuis un envoi qui n'est
  // plus le sien, et rendrait une latence de plusieurs secondes.
  if (tick_pending_) {
    tick_pending_ = false;
    round_trip_ms_ = static_cast<std::int32_t>(local_now_ms - tick_sent_ms_);
  }
  NoteServerTimestamp(server_tick, local_now_ms);
}

void ServerClock::NoteServerTimestamp(std::uint32_t server_tick,
                                      std::uint32_t local_now_ms) noexcept {
  const std::uint32_t offset = local_now_ms - server_tick;
  if (!known_) {
    window_offset_ms_ = offset;
    window_started_ms_ = local_now_ms;
    has_previous_ = false;
    known_ = true;
    return;
  }
  // LA FENÊTRE SE CLÔT : son meilleur trajet devient celui de la précédente, et
  // la nouvelle repart de cette trame. C'est ce qui laisse l'estimation suivre
  // une horloge qui dérive, ou un serveur qui a redémarré.
  if (local_now_ms - window_started_ms_ >= kOffsetWindowMs) {
    previous_offset_ms_ = window_offset_ms_;
    has_previous_ = true;
    window_offset_ms_ = offset;
    window_started_ms_ = local_now_ms;
    return;
  }
  // Le trajet le plus rapide donne la meilleure estimation : tout ce qui est
  // au-dessus est de la latence, et la latence ne retranche jamais rien.
  if (Before(offset, window_offset_ms_)) {
    window_offset_ms_ = offset;
  }
}

std::uint32_t ServerClock::OffsetMs() const noexcept {
  if (has_previous_ && Before(previous_offset_ms_, window_offset_ms_)) {
    return previous_offset_ms_;
  }
  return window_offset_ms_;
}

std::uint32_t ServerClock::LocalTimeOf(std::uint32_t server_tick,
                                       std::uint32_t local_now_ms) const {
  if (!known_) {
    return local_now_ms;
  }
  const std::uint32_t local = server_tick + OffsetMs();
  // Rien ne commence dans le futur.
  if (!Before(local, local_now_ms)) {
    return local_now_ms;
  }
  // Et rien ne remonte avant l'origine de l'horloge de l'appelant.
  const std::uint32_t age_ms = local_now_ms - local;
  return age_ms > local_now_ms ? 0 : local_now_ms - age_ms;
}

}  // namespace ro::sim
