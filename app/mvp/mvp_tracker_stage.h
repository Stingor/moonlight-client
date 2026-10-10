#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/mvp_tracker.h"
#include "ro/net/world_update.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/game_images.h"
#include "ro/sim/mvp_tracker.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/mvp_tracker_window.h"
#include "ro/vfs/map_display_names.h"

#include "chat/chat_mvp_link.h"
#include "mvp/mvp_line_file.h"
#include "options/mvp_tracker_options.h"
#include "world/monster_sprite.h"

namespace moonclient {

// L'ÉTAGE DU CARNET DE CHASSE MVP : le livre que le réseau remplit, la vue que
// la fenêtre dessine, ce qu'on fait de ce qu'elle demande — et tout ce qui
// gravite autour : alertes, icône du menu, tombes de la minimap, liens
// `<MVPL>`, lignes détachées.
//
// 🔴 IL VIT DÈS LA CONSTRUCTION DE LA PARTIE : l'invitation en attente arrive
// pendant la poignée de main, avant toute ouverture, et la modale s'impose
// fenêtre fermée.
//
// 🔴 L'INTERRUPTEUR ÉTEINT LE CARNET POUR DE BON : rien ne s'envoie, rien ne
// s'ouvre, rien ne se montre — ni fenêtre, ni alerte, ni tombe, ni ligne
// détachée, ni invitation. Les trames qui arrivent encore remplissent le livre
// sans rien montrer, et les lignes détachées restent dans leur fichier :
// rallumé, tout revient, et l'instantané se redemande.

// L'heure du poste, en secondes UNIX.
[[nodiscard]] std::int64_t LocalUnixSeconds();

// LES PIÈCES DU CARNET dans les fenêtres de l'interface.
struct MvpTrackerWindows {
  ro::ui::MvpTrackerWindowState window;
  const ro::ui::MvpTrackerView* view = nullptr;
  ro::ui::MvpTrackerAction answer;
  // L'aperçu d'un `<MVPL>` survolé : posé par le suivi des liens, consommé —
  // et effacé — par le dessin.
  std::optional<ro::ui::MvpLinkTip> link_tip;
  // L'icône du menu s'allume : un favori entre dans sa fenêtre.
  bool badge_lit = false;
  // L'INTERRUPTEUR, relevé à chaque avancée : éteint, l'icône du menu disparaît.
  bool enabled = false;
};

// CE QUE L'ÉTAGE EMPRUNTE À LA PARTIE. ⚠ TOUT PEUT MANQUER : une installation
// incomplète donne un carnet sans art, jamais un carnet qui plante.
struct MvpTrackerPieces {
  // Nul : aucun sprite.
  ro::render::ActorWardrobe* wardrobe = nullptr;
  // Nul : ni plan de carte ni étoile.
  ro::render::GameImages* images = nullptr;
  // Nul : la colonne Carte ne montre que le nom interne.
  const ro::vfs::MapDisplayNames* map_names = nullptr;
};

// CE QUE L'ÉTAGE REÇOIT À CHAQUE IMAGE.
struct MvpTrackerContext {
  ro::net::Connection& link;
  MvpTrackerOptions& options;
  // La poignée de main du serveur est vérifiée.
  bool verified = false;
  std::int64_t local_now_s = 0;
  std::uint32_t now_ms = 0;
  // Le côté de la case d'un sprite, en pixels d'écran : `kMvpSpritePixels` à
  // l'échelle de l'interface.
  int sprite_box_px = static_cast<int>(ro::ui::kMvpSpritePixels);
  // Le nom de notre personnage, en UTF-8 ; vide hors du jeu.
  std::string_view own_name;
};

// La mémoire des alertes déjà sonnées : une sonnerie par OBSERVATION.
inline constexpr std::size_t kMvpMostFiredAlerts = 32;
// Plus vieille qu'une heure après l'ouverture de sa fenêtre, une échéance n'est
// plus une nouvelle.
inline constexpr std::int64_t kMvpAlertStaleS = 3600;
// Les sprites NEUFS composés par image, table et lignes confondues.
inline constexpr int kMvpSpriteLoadsPerFrame = 2;
// Le pas de la rotation du sprite d'un créneau scripté.
inline constexpr std::uint32_t kMvpScriptedSpriteTurnMs = 1500;
// Les alertes allumées montrées ensemble ; au-delà, la plus ancienne sort.
inline constexpr std::size_t kMvpMostAlertsShown = 4;
// Le rythme de l'instantané redemandé, carnet ouvert et déplié, contre un
// serveur qui ne pousse pas la présence des membres.
inline constexpr std::uint32_t kMvpPresenceRefreshMs = 30000;
// Le même rythme contre un serveur qui pousse la présence, mais pas le niveau
// des membres : lui seul se rafraîchit encore.
inline constexpr std::uint32_t kMvpPushedPresenceRefreshMs = 300000;
// Ce qu'on attend la réponse à une commande avant que les boutons de
// l'invitation se rendent.
inline constexpr std::uint32_t kMvpAnswerPatienceMs = 10000;
// Au-delà, une commande ne s'apparie plus à rien : sa réponse est tenue pour
// perdue. ⚠ BIEN PLUS LONG QUE LA PATIENCE DES BOUTONS : une réponse seulement
// EN RETARD — le serveur écrit en base avant de répondre — purgée trop tôt
// prêterait sa réponse à la commande suivante.
inline constexpr std::uint32_t kMvpLostAnswerMs = 60000;
// Les commandes dont on attend le résultat ; au-delà, la plus ancienne sort.
inline constexpr std::size_t kMvpMostAwaitedResults = 16;

// SOMMES-NOUS PROPRIÉTAIRE ? Le serveur qui dit notre compte le dit : notre
// compte est-il le propriétaire. Contre un serveur qui ne le dit pas, on cherche
// notre personnage parmi les membres, par son nom. Vide : on ne sait pas (sans
// groupe ; ou, en repli, nom vide ou notre tête absente — voir l'ADR 0099).
[[nodiscard]] std::optional<bool> MvpOwnership(const ro::sim::MvpGroup& group,
                                               std::string_view own_name_utf8);

// CE QUE FAIT LE MAJ+CLIC SUR LE NOM D'UN RANG, déjà traduit : partager le
// respawn si le rang porte une heure, le lien seul sinon — ou sans rang.
[[nodiscard]] const char* MvpNameHint(const ro::ui::MvpSlotView* row);

class MvpTrackerStage {
 public:
  void See(MvpTrackerPieces pieces) noexcept;
  // Lit les lignes détachées sous `savedata\`.
  void SeeSaveRoot(std::filesystem::path client_root);
  // UNE NOUVELLE PARTIE : ce que le serveur avait dit — catalogue,
  // observations, favoris, groupe, invitation, horloge —, les alertes déjà
  // sonnées, les portes en attente et les gestes de la fenêtre s'effacent ;
  // l'instantané se redemandera. Restent les lignes détachées, l'épingle et
  // l'ouverture de la fenêtre, qui sont au joueur.
  void StartSession(MvpTrackerWindows& windows);

