#include "items/card_album_view.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::net::CardAlbumCommand;
using ro::sim::CardAlbumResult;
using ro::ui::CardAlbumTone;

// Combien de temps un compte rendu, et la pochette qu'il désigne, restent
// affichés.
constexpr std::uint32_t kNoticeMs = 6000;

// LES DEUX ALBUMS D'OBJETS qui rendent des cartes, dans l'ordre des macarons.
// Le nom de repli sert tant que la table des objets ne connaît pas l'objet : un
// macaron sans nom ne dirait rien.
struct AlbumSource {
  std::uint32_t item_id;
  const char* letter;
  const char* fallback;
};
constexpr std::array<AlbumSource, ro::ui::kCardAlbumSourceCount> kAlbumSources = {{
    {616, "O", "Old Card Album"},
    {12246, "M", "Mystical Card Album"},
}};

// L'EMPLACEMENT PRINCIPAL d'un masque : une carte multi-emplacements se nomme
// par le plus significatif — la tête avant le corps, le corps avant les mains.
// Termes du jeu, en anglais.
[[nodiscard]] const char* PrimarySlotLabel(std::uint32_t equip) {
  using namespace ro::sim;
  if ((equip & kEquipHeadTop) != 0) {
    return "Head top";
  }
  if ((equip & kEquipHeadMid) != 0) {
    return "Head mid";
  }
  if ((equip & kEquipHeadLow) != 0) {
    return "Head bot";
  }
  if ((equip & kEquipArmor) != 0) {
    return "Armor";
  }
  if ((equip & kEquipGarment) != 0) {
    return "Garment";
  }
  if ((equip & kEquipShoes) != 0) {
    return "Shoes";
  }
  if ((equip & (kEquipAccessoryLeft | kEquipAccessoryRight)) != 0) {
    return "Accessory";
  }
  if ((equip & kEquipShield) != 0) {
    return "Shield";
  }
  if ((equip & kEquipWeapon) != 0) {
    return "Weapon";
  }
  if ((equip & kEquipAmmo) != 0) {
    return "Ammunition";
  }
  if ((equip & kEquipCostumeMask) != 0) {
    return "Costume";
  }
  if ((equip & kEquipShadowMask) != 0) {
    return "Shadow";
  }
  return "";
}

// Les intercalaires où une carte se range, d'après son masque.
[[nodiscard]] std::uint8_t FamiliesOf(std::uint32_t equip) {
  using namespace ro::sim;
  std::uint8_t families = 0;
  const auto add = [&](std::uint32_t bits, std::uint8_t family) {
    if ((equip & bits) != 0) {
      families |= family;
    }
  };
  add(kEquipArmor, ro::ui::kCardFamilyArmor);
  add(kEquipWeapon, ro::ui::kCardFamilyWeapon);
  add(kEquipShield, ro::ui::kCardFamilyShield);
  add(kEquipGarment, ro::ui::kCardFamilyGarment);
  add(kEquipShoes, ro::ui::kCardFamilyShoes);
  add(kEquipAccessoryLeft | kEquipAccessoryRight, ro::ui::kCardFamilyAccessory);
  add(kEquipHeadTop | kEquipHeadMid | kEquipHeadLow, ro::ui::kCardFamilyHead);
  return families;
}

