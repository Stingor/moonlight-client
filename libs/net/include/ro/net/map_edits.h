#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_edit_packets.h"

namespace ro::net {

// ── LES ÉDITIONS DE CARTE QUE LE SERVEUR TIENT (ADR 0124) ───────────────────
//
//   CZ 0f45  un morceau du document qu'enregistre le staff
//   ZC 0f46  <carte>.16B <verdict>.B <révision>.L
//   ZC 0f47  un morceau d'un document servi
//   ZC 0f48  <carte>.16B <révision>.L <publication>.B
//   CZ 0f49  <carte>.16B
//
// 🔴 RÉSERVÉ À QUI ANNONCE `kUiCapMapEdits` : le serveur n'écrit rien de tout
// cela à un autre, et ignore ses envois.

// Le verdict d'un envoi. ⚠ Valeurs sur le fil.
enum class MapEditVerdictKind : std::uint8_t {
  kAccepted = 0,
  kStale = 1,
  kForbidden = 2,
  kMalformed = 3,
  kStorage = 4,
};

// « Voici la révision que je détiens pour la carte où tu entres » ; 0 : aucune.
struct MapEditAnnounce {
  std::string map;
  std::uint32_t revision = 0;
  bool can_publish = false;
};

// Le verdict d'un envoi. `revision` : la nouvelle si accepté, la courante si
// périmé.
struct MapEditVerdict {
  std::string map;
  MapEditVerdictKind kind = MapEditVerdictKind::kMalformed;
  std::uint32_t revision = 0;
};

// Un document servi, entier et décompressé. Il n'est pas encore validé.
struct ServedMapEdit {
  std::string map;
  std::uint32_t revision = 0;
  std::string author;
  std::string text;
};

// LE TEXTE COMPRESSÉ, prêt à partir, ou l'erreur qui l'en empêche — vide, ou
// au-delà d'un plafond.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> ZipMapEdit(std::string_view text);
// LE TEXTE D'UN FLUX, qui doit donner EXACTEMENT `text_size` octets : un flux
// qui en promet moins qu'il n'en donne est refusé au plafond, sans allouer plus.
[[nodiscard]] core::Result<std::string> UnzipMapEdit(std::span<const std::uint8_t> zipped,
                                                     std::uint32_t text_size);

// CE QUE LE SERVEUR A DIT, retenu jusqu'à ce que l'appelant le prenne.
class MapEditInbox {
 public:
  // Lit une trame `0x0F46`, `0x0F47` ou `0x0F48` ; `kUnhandled` pour un autre
  // opcode.
  [[nodiscard]] FrameOutcome Apply(const Frame& frame);

  [[nodiscard]] std::vector<MapEditAnnounce> TakeAnnounces() {
    return std::exchange(announces_, {});
  }
  [[nodiscard]] std::vector<MapEditVerdict> TakeVerdicts() { return std::exchange(verdicts_, {}); }
  [[nodiscard]] std::vector<ServedMapEdit> TakeDocuments() { return std::exchange(documents_, {}); }
  // Une ligne par document qui n'a pas pu s'assembler ou se décompresser.
  [[nodiscard]] std::vector<std::string> TakeRefusals() { return std::exchange(refusals_, {}); }
  // Une liaison perdue ne reprend pas un transfert entamé.
  void Reset();

 private:
  proto::MapEditAssembler assembler_;
  std::vector<MapEditAnnounce> announces_;
  std::vector<MapEditVerdict> verdicts_;
  std::vector<ServedMapEdit> documents_;
  std::vector<std::string> refusals_;
};

// ENREGISTRER LE DOCUMENT de `map` sur le serveur, fondé sur `base_revision` :
// le texte part compressé, en morceaux.
[[nodiscard]] core::Result<void> PublishMapEdit(Connection& link, std::string_view map,
                                                std::uint32_t base_revision, std::string_view text);
// DEMANDER LE DOCUMENT de `map`, la carte où se tient le joueur.
[[nodiscard]] core::Result<void> RequestMapEdit(Connection& link, std::string_view map);

}  // namespace ro::net
