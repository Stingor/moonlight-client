#include "character/shortcut_view.h"

#include <utility>

#include <SDL3/SDL_keyboard.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/inventory.h"
#include "ro/net/skill.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"
#include "ro/ui/staff_tools.h"

#include "character/equip_slot_map.h"
#include "character/shortcut_file.h"
#include "game/homunculus_stage.h"
#include "game/mercenary_stage.h"

namespace moonclient {

// 🔴 LES DEUX NOMBRES DES BARRES VIVENT DANS DEUX COUCHES, et rien ne les relie
// à part ceci. `ui` ne dépend pas de `sim` — c'est le graphe des couches, tenu
// par l'éditeur de liens —, donc chacune porte les siens. `app` est la seule à
// connaître les deux : c'est donc ici que la divergence doit devenir une erreur
// de compilation, et non une case qui se dessine au mauvais endroit. Fiche 0038.
static_assert(ro::sim::kShortcutBarCount == ro::ui::kShortcutBarCount);
static_assert(ro::sim::kShortcutSlotsPerBar == ro::ui::kShortcutSlotsPerBar);
static_assert(ro::sim::kShortcutSlotCount == ro::ui::kShortcutSlotTotal);

namespace {

// L'objet du sac qui porte cet identifiant, ou rien.
//
// ⚠ LA PREMIERE PILE TROUVEE, et cela suffit : le serveur retire d'abord de
// celle-la, et la suivante prendra le relais a l'image d'apres.
[[nodiscard]] const ro::sim::InventoryItem* FindInBag(const ro::sim::World& world,
                                                      std::uint32_t item_id) {
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.item_id == item_id) {
      return &item;
    }
  }
  return nullptr;
}

// Combien il en reste EN TOUT, toutes piles confondues.
[[nodiscard]] int RemainingInBag(const ro::sim::World& world, std::uint32_t item_id) {
  int total = 0;
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.item_id == item_id) {
      total += item.amount;
    }
  }
  return total;
}

}  // namespace

ShortcutBarView::ShortcutBarView(std::filesystem::path client_root, const ro::sim::SkillTree* tree,
                                 ro::render::SkillIcons* icons)
    : client_root_(std::move(client_root)), tree_(tree), icons_(icons) {}

const ro::sim::GuildSkill* ShortcutBarView::GuildSkillOf(std::uint16_t skill_id, bool* held) const {
  if (held != nullptr) {
    *held = false;
  }
  if (guild_ == nullptr || skill_id < ro::sim::kFirstGuildSkill) {
    return nullptr;
  }
  const ro::sim::GuildSkill* skill = guild_->FindSkill(skill_id);
  if (skill == nullptr || skill->level <= 0) {
    return nullptr;
  }
  if (held != nullptr) {
    *held = true;
  }
  return guild_->MayWield(skill_id, guild_char_id_) ? skill : nullptr;
}

void ShortcutBarView::FollowCharacter(const ro::sim::World& world) {
  const std::string_view named = world.player_name();
  if (!named.empty() && named != character_) {
    // ⚠ LA BARRE REPART DE ZÉRO AVANT D'ÊTRE RELUE. Sans cela, les cases du
    // personnage précédent resteraient sous celles que le fichier ne remplit
    // pas.
    bar_ = ro::sim::ShortcutBars();
    character_ = std::string(named);
    job_.reset();
    LoadShortcutBar(client_root_, character_, tree_, bar_);
    // Ce qui vient d'être lu est déjà « enregistré » : sans cela, le fichier se
    // réécrirait à la première image.
    saved_revision_ = bar_.revision();
  }
  FollowJob(world);
}