[[nodiscard]] const char* ResultText(CardAlbumResult result) {
  switch (result) {
    case CardAlbumResult::kOk:
      return Tr("C'est fait.");
    case CardAlbumResult::kNotACard:
      return Tr("Seules les cartes de monstre entrent dans l'album.");
    case CardAlbumResult::kLocked:
      return Tr("Cet emplacement n'est pas encore débloqué : sacrifiez d'abord une carte.");
    case CardAlbumResult::kAlreadyUnlocked:
      return Tr("Cet emplacement est déjà débloqué.");
    case CardAlbumResult::kNotEnough:
      return Tr("Il n'y en a pas autant en réserve.");
    case CardAlbumResult::kInventoryFull:
      return Tr("Votre inventory est plein.");
    case CardAlbumResult::kBusy:
      return Tr("Impossible pendant un échange ou avec un entrepôt ouvert.");
    case CardAlbumResult::kBound:
      return Tr("Une carte liée ou en location ne peut pas être rangée.");
    case CardAlbumResult::kStackFull:
      return Tr("La réserve de cette carte est pleine.");
    case CardAlbumResult::kNoAccount:
      return Tr(
          "Ce compte de jeu n'est rattaché à aucun compte Moonlight : l'album ne peut pas "
          "être enregistré.");
    case CardAlbumResult::kInUse:
      return Tr(
          "L'album est déjà ouvert sur un autre de vos comptes de jeu. Fermez-le là-bas "
          "d'abord.");
    case CardAlbumResult::kNotOpen:
      return Tr("L'album n'est pas ouvert : rouvrez la fenêtre.");
    case CardAlbumResult::kFail:
      break;
  }
  // Un refus générique — ou un code d'un serveur plus neuf que nous.
  return Tr("Le serveur a refusé l'opération.");
}

[[nodiscard]] ro::ui::SkinImage ImageOf(const ro::render::IconTexture* texture) {
  if (texture == nullptr || !texture->usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(texture->texture.name()),
                           .width = static_cast<float>(texture->width_px),
                           .height = static_cast<float>(texture->height_px)};
}

}  // namespace

CardAlbumView::CardAlbumView(const std::filesystem::path& client_root) : delta_(client_root) {}

void CardAlbumView::Send(CardAlbumCommand command, std::uint32_t arg, std::uint16_t amount,
                         std::uint32_t card_id, ro::net::Connection& link) {
  // Retenu AVANT l'envoi : la réponse ne redit pas ce qu'on a demandé.
  last_command_ = command;
  last_card_ = card_id;
  last_amount_ = amount;
  in_flight_ = true;
  if (command == CardAlbumCommand::kRefresh) {
    loading_ = true;
  }
  if (auto sent = ro::net::RequestCardAlbum(link, command, arg, amount); !sent) {
    ro::core::LogWarning("album : commande non émise : {}", sent.error().detail);
    in_flight_ = false;
    loading_ = false;
  }
}

void CardAlbumView::Report(std::string text, CardAlbumTone tone, std::uint32_t now_ms) {
  notice_ = std::move(text);
  notice_tone_ = tone;
  notice_ms_ = now_ms;
  notice_live_ = true;
}

std::string CardAlbumView::ShortName(std::uint32_t card_id) const {
  for (const ro::ui::CardAlbumPocketView& pocket : pockets_) {
    if (pocket.card_id == card_id && !pocket.name.empty()) {
      return ro::ui::CardAlbumShortName(pocket.name);
    }
  }
  return Tr("Cette carte");
}

