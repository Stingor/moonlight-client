// L'étage du carnet de chasse MVP : trames, instantané, portes, intentions de
// la fenêtre, import, invitation, alertes, signalement et lignes détachées.

#include "mvp/mvp_tracker_stage.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/net/mvp_tracker.h"
#include "ro/proto/map_packets.h"

namespace moonclient {
namespace {

constexpr std::int64_t kSecondsPerMinute = 60;
// L'heure la plus tardive que la commande de saisie transporte : son champ est
// un entier de 32 bits non signé.
constexpr std::int64_t kMostCommandSeconds = std::numeric_limits<std::uint32_t>::max();

}  // namespace

std::int64_t LocalUnixSeconds() {
  return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count());
}

void MvpTrackerStage::See(MvpTrackerPieces pieces) noexcept {
  pieces_ = pieces;
  // Les noms lisibles venaient peut-être d'une autre table.
  map_label_cache_.clear();
}

void MvpTrackerStage::SeeSaveRoot(std::filesystem::path client_root) {
  client_root_ = std::move(client_root);
  // ⚠ UN FICHIER ILLISIBLE NE SE RÉÉCRIT PLUS : ce que le joueur y corrigerait
  // à la main serait écrasé par une liste vide.
  lines_readable_ = LoadMvpLines(client_root_, lines_);
  ResolveLines();
}

void MvpTrackerStage::StartSession(MvpTrackerWindows& windows) {
  book_.Clear();
  // 🔴 LA PARTIE QUI ENTRE N'A RIEN DEMANDÉ : le serveur la reconnaît parfois
  // dès sa première image, et seul ce réarmement redemande alors l'instantané.
  was_verified_ = false;
  verified_ = false;
  snapshot_armed_ = true;
  toggle_asked_ = false;
  open_asked_ = false;
  open_on_asked_.reset();
  // 🔴 LES RÉPONSES ATTENDUES ÉTAIENT CELLES D'UNE AUTRE LIAISON : aucune
  // n'arrivera, et la première de la partie s'apparierait à tort.
  awaited_.clear();
  notices_.clear();
  result_in_modal_ = false;
  invite_answer_sent_ = false;
  invite_answer_ms_ = 0;
  invite_refusal_ = 0;
  invite_gone_ = false;
  invite_dismissed_ = 0;
  snapshot_reply_open_ = false;
  after_reply_group_ = false;
  handshake_invite_due_ = true;
  // Une alerte déjà sonnée re-sonne pour le personnage suivant.
  fired_.clear();
  lit_alerts_.clear();
  alert_sound_ = false;
  facts_kept_.clear();
  // ⚠ LA VUE POINTAIT DANS LE LIVRE QUI VIENT DE SE VIDER : elle repart vide
  // jusqu'à la prochaine composition.
  view_ = ro::ui::MvpTrackerView{};
  // Les lignes gardent leur clé ; leur créneau attend le prochain catalogue.
  ResolveLines();

  // ⚠ LA FENÊTRE GARDE SON ÉPINGLE ET SON OUVERTURE : seuls ses gestes en cours
  // et ce qu'elle allait demander désignaient la partie d'avant.
  windows.answer = {};
  windows.link_tip.reset();
  windows.badge_lit = false;
  ro::ui::MvpTrackerWindowState& window = windows.window;
  window.confirm_leave = false;
  window.confirm_dissolve = false;
  window.drag_slot = ro::ui::kMvpNoSlotView;
  window.row_dragged = false;
  window.line_drag = -1;
  window.menu_slot = ro::ui::kMvpNoSlotView;
  window.menu_requested = false;
  window.manual_slot = ro::ui::kMvpNoSlotView;
  window.open_manual_popup = false;
  window.focus_slot = ro::ui::kMvpNoSlotView;
  window.focus_scrolled = false;
  window.invite_deferred = false;
}

