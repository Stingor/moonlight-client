#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── « JOUEURS EN LIGNE », POUR LE STAFF ─────────────────────────────────────
//
// Ce que `@users`, `@who` et `@client` disent, réuni : qui est en ligne, où,
// depuis quand, ce qu'il vient de faire, avec quel client, et ce qui mérite un
// regard. La liste arrive du serveur telle quelle ; la fenêtre la trie, la
// filtre et la met en mots, et ne rend que deux intentions — redemander, et
// ouvrir le menu d'un joueur.

inline constexpr const char* kStaffRosterWindowId = "staff_roster";

// Où en est la demande.
enum class StaffRosterPhase : std::uint8_t {
  // Partie, sans liste à montrer encore.
  kAsking,
  // Partie depuis trop longtemps : le serveur ne connaît peut-être pas le
  // paquet.
  kSilent,
  // Le serveur refuse : ce compte n'est pas du staff.
  kDenied,
  // Le serveur a répondu par un statut que ce client ne connaît pas.
  kUnreadable,
  kReady,
};

enum class StaffRosterClientKind : std::uint8_t {
  // Aucune poignée de main acceptée.
  kNone,
  kOriginal,
  kMoonclient,
  // Un client que cette version ne sait pas nommer.
  kOther,
};

// Les genres d'action, au rang de `StaffRosterRow::action_age_s`.
enum class StaffRosterActivity : std::uint8_t {
  kWalk,
  kFight,
  kItem,
  kTalk,
  kGesture,
  kNpc,
  kCommand,
};
inline constexpr std::size_t kStaffRosterActivityCount = 7;

struct StaffRosterRow {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string name;
  // Vides : il n'en a pas.
  std::string guild;
  std::string party;
  // Le nom du job, déjà résolu ; vide quand la table ne le nomme pas.
  std::string job_name;
  int base_level = 0;
  int job_level = 0;
  // La carte source quand `instance_id` n'est pas nul.
  std::string map;
  int instance_id = 0;
  int cell_x = 0;
  int cell_y = 0;
  int group_level = 0;
  // Le niveau de groupe est celui du staff.
  bool staff = false;

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

  // Depuis le début de la session de compte, depuis l'arrivée de ce
  // personnage, et depuis sa dernière action.
  std::uint32_t online_s = 0;
  std::uint32_t character_s = 0;
  std::uint32_t idle_s = 0;
  // L'âge de la dernière action de chaque genre ; rien tant qu'il n'y en a pas
  // eu.
  std::array<std::optional<std::uint32_t>, kStaffRosterActivityCount> action_age_s;

  StaffRosterClientKind client = StaffRosterClientKind::kNone;
  // Négatif : jamais patché.
  int patch_index = -1;
  std::string revision;
  bool tree_modified = false;
  // Son patch est en deçà du plus récent connu pour ce client.
  bool client_outdated = false;
  // Un moonclient au même patch que le nôtre, d'une autre révision.
  bool other_revision = false;

  // Le même numéro pour ceux qui partagent une machine, ou un compte
  // Moonlight ; zéro pour qui est seul.
  int machine_group = 0;
  int account_group = 0;

  int cheat_clock = 0;
  int cheat_cadence = 0;
  int cheat_client = 0;
  // Minutes de silence restantes.
  int mute_minutes = 0;
};

struct StaffRosterView {
  StaffRosterPhase phase = StaffRosterPhase::kAsking;
  std::vector<StaffRosterRow> rows;
  // Une demande est en route : la liste montrée est la précédente.
  bool refreshing = false;
  // L'âge de la liste montrée.
  std::uint32_t age_s = 0;
  // Le délai entre deux demandes est écoulé.
  bool can_refresh = true;
  // La cadence de l'actualisation automatique, que sa case annonce.
  std::uint32_t refresh_period_s = 0;
};

enum class StaffRosterTab : std::uint8_t {
  kPlayers,
  // Par carte, comme `@users`.
  kMaps,
};

struct StaffRosterFilters {
  bool staff_only = false;
  bool hide_offline_shops = true;
  bool idle_only = false;
  // Ceux qui partagent une machine ou un compte Moonlight.
  bool shared_only = false;
  // Ceux qui portent un signal : voir `RowIsFlagged`.
  bool flagged_only = false;

  friend bool operator==(const StaffRosterFilters&, const StaffRosterFilters&) = default;
};

// Les colonnes de la vue « Joueurs », dans leur ordre d'affichage.
enum class StaffRosterColumn : int {
  kPlayer,
  kJob,
  kLevel,
  kMap,
  kGuild,
  kOnline,
  kIdle,
  kAction,
  kClient,
};
inline constexpr int kStaffRosterColumnCount = 9;

