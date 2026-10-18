#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::proto {

// ── LES DOCUMENTS D'ÉDITION DE CARTE, EN MORCEAUX (ADR 0124) ────────────────
//
// Un document voyage compressé, en morceaux du flux compressé, dans l'ordre :
// `CZ 0x0F45` du client au serveur, `ZC 0x0F47` du serveur au client. Chaque
// morceau dit le total compressé, la taille du texte et son décalage ; le
// décalage 0 ouvre un transfert, le morceau qui atteint le total le ferme.
//
//   CZ 0f45 <len>.W <carte>.16B <révision de base>.L <compressé>.L <texte>.L
//           <décalage>.L puis les octets du morceau
//   ZC 0f47 <len>.W <carte>.16B <révision>.L <compressé>.L <texte>.L
//           <décalage>.L <auteur>.24B puis les octets du morceau
//
// Ici, rien que des octets : la compression vit dans `ro::net`.

// UN MORCEAU ENVOYÉ AU SERVEUR : son tampon de réception d'une session vaut
// 2 Kio et ne grandit pas ; un paquet plus long ferme la session.
inline constexpr std::size_t kMapEditPutChunkBytes = 1024;
// UN MORCEAU REÇU : la longueur d'un paquet du serveur est un entier signé de
// 16 bits.
inline constexpr std::size_t kMapEditDocChunkBytes = 16384;
// LES PLAFONDS D'UN DOCUMENT, compressé puis en texte.
inline constexpr std::size_t kMapEditMostZippedBytes = 256 * 1024;
inline constexpr std::size_t kMapEditMostTextBytes = 4 * 1024 * 1024;
// Le champ du nom de la carte, zéro terminal compris.
inline constexpr std::size_t kMapEditNameBytes = 16;

// Un morceau lu sur le fil. `revision` : la révision de base d'un envoi, celle
// du document d'un morceau servi. `author` : vide pour un envoi.
struct MapEditChunk {
  std::string map;
  std::uint32_t revision = 0;
  std::uint32_t zipped_size = 0;
  std::uint32_t text_size = 0;
  std::uint32_t offset = 0;
  std::string author;
  std::vector<std::uint8_t> bytes;

  friend bool operator==(const MapEditChunk&, const MapEditChunk&) = default;
};

// UN NOM DE CARTE QUI PEUT NOMMER UN FICHIER : non vide, tenant dans le champ,
// fait seulement de lettres et chiffres ASCII, de `_`, `-` et `@`.
[[nodiscard]] bool IsMapEditName(std::string_view map);

// Les trames d'un envoi, ou aucune quand le nom de la carte n'en est pas un,
// que le flux est vide, ou qu'il dépasse un plafond.
[[nodiscard]] std::vector<std::vector<std::uint8_t>> EncodeMapEditPut(
    std::string_view map, std::uint32_t base_revision, std::uint32_t text_size,
    std::span<const std::uint8_t> zipped);
// Les trames d'un document servi, aux mêmes conditions. Le client ne l'émet
// jamais : il sert aux essais, qui jouent le serveur.
[[nodiscard]] std::vector<std::vector<std::uint8_t>> EncodeMapEditDoc(
    std::string_view map, std::uint32_t revision, std::string_view author, std::uint32_t text_size,
    std::span<const std::uint8_t> zipped);

// Une trame lue, ou rien quand elle est trop courte, porte un autre opcode, ou
// une longueur qui n'est pas la sienne.
[[nodiscard]] std::optional<MapEditChunk> DecodeMapEditPut(std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<MapEditChunk> DecodeMapEditDoc(std::span<const std::uint8_t> frame);

// Un document entier, encore compressé.
struct AssembledMapEdit {
  std::string map;
  std::uint32_t revision = 0;
  std::uint32_t text_size = 0;
  std::string author;
  std::vector<std::uint8_t> zipped;
};

// LE RÉASSEMBLAGE d'un document reçu en morceaux. Un seul transfert à la fois :
// le serveur envoie les morceaux d'un document d'une traite, et le flux garde
// leur ordre.
class MapEditAssembler {
 public:
  enum class Step {
    // Le morceau est retenu ; le document n'est pas complet.
    kPending,
    // Le document est complet : `Take` le rend.
    kComplete,
    // Le morceau ne suit pas, ou dépasse un plafond : le transfert est jeté, et
    // `refusal` dit pourquoi.
    kRefused,
  };

  Step Feed(MapEditChunk chunk);
  // Le document complet, après un `kComplete`.
  [[nodiscard]] AssembledMapEdit Take();
  [[nodiscard]] const std::string& refusal() const noexcept { return refusal_; }
  // Oublie le transfert en cours : une liaison perdue ne le reprendra pas.
  void Reset();

 private:
  Step Refuse(std::string why);

  bool receiving_ = false;
  AssembledMapEdit building_;
  std::uint32_t zipped_size_ = 0;
  std::optional<AssembledMapEdit> complete_;
  std::string refusal_;
};

}  // namespace ro::proto
