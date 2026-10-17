#pragma once

// LE COURRIER (RODEX), DANS LA BOUCLE DE LA PARTIE : la boîte qu'on ouvre et
// rafraîchit, les courriers qu'on lit et dont on récupère les pièces jointes,
// l'écriture et ses garde-fous, et ce que tout cela dit dans la conversation.
//
// La boîte vit ici, dans `sim::Mailbox` ; les fenêtres ne font que demander, et
// chaque demande passe par un verbe de `net`, qui revalide avant d'émettre.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/render/game_images.h"
#include "ro/sim/mailbox.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/mail_window.h"
#include "ro/vfs/job_names.h"

#include "game/bug_report_context.h"
#include "game/mail_write_flow.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

// LES DEUX FENÊTRES DU COURRIER, telles que `UiStage::Windows` les porte : pour
// chacune, ce qu'elle garde d'une image à l'autre, la vue que la boucle
// compose, et ce que le joueur a demandé.
struct MailWindows {
  ro::ui::MailboxWindowState box;
  const ro::ui::MailboxView* box_view = nullptr;
  ro::ui::MailboxAction box_answer;

  ro::ui::MailComposeWindowState compose;
  const ro::ui::MailComposeView* compose_view = nullptr;
  ro::ui::MailComposeAction compose_answer;

  // L'icône du menu s'allume : la pastille du serveur, ou un courrier non lu
  // dans la liste.
  bool news_lit = false;
};

// L'instant présent, en secondes Unix : l'expiration des courriers se compte
// sur l'horloge réelle, pas sur celle de la partie.
[[nodiscard]] std::int64_t MailNowUnixSeconds();