inline constexpr std::size_t kStaffRosterSearchBytes = 64;

struct StaffRosterWindowState {
  bool open = false;
  // Mise au premier plan à la prochaine image.
  bool raise = false;
  StaffRosterTab tab = StaffRosterTab::kPlayers;
  std::array<char, kStaffRosterSearchBytes> search{};
  StaffRosterFilters filters;
  bool auto_refresh = true;
  // Les rangs des lignes montrées, dans l'ordre du tri : refait à chaque image.
  std::vector<std::size_t> shown_order;
};

struct StaffRosterAnswer {
  // Redemander la liste au serveur.
  bool refresh = false;
  // Le joueur dont le menu est demandé.
  std::optional<std::uint32_t> menu_account_id;
  // Le serveur ne répond pas : envoyer `@users`, dont la réponse s'écrit dans
  // la conversation.
  bool say_command = false;
};

// ── LA LOGIQUE DE LA LISTE, sans dessin ─────────────────────────────────────

// Au-delà, un joueur compte parmi les inactifs.
inline constexpr std::uint32_t kStaffRosterIdleSeconds = 300;

// Une durée en clair : « 12 s », « 18 min », « 3 h 02 », « 2 j 04 h ».
[[nodiscard]] std::string StaffRosterDurationText(std::uint32_t seconds);

// Le genre de sa dernière action, tant qu'il en a fait une.
[[nodiscard]] std::optional<StaffRosterActivity> FreshestActivity(const StaffRosterRow& row);

// Inactif depuis `kStaffRosterIdleSeconds`. Une échoppe hors ligne n'est pas un
// joueur inactif : personne ne la tient.
[[nodiscard]] bool RowIsIdle(const StaffRosterRow& row);

// Il partage sa machine ou son compte Moonlight avec un autre joueur en ligne.
[[nodiscard]] bool RowIsShared(const StaffRosterRow& row);

// La ligne porte un signal : un indice de triche, un silence, la prison, un
// client sans poignée de main ou bâti sur un arbre retouché.
[[nodiscard]] bool RowIsFlagged(const StaffRosterRow& row);

// La ligne passe les filtres, et `needle` se lit, à la casse près, dans son
// nom, sa guilde, son groupe, sa carte ou son job. Une recherche vide retient
// tout.
[[nodiscard]] bool RowMatches(const StaffRosterRow& row, std::string_view needle,
                              const StaffRosterFilters& filters);

// Les rangs des lignes retenues, triés sur `column`, le nom départageant les
// égaux.
[[nodiscard]] std::vector<std::size_t> OrderedRows(std::span<const StaffRosterRow> rows,
                                                   std::string_view needle,
                                                   const StaffRosterFilters& filters,
                                                   StaffRosterColumn column, bool descending);

// Les noms des autres joueurs du même groupe de machine, ou de compte, que la
// ligne `rank`.
[[nodiscard]] std::vector<std::string_view> MachineMates(std::span<const StaffRosterRow> rows,
                                                         std::size_t rank);
[[nodiscard]] std::vector<std::string_view> AccountMates(std::span<const StaffRosterRow> rows,
                                                         std::size_t rank);

struct StaffRosterSummary {
  // Les joueurs tenus par un client, sans les échoppes hors ligne.
  int players = 0;
  int offline_shops = 0;
  // Les cartes où se trouve au moins un joueur ou une échoppe ; chaque instance
  // compte pour elle-même.
  int maps = 0;
  int outdated_clients = 0;
  int flagged = 0;
};
[[nodiscard]] StaffRosterSummary SummaryOf(std::span<const StaffRosterRow> rows);

// Une carte de la vue « Cartes », avec les rangs de ses lignes.
struct StaffRosterMapGroup {
  std::string_view map;
  int instance_id = 0;
  int players = 0;
  int offline_shops = 0;
  std::vector<std::size_t> rows;
};

// Les cartes des lignes de `order`, la plus peuplée d'abord ; à égalité, par
// nom. Chaque carte garde ses lignes dans l'ordre de `order`.
[[nodiscard]] std::vector<StaffRosterMapGroup> MapGroupsOf(std::span<const StaffRosterRow> rows,
                                                           std::span<const std::size_t> order);

// Dessine la fenêtre. Fermée, elle ne dessine rien et ne rend rien.
[[nodiscard]] StaffRosterAnswer DrawStaffRosterWindow(StaffRosterWindowState& state,
                                                      const StaffRosterView& view,
                                                      const SkinArt& art);

}  // namespace ro::ui
