#include "chat/chat_tab_view.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <imgui.h>

#include "ro/core/decimal_text.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"

#include "chat/chat_line_color.h"
#include "chat/chat_tab_file.h"

namespace moonclient {
namespace {

using ro::sim::ChatKind;

// L'IDENTITÉ D'UNE LIGNE POUR LE CACHE DU FIL — voir `ro::ui::ChatLineView::id`.
//
// ⚠ EN TÊTE, CE QUI CHANGE L'ASPECT D'UNE MÊME LIGNE : voir `LookBits`.
// ⚠ PUIS, JUSTE DESSOUS, LA SOURCE : la conversation de la partie vient APRÈS
// le journal de la précédente, comme dans le fil, parce que le cache exige des
// identités qui croissent avec lui.
constexpr int kLiveLineBit = 55;
constexpr std::uint64_t kLiveLine = std::uint64_t{1} << kLiveLineBit;
// Et pour le journal, l'époque de sa lecture, puis le rang de la ligne.
constexpr int kPastEpochShift = 32;
constexpr std::uint64_t kPastEpochMask = (std::uint64_t{1} << (kLiveLineBit - kPastEpochShift)) - 1;

// Pas d'étiquette : hors diagnostic, aucune ligne n'en porte.
constexpr std::string_view kNoTag;

// « Aucun onglet ». ⚠ PAS ZÉRO : zéro est le rang du PREMIER onglet, et le
// confondre avec « rien » ferait agir sur lui tous les gestes qui n'ont pas
// eu lieu.
constexpr std::size_t kNoTab = std::numeric_limits<std::size_t>::max();

// Le gris du repère qui sépare deux sessions. C'est celui des en-têtes du
// client : il se lit sans attirer l'œil, ce qui est exactement son rôle.
constexpr ImU32 kPastMarkColor = IM_COL32(155, 155, 155, 255);

// L'expéditeur d'un `<MVPL>` relu dans le journal. ⚠ LE JOURNAL N'ÉCRIT PAS LE
// GENRE, mais la couleur résolue le porte : une ligne « chat public » de la
// teinte du serveur est une réponse du serveur, et elle n'a pas d'auteur.
// 🔴 UN CHUCHOTEMENT SE RÉSOUT PAR SON SENS : le journal n'en garde que
// l'en-tête, d'où se relisent le sens et le pair. Voir `MvpPastWhisperSenderOf`
// pour l'écho d'un envoi.
[[nodiscard]] std::string PastMvpSenderOf(const PastChatLine& line) {
  if (FindMvpLink(line.text, 0) == std::string_view::npos) {
    return {};
  }
  if (line.type == ro::sim::ChatType::kWhisper) {
    return MvpPastWhisperSenderOf(WhisperFactsOfLogText(line.text));
  }
  const bool from_server = line.type == ro::sim::ChatType::kPublicChat &&
                           line.color == ShownRgbOfChatLine(ChatKind::kServer, 0);
  return from_server ? std::string() : MvpSenderOf(line.type, line.text);
}

// L'étiquette du GENRE d'une ligne, montrée en tête au diagnostic avant celle
// de son type. ⚠ NON TRADUITE : c'est un repère de mise au point, que le
// joueur recopie dans un rapport, et il doit s'y lire pareil dans toutes les
// langues.
[[nodiscard]] std::string_view KindTag(ChatKind kind) {
  switch (kind) {
    case ChatKind::kPublic:
      return "parole";
    case ChatKind::kSelf:
      return "ma_parole";
    case ChatKind::kServer:
      return "serveur";
    case ChatKind::kAnnouncement:
      return "annonce";
    case ChatKind::kWhisper:
      return "chuchotement";
    case ChatKind::kParty:
      return "groupe";
    case ChatKind::kGuild:
      return "guilde";
    case ChatKind::kChannel:
      return "canal";
    case ChatKind::kClan:
      return "clan";
    case ChatKind::kItem:
      return "objet";
    case ChatKind::kBattle:
      return "combat";
    case ChatKind::kPartyBattle:
      return "combat_du_groupe";
    case ChatKind::kExp:
      return "experience";
    case ChatKind::kCall:
      return "appel_d_urgence";
    case ChatKind::kBattlefield:
      return "champ_de_bataille";
  }
  return {};
}

// Combien de secondes dans une minute, une heure, un jour. Nommées plutôt
// qu'écrites : un 3600 au milieu d'une division ne dit pas ce qu'il divise.
constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr int kSecondsPerDay = 24 * kSecondsPerHour;
constexpr std::uint32_t kMsPerSecond = 1000;

// L'heure locale, telle que le système la donne. Rend faux si elle est
// illisible — une date absente vaut mieux qu'une date inventée.
[[nodiscard]] bool LocalNow(std::tm* into) {
  const std::time_t stamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#ifdef _WIN32
  return localtime_s(into, &stamp) == 0;
#else
  return localtime_r(&stamp, into) != nullptr;
#endif
}

// La date et l'heure, telles que le fichier les écrit et que le repère les
// montre. ⚠ ELLE N'EST PAS DÉCORATIVE : l'heure d'une ligne ment sur son jour,
// et « 21:12 » se lit comme ce soir alors que c'était hier.
[[nodiscard]] std::string StampNow() {
  std::tm local{};
  if (!LocalNow(&local)) {
    return {};
  }
  // Six champs de `std::tm`, que le tampon tient même à leur pire valeur.
  std::array<char, 6 * ro::core::kDecimalTextBytes<int>> text{};
  std::snprintf(text.data(), text.size(), "%04d-%02d-%02d %02d:%02d:%02d", local.tm_year + 1900,
                local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
  return std::string(text.data());
}

// Le repère qui coiffe une session, à l'écran comme dans le journal. ⚠ UNE
// SEULE FORME : deux libellés pour la même chose se répondraient mal dans un
// fichier que le joueur relit à côté de sa fenêtre.
[[nodiscard]] std::string SessionMark(std::string_view stamp) {
  std::array<char, 96> mark{};
  if (stamp.empty()) {
    std::snprintf(mark.data(), mark.size(), "%s", ro::core::Tr("---- session précédente ----"));
  } else {
    std::snprintf(mark.data(), mark.size(), ro::core::Tr("---- session du %s ----"),
                  std::string(stamp).c_str());
  }
  return std::string(mark.data());
}

// La DATE seule d'un horodatage « AAAA-MM-JJ hh:mm:ss ». Le journal se range par
// jour : c'est la date qu'on a en tête en retournant chercher ce qui s'est dit.
constexpr std::size_t kDateTextSize = 10;

// La seconde du jour qu'il est MAINTENANT, heure locale.
[[nodiscard]] int DaySecondNow() {
  std::tm local{};
  if (!LocalNow(&local)) {
    return 0;
  }
  return local.tm_hour * kSecondsPerHour + local.tm_min * kSecondsPerMinute + local.tm_sec;
}

// L'APPARENCE D'UNE FENÊTRE : la générale, ou celle de son onglet actif quand
// il en a une à lui.
//
// ⚠ LE DRAPEAU COMMANDE TOUT : sans lui, un onglet garderait la copie des
// réglages du jour où on les a touchés, et cesserait de suivre les suivants.
[[nodiscard]] ro::ui::ChatAppearance LookOf(const ro::ui::ChatAppearance& general,
                                            const TabLook& own) {
  if (!own.own) {
    return general;
  }
  ro::ui::ChatAppearance mine = general;
  // ⚠ BORNÉ ICI, PAS À L'ÉCRITURE : un fichier retouché à la main peut porter
  // n'importe quoi, et un texte à zéro pour cent ne se lirait plus du tout.
  mine.font_percent = std::clamp(own.font_percent, ro::ui::kSmallestChatFontPercent,
                                 ro::ui::kLargestChatFontPercent);
  mine.padding_pixels = std::clamp(own.padding_pixels, 0, ro::ui::kLargestChatPaddingPixels);
  mine.line_gap_pixels = std::clamp(own.line_gap_pixels, 0, ro::ui::kLargestChatLineGapPixels);
  mine.body = own.body;
  return mine;
}

// Ce que le menu d'un onglet doit MONTRER. ⚠ REMPLI DEPUIS LA GÉNÉRALE quand
// l'onglet n'a rien à lui : les barres doivent partir de ce que le joueur a
// sous les yeux, sinon cocher « une apparence à lui » ferait tout sauter d'un
// coup à des valeurs qu'il n'a pas choisies.
[[nodiscard]] ro::ui::ChatTabLook ShownLook(const ro::ui::ChatAppearance& general,
                                            const TabLook& own) {
  if (own.own) {
    return ro::ui::ChatTabLook{.own = true,
                               .font_percent = own.font_percent,
                               .padding_pixels = own.padding_pixels,
                               .line_gap_pixels = own.line_gap_pixels,
                               .body = own.body};
  }
  return ro::ui::ChatTabLook{.own = false,
                             .font_percent = general.font_percent,
                             .padding_pixels = general.padding_pixels,
                             .line_gap_pixels = general.line_gap_pixels,
                             .body = general.body};
}

}  // namespace

ChatTabsView::ChatTabsView(std::filesystem::path client_root)
    : client_root_(std::move(client_root)) {
  std::error_code missing;
  // ⚠ UN FICHIER ABSENT S'ÉCRIT TOUT DE SUITE, comme celui des réglages : un
  // fichier qu'on ne trouve pas n'existe pas, et le joueur à qui l'on dit
  // « corrige-le à la main » doit avoir quelque chose à ouvrir dès sa première
  // partie — ne serait-ce que pour y lire les noms de genre.
  const bool first_time = !std::filesystem::exists(ChatTabFilePath(client_root_), missing);
  LoadChatTabs(client_root_, tabs_);
  // ⚠ CE QUI VIENT D'ÊTRE LU EST DÉJÀ « ENREGISTRÉ ». Sans cela, la lecture
  // compterait comme un changement et le fichier serait réécrit à la première
  // image de chaque partie.
  saved_revision_ = tabs_.revision();
  if (first_time) {
    static_cast<void>(SaveChatTabs(client_root_, tabs_));
  }
}

void ChatTabsView::Refresh(ro::ui::ChatWindows& windows, std::span<const ro::sim::ChatLine> chat,
                           std::uint32_t now_ms, const ro::ui::ChatAppearance& look,
                           bool diagnostic) {
  if (day_second_at_boot_ < 0) {
    day_second_at_boot_ = DaySecondNow() - static_cast<int>(now_ms / kMsPerSecond);
  }
  // Le rang de la dernière ligne, mis de côté pour le geste : `Apply` tourne
  // après le dessin et ne voit plus la conversation.
  last_seq_ = chat.empty() ? last_seq_ : chat.back().seq;
  if (!opened_read_ && !chat.empty()) {
    opened_read_ = true;
    for (std::size_t rank = 0; rank < tabs_.count(); ++rank) {
      tabs_.MarkRead(rank, last_seq_);
    }
  }
  Harvest(chat);

  // ── LES FENÊTRES VIVANTES, ET ELLES SEULES ────────────────────────
  // ⚠ C'EST ICI QUE LES ÉTATS NAISSENT ET MEURENT. Une fenêtre dont plus aucun
  // onglet ne relève n'a plus rien à montrer ; garder son état serait une fuite
  // que rien à l'écran ne signalerait.
  // ⚠ LE COMPTEUR SEUL DANS UNE FLOTTANTE LA QUITTE : il redevient sa fenêtre.
  if (tabs_.lone_meter()) {
    UndockMeter();
  }
  living_ = tabs_.Groups();
  windows.Keep(living_);
  for (auto it = shown_.begin(); it != shown_.end();) {
    it = std::find(living_.begin(), living_.end(), it->first) == living_.end() ? shown_.erase(it)
                                                                               : std::next(it);
  }

  // ⚠ REPARTIE DE ZÉRO À CHAQUE IMAGE, fenêtre fermée comprise : un jeton ne
  // vaut que pour l'image qui l'a rendu, et le salon et les conversations
  // ajoutent les leurs après — la garder ferait grossir une liste que rien ne
  // viderait. Les refus des pseudos aussi : ils changent avec le groupe.
  shown_links_.clear();
  shown_pictures_.clear();
  refusals_.clear();

  // ⚠ LA MOISSON EST PASSÉE AVANT : ce qui se dit fenêtre fermée se dit quand
  // même, et le monde efface les plus anciennes lignes de chaque genre. Le TRI,
  // lui, ne sert que ce qui est ouvert.
  if (!windows.main.open) {
    return;
  }
  const std::span<const ChatTab> tabs = tabs_.tabs();
  for (const std::uint32_t group : living_) {
    const ro::ui::ChatWindowState& state = windows.For(group);
    GroupViews& views = shown_[group];

    // Les languettes de CETTE fenêtre, et le rang réel de chacune. ⚠ LES DEUX
    // ENSEMBLE : la fenêtre ne connaît que ses propres onglets, et ce qu'elle
    // demandera se lira dans SA numérotation — c'est cette table qui la traduit.
    views.tabs.clear();
    views.ranks.clear();
    for (std::size_t rank = 0; rank < tabs.size(); ++rank) {
      if (tabs[rank].group != group) {
        continue;
      }
      const bool meter = tabs[rank].content == ChatTabContent::kDamageMeter;
      views.tabs.push_back(ro::ui::ChatStripTab{.name = meter ? meter_label_ : tabs[rank].name,
                                                .cleared = tabs[rank].cleared_through != 0,
                                                .meter = meter,
                                                .unread = UnreadOf(rank, chat)});
      views.ranks.push_back(rank);
    }

    views.lines.clear();
    views.decorated.clear();
    // L'onglet montré, borné comme la fenêtre le borne : elle a pu ne pas
    // encore voir un onglet parti ailleurs. ⚠ CELUI DU COMPTEUR NE MONTRE
    // AUCUNE LIGNE : rien à trier.
    const auto shown_rank = [&views, &state] {
      return views.ranks[static_cast<std::size_t>(
          std::clamp(state.tab, 0, static_cast<int>(views.ranks.size()) - 1))];
    };
    if (!views.ranks.empty() && tabs[shown_rank()].content == ChatTabContent::kLog) {
      const ChatTab& tab = tabs[shown_rank()];
      views.lines.reserve(chat.size() + past_.lines.size() + 1);
      // ── CE QUI S'EST DIT LA PARTIE D'AVANT, EN TÊTE ──────────────────────
      // ⚠ CE SONT LES PLUS ANCIENNES : les mettre à la suite les ferait passer
      // pour ce qui vient d'arriver.
      //
      // Le vidage ne les touche pas : son rang désigne la session EN COURS, et
      // celles-ci n'en ont pas. Les filtres de genre, eux, s'appliquent — un
      // onglet « Guilde » n'a pas à montrer la parole publique d'hier.
      //
      // L'IDENTITÉ DE CHAQUE LIGNE pour le cache du fil : son aspect, sa source,
      // puis son rang — voir les constantes en tête de ce fichier. Le rang du
      // journal se compte AVANT le filtre, pour qu'une ligne garde la sienne
      // d'un onglet à l'autre.
      const std::uint64_t look_bits = LookBits(diagnostic);
      const std::uint64_t past_bits = (past_epoch_ & kPastEpochMask) << kPastEpochShift;
      std::uint64_t past_rank = 0;
      for (const PastChatLine& line : past_.lines) {
        ++past_rank;
        if (!diagnostic && !tab.Accepts(line.type)) {
          continue;
        }
        // ⚠ SANS ÉTIQUETTE DE GENRE : l'historique ne l'écrit pas.
        ro::ui::ChatLineView view{.type_tag = diagnostic ? ChatTypeTag(line.type) : kNoTag,
                                  .color = ColorOfShownRgb(line.color),
                                  .hour = line.hour,
                                  .minute = line.minute,
                                  .second = line.second};
        // ⚠ SANS RANG DE FIL (`seq`) : la partie d'avant n'est jamais « nouvelle ».
        ShowText(line.text, view, views, PastMvpSenderOf(line),
                 line.type == ro::sim::ChatType::kDiscord);
        view.id = CarriesMvpLink(view) ? 0 : look_bits | past_bits | past_rank;
        views.lines.push_back(view);
      }
      // ⚠ LE REPÈRE PARAÎT DANS TOUS LES ONGLETS, quel que soit leur filtre : il
      // ne décrit pas la conversation, il décrit le JOURNAL. Et il est
      // nécessaire parce que l'heure d'une ligne MENT sur son jour — « 21:12 » se
      // lit comme ce soir alors que c'était hier.
      if (!past_.lines.empty()) {
        views.lines.push_back(ro::ui::ChatLineView{.text = past_mark_, .color = kPastMarkColor});
      }
      for (const ro::sim::ChatLine& line : chat) {
        // ⚠ LA GARDE SUR ZÉRO N'EST PAS DÉCORATIVE : tant que rien n'a été vidé,
        // la règle ne doit pas s'appliquer DU TOUT. Sans elle, une ligne dont le
        // rang n'aurait pas encore été posé disparaîtrait de tous les onglets.
        //
        // ⚠ LE DIAGNOSTIC LÈVE AUSSI LE VIDAGE, et pas seulement les types : il
        // répond à « où est passée cette ligne », et un onglet vidé la cache tout
        // autant qu'un filtre.
        if (!diagnostic && tab.cleared_through != 0 && line.seq <= tab.cleared_through) {
          continue;
        }
        if (!diagnostic && !tab.Accepts(line.type)) {
          continue;
        }
        // CE QU'UNE CONVERSATION 1:1 A PRIS n'y paraît pas — sauf au
        // diagnostic, qui répond à « où est passée cette ligne ».
        if (!diagnostic && desk_ != nullptr && line.whisper.side != ro::sim::WhisperSide::kNone &&
            desk_->ShownApart(line.seq)) {
          continue;
        }
        // Le modulo ramène dans le jour, et l'ajout d'un jour écarte le négatif :
        // une partie commencée à 00 h 10 porte un repère négatif dès que l'horloge
        // de la boucle dépasse dix minutes.
        const int second = ((day_second_at_boot_ + static_cast<int>(line.said_ms / kMsPerSecond)) %
                                kSecondsPerDay +
                            kSecondsPerDay) %
                           kSecondsPerDay;
        ro::ui::ChatLineView view{
            .tag = diagnostic ? KindTag(line.kind) : kNoTag,
            .type_tag = diagnostic ? ChatTypeTag(line.type) : kNoTag,
            .color = ColorOfChatLine(line.kind, line.color),
            .hour = static_cast<std::uint8_t>(second / kSecondsPerHour),
            .minute = static_cast<std::uint8_t>((second / kSecondsPerMinute) % kSecondsPerMinute),
            .second = static_cast<std::uint8_t>(second % kSecondsPerMinute)};
        ShowLine(line, WhisperSurface::kLog, view, views);
        view.id = CarriesMvpLink(view) ? 0 : look_bits | kLiveLine | line.seq;
        // ⚠ LE RANG NU, MÊME QUAND L'IDENTITÉ EST NULLE : c'est lui qui compte ce
        // qui est arrivé pendant qu'on lisait plus haut.
        view.seq = line.seq;
        view.mention = std::ranges::binary_search(rich_.naming_seqs, line.seq);
        views.lines.push_back(view);
      }
    }

    // L'apparence de CETTE fenêtre : celle de son onglet actif quand il en a une.
    // ⚠ DÉCIDÉE PAR L'ONGLET ACTIF, et elle vaut pour tout le cadre : les marges et
    // le fond décrivent une fenêtre, pas un fil.
    views.look = look;
    views.tuned = ro::ui::ChatTabLook{};
    views.active.reset();
    if (!views.ranks.empty()) {
      const auto active = static_cast<std::size_t>(
          std::clamp(state.tab, 0, static_cast<int>(views.ranks.size()) - 1));
      views.active = views.ranks[active];
      // L'onglet MONTRÉ est lu : rien de ce qui est arrivé ne le marque.
      tabs_.MarkRead(views.ranks[active], last_seq_);
      views.tabs[active].unread = false;
      views.look = LookOf(look, tabs[views.ranks[active]].look);
      // 🔴 LE VERROU VIENT DES ONGLETS, PAS DES RÉGLAGES. Il décrit CETTE
      // fenêtre-ci, et chacun de ses onglets en porte la même copie ; celui de
      // l'actif vaut donc pour la fenêtre entière.
      views.look.locked = tabs[views.ranks[active]].locked;
    }

    // Le diagnostic vaut aussi pour le menu : il y numérote les cases.
    views.look.diagnostic = diagnostic;

    // Les filtres, seulement quand un onglet de CETTE fenêtre se règle : c'est
    // une liste de libellés traduits, et elle ne sert que popup ouvert.
    views.types.clear();
    if (state.configuring >= 0 &&
        static_cast<std::size_t>(state.configuring) < views.ranks.size() &&
        tabs[views.ranks[static_cast<std::size_t>(state.configuring)]].content ==
            ChatTabContent::kLog) {
      const ChatTab& shown = tabs[views.ranks[static_cast<std::size_t>(state.configuring)]];
      for (std::size_t rank = 0; rank < ro::sim::kChatTypeCount; ++rank) {
        const auto type = static_cast<ro::sim::ChatType>(rank);
        views.types.push_back(ro::ui::ChatTypeToggle{
            .label = ChatTypeLabel(type),
            .tag = ChatTypeTag(type),
            .on = shown.Accepts(type),
            .waiting = !IsFedChatType(type),
            .waiting_tip = ro::core::Tr("Aucun message n'alimente encore ce type : la case "
                                        "attend les lignes qui la rempliront.")});
      }
      // ⚠ CELLE DE L'ONGLET QU'ON RÈGLE, pas de celui qu'on montre : le clic droit
      // configure ce qu'il désigne.
      views.tuned = ShownLook(look, shown.look);
    }
  }
}

std::span<const ro::ui::ChatStripTab> ChatTabsView::tabs(std::uint32_t group) const {
  const auto found = shown_.find(group);
  return found == shown_.end() ? std::span<const ro::ui::ChatStripTab>() : found->second.tabs;
}

const ChatLinkTarget* ChatTabsView::LinkOf(std::uint32_t token) const {
  return token < shown_links_.size() ? &shown_links_[token] : nullptr;
}

std::span<const ro::ui::ChatLineView> ChatTabsView::lines(std::uint32_t group) const {
  const auto found = shown_.find(group);
  return found == shown_.end() ? std::span<const ro::ui::ChatLineView>() : found->second.lines;
}

ro::ui::ChatLogLayout* ChatTabsView::layout(std::uint32_t group) const {
  const auto found = shown_.find(group);
  return found == shown_.end() ? nullptr : &found->second.layout;
}

std::span<const ro::ui::ChatTypeToggle> ChatTabsView::types(std::uint32_t group) const {
  const auto found = shown_.find(group);
  return found == shown_.end() ? std::span<const ro::ui::ChatTypeToggle>() : found->second.types;
}

const ro::ui::ChatAppearance& ChatTabsView::look(std::uint32_t group) const {
  const auto found = shown_.find(group);
  // Une fenêtre que le tri ne connaît pas n'a rien à dessiner ; l'apparence
  // générale par défaut vaut mieux qu'une référence sur rien.
  static const ro::ui::ChatAppearance kFallback;
  return found == shown_.end() ? kFallback : found->second.look;
}

const ro::ui::ChatTabLook& ChatTabsView::tuned_look(std::uint32_t group) const {
  const auto found = shown_.find(group);
  static const ro::ui::ChatTabLook kFallback;
  return found == shown_.end() ? kFallback : found->second.tuned;
}

void ChatTabsView::Apply(std::uint32_t group, const ro::ui::ChatWindowAction& asked,
                         ro::ui::ChatWindowState& state) {
  const auto found = shown_.find(group);
  if (found == shown_.end()) {
    return;  // une fenêtre que le tri ne connaît plus : ses rangs ne valent rien
  }
  const std::vector<std::size_t>& ranks = found->second.ranks;
  // Le rang RÉEL d'une languette de cette fenêtre-ci. ⚠ TOUT PASSE PAR LÀ : la
  // fenêtre compte SES onglets à partir de zéro, et prendre son compte pour un
  // rang réel toucherait l'onglet d'à côté — ou celui d'une autre fenêtre.
  const auto real = [&ranks](int local) -> std::size_t {
    return local >= 0 && static_cast<std::size_t>(local) < ranks.size()
               ? ranks[static_cast<std::size_t>(local)]
               : std::numeric_limits<std::size_t>::max();
  };

  if (asked.add_tab) {
    // ⚠ ON SAUTE SUR L'ONGLET NEUF. Un onglet qu'on vient de créer et qu'il faut
    // aller chercher se lit comme un clic qui n'a rien fait.
    //
    // ⚠ ET IL NAÎT DANS LA FENÊTRE QUI L'A DEMANDÉ : le créer dans la
    // principale depuis une flottante le ferait apparaître ailleurs qu'on l'a
    // demandé — donc nulle part, aux yeux du joueur.
    const std::size_t born = tabs_.Add(NewTabName(tabs_.tabs()));
    tabs_.Attach(born, group);
    state.tab = static_cast<int>(ranks.size());
  }
  if (const std::size_t rank = real(asked.renamed_tab); rank != kNoTab) {
    tabs_.Rename(rank, asked.new_name);
  }
  if (asked.set_locked.has_value()) {
    // ⚠ SUR TOUS LES ONGLETS DU GROUPE : le verrou décrit la fenêtre, et
    // n'importe lequel d'entre eux peut devenir l'actif.
    tabs_.SetGroupLocked(group, *asked.set_locked);
  }
  if (asked.set_every_type.has_value() && state.configuring >= 0) {
    if (const std::size_t rank = real(state.configuring); rank != kNoTab) {
      // 🔴 TOUS, Y COMPRIS LES ANNONCES ET LES CASES EN ATTENTE. « Tout
      // décocher » qui laisserait passer les annonces serait un mensonge, et
      // le joueur chercherait longtemps la case qui reste.
      tabs_.SetAccepted(rank, *asked.set_every_type ? EveryChatType() : ChatTypeMask{});
    }
  }
  if (asked.toggled_type >= 0 && state.configuring >= 0) {
    const std::optional<ro::sim::ChatType> type = ChatTypeOf(asked.toggled_type);
    if (const std::size_t rank = real(state.configuring); type.has_value() && rank != kNoTab) {
      // ⚠ LE MASQUE SE RECOMPOSE DEPUIS CELUI QU'ON A : la fenêtre ne dit que
      // la case basculée, elle ne connaît pas de masque. Son rang est le
      // numéro du type, puisque la liste les suit dans l'ordre.
      const ChatTypeMask before = tabs_.tabs()[rank].accepted;
      const ChatTypeMask bit = MaskOf(*type);
      tabs_.SetAccepted(rank, (before & bit) != 0 ? before & ~bit : before | bit);
    }
  }
  if (asked.moved_from >= 0 && asked.moved_to >= 0) {
    // L'onglet MONTRÉ suit celui qu'on déplace : le perdre de vue parce qu'on
    // vient de le ranger ailleurs se lirait comme un geste qui a raté.
    //
    // ⚠ LE DÉPLACEMENT SE LIT DANS LA BANDE, DONC ENTRE VOISINS DE FENÊTRE. Le
    // rang d'arrivée est celui d'une languette de cette fenêtre-ci ; le traduire
    // en rang réel demande celui de la languette qui occupait la place.
    const std::size_t from = real(asked.moved_from);
    const std::size_t to =
        asked.moved_to >= static_cast<int>(ranks.size()) ? ranks.back() + 1 : real(asked.moved_to);
    if (from != kNoTab && to != kNoTab) {
      const bool was_shown = state.tab == asked.moved_from;
      tabs_.Move(from, to);
      if (was_shown) {
        state.tab = asked.moved_to > asked.moved_from ? asked.moved_to - 1 : asked.moved_to;
      }
    }
  }
  if (const std::size_t rank = real(asked.clear_tab); rank != kNoTab) {
    tabs_.ClearThrough(rank, last_seq_);
  }
  if (const std::size_t rank = real(asked.unclear_tab); rank != kNoTab) {
    tabs_.ClearThrough(rank, 0);
  }
  // ── OÙ CET ONGLET EST MONTRÉ ──────────────────────────────────────
  if (const std::size_t rank = real(asked.detach_tab); rank != kNoTab) {
    static_cast<void>(tabs_.Detach(rank));
    // ⚠ LA FENÊTRE QUI VIENT DE LE PERDRE REGARDE AILLEURS : son rang désignait
    // celui qui est parti, et la languette d'à côté a pris sa place.
    state.configuring = -1;
  }
  if (const std::size_t rank = real(asked.attach_tab); rank != kNoTab) {
    tabs_.Attach(rank, 0);
    state.configuring = -1;
  }
  if (asked.tuned_look) {
    if (const std::size_t rank = real(state.configuring); rank != kNoTab) {
      tabs_.SetLook(rank, TabLook{.own = asked.look.own,
                                  .font_percent = asked.look.font_percent,
                                  .padding_pixels = asked.look.padding_pixels,
                                  .line_gap_pixels = asked.look.line_gap_pixels,
                                  .body = asked.look.body});
    }
  }
  if (const std::size_t rank = real(asked.remove_tab); rank != kNoTab) {
    tabs_.Remove(rank);
    state.configuring = -1;
  }
  SaveIfNeeded();
}

void ChatTabsView::ApplyDrop(const ro::ui::ChatDrop& drop) {
  if (!drop.asked) {
    return;
  }
  const auto from = shown_.find(drop.from_group);
  if (from == shown_.end() || drop.from_tab < 0 ||
      static_cast<std::size_t>(drop.from_tab) >= from->second.ranks.size()) {
    return;
  }
  const std::size_t moved = from->second.ranks[static_cast<std::size_t>(drop.from_tab)];

  if (drop.to_new_window) {
    static_cast<void>(tabs_.Detach(moved));
    SaveIfNeeded();
    return;
  }

  const auto into = shown_.find(drop.to_group);
  if (into == shown_.end()) {
    return;  // une fenêtre que le tri ne connaît plus
  }
  // ⚠ LES RANGS DE LA BANDE VISÉE SONT CEUX D'AVANT LE DÉPÔT, et c'est
  // exactement ce qu'il faut : ils décrivent la bande telle que le joueur la
  // voyait en lâchant.
  const std::vector<std::size_t>& target = into->second.ranks;
  const std::size_t before =
      drop.to_rank >= 0 && static_cast<std::size_t>(drop.to_rank) < target.size()
          ? target[static_cast<std::size_t>(drop.to_rank)]
          : (target.empty() ? tabs_.count() : target.back() + 1);
  tabs_.Attach(moved, drop.to_group);
  tabs_.Move(moved, before);
  SaveIfNeeded();
}

void ChatTabsView::Harvest(std::span<const ro::sim::ChatLine> chat) {
  for (const ro::sim::ChatLine& line : chat) {
    if (line.seq <= harvested_through_) {
      continue;
    }
    harvested_through_ = line.seq;
    const int second =
        ((day_second_at_boot_ + static_cast<int>(line.said_ms / kMsPerSecond)) % kSecondsPerDay +
         kSecondsPerDay) %
        kSecondsPerDay;
    seen_.push_back(PastChatLine{
        .type = line.type,
        // ⚠ UN CHUCHOTEMENT AVEC SON EN-TÊTE : sans lui, il se relirait comme une
        // parole publique.
        .text = WhisperLogText(line, words_),
        .hour = static_cast<std::uint8_t>(second / kSecondsPerHour),
        .minute = static_cast<std::uint8_t>((second / kSecondsPerMinute) % kSecondsPerMinute),
        .second = static_cast<std::uint8_t>(second % kSecondsPerMinute),
        .color = ShownRgbOfChatLine(line.kind, line.color)});
    // ⚠ ÉCRITE TOUT DE SUITE, pas à la fermeture : c'est ce qui distingue le
    // journal de l'historique, et ce qui le fait survivre à une sortie brutale.
    log_.Write(seen_.back());
  }
  // ⚠ BORNÉE, SINON C'EST UNE FUITE : une soirée sur un serveur bavard, c'est
  // des dizaines de milliers de lignes, dont on n'enregistrera de toute façon
  // que la fin. On garde donc de quoi servir le plus grand réglage possible.
  if (seen_.size() > static_cast<std::size_t>(kLargestChatHistory)) {
    seen_.erase(seen_.begin(),
                seen_.begin() + static_cast<std::ptrdiff_t>(
                                    seen_.size() - static_cast<std::size_t>(kLargestChatHistory)));
  }
}

void ChatTabsView::RestorePast(bool keep_chat, bool keep_typed) {
  // Le journal va changer : ses rangs ne désignent plus les mêmes lignes.
  ++past_epoch_;
  if (!keep_chat && !keep_typed) {
    // Plus personne ne tient à ce fichier : il s'en va tout entier.
    ForgetChatHistory(client_root_);
    past_ = PastChat{};
    return;
  }
  past_ = LoadChatHistory(client_root_);
  // 🔴 CE QU'UNE CASE DÉCOCHÉE GARDAIT S'EN VA TOUT DE SUITE, pas à la
  // fermeture : entre les deux il y a une partie entière, et le fichier porte
  // des chuchotements en clair. On le réécrit donc amputé, sur-le-champ.
  const bool dropping =
      (!keep_chat && !past_.lines.empty()) || (!keep_typed && !past_.typed.empty());
  if (!keep_chat) {
    past_.lines.clear();
    past_.whispered.clear();
  }
  if (!keep_typed) {
    past_.typed.clear();
  }
  if (dropping) {
    static_cast<void>(SaveChatHistory(client_root_, past_.lines, kLargestChatHistory,
                                      past_.saved_at, past_.whispered, past_.typed));
  }
  if (past_.lines.empty()) {
    return;
  }
  past_mark_ = SessionMark(past_.saved_at);
  ro::core::LogInfo("conversation : {} ligne(s) de la partie d'avant", past_.lines.size());
}

void ChatTabsView::KeepLog(bool wanted) {
  // 🔴 LE TÉMOIN EST CELUI DE LA CASE, PAS CELUI DU JOURNAL. Cette case se
  // repousse à chaque image ; lire `log_.open()` pour savoir si l'on a déjà
  // agi confond « ouvert » avec « demandé ». Un journal qui refuse de s'ouvrir
  // se désarme — et se faisait rouvrir ici à l'image suivante, ce qui défaisait
  // le « DÉSARMÉ POUR DE BON » de `ChatLogWriter::Begin` et faisait repartir sa
  // plainte à chaque ligne dite, indéfiniment.
  //
  // Une garde qui borne la CONDITION ne borne pas le NOMBRE d'écritures, et
  // toute ligne du journal est vidée sur le disque à l'écriture.
  if (wanted == log_wanted_) {
    return;
  }
  log_wanted_ = wanted;
  if (!wanted) {
    // ⚠ FERMÉ, MAIS PAS EFFACÉ, contrairement à l'historique. Ce fichier-là est
    // au joueur : il l'a demandé, il l'ouvre avec ses outils, il le range où il
    // veut. Le supprimer parce qu'il décoche une case lui retirerait ce qu'il
    // était en train de garder.
    log_.Close();
    return;
  }
  const std::string stamp = StampNow();
  if (stamp.size() < kDateTextSize) {
    // Sans date lisible, le journal n'aurait pas de nom qui le range ni de
    // repère qui le date. On s'en passe plutôt que d'inventer un jour.
    ro::core::LogWarning(
        "journal de conversation : l'heure locale est illisible — rien ne sera gardé sur le "
        "disque");
    return;
  }
  log_.Open(client_root_, std::string_view(stamp).substr(0, kDateTextSize), SessionMark(stamp));
}

ChatLogDump ChatTabsView::SaveLogs() const {
  // Ce que la fenêtre montre, dans le même ordre : la partie d'avant coiffée de
  // son repère, puis celle-ci.
  const std::array<ChatLogPart, 2> parts = {
      ChatLogPart{.heading = past_mark_, .lines = past_.lines},
      ChatLogPart{.heading = {}, .lines = seen_}};
  return DumpChatLogs(client_root_, tabs_.tabs(), parts);
}

void ChatTabsView::SavePast(bool keep_chat, int keep, std::span<const std::string> whispered,
                            bool keep_typed, std::span<const std::string> typed) const {
  if (!keep_chat && !keep_typed) {
    return;
  }
  // ⚠ CE QU'ON A VU PASSER, ET NON CE QUE LE MONDE GARDE : son tampon est
  // borné, et réglable, donc s'en remettre à lui au moment de fermer ne
  // sauverait que la fin de la soirée. Les anciennes viennent d'abord, comme
  // elles se lisent.
  std::vector<PastChatLine> all;
  if (keep_chat) {
    all = past_.lines;
    all.insert(all.end(), seen_.begin(), seen_.end());
  }
  // ⚠ CHAQUE MOITIÉ N'ÉCRIT QUE SOUS SA PROPRE CASE : le fichier est commun,
  // les deux refus ne le sont pas.
  static_cast<void>(SaveChatHistory(client_root_, all, keep, StampNow(),
                                    keep_chat ? whispered : std::span<const std::string>(),
                                    keep_typed ? typed : std::span<const std::string>()));
}

void ChatTabsView::ForgetPast() {
  ForgetChatHistory(client_root_);
  ++past_epoch_;
  past_ = PastChat{};
  seen_.clear();
}

void ChatTabsView::SaveIfNeeded() {
  // On compare un compte plutôt que de lever un drapeau : la liste se modifie
  // depuis la fenêtre, depuis le fichier, et demain d'ailleurs.
  if (tabs_.revision() == saved_revision_) {
    return;
  }
  // Retenu même si l'écriture échoue : réessayer soixante fois par seconde sur
  // un dossier en lecture seule noierait le journal.
  saved_revision_ = tabs_.revision();
  static_cast<void>(SaveChatTabs(client_root_, tabs_));
}

}  // namespace moonclient