ro::net::FrameOutcome MvpTrackerStage::ApplyFrame(const ro::net::Frame& frame,
                                                  std::int64_t local_now_s, std::uint32_t now_ms) {
  const std::uint32_t results_before = book_.result_serial();
  const std::uint32_t invite_before = book_.pending_invite();
  const ro::net::FrameOutcome outcome =
      ro::net::ApplyMvpTrackerFrame(frame, book_, local_now_s, now_ms);
  // 🔴 D'OÙ VIENT L'INVITATION. Seule une invitation POUSSÉE est nouvelle :
  // elle lève le congé ; celles qui renvoient la même le tiennent. Le serveur
  // qui la dit fait foi. Quand la trame ne le dit pas — un serveur ancien, ou
  // l'invitation de la poignée de main, partie avant l'annonce de nos
  // capacités —, l'ordre des trames en décide, et cet ordre se suit toujours.
  // Les deux qui renvoient la même :
  // - la POIGNÉE DE MAIN pousse la sienne avant toute trame du groupe ;
  // - la RÉPONSE À L'INSTANTANÉ, d'un seul tenant, s'ouvre sur le catalogue —
  //   qu'aucune autre trame ne porte —, passe par les favoris et les
  //   observations, puis finit par le groupe, suivi IMMÉDIATEMENT de
  //   l'invitation s'il y en a une, sans rien entre eux.
  // ⚠ UN GROUPE POUSSÉ SEUL — exclusion, dissolution, départ depuis une autre
  // session du compte — n'est pas une réponse : l'invitation qui le suit est
  // nouvelle.
  // ⚠ LA RÉPONSE EST SUPPOSÉE ARRIVER D'UN TRAIT, dans la même image : une
  // invitation qui suit son groupe après une avancée est prise pour poussée.
  // Deux confusions en restent : dans la même image, une invitation poussée
  // juste après une réponse qui finit sur le groupe reste congédiée ; une
  // réponse que TCP coupe entre le groupe et l'invitation fait revenir
  // l'invitation congédiée.
  // ⚠ COMME LE DÉCODEUR : tout kind qui n'est ni l'invitation ni le résultat est
  // le groupe.
  bool invitation = false;
  bool group = false;
  bool state = false;
  bool catalogue = false;
  if (ro::proto::ZcMoonlightMvpGroup packet;
      frame.opcode == ro::proto::ZcMoonlightMvpGroup::kOpcode &&
      ro::proto::Decode(frame.bytes, packet)) {
    invitation = packet.kind == static_cast<std::uint8_t>(ro::net::MvpGroupKind::kInvitation);
    group = !invitation && packet.kind != static_cast<std::uint8_t>(ro::net::MvpGroupKind::kResult);
  } else if (ro::proto::ZcMoonlightMvpState head;
             frame.opcode == ro::proto::ZcMoonlightMvpState::kOpcode &&
             ro::proto::Decode(frame.bytes, head)) {
    state = true;
    catalogue = head.kind == static_cast<std::uint8_t>(ro::net::MvpStateKind::kCatalogue);
  }
  if (invitation) {
    const bool from_handshake = std::exchange(handshake_invite_due_, false);
    const ro::sim::MvpInviteOrigin origin = book_.invite_origin();
    const bool pushed = origin == ro::sim::MvpInviteOrigin::kUnknown
                            ? !after_reply_group_ && !from_handshake
                            : origin == ro::sim::MvpInviteOrigin::kPushed;
    if (pushed) {
      invite_dismissed_ = 0;
    }
  } else if (group) {
    handshake_invite_due_ = false;
  }
  after_reply_group_ = group && snapshot_reply_open_;
  snapshot_reply_open_ = catalogue || (snapshot_reply_open_ && state);
  // UNE AUTRE INVITATION, ou plus aucune : la réponse donnée à la précédente ne
  // la concerne pas. L'instantané qui renvoie la même ne change rien.
  if (book_.pending_invite() != invite_before) {
    invite_answer_sent_ = false;
    invite_refusal_ = 0;
    invite_gone_ = false;
  }
  if (book_.result_serial() != results_before) {
    SettleResult(book_.last_result(), now_ms, book_.last_result_tag());
  }
  return outcome;
}