void ShortcutBarView::FollowJob(const ro::sim::World& world) {
  // Le niveau de groupe arrive APRÈS le nom : tant qu'il n'est pas là, la
  // barre reste le jeu du personnage, et la bascule se fait à son arrivée.
  const ro::sim::Actor* self = world.player();
  if (character_.empty() || self == nullptr || world.group_level() < ro::ui::kStaffMinGroupLevel) {
    return;
  }
  const std::uint16_t job = self->look.job;
  if (job_ == job) {
    return;
  }
  // 🔴 CE QUI A BOUGÉ SOUS L'ANCIEN MÉTIER S'ÉCRIT SOUS L'ANCIEN MÉTIER, avant
  // que `job_` ne désigne le nouveau.
  SaveIfNeeded();
  job_ = job;
  ro::sim::ShortcutBars retained;
  if (LoadJobShortcutBar(client_root_, character_, job, tree_, retained)) {
    bar_ = retained;
    saved_revision_ = bar_.revision();
    return;
  }
  // ⚠ UN MÉTIER SANS JEU GARDE LA BARRE QUI EST À L'ÉCRAN, et la retient
  // aussitôt comme la sienne. Repartir d'une barre vide ferait perdre les
  // objets, qui servent à tous les métiers.
  saved_revision_ = bar_.revision();
  static_cast<void>(SaveShortcutBar(client_root_, character_, tree_, bar_, job_));
}

void ShortcutBarView::Refresh(const ro::sim::World& world, const Hotkeys& bindings,
                              std::uint32_t now_ms) {
  const std::span<const ro::sim::ShortcutSlot> slots = bar_.slots();
  keys_.clear();
  keys_.reserve(slots.size());
  for (std::size_t rank = 0; rank < slots.size(); ++rank) {
    // ⚠ UNE CASE PEUT N'AVOIR AUCUNE TOUCHE, et la plupart n'en ont pas : seule
    // la première barre en reçoit par défaut. Le libellé est alors vide, et la
    // case se dessine sans étiquette.
    //
    // 🔴 AVEC SES MODIFICATEURS. La barre composait le sien sans eux, au motif
    // qu'une case est petite ; tant que neuf cases portaient F1 à F9, cela
    // restait juste. Sept barres sur huit n'ont désormais de touches qu'avec un
    // modificateur — « F1 » pour Ctrl+F1 est un MENSONGE, et la case d'à côté
    // ment de la même façon. C'est le dessin qui rétrécit ce qui déborde.
    keys_.push_back(LabelOfBinding(bindings.Of(ActionRef::OfShortcut(rank))));
  }

  // ⚠ LES VUES SE POSENT APRÈS, quand le tampon des libellés ne bouge plus.
  slots_.clear();
  slots_.reserve(slots.size());
  for (std::size_t rank = 0; rank < slots.size(); ++rank) {
    const ro::sim::ShortcutSlot& slot = slots[rank];
    ro::ui::ShortcutSlotView view;
    view.key = keys_[rank];
    view.highlighted = highlighted_skill_ != 0 && slot.skill_id == highlighted_skill_;
    if (!slot.filled()) {
      slots_.push_back(view);
      continue;
    }
    // UN OBJET SE MONTRE AUTREMENT : son nom vient de la table de
    // l'installation, son icône du même dossier que celles des compétences, et
    // ce qui le grise n'est pas l'oubli mais l'épuisement.
    if (slot.holds_item()) {
      view.item_id = slot.item_id;
      const ro::vfs::ItemEntry* entry =
          item_table_ != nullptr ? item_table_->Find(slot.item_id) : nullptr;
      if (entry != nullptr) {
        view.name = entry->name;
      }
      if (item_icons_ != nullptr) {
        const ro::render::IconTexture& icon = item_icons_->Get(slot.item_id);
        view.icon = ro::ui::SkinImage{
            .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
            .width = static_cast<float>(icon.width_px),
            .height = static_cast<float>(icon.height_px)};
      }
      view.amount = RemainingInBag(world, slot.item_id);
      view.known = view.amount > 0;
      slots_.push_back(view);
      continue;
    }
    view.skill_id = slot.skill_id;
    view.level = slot.level;
    // ⚠ TROIS SOURCES CROISÉES ICI, ET NULLE PART AILLEURS : l'arbre du client
    // pour le nom et l'icône, le livre du serveur pour ce qui est encore appris
    // et pour l'attente, la barre pour le rang qu'on a posé.
    const ro::sim::SkillNode* node = tree_ != nullptr ? tree_->Find(slot.skill_id) : nullptr;
    if (node != nullptr) {
      view.name = node->display_name;
      if (icons_ != nullptr) {
        const ro::render::IconTexture& icon = icons_->Get(slot.skill_id, node->idname);
        view.icon = ro::ui::SkinImage{
            .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
            .width = static_cast<float>(icon.width_px),
            .height = static_cast<float>(icon.height_px)};
      }
    }
    // ⚠ LE RANG APPRIS VIENT DU SERVEUR, ET IL BORNE LE RÉGLAGE DE LA CASE :
    // Maj + molette ne doit pas monter au-delà de ce qu'il accepterait.
    const ro::sim::KnownSkill* learned = world.skill_book().Find(slot.skill_id);
    view.known = learned != nullptr;
    view.most_level = learned != nullptr ? static_cast<int>(learned->level) : 0;
    if (const ro::sim::GuildSkill* guild = GuildSkillOf(slot.skill_id); guild != nullptr) {
      view.known = true;
      view.most_level = guild->level;
    }
    if (const ro::sim::HomunculusSkill* homunculus =
            homunculus_ != nullptr ? homunculus_->FindSkill(slot.skill_id) : nullptr;
        homunculus != nullptr) {
      view.known = true;
      view.most_level = homunculus->level;
    } else if (const ro::sim::MercenarySkill* mercenary =
                   mercenary_ != nullptr ? mercenary_->FindSkill(slot.skill_id) : nullptr;
               mercenary != nullptr) {
      view.known = true;
      view.most_level = mercenary->level;
    }
    view.cooldown = world.skill_book().CooldownFraction(slot.skill_id, now_ms);
    // ⚠ ET LE TEMPS EN CLAIR, pas seulement la part : sur trois minutes, le voile
    // bouge d'un pixel par seconde et ne dit pas s'il en reste dix ou cent.
    view.cooldown_ms = world.skill_book().CooldownRemainingMs(slot.skill_id, now_ms);
    slots_.push_back(view);
  }
}