ro::net::FrameOutcome CardAlbumView::ApplyFrame(const ro::net::Frame& frame,
                                                ro::net::Connection& link, std::uint32_t now_ms) {
  const int unlocked_before = album_.unlocked_count();
  const std::int64_t reserve_before = album_.total_reserve();
  const ro::net::FrameOutcome outcome = ro::net::ApplyCardAlbumFrame(frame, album_);
  if (outcome != ro::net::FrameOutcome::kApplied) {
    return outcome;
  }
  loading_ = false;
  // Un état que nous n'avons PAS demandé — `@storealbum` fait répondre le
  // serveur de la même façon. Le formuler avec la dernière commande dirait la
  // dernière carte rangée à la main : on le dit d'après ce qui a CHANGÉ.
  const bool unsolicited = !in_flight_;
  in_flight_ = false;
  pocket_ids_.clear();
  for (const ro::sim::CardAlbumCard& card : album_.cards()) {
    pocket_ids_.push_back(card.card_id);
  }
  std::ranges::sort(pocket_ids_);
  if (album_.blocked()) {
    chain_card_ = 0;
    chained_deposit_ = false;
    pending_show_ = 0;
    Report(ResultText(CardAlbumResult::kInUse), CardAlbumTone::kRefusal, now_ms);
    return outcome;
  }
  if (unsolicited) {
    last_command_ = CardAlbumCommand::kRefresh;
    unsolicited_unlocked_ = album_.unlocked_count() - unlocked_before;
    unsolicited_reserve_ = album_.total_reserve() - reserve_before;
    ReportAnswer(now_ms);
    ServePendingShow(now_ms);
    return outcome;
  }
  unsolicited_unlocked_ = 0;
  unsolicited_reserve_ = 0;
  const bool done = album_.last_result() == CardAlbumResult::kOk;
  // Le sacrifice a ouvert la pochette : le reste de la pile y entre sans que le
  // joueur ait à le redemander. Le compte rendu dira les deux.
  if (last_command_ == CardAlbumCommand::kUnlock && chain_card_ != 0) {
    const bool opened = done && last_card_ == chain_card_;
    const std::uint16_t slot = chain_slot_;
    const std::uint32_t card = chain_card_;
    const int rest = chain_amount_;
    chain_card_ = 0;
    chain_slot_ = 0;
    chain_amount_ = 0;
    if (opened && rest > 0) {
      chained_deposit_ = true;
      Send(CardAlbumCommand::kPut, slot, static_cast<std::uint16_t>(rest), card, link);
      return outcome;  // le compte rendu viendra avec le dépôt
    }
  }
  // ⚠ CONSOMMÉ ICI, y compris quand le dépôt est REFUSÉ : le sacrifice qui le
  // précédait, lui, a eu lieu, et le joueur doit le savoir.
  last_chained_ = last_command_ == CardAlbumCommand::kPut && chained_deposit_;
  chained_deposit_ = false;
  ReportAnswer(now_ms);
  // Un ordre qui a ABOUTI sur une carte précise : on va la montrer. Un simple
  // rafraîchissement ne déplace rien — sinon les pages tourneraient sous les
  // doigts du joueur à chaque ouverture.
  if ((done || last_chained_) && last_command_ != CardAlbumCommand::kRefresh && last_card_ != 0) {
    highlight_card_ = last_card_;
    highlight_ms_ = now_ms;
    ++highlight_serial_;
    highlight_reveal_ = false;
  }
  // 🔴 APRÈS L'ORDRE : la demande la plus récente du joueur l'emporte sur le
  // surlignage de l'ordre, qui garde son compte rendu. Un dépôt enchaîné, lui,
  // la fait attendre sa propre réponse.
  ServePendingShow(now_ms);
  return outcome;
}

void CardAlbumView::ServePendingShow(std::uint32_t now_ms) {
  if (pending_show_ != 0) {
    ShowNow(std::exchange(pending_show_, 0), now_ms);
  }
}

void CardAlbumView::Show(std::uint32_t card_id, ro::ui::CardAlbumWindowState& state,
                         ro::net::Connection& link, std::uint32_t now_ms) {
  if (card_id == 0) {
    return;
  }
  if (state.open) {
    state.raise = true;
  } else {
    Toggle(state, link, now_ms);
  }
  // Une réponse est attendue — l'état que l'ouverture vient de demander, ou un
  // ordre en vol : c'est elle qui portera le catalogue à jour.
  if (loading_ || in_flight_ || album_.arrival() == 0) {
    pending_show_ = card_id;
    return;
  }
  ShowNow(card_id, now_ms);
}

void CardAlbumView::ShowNow(std::uint32_t card_id, std::uint32_t now_ms) {
  if (album_.Find(card_id) == nullptr) {
    const char* warning = Tr("Cette carte n'a pas de pochette dans l'album");
    // UN ORDRE VIENT DE RÉPONDRE DANS LA MÊME IMAGE : son compte rendu reste, et
    // l'avertissement se lit sur la ligne suivante, du ton le plus grave.
    if (notice_live_ && notice_ms_ == now_ms && !notice_.empty()) {
      notice_ += '\n';
      notice_ += warning;
      notice_tone_ = std::max(notice_tone_, CardAlbumTone::kWarning);
      return;
    }
    Report(warning, CardAlbumTone::kWarning, now_ms);
    return;
  }
  // UN NOUVEAU NUMÉRO DE SÉRIE : la fenêtre tourne ses pages une fois.
  highlight_card_ = card_id;
  highlight_ms_ = now_ms;
  ++highlight_serial_;
  highlight_reveal_ = true;
}

