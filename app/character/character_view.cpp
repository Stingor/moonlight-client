#include "character/character_view.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/character.h"
#include "ro/net/character_ledger.h"
#include "ro/net/companion.h"
#include "ro/net/equip_attributes.h"
#include "ro/net/inventory.h"
#include "ro/net/player_config.h"
#include "ro/proto/map_packets.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/player_bonuses.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_name.h"

#include "character/character_bonus_rows.h"
#include "character/equip_attribute_lines.h"
#include "character/equip_change_line.h"
#include "character/equip_slot_map.h"
#include "items/inventory_view.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

using ro::ui::EquipSlot;

// Le numéro du serveur pour la première des six (`SP_STR`). Les cinq autres
// suivent, dans l'ordre où la fiche les montre.
constexpr std::uint16_t kFirstAttributeParameter =
    static_cast<std::uint16_t>(ro::sim::ServerParameter::kStr);

// Une valeur qui n'est pas encore arrivée ne se montre pas telle quelle : la
// sentinelle vaut moins un, et « -1 » sur une fiche se lit comme un défaut.
[[nodiscard]] std::int64_t Shown(std::int64_t value) {
  return value == ro::sim::kUnknownStat ? 0 : value;
}

// La même chose pour la vie, qui porte SA sentinelle — `kUnknownHealth` —
// parce qu'elle vit sur l'acteur et non dans les nombres du personnage.
[[nodiscard]] std::int64_t Alive(std::int32_t value) {
  return value == ro::sim::kUnknownHealth ? 0 : value;
}

// LES TROIS COMPAGNONS, dans l'ordre où la colonne les empile.
constexpr std::array<ro::sim::CompanionKind, ro::ui::kCompanionCaseMost> kCompanionOrder = {
    ro::sim::CompanionKind::kCart, ro::sim::CompanionKind::kMount, ro::sim::CompanionKind::kFalcon};

}  // namespace

CharacterView::CharacterView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                             ro::render::ItemIcons* icons, const ro::vfs::JobNames* jobs,
                             const ro::sim::SkillTree* skills, const ro::vfs::StatusTable* statuses,
                             ro::render::SkillIcons* skill_icons,
                             ro::render::ActorWardrobe* wardrobe)
    : items_(items),
      words_(words),
      icons_(icons),
      jobs_(jobs),
      skills_(skills),
      statuses_(statuses),
      skill_icons_(skill_icons) {
  if (wardrobe != nullptr) {
    avatar_.emplace(*wardrobe);
  }
}

std::size_t CharacterPoseAnimation(ro::ui::CharacterPose pose) {
  switch (pose) {
    case ro::ui::CharacterPose::kIdle:
      return ro::render::kAnimationIdle;
    case ro::ui::CharacterPose::kWalk:
      return ro::render::kAnimationWalk;
    case ro::ui::CharacterPose::kSit:
      return ro::render::kAnimationSit;
    case ro::ui::CharacterPose::kReady:
      return ro::render::kAnimationReady;
  }
  return ro::render::kAnimationIdle;
}

void CharacterView::RefreshAvatar(const ro::sim::World& world,
                                  const ro::ui::CharacterAvatarState& avatar,
                                  ro::ui::SkinImage& image, std::uint32_t now_ms,
                                  const ro::render::ActorLook& dressed) {
  // ⚠ EFFACÉE D'ABORD : une image gardée d'un personnage qui vient de partir
  // montrerait l'ancien, et rien ne le dirait.
  image = ro::ui::SkinImage{};
  const ro::sim::Actor* player = world.player();
  if (!avatar_.has_value() || player == nullptr) {
    return;
  }
  // ⚠ LA BOÎTE VIENT DE LA FENÊTRE, et elle vaut zéro tant que la fiche n'a pas
  // été dessinée une fois : composer sur du vide donnerait une texture d'un
  // pixel, qu'il faudrait ensuite jeter.
  const auto width = static_cast<std::int32_t>(avatar.box_width);
  const auto height = static_cast<std::int32_t>(avatar.box_height);
  if (width <= 0 || height <= 0) {
    return;
  }

  ro::render::ActorVignette::Pose pose;
  pose.look = dressed;
  pose.animation = CharacterPoseAnimation(avatar.pose);
  pose.direction = avatar.direction;
  // ⚠ LES DEUX CONDITIONS : le joueur peut avoir choisi « animé » sur une pose
  // qui ne défile pas — un réglage relu d'une version précédente, par exemple —
  // et le corps se mettrait alors à parcourir des images qui sont des poses.
  pose.animated = avatar.animated && ro::ui::CharacterPoseAnimates(avatar.pose);
  pose.elapsed_ms = static_cast<float>(now_ms);
  pose.box.width = width;
  pose.box.height = height;
  pose.box.min_body_height = avatar.box_height * ro::ui::kCharacterAvatarBodyFloor;

  const ro::render::IconTexture& drawn = avatar_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  // 🔴 LA TAILLE POSÉE EST CELLE DE LA BOÎTE, PAS CELLE DE LA TEXTURE. Celle-ci
  // est en pixels de SPRITE — la boîte divisée par l'échelle du cadrage — et
  // c'est la carte qui l'agrandit, en bilinéaire. Poser ses dimensions à elle
  // dessinerait un personnage à l'échelle un, au milieu d'une case trop grande.
  image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                            .width = avatar.box_width,
                            .height = avatar.box_height};
}

