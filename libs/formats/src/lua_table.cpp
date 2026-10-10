#include "ro/formats/lua_table.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>

#include "ro/core/byte_reader.h"

#include "lua_table_internal.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::Error;
using core::ErrorCode;

// L'en-tête d'un chunk Lua 5.1 : douze octets qui décrivent la machine qui l'a
// produit. Nous n'acceptons QUE la sienne — 32 bits, petit-boutiste, nombres
// flottants sur huit octets. Une autre se lirait de travers sans rien signaler.
constexpr std::array<std::uint8_t, 4> kSignature = {0x1B, 'L', 'u', 'a'};
constexpr std::uint8_t kVersion51 = 0x51;
constexpr std::uint8_t kLittleEndian = 1;
constexpr std::uint8_t kIntSize = 4;
constexpr std::uint8_t kSizeTSize = 4;
constexpr std::uint8_t kInstructionSize = 4;
constexpr std::uint8_t kNumberSize = 8;
constexpr std::uint8_t kFloatingNumbers = 0;

// Les types de constante, tels que le fichier les marque.
constexpr std::uint8_t kConstantNil = 0;
constexpr std::uint8_t kConstantBoolean = 1;
constexpr std::uint8_t kConstantNumber = 3;
constexpr std::uint8_t kConstantString = 4;

using lua_internal::ArgA;
using lua_internal::ArgB;
using lua_internal::ArgBx;
using lua_internal::ArgC;
using lua_internal::kConstantFlag;
using lua_internal::OpcodeOf;
using lua_internal::Prototype;
using namespace lua_internal::opcodes;

// Combien de valeurs SETLIST pose par appel. La constante s'appelle
// LFIELDS_PER_FLUSH dans lvm.c, et c'est elle qui donne le rang de la première.
constexpr std::uint32_t kFieldsPerFlush = 50;

Error Refuse(ErrorCode code, std::string detail) { return Error{code, std::move(detail)}; }

double ReadNumber(ByteReader& reader) { return std::bit_cast<double>(reader.U64()); }

// Une chaîne : sa longueur, puis ses octets, le dernier étant le NUL final.
// Une longueur nulle vaut « pas de chaîne » et non « chaîne vide ».
std::string ReadString(ByteReader& reader) {
  const std::uint32_t length = reader.U32();
  if (length == 0) {
    return {};
  }
  const std::span<const std::uint8_t> bytes = reader.Bytes(length);
  if (bytes.empty()) {
    return {};
  }
  // Le NUL final est compté dans la longueur mais ne fait pas partie du texte.
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size() - 1);
}

bool ReadPrototype(ByteReader& reader, Prototype& out, int depth) {
  if (depth > kMaxLuaTableDepth) {
    return false;
  }
  ReadString(reader);  // le fichier source, pour le débogage
  reader.Skip(4);      // première ligne
  reader.Skip(4);      // dernière ligne
  out.upvalue_count = reader.U8();
  reader.Skip(1);  // nombre de paramètres
  reader.Skip(1);  // varargs
  out.max_stack = reader.U8();

  const std::uint32_t instructions = reader.U32();
  if (!reader.ok() || instructions > reader.remaining() / kInstructionSize) {
    return false;
  }
  out.code.reserve(instructions);
  for (std::uint32_t index = 0; index < instructions; ++index) {
    out.code.push_back(reader.U32());
  }

  const std::uint32_t constants = reader.U32();
  if (!reader.ok() || constants > reader.remaining()) {
    return false;
  }
  out.constants.reserve(constants);
  for (std::uint32_t index = 0; index < constants; ++index) {
    LuaValue value;
    switch (reader.U8()) {
      case kConstantNil:
        break;
      case kConstantBoolean:
        value.kind = LuaValue::Kind::kBoolean;
        value.boolean = reader.U8() != 0;
        break;
      case kConstantNumber:
        value.kind = LuaValue::Kind::kNumber;
        value.number = ReadNumber(reader);
        break;
      case kConstantString:
        value.kind = LuaValue::Kind::kString;
        value.text = ReadString(reader);
        break;
      default:
        return false;
    }
    out.constants.push_back(std::move(value));
  }

  const std::uint32_t protos = reader.U32();
  if (!reader.ok() || protos > reader.remaining()) {
    return false;
  }
  out.protos.resize(protos);
  for (std::uint32_t index = 0; index < protos; ++index) {
    if (!ReadPrototype(reader, out.protos[index], depth + 1)) {
      return false;
    }
  }

  // Les tables de débogage : lignes, variables locales, valeurs externes. On les
  // TRAVERSE sans les garder — mais on les traverse, sinon la fonction suivante
  // se lirait à partir du mauvais octet.
  const std::uint32_t lines = reader.U32();
  if (!reader.ok() || lines > reader.remaining() / kIntSize) {
    return false;
  }
  reader.Skip(static_cast<std::size_t>(lines) * kIntSize);

  const std::uint32_t locals = reader.U32();
  if (!reader.ok() || locals > reader.remaining()) {
    return false;
  }
  for (std::uint32_t index = 0; index < locals; ++index) {
    ReadString(reader);
    reader.Skip(8);  // portée : première et dernière instruction
  }

  const std::uint32_t upvalues = reader.U32();
  if (!reader.ok() || upvalues > reader.remaining()) {
    return false;
  }
  for (std::uint32_t index = 0; index < upvalues; ++index) {
    ReadString(reader);
  }
  return reader.ok();
}

}  // namespace