void CardAlbumView::NoteCardItems(std::vector<std::uint32_t> card_ids) {
  card_ids_ = std::move(card_ids);
  std::ranges::sort(card_ids_);
  const auto repeated = std::ranges::unique(card_ids_);
  card_ids_.erase(repeated.begin(), repeated.end());
}

ro::ui::CardAlbumLinkFacts CardAlbumView::LinkFacts(bool available) const noexcept {
  return ro::ui::CardAlbumLinkFacts{.available = available,
                                    .card_ids = card_ids_,
                                    .pocket_ids = pocket_ids_,
                                    .pockets_known = album_.arrival() != 0 && !album_.blocked()};
}

void CardAlbumView::ReportAnswer(std::uint32_t now_ms) {
  const CardAlbumResult result = album_.last_result();
  const std::string name = ShortName(last_card_);
  if (result != CardAlbumResult::kOk) {
    if (last_chained_ && last_command_ == CardAlbumCommand::kPut) {
      // Le sacrifice a eu lieu, le dépôt qui suivait non : les deux se disent,
      // sinon le joueur croit avoir perdu sa carte pour rien.
      Report(ro::core::Printed(
                 Tr("%s : pochette DÉBLOQUÉE, mais les copies restantes n'ont pas été rangées "
                    ": %s"),
                 name.c_str(), ResultText(result)),
             CardAlbumTone::kWarning, now_ms);
    } else {
      Report(ResultText(result), CardAlbumTone::kRefusal, now_ms);
    }
    return;
  }
  switch (last_command_) {
    case CardAlbumCommand::kRefresh: {
      // Un rafraîchissement demandé n'est pas un événement ; un état reçu SANS
      // demande l'est, s'il a changé quelque chose.
      if (unsolicited_reserve_ <= 0 && unsolicited_unlocked_ <= 0) {
        return;
      }
      Report(ro::core::Printed(
                 Tr("Album mis à jour : %lld copie(s) rangée(s), %d pochette(s) ouverte(s)."),
                 static_cast<long long>(unsolicited_reserve_), unsolicited_unlocked_),
             CardAlbumTone::kDone, now_ms);
      return;
    }
    case CardAlbumCommand::kUnlock: {
      Report(ro::core::Printed(
                 Tr("%s : pochette DÉBLOQUÉE. Vous pouvez désormais y ranger vos copies."),
                 name.c_str()),
             CardAlbumTone::kDone, now_ms);
      return;
    }
    case CardAlbumCommand::kPut:
    case CardAlbumCommand::kGet: {
      // ⚠ LE FORMAT RESTE UN LITTÉRAL traduit, à même l'appel : c'est ce qui
      // laisse le compilateur vérifier ses arguments.
      std::string text;
      const auto amount = static_cast<long long>(last_amount_);
      if (last_command_ == CardAlbumCommand::kGet) {
        text = ro::core::Printed(Tr("%s : %lld reprise(s) de l'album."), name.c_str(), amount);
      } else if (last_chained_) {
        text = ro::core::Printed(Tr("%s : pochette DÉBLOQUÉE, et %lld copie(s) rangée(s)."),
                                 name.c_str(), amount);
      } else {
        text = ro::core::Printed(Tr("%s : %lld rangée(s) dans l'album."), name.c_str(), amount);
      }
      Report(text, CardAlbumTone::kDone, now_ms);
      return;
    }
    case CardAlbumCommand::kClose:
      return;
  }
}

void CardAlbumView::Toggle(ro::ui::CardAlbumWindowState& state, ro::net::Connection& link,
                           std::uint32_t now_ms) {
  (void)now_ms;
  if (state.open) {
    // ⚠ LA FENÊTRE RENDRA LE VERROU ELLE-MÊME à l'image suivante : c'est elle
    // qui voit qu'elle vient de se fermer, quelle que soit la main qui l'a
    // fermée.
    state.open = false;
    return;
  }
  state.open = true;
  state.raise = true;
  // On ne montre rien tant que le serveur n'a pas répondu : l'album est son
  // état, pas le nôtre.
  Send(CardAlbumCommand::kRefresh, 0, 0, 0, link);
}

