#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/entry_queue.h"
#include "ro/sim/guild.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/entry_queue_window.h"
#include "ro/ui/skin_art.h"
#include "ro/vfs/vfs.h"

#include "game/battleground_file.h"
#include "game/bug_report_context.h"
#include "game/interface_sound.h"

namespace moonclient {

// ── L'ÉTAGE DU CHAMP DE BATAILLE ────────────────────────────────────────────
//
// La file d'inscription que le réseau remplit, la vue que ses fenêtres
// dessinent, et ce qu'on fait de ce qu'elles demandent : envoyer, ouvrir,
// fermer, dire, sonner. Le combat lui-même — camps, score, coéquipiers — vit
// dans le monde, et ses trames passent aussi par ici.
//
// 🔴 LA FILE SURVIT AUX CARTES ET NON À LA PARTIE : une nouvelle connexion
// repart hors file, et le serveur redit ce qu'il tient.
//
// ⚠ LE SERVEUR JUGE : effectif, niveau, délai. Les seules gardes d'ici sont
// celles qu'il ne dirait pas — liaison prête, chef du groupe pour une
// inscription de groupe, pas d'inscription depuis un champ de bataille.

// Une image du jeu, par son chemin sous la racine des skins, en UTF-8 : la
// minimap d'un terrain.
using BattlegroundImageAt = std::function<ro::ui::SkinImage(std::string_view)>;

// UN OBJET DE LA BASE, tel qu'une récompense le montre : son icône et son nom
// dans la langue du joueur. Nom vide : l'objet n'est pas dans la base.
struct BattlegroundItemFace {
  ro::ui::SkinImage icon;
  std::string name;
};
using BattlegroundItemAt = std::function<BattlegroundItemFace(std::uint32_t)>;

// LES PIÈCES DE L'ÉTAGE dans les fenêtres de l'interface.
struct BattlegroundWindows {
  ro::ui::EntryQueueWindowState window;
  // La vue de l'image ; nulle tant que l'étage n'en a composé aucune.
  const ro::ui::EntryQueueView* view = nullptr;
  // Ce que les fenêtres ont demandé, lu à l'avancée suivante.
  ro::ui::EntryQueueAction answer;
  // En file, à la dernière avancée : la pastille de l'icône du menu.
  bool queued = false;
  // Le terrain où l'on est en file, tel que le joueur le lit ; vide hors file.
  std::string queued_name;
};

// CE QUE L'ÉTAGE REÇOIT À CHAQUE IMAGE.
struct BattlegroundContext {
  ro::net::Connection& link;
  ro::sim::World& world;
  const ro::sim::PartyRoster& party;
  const ro::sim::GuildBook& guild;
  // Nul : le fichier des terrains ne se lit pas, et la liste reste vide.
  ro::vfs::Vfs* vfs = nullptr;
  BattlegroundImageAt image_at;
  // Vide : une récompense en objet se montre « #numéro », sans icône.
  BattlegroundItemAt item_at;
  // La liaison accepte un envoi.
  bool link_ready = false;
  // Notre personnage : la guilde reconnaît son maître par lui.
  std::uint32_t own_char_id = 0;
  std::uint32_t now_ms = 0;
};

class BattlegroundStage {
 public:
  // UNE NOUVELLE PARTIE : la file et ses fenêtres repartent à zéro.
  void StartSession(BattlegroundWindows& windows);

  // La file, puis le combat. ⚠ CONSULTÉ À CHAQUE TRAME, même réclamée par un
  // autre traducteur : la vie d'un coéquipier intéresse aussi le monde. Les
  // mots interdits se masquent dans la parole.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 std::uint32_t now_ms);
  // Les mots interdits de l'installation. Nuls : rien ne se masque.
  void SeeBannedWords(const ro::sim::BannedWords* banned) noexcept { banned_ = banned; }

  // À CHAQUE IMAGE : la table des terrains, la réponse des fenêtres, les
  // transitions de la file, l'effectif redemandé, la vue.
  void Advance(BattlegroundWindows& windows, const BattlegroundContext& context);

  // L'icône du menu : ferme la liste et « en attente » si l'une est ouverte ;
  // sinon ouvre « en attente » en file, la liste hors file.
  void RequestOpen(BattlegroundWindows& windows);