namespace lua_internal {

// Retirer une entrée décale toutes celles qui suivent : l'index se refait. Une
// clé mise à nil est rare dans un fichier de données, et le refaire vaut
// mieux que de le tenir à jour entrée par entrée.
template <typename Key, typename Entries, typename Slots>
void Rebuild(const Entries& entries, Slots& slots) {
  slots.clear();
  for (std::size_t slot = 0; slot < entries.size(); ++slot) {
    slots.emplace(Key(entries[slot].first), slot);
  }
}

void SetByName(LuaTable& table, const std::string& name, LuaValue value) {
  const auto found = table.slot_by_name.find(name);
  if (value.is_nil()) {
    if (found != table.slot_by_name.end()) {
      table.by_name.erase(table.by_name.begin() + static_cast<std::ptrdiff_t>(found->second));
      Rebuild<std::string>(table.by_name, table.slot_by_name);
    }
    return;
  }
  if (found != table.slot_by_name.end()) {
    table.by_name[found->second].second = std::move(value);
    return;
  }
  table.slot_by_name.emplace(name, table.by_name.size());
  table.by_name.emplace_back(name, std::move(value));
}

void SetByIndex(LuaTable& table, double index, LuaValue value) {
  const auto found = table.slot_by_index.find(index);
  if (value.is_nil()) {
    if (found != table.slot_by_index.end()) {
      table.by_index.erase(table.by_index.begin() + static_cast<std::ptrdiff_t>(found->second));
      Rebuild<double>(table.by_index, table.slot_by_index);
    }
    return;
  }
  if (found != table.slot_by_index.end()) {
    table.by_index[found->second].second = std::move(value);
    return;
  }
  table.slot_by_index.emplace(index, table.by_index.size());
  table.by_index.emplace_back(index, std::move(value));
}

bool Assign(LuaTable& table, const LuaValue& key, LuaValue value) {
  if (key.kind == LuaValue::Kind::kString) {
    SetByName(table, key.text, std::move(value));
    return true;
  }
  if (key.kind == LuaValue::Kind::kNumber) {
    SetByIndex(table, key.number, std::move(value));
    return true;
  }
  return false;
}

}  // namespace lua_internal

namespace {

using lua_internal::Assign;
using lua_internal::SetByIndex;
using lua_internal::SetByName;

// Exécute le chunk principal : rien que de la construction de tables.
//
// Il part des globales qu'on lui DONNE : le client enchaîne ses fichiers dans
// un même état, et le second lit ce que le premier a nommé.
class Evaluator {
 public:
  Evaluator(const Prototype& prototype, LuaTable globals, UnnamedKey unnamed)
      : prototype_(prototype), globals_(std::move(globals)), unnamed_(unnamed) {}