// LES CASES DE COMPAGNON, refaites à chaque image.
//
// 🔴 C'EST LE SERVEUR QUI DIT TOUT — les états et les identifiants de
// compétence par son paquet des compagnons, les niveaux par le grimoire. Aucun
// numéro n'est écrit ici : l'icône se retrouve par l'identifiant reçu, et le
// jour où le serveur renumérote, la case suit. Voir `ro/sim/companion.h`.
void CharacterView::RefreshCompanions(const ro::sim::World& world) {
  companions_.clear();
  const ro::sim::CompanionState& told = world.companions();
  for (std::size_t rank = 0; rank < kCompanionOrder.size(); ++rank) {
    const ro::sim::CompanionKind kind = kCompanionOrder[rank];
    // ⚠ PAS APPRISE, PAS DE CASE. Un métier qui n'aura jamais de chariot ne
    // doit pas en voir une grisée : il n'y a rien à y comprendre.
    if (ro::sim::CompanionLevelOf(told, world.skill_book(), kind) == 0) {
      continue;
    }
    const std::uint16_t skill = ro::sim::CompanionSkillOf(told, kind);
    std::string& name = companion_names_[rank];
    name.clear();
    ro::ui::SkinImage icon;
    const ro::sim::SkillNode* node = skills_ != nullptr ? skills_->Find(skill) : nullptr;
    if (node != nullptr) {
      name = node->display_name;
      if (skill_icons_ != nullptr) {
        const ro::render::IconTexture& drawn = skill_icons_->Get(skill, node->idname);
        if (drawn.usable()) {
          icon = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                   .width = static_cast<float>(drawn.width_px),
                                   .height = static_cast<float>(drawn.height_px)};
        }
      }
    }
    // ⚠ UN REPLI QUI DIT LE NUMÉRO plutôt qu'une case muette : sans les tables
    // de l'installation, l'infobulle reste la seule chose qui désigne la case.
    if (name.empty()) {
      name = std::to_string(static_cast<int>(skill));
    }
    companions_.push_back(
        ro::ui::CompanionCaseView{.rank = static_cast<std::uint8_t>(rank),
                                  .name = name,
                                  .icon = icon,
                                  .active = ro::sim::CompanionActive(told, kind)});
  }
}