void ShortcutBarView::Cast(std::size_t slot, SkillBookPanel& book, ro::net::Connection& link,
                           ro::sim::World& world, std::uint32_t now_ms, bool repeated) {
  const ro::sim::ShortcutSlot what = bar_.At(slot);
  if (!what.filled()) {
    return;
  }
  // 🔴 UNE TOUCHE TENUE NE RÉPÈTE JAMAIS UN OBJET, ET UNE COMPÉTENCE SOUS UNE
  // CONDITION : voir `Cast` dans l'en-tête.
  if (repeated) {
    LatchCasting(book);
    if (what.holds_item()) {
      return;
    }
    if (repeating_slot_ != slot) {
      return;
    } else if (const ro::sim::KnownSkill* armed = book.armed();
               armed != nullptr && armed->id == what.skill_id) {
      // ⚠ DÉJÀ ARMÉE, ELLE LE RESTE : un second choix de la même compétence la
      // désarme dans le grimoire, et la répétition la ferait clignoter.
      return;
    }
  }
  if (what.holds_item()) {
    // 🔴 ON CHERCHE LA PLACE AU MOMENT DE PARTIR, jamais avant. La case retient
    // un identifiant d'objet ; sa place dans le sac change dès qu'on consomme la
    // pile précédente, et une place retenue désignerait bientôt autre chose.
    const ro::sim::InventoryItem* held = ShortcutTargetIn(world.inventory(), what.item_id);
    if (held == nullptr) {
      world.RecordChat(ro::sim::ChatKind::kServer, 0,
                       std::string(ro::core::Tr("Il n'en reste plus.")), now_ms);
      return;
    }
    const ShortcutItemGesture gesture = ShortcutGestureOf(*held);
    if (gesture.act == ShortcutItemAct::kNothing) {
      return;
    }
    // ⚠ PORTER NE COMPTE PAS COMME UN USAGE : Quick cast répète ce qui se
    // consomme, et une pièce qu'on remet à chaque période ferait du bruit.
    if (gesture.act == ShortcutItemAct::kWear) {
      if (auto sent = ro::net::RequestEquip(link, held->client_slot, gesture.wear_at); !sent) {
        ro::core::LogWarning("barre : demande non émise — {}", ro::core::Describe(sent.error()));
      }
      return;
    }
    if (auto sent = ro::net::RequestUseItem(link, held->client_slot, world.player_id()); !sent) {
      ro::core::LogWarning("barre : demande non émise — {}", ro::core::Describe(sent.error()));
      return;
    }
    ++item_uses_;
    last_item_slot_ = slot;
    last_item_id_ = what.item_id;
    return;
  }
  // UNE COMPÉTENCE DE GUILDE se lance sur soi, au rang de la case borné par le
  // rang appris — sans passer par le grimoire, qui ne la connaît pas.
  bool guild_held = false;
  const ro::sim::GuildSkill* guild = GuildSkillOf(what.skill_id, &guild_held);
  if (guild == nullptr && guild_held) {
    // ⚠ LE SERVEUR LA REFUSERAIT SANS UN MOT : le dire ici plutôt qu'envoyer.
    world.RecordChat(ro::sim::ChatKind::kServer, 0,
                     std::string(ro::core::Tr("Réservée au maître de guilde.")), now_ms);
    return;
  }
  if (guild != nullptr) {
    const int level = what.level > 0 && what.level <= guild->level ? what.level : guild->level;
    if (auto sent = ro::net::RequestSkillOnTarget(
            link, what.skill_id, static_cast<std::uint16_t>(level), world.player_id());
        !sent) {
      ro::core::LogWarning("barre : demande non émise — {}", ro::core::Describe(sent.error()));
    } else {
      ++guild_casts_;
    }
    return;
  }
  if (CastCompanionSkill(what.skill_id, what.level, link, world, now_ms)) {
    return;
  }
  const ro::sim::KnownSkill* known = world.skill_book().Find(what.skill_id);
  if (known == nullptr) {
    // ⚠ LA CASE NE S'EFFACE PAS. Le serveur n'a peut-être pas encore parlé de
    // cette carte, ou le métier a changé et changera encore : une case vidée
    // d'elle-même serait perdue pour de bon.
    world.RecordChat(ro::sim::ChatKind::kServer, 0,
                     std::string(ro::core::Tr("Ce skill n'est plus connu.")), now_ms);
    return;
  }
  // ⚠ LE RANG DE LA CASE, PAS CELUI QU'ON SAIT. Poser une compétence au rang
  // trois puis la monter ne doit pas changer ce qui part : c'est ce que le
  // joueur a réglé.
  ro::sim::KnownSkill at_slot = *known;
  at_slot.level = what.level > 0 && what.level <= known->level ? what.level : known->level;
  book.ChooseSkill(link, world, at_slot, now_ms);
}