  // LES LIGNES À ÉCRIRE DANS LA CONVERSATION, déjà traduites. Consommées.
  [[nodiscard]] std::vector<std::string> TakeNotices();
  // LES SONS À JOUER. Consommés.
  [[nodiscard]] std::vector<InterfaceSoundFile> TakeSounds();
  // LE RAPPORT DE BUG que le bouton d'une fenêtre a demandé. Consommé.
  [[nodiscard]] std::optional<BugReportContext> TakeBugReport() noexcept;
  // CE QUE LE JOUEUR A FAIT DU LIEN D'UNE RÉCOMPENSE à la dernière image —
  // survol, geste, entrée du menu — : la partie le joue comme tout objet de
  // catalogue. Consommé.
  [[nodiscard]] ro::ui::ChatLinkAsk TakeItemGesture() noexcept;

  [[nodiscard]] const ro::sim::EntryQueue& queue() const noexcept { return queue_; }

 private:
  const ro::sim::BannedWords* banned_ = nullptr;
  // La table se lit une fois, à la première avancée qui a de quoi la lire ;
  // un échec se dit une fois.
  void EnsureTable(ro::vfs::Vfs* vfs);
  // Ce que les fenêtres ont demandé à l'image précédente.
  void ApplyAnswer(BattlegroundWindows& windows, const BattlegroundContext& context);
  // LE REFUS D'UNE INSCRIPTION à ce terrain, déjà traduit ; nul si elle peut
  // partir. Jugé pour la vue comme pour la demande : voir `ApplyRefusal`.
  [[nodiscard]] static const char* RefusalOf(ro::sim::EntryQueueApplyKind kind,
                                             const BattlegroundField& field,
                                             const BattlegroundContext& context);
  // Les fenêtres suivent la phase de la file, à chaque changement.
  void FollowTransitions(BattlegroundWindows& windows);
  // Les avis et les sons que la file a levés.
  void CollectQueueNews();
  // Le nom qu'un terrain montre au joueur ; celui du serveur s'il n'est pas à
  // la table.
  [[nodiscard]] std::string_view DisplayNameOf(std::string_view server_name) const;
  // LA VUE, refaite à chaque image : `battleground_compose.cpp`.
  void ComposeView(BattlegroundWindows& windows, const BattlegroundContext& context);
  // Une récompense telle que le détail la montre ; son texte composé s'ajoute
  // à `reward_texts_`, qui doit avoir la place.
  [[nodiscard]] ro::ui::EntryQueueRewardView RewardView(const BattlegroundReward& reward,
                                                        std::string_view language,
                                                        const BattlegroundContext& context);

  ro::sim::EntryQueue queue_;
  std::optional<BattlegroundFile> table_;
  bool table_tried_ = false;
  // Les objets de récompense absents de la base, déjà dits au journal.
  std::set<std::uint32_t> unknown_items_told_;
  // La dernière phase suivie : voir `ro::sim::EntryQueue::phase_serial`.
  std::uint32_t seen_serial_ = 0;
  // En file à la dernière transition suivie : l'entrée en file ouvre « en
  // attente », une seule fois.
  bool was_queued_ = false;
  std::vector<std::string> notices_;
  std::vector<InterfaceSoundFile> sounds_;
  // Le résultat que la boîte montre, déjà traduit ; vide : aucun.
  std::string notice_shown_;
  std::optional<BugReportContext> bug_report_;
  ro::ui::ChatLinkAsk item_gesture_;
  // LA COMPOSITION : ce que les vues pointent. ⚠ RÉSERVÉ AVANT LES VUES.
  ro::ui::EntryQueueView view_;
  std::vector<ro::ui::EntryQueueFieldView> field_views_;
  // Les textes qu'un terrain compose, déjà traduits. Ses autres textes sont
  // des vues dans le fichier, qui ne bouge plus une fois lu.
  struct FieldTexts {
    std::string condition;
    std::string team;
  };
  // Par terrain, dans l'ordre de la liste.
  std::vector<FieldTexts> field_texts_;
  std::vector<ro::ui::EntryQueueRewardView> reward_views_;
  std::vector<std::string> reward_texts_;
  std::string queued_name_;
  std::string invited_name_;
};

}  // namespace moonclient