void CharacterView::Refresh(bool open, const ro::sim::World& world, bool costume) {
  if (!open) {
    return;
  }
  RefreshCompanions(world);
  slots_.assign(ro::ui::kEquipSlotCount, ro::ui::EquipSlotView{});

  const ro::sim::Inventory& bag = world.inventory();
  for (std::size_t rank = 0; rank < ro::ui::kEquipSlotCount; ++rank) {
    const auto slot = static_cast<EquipSlot>(rank);
    const std::uint32_t location = EquipLocationOf(slot, costume);
    if (location == 0) {
      continue;
    }
    const ro::sim::InventoryItem* piece = bag.WornAt(location);
    if (piece == nullptr) {
      continue;
    }
    // 🔴 LA CASE DU BOUCLIER SE VIDE DEVANT UNE ARME À DEUX MAINS. Le serveur
    // lui donne les DEUX bits — c'est `EQP_ARMS` —, donc elle répond ici ; sans
    // ce tri, le mannequin dessinerait la même arme deux fois et un vrai port à
    // deux armes serait indiscernable d'une claymore.
    if (slot == EquipSlot::kShield && ro::sim::IsTwoHanded(piece->worn_at)) {
      continue;
    }

    std::string name;
    if (items_ != nullptr && words_ != nullptr) {
      ro::vfs::ItemNameFacts facts = ItemNameFactsOf(*piece);
      creators_.Fill(facts);
      name = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
    }
    names_[rank] = std::move(name);

    ro::ui::EquipSlotView& view = slots_[rank];
    view.client_slot = piece->client_slot;
    view.item_id = piece->item_id;
    view.refine_level = piece->refine_level;
    view.amount = piece->amount;
    view.damaged = piece->damaged;
    if (icons_ != nullptr) {
      const ro::render::IconTexture& icon = icons_->Get(piece->item_id);
      view.icon = ro::ui::SkinImage{
          .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
          .width = static_cast<float>(icon.width_px),
          .height = static_cast<float>(icon.height_px)};
    }
  }
  // ⚠ LES NOMS SE DÉSIGNENT APRÈS LA BOUCLE. Ici le tableau ne se réalloue
  // jamais — il a une taille fixe —, mais poser la vue au même endroit que dans
  // le sac garde les deux lisibles de la même façon.
  for (std::size_t rank = 0; rank < ro::ui::kEquipSlotCount; ++rank) {
    slots_[rank].name = names_[rank];
  }

  const ro::sim::PlayerStatus& status = world.player_status();
  const ro::sim::Actor* self = world.player();

  character_name_ = self != nullptr ? self->name : std::string();
  header_ = ro::ui::CharacterHeaderView{
      .name = character_name_,
      // LE MÉTIER, par la table du client. ⚠ VIDE POUR CE QU'ELLE IGNORE, et la
      // fenêtre écrit alors ses niveaux seuls — ce qu'elle faisait pour tout le
      // monde jusqu'ici.
      .job = jobs_ != nullptr && self != nullptr ? jobs_->Find(self->look.job) : std::string_view(),
      .base_level = Shown(status.base_level),
      .job_level = Shown(status.job_level),
      // LA VIE VIT SUR L'ACTEUR, pas dans les nombres du personnage : c'est le
      // seul endroit où les deux se croisent. ⚠ ET SA SENTINELLE EST LA SIENNE,
      // `kUnknownHealth`, distincte de celle des nombres.
      .hp = self != nullptr ? Alive(self->health) : 0,
      .max_hp = self != nullptr ? Alive(self->max_health) : 0,
      .sp = Shown(status.sp),
      .max_sp = Shown(status.max_sp),
  };

  const std::array<std::int64_t, 6> base = {status.str,  status.agi, status.vit,
                                            status.int_, status.dex, status.luk};
  const std::array<std::int64_t, 6> bonus = {status.str_bonus, status.agi_bonus, status.vit_bonus,
                                             status.int_bonus, status.dex_bonus, status.luk_bonus};
  const std::array<std::int64_t, 6> cost = {status.str_cost, status.agi_cost, status.vit_cost,
                                            status.int_cost, status.dex_cost, status.luk_cost};
  for (std::size_t rank = 0; rank < stats_.attributes.size(); ++rank) {
    stats_.attributes[rank] = ro::ui::CharacterAttributeView{
        .base = Shown(base[rank]),
        .bonus = Shown(bonus[rank]),
        // ⚠ LE COÛT GARDE SA SENTINELLE À ZÉRO, et c'est voulu : « pas encore
        // dit » et « gratuit » se ressemblent, et la fenêtre retire ses boutons
        // sur zéro plutôt que d'envoyer une demande qu'on ne sait pas payable.
        .cost = Shown(cost[rank]),
    };
  }

  stats_.status_point = Shown(status.status_point);
  stats_.atk1 = Shown(status.atk1);
  stats_.atk2 = Shown(status.atk2);
  stats_.matk1 = Shown(status.matk1);
  stats_.matk2 = Shown(status.matk2);
  stats_.def1 = Shown(status.def1);
  stats_.def2 = Shown(status.def2);
  stats_.mdef1 = Shown(status.mdef1);
  stats_.mdef2 = Shown(status.mdef2);
  stats_.hit = Shown(status.hit);
  stats_.flee1 = Shown(status.flee1);
  stats_.flee2 = Shown(status.flee2);
  stats_.critical = Shown(status.critical);
  // ⚠ LA VITESSE D'ATTAQUE SE DÉRIVE DU DÉLAI DE MOTION, et la dérivation garde
  // la sentinelle : un délai pas encore arrivé afficherait 200, une valeur
  // plausible que personne ne mettrait en doute.
  stats_.aspd = Shown(ro::sim::DisplayedAspd(status.amotion));

  BuildBonusRows(world.player_bonuses(), bonus_rows_);
  stats_.equipment_bonuses = bonus_rows_;
  const ro::vfs::ItemWords no_words;
  BuildConditionalRows(world.player_bonuses(), words_ != nullptr ? *words_ : no_words,
                       conditional_labels_, conditional_rows_);
  stats_.conditional_bonuses = conditional_rows_;
  const EquipAttributeNames names = Names();
  BuildSkillRows(world.player_bonuses(), skills_, names.status_of, skill_labels_, skill_rows_);
  stats_.skill_bonuses = skill_rows_;
  BuildItemRows(world.player_bonuses(), names.item_of, item_labels_, item_rows_);
  stats_.item_bonuses = item_rows_;
}

