#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_team_play.h"

namespace ro::sim {

// ── LE LOCKSTEP D'UNE PARTIE COOP, sans fil ni horloge ─────────────────────
//
// Chaque client simule toute la partie. Il n'échange que ses entrées, datées d'un
// tic, et un tic ne s'exécute que lorsque les entrées de tous sont arrivées.
//
// UNE TRAME : [through_tick:4][count:1] puis `count` entrées des tics
// (through_tick − count + 1 … through_tick), chacune [cap:1][bulletin:1], et
// éventuellement [digest_tick:4][digest:8]. Tout est petit-boutiste.
//
// DEUX CHEMINS, DEUX RÔLES. Seul le chemin fiable, qui livre à tous les mêmes trames
// dans le même ordre, fait avancer la partie confirmée : `Receive`. Le chemin rapide
// peut perdre, doubler ou devancer : il ne sert qu'à mieux prédire le présent, `Hear`.
// Deux clients ne peuvent donc pas confirmer des entrées différentes.

// Les tics déjà envoyés qu'une trame redit, pour le chemin rapide. Une trame porte
// en plus tous les tics que nulle autre n'a portés, si nombreux soient-ils.
inline constexpr std::size_t kLockstepRedundantTicks = 10;
// Un bulletin absent, sur le fil.
inline constexpr std::uint8_t kLockstepNoBallot = 0xFF;
// Une trame part tous ces tics.
inline constexpr std::uint32_t kLockstepFrameEveryTicks = 2;
// Une empreinte part tous ces tics.
inline constexpr std::uint32_t kLockstepDigestEveryTicks = 50;
// Le client ne se donne pas plus de cette avance sur le dernier tic confirmé :
// au-delà, il attend les autres.
inline constexpr std::uint32_t kLockstepMostLeadTicks = 25;
// Les entrées d'un autre joueur gardées en attente de leur tic : le double de ce
// que son avance permet à un client honnête. Au-delà, elles s'ignorent.
inline constexpr std::size_t kLockstepMostPendingTicks = std::size_t{kLockstepMostLeadTicks} * 8U;
// Une trame ne dépasse pas ces octets : le contrat du serveur.
inline constexpr std::size_t kLockstepMostFrameBytes = 256;

// Une trame lue. Rien si elle est mal formée.
struct LockstepFrame {
  std::uint32_t through_tick = 0;
  std::vector<SurvivorsTeamInput> inputs;
  std::optional<std::uint32_t> digest_tick;
  std::uint64_t digest = 0;
};
[[nodiscard]] std::optional<LockstepFrame> DecodeLockstepFrame(std::span<const std::uint8_t> bytes);

// LA PART D'UN JOUEUR dans le salon : ce qu'il apporte à la partie, et la version
// des règles qu'il joue. Deux clients aux règles différentes divergeraient sans
// erreur : ils ne partent pas ensemble.
struct SurvivorsCoopPart {
  std::uint16_t ruleset_version = 0;
  std::uint64_t rules_digest = 0;
  std::uint16_t arena_id = 0;
  std::uint8_t cursed_water = 0;
  SurvivorsAllyStart player;
  [[nodiscard]] bool operator==(const SurvivorsCoopPart&) const = default;
};
// L'empreinte des règles : ce qui fixe une partie, classes, skills, vagues et échelle.
[[nodiscard]] std::uint64_t SurvivorsRulesDigest(const SurvivorsRules& rules);
[[nodiscard]] std::vector<std::uint8_t> EncodeSurvivorsCoopPart(const SurvivorsCoopPart& part);
// Rend la part et ce qui la suit (`trailing_bytes`) : l'apparence, qu'app lit.
[[nodiscard]] std::optional<SurvivorsCoopPart> DecodeSurvivorsCoopPart(
    std::span<const std::uint8_t> bytes, std::size_t* trailing_bytes);
// LE DÉPART DE L'ÉQUIPE : la graine du serveur, l'arène du premier, et la Cursed
// Water la plus basse, pour que personne ne subisse un monde plus dur qu'il ne l'a
// choisi. Rien pour un mélange de règles.
[[nodiscard]] std::optional<SurvivorsStart> TeamStartOf(std::uint64_t seed,
                                                        const SurvivorsRules& rules,
                                                        std::span<const SurvivorsCoopPart> parts);

// L'ÉTAT D'UNE PARTIE COOP SUR CE CLIENT.
//
// `confirmed` est la vérité : elle n'avance que par des entrées connues de tous.
// `present` est ce que le joueur voit : `confirmed`, plus les tics dont seule son
// entrée est connue, en supposant que les autres tiennent leur dernier cap. Son
// héros répond donc dès la touche, et un écart se corrige à l'image suivante, sans
// geler personne. Rien de `present` ne revient dans `confirmed`.
class SurvivorsMatch {
 public:
  // `input_delay_ticks` : l'entrée tapée au tic t vaut pour le tic t + délai.
  SurvivorsMatch(SurvivorsRun run, std::size_t local_player, std::uint32_t input_delay_ticks);

