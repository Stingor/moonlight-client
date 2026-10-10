#include "items/view_equip_view.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>

#include "ro/core/i18n.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_name.h"

#include "items/inventory_view.h"
#include "items/view_equip_rows.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les jetons de MA colonne partent d'ici, suivis de la case du sac : ceux de la
// sienne sont les rangs de ses pièces, qu'aucune fiche ne compte en telle
// quantité.
constexpr std::uint32_t kMineTokenBase = 0x10000;

// L'identité de la fenêtre, qui ne change pas quand le titre change.
constexpr const char* kWindowIdentity = "###view_equip";

// Le nombre d'orientations d'un .ACT.
constexpr int kDirections = 8;

// Au repos, les trois images d'un joueur sont les trois orientations du REGARD ;
// la première le porte droit devant.
constexpr std::size_t kLookingAheadFrame = 0;

constexpr std::uint32_t kMillisecondsPerSecond = 1000;

// Assez pour « Équipement de » et un nom de personnage.
constexpr std::size_t kTitleBytes = 160;

}  // namespace

ViewedEquipmentView::ViewedEquipmentView(const ro::vfs::ItemTable* items,
                                         const ro::vfs::ItemWords* words,
                                         ro::render::ItemIcons* icons,
                                         ro::render::ActorWardrobe* wardrobe)
    : items_(items), words_(words), icons_(icons), wardrobe_(wardrobe) {
  if (wardrobe_ != nullptr) {
    doll_.emplace(*wardrobe_);
  }
}

void ViewedEquipmentView::Refresh(const ro::sim::ViewedEquipmentBook& book,
                                  const ro::sim::World& world, ActorStyles& styles, bool staff,
                                  ro::ui::ViewEquipWindowState& state, std::uint32_t now_ms) {
  // ⚠ UN REFUS NE DÉFAIT PAS LA DEMANDE : la prochaine en remplacera l'AID.
  if (book.received() != received_seen_) {
    received_seen_ = book.received();
    // 🔴 L'AID DEMANDÉ DEVIENT CELUI DU JOUEUR AFFICHÉ, et seulement maintenant.
    // Une réponse que nous n'avons pas demandée n'en a pas : zéro.
    shown_id_ = pending_id_;
    pending_id_ = 0;
    received_ms_ = now_ms;
    // ELLE RESTE OUVERTE, et passe devant, quand on inspecte quelqu'un d'autre.
    state.open = true;
    state.raise = true;
  }
  if (!state.open || !book.shown().has_value()) {
    return;
  }
  const ro::sim::ViewedEquipment& sheet = *book.shown();

  std::array<char, kTitleBytes> named{};
  if (sheet.name.empty()) {
    std::snprintf(named.data(), named.size(), "%s", Tr("Équipement"));
  } else {
    std::snprintf(named.data(), named.size(), Tr("Équipement de %s"), sheet.name.c_str());
  }
  title_ = std::string(named.data()) + kWindowIdentity;

  view_.title = title_;
  view_.sex = sheet.look.sex == 1 ? Tr("Homme") : Tr("Femme");
  view_.age_seconds = static_cast<int>((now_ms - received_ms_) / kMillisecondsPerSecond);
  view_.staff = staff && !sheet.name.empty();
  view_.can_refresh = shown_id_ != 0;
  view_.piece_count = static_cast<int>(sheet.pieces.size());
  ComposeRows(sheet, world.inventory());
  ComposeDoll(sheet, world, styles, state, now_ms);
}

