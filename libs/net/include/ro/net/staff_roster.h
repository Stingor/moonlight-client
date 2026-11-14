#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// ── LES JOUEURS EN LIGNE, pour la fenêtre du staff ──────────────────────────
//
//   CZ 0x0F67 [op][len:2][demande:2]
//   ZC 0x0F68 [op][len:2][demande:2][statut:1][page:1][pages:1]
//             puis une entrée de taille fixe par joueur
//
// Réservé aux clients ayant donné leur poignée de main, sans bit de capacité :
// la réponse ne part qu'à qui la demande. Le serveur garde au niveau de groupe
// 80, tait une session cachée d'un niveau supérieur à celui du demandeur, et
// ignore sans répondre une demande hors cadence.
//
// Une réponse tient en `pages` trames envoyées à la suite, que
// `StaffRosterAssembly` réunit. Le numéro de demande revient en écho dans
// chacune.

enum class StaffRosterStatus : std::uint8_t {
  kServed = 0,
  // Le demandeur n'est pas du staff.
  kDenied = 1,
  // Un statut que ce client ne connaît pas : il ne promet aucune liste.
  kUnknown = 0xFF,
};

enum class StaffRosterClient : std::uint8_t {
  // Aucune poignée de main acceptée.
  kNone = 0,
  // Le client d'origine et sa greffe.
  kOriginal = 1,
  kMoonclient = 2,
  // Un client que cette version ne sait pas nommer.
  kOther = 0xFF,
};

// Les genres d'action dont le serveur garde l'instant, dans l'ordre du fil.
enum class StaffActivity : std::uint8_t {
  kWalk = 0,
  // Attaque ou skill.
  kFight,
  // Item employé, équipé, retiré ou jeté.
  kItem,
  // Parole, tous canaux confondus.
  kTalk,
  // Emote, s'asseoir, se lever.
  kGesture,
  // Réponse à un dialogue de PNJ.
  kNpc,
  // Commande @.
  kCommand,
};
inline constexpr std::size_t kStaffActivityCount = 7;

struct StaffRosterState {
  bool vending = false;
  bool buying = false;
  // L'échoppe tient sans client : le joueur est hors ligne.
  bool offline_shop = false;
  bool chat_room = false;
  bool trading = false;
  bool dead = false;
  bool sitting = false;
  bool hidden = false;
  // Absence déclarée par son client.
  bool away = false;
  bool jailed = false;
  bool npc_dialog = false;
  bool storage_open = false;

  friend bool operator==(const StaffRosterState&, const StaffRosterState&) = default;
};

struct StaffRosterPlayer {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  // Les quatre textes sont en UTF-8. Guilde et groupe vides : il n'en a pas.
  std::string name;
  std::string guild;
  std::string party;
  // La carte SOURCE quand `instance_id` n'est pas nul.
  std::string map;
  std::uint16_t instance_id = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint16_t job = 0;
  std::uint16_t base_level = 0;
  std::uint16_t job_level = 0;
  std::uint8_t group_level = 0;
  StaffRosterState state;
  // Depuis le début de la session de COMPTE : un changement de personnage ne la
  // remet pas à zéro.
  std::uint32_t online_s = 0;
  // Depuis l'arrivée de ce personnage.
  std::uint32_t character_s = 0;
  // Depuis sa dernière action, tous genres confondus.
  std::uint32_t idle_s = 0;
  // L'âge de la dernière action de chaque genre, au rang de `StaffActivity` ;
  // rien tant que le personnage n'en a pas fait.
  std::array<std::optional<std::uint32_t>, kStaffActivityCount> action_age_s;
  StaffRosterClient client = StaffRosterClient::kNone;
  // La révision déclarée portait des retouches.
  bool tree_modified = false;
  // Négatif : jamais patché, ou sans poignée de main.
  std::int32_t patch_index = -1;
  // La révision que moonclient déclare ; vide pour tout autre client.
  std::string commit;
  // Le même numéro pour les joueurs de la réponse qui partagent une machine, ou
  // un compte Moonlight ; zéro pour qui est seul de son espèce.
  std::uint16_t machine_group = 0;
  std::uint16_t account_group = 0;
  // Les signalements écrits au journal du serveur depuis l'arrivée : dérive
  // d'horloge, cadence trop régulière, détecteur de son client.
  std::uint16_t cheat_clock = 0;
  std::uint16_t cheat_cadence = 0;
  std::uint16_t cheat_client = 0;
  // Minutes de silence restantes ; zéro quand il peut parler.
  std::int32_t mute_minutes = 0;
};

// Une trame de la réponse.
struct StaffRosterPage {
  std::uint16_t request_id = 0;
  StaffRosterStatus status = StaffRosterStatus::kUnknown;
  // Le rang de cette trame, depuis zéro, et le nombre de trames de la réponse.
  std::uint8_t page = 0;
  std::uint8_t pages = 0;
  // Vide hors du statut « servi ».
  std::vector<StaffRosterPlayer> players;
};

// La réponse entière.
struct StaffRosterAnswer {
  StaffRosterStatus status = StaffRosterStatus::kUnknown;
  std::vector<StaffRosterPlayer> players;
};

// Réunit les trames d'une réponse, dans l'ordre où le serveur les envoie.
class StaffRosterAssembly {
 public:
  // Attend la réponse à `request_id` ; ce qui était amassé est jeté.
  void Expect(std::uint16_t request_id);

  // Range une trame, et rend la réponse quand cette trame la termine. La trame
  // d'une autre demande est jetée ; une trame hors de son rang, ou qui annonce
  // un autre total que la première, jette aussi ce qui était amassé.
  [[nodiscard]] std::optional<StaffRosterAnswer> Take(StaffRosterPage page);

  [[nodiscard]] bool waiting() const noexcept { return expected_.has_value(); }

 private:
  std::optional<std::uint16_t> expected_;
  std::uint8_t next_page_ = 0;
  std::uint8_t pages_ = 0;
  StaffRosterAnswer amassed_;
};

// L'opcode de la réponse, pour qui doit reconnaître une trame qui ne se décode
// pas.
inline constexpr std::uint16_t kStaffRosterReplyOpcode = 0x0F68;

// « Qui est en ligne ? »
[[nodiscard]] core::Result<void> RequestStaffRoster(Connection& link, std::uint16_t request_id);

// Une trame de la réponse, ou rien quand la trame n'en est pas une ou qu'elle
// ne se découpe pas en entrées entières.
[[nodiscard]] std::optional<StaffRosterPage> DecodeStaffRoster(std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<StaffRosterPage> ReadStaffRoster(const Frame& frame);

}  // namespace ro::net