bool ShortcutBarView::CastCompanionSkill(std::uint16_t skill_id, int level,
                                         ro::net::Connection& link, ro::sim::World& world,
                                         std::uint32_t now_ms) {
  if (homunculus_ != nullptr && homunculus_->FindSkill(skill_id) != nullptr) {
    if (homunculus_->UseSkill(link, world, skill_id, level, now_ms)) {
      ++companion_casts_;
    }
    return true;
  }
  const ro::sim::MercenarySkill* skill =
      mercenary_ != nullptr ? mercenary_->FindSkill(skill_id) : nullptr;
  if (skill == nullptr) {
    return false;
  }
  // ⚠ CELLE QUI VISE UNE CIBLE OU UNE CASE NE FAIT QUE S'ARMER : ce n'est pas un
  // lancer, et elle ne compte pas.
  const bool only_arms = (skill->targeting & ro::sim::kSkillOnSelf) == 0;
  // ⚠ LE RANG APPRIS, comme l'onglet du mercenaire : c'est lui qui l'envoie, et
  // qui refuse ce qui ne part pas, en disant pourquoi.
  if (const char* refused = mercenary_->Cast(link, skill_id); refused != nullptr) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(refused), now_ms);
    return true;
  }
  if (!only_arms) {
    ++companion_casts_;
  }
  return true;
}

bool ShortcutBarView::RepeatItemSlot(std::size_t slot, std::uint32_t item_id,
                                     ro::net::Connection& link, const ro::sim::World& world) {
  const ro::sim::ShortcutSlot what = bar_.At(slot);
  if (!what.holds_item() || what.item_id != item_id) {
    return false;
  }
  const ro::sim::InventoryItem* held = ShortcutTargetIn(world.inventory(), item_id);
  if (held == nullptr || ShortcutGestureOf(*held).act != ShortcutItemAct::kUse) {
    return false;
  }
  if (auto sent = ro::net::RequestUseItem(link, held->client_slot, world.player_id()); !sent) {
    ro::core::LogWarning("barre : demande non émise — {}", ro::core::Describe(sent.error()));
    return false;
  }
  return true;
}