bool MvpTrackerStage::Advance(MvpTrackerWindows& windows, const MvpTrackerContext& context) {
  // Les faits des liens de l'image précédente : leurs menus sont dessinés.
  facts_kept_.clear();
  // 🔴 LA VÉRIFICATION PERDUE, C'EST UNE LIAISON NEUVE : elle ne répondra pas
  // aux commandes d'avant, et la file se vide avant d'envoyer quoi que ce soit.
  if (!context.verified && was_verified_) {
    awaited_.clear();
  }
  // La réponse à l'instantané est arrivée d'un trait : l'invitation qui suit
  // son groupe à une autre image a été poussée — voir `ApplyFrame`.
  after_reply_group_ = false;
  // Une poignée de main à venir repoussera l'invitation en attente : ce sera
  // la même, pas une nouvelle. La réponse d'une liaison perdue ne finira pas.
  if (!context.verified) {
    handshake_invite_due_ = true;
    snapshot_reply_open_ = false;
  }
  verified_ = context.verified;
  now_ms_ = context.now_ms;
  // 🔴 L'INTERRUPTEUR EST LE SEUL À ALLUMER LE CARNET : aucune porte ne le
  // touche. Tel que la partie l'a annoncé au serveur, plus tôt dans l'image.
  const MvpTrackerSettings hunt = context.options.settings();
  enabled_ = hunt.enabled;
  windows.enabled = enabled_;
  bool changed = false;
  if (enabled_) {
    changed = ApplyAnswer(std::exchange(windows.answer, {}), windows, context);
  } else {
    SwitchedOff(windows);
  }

  // ── LES PORTES : éteint, `SwitchedOff` vient de les fermer ──
  if (std::exchange(toggle_asked_, false)) {
    if (windows.window.open) {
      windows.window.open = false;
    } else {
      Open(windows.window);
    }
  }
  if (std::exchange(open_asked_, false)) {
    Open(windows.window);
  }
  if (const std::optional<std::uint16_t> aimed = std::exchange(open_on_asked_, std::nullopt);
      aimed.has_value()) {
    Open(windows.window);
    // Sans créneau, le rang visé s'efface : ouvrir sans viser ne surligne rien.
    windows.window.focus_slot = *aimed;
    windows.window.focus_ms = context.now_ms;
    windows.window.focus_scrolled = false;
  }
  window_shown_ = windows.window.open && !windows.window.was_collapsed;

  // ── L'INSTANTANÉ ──
  // 🔴 À CHAQUE PASSAGE EN VÉRIFIÉ, CARNET ALLUMÉ, et pas seulement à
  // l'ouverture : alertes, icône, tombes et lignes détachées vivent fenêtre
  // fermée. Une demande armée carnet éteint attend l'interrupteur au lieu de se
  // perdre.
  // ⚠ ÉTEINT, LE CARNET SE RÉARME : le serveur cesse alors de diffuser les
  // observations du groupe, et ne renvoie rien quand l'interrupteur revient.
  if (!enabled_) {
    snapshot_armed_ = true;
  }
  if (!context.verified) {
    snapshot_armed_ = true;
    was_verified_ = false;
  } else {
    if (!was_verified_) {
      was_verified_ = true;
      snapshot_armed_ = true;
    }
    // LA PRÉSENCE DES MEMBRES. L'instantané se redemande à intervalle, carnet
    // ouvert et déplié. Le serveur qui écrit nos queues pousse la présence à
    // chaque connexion et déconnexion, carnet allumé — une queue lue dans la
    // session en est la preuve —, mais pas le niveau d'un membre : contre lui,
    // l'intervalle est seulement plus lent. L'instantané renvoie tout, mais
    // rien ne re-sonne : les observations gardent leur heure de signalement.
    const std::uint32_t refresh_ms =
        book_.extensions_seen() ? kMvpPushedPresenceRefreshMs : kMvpPresenceRefreshMs;
    if (hunt.enabled && window_shown_ && context.now_ms - snapshot_ms_ >= refresh_ms) {
      snapshot_armed_ = true;
    }
    if (snapshot_armed_ && hunt.enabled) {
      Send(context.link, ro::net::MvpCommand::kSnapshot, 0, 0);
      snapshot_armed_ = false;
      snapshot_ms_ = context.now_ms;
    }
  }

  const std::int64_t server_now_s = ServerNow(context.local_now_s);
  if (hunt.enabled) {
    DetectAlerts(hunt, server_now_s, context.now_ms);
  }
  windows.badge_lit =
      hunt.enabled && ro::sim::MvpBadgeLit(book_, server_now_s, hunt.alert_lead_min);

  // ── LES LIGNES DÉTACHÉES : chaque catalogue renumérote les créneaux ──
  if (book_.catalog_serial() != lines_resolved_for_ || line_slots_.size() != lines_.size()) {
    ResolveLines();
  }
  // ⚠ LES TAILLES MESURÉES SUIVENT LES LIGNES : une liste raccourcie ne doit
  // pas prêter à la ligne suivante la taille d'une ligne partie.
  if (windows.window.line_sizes.size() > lines_.size()) {
    windows.window.line_sizes.resize(lines_.size());
  }
  if (windows.window.line_drag >= static_cast<int>(lines_.size())) {
    windows.window.line_drag = -1;
  }

  ComposeView(windows, context, hunt);
  windows.view = &view_;
  return changed;
}

