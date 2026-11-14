#include "support/lua_builder.h"

#include <bit>

namespace ro::test_support {
namespace {

constexpr std::uint8_t kConstantNil = 0;
constexpr std::uint8_t kConstantBoolean = 1;
constexpr std::uint8_t kConstantNumber = 3;
constexpr std::uint8_t kConstantString = 4;

constexpr std::uint32_t kOpMove = 0;
constexpr std::uint32_t kOpLoadK = 1;
constexpr std::uint32_t kOpLoadBool = 2;
constexpr std::uint32_t kOpLoadNil = 3;
constexpr std::uint32_t kOpGetGlobal = 5;
constexpr std::uint32_t kOpGetTable = 6;
constexpr std::uint32_t kOpSetGlobal = 7;
constexpr std::uint32_t kOpSetTable = 9;
constexpr std::uint32_t kOpNewTable = 10;
constexpr std::uint32_t kOpReturn = 30;
constexpr std::uint32_t kOpSetList = 34;
constexpr std::uint32_t kOpClosure = 36;

void PushU8(std::vector<std::uint8_t>& out, std::uint8_t value) { out.push_back(value); }

void PushU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
  }
}

void PushNumber(std::vector<std::uint8_t>& out, double value) {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  for (int shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((bits >> shift) & 0xFF));
  }
}

// Une chaîne : sa longueur NUL compris, puis ses octets et le NUL.
void PushString(std::vector<std::uint8_t>& out, std::string_view text) {
  PushU32(out, static_cast<std::uint32_t>(text.size() + 1));
  out.insert(out.end(), text.begin(), text.end());
  out.push_back(0);
}

// Une sous-fonction vide, juste de quoi donner une cible à CLOSURE.
void PushEmptyPrototype(std::vector<std::uint8_t>& out, std::uint8_t upvalues) {
  PushU32(out, 0);          // pas de nom de source
  PushU32(out, 0);          // première ligne
  PushU32(out, 0);          // dernière ligne
  PushU8(out, upvalues);    // valeurs externes
  PushU8(out, 0);           // paramètres
  PushU8(out, 0);           // varargs
  PushU8(out, 2);           // pile
  PushU32(out, 1);          // une instruction
  PushU32(out, kOpReturn);  // RETURN 0 1
  PushU32(out, 0);          // constantes
  PushU32(out, 0);          // sous-fonctions
  PushU32(out, 0);          // lignes
  PushU32(out, 0);          // variables locales
  PushU32(out, 0);          // valeurs externes nommées
}

}  // namespace

std::uint32_t LuaChunkBuilder::AddNil() {
  constants_.push_back(Constant{kConstantNil, false, 0.0, {}});
  return static_cast<std::uint32_t>(constants_.size() - 1);
}

std::uint32_t LuaChunkBuilder::AddBoolean(bool value) {
  constants_.push_back(Constant{kConstantBoolean, value, 0.0, {}});
  return static_cast<std::uint32_t>(constants_.size() - 1);
}

std::uint32_t LuaChunkBuilder::AddNumber(double value) {
  constants_.push_back(Constant{kConstantNumber, false, value, {}});
  return static_cast<std::uint32_t>(constants_.size() - 1);
}

std::uint32_t LuaChunkBuilder::AddString(std::string_view value) {
  constants_.push_back(Constant{kConstantString, false, 0.0, std::string(value)});
  return static_cast<std::uint32_t>(constants_.size() - 1);
}

void LuaChunkBuilder::Emit(std::uint32_t opcode, std::uint32_t a, std::uint32_t b,
                           std::uint32_t c) {
  code_.push_back(opcode | (a << 6) | (c << 14) | (b << 23));
}

void LuaChunkBuilder::EmitBx(std::uint32_t opcode, std::uint32_t a, std::uint32_t bx) {
  code_.push_back(opcode | (a << 6) | (bx << 14));
}

void LuaChunkBuilder::Move(std::uint32_t destination, std::uint32_t source) {
  Emit(kOpMove, destination, source, 0);
}

void LuaChunkBuilder::LoadConstant(std::uint32_t destination, std::uint32_t constant) {
  EmitBx(kOpLoadK, destination, constant);
}

