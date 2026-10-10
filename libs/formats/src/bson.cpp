#include "ro/formats/bson.h"

#include <bit>
#include <cstddef>
#include <format>
#include <memory>
#include <utility>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

// Les six types d'élément que les trois fichiers de l'installation emploient.
// Tout autre est refusé, y compris ceux que BSON définit : nous n'avons aucun
// octet réel sur quoi les éprouver.
enum class ElementType : std::uint8_t {
  kDouble = 0x01,
  kString = 0x02,
  kDocument = 0x03,
  kArray = 0x04,
  kBoolean = 0x08,
  kInt32 = 0x10,
};

// Un document annonce sa taille sur quatre octets et finit par un octet nul ;
// les deux sont COMPTÉS dans la taille annoncée. Un document vide en fait donc
// cinq, et rien de plus petit n'est un document.
constexpr std::size_t kDocumentSizeField = 4;
constexpr std::size_t kDocumentTerminator = 1;
constexpr std::size_t kSmallestDocument = kDocumentSizeField + kDocumentTerminator;

// Le zéro qui termine un document, et qui ne peut donc pas être un type.
constexpr std::uint8_t kEndOfDocument = 0;

core::Result<BsonDocument> ReadDocument(core::ByteReader& reader, std::size_t depth);

core::Result<BsonValue> ReadValue(core::ByteReader& reader, ElementType type, std::size_t depth) {
  BsonValue value;
  switch (type) {
    case ElementType::kDouble:
      value.kind = BsonValue::Kind::kNumber;
      value.number = std::bit_cast<double>(reader.U64());
      break;
    case ElementType::kString: {
      // La longueur annoncée COMPTE le zéro final : une chaîne vide en annonce
      // un. Zéro ou moins veut dire que la lecture a dérapé.
      const std::int32_t declared = reader.I32();
      if (!reader.ok() || declared < 1) {
        return core::Fail(core::ErrorCode::kCorruptData,
                          std::format("longueur de chaîne {}", declared));
      }
      const std::span<const std::uint8_t> raw = reader.Bytes(static_cast<std::size_t>(declared));
      if (!reader.ok() || raw.back() != 0) {
        return core::Fail(core::ErrorCode::kCorruptData, "chaîne non terminée par un zéro");
      }
      value.kind = BsonValue::Kind::kText;
      value.text.assign(reinterpret_cast<const char*>(raw.data()), raw.size() - 1);
      break;
    }
    case ElementType::kDocument:
    case ElementType::kArray: {
      core::Result<BsonDocument> nested = ReadDocument(reader, depth + 1);
      if (!nested) {
        return core::Fail(nested.error().code, std::move(nested.error().detail));
      }
      value.kind =
          type == ElementType::kArray ? BsonValue::Kind::kArray : BsonValue::Kind::kDocument;
      value.document = std::make_shared<BsonDocument>(std::move(*nested));
      break;
    }
    case ElementType::kBoolean: {
      const std::uint8_t raw = reader.U8();
      // Ni zéro ni un ne se lit « à peu près vrai » : c'est le signe que
      // l'octet lu n'est pas celui qu'on croit.
      if (raw > 1) {
        return core::Fail(core::ErrorCode::kCorruptData, std::format("booléen {}", raw));
      }
      value.kind = BsonValue::Kind::kBoolean;
      value.boolean = raw != 0;
      break;
    }
    case ElementType::kInt32:
      value.kind = BsonValue::Kind::kInteger;
      value.integer = reader.I32();
      break;
  }
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "élément tronqué");
  }
  return value;
}

core::Result<BsonDocument> ReadDocument(core::ByteReader& reader, std::size_t depth) {
  if (depth > kMaxBsonDepth) {
    return core::Fail(core::ErrorCode::kCorruptData,
                      std::format("documents imbriqués au-delà de {}", kMaxBsonDepth));
  }
  const std::size_t begin = reader.offset();
  const std::int32_t declared = reader.I32();
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "taille de document absente");
  }
  if (declared < static_cast<std::int32_t>(kSmallestDocument)) {
    return core::Fail(core::ErrorCode::kCorruptData,
                      std::format("taille de document {}", declared));
  }
  const std::size_t end = begin + static_cast<std::size_t>(declared);
  if (end > reader.size()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd,
                      std::format("document de {} octets à {}", declared, begin));
  }

  BsonDocument document;
  // On s'arrête un octet avant la fin : ce dernier est le zéro de terminaison,
  // et le lire comme un type ferait boucler sur un document vide.
  while (reader.ok() && reader.offset() + kDocumentTerminator < end) {
    const std::uint8_t type = reader.U8();
    if (type == kEndOfDocument) {
      return core::Fail(core::ErrorCode::kCorruptData,
                        std::format("document terminé avant les {} octets annoncés", declared));
    }
    const std::string_view name = reader.CString();
    if (!reader.ok()) {
      return core::Fail(core::ErrorCode::kUnexpectedEnd, "nom de champ tronqué");
    }
    switch (static_cast<ElementType>(type)) {
      case ElementType::kDouble:
      case ElementType::kString:
      case ElementType::kDocument:
      case ElementType::kArray:
      case ElementType::kBoolean:
      case ElementType::kInt32:
        break;
      default:
        return core::Fail(core::ErrorCode::kUnsupportedVersion,
                          std::format("type d'élément 0x{:02X}, champ {}", type, name));
    }
    core::Result<BsonValue> value = ReadValue(reader, static_cast<ElementType>(type), depth);
    if (!value) {
      return core::Fail(value.error().code,
                        std::format("champ {} : {}", name, value.error().detail));
    }
    document.fields.emplace_back(std::string(name), std::move(*value));
  }

  // Le zéro final doit tomber EXACTEMENT là où la taille annoncée le place :
  // c'est ce qui dénonce un champ qui a débordé de son document.
  const std::uint8_t terminator = reader.U8();
  if (!reader.ok() || terminator != kEndOfDocument || reader.offset() != end) {
    return core::Fail(core::ErrorCode::kCorruptData,
                      std::format("document de {} octets mal terminé", declared));
  }
  return document;
}

}  // namespace

const BsonValue* BsonDocument::Find(std::string_view name) const {
  for (const std::pair<std::string, BsonValue>& field : fields) {
    if (field.first == name) {
      return &field.second;
    }
  }
  return nullptr;
}

core::Result<BsonDocument> ParseBson(std::span<const std::uint8_t> bytes) {
  core::ByteReader reader(bytes);
  core::Result<BsonDocument> document = ReadDocument(reader, 0);
  if (!document) {
    return document;
  }
  // La racine occupe tout le fichier : des octets de rab veulent dire qu'on ne
  // lit pas le fichier que l'on croit.
  if (reader.offset() != bytes.size()) {
    return core::Fail(
        core::ErrorCode::kCorruptData,
        std::format("{} octets après le document racine", bytes.size() - reader.offset()));
  }
  return document;
}

}  // namespace ro::formats
