#pragma once

// LES CHUCHOTEMENTS, DANS LA BOUCLE DE LA PARTIE : les deux paquets du serveur,
// les conversations 1:1 et leurs fenêtres, les gestes sur un pseudo, et l'alarme.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/audio/audio_engine.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/whisper.h"
#include "ro/sim/whisper_blocks.h"
#include "ro/sim/world.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/whisper_window.h"
#include "ro/vfs/vfs.h"

#include "chat/chat_item_link.h"
#include "chat/player_link_judge.h"
#include "chat/whisper_block_memory.h"
#include "chat/whisper_desk.h"
#include "web/http_transport.h"
#include "web/user_config_web.h"

namespace moonclient {

// LES FENÊTRES DES CONVERSATIONS, telles que l'interface les porte.
struct WhisperWindows {
  // L'état de chaque fenêtre, par correspondant. ⚠ UNE TABLE ORDONNÉE : les
  // états sont pris par référence pendant le dessin.
  std::map<std::string, ro::ui::WhisperWindowState, std::less<>> states;
  // Ce que chacune montre, composé avant le dessin. Le fil, lui, vient des
  // onglets, qui seuls savent décorer une ligne.
  std::vector<ro::ui::WhisperWindowView> views;
  // CE QU'ELLES ONT RENDU, par correspondant, suivi à l'image suivante.
  std::vector<std::pair<std::string, ro::ui::WhisperWindowAction>> answers;
  // Les identités des fenêtres, déclarées à la pile d'Échap.
  std::vector<std::string> escape_ids;
};

// CE QUE L'ÉTAPE LIT AILLEURS, à chaque image.
struct WhisperContext {
  ro::net::Connection& link;
  ro::sim::World& world;
  const FriendSetup& setup;
  std::uint32_t now_ms = 0;
  // CE QUE LE MENU D'UN RESPAWN DE MVP POSÉ DANS UNE SAISIE SAIT D'AVANCE :
  // seul le carnet des MVP le connaît. Vide, ses pastilles n'en savent rien.
  std::function<const ro::ui::MvpLinkMenuFacts*(const MvpLink&)> judge_mvp;
};

// LE GESTE « CHUCHOTER » ORDINAIRE : le nom dans la case du destinataire, la
// barre dépliée, le clavier dans la phrase. Rien ne part sur le fil.
void ArmWhisper(ro::ui::ChatInputState& bar, std::string_view name);

// L'ALARME D'UNE CONVERSATION QUI S'OUVRE. Le client joue d'abord la
// sonnerie de messagerie quand l'installation en porte une — case cochée ou
// non —, et sinon le son de bouton, sous la case seulement.
void PlayWhisperAlarm(ro::audio::AudioEngine& audio, ro::vfs::Vfs& vfs,
                      const std::filesystem::path& client_root, bool alarm);

class WhisperStage {
 public:
  // Une partie commence : rien n'est ouvert, rien n'attend d'accusé.
  // `last_seq` est le rang de la dernière ligne déjà dite.
  void StartSession(std::uint64_t last_seq);

  [[nodiscard]] ro::sim::WhisperOutbox& outbox() noexcept { return outbox_; }
  [[nodiscard]] const WhisperDesk& desk() const noexcept { return desk_; }
  // QUI NE PEUT PLUS NOUS CHUCHOTER, tel que le serveur l'a dit.
  [[nodiscard]] ro::sim::WhisperBlockList& blocks() noexcept { return blocks_; }

  // LA LISTE DE BLOCAGE RANGÉE AU SERVICE WEB : relue dès maintenant, rejouée
  // après la première carte, sauvegardée à chaque changement — voir
  // `chat/whisper_block_memory.h`. `origin` vide : pas de service.
  void RememberBlocks(std::string origin, web::UserConfigIdentity identity);
  // L'avis de chargement d'une carte est parti : le serveur écoute.
  void NoteMapEntered() noexcept;

  // Les deux paquets du chuchotement, les trois réponses du blocage, les deux
  // annonces — la voix d'un nom ignoré s'y tait —, puis la parole des joueurs.
  // Les mots interdits se masquent dans les deux dernières. ⚠ AVANT LE
  // MONDE : lui lirait tout cela, mais sans nos amis, nos envois ni la liste.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 const ro::sim::FriendList& friends,
                                                 const ro::sim::GuildBook& guild,
                                                 std::uint32_t now_ms);

