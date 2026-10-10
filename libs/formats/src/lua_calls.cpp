// Voir l'en-tête.

#include "ro/formats/lua_calls.h"

#include <optional>
#include <string>
#include <utility>

#include "lua_lexer.h"
#include "lua_table_internal.h"

namespace ro::formats {
namespace {

using core::ErrorCode;
using lua_internal::ArgA;
using lua_internal::ArgB;
using lua_internal::ArgBx;
using lua_internal::ArgC;
using lua_internal::OpcodeOf;
using lua_internal::Prototype;
using namespace lua_internal::opcodes;

// Un appel qui ne garde aucun résultat : C vaut un de plus que leur nombre.
constexpr std::uint32_t kNoResult = 1;

core::Error Refuse(std::string detail) {
  return core::Error{ErrorCode::kUnsupportedVersion, std::move(detail)};
}

core::Error OutOfRange() {
  return core::Error{ErrorCode::kCorruptData, "registre ou constante hors de portée"};
}

// Un registre de la fonction : une valeur littérale, ou le NOM d'une globale
// chargée pour être appelée.
struct Slot {
  LuaValue value;
  std::optional<std::string> global;
};

// La fonction que le chunk principal pose sous ce nom, ou un refus.
core::Result<const Prototype*> FindFunction(const Prototype& main, std::string_view name) {
  std::vector<std::optional<std::size_t>> closures(main.max_stack);
  for (std::size_t at = 0; at < main.code.size(); ++at) {
    const std::uint32_t word = main.code[at];
    switch (OpcodeOf(word)) {
      case kOpClosure: {
        const std::uint32_t index = ArgBx(word);
        if (index >= main.protos.size() || ArgA(word) >= closures.size()) {
          return std::unexpected(OutOfRange());
        }
        closures[ArgA(word)] = index;
        // Les pseudo-instructions qui suivent décrivent les valeurs externes.
        at += main.protos[index].upvalue_count;
        break;
      }
      case kOpSetGlobal: {
        const std::uint32_t name_index = ArgBx(word);
        if (ArgA(word) >= closures.size() || name_index >= main.constants.size()) {
          return std::unexpected(OutOfRange());
        }
        const LuaValue& global = main.constants[name_index];
        const std::optional<std::size_t>& closure = closures[ArgA(word)];
        if (global.kind == LuaValue::Kind::kString && global.text == name && closure.has_value()) {
          return &main.protos[*closure];
        }
        break;
      }
      case kOpReturn:
        return std::unexpected(
            core::Error{ErrorCode::kNotFound, "le chunk ne pose aucune fonction de ce nom"});
      default:
        return std::unexpected(
            Refuse("chunk principal hors du cadre : il fait autre chose que "
                   "poser des fonctions"));
    }
  }
  return std::unexpected(
      core::Error{ErrorCode::kNotFound, "le chunk ne pose aucune fonction de ce nom"});
}

core::Result<std::vector<LuaCall>> CollectCalls(const Prototype& function) {
  std::vector<Slot> registers(function.max_stack);
  std::vector<LuaCall> calls;
  for (const std::uint32_t word : function.code) {
    const std::uint32_t a = ArgA(word);
    switch (OpcodeOf(word)) {
      case kOpGetGlobal: {
        const std::uint32_t name_index = ArgBx(word);
        if (a >= registers.size() || name_index >= function.constants.size() ||
            function.constants[name_index].kind != LuaValue::Kind::kString) {
          return std::unexpected(OutOfRange());
        }
        registers[a] = Slot{.value = {}, .global = function.constants[name_index].text};
        break;
      }
      case kOpLoadK: {
        const std::uint32_t constant = ArgBx(word);
        if (a >= registers.size() || constant >= function.constants.size()) {
          return std::unexpected(OutOfRange());
        }
        registers[a] = Slot{.value = function.constants[constant], .global = std::nullopt};
        break;
      }
      case kOpLoadBool: {
        // Un C non nul saute l'instruction suivante : c'est un saut.
        if (ArgC(word) != 0) {
          return std::unexpected(Refuse("saut dans une fonction de données"));
        }
        if (a >= registers.size()) {
          return std::unexpected(OutOfRange());
        }
        LuaValue value;
        value.kind = LuaValue::Kind::kBoolean;
        value.boolean = ArgB(word) != 0;
        registers[a] = Slot{.value = std::move(value), .global = std::nullopt};
        break;
      }
      case kOpLoadNil:
        for (std::uint32_t index = a; index <= ArgB(word); ++index) {
          if (index >= registers.size()) {
            return std::unexpected(OutOfRange());
          }
          registers[index] = Slot{};
        }
        break;
      case kOpCall: {
        const std::uint32_t argument_count_plus_one = ArgB(word);
        // B nul : les arguments vont jusqu'au sommet de la pile, donc viennent
        // d'un autre appel. C différent de un : un résultat est gardé.
        if (argument_count_plus_one == 0 || ArgC(word) != kNoResult) {
          return std::unexpected(Refuse("appel dont un argument ou le résultat est calculé"));
        }
        if (a + argument_count_plus_one > registers.size()) {
          return std::unexpected(OutOfRange());
        }
        const Slot& callee = registers[a];
        if (!callee.global.has_value()) {
          return std::unexpected(Refuse("appel d'autre chose qu'une globale nommée"));
        }
        LuaCall call;
        call.name = *callee.global;
        for (std::uint32_t index = 1; index < argument_count_plus_one; ++index) {
          const Slot& argument = registers[a + index];
          if (argument.global.has_value()) {
            return std::unexpected(Refuse("argument tiré d'une globale, non littéral"));
          }
          call.arguments.push_back(argument.value);
        }
        calls.push_back(std::move(call));
        break;
      }
      case kOpReturn:
        return calls;
      default:
        return std::unexpected(
            Refuse("opcode hors du cadre : la fonction fait autre chose qu'appeler"));
    }
  }
  return calls;
}

// ── LA FORME EN CLAIR ───────────────────────────────────────────────────────
//
// Le fichier ne fait que DÉFINIR des fonctions : `function nom() … end`. Le corps
// de chacune n'est qu'une suite d'appels `Nom(littéral, …)`, puis au plus un
// `return littéral, …` juste avant le `end`. Tout le reste se refuse.

constexpr std::uint8_t kCompiledMark = 0x1B;

using lua_internal::Lexer;
using lua_internal::Token;

class SourceReader {
 public:
  SourceReader(std::string_view source, std::string_view wanted)
      : lexer_(source), wanted_(wanted) {}