EquipAttributeNames CharacterView::Names() const {
  return EquipAttributeNames{
      .words = words_,
      .skills = skills_,
      .status_of =
          [this](std::uint32_t effect_id) {
            return statuses_ != nullptr ? statuses_->NameOf(effect_id) : std::string_view();
          },
      // ⚠ LE NOM NU, SANS RAFFINEMENT NI CARTES : un bonus vise une ESPÈCE
      // d'objet, pas un exemplaire. Composer le nom décoré ferait croire à un
      // objet précis.
      .item_of =
          [this](std::uint32_t item_id) {
            if (items_ == nullptr || words_ == nullptr) {
              return std::string();
            }
            return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(
                ro::vfs::ItemNameFacts{.item_id = item_id}, *items_, *words_));
          }};
}

ro::net::FrameOutcome CharacterView::SayEquipAttributes(const ro::net::Frame& frame,
                                                        ro::sim::World& world,
                                                        std::uint32_t now_ms) const {
  if (frame.opcode != ro::proto::ZcMoonlightEquipAttributes::kOpcode) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  const std::optional<ro::sim::EquipAttributes> told = ro::net::ReadEquipAttributes(frame);
  if (!told.has_value()) {
    return ro::net::FrameOutcome::kUndecodable;
  }
  for (std::string& line : ComposeEquipAttributeLines(*told, Names())) {
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kEquipAttributes, 0,
                     std::move(line), now_ms, EquipAttributesColor(told->equipped));
  }
  return ro::net::FrameOutcome::kApplied;
}

ro::net::FrameOutcome CharacterView::SayEquipChange(const ro::net::Frame& frame,
                                                    ro::sim::World& world,
                                                    std::uint32_t now_ms) const {
  const std::optional<ro::net::EquipChange> change = ro::net::GrantedEquipChangeOf(frame);
  if (!change.has_value()) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  const std::string name = ShownName(world, change->client_slot);
  if (name.empty()) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  EquipChangeLine line = EquipChangeLineOf(name, change->equipped);
  world.RecordChat(ro::sim::ChatKind::kServer, line.type, 0, std::move(line.text), now_ms,
                   line.color);
  return ro::net::FrameOutcome::kApplied;
}

ro::net::FrameOutcome CharacterView::SayEquipSwitch(const ro::net::Frame& frame,
                                                    ro::sim::World& world,
                                                    std::uint32_t now_ms) const {
  const std::optional<ro::sim::EquipSwitchNews> news = ro::net::EquipSwitchNewsOf(frame);
  if (!news.has_value()) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  std::vector<EquipChangeLine> lines = EquipSwitchLines(*news, ShownName(world, news->client_slot));
  if (lines.empty()) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  for (EquipChangeLine& line : lines) {
    world.RecordChat(ro::sim::ChatKind::kServer, line.type, 0, std::move(line.text), now_ms,
                     line.color);
  }
  return ro::net::FrameOutcome::kApplied;
}

std::string CharacterView::ShownName(const ro::sim::World& world, std::uint16_t client_slot) const {
  if (items_ == nullptr || words_ == nullptr) {
    return {};
  }
  // ⚠ LE SAC APRÈS LE MONDE : la pièce y est toujours, posée, rendue ou rangée.
  const ro::sim::InventoryItem* piece = world.inventory().Find(client_slot);
  if (piece == nullptr) {
    return {};
  }
  ro::vfs::ItemNameFacts facts = ItemNameFactsOf(*piece);
  creators_.Fill(facts);
  return ro::vfs::ItemNameText(
      ro::vfs::ComposeItemName(facts, *items_, *words_, ro::vfs::ItemNameSlots::kShown));
}