void MvpTrackerStage::RequestToggle() noexcept { toggle_asked_ = true; }

void MvpTrackerStage::RequestOpen() noexcept { open_asked_ = true; }

void MvpTrackerStage::RequestOpenOn(std::uint16_t slot_id) noexcept { open_on_asked_ = slot_id; }

void MvpTrackerStage::Invite(std::string_view name_utf8, ro::net::Connection& link) {
  // 🔴 UN GESTE QUI NE PART PAS SE DIT : sinon il se perd sans un mot. Carnet
  // éteint, aucun menu ne l'offre ; un geste d'avant l'extinction se dit aussi.
  if (!enabled_) {
    NoteNotSent(ro::net::MvpCommand::kInvite, name_utf8, ro::ui::MvpTrackerOffText());
    return;
  }
  if (!Send(link, ro::net::MvpCommand::kInvite, 0, 0, name_utf8, std::string(name_utf8))) {
    NoteNotSent(ro::net::MvpCommand::kInvite, name_utf8, ro::ui::MvpLinkPendingText());
  }
}

void MvpTrackerStage::Import(const MvpLink& link, std::string_view sender_utf8,
                             ro::net::Connection& to) {
  const std::uint16_t slot_id = SlotOf(link);
  // Le nom que le lien porte ; à défaut, celui du créneau.
  std::string subject = link.name;
  if (const ro::sim::MvpSlot* slot = book_.FindSlot(slot_id); subject.empty() && slot != nullptr) {
    subject = SlotLabel(*slot);
  }
  // 🔴 LA MÊME RÈGLE QUE LE MENU : l'entrée grisée ne doit pas pouvoir passer
  // par un autre chemin. Le serveur arbitre par précision, et une saisie qu'il
  // ignorerait laisserait croire que le clic a pris. Refusé, rien ne part et
  // rien ne s'ouvre — une ouverture ressemblerait à un succès —, et le chat dit
  // pourquoi.
  if (const char* refusal = ImportRefusal(link); refusal != nullptr) {
    NoteNotSent(ro::net::MvpCommand::kReportManual, subject, refusal);
    return;
  }
  if (!Send(to, ro::net::MvpCommand::kReportManual, slot_id,
            static_cast<std::uint32_t>(link.kill_time_s),
            ro::net::MvpManualReportText(link.tomb_x, link.tomb_y, sender_utf8), subject)) {
    NoteNotSent(ro::net::MvpCommand::kReportManual, subject, ro::ui::MvpLinkPendingText());
    return;
  }
  // Le carnet s'ouvre SUR la ligne : le rang qui change est la seule
  // confirmation qui vaille.
  RequestOpenOn(slot_id);
}

void MvpTrackerStage::PutBackAllLines() {
  lines_.clear();
  ResolveLines();
  SaveLines();
}

int MvpTrackerStage::line_count() const noexcept { return static_cast<int>(lines_.size()); }

std::int64_t MvpTrackerStage::ServerNow(std::int64_t local_now_s) const noexcept {
  return book_.ServerNow(local_now_s);
}

std::uint16_t MvpTrackerStage::SlotOf(const MvpLink& link) const noexcept {
  const ro::sim::MvpSlot* slot =
      book_.FindSlotFor(static_cast<std::uint16_t>(link.monster_id), link.map);
  return slot != nullptr ? slot->slot_id : ro::sim::kMvpNoSlot;
}

bool MvpTrackerStage::TakeAlertSound() noexcept { return std::exchange(alert_sound_, false); }

void MvpTrackerStage::Open(ro::ui::MvpTrackerWindowState& window) {
  // TOUTE OUVERTURE DEMANDÉE MET AU PREMIER PLAN, y compris celle d'un carnet
  // déjà ouvert sous une autre fenêtre : c'est lui qu'on a voulu voir.
  window.open = true;
  window.raise = true;
  snapshot_armed_ = true;
}

