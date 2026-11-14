#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ro::test_support {

// ── LE SERVEUR DE CLASSEMENT SIMULÉ ─────────────────────────────────────────
//
// Il tient le contrat du serveur (`ro/net/survivors_ranking.h`) sans
// socket : il reçoit les octets que le client émettrait et rend les trames
// qu'un serveur rendrait. Les jetons vivent par personnage, en mémoire, et se
// perdent au redémarrage ; le score se refait par `SurvivorsScore` ; le
// tableau garde le meilleur score de chaque compte, trié par score, temps
// survécu puis ordre d'arrivée.
//
// 🔴 UNE COUPURE SE COMPTE, ELLE NE SE TOLÈRE PAS. Le « serveur d'aujourd'hui »
// ne connaît aucun opcode du service : chacun qu'il reçoit coupe la session, et
// `cuts()` le compte. Un serveur qui connaît le service coupe de même sur une
// trame d'une taille fausse. Un essai fini sans coupure prouve que le client
// n'a rien émis qu'un serveur refuserait.
//
// Comme le serveur réel, il n'entend une trame du service que d'un personnage
// qui a annoncé le bit (`unheard()` compte celles qu'il tait), et il cadence
// par seau : un jeton par 10 s, son refus « trop tôt » deux fois par seconde au
// plus (au-delà, la demande se tait), un tableau par 2 s, l'annonce du service
// deux fois par 2 s. Une demande hors cadence ne repousse pas la suivante.

// Où se tient le personnage quand il demande un jeton.
enum class SurvivorsServerPlace : std::uint8_t { kTown, kIndoor, kField };

// LES BORNES D'UNE TABLE D'ESSAI, plus larges que toute partie livrée : une
// partie qui les dépasse est invraisemblable.
struct SurvivorsServerBounds {
  std::uint32_t most_survived_steps = 0;
  std::uint32_t most_kill_points_per_second = 0;
  std::uint16_t most_boss_kills = 0;
  std::uint16_t most_level = 0;
};

// Vingt minutes de pas de 20 ms, mille points par seconde, dix boss, le
// niveau 99.
inline constexpr std::uint32_t kTestBoundSurvivedSteps = 60000;
inline constexpr std::uint32_t kTestBoundKillPointsPerSecond = 1000;
inline constexpr std::uint16_t kTestBoundBossKills = 10;
inline constexpr std::uint16_t kTestBoundLevel = 99;

// Un jeton vaut 24 h après son heure (amendement A5).
inline constexpr std::int64_t kTestTicketLifetimeS = 86400;

// CE QUE LE SERVEUR TIENT. Les quatre réglages d'amendement, tous posés par
// défaut, s'éteignent pour essayer le client contre le contrat de la
// conception seul.
struct SurvivorsRankingServerSetup {
  // Faux : le serveur d'aujourd'hui, qui coupe sur tout opcode du service.
  bool knows_service = true;
  std::uint16_t ruleset_low = 0;
  std::uint16_t ruleset_high = 0;
  // Les arènes et les classes classées ; une autre rend `result 3`.
  std::vector<std::uint16_t> arena_ids;
  std::vector<std::uint16_t> class_ids;
  SurvivorsServerBounds bounds{.most_survived_steps = kTestBoundSurvivedSteps,
                               .most_kill_points_per_second = kTestBoundKillPointsPerSecond,
                               .most_boss_kills = kTestBoundBossKills,
                               .most_level = kTestBoundLevel};
  bool idempotent_end = true;    // A2 : un `END` redit reçoit le même `RESULT`
  bool weekly_challenge = true;  // A3 : le défi se classe à la semaine
  bool indoors_accepted = true;  // A4 : un intérieur vaut une ville
  // A5 : la durée de vie d'un jeton ; 0, il ne meurt qu'au redémarrage.
  std::int64_t ticket_lifetime_s = kTestTicketLifetimeS;
};

// Un serveur qui classe la version `ruleset`, sur une arène et une classe.
[[nodiscard]] SurvivorsRankingServerSetup ServingSetup(std::uint16_t ruleset,
                                                       std::uint16_t arena_id,
                                                       std::uint16_t class_id);
// De même, sur plusieurs classes, chacune à ses tableaux.
[[nodiscard]] SurvivorsRankingServerSetup ServingSetup(std::uint16_t ruleset,
                                                       std::uint16_t arena_id,
                                                       std::vector<std::uint16_t> class_ids);
// Le serveur d'aujourd'hui.
[[nodiscard]] SurvivorsRankingServerSetup TodaySetup();