void LuaChunkBuilder::LoadBoolean(std::uint32_t destination, bool value, bool skip_next) {
  Emit(kOpLoadBool, destination, value ? 1 : 0, skip_next ? 1 : 0);
}

void LuaChunkBuilder::LoadNil(std::uint32_t first, std::uint32_t last) {
  Emit(kOpLoadNil, first, last, 0);
}

void LuaChunkBuilder::GetGlobal(std::uint32_t destination, std::uint32_t name_constant) {
  EmitBx(kOpGetGlobal, destination, name_constant);
}

void LuaChunkBuilder::SetGlobal(std::uint32_t source, std::uint32_t name_constant) {
  EmitBx(kOpSetGlobal, source, name_constant);
}

void LuaChunkBuilder::GetTable(std::uint32_t destination, std::uint32_t table, std::uint32_t key) {
  Emit(kOpGetTable, destination, table, key);
}

void LuaChunkBuilder::SetTable(std::uint32_t table, std::uint32_t key, std::uint32_t value) {
  Emit(kOpSetTable, table, key, value);
}

void LuaChunkBuilder::NewTable(std::uint32_t destination) { Emit(kOpNewTable, destination, 0, 0); }

void LuaChunkBuilder::SetList(std::uint32_t table, std::uint32_t count, std::uint32_t batch) {
  Emit(kOpSetList, table, count, batch);
}

void LuaChunkBuilder::SetListExtended(std::uint32_t table, std::uint32_t count,
                                      std::uint32_t batch) {
  Emit(kOpSetList, table, count, 0);
  code_.push_back(batch);
}

void LuaChunkBuilder::Closure(std::uint32_t destination, std::uint32_t proto,
                              std::uint32_t upvalues) {
  EmitBx(kOpClosure, destination, proto);
  // Les pseudo-instructions qui décrivent les valeurs externes. Le lecteur doit
  // les SAUTER ; ici on pose des MOVE, qui écraseraient un registre s'ils
  // étaient exécutés.
  for (std::uint32_t index = 0; index < upvalues; ++index) {
    Emit(kOpMove, 0, 0, 0);
  }
}

void LuaChunkBuilder::Return() { Emit(kOpReturn, 0, 1, 0); }

void LuaChunkBuilder::RawInstruction(std::uint32_t word) { code_.push_back(word); }

std::vector<std::uint8_t> LuaChunkBuilder::Header(std::uint8_t version, std::uint8_t endianness,
                                                  std::uint8_t number_size) {
  std::vector<std::uint8_t> out;
  out.push_back(0x1B);
  out.push_back('L');
  out.push_back('u');
  out.push_back('a');
  out.push_back(version);
  out.push_back(0);  // format officiel
  out.push_back(endianness);
  out.push_back(4);  // int
  out.push_back(4);  // size_t
  out.push_back(4);  // instruction
  out.push_back(number_size);
  out.push_back(0);  // nombres flottants
  return out;
}

std::vector<std::uint8_t> LuaChunkBuilder::Build() const {
  std::vector<std::uint8_t> out = Header();

  PushString(out, "@essai.lua");
  PushU32(out, 0);  // première ligne
  PushU32(out, 0);  // dernière ligne
  PushU8(out, 0);   // valeurs externes
  PushU8(out, 0);   // paramètres
  PushU8(out, 0);   // varargs
  PushU8(out, max_stack_);

  PushU32(out, static_cast<std::uint32_t>(code_.size()));
  for (const std::uint32_t word : code_) {
    PushU32(out, word);
  }

  PushU32(out, static_cast<std::uint32_t>(constants_.size()));
  for (const Constant& constant : constants_) {
    PushU8(out, constant.kind);
    switch (constant.kind) {
      case kConstantBoolean:
        PushU8(out, constant.boolean ? 1 : 0);
        break;
      case kConstantNumber:
        PushNumber(out, constant.number);
        break;
      case kConstantString:
        PushString(out, constant.text);
        break;
      default:
        break;
    }
  }

  PushU32(out, proto_count_);
  for (std::uint8_t index = 0; index < proto_count_; ++index) {
    PushEmptyPrototype(out, proto_upvalues_);
  }

  PushU32(out, 0);  // lignes
  PushU32(out, 0);  // variables locales
  PushU32(out, 0);  // valeurs externes nommées
  return out;
}

}  // namespace ro::test_support