void MvpTrackerStage::SwitchedOff(MvpTrackerWindows& windows) {
  // 🔴 ÉTEINT, LE CARNET N'OUVRE RIEN, N'ENVOIE RIEN ET NE MONTRE RIEN : les
  // portes en attente tombent, la fenêtre se ferme, ce qu'elle demandait se
  // perd, et aucune réponse n'est plus attendue — celle qui arriverait ne se
  // dirait pas.
  toggle_asked_ = false;
  open_asked_ = false;
  open_on_asked_.reset();
  windows.answer = {};
  windows.window.open = false;
  awaited_.clear();
  lit_alerts_.clear();
  alert_sound_ = false;
}

bool MvpTrackerStage::ApplyAnswer(const ro::ui::MvpTrackerAction& answer,
                                  MvpTrackerWindows& windows, const MvpTrackerContext& context) {
  using ro::net::MvpCommand;
  ro::net::Connection& link = context.link;
  bool changed = false;
  // 🔴 UN GESTE QUI NE PART PAS SE DIT : la fenêtre l'a offert sur la liaison
  // de l'image précédente, qui a pu tomber depuis. Une ligne pour tous les
  // gestes de l'image, sans sujet ; l'invitation et la saisie ont la leur.
  bool lost = false;
  if (!answer.create_group.empty()) {
    lost |= !Send(link, MvpCommand::kCreateGroup, 0, 0, answer.create_group, answer.create_group);
  }
  if (!answer.invite_member.empty()) {
    Invite(answer.invite_member, link);
  }
  if (!answer.kick_member.empty()) {
    lost |= !Send(link, MvpCommand::kKick, 0, 0, answer.kick_member, answer.kick_member);
  }
  if (answer.leave_group) {
    lost |= !Send(link, MvpCommand::kLeaveGroup, 0, 0);
  }
  // Le serveur arbitre : seul le propriétaire dissout, les autres reçoivent un
  // refus.
  if (answer.dissolve_group) {
    lost |= !Send(link, MvpCommand::kDissolveGroup, 0, 0);
  }
  // 🔴 L'INVITATION ATTEND LA RÉPONSE DU SERVEUR : effacée d'avance, un refus
  // (groupe plein, invitation périmée) la ferait disparaître sans un mot. Le
  // résultat l'efface, ou dit pourquoi elle reste — voir `SettleResult`.
  // Une réponse que le serveur jetterait ne grise rien : rien n'est parti.
  if (answer.accept_invite || answer.decline_invite) {
    if (Send(link, answer.accept_invite ? MvpCommand::kAcceptInvite : MvpCommand::kDeclineInvite, 0,
             0)) {
      invite_answer_sent_ = true;
      invite_answer_ms_ = context.now_ms;
      invite_refusal_ = 0;
      invite_gone_ = false;
    } else {
      lost = true;
    }
  }
  // « FERMER » une invitation refusée : rien ne part, et ni l'instantané ni la
  // poignée de main qui la renverraient ne la remontrent. Une invitation
  // poussée, elle, est un nouveau geste de son groupe — voir `ApplyFrame`.
  if (answer.dismiss_invite) {
    invite_dismissed_ = book_.pending_invite();
    book_.ClearInvitation();
    invite_answer_sent_ = false;
    invite_refusal_ = 0;
    invite_gone_ = false;
  }
  // ⚠ L'ÉTOILE NE BASCULE QU'AU RETOUR DES FAVORIS : pas d'optimisme.
  if (answer.favorite_slot != ro::ui::kMvpNoSlotView) {
    lost |= !Send(link, MvpCommand::kFavorite, answer.favorite_slot, answer.favorite_on ? 1U : 0U);
  }
  if (lost) {
    NoteGesturesNotSent(ro::ui::MvpLinkPendingText());
  }
  if (answer.manual_slot != ro::ui::kMvpNoSlotView) {
    const ro::sim::MvpSlot* slot = book_.FindSlot(answer.manual_slot);
    const std::string subject = slot != nullptr ? std::string(SlotLabel(*slot)) : std::string();
    if (!Send(link, MvpCommand::kReportManual, answer.manual_slot,
              static_cast<std::uint32_t>(answer.manual_kill_time_s), {}, subject)) {
      NoteNotSent(MvpCommand::kReportManual, subject, ro::ui::MvpLinkPendingText());
    }
  }
  // LE BANDEAU D'ALERTE CLIQUÉ : le carnet sur la ligne, et l'alerte s'éteint.
  if (answer.alert_slot != ro::ui::kMvpNoSlotView) {
    RequestOpenOn(answer.alert_slot);
    std::erase_if(lit_alerts_,
                  [&](const LitAlert& lit) { return lit.slot_id == answer.alert_slot; });
  }
  if (answer.delay_shown.has_value()) {
    changed |= context.options.SetShowDelay(*answer.delay_shown);
  }
  if (answer.pin_slot != ro::ui::kMvpNoSlotView) {
    PinSlot(answer.pin_slot, answer.pin_x, answer.pin_y);
  }
  const auto line_count = static_cast<int>(lines_.size());
  if (answer.moved_line >= 0 && answer.moved_line < line_count) {
    MvpPinnedLine& moved = lines_[static_cast<std::size_t>(answer.moved_line)];
    moved.x = answer.moved_x;
    moved.y = answer.moved_y;
  }
  // Un simple clic sans déplacement écrit aussi : le relâchement est le geste.
  if (answer.lines_released) {
    SaveLines();
  }
  if (answer.removed_line >= 0 && answer.removed_line < line_count) {
    const auto removed = static_cast<std::size_t>(answer.removed_line);
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(removed));
    // ⚠ LES TAILLES ET LE GLISSER DE LA FENÊTRE SONT RANGÉS PAR LIGNE : ils
    // suivent l'effacement, sinon chaque ligne d'après hériterait de la taille
    // de sa voisine.
    std::vector<ImVec2>& sizes = windows.window.line_sizes;
    if (removed < sizes.size()) {
      sizes.erase(sizes.begin() + static_cast<std::ptrdiff_t>(removed));
    }
    if (windows.window.line_drag == answer.removed_line) {
      windows.window.line_drag = -1;
    } else if (windows.window.line_drag > answer.removed_line) {
      --windows.window.line_drag;
    }
    ResolveLines();
    SaveLines();
  }
  return changed;
}