class SurvivorsRankingServer {
 public:
  using Frames = std::vector<std::vector<std::uint8_t>>;

  explicit SurvivorsRankingServer(SurvivorsRankingServerSetup setup);

  // L'horloge injectée, en millisecondes UTC.
  void SetNowMs(std::int64_t utc_ms) noexcept { now_ms_ = utc_ms; }

  // Un personnage du serveur : `user_id` nul, son compte n'est pas rattaché.
  void AddCharacter(std::uint32_t character_id, std::uint32_t user_id, std::string name);
  void SetPlace(std::uint32_t character_id, SurvivorsServerPlace place);

  // `CZ 0x0F24` : le bit du service, s'il paraît, fait répondre l'annonce.
  [[nodiscard]] Frames AnnounceCaps(std::uint32_t character_id, std::uint32_t caps);
  // Une trame du client ; les réponses, dans l'ordre.
  [[nodiscard]] Frames Receive(std::uint32_t character_id, std::span<const std::uint8_t> frame);

  // Le redémarrage : jetons et réponses gardées perdus, tableaux gardés.
  void Restart();

  [[nodiscard]] std::size_t cuts() const noexcept { return cuts_; }
  // Les demandes de tableau tues par la cadence.
  [[nodiscard]] std::size_t throttled() const noexcept { return throttled_; }
  // Les trames du service tues faute d'annonce du bit.
  [[nodiscard]] std::size_t unheard() const noexcept { return unheard_; }
  [[nodiscard]] std::size_t frames_received() const noexcept { return frames_received_; }

 private:
  // Un seau de cadence : `burst` passages d'affilée, puis un par `interval_ms`.
  // Un refus ne repousse rien.
  struct Cadence {
    std::int64_t interval_ms = 0;
    std::int64_t burst = 1;
    std::optional<std::int64_t> due_ms;
    [[nodiscard]] bool Pass(std::int64_t now_ms);
  };
  // Ce qui désigne un tableau, la semaine comprise (année ISO × 100 + semaine,
  // 0 hors défi classé à la semaine).
  struct BoardKey {
    std::uint16_t ruleset = 0;
    std::uint16_t arena_id = 0;
    std::uint16_t class_id = 0;
    std::uint8_t board = 0;
    std::uint32_t week = 0;
    [[nodiscard]] bool operator==(const BoardKey&) const = default;
  };
  struct Character {
    std::uint32_t character_id = 0;
    std::uint32_t user_id = 0;
    std::string name;
    SurvivorsServerPlace place = SurvivorsServerPlace::kTown;
    bool announced = false;  // le bit du service vu à la dernière annonce
    // Le jeton ouvert, un seul : une demande neuve remplace le précédent.
    std::optional<std::uint32_t> ticket;
    std::int64_t issued_at_s = 0;
    BoardKey key;
    Cadence ticket_cadence;
    Cadence refusal_cadence;
    Cadence board_cadence;
    Cadence service_cadence;
    // A2 : le dernier jeton consommé et la réponse rendue.
    std::optional<std::uint32_t> consumed_ticket;
    std::vector<std::uint8_t> consumed_reply;
  };
  // Le meilleur score d'un compte sur un tableau.
  struct BestRow {
    std::uint32_t user_id = 0;
    std::string name;
    BoardKey key;
    std::uint64_t score = 0;
    std::uint32_t survived_steps = 0;
    std::uint16_t level = 0;
    std::uint64_t arrival = 0;
  };

  // L'ordre du tableau : le score, puis le temps survécu, puis l'arrivée.
  [[nodiscard]] static bool Ahead(const BestRow& left, const BestRow& right) noexcept;
  [[nodiscard]] Character* Find(std::uint32_t character_id);
  [[nodiscard]] Frames Begin(Character& who, std::span<const std::uint8_t> frame);
  [[nodiscard]] Frames End(Character& who, std::span<const std::uint8_t> frame);
  [[nodiscard]] Frames Board(Character& who, std::span<const std::uint8_t> frame);
  [[nodiscard]] std::int64_t now_s() const noexcept;
  [[nodiscard]] std::uint32_t WeekOf(std::uint8_t board, std::int64_t utc_s) const;

  SurvivorsRankingServerSetup setup_;
  std::vector<Character> characters_;
  std::vector<BestRow> best_;
  std::int64_t now_ms_ = 0;
  std::uint32_t next_ticket_ = 1;
  std::uint64_t next_arrival_ = 0;
  std::size_t cuts_ = 0;
  std::size_t throttled_ = 0;
  std::size_t unheard_ = 0;
  std::size_t frames_received_ = 0;
};

}  // namespace ro::test_support