  [[nodiscard]] core::Result<LuaTable> Run() {
    registers_.assign(prototype_.max_stack, LuaValue{});
    LuaTable& globals = globals_;

    for (std::size_t at = 0; at < prototype_.code.size(); ++at) {
      const std::uint32_t word = prototype_.code[at];
      switch (OpcodeOf(word)) {
        case kOpMove:
          if (!Copy(ArgA(word), ArgB(word))) {
            return std::unexpected(OutOfRange());
          }
          break;
        case kOpLoadK:
          if (!Store(ArgA(word), Constant(ArgBx(word)))) {
            return std::unexpected(OutOfRange());
          }
          break;
        case kOpLoadBool: {
          LuaValue value;
          value.kind = LuaValue::Kind::kBoolean;
          value.boolean = ArgB(word) != 0;
          if (!Store(ArgA(word), value)) {
            return std::unexpected(OutOfRange());
          }
          // Le C non nul saute l'instruction suivante : c'est la moitié « sinon »
          // d'une comparaison compilée.
          if (ArgC(word) != 0) {
            ++at;
          }
          break;
        }
        case kOpLoadNil:
          for (std::uint32_t index = ArgA(word); index <= ArgB(word); ++index) {
            if (!Store(index, LuaValue{})) {
              return std::unexpected(OutOfRange());
            }
          }
          break;
        case kOpGetGlobal: {
          const LuaValue name = Constant(ArgBx(word));
          if (name.kind != LuaValue::Kind::kString) {
            return std::unexpected(OutOfRange());
          }
          const LuaValue* found = globals.Find(name.text);
          if (!Store(ArgA(word), found != nullptr ? *found : LuaValue{})) {
            return std::unexpected(OutOfRange());
          }
          break;
        }
        case kOpSetGlobal: {
          const LuaValue name = Constant(ArgBx(word));
          if (name.kind != LuaValue::Kind::kString || ArgA(word) >= registers_.size()) {
            return std::unexpected(OutOfRange());
          }
          SetByName(globals, name.text, registers_[ArgA(word)]);
          break;
        }
        case kOpGetTable: {
          if (ArgB(word) >= registers_.size()) {
            return std::unexpected(OutOfRange());
          }
          const LuaValue& source = registers_[ArgB(word)];
          const LuaValue key = Slot(ArgC(word));
          LuaValue value;
          // ⚠ SEULES UNE CHAÎNE ET UN NOMBRE DÉSIGNENT UNE ENTRÉE. `t[nil]` rend
          // nil en Lua ; chercher `key.number`, qui vaut zéro pour nil ou un
          // booléen, rendrait en silence l'entrée [0].
          if (source.kind == LuaValue::Kind::kTable && source.table != nullptr &&
              (key.kind == LuaValue::Kind::kString || key.kind == LuaValue::Kind::kNumber)) {
            const LuaValue* found = key.kind == LuaValue::Kind::kString
                                        ? source.table->Find(key.text)
                                        : source.table->Find(key.number);
            if (found != nullptr) {
              value = *found;
            }
          }
          if (!Store(ArgA(word), value)) {
            return std::unexpected(OutOfRange());
          }
          break;
        }
        case kOpSetTable: {
          if (ArgA(word) >= registers_.size()) {
            return std::unexpected(OutOfRange());
          }
          const LuaValue& target = registers_[ArgA(word)];
          if (target.kind != LuaValue::Kind::kTable || target.table == nullptr) {
            return std::unexpected(OutOfRange());
          }
          const LuaValue key = Slot(ArgB(word));
          // UNE CLÉ NULLE SAUTE SON ENTRÉE si l'appelant l'a demandé, comme en
          // clair : voir `UnnamedKey`.
          if (key.kind == LuaValue::Kind::kNil && unnamed_ == UnnamedKey::kSkipEntry) {
            break;
          }
          if (!Assign(*target.table, key, Slot(ArgC(word)))) {
            return std::unexpected(Refuse(ErrorCode::kCorruptData,
                                          "clé de table d'un type que l'on ne sait pas retrouver"));
          }
          break;
        }
        case kOpNewTable: {
          LuaValue value;
          value.kind = LuaValue::Kind::kTable;
          value.table = std::make_shared<LuaTable>();
          if (!Store(ArgA(word), std::move(value))) {
            return std::unexpected(OutOfRange());
          }
          break;
        }
        case kOpSetList: {
          const std::uint32_t count = ArgB(word);
          std::uint32_t batch = ArgC(word);
          // Un C nul dit que le vrai rang est écrit dans le MOT SUIVANT, qui
          // n'est alors pas une instruction. L'oublier exécuterait un nombre.
          if (batch == 0) {
            ++at;
            if (at >= prototype_.code.size()) {
              return std::unexpected(OutOfRange());
            }
            batch = prototype_.code[at];
          }
          // Un compte nul veut dire « jusqu'au sommet de la pile », ce qui ne se
          // produit qu'après un appel ou un varargs. Ces fichiers n'en ont pas.
          if (count == 0) {
            return std::unexpected(Refuse(ErrorCode::kUnsupportedVersion,
                                          "SETLIST ouvert : le chunk ne se contente pas de "
                                          "construire des tables"));
          }
          if (ArgA(word) >= registers_.size()) {
            return std::unexpected(OutOfRange());
          }
          const LuaValue& target = registers_[ArgA(word)];
          if (target.kind != LuaValue::Kind::kTable || target.table == nullptr) {
            return std::unexpected(OutOfRange());
          }
          for (std::uint32_t index = 1; index <= count; ++index) {
            if (ArgA(word) + index >= registers_.size()) {
              return std::unexpected(OutOfRange());
            }
            const double key = static_cast<double>((batch - 1) * kFieldsPerFlush + index);
            SetByIndex(*target.table, key, registers_[ArgA(word) + index]);
          }
          break;
        }
        case kOpClosure: {
          // La fonction est gardée OPAQUE. Les instructions qui la suivent
          // décrivent ses valeurs externes et ne s'exécutent pas : les sauter est
          // obligatoire, sinon on lirait un MOVE comme une instruction du chunk.
          const std::uint32_t index = ArgBx(word);
          if (index >= prototype_.protos.size()) {
            return std::unexpected(OutOfRange());
          }
          LuaValue value;
          value.kind = LuaValue::Kind::kFunction;
          if (!Store(ArgA(word), std::move(value))) {
            return std::unexpected(OutOfRange());
          }
          at += prototype_.protos[index].upvalue_count;
          break;
        }
        case kOpClose:
          break;
        case kOpReturn:
          return std::move(globals_);
        default:
          return std::unexpected(
              Refuse(ErrorCode::kUnsupportedVersion,
                     "opcode hors du cadre : ce chunk fait autre chose que des tables"));
      }
      if (!registers_valid_) {
        return std::unexpected(OutOfRange());
      }
    }
    return std::move(globals_);
  }