void CardAlbumView::Apply(const ro::ui::CardAlbumWindowAction& asked, ro::net::Connection& link,
                          std::uint32_t now_ms) {
  using ro::ui::CardAlbumVerb;
  if (notice_live_ && now_ms - notice_ms_ >= kNoticeMs) {
    notice_live_ = false;
  }
  if (staff_notice_live_ && now_ms - staff_notice_ms_ >= kNoticeMs) {
    staff_notice_live_ = false;
  }
  if (highlight_card_ != 0 && now_ms - highlight_ms_ >= kNoticeMs) {
    highlight_card_ = 0;
  }
  switch (asked.verb) {
    case CardAlbumVerb::kNothing:
      break;
    case CardAlbumVerb::kRefresh:
      Send(CardAlbumCommand::kRefresh, 0, 0, 0, link);
      break;
    case CardAlbumVerb::kClose:
      // Rendre le verrou : sans réponse, et rien à retenir.
      if (auto sent = ro::net::RequestCardAlbum(link, CardAlbumCommand::kClose, 0, 0); !sent) {
        ro::core::LogWarning("album : fermeture non émise : {}", sent.error().detail);
      }
      chain_card_ = 0;
      chained_deposit_ = false;
      break;
    case CardAlbumVerb::kSacrifice:
      // 🔴 TOUJOURS UNE COPIE : le sacrifice n'a pas de quantité, et en accepter
      // une inviterait à en brûler plusieurs par accident. Le RESTE de la pile
      // suivra si le serveur confirme l'ouverture.
      chain_slot_ = asked.slot;
      chain_card_ = asked.card_id;
      chain_amount_ = std::max(0, asked.rest);
      Send(CardAlbumCommand::kUnlock, asked.slot, 1, asked.card_id, link);
      break;
    case CardAlbumVerb::kDeposit:
      Send(CardAlbumCommand::kPut, asked.slot, static_cast<std::uint16_t>(asked.amount),
           asked.card_id, link);
      break;
    case CardAlbumVerb::kWithdraw:
      Send(CardAlbumCommand::kGet, asked.card_id, static_cast<std::uint16_t>(asked.amount),
           asked.card_id, link);
      break;
  }
  if (asked.refused_not_a_card) {
    // Le même refus que le serveur donnerait, sans faire le voyage.
    last_command_ = CardAlbumCommand::kPut;
    Report(ResultText(CardAlbumResult::kNotACard), CardAlbumTone::kRefusal, now_ms);
  }
  // ── Le bon de travail du staff ────────────────────────────────────────────
  // 🔴 RIEN NE PART AU SERVEUR : c'est un fichier qu'un script consommera.
  const ro::ui::CardAlbumStaffAsk& staff = asked.staff;
  if (staff.album_id == 0 || staff.card_id == 0) {
    return;
  }
  std::string album_name = Tr("l'album");
  for (std::size_t rank = 0; rank < sources_.size(); ++rank) {
    if (sources_[rank].item_id == staff.album_id) {
      album_name = source_names_[rank];
    }
  }
  const std::string card = ShortName(staff.card_id);
  bool written = false;
  std::string text;
  if (staff.cancel) {
    written = delta_.Clear(staff.album_id, staff.card_id);
    text = ro::core::Printed(
        "%s", written ? Tr("Demande annulée.")
                      : Tr("Annulation NON écrite : voir le dossier savedata et le journal."));
  } else {
    const bool remove = staff.intent == ro::ui::CardAlbumIntent::kRemove;
    written = delta_.Set(AlbumDeltaEntry{
        .album_id = staff.album_id,
        .card_id = staff.card_id,
        .action = remove ? AlbumDeltaAction::kRemove : AlbumDeltaAction::kAdd,
        .rate = std::clamp(staff.rate, ro::ui::kCardAlbumRateLeast, ro::ui::kCardAlbumRateMost)});
    if (written) {
      text = ro::core::Printed(remove ? Tr("Demandé : RETIRER %s de %s. Reste à lancer le script.")
                                      : Tr("Demandé : AJOUTER %s à %s. Reste à lancer le script."),
                               card.c_str(), album_name.c_str());
    } else {
      // 🔴 LE DIRE : l'état d'avant est rétabli, le macaron d'attente ne
      // paraîtra donc pas — sans message, le geste aurait l'air ignoré.
      text = ro::core::Printed(
          "%s", Tr("Bon de travail NON écrit : voir le dossier savedata et le journal."));
    }
  }
  staff_notice_ = text;
  staff_notice_ok_ = written;
  staff_notice_ms_ = now_ms;
  staff_notice_live_ = true;
}