void CharacterView::Apply(const ro::ui::CharacterWindowAction& asked, ro::net::Connection& link,
                          const ro::sim::World& world) {
  // « MONTRER MON ÉQUIPEMENT » ET « VOIR LES COSTUMES » se lisent EN TÊTE, et
  // sans s'arrêter là : ils ne désignent ni pièce ni compagnon, et ne doivent
  // pas en masquer un.
  if (asked.show_equipment.has_value()) {
    if (const ro::core::Result<void> sent =
            ro::net::RequestShowEquipment(link, *asked.show_equipment);
        !sent) {
      ro::core::LogWarning("fiche : demande non émise — {}", ro::core::Describe(sent.error()));
    }
  }
  if (asked.show_costumes.has_value()) {
    if (const ro::core::Result<void> sent =
            ro::net::RequestShowCostumes(link, *asked.show_costumes);
        !sent) {
      ro::core::LogWarning("fiche : demande non émise — {}", ro::core::Describe(sent.error()));
    }
  }
  // ⚠ BASCULER UN COMPAGNON N'EST PAS UN VERBE DU MANNEQUIN, et c'est pourquoi
  // il se lit AVANT l'aiguillage : il ne désigne aucune pièce, et son rang
  // n'est pas une place du sac.
  if (asked.toggled_companion >= 0 &&
      static_cast<std::size_t>(asked.toggled_companion) < kCompanionOrder.size()) {
    const ro::sim::CompanionKind kind =
        kCompanionOrder[static_cast<std::size_t>(asked.toggled_companion)];
    // 🔴 LE MÊME CLIC APPELLE ET RENVOIE, et c'est l'état COURANT qui tranche —
    // relu ici, pas celui que la case montrait : le serveur a pu changer d'avis
    // entre le dessin et le clic.
    const bool there = ro::sim::CompanionActive(world.companions(), kind);
    const ro::core::Result<void> asked_for = ro::net::RequestCompanion(
        link, kind, there ? ro::net::CompanionWish::kDismiss : ro::net::CompanionWish::kSummon);
    if (!asked_for) {
      ro::core::LogWarning("compagnon : demande non émise — {}",
                           ro::core::Describe(asked_for.error()));
    }
    return;
  }
  ro::core::Result<void> sent;
  switch (asked.verb) {
    case ro::ui::CharacterVerb::kNothing:
      return;
    case ro::ui::CharacterVerb::kTakeOff: {
      // 🔴 LA PLACE SE REVÉRIFIE, on ne se fie pas au clic. Entre le double-clic
      // et l'émission, la pièce a pu être retirée par ailleurs — un changement
      // de tenue, une casse — et agir sur une place libérée ferait au mieux une
      // demande refusée.
      const ro::sim::InventoryItem* piece = world.inventory().Find(asked.slot);
      if (piece == nullptr) {
        return;
      }
      sent = ro::net::RequestUnequip(link, piece->client_slot);
      break;
    }
    case ro::ui::CharacterVerb::kWear: {
      // 🔴 LA PLACE SE REVÉRIFIE ICI AUSSI, et davantage : la charge d'un
      // glisser a traversé une image entière, pendant laquelle le sac a pu se
      // renuméroter.
      const ro::sim::InventoryItem* piece = world.inventory().Find(asked.slot);
      if (piece == nullptr || piece->wearable_at == 0) {
        return;
      }
      sent = ro::net::RequestEquip(link, piece->client_slot,
                                   WearAt(piece->wearable_at, asked.alternate));
      break;
    }
    case ro::ui::CharacterVerb::kRaiseAttribute: {
      if (asked.attribute >= stats_.attributes.size() || asked.amount <= 0) {
        return;
      }
      const auto parameter = static_cast<std::uint16_t>(kFirstAttributeParameter + asked.attribute);
      // ⚠ LE MONTANT TIENT SUR UN OCTET. Le bouton « Max » en demande 255 —
      // plus que ce qui est payable — et c'est le serveur qui le borne au
      // nombre de crans que les points permettent.
      const int amount = asked.amount > 255 ? 255 : asked.amount;
      sent = ro::net::RequestStatUp(link, parameter, static_cast<std::uint8_t>(amount));
      break;
    }
  }
  if (!sent) {
    ro::core::LogWarning("fiche : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

}  // namespace moonclient
