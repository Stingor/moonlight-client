#include "character/staff_view.h"

#include <array>
#include <cstdio>
#include <string>

#include "ro/core/log.h"
#include "ro/net/chat_target.h"
#include "ro/sim/companion.h"
#include "ro/sim/world.h"

#include "chat/navigation_command.h"
#include "chat/staff_item_command.h"

namespace moonclient {
namespace {

// LE NOM DE LA COMMANDE D'UNE CARACTÉRISTIQUE, dans l'ordre du serveur
// (`parameter_names[]` : str, agi, vit, int, dex, luk). C'est le MÊME ordre que
// celui des rangées du volet des nombres, et c'est ce qui laisse le volet du
// staff s'aligner dessus sans conversion.
[[nodiscard]] const char* AttributeCommand(int rank) {
  constexpr std::array<const char*, 6> kNames = {"str", "agi", "vit", "int", "dex", "luk"};
  if (rank < 0 || rank >= static_cast<int>(kNames.size())) {
    return kNames[0];
  }
  return kNames[static_cast<std::size_t>(rank)];
}

}  // namespace

void FeedStaffPane(const ro::sim::World& world, StaffJobs& jobs, const ro::vfs::JobNames* names,
                   ro::ui::StaffPaneView& pane) {
  pane.jobs = jobs.Rebuild(names);
  const ro::sim::Actor* self = world.player();
  pane.own_job = self != nullptr ? self->look.job : 0;

  // ⚠ ZÉRO VEUT DIRE « LE SERVEUR N'A RIEN DIT », et ce n'est pas la vitesse
  // normale : tant qu'il se tait, le curseur garde son défaut plutôt que de
  // sauter sur un chiffre inventé.
  const ro::sim::SessionSettings& session = world.session();
  pane.server_speed =
      session.walk_speed > 0 ? static_cast<int>(session.walk_speed) : ro::ui::kWalkSpeedNormal;
  // ⚠ CE QUE LE SERVEUR ANNONCE, et non un souvenir : il garde ces deux états
  // d'une connexion à l'autre et les redit après chaque bascule.
  pane.killer = session.killer;
  pane.killable = session.killable;
  pane.player_test = session.player_test;
  pane.no_delay = session.no_delay;

  // 🔴 L'ÉTAT VISUEL VIENT DU SERVEUR, ET DE LUI SEUL. Le client d'origine le
  // devine depuis son masque d'option — et il se trompe : sous `NEW_CARTS`, le
  // chariot n'y est pas. Voir `ro/sim/companion.h`.
  const ro::sim::CompanionState& told = world.companions();
  pane.riding = told.mounted;
  pane.falcon = told.falcon_out;
  pane.cart_level = static_cast<int>(told.cart_type);
  // ⚠ L'OPTION ET NON UN SOUVENIR : le serveur la renvoie à l'entrée sur la
  // carte et après chaque bascule, et il la garde d'une connexion à l'autre.
  pane.hidden = self != nullptr && (self->option & ro::sim::kOptionInvisible) != 0;
}

void KeepBattleIgnore(bool wanted, StaffSession& session, ro::net::Connection& link,
                      const ro::sim::World& world) {
  if (wanted == session.battle_ignored) {
    return;
  }
  // ⚠ NOTÉ MÊME SI L'ENVOI ÉCHOUE : sans cela, un lien coupé ferait réessayer
  // à chaque image, et le journal s'en remplirait.
  session.battle_ignored = wanted;
  if (auto sent = ro::net::SendServerCommand(link, world.player_name(), "@battleignore"); !sent) {
    ro::core::LogWarning("commande de staff « @battleignore » non émise : {}",
                         ro::core::Describe(sent.error()));
  }
}

void SendStaffOrder(const ro::ui::StaffAction& asked, ro::net::Connection& link,
                    const ro::sim::World& world) {
  // Assez pour la plus longue : « @refine 512 -10 », et pour un zeny à dix
  // chiffres signé.
  std::array<char, 48> line{};
  switch (asked.order) {
    case ro::ui::StaffOrder::kNone:
      return;
    case ro::ui::StaffOrder::kAdjustStat:
      // ⚠ LE SIGNE EST DANS LE FORMAT : le serveur lit « @str <+/-ajustement> »,
      // et un montant nu passerait pour une valeur cible.
      std::snprintf(line.data(), line.size(), "@%s %+d", AttributeCommand(asked.stat),
                    asked.amount);
      break;
    case ro::ui::StaffOrder::kResetStats:
      std::snprintf(line.data(), line.size(), "@resetstat");
      break;
    case ro::ui::StaffOrder::kMaxStats:
      std::snprintf(line.data(), line.size(), "@allstat");
      break;
    case ro::ui::StaffOrder::kChangeJob:
      std::snprintf(line.data(), line.size(), "@job %u", asked.job_id);
      break;
    case ro::ui::StaffOrder::kSetSpeed:
      std::snprintf(line.data(), line.size(), "@speed %d", asked.amount);
      break;
    case ro::ui::StaffOrder::kNormalSpeed:
      // 🔴 UNE VALEUR NÉGATIVE, ET NON « 150 ». Le serveur pose un verrou dès que
      // la vitesse s'écarte du défaut ; seule une valeur négative le relâche.
      // Envoyer 150 rendrait la bonne vitesse et GARDERAIT le verrou.
      std::snprintf(line.data(), line.size(), "@speed -1");
      break;
    case ro::ui::StaffOrder::kAdjustBaseLevel:
      std::snprintf(line.data(), line.size(), "@blvl %+d", asked.amount);
      break;
    case ro::ui::StaffOrder::kAdjustJobLevel:
      std::snprintf(line.data(), line.size(), "@jlvl %+d", asked.amount);
      break;
    case ro::ui::StaffOrder::kAdjustZeny:
      std::snprintf(line.data(), line.size(), "@zeny %+d", asked.amount);
      break;
    case ro::ui::StaffOrder::kGiveStatPoints:
      std::snprintf(line.data(), line.size(), "@stpoint %d", asked.amount);
      break;
    case ro::ui::StaffOrder::kRefine:
      // Deux arguments : le masque d'emplacement, puis l'ajustement signé.
      std::snprintf(line.data(), line.size(), "@refine %u %+d", asked.mask, asked.amount);
      break;
    case ro::ui::StaffOrder::kHeal:
      std::snprintf(line.data(), line.size(), "@heal");
      break;
    case ro::ui::StaffOrder::kRepairAll:
      std::snprintf(line.data(), line.size(), "@repairall");
      break;
    case ro::ui::StaffOrder::kToggleHide:
      std::snprintf(line.data(), line.size(), "@hide");
      break;
    case ro::ui::StaffOrder::kToggleKiller:
      std::snprintf(line.data(), line.size(), "@killer");
      break;
    case ro::ui::StaffOrder::kToggleKillable:
      std::snprintf(line.data(), line.size(), "@killable");
      break;
    case ro::ui::StaffOrder::kTogglePlayerTest:
      std::snprintf(line.data(), line.size(), "@playertest");
      break;
    case ro::ui::StaffOrder::kToggleNoDelay:
      std::snprintf(line.data(), line.size(), "@nodelay");
      break;
    case ro::ui::StaffOrder::kSetBattleIgnore:
      // ⚠ RIEN À ENVOYER ICI : c'est un souhait, que l'étage retient et que
      // `KeepBattleIgnore` fait valoir.
      return;
    case ro::ui::StaffOrder::kSetOption:
      // ⚠ LES DEUX PREMIERS PARAMÈTRES À ZÉRO : ils portent les vrais statuts —
      // pétrifié, gelé, maudit —, et la commande les ÉCRASE avec ce qu'on lui
      // donne. Le staff qui pose un état visuel veut être propre, pas gelé.
      std::snprintf(line.data(), line.size(), "@option 0 0 %u", asked.mask);
      break;
    case ro::ui::StaffOrder::kWipeInventory:
      std::snprintf(line.data(), line.size(), "@itemreset");
      break;
  }
  if (auto sent = ro::net::SendServerCommand(link, world.player_name(), line.data()); !sent) {
    // ⚠ LA COMMANDE COMPOSÉE, comme dans `skill_view.cpp` : les deux fenêtres
    // écrivaient la même phrase, et rien ne les distinguait dans le journal.
    ro::core::LogWarning("commande de staff « {} » non émise : {}", line.data(),
                         ro::core::Describe(sent.error()));
  }
}

void SendStaffWarp(const PlaceLink& place, ro::ui::SessionRights rights, ro::net::Connection& link,
                   const ro::sim::World& world) {
  if (place.map.empty() || !rights.staff_tools) {
    return;
  }
  const std::string command = StaffWarpCommand(place);
  if (auto sent = ro::net::SendServerCommand(link, world.player_name(), command); !sent) {
    ro::core::LogWarning("commande de staff « {} » non émise : {}", command,
                         ro::core::Describe(sent.error()));
  }
}

void SendStaffItems(std::span<const ro::ui::StaffItemGrant> grants, ro::ui::SessionRights rights,
                    ro::net::Connection& link, const ro::sim::World& world) {
  for (const std::string& command : StaffItemCommands(grants, rights)) {
    if (auto sent = ro::net::SendServerCommand(link, world.player_name(), command); !sent) {
      ro::core::LogWarning("commande de staff « {} » non émise : {}", command,
                           ro::core::Describe(sent.error()));
    }
  }
}

}  // namespace moonclient