  // UNE ENTRÉE LOCALE, une par tic. Faux si le client a trop d'avance : il attend.
  [[nodiscard]] bool PushLocal(const SurvivorsTeamInput& input);
  // LA TRAME À ENVOYER, ou rien si ce tic n'en appelle pas. La partie finie, la
  // dernière entrée part sans attendre son tour : plus aucune ne la suivra.
  [[nodiscard]] std::optional<std::vector<std::uint8_t>> TakeFrame();
  // UNE TRAME DU CHEMIN FIABLE, d'un autre joueur. Faux si elle est mal formée. Une
  // entrée déjà connue s'ignore.
  bool Receive(std::size_t player, std::span<const std::uint8_t> bytes);
  // UNE TRAME DU CHEMIN RAPIDE : le dernier cap de ce joueur, pour le présent. Elle
  // ne confirme rien.
  void Hear(std::size_t player, std::span<const std::uint8_t> bytes);
  // UN JOUEUR QUI PART, annoncé par le chemin fiable après toutes ses trames : son
  // héros passe au robot au tic qui suit sa dernière entrée connue. Tous les clients
  // ont reçu les mêmes : c'est le même tic partout.
  void Leave(std::size_t player);
  // Exécute tous les tics dont les entrées sont connues, puis recalcule `present`.
  void Advance();

  [[nodiscard]] const SurvivorsRun& confirmed() const noexcept { return play_.run(); }
  [[nodiscard]] const SurvivorsRun& present() const noexcept { return *present_; }
  [[nodiscard]] const SurvivorsTeamJournal& journal() const noexcept { return play_.journal(); }
  [[nodiscard]] SurvivorsNews TakeNews() { return std::exchange(news_, SurvivorsNews{}); }
  // Une empreinte reçue diffère de la nôtre au même tic : la partie a divergé.
  [[nodiscard]] bool diverged() const noexcept { return diverged_; }
  // Le tic qui attend les entrées d'un autre depuis `waiting_ticks` tics locaux.
  [[nodiscard]] std::uint32_t lead_ticks() const noexcept;
  [[nodiscard]] std::size_t local_player() const noexcept { return local_player_; }
  // Le bulletin déposé par chaque héros au vote ouvert.
  [[nodiscard]] bool left(std::size_t player) const noexcept;

 private:
  struct Player {
    std::deque<SurvivorsTeamInput> pending;  // à partir de `first_tick`
    std::uint32_t first_tick = 0;
    std::uint32_t known_through = 0;  // les tics < à ceci sont connus
    std::uint8_t last_heading = kNoHeading;
    std::optional<std::uint32_t> heard_tick;  // le dernier tic du chemin rapide
    std::uint8_t heard_heading = kNoHeading;
    std::optional<std::uint32_t> robot_from;  // le robot joue à partir de ce tic
    std::optional<SurvivorsBot> robot;
  };
  [[nodiscard]] bool TickReady(std::uint32_t tick) const noexcept;
  void RunConfirmedTick();
  void RecomputePresent();
  void CheckDigests();

  SurvivorsTeamPlay play_;
  std::optional<SurvivorsRun> present_;
  SurvivorsNews news_;
  std::size_t local_player_ = 0;
  std::array<Player, kMostHeroes> players_;
  std::uint32_t next_local_tick_ = 0;    // le tic que la prochaine entrée locale vaudra
  std::deque<SurvivorsTeamInput> sent_;  // les dernières entrées locales, pour la redondance
  std::uint32_t framed_next_tick_ = 0;   // le premier tic qu'aucune trame n'a porté
  std::uint32_t frame_due_tick_ = 0;
  // Nos empreintes des derniers tics, et celles reçues des autres.
  struct DigestAt {
    std::uint32_t tick = 0;
    std::uint64_t digest = 0;
  };
  std::deque<DigestAt> own_digests_;
  std::deque<DigestAt> their_digests_;
  std::optional<DigestAt> digest_to_send_;
  bool diverged_ = false;
};

}  // namespace ro::sim