 private:
  static Error OutOfRange() {
    return Refuse(ErrorCode::kCorruptData, "registre, constante ou table hors de portée");
  }

  [[nodiscard]] LuaValue Constant(std::uint32_t index) {
    if (index >= prototype_.constants.size()) {
      registers_valid_ = false;
      return {};
    }
    return prototype_.constants[index];
  }

  // Un argument B ou C : une constante si le bit est posé, un registre sinon.
  [[nodiscard]] LuaValue Slot(std::uint32_t argument) {
    if ((argument & kConstantFlag) != 0) {
      return Constant(argument & ~kConstantFlag);
    }
    if (argument >= registers_.size()) {
      registers_valid_ = false;
      return {};
    }
    return registers_[argument];
  }

  [[nodiscard]] bool Store(std::uint32_t index, LuaValue value) {
    if (index >= registers_.size()) {
      return false;
    }
    registers_[index] = std::move(value);
    return true;
  }

  [[nodiscard]] bool Copy(std::uint32_t destination, std::uint32_t source) {
    if (source >= registers_.size()) {
      return false;
    }
    return Store(destination, registers_[source]);
  }

  const Prototype& prototype_;
  LuaTable globals_;
  UnnamedKey unnamed_ = UnnamedKey::kRefuse;
  std::vector<LuaValue> registers_;
  bool registers_valid_ = true;
};

}  // namespace