void ShortcutBarView::LatchCasting(const SkillBookPanel& book) {
  const std::uint32_t casts = book.entity_casts() + guild_casts_ + companion_casts_;
  if (casts != seen_entity_casts_) {
    seen_entity_casts_ = casts;
    repeating_slot_ = pressed_slot_;
  }
}

void ShortcutBarView::NotePress(std::optional<std::size_t> slot, const SkillBookPanel& book) {
  LatchCasting(book);
  pressed_slot_ = slot;
}

std::uint32_t ShortcutBarView::DescribedItem(std::size_t slot, const ro::sim::World& world,
                                             std::uint16_t* found_at) const {
  const ro::sim::ShortcutSlot what = bar_.At(slot);
  if (!what.holds_item()) {
    return 0;
  }
  const ro::sim::InventoryItem* held = FindInBag(world, what.item_id);
  if (found_at != nullptr) {
    *found_at = held != nullptr ? held->client_slot : 0;
  }
  return what.item_id;
}

void ShortcutBarView::Apply(const ro::ui::ShortcutBarsAction& asked, SkillBookPanel& book,
                            ro::net::Connection& link, ro::sim::World& world,
                            std::uint32_t now_ms) {
  if (asked.dropped_on >= 0 && asked.dropped.skill_id != 0) {
    bar_.Assign(
        static_cast<std::size_t>(asked.dropped_on),
        ro::sim::ShortcutSlot{.skill_id = asked.dropped.skill_id, .level = asked.dropped.level});
  }
  if (asked.item_dropped_on >= 0 && asked.item_dropped.item_id != 0) {
    bar_.Assign(static_cast<std::size_t>(asked.item_dropped_on),
                ro::sim::ShortcutSlot{.item_id = asked.item_dropped.item_id});
  }
  // UNE CASE POSÉE SUR UNE AUTRE : les deux s'échangent. ⚠ UN ÉCHANGE ET NON UN
  // DÉPLACEMENT — lâcher sur une case occupée rend son contenu à la place qu'on
  // quitte, au lieu de l'écraser pour un geste d'un pixel trop à droite.
  if (asked.moved_from >= 0 && asked.moved_to >= 0) {
    bar_.Swap(static_cast<std::size_t>(asked.moved_from), static_cast<std::size_t>(asked.moved_to));
  }
  // Lâchée HORS des barres, elle se vide : le geste s'apprend seul, là où le
  // clic molette demandait de le deviner.
  if (asked.dropped_away >= 0) {
    bar_.Clear(static_cast<std::size_t>(asked.dropped_away));
  }
  if (asked.cleared >= 0) {
    bar_.Clear(static_cast<std::size_t>(asked.cleared));
  }
  // ⚠ LE RANG SE POSE SUR LA CASE, pas sur la compétence : la barre peut porter
  // la MÊME deux fois à deux rangs différents.
  if (asked.level_for >= 0) {
    bar_.SetLevel(static_cast<std::size_t>(asked.level_for),
                  static_cast<std::uint16_t>(asked.level));
  }
  if (asked.cast >= 0) {
    Cast(static_cast<std::size_t>(asked.cast), book, link, world, now_ms);
  }
  SaveIfNeeded();
}

void ShortcutBarView::PlaceSkill(std::size_t index, std::uint16_t skill_id, std::uint16_t level) {
  if (index >= ro::sim::kShortcutSlotCount) {
    return;
  }
  bar_.Assign(index, ro::sim::ShortcutSlot{.skill_id = skill_id, .level = level});
  SaveIfNeeded();
}

void ShortcutBarView::SaveIfNeeded() {
  // On compare un compte plutôt que de lever un drapeau : la barre se modifie
  // depuis le dépôt, la molette, et demain depuis un menu.
  if (bar_.revision() == saved_revision_ || character_.empty()) {
    return;
  }
  // Retenu même si l'écriture échoue : réessayer soixante fois par seconde sur
  // un dossier en lecture seule noierait le journal.
  saved_revision_ = bar_.revision();
  static_cast<void>(SaveShortcutBar(client_root_, character_, tree_, bar_, job_));
}

}  // namespace moonclient