void MvpTrackerStage::DetectAlerts(const MvpTrackerSettings& hunt, std::int64_t server_now_s,
                                   std::uint32_t now_ms) {
  const std::int64_t lead_s = static_cast<std::int64_t>(hunt.alert_lead_min) * kSecondsPerMinute;
  // Une sonnerie s'oublie quand son alerte n'a plus lieu d'être : fenêtre
  // passée, créneau disparu, ou observation remplacée par une plus fraîche —
  // qui sonnera pour son compte.
  std::erase_if(fired_, [&](const FiredAlert& fired) {
    const std::optional<ro::sim::MvpWindow> window = book_.WindowOf(fired.slot_id);
    const ro::sim::MvpObservation* observation = book_.FindObservation(fired.slot_id);
    return !window.has_value() || observation == nullptr ||
           observation->reported_at_s != fired.reported_at_s ||
           server_now_s > window->from_s + kMvpAlertStaleS;
  });
  for (const std::uint16_t slot_id : book_.favorites()) {
    const std::optional<ro::sim::MvpWindow> window = book_.WindowOf(slot_id);
    // Pas encore dans le préavis, ou trop vieux pour être une nouvelle.
    if (!window.has_value() || server_now_s + lead_s < window->from_s ||
        server_now_s > window->from_s + kMvpAlertStaleS) {
      continue;
    }
    const ro::sim::MvpObservation* observation = book_.FindObservation(slot_id);
    if (observation == nullptr) {
      continue;
    }
    // 🔴 UNE SONNERIE PAR OBSERVATION : une observation plus fraîche — un miroir
    // qui précise un kill — a le droit de re-sonner, la même non.
    if (std::ranges::any_of(fired_, [&](const FiredAlert& fired) {
          return fired.slot_id == slot_id && fired.reported_at_s == observation->reported_at_s;
        })) {
      continue;
    }
    fired_.push_back(FiredAlert{.slot_id = slot_id, .reported_at_s = observation->reported_at_s});
    // LES ALERTES S'EMPILENT : deux favoris qui entrent ensemble se montrent
    // tous deux. Déjà allumé, le créneau se rallume EN QUEUE, comme la plus
    // récente qu'il est devenu ; au-delà de la pile, la plus ancienne sort.
    std::erase_if(lit_alerts_, [slot_id](const LitAlert& lit) { return lit.slot_id == slot_id; });
    lit_alerts_.push_back(LitAlert{.slot_id = slot_id, .lit_ms = now_ms});
    if (lit_alerts_.size() > kMvpMostAlertsShown) {
      lit_alerts_.erase(lit_alerts_.begin());
    }
    // ⚠ UN SEUL SON PAR IMAGE, quel que soit le nombre de favoris qui sonnent.
    if (hunt.alert_sound) {
      alert_sound_ = true;
    }
  }
}

