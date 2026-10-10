#include "world/item_pickup_report.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/world.h"
#include "ro/vfs/item_words.h"

#include "items/inventory_view.h"
#include "items/pickup_refusal.h"

namespace moonclient {
namespace {

// Une phrase prête, avec sa couleur et son rang d'arrivée.
struct PickupLine {
  std::string text;
  ro::sim::PickupArrival arrival = 0;
  std::uint32_t said_ms = 0;
  std::uint32_t color = 0;
  // Sa case de filtre : les gains et les jets ont la leur, un refus de
  // ramassage va dans la case générale.
  ro::sim::ChatType type = ro::sim::ChatType::kItemGetDrop;
};

}  // namespace

void ItemPickupReport::Notice(ro::sim::World& world, const CreatorNames& creators,
                              std::uint32_t now_ms) {
  // 🔴 ON COMPOSE D'ABORD, ON ÉCRIT ENSUITE. Les deux journaux sont lus par des
  // vues sur les vecteurs du sac ; écrire dans le monde entre deux lectures
  // reviendrait à parcourir une collection en la modifiant, et rien ne dirait
  // que c'est ce qui s'est passé.
  std::vector<PickupLine> lines;

  const ro::sim::Inventory& bag = world.inventory();

  // ── LES GAINS ─────────────────────────────────────────────────────────────
  const std::uint64_t last_obtained = bag.last_obtained_seq();
  if (last_obtained < seen_obtained_seq_) {
    // Le rang a reculé : c'est une autre session, et le témoin d'avant ne
    // désigne plus rien.
    seen_obtained_seq_ = 0;
  }
  // ⚠ LES REFUS ATTENDENT AUSSI : écrits seuls, ils passeraient devant le gain
  // retenu qui les précède.
  if (AwaitsCreator(bag, seen_obtained_seq_, creators, now_ms)) {
    return;
  }
  for (const ro::sim::ObtainedItem& entry : bag.obtained()) {
    if (entry.seq <= seen_obtained_seq_) {
      continue;
    }
    if (items_ == nullptr) {
      // Sans table, aucun nom à écrire — le gain se tait, et le refus parlera
      // quand même. Le témoin avance tout de même : une ligne qu'on a choisi de
      // ne pas écrire n'est pas une ligne à rattraper plus tard.
      continue;
    }
    // ⚠ LE MÊME NOM QUE LE BANDEAU : voir `ReportedItemName`.
    const std::string name = ReportedItemName(entry.picked, creators, *items_, *words_);
    const int amount = std::max<int>(entry.picked.amount, 1);
    // Sans le libellé de l'installation, la ligne se tait : le bandeau, lui,
    // montre déjà le gain.
    std::string gain_text = ro::vfs::GotItemText(*words_, name, amount);
    if (gain_text.empty()) {
      continue;
    }
    lines.push_back(PickupLine{.text = std::move(gain_text),
                               .arrival = entry.arrival,
                               .said_ms = entry.obtained_ms,
                               .color = kPickupGainColor});
  }
  seen_obtained_seq_ = last_obtained;

  // ── LES REFUS ─────────────────────────────────────────────────────────────
  const std::uint64_t last_refused = bag.last_refused_seq();
  if (last_refused < seen_refused_seq_) {
    seen_refused_seq_ = 0;
  }
  for (const ro::sim::RefusedPickup& entry : bag.refused()) {
    if (entry.seq <= seen_refused_seq_) {
      continue;
    }
    const std::optional<ro::vfs::ItemPickupRefusal> why = PickupRefusalOf(entry.result);
    if (!why.has_value()) {
      // Un code sans message : le client se tait, nous aussi.
      continue;
    }
    const std::string_view refusal_text = ro::vfs::PickupRefusalText(*words_, *why);
    if (refusal_text.empty()) {
      continue;
    }
    lines.push_back(PickupLine{.text = std::string(refusal_text),
                               .arrival = entry.arrival,
                               .said_ms = entry.refused_ms,
                               .color = kPickupRefusalColor,
                               .type = ro::sim::ChatType::kPublic});
  }
  seen_refused_seq_ = last_refused;

  // ── LES JETS ──────────────────────────────────────────────────────────────
  const std::uint64_t last_dropped = bag.last_dropped_seq();
  if (last_dropped < seen_dropped_seq_) {
    seen_dropped_seq_ = 0;
  }
  for (const ro::sim::DroppedItem& entry : bag.dropped()) {
    if (entry.seq <= seen_dropped_seq_ || items_ == nullptr) {
      continue;
    }
    const std::string name = ReportedItemName(entry.dropped, creators, *items_, *words_);
    std::string drop_text =
        ro::vfs::DroppedItemText(*words_, name, std::max<int>(entry.dropped.amount, 1));
    if (drop_text.empty()) {
      continue;
    }
    lines.push_back(PickupLine{.text = std::move(drop_text),
                               .arrival = entry.arrival,
                               .said_ms = entry.dropped_ms,
                               .color = kPickupDropColor});
  }
  seen_dropped_seq_ = last_dropped;

  // ⚠ REMISES DANS L'ORDRE D'ARRIVÉE, et non dans celui des deux journaux : ils
  // sont lus l'un après l'autre, si bien qu'un refus survenu AVANT un gain
  // s'écrirait après lui — et paraîtrait porter sur l'objet que le gain vient
  // de nommer, seul objet cité à l'écran.
  //
  // 🔴 ET SUR LE RANG D'ARRIVÉE, JAMAIS SUR L'HORLOGE : l'appelant date toute
  // une fournée de trames du même instant, celui où il l'a drainée. Trier sur
  // `said_ms` ne départagerait donc rien, dans le cas même où il le faut.
  std::sort(lines.begin(), lines.end(), [](const PickupLine& first, const PickupLine& second) {
    return first.arrival < second.arrival;
  });

  // ⚠ L'INSTANT EST CELUI DU FAIT, pas celui du relevé : une ligne relevée en
  // retard ne doit pas paraître plus récente que ce qui l'a suivie.
  for (PickupLine& line : lines) {
    if (line.text.empty()) {
      continue;
    }
    // ⚠ LEUR PROPRE GENRE, ET NON « SERVEUR » : le serveur les envoie, mais
    // TOUTE entrée dans le sac en écrit une — coffre repris, achat, courrier.
    // Rangées sous « serveur », s'en défaire emporterait les réponses
    // d'@commande, les échecs de compétence, et les refus eux-mêmes.
    world.RecordChat(ro::sim::ChatKind::kItem, line.type, 0, std::move(line.text), line.said_ms,
                     line.color);
  }
}

}  // namespace moonclient
