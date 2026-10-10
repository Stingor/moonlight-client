#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/gr2_model.h"

namespace ro::formats::gr2 {

// LE CONTENEUR GRANNY 2 : un en-tête, puis des sections — chacune brute ou
// compressée — et, pour chacune, la liste des mots qui sont des POINTEURS vers
// une autre section. Chargé, le fichier est l'image mémoire d'une arborescence
// d'objets sur trente-deux bits, dont il porte aussi la description des types.
//
// On la reconstitue dans un espace d'adresses à nous : les sections posées bout
// à bout dans un seul tampon, chaque pointeur réécrit en adresse de ce tampon.
// L'adresse 0 reste le pointeur nul.

// La mémoire du fichier chargé. Toute lecture hors du tampon rend zéro et lève
// un drapeau d'échec RÉMANENT, qu'on vérifie une fois à la fin.
class Gr2Memory {
 public:
  Gr2Memory() = default;
  explicit Gr2Memory(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}

  std::uint32_t U32(std::uint32_t address);
  std::int32_t I32(std::uint32_t address) { return static_cast<std::int32_t>(U32(address)); }
  std::uint16_t U16(std::uint32_t address);
  std::uint8_t U8(std::uint32_t address);
  float F32(std::uint32_t address);
  // La chaîne terminée par NUL qui commence à `address` ; vide pour le nul.
  std::string String(std::uint32_t address);
  // Vrai si [address, address + size) est dans le tampon.
  [[nodiscard]] bool Holds(std::uint64_t address, std::uint64_t size) const noexcept;
  [[nodiscard]] std::span<const std::uint8_t> Bytes(std::uint32_t address, std::uint32_t size);

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  void Invalidate() noexcept { ok_ = false; }

 private:
  std::vector<std::uint8_t> bytes_;
  bool ok_ = true;
};

// Le genre d'un membre, tel que le fichier le numérote.
enum class Gr2Kind : std::uint32_t {
  kEnd = 0,
  kInline = 1,
  kReference = 2,
  kReferenceToArray = 3,
  kArrayOfReferences = 4,
  kVariantReference = 5,
  kSwitchable = 6,
  kReferenceToVariantArray = 7,
  kString = 8,
  kTransform = 9,
  kReal32 = 10,
  kInt8 = 11,
  kUInt8 = 12,
  kBinormalInt8 = 13,
  kNormalUInt8 = 14,
  kInt16 = 15,
  kUInt16 = 16,
  kBinormalInt16 = 17,
  kNormalUInt16 = 18,
  kInt32 = 19,
  kUInt32 = 20,
};

struct Gr2Member {
  Gr2Kind kind = Gr2Kind::kEnd;
  std::string name;
  // L'adresse du type désigné : celui d'un membre en ligne, ou des objets
  // qu'une référence vise.
  std::uint32_t type = 0;
  // Le nombre d'éléments d'un membre tableau ; 1 sinon.
  std::uint32_t width = 1;
  // Son décalage dans l'objet, en octets.
  std::uint32_t offset = 0;
};

// Un objet : son type et son adresse. Le nul n'a ni l'un ni l'autre.
struct Gr2Object {
  std::uint32_t type = 0;
  std::uint32_t address = 0;
  [[nodiscard]] bool present() const noexcept { return type != 0 && address != 0; }
};

// Un tableau d'objets d'un type que le tableau porte lui-même.
struct Gr2VariantArray {
  std::uint32_t type = 0;
  std::uint32_t count = 0;
  std::uint32_t address = 0;
};

// Le fichier chargé, et l'accès aux champs de ses objets PAR LEUR NOM, comme
// le fait la bibliothèque d'origine : un champ absent du type prend sa valeur
// par défaut, sans erreur.
class Gr2File {
 public:
  Gr2File(Gr2Memory memory, Gr2Object root) : memory_(std::move(memory)), root_(root) {}

  [[nodiscard]] Gr2Object root() const noexcept { return root_; }
  [[nodiscard]] bool ok() const noexcept { return memory_.ok(); }

  const std::vector<Gr2Member>& Members(std::uint32_t type);
  std::uint32_t SizeOf(std::uint32_t type);
  // Le membre de ce nom, sans égard à la casse.
  std::optional<Gr2Member> MemberNamed(std::uint32_t type, std::string_view name);
  std::optional<Gr2Member> Field(const Gr2Object& object, std::string_view name);

  std::string String(const Gr2Object& object, std::string_view name);
  std::int32_t Int(const Gr2Object& object, std::string_view name, std::int32_t fallback);
  float Real(const Gr2Object& object, std::string_view name, float fallback);
  // Les `out.size()` premiers éléments d'un champ numérique, convertis.
  void Reals(const Gr2Object& object, std::string_view name, std::span<float> out);
  GrannyTransform Transform(const Gr2Object& object, std::string_view name);
  Gr2Object Reference(const Gr2Object& object, std::string_view name);
  // Les éléments d'un tableau d'objets, qu'il soit rangé en ligne ou fait de
  // références ; une référence nulle rend un objet nul, à son rang.
  std::vector<Gr2Object> Array(const Gr2Object& object, std::string_view name);
  Gr2VariantArray VariantArray(const Gr2Object& object, std::string_view name);
  // Les valeurs d'un tableau de nombres — un tableau d'objets à un seul membre
  // numérique —, converties.
  std::vector<float> RealArray(const Gr2Object& object, std::string_view name);
  std::vector<std::int64_t> IntegerArray(const Gr2Object& object, std::string_view name);

  // L'élément `index` d'un membre numérique, BRUT : un entier normalisé rend
  // son entier, pas sa fraction.
  double Scalar(const Gr2Member& member, std::uint32_t address, std::uint32_t index);
  // Le même, ramené à l'unité pour un entier normalisé.
  double Normalized(const Gr2Member& member, std::uint32_t address, std::uint32_t index);

  Gr2Memory& memory() noexcept { return memory_; }

 private:
  std::vector<std::int64_t> Numbers(const Gr2Object& object, std::string_view name,
                                    std::vector<float>* reals);

  Gr2Memory memory_;
  Gr2Object root_;
  std::map<std::uint32_t, std::vector<Gr2Member>> members_;
  std::map<std::uint32_t, std::uint32_t> sizes_;
  std::uint32_t depth_ = 0;
};

// Charge le conteneur : sections décompressées, pointeurs réécrits.
[[nodiscard]] core::Result<Gr2File> LoadGr2File(std::span<const std::uint8_t> bytes);

}  // namespace ro::formats::gr2