void CardAlbumView::BuildSources(const CardAlbumPieces& pieces) {
  if (sources_built_ || pieces.packages == nullptr || pieces.packages->empty()) {
    return;
  }
  sources_built_ = true;
  for (std::size_t rank = 0; rank < kAlbumSources.size(); ++rank) {
    const ro::vfs::PackageTable* table = pieces.packages->Find(kAlbumSources[rank].item_id);
    if (table == nullptr) {
      continue;
    }
    auto& chances = chances_[rank];
    for (const ro::vfs::PackageGroup& group : table->groups) {
      for (const ro::vfs::PackageEntry& entry : group.entries) {
        // Un album rend des OBJETS ; une branche invoquerait des monstres.
        if (entry.is_monster || entry.id == 0) {
          continue;
        }
        ro::ui::CardAlbumSourceMark& mark = chances[entry.id];
        const bool first = !mark.present;
        mark.present = true;
        mark.percent += ro::vfs::PackagePercent(group.guaranteed, entry.weight, group.total);
        mark.weight += entry.weight;
        // Le dénominateur ne se cite que s'il est UNIQUE.
        mark.total = first ? group.total : (mark.total == group.total ? mark.total : 0);
      }
    }
    // Le poids DOMINANT de l'album, par simple comptage : c'est ce que la
    // demande d'ajout du staff propose. Mesuré sur la table, qui fait foi.
    std::unordered_map<int, int> tally;
    int best = 0;
    int best_count = 0;
    for (const auto& [card, mark] : chances) {
      (void)card;
      const int count = ++tally[mark.weight];
      if (count > best_count || (count == best_count && mark.weight < best)) {
        best_count = count;
        best = mark.weight;
      }
    }
    typical_rates_[rank] = best;
  }
}

void CardAlbumView::RebuildPockets(const CardAlbumPieces& pieces) {
  pockets_.clear();
  pockets_.reserve(album_.cards().size());
  for (const ro::sim::CardAlbumCard& card : album_.cards()) {
    ro::ui::CardAlbumPocketView pocket;
    pocket.card_id = card.card_id;
    pocket.reserve = card.reserve;
    pocket.unlocked = card.unlocked;
    pocket.families = FamiliesOf(card.equip_mask);
    pocket.dropper_rank = card.dropper_rank;
    pocket.slot_label = PrimarySlotLabel(card.equip_mask);
    if (pieces.items != nullptr) {
      if (const ro::vfs::ItemEntry* entry = pieces.items->Find(card.card_id); entry != nullptr) {
        pocket.name = entry->name;
      }
    }
    for (std::size_t rank = 0; rank < kAlbumSources.size(); ++rank) {
      const auto found = chances_[rank].find(card.card_id);
      if (found != chances_[rank].end()) {
        pocket.sources[rank] = found->second;
      }
      if (const AlbumDeltaEntry* intent = delta_.Find(kAlbumSources[rank].item_id, card.card_id);
          intent != nullptr) {
        pocket.sources[rank].intent = intent->action == AlbumDeltaAction::kRemove
                                          ? ro::ui::CardAlbumIntent::kRemove
                                          : ro::ui::CardAlbumIntent::kAdd;
        pocket.sources[rank].intent_rate = intent->rate;
      }
    }
    pockets_.push_back(std::move(pocket));
  }
  ++revision_;
}