  [[nodiscard]] core::Result<std::vector<LuaCall>> Run() {
    if (auto first = Advance(); !first) {
      return std::unexpected(first.error());
    }
    while (token_.kind != Token::Kind::kEnd) {
      if (auto defined = Function(); !defined) {
        return std::unexpected(defined.error());
      }
    }
    if (!found_) {
      return std::unexpected(
          core::Error{ErrorCode::kNotFound, "le source ne définit aucune fonction de ce nom"});
    }
    return std::move(calls_);
  }

 private:
  [[nodiscard]] core::Result<void> Advance() {
    auto next = lexer_.Next();
    if (!next) {
      return std::unexpected(next.error());
    }
    token_ = std::move(*next);
    return {};
  }

  [[nodiscard]] bool Is(Token::Kind kind, std::string_view text) const {
    return token_.kind == kind && token_.text == text;
  }

  [[nodiscard]] std::unexpected<core::Error> Outside(std::string_view what) const {
    return std::unexpected(Refuse("ligne " + std::to_string(token_.line) + " : " +
                                  std::string(what) + " hors du cadre d'une fonction de données"));
  }

  // Attend ce lexème, et passe au suivant.
  [[nodiscard]] core::Result<void> Expect(Token::Kind kind, std::string_view text) {
    if (!Is(kind, text)) {
      return Outside("« " + std::string(token_.text) + " »");
    }
    return Advance();
  }

  [[nodiscard]] core::Result<void> Function() {
    if (auto opened = Expect(Token::Kind::kKeyword, "function"); !opened) {
      return opened;
    }
    if (token_.kind != Token::Kind::kName) {
      return Outside("une fonction sans nom");
    }
    const bool wanted = token_.text == wanted_;
    found_ = found_ || wanted;
    if (auto next = Advance(); !next) {
      return next;
    }
    // Une fonction de données ne prend aucun paramètre.
    if (auto opened = Expect(Token::Kind::kSymbol, "("); !opened) {
      return opened;
    }
    if (auto closed = Expect(Token::Kind::kSymbol, ")"); !closed) {
      return closed;
    }
    while (token_.kind == Token::Kind::kName) {
      auto call = Call();
      if (!call) {
        return std::unexpected(call.error());
      }
      if (wanted) {
        calls_.push_back(std::move(*call));
      }
    }
    if (Is(Token::Kind::kKeyword, "return")) {
      if (auto next = Advance(); !next) {
        return next;
      }
      if (!Is(Token::Kind::kKeyword, "end")) {
        if (auto values = Literals(nullptr); !values) {
          return values;
        }
      }
      if (auto skipped = Skip(";"); !skipped) {
        return skipped;
      }
    }
    return Expect(Token::Kind::kKeyword, "end");
  }