  // LES MOTS INTERDITS DE L'INSTALLATION : masqués dans ce qui arrive, et dans
  // ce qui part d'une conversation 1:1. Nuls : rien ne se masque.
  void SeeBannedWords(const ro::sim::BannedWords* banned) noexcept { banned_ = banned; }

  // « CHUCHOTER DANS UNE FENÊTRE… » : ouvre ou ramène, et donne le clavier.
  void Open(std::string_view peer);

  // UN GESTE SUR UN PSEUDO, venu du fil ou d'une conversation.
  void FollowPlayer(std::string_view name, ro::ui::ChatLinkGesture gesture,
                    ro::net::Connection& link, ro::ui::ChatInputState& bar);

  // L'IMAGE : ce que les fenêtres ont rendu, les lignes neuves, puis les vues.
  // Rend combien de conversations une ligne a fait naître — l'alarme sonne pour
  // elles.
  int Advance(WhisperWindows& windows, const WhisperContext& context);

  // ── LES LIENS POSÉS DANS UNE SAISIE 1:1 ──────────────────────────────────
  //
  // 🔴 LA DERNIÈRE SAISIE ACTIVE, ET NON CELLE QUI A LE FOCUS : au moment du
  // Maj+clic, le focus est sur la fenêtre cliquée — le sac, le carnet —, jamais
  // sur une saisie. Une saisie 1:1 qui prend le clavier devient donc la cible,
  // et le reste jusqu'à ce que la barre de la conversation le reprenne ou que
  // sa fenêtre se ferme.
  //
  // Une saisie 1:1 était la dernière active : un lien posé va dans la sienne.
  [[nodiscard]] bool TakesLinks() const noexcept;
  // La barre de la conversation a repris le clavier.
  void ReleaseLinks() noexcept;
  // Pose le lien dans la saisie de la dernière conversation active, si sa
  // fenêtre est ouverte. Faux, et rien ne change, quand la place manque ou
  // qu'aucune ne le prend.
  [[nodiscard]] bool PlaceLink(PendingChatLink link, WhisperWindows& windows);
  // Les liens en attente dans la saisie de ce correspondant, dans l'ordre de
  // pose : le jeton d'une de ses pastilles en est le rang. Nul quand elle n'en
  // porte aucun.
  [[nodiscard]] const std::vector<PendingChatLink>* PendingOf(std::string_view peer) const;

 private:
  // Résout la phrase validée, la mesure, puis l'envoie ou la retient. Rend
  // vrai quand elle est retenue.
  bool Settle(WhisperWindows& windows, const std::string& peer, const std::string& said,
              const WhisperContext& context);
  void Send(std::string_view peer, std::string text, const WhisperContext& context);
  // Oublie les liens des saisies fermées, et ceux dont le libellé a quitté
  // leur champ.
  void PruneLinks(const WhisperWindows& windows);
  void Compose(WhisperWindows& windows, const WhisperContext& context);

  WhisperDesk desk_;
  ro::sim::WhisperOutbox outbox_;
  ro::sim::WhisperBlockList blocks_;
  const ro::sim::BannedWords* banned_ = nullptr;
  // ⚠ LE TRANSPORT AVANT LA MÉMOIRE QUI LE REGARDE : elle meurt la première.
  std::unique_ptr<web::HttpTransport> blocks_transport_;
  std::optional<WhisperBlockMemory> block_memory_;
  // Les titres de l'image. ⚠ LES VUES POINTENT DEDANS : ils se composent tous
  // avant qu'une vue ne soit prise.
  std::vector<std::string> titles_;
  // Un geste a demandé une conversation que le plafond n'a pas pu loger.
  bool refused_open_ = false;
  // LES LIENS POSÉS, par correspondant, dans l'ordre de pose.
  std::map<std::string, std::vector<PendingChatLink>, std::less<>> links_;
  // Le correspondant dont la saisie était la dernière active ; vide quand
  // c'est la barre de la conversation.
  std::string links_peer_;
  // Les pastilles de chaque vue, au rang de sa conversation, et leurs
  // libellés. ⚠ RECOPIÉS COMME LES TITRES : un lien posé entre la composition
  // et le dessin déplacerait ceux de `links_`, et les vues pointent dedans.
  std::vector<std::vector<ro::ui::ChatInputLink>> chips_;
  std::vector<std::string> chip_labels_;
};

}  // namespace moonclient
