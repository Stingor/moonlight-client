#pragma once

// LE FAMILIER DANS LA BOUCLE DE LA PARTIE : ce que le serveur en dit, ce que
// ses deux fenêtres montrent, et ce que leurs gestes deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/pet_packets.h"
#include "ro/sim/hunger_alarm.h"
#include "ro/sim/pet.h"
#include "ro/sim/world.h"
#include "ro/ui/pet_hatch_window.h"
#include "ro/ui/pet_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/pet_tables.h"
#include "ro/vfs/vfs.h"

#include "world/item_obtain_toast.h"
#include "world/monster_sprite.h"

namespace moonclient {

struct GamePieces;

class PetStage {
 public:
  // Lit une trame du serveur ; `kUnhandled` si elle ne le regarde pas. Le
  // résultat d'une évolution, l'accusé d'un repas et l'écho de
  // l'auto-alimentation s'écrivent dans la conversation.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 std::uint32_t now_ms);

  // Compose les vues des deux fenêtres, à chaque image, avant le dessin. Ouvre
  // la fiche quand l'intimité tombe, la ferme quand le familier est parti ;
  // ouvre la liste d'éclosion que le serveur envoie.
  void Stage(ro::vfs::Vfs& vfs, const ro::sim::World& world, GamePieces& pieces,
             ro::ui::PetWindowState& window, ro::ui::PetWindowView& view,
             ro::ui::PetHatchWindowState& hatch, ro::ui::PetHatchWindowView& hatch_view);

  // Ce que les deux fenêtres ont demandé, mis sur le fil — gardes du client
  // comprises. Suit aussi la présence de l'acteur, et joue la bascule qu'une
  // touche ou l'icône du menu a demandée dans `window`.
  void Apply(const ro::ui::PetWindowAction& asked, const ro::ui::PetHatchWindowAction& hatched,
             ro::ui::PetWindowState& window, ro::net::Connection& link, ro::sim::World& world,
             std::uint32_t now_ms);

  // ── CE QUE LE MENU D'ENTITÉ DEMANDE ─────────────────────────────────────

  // L'acteur de MON familier ; zéro quand aucun n'est sorti ou qu'il est parti.
  [[nodiscard]] std::uint32_t own_pet_id() const noexcept { return pet_.actor_id(); }
  [[nodiscard]] bool has_accessory() const noexcept {
    return pet_.known() && pet_.wears_accessory();
  }

  // ── CE QUE LE SAC DEMANDE ───────────────────────────────────────────────

  // Un item que le client propose au pet : un accessoire de pet, et, la table
  // des accessoires lue, un accessoire qu'elle connaît.
  [[nodiscard]] bool OffersAsAccessory(const ro::sim::InventoryItem& item) const;
  // Sans pet sorti, le dit dans la conversation et rend vrai : rien ne part.
  // Avec, rend faux — le serveur juge seul si l'accessoire est le sien.
  bool RefusesAccessory(ro::sim::World& world) const;

  // « Statut du pet » : demande la fiche au serveur et la bascule. Sans
  // familier sorti, rien.
  void ToggleWindow(ro::ui::PetWindowState& window, ro::net::Connection& link);

  // L'ALERTE DE FAIM qui vient de sonner, pour le bandeau ; vide sinon.
  [[nodiscard]] std::optional<ToastNotice> TakeHungerNotice() noexcept {
    return std::exchange(hunger_notice_, std::nullopt);
  }

  // Nourrir, APRÈS la confirmation que le menu a posée. Les gardes du client
  // passent d'abord : lettre en rédaction, échange en cours, pas de nourriture
  // en sac — chacune refusée dans la conversation, sans rien envoyer.
  void Feed(ro::net::Connection& link, ro::sim::World& world);
  // Ce qui refuse « nourrir » AVANT même la question — une lettre en
  // rédaction, un échange en cours —, traduit ; nul quand rien ne s'y oppose.
  [[nodiscard]] const char* FeedRefusal() const;
  void Perform(ro::net::Connection& link);
  void UnequipAccessory(ro::net::Connection& link);
  void ReturnToEgg(ro::net::Connection& link);

 private:
  void LoadTables(ro::vfs::Vfs& vfs);
  void Command(ro::net::Connection& link, ro::proto::PetCommand command);
  // Le dernier garde-fou de « nourrir » : la nourriture de l'espèce en sac.
  void FeedIfFood(ro::net::Connection& link, ro::sim::World& world);
  void Say(ro::sim::World& world, std::string text, std::uint32_t color) const;

  void ComposeWindow(const ro::sim::World& world, GamePieces& pieces,
                     ro::ui::PetWindowState& window, ro::ui::PetWindowView& view);
  void ComposePanel(const ro::sim::World& world, GamePieces& pieces, std::uint32_t source_egg,
                    ro::ui::PetWindowState& window, ro::ui::PetWindowView& view);
  void ComposeHatch(const ro::sim::World& world, GamePieces& pieces,
                    ro::ui::PetHatchWindowView& view) const;

  ro::sim::Pet pet_;
  // Lues au premier besoin ; vides si l'installation ne les porte pas.
  std::optional<ro::vfs::PetTables> tables_;
  bool tables_tried_ = false;

  std::optional<MonsterSprite> sprite_;
  std::optional<MonsterSprite> source_sprite_;
  std::optional<MonsterSprite> target_sprite_;

  // L'horloge de la dernière image : la composition n'en reçoit pas.
  std::uint32_t last_now_ms_ = 0;
  // La table des items, relevée à chaque image : le refus de « nourrir » nomme
  // la nourriture qui manque.
  const ro::vfs::ItemTable* items_ = nullptr;
  // Ce qui refuse « nourrir », relevé à chaque image.
  bool mail_writing_ = false;
  bool trading_ = false;
  // L'intimité vient de tomber : la fiche s'ouvre à la prochaine composition.
  bool force_open_ = false;
  ro::sim::HungerAlarm hunger_alarm_;
  std::optional<ToastNotice> hunger_notice_;
};

}  // namespace moonclient