class MailStage {
 public:
  // Ce que le courrier lit ailleurs : les noms et icônes des items, les mots
  // du jeu, les noms de job, les images de l'installation. Tous facultatifs.
  void See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           const ro::vfs::ItemWords* words, const ro::vfs::JobNames* jobs,
           ro::render::GameImages* images) noexcept;

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // Une trame reçue. `now_unix_s` fixe l'expiration des courriers listés.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 std::int64_t now_unix_s);

  // L'ICÔNE DU MENU ET SA TOUCHE : ouvrir la boîte — et demander sa liste —,
  // ou la refermer, ce qui n'émet rien. Joué à l'image suivante. L'ordre du
  // serveur (`@mail`) prend le même chemin (`MailBoxMoveFor`).
  void Toggle() noexcept { toggle_asked_ = true; }

  // « ENVOYER UN COURRIER… » : une écriture adressée à ce personnage, demandée
  // à l'image suivante.
  void Compose(std::string recipient) { compose_asked_ = std::move(recipient); }

  // « JOINDRE AU COURRIER », depuis le sac : l'ajout part à l'image suivante,
  // une fois rejugé.
  void QueueAttach(std::uint16_t client_slot, int amount);

  // L'écriture se montre : le sac propose d'y joindre.
  [[nodiscard]] bool writing() const noexcept { return flow_.shown(); }

  // LE COURRIER RETIENT-IL LE MONDE ? Pendant l'écriture, le serveur refuse la
  // marche, l'usage d'un item et l'échange, et il les refuse EN SILENCE : la
  // souris ne marche ni n'attaque, comme pendant un échange. L'écriture d'un
  // courrier envoyé reste ouverte chez lui tant que l'annulation est due.
  [[nodiscard]] bool HoldsWorld() const noexcept;

  // L'USAGE D'UN ITEM EST-IL RETENU ? Vrai : retenu, et la raison se dira à
  // l'image suivante.
  [[nodiscard]] bool HoldsItemUse();

  // « PROPOSER UN ÉCHANGE » EST-IL RETENU ? Le serveur l'ignorerait en silence
  // pendant l'écriture. Vrai : retenu, et dit à l'image suivante.
  [[nodiscard]] bool HoldsDealProposal();

  // Ce que l'écriture retient de cette case du sac : joint, ou en route. Le
  // sac MONTRÉ le retranche ; celui du monde n'y touche jamais.
  [[nodiscard]] std::int32_t WithheldAt(std::uint16_t client_slot) const noexcept;

  // L'IMAGE : les silences, les réponses des fenêtres, les nouvelles à dire,
  // puis les vues de la prochaine.
  void Advance(MailWindows& windows, ro::net::Connection& link, ro::sim::World& world,
               const ro::vfs::ItemWords& words, const CreatorNames& creators, std::uint32_t now_ms);

  // La pièce jointe survolée à l'image précédente, et celle qu'on veut
  // décrire — une fois.
  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

  // Le rapport de bug que le joueur a demandé de la boîte ou de l'écriture,
  // avec ce que la fenêtre montrait — une fois.
  [[nodiscard]] std::optional<BugReportContext> TakeBugReport() noexcept;

 private:
  struct QueuedAttach {
    std::uint16_t client_slot = 0;
    int amount = 0;
  };

  void FollowAsks(MailWindows& windows, const TradeContext& context);
  void FollowBox(MailWindows& windows, const TradeContext& context);
  void FollowCompose(MailWindows& windows, const TradeContext& context);
  void FollowWriteState(MailWindows& windows, const TradeContext& context);
  void Obey(const MailWriteOrders& orders, MailWindows& windows, const TradeContext& context);
  void RequestList(const TradeContext& context);
  // `recipient_wire` : le nom en octets du fil, vide pour aucun.
  void AskWrite(std::string_view recipient_wire, const TradeContext& context);
  // Le nom saisi tel qu'il partira : les octets du fil que le serveur a
  // renvoyés avec son accord, si la saisie n'a pas changé depuis ; sinon son
  // encodage strict, ou rien s'il est inexprimable.
  [[nodiscard]] std::optional<std::string> RecipientWire(std::string_view typed_utf8) const;
  void Claim(std::int64_t mail_id, const TradeContext& context);
  void Attach(std::uint16_t client_slot, int amount, const TradeContext& context);
  void Send(const ro::ui::MailComposeSend& send, const TradeContext& context);
  void SayNews(MailWindows& windows, const TradeContext& context);
  void SayHeld(const TradeContext& context);
  // `gesture` : ce que demande l'objet décrit, sans objet pour un survol.
  void Note(std::span<const ro::sim::InventoryItem> items, int index, bool hovered, bool described,
            ro::ui::ChatLinkGesture gesture = ro::ui::ChatLinkGesture::kDescribe);
  void LoadIcons();
  void ComposeBox(const MailWindows& windows, const TradeContext& context);
  void ComposeWrite(const MailWindows& windows, const TradeContext& context);

  TradeItems items_;
  const ro::vfs::JobNames* jobs_ = nullptr;
  ro::render::GameImages* images_ = nullptr;
  bool icons_loaded_ = false;
  // Voir `AchievementStage::art_generation_`.
  std::uint64_t icons_generation_ = 0;
  ro::ui::MailCarriedIcons icons_;

  ro::sim::Mailbox mailbox_;
  MailWriteFlow flow_;
  std::uint32_t news_said_ = 0;
  std::uint32_t write_seq_seen_ = 0;
  // Le destinataire de l'écriture accordée, en UTF-8 et en octets du fil.
  std::string granted_recipient_;
  std::string granted_recipient_wire_;
  bool box_shown_ = false;
  bool toggle_asked_ = false;
  std::optional<std::string> compose_asked_;
  std::vector<QueuedAttach> queued_attaches_;
  // L'envoi en vol portait des pièces jointes : son échec les fera refaire.
  bool send_carried_ = false;
  // Les gestes que le courrier a retenus depuis la dernière image.
  bool item_use_held_ = false;
  bool deal_held_ = false;
  // Le dernier refus, local ou du serveur, pour la fenêtre d'écriture.
  std::string compose_error_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;
  std::optional<BugReportContext> bug_report_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ─────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs remplis.
  std::vector<ro::ui::MailRowView> rows_;
  std::vector<ro::sim::InventoryItem> content_items_;
  std::vector<std::string> content_names_;
  std::vector<ro::ui::MailItemView> content_lines_;
  ro::ui::MailContentView content_view_;
  ro::ui::MailboxView box_view_;
  std::vector<ro::sim::InventoryItem> attached_items_;
  std::vector<std::string> attached_names_;
  std::vector<ro::ui::MailAttachmentView> attached_;
  std::vector<ro::ui::MailBagView> bag_;
  std::string checked_job_;
  ro::ui::MailComposeView compose_view_;
};

}  // namespace moonclient