std::optional<std::uint32_t> LuaWholeNumber(double value, std::uint32_t most) {
  // ⚠ LA PLAGE SE TESTE AVANT LA CONVERSION, en double : c'est la conversion
  // elle-même qui serait indéfinie. Un NaN échoue les deux comparaisons.
  if (!(value >= 0.0 && value <= static_cast<double>(most)) || std::floor(value) != value) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}

bool AliasGlobal(LuaTable& globals, std::string_view known, std::string_view alias) {
  const LuaValue* value = globals.Find(known);
  if (value == nullptr) {
    return false;
  }
  // ⚠ UNE COPIE DE LA VALEUR, dont la table est un pointeur PARTAGÉ : les deux
  // noms désignent la même table, comme chez le client.
  SetByName(globals, std::string(alias), *value);
  return true;
}

const LuaValue* LuaTable::Find(std::string_view name) const {
  const auto found = slot_by_name.find(name);
  return found != slot_by_name.end() ? &by_name[found->second].second : nullptr;
}

namespace lua_internal {

LuaValue* FindMutable(LuaTable& table, std::string_view name) {
  const auto found = table.slot_by_name.find(name);
  return found != table.slot_by_name.end() ? &table.by_name[found->second].second : nullptr;
}

}  // namespace lua_internal

const LuaValue* LuaTable::Find(double index) const {
  const auto found = slot_by_index.find(index);
  return found != slot_by_index.end() ? &by_index[found->second].second : nullptr;
}

core::Result<LuaTable> ReadLuaChunk(std::span<const std::uint8_t> bytes, LuaTable globals,
                                    UnnamedKey unnamed) {
  core::Result<Prototype> main = lua_internal::ReadCompiledMain(bytes);
  if (!main) {
    return std::unexpected(main.error());
  }
  Evaluator evaluator(*main, std::move(globals), unnamed);
  return evaluator.Run();
}

namespace lua_internal {

core::Result<Prototype> ReadCompiledMain(std::span<const std::uint8_t> bytes) {
  ByteReader reader(bytes);
  for (const std::uint8_t expected : kSignature) {
    if (reader.U8() != expected) {
      return std::unexpected(Refuse(ErrorCode::kBadSignature, "ce n'est pas un chunk Lua"));
    }
  }
  const std::uint8_t version = reader.U8();
  reader.Skip(1);  // format, toujours nul pour le Lua officiel
  const std::uint8_t endianness = reader.U8();
  const std::uint8_t int_size = reader.U8();
  const std::uint8_t size_t_size = reader.U8();
  const std::uint8_t instruction_size = reader.U8();
  const std::uint8_t number_size = reader.U8();
  const std::uint8_t integral = reader.U8();
  if (!reader.ok()) {
    return std::unexpected(Refuse(ErrorCode::kUnexpectedEnd, "en-tête tronqué"));
  }
  if (version != kVersion51) {
    return std::unexpected(
        Refuse(ErrorCode::kUnsupportedVersion, "chunk Lua d'une autre version que 5.1"));
  }
  if (endianness != kLittleEndian || int_size != kIntSize || size_t_size != kSizeTSize ||
      instruction_size != kInstructionSize || number_size != kNumberSize ||
      integral != kFloatingNumbers) {
    return std::unexpected(Refuse(ErrorCode::kUnsupportedVersion,
                                  "chunk compilé pour une autre machine que le client"));
  }

  Prototype main;
  if (!ReadPrototype(reader, main, 0)) {
    return std::unexpected(Refuse(ErrorCode::kCorruptData, "chunk illisible"));
  }
  return main;
}

}  // namespace lua_internal

core::Result<LuaTable> ReadLuaFile(std::span<const std::uint8_t> bytes, LuaTable globals,
                                   UnnamedKey unnamed) {
  // La signature d'un chunk ne peut pas ouvrir un fichier en clair : 0x1B n'est
  // pas un caractère que Lua accepte en tête d'un source. C'est le même
  // discriminant que celui du client, qui charge les deux formes sans le dire.
  const bool compiled = bytes.size() >= kSignature.size() &&
                        std::equal(kSignature.begin(), kSignature.end(), bytes.begin());
  if (compiled) {
    return ReadLuaChunk(bytes, std::move(globals), unnamed);
  }
  return ReadLuaSource(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
                       std::move(globals), unnamed);
}

}  // namespace ro::formats