  // `ZC 0x0F31` et `ZC 0x0F32`. ⚠ ACTIF DÈS LA CONSTRUCTION : voir l'en-tête.
  // Un résultat reçu s'apparie ici à sa commande — voir `SettleResult`.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 std::int64_t local_now_s, std::uint32_t now_ms);

  // À CHAQUE IMAGE : les demandes en attente (ouvrir, basculer, viser),
  // l'instantané, les réponses de la fenêtre, les alertes, le signalement, les
  // lignes, la vue. Rend vrai si un réglage a changé.
  [[nodiscard]] bool Advance(MvpTrackerWindows& windows, const MvpTrackerContext& context);

  // LES PORTES, jouées à l'image suivante, dans `Advance`, qui a les réglages
  // en main. 🔴 CARNET ÉTEINT, AUCUNE N'OUVRE RIEN : seule la case des options
  // le rallume.
  // Le raccourci et l'icône.
  void RequestToggle() noexcept;
  // Le bouton des options, « Accepter » une invitation.
  void RequestOpen() noexcept;
  // Un lien `<MVPL>`, un import. `ro::sim::kMvpNoSlot` : ouvrir sans viser.
  void RequestOpenOn(std::uint16_t slot_id) noexcept;

  // Envoyé tout de suite. Hors liaison vérifiée, rien ne part, et le chat le
  // dit.
  void Invite(std::string_view name_utf8, ro::net::Connection& link);
  // L'IMPORT D'UN `<MVPL>` : la saisie de sa mort au carnet du groupe, avec la
  // tombe et l'expéditeur, puis l'ouverture sur le créneau. Un lien que la
  // règle des faits refuse n'envoie rien, n'ouvre rien, et le chat dit
  // pourquoi.
  void Import(const MvpLink& link, std::string_view sender_utf8, ro::net::Connection& to);
  // « Tout remettre au carnet (n) ».
  void PutBackAllLines();
  [[nodiscard]] int line_count() const noexcept;
  // LES LIGNES À ÉCRIRE DANS LE CHAT, déjà traduites : le résultat d'une
  // invitation ou d'une saisie, carnet fermé ; le geste qui n'a pas pu partir.
  // Consommées.
  [[nodiscard]] std::vector<std::string> TakeNotices();

