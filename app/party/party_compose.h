#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/friend_list.h"
#include "ro/sim/party_roster.h"
#include "ro/ui/party_friend_window.h"
#include "ro/ui/party_grid.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/status_cell.h"

namespace moonclient {

// ── LE GROUPE ET LES AMIS, EN VUES ──────────────────────────────────────────
//
// La fenêtre Groupe / Amis et la grille de groupe lisent la même chose : la
// liste du serveur, la liste d'amis, ce que le monde sait de chacun, et ce que
// l'installation dessine. Ce fichier les croise, une fois par image ; `ui` ne
// voit que le résultat.
//
// ⚠ MA TUILE EXISTE MÊME HORS GROUPE : la grille sert aussi de HUD de vitals.
// Mes PV, mon PS et ma position viennent de mes propres valeurs, jamais de la
// liste — le serveur ne se redécrit pas le joueur à lui-même.

// CE QUE LA PARTIE SAIT DE MOI.
struct PartySelf {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string name;
  // Ma carte BRUTE, extension comprise : la forme que la liste donne aux
  // membres, et contre laquelle on les compare.
  std::string map_raw;
  std::uint16_t job = 0;
  int base_level = 0;
  bool hp_known = false;
  int hp = 0;
  int max_hp = 0;
  bool sp_known = false;
  int sp = 0;
  int max_sp = 0;
  bool position_known = false;
  int x = 0;
  int y = 0;
};

// La mini-carte d'une carte, et ses dimensions en cellules.
struct PartyMinimap {
  ro::ui::SkinImage image;
  int width_cells = 0;
  int height_cells = 0;
};

// Les états d'un personnage. `known` à faux : on n'en sait rien, ce qui n'est
// pas « aucun état ».
struct PartyStatuses {
  std::span<const ro::ui::StatusCellView> cells;
  bool known = false;
  // 🔴 LA LISTE ET LA GRILLE ONT CHACUNE LEUR APERÇU, mais partagent ces cases :
  // les `genuine` premières sont de vrais états, les suivantes des faux. Une
  // surface sans aperçu s'arrête là, et ne tient pour « su » que ce qui l'est
  // sans lui.
  std::size_t genuine = 0;
  bool genuinely_known = false;
};

// CE QUE LA COMPOSITION DEMANDE AU JEU : l'écran, les tables, les textures.
//
// ⚠ UNE INTERFACE, ET NON LE JEU : c'est ce qui rend la composition essayable
// sans monde ni fenêtre. Les vues rendues POINTENT dans ce que ces méthodes
// rendent, qui doit donc vivre jusqu'à la fin du dessin.
class PartyLens {
 public:
  PartyLens() = default;
  PartyLens(const PartyLens&) = delete;
  PartyLens& operator=(const PartyLens&) = delete;
  PartyLens(PartyLens&&) = delete;
  PartyLens& operator=(PartyLens&&) = delete;
  virtual ~PartyLens() = default;