ro::ui::ViewEquipPieceView ViewedEquipmentView::PieceView(const ro::sim::InventoryItem& piece,
                                                          std::uint32_t token) {
  ro::ui::ViewEquipPieceView shown;
  shown.item_id = piece.item_id;
  shown.token = token;
  shown.damaged = piece.damaged;
  // Un objet NON identifié ne révèle pas son nom composé. Le serveur n'envoie
  // que de l'équipement porté, donc identifié ; le drapeau existe, et le dire
  // coûte une ligne.
  if (!piece.identified) {
    shown.label = Tr("(non identifié)");
  } else if (items_ != nullptr && words_ != nullptr) {
    ro::vfs::ItemNameFacts facts = ItemNameFactsOf(piece);
    creators_.Fill(facts);
    shown.label = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
  }
  if (icons_ != nullptr) {
    const ro::render::IconTexture& icon = icons_->Get(piece.item_id);
    if (icon.usable()) {
      shown.icon = ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                                     .width = static_cast<float>(icon.width_px),
                                     .height = static_cast<float>(icon.height_px)};
    }
  }
  return shown;
}

void ViewedEquipmentView::ComposeRows(const ro::sim::ViewedEquipment& sheet,
                                      const ro::sim::Inventory& mine) {
  const std::span<const ro::sim::InventoryItem> theirs = sheet.pieces;
  view_.sections.clear();

  const auto section = [&](const char* title, std::span<const EquipPlace> order,
                           EquipSection kind) {
    ro::ui::ViewEquipSectionView built;
    built.title = title;
    const std::optional<std::size_t> weapon = RankAt(theirs, EquipPlace::kWeapon, kind);
    for (const EquipPlace place : order) {
      ro::ui::ViewEquipRowView row;
      row.place = EquipPlaceLabel(place);
      if (const std::optional<std::size_t> rank = RankAt(theirs, place, kind); rank.has_value()) {
        row.theirs = PieceView(theirs[*rank], static_cast<std::uint32_t>(*rank));
      } else if (place == EquipPlace::kShield && kind == EquipSection::kEquipment &&
                 weapon.has_value() && PlaceOf(theirs[*weapon].worn_at).two_handed) {
        row.held_by_both_hands = true;
      }
      if (const ro::sim::InventoryItem* worn = mine.WornAt(LocationOf(place, kind));
          worn != nullptr) {
        row.mine = PieceView(*worn, kMineTokenBase + worn->client_slot);
        row.same_base = row.theirs.has_value() && row.theirs->item_id == row.mine->item_id;
      }
      built.rows.push_back(std::move(row));
    }
    view_.sections.push_back(std::move(built));
  };

  section(Tr("Équipement###equipement_vu_equipement"), kEquipmentOrder, EquipSection::kEquipment);
  // LA SECTION COSTUME N'EXISTE QUE SI LA CIBLE EN PORTE : dix lignes « — » ne
  // disent rien de plus que son absence.
  if (std::ranges::any_of(theirs, [](const ro::sim::InventoryItem& piece) {
        return PlaceOf(piece.worn_at).section == EquipSection::kCostume;
      })) {
    section(Tr("Costume###equipement_vu_costume"), kCostumeOrder, EquipSection::kCostume);
  }
  // HORS GRILLE — munition, ombre —, chacune garde sa ligne.
  ro::ui::ViewEquipSectionView others;
  others.title = Tr("Autres###equipement_vu_autres");
  others.comparable = false;
  for (std::size_t rank = 0; rank < theirs.size(); ++rank) {
    const PiecePlace where = PlaceOf(theirs[rank].worn_at);
    if (!where.ammo && !where.shadow) {
      continue;
    }
    ro::ui::ViewEquipRowView row;
    row.place = where.ammo ? Tr("Munition") : Tr("Ombre");
    row.theirs = PieceView(theirs[rank], static_cast<std::uint32_t>(rank));
    others.rows.push_back(std::move(row));
  }
  if (!others.rows.empty()) {
    view_.sections.push_back(std::move(others));
  }
}