  // ── LECTURES DES ACCROCHES ──
  [[nodiscard]] const ro::sim::MvpTrackerBook& book() const noexcept { return book_; }
  // La liaison était vérifiée à la dernière avancée : un geste qui envoie peut
  // partir. Les menus qui en offrent un s'y fient.
  [[nodiscard]] bool link_ready() const noexcept { return verified_; }
  [[nodiscard]] std::int64_t ServerNow(std::int64_t local_now_s) const noexcept;
  // Le créneau d'un lien, repli scripté compris ; `ro::sim::kMvpNoSlot` sinon.
  [[nodiscard]] std::uint16_t SlotOf(const MvpLink& link) const noexcept;
  // Ce que le menu d'un `<MVPL>` sait d'avance. ⚠ GARDÉ JUSQU'AU PROCHAIN
  // `Advance` : le pointeur est stable le temps de l'image. Jamais nul.
  [[nodiscard]] const ro::ui::MvpLinkMenuFacts* FactsFor(const MvpLink& link);
  // L'heure locale dit si le lien est échu.
  [[nodiscard]] ro::ui::MvpLinkTip TipOf(const MvpLink& link, std::int64_t local_now_s) const;
  // Les tombes de `map` — aucune si `shown` est faux —, gardées jusqu'au
  // prochain appel.
  [[nodiscard]] std::span<const ro::ui::MinimapTomb> TombsOn(std::string_view map, bool shown,
                                                             std::int64_t local_now_s);
  // L'alerte a sonné à cette image : une fois, quel que soit le nombre de
  // favoris.
  [[nodiscard]] bool TakeAlertSound() noexcept;

 private:
  // L'OUVERTURE, carnet allumé : la fenêtre s'ouvre et passe au premier plan,
  // et l'instantané se redemande.
  void Open(ro::ui::MvpTrackerWindowState& window);
  // CARNET ÉTEINT, à chaque avancée : portes, fenêtre, gestes, réponses
  // attendues et alertes allumées tombent.
  void SwitchedOff(MvpTrackerWindows& windows);
  // Ce que la fenêtre a demandé à l'image précédente. Rend vrai si un réglage
  // a changé (colonne « Délai »).
  [[nodiscard]] bool ApplyAnswer(const ro::ui::MvpTrackerAction& answer, MvpTrackerWindows& windows,
                                 const MvpTrackerContext& context);
  // `mvp_tracker_results.cpp` : envoie avec un identifiant, et retient ce que
  // la réponse dira pour les commandes qui en reçoivent une (2 à 8 et 10). Rend
  // faux si rien n'est parti : hors vérification, le serveur jetterait la
  // commande sans répondre.
  bool Send(ro::net::Connection& link, ro::net::MvpCommand command, std::uint32_t a,
            std::uint32_t b, std::string_view text_utf8 = {}, std::string subject = {});
  // Le résultat va à la commande que son étiquette désigne ; sans étiquette —
  // un serveur qui ne la rend pas —, à la plus ancienne en attente.
  void SettleResult(std::uint8_t code, std::uint32_t now_ms,
                    const std::optional<ro::sim::MvpResultTag>& tag);
  // La sonnerie des favoris qui entrent dans leur préavis.
  void DetectAlerts(const MvpTrackerSettings& hunt, std::int64_t server_now_s,
                    std::uint32_t now_ms);
  // La raison, déjà traduite, qui refuse l'import d'un lien ; nulle s'il passe.
  [[nodiscard]] const char* ImportRefusal(const MvpLink& link) const;
  // La ligne du chat d'une invitation ou d'une saisie qui n'est pas partie.
  void NoteNotSent(ro::net::MvpCommand command, std::string_view subject, const char* reason);
  // La ligne du chat des autres gestes de l'image qui ne sont pas partis, une
  // pour tous, sans sujet.
  void NoteGesturesNotSent(const char* reason);
  // LES LIGNES DÉTACHÉES : un créneau posé, le créneau de chacune, l'écriture.
  void PinSlot(std::uint16_t slot_id, int x, int y);
  void ResolveLines();
  void SaveLines() const;
  // Le libellé d'un créneau (nom, art scripté, repli), le nom lisible d'une
  // carte.
  [[nodiscard]] std::string_view SlotLabel(const ro::sim::MvpSlot& slot) const;
  [[nodiscard]] std::string_view MapLabel(const std::string& map);
  // LA VUE, refaite à chaque image : `mvp_tracker_compose.cpp`.
  void ComposeView(MvpTrackerWindows& windows, const MvpTrackerContext& context,
                   const MvpTrackerSettings& hunt);
  void ComposeSlots(std::uint32_t now_ms);
  void ComposeAlerts(std::uint32_t now_ms);
  void ComposeSprites(const std::vector<std::uint16_t>& wanted, const MvpTrackerContext& context,
                      const MvpTrackerSettings& hunt);
  [[nodiscard]] ro::ui::SkinImage ImageAt(std::string_view path);