const char* MvpTrackerStage::ImportRefusal(const MvpLink& link) const {
  // ⚠ L'ORDRE EST FIXE : l'interrupteur d'abord — éteint, le carnet n'envoie
  // rien —, puis la liaison — hors vérification, le serveur jette la commande
  // sans répondre —, puis sans groupe, même quand le créneau est introuvable.
  if (!enabled_) {
    return ro::ui::MvpTrackerOffText();
  }
  if (!verified_) {
    return ro::ui::MvpLinkPendingText();
  }
  if (book_.group().group_id == 0) {
    return ro::core::Tr("Créez un groupe de chasse pour tenir un carnet.");
  }
  const ro::sim::MvpSlot* slot =
      book_.FindSlotFor(static_cast<std::uint16_t>(link.monster_id), link.map);
  if (slot == nullptr) {
    return ro::core::Tr("Ce créneau n'est pas dans votre catalogue.");
  }
  // Le serveur arbitre par PRÉCISION : une saisie ne remplace jamais une tombe,
  // un kill ou un Convex Mirror — ni une source inconnue plus haute.
  if (const ro::sim::MvpObservation* mine = book_.FindObservation(slot->slot_id);
      mine != nullptr &&
      mine->source_raw > static_cast<std::uint8_t>(ro::sim::MvpSource::kManual)) {
    return ro::core::Tr("Votre groupe en sait déjà plus : il l'a vu de ses yeux.");
  }
  // Un lien qui ne porte que l'instant exact ne s'importe pas : la saisie ne
  // transporte qu'une heure de mort.
  //
  // ⚠ ET UNE HEURE QUE LA COMMANDE NE PORTE PAS n'en est pas une : tronquée, elle
  // deviendrait une autre heure, plausible, que le serveur accepterait.
  if (link.kill_time_s <= 0) {
    return ro::core::Tr("Ce lien ne porte pas d'heure de mort.");
  }
  if (link.kill_time_s > kMostCommandSeconds) {
    return ro::core::Tr("L'heure de mort de ce lien est hors de portée de la saisie.");
  }
  return nullptr;
}

void MvpTrackerStage::PinSlot(std::uint16_t slot_id, int x, int y) {
  const ro::sim::MvpSlot* slot = book_.FindSlot(slot_id);
  if (slot == nullptr) {
    return;
  }
  // UN CRÉNEAU, UNE LIGNE, reconnue par sa clé stable.
  if (std::ranges::any_of(lines_, [slot](const MvpPinnedLine& line) {
        return line.mob_id == slot->mob_id && line.map == slot->map;
      })) {
    return;
  }
  lines_.push_back(MvpPinnedLine{.mob_id = slot->mob_id, .map = slot->map, .x = x, .y = y});
  ResolveLines();
  SaveLines();
}

void MvpTrackerStage::ResolveLines() {
  // Le monstre ET la carte : le même MVP vit sur plusieurs cartes, et un créneau
  // scripté n'a que sa carte.
  line_slots_.assign(lines_.size(), ro::sim::kMvpNoSlot);
  const std::span<const ro::sim::MvpSlot> slots = book_.slots();
  for (std::size_t line = 0; line < lines_.size(); ++line) {
    const auto found = std::ranges::find_if(slots, [&](const ro::sim::MvpSlot& slot) {
      return slot.mob_id == lines_[line].mob_id && slot.map == lines_[line].map;
    });
    if (found != slots.end()) {
      line_slots_[line] = found->slot_id;
    }
  }
  lines_resolved_for_ = book_.catalog_serial();
}

void MvpTrackerStage::SaveLines() const {
  // Sans dossier — un banc d'essai —, ou après une lecture ratée : rien ne
  // s'écrit.
  if (client_root_.empty() || !lines_readable_) {
    return;
  }
  static_cast<void>(SaveMvpLines(client_root_, lines_));
}

}  // namespace moonclient