  // Un acteur représente ce compte dans la scène.
  [[nodiscard]] virtual bool ActorPresent(std::uint32_t account_id) const = 0;
  [[nodiscard]] virtual ro::ui::SkinImage JobIcon(std::uint16_t job) = 0;
  // Déjà traduit ; vide quand la table ne le connaît pas.
  [[nodiscard]] virtual std::string_view JobName(std::uint16_t job) const = 0;
  // Le nom affiché d'une carte brute, vide quand la table ne la connaît pas.
  [[nodiscard]] virtual std::string_view MapName(std::string_view map_raw) const = 0;
  // Vide quand l'installation ne la dessine pas.
  [[nodiscard]] virtual PartyMinimap Minimap(std::string_view map_raw) = 0;
  // La tête de l'acteur qui représente ce compte ; vide sans acteur.
  [[nodiscard]] virtual ro::ui::SkinImage Head(std::uint32_t account_id) = 0;
  [[nodiscard]] virtual ro::ui::SkinImage Crown() = 0;
  [[nodiscard]] virtual PartyStatuses Statuses(std::uint32_t account_id, bool is_me) = 0;
  // Pourquoi « Inviter dans mon carnet MVP » est refusé à ce joueur, DÉJÀ
  // TRADUIT ; nul quand l'invitation est offerte. `same_account` : un autre
  // personnage de mon compte. Par défaut, rien ne s'y oppose.
  [[nodiscard]] virtual const char* MvpInviteRefusal(std::string_view name,
                                                     bool same_account) const;
};

// ── LES FAUX MEMBRES DU MODE RÉGLAGE ─────────────────────────────────────
//
// De quoi poser et régler la grille sans groupe : une tuile par état qu'elle
// sait montrer — vie haute, moyenne, basse, à terre, hors de portée, hors
// ligne, et un chef.
//
// 🔴 LEURS COMPTES SONT RÉSERVÉS, en haut de la plage : aucun joueur ne les
// porte, et `IsFakeMember` permet d'écarter tout geste sur eux avant qu'il ne
// parte au serveur — une expulsion d'un faux membre partirait sinon pour de bon.
inline constexpr std::uint32_t kFakeMemberAccountBase = 0xFFFF'FF00U;

[[nodiscard]] constexpr bool IsFakeMember(std::uint32_t account_id) noexcept {
  return account_id >= kFakeMemberAccountBase;
}

// Ce que la composition lit à chaque image.
struct PartyComposeInput {
  const ro::sim::PartyRoster* party = nullptr;
  const ro::sim::FriendList* friends = nullptr;
  PartySelf self;
  // La désignation du jeu, zéro pour aucune.
  std::uint32_t target_id = 0;
  std::uint32_t now_ms = 0;
  bool grid_shown = false;
  // `PartyFriendLook::head_mode` : les têtes ne se composent que là où elles se
  // montrent.
  int head_mode = 0;
  // Le mode réglage de la grille : des faux membres après les vrais. ⚠ JAMAIS
  // PERSISTÉ, comme l'aperçu des faux états.
  bool fake_members = false;
  // L'aperçu des faux états, surface par surface.
  bool window_preview = false;
  bool grid_preview = false;
  // Le carnet de chasse MVP est éteint : aucune ligne n'offre d'y inviter.
  bool mvp_invites_withdrawn = false;
};

class PartyViews {
 public:
  void Compose(const PartyComposeInput& input, PartyLens& lens);

  // ⚠ ELLES POINTENT DANS CET OBJET, dans la liste du serveur et dans ce que la
  // lentille a rendu : elles ne survivent ni au `Compose` suivant, ni à une
  // trame appliquée entre les deux.
  [[nodiscard]] const ro::ui::PartyFriendView& window() const noexcept { return window_; }
  [[nodiscard]] const ro::ui::PartyGridView& grid() const noexcept { return grid_; }

 private:
  // `statuses` reçoit ce que la lentille sait de ses états.
  [[nodiscard]] ro::ui::PartyMemberView MemberRow(const ro::sim::PartyMember& member,
                                                  const PartyComposeInput& input, PartyLens& lens,
                                                  PartyStatuses& statuses) const;
  [[nodiscard]] ro::ui::PartyMemberView SelfRow(PartyLens& lens, PartyStatuses& statuses) const;
  void ComposeFriends(const PartyComposeInput& input, PartyLens& lens);
  void ComposeTiles(const PartyComposeInput& input, bool self_listed);
  void ComposeFakeTiles(const PartyComposeInput& input, PartyLens& lens);
  void ComposeInvite(const PartyComposeInput& input);

  PartySelf self_;
  std::vector<ro::ui::PartyMemberView> members_;
  // Rang pour rang avec `members_`.
  std::vector<PartyStatuses> members_statuses_;
  // Ceux de ma ligne à part, hors groupe.
  PartyStatuses self_statuses_;
  // Ma ligne quand la liste ne me porte pas : hors groupe, elle ne sert qu'à la
  // grille.
  ro::ui::PartyMemberView self_row_;
  std::vector<ro::ui::FriendView> friends_;
  std::vector<ro::ui::PartyGridTileView> tiles_;
  // Les noms des faux membres, que leurs tuiles désignent.
  std::vector<std::string> fake_names_;
  ro::ui::PartyFriendView window_;
  ro::ui::PartyGridView grid_;

  // LA DEMANDE PRÉSENTÉE : la vue n'en montre qu'une, le serveur peut en tenir
  // deux. La plus récente passe devant, l'autre attend son tour.
  std::optional<std::uint32_t> seen_party_invite_;
  std::optional<std::uint64_t> seen_friend_invite_;
  bool party_invite_first_ = true;
};

}  // namespace moonclient