  [[nodiscard]] core::Result<LuaCall> Call() {
    LuaCall call;
    call.name = std::string(token_.text);
    if (auto next = Advance(); !next) {
      return std::unexpected(next.error());
    }
    if (auto opened = Expect(Token::Kind::kSymbol, "("); !opened) {
      return std::unexpected(opened.error());
    }
    if (!Is(Token::Kind::kSymbol, ")")) {
      if (auto values = Literals(&call.arguments); !values) {
        return std::unexpected(values.error());
      }
    }
    if (auto closed = Expect(Token::Kind::kSymbol, ")"); !closed) {
      return std::unexpected(closed.error());
    }
    if (auto skipped = Skip(";"); !skipped) {
      return std::unexpected(skipped.error());
    }
    return call;
  }

  // Une liste de littéraux séparés par des virgules.
  [[nodiscard]] core::Result<void> Literals(std::vector<LuaValue>* into) {
    while (true) {
      auto value = Literal();
      if (!value) {
        return std::unexpected(value.error());
      }
      if (into != nullptr) {
        into->push_back(std::move(*value));
      }
      if (!Is(Token::Kind::kSymbol, ",")) {
        return {};
      }
      if (auto next = Advance(); !next) {
        return next;
      }
    }
  }

  [[nodiscard]] core::Result<LuaValue> Literal() {
    LuaValue value;
    bool negative = false;
    if (Is(Token::Kind::kSymbol, "-")) {
      negative = true;
      if (auto next = Advance(); !next) {
        return std::unexpected(next.error());
      }
    }
    if (token_.kind == Token::Kind::kNumber) {
      value.kind = LuaValue::Kind::kNumber;
      value.number = negative ? -token_.number : token_.number;
    } else if (negative) {
      return Outside("un signe moins devant autre chose qu'un nombre");
    } else if (token_.kind == Token::Kind::kString) {
      value.kind = LuaValue::Kind::kString;
      value.text = token_.bytes;
    } else if (Is(Token::Kind::kKeyword, "true") || Is(Token::Kind::kKeyword, "false")) {
      value.kind = LuaValue::Kind::kBoolean;
      value.boolean = token_.text == "true";
    } else if (!Is(Token::Kind::kKeyword, "nil")) {
      return Outside("un argument non littéral");
    }
    if (auto next = Advance(); !next) {
      return std::unexpected(next.error());
    }
    return value;
  }

  // Passe ce symbole facultatif s'il est là.
  [[nodiscard]] core::Result<void> Skip(std::string_view symbol) {
    if (Is(Token::Kind::kSymbol, symbol)) {
      return Advance();
    }
    return {};
  }

  Lexer lexer_;
  std::string_view wanted_;
  Token token_;
  bool found_ = false;
  std::vector<LuaCall> calls_;
};

core::Result<std::vector<LuaCall>> CallsFromSource(std::string_view source,
                                                   std::string_view function_name) {
  SourceReader reader(source, function_name);
  return reader.Run();
}

}  // namespace

core::Result<std::vector<LuaCall>> ReadLuaFunctionCalls(std::span<const std::uint8_t> bytes,
                                                        std::string_view function_name) {
  // Le même discriminant que le chargeur du client : un chunk compilé s'ouvre
  // sur un caractère qu'aucun source n'accepte en tête.
  if (bytes.empty() || bytes.front() != kCompiledMark) {
    return CallsFromSource(
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), function_name);
  }
  const core::Result<Prototype> main = lua_internal::ReadCompiledMain(bytes);
  if (!main) {
    return std::unexpected(main.error());
  }
  const core::Result<const Prototype*> function = FindFunction(*main, function_name);
  if (!function) {
    return std::unexpected(function.error());
  }
  return CollectCalls(**function);
}

}  // namespace ro::formats
