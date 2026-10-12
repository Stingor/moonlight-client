#pragma once

// LES SALONS DE DISCUSSION, DANS LA BOUCLE DE LA PARTIE : le formulaire de
// création, l'entrée chez un autre — mot de passe compris —, la salle, et ce
// que le salon dit dans la conversation.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/chat_room.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/chat_room.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/world.h"
#include "ro/ui/chat_room_window.h"
#include "ro/vfs/item_words.h"

#include "game/chat_room_memory.h"
#include "game/chat_room_rules.h"
#include "game/interface_sound.h"

namespace moonclient {

// LES FENÊTRES DU SALON, telles que l'interface les porte. Une vue nulle ne se
// dessine pas.
struct ChatRoomWindows {
  ro::ui::ChatRoomMakeState make;
  const ro::ui::ChatRoomMakeView* make_view = nullptr;
  ro::ui::ChatRoomMakeAction make_answer;

  ro::ui::ChatRoomWindowState room;
  const ro::ui::ChatRoomView* room_view = nullptr;
  ro::ui::ChatRoomWindowAction room_answer;

  ro::ui::ChatRoomPasswordState password;
  const ro::ui::ChatRoomPasswordView* password_view = nullptr;
  ro::ui::ChatRoomPasswordAction password_answer;
};

// CE QUE LE SALON LIT AILLEURS, à chaque image.
struct ChatRoomContext {
  ro::net::Connection& link;
  ro::sim::World& world;
  const ro::vfs::ItemWords& words;
  // Pour griser « Ajouter en ami », et pour ne pas envoyer ce que le serveur
  // refuserait.
  const ro::sim::FriendList& friends;
  ChatRoomStoreState stores;
  std::uint32_t now_ms = 0;
};

class ChatRoomStage {
 public:
  // Où le formulaire se retient. Lu une fois : la saisie survit aux parties.
  void See(std::filesystem::path client_root);

  // Une partie commence : on n'est dans aucun salon.
  void StartSession();

  [[nodiscard]] const ro::sim::ChatRoom& room() const noexcept { return room_; }

  // Une trame reçue. ⚠ CE QU'ELLE A À DIRE S'ÉCRIT AUSSITÔT, et non en fin
  // d'image : dans un même lot, l'arrivée d'un membre doit précéder sa parole.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 const ro::vfs::ItemWords& words,
                                                 std::uint32_t now_ms);
  // La parole qui, en salon, revient au salon, ses mots interdits masqués.
  // ⚠ AVANT LE MONDE.
  [[nodiscard]] ro::net::FrameOutcome ClaimLine(const ro::net::Frame& frame,
                                                const ro::sim::World& world, std::uint32_t now_ms);

  // LES MOTS INTERDITS DE L'INSTALLATION : masqués dans la parole et le titre
  // d'un salon, refusés dans le titre qu'on lui donne. Nuls : rien.
  void SeeBannedWords(const ro::sim::BannedWords* banned) noexcept { banned_ = banned; }

  // LE RACCOURCI : il ouvre et referme le formulaire. ⚠ DANS UN SALON, il
  // montre la salle — c'est elle que le joueur cherchait, et le serveur
  // refuserait un second salon sans un mot.
  void ToggleMake() noexcept { asked_ = Asked::kToggle; }
  // La commande `/chat`, qui ne fait qu'ouvrir.
  void OpenMake() noexcept { asked_ = Asked::kOpen; }
  // Le double-clic sur l'enseigne d'un salon.
  void AskJoin(std::uint32_t room_id, std::uint8_t status);
  // La salle, devant.
  void Raise() noexcept { raise_room_ = true; }
  // `/q` : on sort du salon, s'il y en a un. Rend faux hors salon.
  bool AskLeave() noexcept;

  // « Enregistrer le chat », choisi dans le menu d'un membre depuis le dernier
  // appel. L'écriture revient à qui tient le fil horodaté : voir
  // `ChatTabsView::SaveRoomLog`.
  [[nodiscard]] bool TakeSaveAsked() noexcept { return std::exchange(save_asked_, false); }

  // La barre de la conversation est-elle prêtée à la salle ?
  [[nodiscard]] bool LendsInput() const noexcept { return room_.in_room(); }

  // L'IMAGE : les réponses des fenêtres, les nouvelles à dire, puis les vues.
  void Advance(ChatRoomWindows& windows, const ChatRoomContext& context);

  // Les sons d'interface que les nouvelles du salon ont demandés depuis le
  // dernier appel : l'arrivée d'un membre. Voir `ChatRoomNewsRings`.
  [[nodiscard]] std::vector<InterfaceSoundFile> TakeSounds() { return std::exchange(sounds_, {}); }

 private:
  const ro::sim::BannedWords* banned_ = nullptr;
  enum class Asked : std::uint8_t { kNone, kToggle, kOpen };

  void FollowMake(ChatRoomWindows& windows, const ChatRoomContext& context);
  void FollowRoom(ChatRoomWindows& windows, const ChatRoomContext& context);
  void FollowPassword(ChatRoomWindows& windows, const ChatRoomContext& context);
  void Join(ChatRoomWindows& windows, const ChatRoomContext& context, std::uint32_t room_id,
            std::uint8_t status);
  void Create(ChatRoomWindows& windows, const ChatRoomContext& context);
  void SayNews(ro::sim::World& world, const ro::vfs::ItemWords& words, std::uint32_t now_ms);
  void ApplySettings(ChatRoomWindows& windows, const ChatRoomContext& context);
  void Compose(ChatRoomWindows& windows, const ChatRoomContext& context);
  void ComposeWords(const ro::vfs::ItemWords& words);
  void SetError(std::string text, ro::ui::ChatRoomField field);

  ro::sim::ChatRoom room_;
  std::filesystem::path client_root_;
  ChatRoomMemory memory_;
  // La mémoire n'a été posée qu'une fois dans le formulaire : rouvert, il
  // garde ce que le joueur y a laissé.
  bool seeded_ = false;
  Asked asked_ = Asked::kNone;
  bool raise_room_ = false;
  bool leave_asked_ = false;
  bool save_asked_ = false;
  // L'enseigne qu'un double-clic a désignée, jouée à l'image suivante.
  struct JoinAsk {
    std::uint32_t room_id = 0;
    std::uint8_t status = 0;
  };
  std::optional<JoinAsk> join_;
  // Le salon dont la fenêtre s'est préparée en dernier.
  std::uint32_t shown_room_seq_ = 0;
  // Les sons à jouer, que `TakeSounds` vide.
  std::vector<InterfaceSoundFile> sounds_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ─────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois tout rempli.
  std::string error_;
  ro::ui::ChatRoomField error_field_ = ro::ui::ChatRoomField::kNone;
  std::string password_error_;
  // Le refus des réglages, dit DANS leur volet, qui reste ouvert.
  std::string settings_error_;
  std::string blocker_;
  std::string title_taken_;
  std::vector<NearbyChatRoom> nearby_;
  std::vector<ro::ui::ChatRoomNearbyView> nearby_views_;
  std::string caption_;
  std::vector<ro::ui::ChatRoomMemberView> members_;
  ro::ui::ChatRoomWordsView words_;
  ro::ui::ChatRoomMakeView make_view_;
  ro::ui::ChatRoomView room_view_;
  ro::ui::ChatRoomPasswordView password_view_;
};

}  // namespace moonclient