// LE PANTIN, depuis l'EN-TÊTE : ce que la cible montre vraiment à l'écran,
// costumes compris.
//
// ⚠ NI L'ARME NI LE BOUCLIER : le paquet ne porte pas leur apparence. Laissés
// de côté DÉLIBÉRÉMENT — la liste dit déjà les deux pièces, avec leurs cartes
// et leur raffinement.
//
// 🔴 LES COULEURS SONT CELLES QUE PORTE L'ACTEUR, pas un recalcul : ce sont
// celles que le monde applique au personnage d'en face, et le pantin ne peut
// pas en diverger.
void ViewedEquipmentView::ComposeDoll(const ro::sim::ViewedEquipment& sheet,
                                      const ro::sim::World& world, ActorStyles& styles,
                                      const ro::ui::ViewEquipWindowState& state,
                                      std::uint32_t now_ms) {
  view_.doll = ro::ui::SkinImage{};
  if (!doll_.has_value() || wardrobe_ == nullptr || state.doll_width <= 0.0F ||
      state.doll_height <= 0.0F) {
    return;
  }
  ro::render::ActorLook look;
  look.player = true;
  look.job = sheet.look.job;
  look.sex = sheet.look.sex;
  look.hair = sheet.look.hair;
  look.body_style = sheet.look.body2;
  look.headgears = {sheet.look.head_low, sheet.look.head_mid, sheet.look.head_top};
  look.robe = sheet.look.robe;
  look.hair_palette = static_cast<std::int16_t>(sheet.look.hair_color);
  look.body_palette = static_cast<std::int16_t>(sheet.look.clothes_color);
  // La monture est un ÉTAT de l'acteur, que le paquet ne dit pas : la cible est
  // sur notre carte, et son acteur le porte quand il est en vue.
  if (const ro::sim::Actor* actor = shown_id_ != 0 ? world.Find(shown_id_) : nullptr;
      actor != nullptr) {
    look.cash_mount = actor->status_effects.contains(ro::sim::kStatusAllRiding);
  }
  if (shown_id_ != 0) {
    styles.Dress(shown_id_, look, *wardrobe_, world.styles());
  }
  if (ro::render::DrawnAsModel(wardrobe_->catalog(), look)) {
    return;
  }
  ro::render::ActorVignette::Pose pose;
  pose.look = look;
  pose.animation = ro::render::kAnimationIdle;
  pose.direction =
      static_cast<std::size_t>(((state.doll_direction % kDirections) + kDirections) % kDirections);
  // ⚠ L'HORLOGE COURT, mais le corps TIENT son regard : faire défiler l'action
  // de repos ferait tourner la tête sans fin. Ce qu'il porte continue de vivre.
  pose.animated = true;
  pose.held_frame = kLookingAheadFrame;
  pose.elapsed_ms = static_cast<float>(now_ms);
  pose.box.width = static_cast<std::int32_t>(state.doll_width);
  pose.box.height = static_cast<std::int32_t>(state.doll_height);
  const ro::render::IconTexture& drawn = doll_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  view_.doll = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                 .width = state.doll_width,
                                 .height = state.doll_height};
}

ViewedEquipmentView::Designated ViewedEquipmentView::PieceOf(
    const ro::sim::ViewedEquipmentBook& book, const ro::sim::World& world, std::uint32_t token) {
  if (token >= kMineTokenBase) {
    const ro::sim::InventoryItem* worn =
        world.inventory().Find(static_cast<std::uint16_t>(token - kMineTokenBase));
    return worn != nullptr && worn->equipped() ? Designated{.item = worn, .theirs = false}
                                               : Designated{};
  }
  if (!book.shown().has_value() || token >= book.shown()->pieces.size()) {
    return {};
  }
  return Designated{.item = &book.shown()->pieces[token], .theirs = true};
}

void ViewedEquipmentView::Forget(ro::sim::ViewedEquipmentBook& book,
                                 ro::ui::ViewEquipWindowState& state) noexcept {
  state.open = false;
  state.menu.armed = false;
  book.Forget();
  pending_id_ = 0;
  shown_id_ = 0;
  view_ = ro::ui::ViewEquipView{};
}

}  // namespace moonclient