void CardAlbumView::Compose(const CardAlbumPieces& pieces, const ro::sim::World& world,
                            const ro::ui::CardAlbumWindowState& state, bool staff) {
  const bool fresh_sources = !sources_built_;
  BuildSources(pieces);
  const bool sources_now = sources_built_ && fresh_sources;

  // Les deux albums d'objets, nommés comme le jeu les nomme.
  if (sources_.empty()) {
    for (const AlbumSource& source : kAlbumSources) {
      const ro::vfs::ItemEntry* entry =
          pieces.items != nullptr ? pieces.items->Find(source.item_id) : nullptr;
      source_names_.emplace_back(entry != nullptr && !entry->name.empty() ? entry->name
                                                                          : source.fallback);
    }
    for (std::size_t rank = 0; rank < kAlbumSources.size(); ++rank) {
      sources_.push_back(ro::ui::CardAlbumSourceView{.item_id = kAlbumSources[rank].item_id,
                                                     .letter = kAlbumSources[rank].letter,
                                                     .name = source_names_[rank]});
    }
  }
  for (std::size_t rank = 0; rank < sources_.size(); ++rank) {
    sources_[rank].typical_rate = typical_rates_[rank];
  }

  if (!pockets_built_ || pockets_arrival_ != album_.arrival() ||
      pockets_delta_ != delta_.revision() || sources_now) {
    pockets_built_ = true;
    pockets_arrival_ = album_.arrival();
    pockets_delta_ = delta_.revision();
    RebuildPockets(pieces);
  }

  // Les cartes du sac : ce qu'on peut offrir à l'album. Le serveur revalide
  // tout ; ceci ne sert qu'à PROPOSER.
  hand_.clear();
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (ro::sim::ItemCategoryOf(item) != ro::sim::ItemCategory::kCard || item.amount <= 0) {
      continue;
    }
    hand_.push_back(ro::ui::CardAlbumHandCard{
        .client_slot = item.client_slot, .card_id = item.item_id, .amount = item.amount});
  }

  // Les vignettes que la fenêtre a dessinées à l'image précédente.
  art_.clear();
  if (pieces.thumbnails != nullptr) {
    pieces.thumbnails->BeginFrame();
  }
  for (const ro::ui::CardAlbumArtAsk& wanted : state.wanted) {
    ro::ui::CardAlbumArt art{.card_id = wanted.card_id, .silhouette = wanted.silhouette};
    if (pieces.thumbnails != nullptr) {
      const ro::render::CardThumbnails::Thumb thumb = pieces.thumbnails->Get(
          wanted.card_id, wanted.silhouette, state.wanted_width, state.wanted_height);
      art.picture = ImageOf(thumb.texture);
      art.pending = thumb.pending;
    }
    if (pieces.icons != nullptr) {
      art.icon = ImageOf(&pieces.icons->Get(wanted.card_id));
    }
    art_.push_back(art);
  }

  view_ = ro::ui::CardAlbumView{};
  view_.pockets = pockets_;
  view_.revision = revision_;
  view_.unlocked_count = album_.unlocked_count();
  view_.total_reserve = album_.total_reserve();
  view_.dropper_known = album_.dropper_known();
  view_.blocked = album_.blocked();
  view_.loading = loading_;
  view_.hand = hand_;
  view_.sources = sources_;
  view_.art = art_;
  view_.art_width = state.wanted_width;
  view_.art_height = state.wanted_height;
  if (notice_live_) {
    view_.notice = notice_;
    view_.notice_tone = notice_tone_;
  }
  if (staff_notice_live_ && staff) {
    view_.staff_notice = staff_notice_;
    view_.staff_notice_ok = staff_notice_ok_;
  }
  view_.highlight_card = highlight_card_;
  view_.highlight_serial = highlight_serial_;
  view_.highlight_reveal = highlight_reveal_;
  view_.staff = staff;
}

}  // namespace moonclient
