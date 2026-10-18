#include "world/map_edit_sync.h"

#include <array>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/proto/map_edit_packets.h"

namespace moonclient {
namespace {

// La place d'une nouvelle de la barre d'état, zéro final compris.
constexpr std::size_t kStatusCapacity = 256;

// Une nouvelle mise en forme. ⚠ Le format se passe à `snprintf` en littéral,
// le texte traduit au point d'appel : le compilateur en vérifie alors les arguments.
using StatusLine = std::array<char, kStatusCapacity>;

}  // namespace

void MapEditSync::StartSession() {
  inbox_.Reset();
  announced_.clear();
  stale_.clear();
  masked_.clear();
  reloading_.clear();
}

MapEditSync::Report MapEditSync::Serve(std::string_view shown_map, MapEditStore& store,
                                       ro::vfs::Vfs& vfs, ro::net::Connection& link) {
  Report report;
  for (const std::string& refusal : inbox_.TakeRefusals()) {
    ro::core::LogWarning("édition de carte servie refusée — {}", refusal);
  }
  for (ro::net::MapEditAnnounce& announce : inbox_.TakeAnnounces()) {
    if (!ro::proto::IsMapEditName(announce.map)) {
      continue;
    }
    store.Load(announce.map, vfs);
    const std::uint32_t held = store.Revision(announce.map);
    // UNE AUTRE RÉVISION QUE LA NÔTRE SE DEMANDE, plus ancienne comme plus
    // récente : une base remise à zéro a raison contre nous. Sans révision, le
    // serveur ne tient rien, et le document local reste.
    if (announce.revision != 0 && announce.revision != held) {
      ro::core::LogInfo("édition de {} : le serveur tient la révision {}, ce client la {}",
                        announce.map, announce.revision, held);
      if (auto asked = ro::net::RequestMapEdit(link, announce.map); !asked) {
        ro::core::LogWarning("édition de {} non demandée — {}", announce.map,
                             ro::core::Describe(asked.error()));
      }
    }
    const std::string map_name = announce.map;
    announced_[map_name] = std::move(announce);
  }
  // LES VERDICTS AVANT LES DOCUMENTS : l'auteur apprend sa révision avant de
  // recevoir le document qu'il vient d'envoyer, et le reconnaît.
  for (const ro::net::MapEditVerdict& verdict : inbox_.TakeVerdicts()) {
    Judge(verdict, store, link, report);
  }
  for (ro::net::ServedMapEdit& served : inbox_.TakeDocuments()) {
    Receive(std::move(served), shown_map, store, vfs, report);
  }
  return report;
}

void MapEditSync::Judge(const ro::net::MapEditVerdict& verdict, MapEditStore& store,
                        ro::net::Connection& link, Report& report) {
  const auto revision = static_cast<unsigned>(verdict.revision);
  StatusLine line{};
  switch (verdict.kind) {
    case ro::net::MapEditVerdictKind::kAccepted:
      store.SetRevision(verdict.map, verdict.revision);
      stale_.erase(verdict.map);
      masked_.erase(verdict.map);
      ro::core::LogInfo("édition de {} publiée : révision {}", verdict.map, verdict.revision);
      std::snprintf(line.data(), line.size(), ro::core::Tr("Publié sur le serveur : révision %u."),
                    revision);
      report.status = line.data();
      return;
    case ro::net::MapEditVerdictKind::kStale:
      stale_[verdict.map] = verdict.revision;
      ro::core::LogWarning("édition de {} périmée : le serveur tient la révision {}", verdict.map,
                           verdict.revision);
      std::snprintf(
          line.data(), line.size(),
          ro::core::Tr("La révision %u du serveur devance la vôtre : écrasez-la ou rechargez-la."),
          revision);
      report.status = line.data();
      // LE DOCUMENT QUI A DEVANCÉ, pour pouvoir le recharger.
      if (auto asked = ro::net::RequestMapEdit(link, verdict.map); !asked) {
        ro::core::LogWarning("édition de {} non demandée — {}", verdict.map,
                             ro::core::Describe(asked.error()));
      }
      return;
    case ro::net::MapEditVerdictKind::kForbidden:
      report.status = ro::core::Tr("Le serveur refuse : votre niveau ne permet pas de publier.");
      break;
    case ro::net::MapEditVerdictKind::kMalformed:
      report.status = ro::core::Tr("Le serveur n'a pas pu lire le document.");
      break;
    case ro::net::MapEditVerdictKind::kStorage:
      report.status = ro::core::Tr("Le serveur n'a pas pu ranger le document.");
      break;
  }
  ro::core::LogWarning("édition de {} non publiée — {}", verdict.map, report.status);
}

void MapEditSync::Receive(ro::net::ServedMapEdit served, std::string_view shown_map,
                          MapEditStore& store, ro::vfs::Vfs& vfs, Report& report) {
  if (!ro::proto::IsMapEditName(served.map)) {
    ro::core::LogWarning("édition servie pour une carte au nom invalide, ignorée");
    return;
  }
  // UN DOCUMENT INVALIDE NE S'APPLIQUE PAS, ET NE S'ÉCRIT PAS.
  auto revised = ro::mapedit::ParseRevisedMapEdit(served.text, served.map);
  if (!revised) {
    ro::core::LogWarning("édition de {}, révision {} de {}, refusée — {}", served.map,
                         served.revision, served.author, ro::core::Describe(revised.error()));
    if (served.map == shown_map) {
      StatusLine line{};
      std::snprintf(
          line.data(), line.size(),
          ro::core::Tr("La révision %u reçue du serveur est illisible : elle ne s'applique pas."),
          static_cast<unsigned>(served.revision));
      report.status = line.data();
    }
    return;
  }
  store.Load(served.map, vfs);
  ro::mapedit::MapEdit& edit = revised->edit;
  // LE DOCUMENT QU'ON TIENT DÉJÀ — le sien, que l'auteur reçoit en retour — ne
  // fait que s'écrire sous sa révision.
  if (edit == store.Document(served.map) && !store.Unsaved(served.map)) {
    if (auto written = store.Adopt(served.map, std::move(edit), served.revision); !written) {
      ro::core::LogError("édition de {} non écrite — {}", served.map,
                         ro::core::Describe(written.error()));
    }
    return;
  }
  const bool reload = reloading_.erase(served.map) > 0;
  if (store.Unsaved(served.map) && !reload) {
    if (auto written = store.WriteServed(served.map, edit, served.revision); !written) {
      ro::core::LogError("édition de {} non écrite — {}", served.map,
                         ro::core::Describe(written.error()));
    }
    // LA RÉVISION QU'ON VIENT DE PUBLIER, retouchée depuis : rien n'est masqué.
    if (served.revision == store.Revision(served.map)) {
      return;
    }
    ro::core::LogInfo(
        "édition de {} : révision {} de {} reçue, masquée par des retouches non "
        "enregistrées",
        served.map, served.revision, served.author);
    if (served.map == shown_map) {
      StatusLine line{};
      std::snprintf(
          line.data(), line.size(),
          ro::core::Tr("Révision %u de %s reçue : vos retouches non enregistrées la masquent."),
          static_cast<unsigned>(served.revision), served.author.c_str());
      report.status = line.data();
    }
    masked_[served.map] = Masked{std::move(edit), served.revision, std::move(served.author)};
    return;
  }
  Adopt(served.map, std::move(edit), served.revision, served.author, shown_map, store, report);
}

void MapEditSync::Adopt(const std::string& map_name, ro::mapedit::MapEdit edit,
                        std::uint32_t revision, const std::string& author,
                        std::string_view shown_map, MapEditStore& store, Report& report) {
  if (auto adopted = store.Adopt(map_name, std::move(edit), revision); !adopted) {
    ro::core::LogError("édition de {}, révision {}, non écrite — {}", map_name, revision,
                       ro::core::Describe(adopted.error()));
    return;
  }
  masked_.erase(map_name);
  stale_.erase(map_name);
  ro::core::LogInfo("édition de {} : révision {} de {} adoptée", map_name, revision, author);
  if (map_name == shown_map) {
    report.replaced = true;
    StatusLine line{};
    std::snprintf(line.data(), line.size(), ro::core::Tr("Révision %u de %s appliquée."),
                  static_cast<unsigned>(revision), author.c_str());
    report.status = line.data();
  }
}

std::string MapEditSync::Publish(std::string_view map_name, const MapEditStore& store,
                                 ro::net::Connection& link) {
  const auto found = announced_.find(map_name);
  // LE SERVEUR QUI N'A RIEN ANNONCÉ ne connaît peut-être pas ces paquets : un
  // opcode inconnu fermerait la session.
  if (found == announced_.end()) {
    return ro::core::Tr("Enregistré sur ce poste : le serveur ne tient pas les éditions de carte.");
  }
  if (!found->second.can_publish) {
    return ro::core::Tr("Enregistré sur ce poste : votre niveau ne permet pas de publier.");
  }
  const std::string text =
      ro::mapedit::MapEditToYaml(store.Document(map_name), std::string(map_name));
  if (auto sent = ro::net::PublishMapEdit(link, map_name, store.Revision(map_name), text); !sent) {
    ro::core::LogError("édition de {} non publiée — {}", map_name,
                       ro::core::Describe(sent.error()));
    return ro::core::Describe(sent.error());
  }
  ro::core::LogInfo("édition de {} envoyée au serveur, fondée sur la révision {}", map_name,
                    store.Revision(map_name));
  return ro::core::Tr("Enregistré, et envoyé au serveur.");
}

std::string MapEditSync::Overwrite(std::string_view map_name, const MapEditStore& store,
                                   ro::net::Connection& link) {
  const auto found = stale_.find(map_name);
  if (found == stale_.end()) {
    return {};
  }
  const std::uint32_t base_revision = found->second;
  const std::string text =
      ro::mapedit::MapEditToYaml(store.Document(map_name), std::string(map_name));
  if (auto sent = ro::net::PublishMapEdit(link, map_name, base_revision, text); !sent) {
    return ro::core::Describe(sent.error());
  }
  stale_.erase(found);
  ro::core::LogInfo("édition de {} renvoyée pour écraser la révision {}", map_name, base_revision);
  return ro::core::Tr("Envoyé au serveur, pour écraser sa révision.");
}

MapEditSync::Report MapEditSync::Reload(std::string_view map_name, std::string_view shown_map,
                                        MapEditStore& store, ro::net::Connection& link) {
  Report report;
  if (const auto found = masked_.find(map_name); found != masked_.end()) {
    Masked masked = std::move(found->second);
    masked_.erase(found);
    Adopt(std::string(map_name), std::move(masked.edit), masked.revision, masked.author, shown_map,
          store, report);
    return report;
  }
  // LE DOCUMENT N'EST PAS ENCORE LÀ : il s'adoptera à son arrivée.
  reloading_.insert(std::string(map_name));
  if (auto asked = ro::net::RequestMapEdit(link, map_name); !asked) {
    report.status = ro::core::Describe(asked.error());
    return report;
  }
  report.status = ro::core::Tr("Révision demandée au serveur.");
  return report;
}

}  // namespace moonclient
