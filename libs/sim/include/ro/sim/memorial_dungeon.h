#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ro::sim {

// ── L'INSTANCE (« MEMORIAL DUNGEON ») DU JOUEUR ─────────────────────────────
//
// Ce que le serveur a dit de l'instance de notre personnage, de notre groupe,
// de notre guilde ou de notre clan : réservée et en file, puis ouverte, avec
// ses deux échéances. HORS DU MONDE : elle survit aux changements de carte.
//
// ⚠ ELLE NE LIT AUCUNE HORLOGE : l'appelant donne l'instant, en secondes Unix
// de la machine. Les échéances du serveur sont, elles, en secondes Unix de SON
// horloge ; le détail maison (`MemorialDetails`) donne l'écart entre les deux.
// Sans lui, l'écart est supposé nul — c'est ce que fait le client d'origine.
//
// ⚠ LE SERVEUR JUGE la destruction : rien ici ne la refuse. Le détail dit
// seulement s'il l'accepterait.

enum class MemorialPhase : std::uint8_t {
  // Aucune instance.
  kNone,
  // Réservée : le serveur prépare les cartes, et la file avance.
  kWaiting,
  // Ouverte : on peut y entrer.
  kOpen,
};

// À QUI APPARTIENT L'INSTANCE, tel que le serveur l'écrit (`e_instance_mode`).
enum class MemorialOwnerKind : std::uint8_t {
  kUnknown = 0,
  kCharacter = 1,
  kParty = 2,
  kGuild = 3,
  kClan = 4,
};

// LA FIN D'UNE INSTANCE, qui s'écrit dans la conversation.
enum class MemorialEnding : std::uint8_t {
  // La réservation a été annulée avant l'ouverture.
  kReservationCancelled,
  // La durée de vie est écoulée.
  kExpired,
  // Personne n'est entré à temps.
  kEntryExpired,
  // Le propriétaire l'a détruite.
  kDestroyed,
};

// LE SERVEUR REFUSE UNE DESTRUCTION EN SILENCE : passé ce délai sans réponse,
// le bouton se rallume.
inline constexpr std::int64_t kMemorialDestroyAnswerS = 5;

// LE DÉTAIL QUE SEUL CE CLIENT REÇOIT (`ZC 0x0F40`).
struct MemorialDetails {
  // L'horloge du serveur moins celle de la machine, en secondes.
  std::int64_t server_ahead_s = 0;
  // Les durées TOTALES ; zéro : sans limite.
  std::uint32_t duration_limit_s = 0;
  std::uint32_t idle_timeout_s = 0;
  // La difficulté choisie à la création ; 1 est la difficulté de base.
  int hard_mode = 1;
  // Le mode turbo : la plupart des dialogues sont sautés.
  bool turbo = false;
  MemorialOwnerKind owner_kind = MemorialOwnerKind::kUnknown;
  // Vide quand le serveur ne l'a plus en mémoire.
  std::string owner_name;
  // Le serveur accepte qu'on la détruise depuis la fenêtre.
  bool destroyable = false;
  // Nous sommes celui qu'il laisse la détruire.
  bool may_destroy = false;
  // Nous sommes sur une carte d'instance : il refuse alors toute destruction.
  bool on_instance_map = false;
  int players_inside = 0;
};

class MemorialDungeon {
 public:
  // `ZC 0x02CB` : réservée, à cette place de la file.
  void TellCreated(std::string name, int wait_position);
  // `ZC 0x02CC` : la file avance.
  void TellWaitPosition(int wait_position);
  // `ZC 0x02CC` à 0xFFFF : la réservation tombe.
  void TellReservationCancelled();
  // `ZC 0x02CD` : ouverte. Échéances en secondes Unix du serveur ; zéro : aucune.
  void TellOpen(std::string name, std::uint32_t keep_limit_s, std::uint32_t idle_limit_s);
  // `ZC 0x02CE` de type 0 : la fermeture à vide change, ou disparaît (zéro).
  void TellIdleLimit(std::uint32_t idle_limit_s);
  // `ZC 0x02CE` des autres types : l'instance n'est plus.
  void TellEnded(MemorialEnding ending);
  // `ZC 0x0F40`. `server_now_s` est l'heure du serveur à l'envoi, `now_unix_s`
  // celle de la machine à la réception.
  void TellDetails(MemorialDetails details, std::uint32_t server_now_s, std::int64_t now_unix_s);
  // La destruction est partie : rien ne repart tant que le serveur n'a pas
  // répondu, ou `kMemorialDestroyAnswerS` durant.
  void NoteDestroyAsked(std::int64_t now_unix_s) noexcept { destroy_asked_at_s_ = now_unix_s; }
  // Tout revient à zéro, sans avis : création refusée, nouvelle partie.
  void Reset();

  [[nodiscard]] MemorialPhase phase() const noexcept { return phase_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] int wait_position() const noexcept { return wait_position_; }
  [[nodiscard]] const std::optional<MemorialDetails>& details() const noexcept { return details_; }
  [[nodiscard]] bool DestroyPending(std::int64_t now_unix_s) const noexcept;
  // Ce qui reste avant la fin de vie, borné à zéro ; rien sans échéance.
  [[nodiscard]] std::optional<std::int64_t> SecondsLeft(std::int64_t now_unix_s) const noexcept;
  // Ce qui reste avant la fermeture faute de joueurs, borné à zéro ; rien quand
  // quelqu'un est dedans ou que l'instance n'en a pas.
  [[nodiscard]] std::optional<std::int64_t> IdleSecondsLeft(std::int64_t now_unix_s) const noexcept;
  // La fin de vie, en secondes Unix de la MACHINE ; rien sans échéance.
  [[nodiscard]] std::optional<std::int64_t> LocalEnd() const noexcept;
  // AUGMENTE quand une instance paraît, s'ouvre ou change de nom : la fenêtre
  // s'ouvre alors d'elle-même. Les rappels du serveur à chaque carte n'y
  // touchent pas.
  [[nodiscard]] std::uint32_t arrival_serial() const noexcept { return arrival_serial_; }
  // Consommées.
  [[nodiscard]] std::vector<MemorialEnding> TakeEndings();

 private:
  void Arrive(std::string name, MemorialPhase phase);
  [[nodiscard]] std::optional<std::int64_t> Left(std::uint32_t limit_s,
                                                 std::int64_t now_unix_s) const noexcept;

  MemorialPhase phase_ = MemorialPhase::kNone;
  std::string name_;
  int wait_position_ = 0;
  std::uint32_t keep_limit_s_ = 0;
  std::uint32_t idle_limit_s_ = 0;
  std::optional<MemorialDetails> details_;
  // Rien tant qu'aucune destruction n'attend sa réponse.
  std::optional<std::int64_t> destroy_asked_at_s_;
  std::uint32_t arrival_serial_ = 0;
  std::vector<MemorialEnding> endings_;
};

}  // namespace ro::sim
