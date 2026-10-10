#pragma once

#include <cstdint>

namespace ro::sim {

// ── L'HORLOGE DU SERVEUR, ET L'ALLER-RETOUR QU'ELLE MESURE ──────────────────
//
// 🔴 CE FICHIER EST NÉ D'UNE COUTURE, ET LE PLAFOND N'EN EST QUE L'OCCASION.
// Caler notre horloge sur celle du serveur ne demande aucun acteur, aucune
// carte, aucun sac : c'est une mécanique fermée, de cinq nombres et de trois
// gestes. Tant qu'elle vivait dans `World`, l'éprouver demandait d'instancier
// un monde entier — et la lire demandait de la retrouver au milieu de six cents
// lignes qui parlent d'autre chose.
//
// 🔴 L'ALLER-RETOUR NE COÛTE RIEN SUR LE FIL. Le client entretient déjà un
// battement d'horloge — sans lui le serveur raccroche au bout d'une minute —,
// et le serveur y répond par `ZC_NOTIFY_TIME`, qu'il n'envoie de nulle part
// ailleurs. Chaque réponse répond donc à une demande, et il suffit de retenir
// QUAND on l'a faite.
//
// ⚠ LE PAQUET NE PORTE PAS NOTRE HEURE : le serveur y met la SIENNE, et il
// ignore celle qu'on lui a envoyée. La mesure vient de nos deux horodatages à
// nous, jamais du contenu.
//
// ⚠ ET LE CHIFFRE A QUELQUES SECONDES. Le battement n'est pas fréquent ; c'est
// une latence récente, pas instantanée.
//
// ── LE DÉCALAGE ─────────────────────────────────────────────────────────────
//
// 🔴 TOUT SE CALCULE MODULO 2³². Le serveur date ses trames avec un `gettick`
// tronqué à 32 bits, qui repasse par zéro au bout de 49,7 jours d'uptime de SA
// machine. Un décalage calculé en entiers larges devient alors immense, et
// chaque départ de marche se daterait avant l'origine : tous les marcheurs
// sauteraient à destination.
//
// 🔴 LE MINIMUM SE PREND SUR UNE FENÊTRE GLISSANTE, et non sur toute la séance.
// Le trajet le plus rapide donne la meilleure estimation, mais les deux
// horloges DÉRIVENT l'une par rapport à l'autre, et un serveur redémarré repart
// de zéro. Un minimum gardé à vie se figerait sur la première heure : les
// départs se dateraient de plus en plus tôt, et un acteur rapide serait dessiné
// en avance sur sa vraie position.
class ServerClock {
 public:
  // La durée d'une fenêtre de mesure. L'estimation retient le meilleur trajet
  // des deux dernières : assez de battements (un toutes les dix secondes) pour
  // qu'au moins un soit rapide, assez court pour suivre la dérive.
  static constexpr std::uint32_t kOffsetWindowMs = 60'000;

  // Retient qu'une demande vient de partir, à cet instant de NOTRE horloge.
  void NoteTickSent(std::uint32_t local_now_ms) noexcept;

  // Range la RÉPONSE au battement (`ZC_NOTIFY_TIME`) : elle ferme
  // l'aller-retour si une demande l'attendait, puis cale le décalage.
  void NoteServerTick(std::uint32_t server_tick, std::uint32_t local_now_ms);

  // Cale le décalage sur une trame datée qui N'EST PAS une réponse — un départ
  // de marche. ⚠ ELLE NE FERME PAS L'ALLER-RETOUR : sinon la première marche
  // reçue après l'envoi passerait pour la réponse, et la latence affichée serait
  // trop courte.
  void NoteServerTimestamp(std::uint32_t server_tick, std::uint32_t local_now_ms) noexcept;

  // Négatif tant qu'aucun échange n'a abouti. ⚠ ZÉRO SERAIT UNE MESURE — et
  // c'en est une : sur un serveur local, l'aller-retour tient sous la
  // milliseconde.
  [[nodiscard]] std::int32_t round_trip_ms() const noexcept { return round_trip_ms_; }

  // Vrai dès qu'une trame datée a permis de caler l'horloge du serveur.
  [[nodiscard]] bool known() const noexcept { return known_; }

  // L'instant local qui correspond à un instant du serveur, borné à ne pas
  // dépasser `local_now_ms` : rien ne commence dans le futur. Rend
  // `local_now_ms` tant qu'aucun décalage n'a été mesuré.
  [[nodiscard]] std::uint32_t LocalTimeOf(std::uint32_t server_tick,
                                          std::uint32_t local_now_ms) const;

 private:
  // Le décalage retenu : le meilleur de la fenêtre en cours et de la précédente.
  [[nodiscard]] std::uint32_t OffsetMs() const noexcept;

  // Ce qu'il faut AJOUTER à un instant du serveur pour obtenir le nôtre, modulo
  // 2³² : le meilleur de la fenêtre en cours, puis celui de la précédente.
  std::uint32_t window_offset_ms_ = 0;
  std::uint32_t previous_offset_ms_ = 0;
  bool has_previous_ = false;
  std::uint32_t window_started_ms_ = 0;
  bool known_ = false;
  std::uint32_t tick_sent_ms_ = 0;
  bool tick_pending_ = false;
  std::int32_t round_trip_ms_ = -1;
};

}  // namespace ro::sim