  ro::sim::MvpTrackerBook book_;
  MvpTrackerPieces pieces_;
  std::filesystem::path client_root_;
  // LES LIGNES DÉTACHÉES, et le créneau que chacune désigne, résolu à chaque
  // catalogue reçu.
  std::vector<MvpPinnedLine> lines_;
  std::vector<std::uint16_t> line_slots_;
  std::uint32_t lines_resolved_for_ = 0;
  bool lines_readable_ = true;
  // LES PORTES ET L'INSTANTANÉ.
  bool snapshot_armed_ = true;
  bool was_verified_ = false;
  bool toggle_asked_ = false;
  bool open_asked_ = false;
  std::optional<std::uint16_t> open_on_asked_;
  std::uint32_t snapshot_ms_ = 0;
  // Ouverte et dépliée, à la dernière avancée.
  bool window_shown_ = false;
  // La vérification et l'heure de la dernière avancée : les envois faits hors
  // d'elle — inviter, importer — s'y fient.
  bool verified_ = false;
  std::uint32_t now_ms_ = 0;
  // L'interrupteur à la dernière avancée : les gestes faits hors d'elle —
  // inviter, importer, les faits d'un lien — s'y fient.
  bool enabled_ = false;
  // LES RÉPONSES ATTENDUES, dans l'ordre d'envoi.
  struct AwaitedResult {
    ro::net::MvpCommand command = ro::net::MvpCommand::kSnapshot;
    std::string subject;
    bool closed_at_send = false;
    std::uint32_t sent_ms = 0;
    std::uint32_t request_id = 0;
  };
  std::deque<AwaitedResult> awaited_;
  // ⚠ JAMAIS REMIS À ZÉRO, même d'une partie à l'autre : une réponse en retard
  // d'une liaison passée ne peut ainsi désigner aucune commande de la suivante.
  std::uint32_t last_request_id_ = 0;
  std::vector<std::string> notices_;
  // Le dernier résultat répondait à « Accepter » ou « Refuser » : la modale le
  // dit déjà, le bandeau rouge se tait.
  bool result_in_modal_ = false;
  // L'INVITATION : réponse partie, refus, invitation morte, invitation
  // congédiée.
  bool invite_answer_sent_ = false;
  std::uint32_t invite_answer_ms_ = 0;
  std::uint8_t invite_refusal_ = 0;
  bool invite_gone_ = false;
  std::uint32_t invite_dismissed_ = 0;
  // D'OÙ VIENT UNE TRAME D'INVITATION qui ne le dit pas — voir `ApplyFrame`.
  // Trois témoins, dans l'ordre, tenus à jour même quand la trame le dit :
  // la réponse à un instantané a commencé sans finir ; la trame précédente
  // était le groupe d'une réponse, et rien ne s'est avancé depuis ;
  // l'invitation que pousse la poignée de main est encore attendue.
  bool snapshot_reply_open_ = false;
  bool after_reply_group_ = false;
  bool handshake_invite_due_ = true;
  // LES ALERTES déjà sonnées, par créneau et par observation.
  struct FiredAlert {
    std::uint16_t slot_id = 0;
    std::int64_t reported_at_s = 0;
  };
  std::array<FiredAlert, kMvpMostFiredAlerts> fired_{};
  std::size_t fired_count_ = 0;
  // LES ALERTES ALLUMÉES, la plus ancienne en tête.
  struct LitAlert {
    std::uint16_t slot_id = 0;
    std::uint32_t lit_ms = 0;
  };
  std::vector<LitAlert> lit_alerts_;
  bool alert_sound_ = false;
  // LA COMPOSITION : ce que les `span` de la vue pointent.
  ro::ui::MvpTrackerView view_;
  std::vector<ro::ui::MvpSlotView> slot_views_;
  std::vector<ro::ui::MvpMemberView> member_views_;
  std::vector<ro::ui::MvpPinnedLineView> line_views_;
  std::vector<ro::ui::MvpSpriteArt> sprite_art_;
  std::vector<ro::ui::MvpAlertView> alert_views_;
  // ⚠ RÉSERVÉ AVANT LES VUES, qui y pointent.
  std::vector<std::string> alert_clocks_;
  // UNE INSTANCE PAR CLASSE de monstre.
  std::map<std::uint16_t, MonsterSprite> sprites_;
  std::string preview_map_;
  // ⚠ UNE FILE, pas un vecteur : les menus POINTENT dedans, et un vecteur qui
  // grandit les déplacerait.
  std::deque<ro::ui::MvpLinkMenuFacts> facts_kept_;
  std::vector<ro::ui::MinimapTomb> tombs_;
  // ⚠ RÉSERVÉ AVANT LES VUES, qui y pointent.
  std::vector<std::string> tomb_texts_;
  std::map<std::string, std::string, std::less<>> map_label_cache_;
};

}  // namespace moonclient
