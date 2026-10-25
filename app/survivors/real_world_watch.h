#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace moonclient {

// LE MONDE RÉEL PENDANT UNE PARTIE DE SURVIVORS. Le personnage connecté reste
// en ligne et immobile 15 minutes : la partie ne commence que là où il ne
// risque rien (U3), et s'interrompt dès que le monde réel le réclame (U5).
// Tout se décide ici sur des RELEVÉS, des faits que app compose depuis le
// monde ; rien ne lit le monde lui-même, et tout s'essaie sans jeu.

// ─── La garde d'entrée (U3) ────────────────────────────────────────────────

// Un monstre hostile plus près que ceci du personnage refuse la partie. La
// distance est celle des portées du jeu : le plus grand des deux écarts de
// cases.
inline constexpr std::uint32_t kHostileGuardCells = 14;

// Une case du sol de la carte du personnage.
struct GroundCell {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

// La distance en cases du plus proche de `hostiles`, rien s'il n'y en a pas.
[[nodiscard]] std::optional<std::uint32_t> NearestCells(GroundCell from,
                                                        std::span<const GroundCell> hostiles);

// Les faits qui décident de l'entrée, relevés au moment du clic.
struct EntryGuardFacts {
  bool on_town_map = false;    // la carte a une fiche de ville
  bool on_indoor_map = false;  // la carte est un intérieur
  // Le monstre hostile vivant le plus proche, en cases ; rien s'il n'y en a
  // aucun en vue.
  std::optional<std::uint32_t> nearest_hostile_cells;
  bool trade_open = false;
  bool npc_dialogue_open = false;
  bool shop_open = false;  // une vente : commerce de PNJ ou boutique du personnage
  bool storage_open = false;
  bool loading = false;  // un changement de carte en cours
  bool dead = false;
  bool link_closed = false;  // la liaison au serveur est perdue
};

// Pourquoi la partie ne peut pas commencer, dans l'ordre où Judge les
// examine : la première qui tient est la seule dite.
enum class EntryRefusal : std::uint8_t {
  kLinkClosed,
  kLoading,
  kDead,
  kNotSheltered,
  kHostileNearby,
  kTradeOpen,
  kNpcDialogueOpen,
  kShopOpen,
  kStorageOpen,
};
inline constexpr std::uint8_t kEntryRefusalCount = 9;

// La première raison de refus, ou rien quand la partie peut commencer.
[[nodiscard]] std::optional<EntryRefusal> Judge(const EntryGuardFacts& facts);

// Le texte français de la raison, marqué pour le catalogue : le bouton grisé
// le dit par `Tr`.
[[nodiscard]] const char* RefusalText(EntryRefusal refusal);

// ─── Les alertes du monde réel (U5) ────────────────────────────────────────

// Ce que app relève du monde à chaque image. Les compteurs ne font que
// croître pendant une partie ; les drapeaux disent un état en cours, et c'est
// leur passage à vrai qui alerte.
struct RealWorldFacts {
  std::int32_t hp = 0;
  std::int32_t max_hp = 0;  // le plafond, qui baisse à la fin d'un bonus
  bool dead = false;
  std::uint32_t map_entries = 0;  // entrées en carte depuis la connexion
  bool trade_offered = false;     // un échange proposé, en attente de réponse
  bool party_invited = false;
  bool guild_invited = false;
  bool friend_invited = false;
  bool server_dialogue_open = false;  // un dialogue de PNJ ouvert par le serveur
  std::uint32_t whispers = 0;         // messages privés reçus
  bool link_open = true;
  [[nodiscard]] bool operator==(const RealWorldFacts&) const = default;
};

// Ce que le monde réel a fait entre deux images, du plus grave au plus
// anodin. Toutes mettent la partie en pause avec un bandeau, sauf kWhisper,
// qui n'incrémente qu'un compteur discret, et kLinkLost, qui fige la partie
// et la sauve.
enum class RealWorldAlert : std::uint8_t {
  kLinkLost,
  kDied,
  kHit,
  kMoved,
  kTrade,
  kInvite,
  kDialogue,
  kWhisper,
};
inline constexpr std::uint8_t kRealWorldAlertCount = 8;

[[nodiscard]] bool Pauses(RealWorldAlert alert) noexcept;
// Le texte français du bandeau de la pause, marqué pour le catalogue ; rien
// pour une alerte qui ne met pas en pause.
[[nodiscard]] const char* BannerText(RealWorldAlert alert);

// Les alertes d'une image.
struct RealWorldReport {
  std::vector<RealWorldAlert> alerts;  // dans l'ordre de l'énumération
  std::uint32_t new_whispers = 0;      // ce que le compteur gagne
  [[nodiscard]] bool pauses() const noexcept;
  [[nodiscard]] bool link_lost() const noexcept;
  // L'alerte qui met en pause la plus grave, celle que le bandeau dit.
  [[nodiscard]] std::optional<RealWorldAlert> banner() const noexcept;
};

// LA VEILLE D'UNE PARTIE : le relevé précédent, comparé à chaque image.
class RealWorldWatch {
 public:
  // Ce qui a changé de `previous` à `now`. Deux relevés égaux ne disent rien ;
  // une hausse de PV (régénération, soin) non plus, ni une baisse que celle du
  // plafond explique.
  [[nodiscard]] static RealWorldReport Compare(const RealWorldFacts& previous,
                                               const RealWorldFacts& now);
  // L'entrée dans la partie : le relevé de départ, sans alerte.
  void Start(const RealWorldFacts& facts) { previous_ = facts; }
  // Le relevé de l'image, comparé au précédent, qu'il remplace.
  [[nodiscard]] RealWorldReport Observe(const RealWorldFacts& now);

 private:
  RealWorldFacts previous_;
};

}  // namespace moonclient
