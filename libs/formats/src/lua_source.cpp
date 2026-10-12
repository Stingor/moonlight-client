#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ro/formats/lua_table.h"

#include "lua_lexer.h"
#include "lua_table_internal.h"

namespace ro::formats {
namespace {

using core::Error;
using core::ErrorCode;
using lua_internal::Assign;
using lua_internal::Contains;
using lua_internal::Lexer;
using lua_internal::SetByIndex;
using lua_internal::SetByName;
using lua_internal::Token;

// Un nom d'appel ne descend jamais plus bas que `Table.Methode`. Le plafond
// n'est pas là pour ces fichiers mais pour un fichier abîmé, dont la suite de
// points ferait grandir le nom sans fin.
constexpr int kDeepestCallName = 8;

// Les mots qui OUVRENT un bloc et ceux qui le FERMENT, pour sauter une fonction
// sans la comprendre. `while` et `for` n'ouvrent rien par eux-mêmes : leur bloc
// commence au `do` qui les suit, déjà compté ; `then`, `else` et `elseif` ne
// changent pas la profondeur d'un `if`.
constexpr std::array<std::string_view, 4> kBlockOpeners = {"function", "if", "do", "repeat"};
constexpr std::array<std::string_view, 2> kBlockClosers = {"end", "until"};

// Les opérateurs binaires. Aucun n'est évalué : un fichier qui calcule fait
// autre chose que des tables, et se refuse au lieu de se lire à moitié.
constexpr std::array<std::string_view, 13> kBinaryOperators = {
    "..", "+", "-", "*", "/", "%", "^", "==", "~=", "<=", ">=", "<", ">"};

// Évalue les instructions, dans les globales données.
class Parser {
 public:
  Parser(std::string_view source, LuaTable globals)
      : lexer_(source), globals_(std::move(globals)) {}

  [[nodiscard]] core::Result<LuaTable> Run() {
    if (!Advance()) {
      return std::unexpected(error_);
    }
    while (current_.kind != Token::Kind::kEnd) {
      if (!Statement()) {
        return std::unexpected(error_);
      }
    }
    return std::move(globals_);
  }

 private:
  [[nodiscard]] bool Advance() {
    auto token = lexer_.Next();
    if (!token) {
      error_ = token.error();
      return false;
    }
    current_ = std::move(*token);
    return true;
  }

  [[nodiscard]] bool Fail(ErrorCode code, std::string what) {
    return FailAt(current_.line, code, std::move(what));
  }
  [[nodiscard]] bool FailAt(int line, ErrorCode code, std::string what) {
    error_ = Error{code, "ligne " + std::to_string(line) + " : " + what};
    return false;
  }
  [[nodiscard]] bool OutOfScope(std::string what) {
    return Fail(ErrorCode::kUnsupportedVersion,
                what + " : ce fichier fait autre chose que des tables");
  }

  [[nodiscard]] bool IsSymbol(std::string_view symbol) const noexcept {
    return current_.kind == Token::Kind::kSymbol && current_.text == symbol;
  }
  [[nodiscard]] bool IsKeyword(std::string_view keyword) const noexcept {
    return current_.kind == Token::Kind::kKeyword && current_.text == keyword;
  }
  [[nodiscard]] bool Expect(std::string_view symbol) {
    if (!IsSymbol(symbol)) {
      return Fail(ErrorCode::kCorruptData, "« " + std::string(symbol) + " » attendu");
    }
    return Advance();
  }

  // `Nom = expression`, `function Nom(...) ... end`, ou un point-virgule.
  [[nodiscard]] bool Statement() {
    if (IsSymbol(";")) {
      return Advance();
    }
    if (IsKeyword("function")) {
      if (!Advance()) {
        return false;
      }
      if (current_.kind != Token::Kind::kName) {
        return OutOfScope("fonction sans nom");
      }
      const std::string name(current_.text);
      if (!Advance()) {
        return false;
      }
      if (IsSymbol(".") || IsSymbol(":")) {
        return OutOfScope("fonction rangée dans une table");
      }
      if (!SkipFunctionBody()) {
        return false;
      }
      LuaValue function;
      function.kind = LuaValue::Kind::kFunction;
      SetByName(globals_, name, std::move(function));
      return true;
    }
    if (current_.kind == Token::Kind::kName) {
      const std::string name(current_.text);
      if (!Advance()) {
        return false;
      }
      if (IsSymbol("[")) {
        return IndexedAssignment(name);
      }
      if (IsSymbol("(") || IsSymbol(".") || IsSymbol(":")) {
        return RecordCall(name);
      }
      if (!IsSymbol("=")) {
        return OutOfScope("instruction hors du cadre");
      }
      if (!Advance()) {
        return false;
      }
      LuaValue value;
      if (!Expression(value, 0)) {
        return false;
      }
      SetByName(globals_, name, std::move(value));
      return true;
    }
    return OutOfScope("instruction hors du cadre");
  }

  // `Table[clé] = valeur` : une affectation qui écrit DANS une table déjà posée.
  //
  // 🔴 CE N'EST PAS UN CONSTRUCTEUR, ET C'EST POURQUOI IL FAUT L'ÉCRIRE À PART.
  // `stateiconinfo` pose `StateIconList = {}` puis HUIT CENTS affectations de
  // cette forme ; sans elles le fichier se refuse entier, à sa première ligne
  // utile, et huit cents noms de statuts avec lui.
  //
  // ⚠ LA TABLE DOIT EXISTER. En Lua, écrire dans une table nulle est une erreur
  // d'exécution ; ici, la créer au vol masquerait un fichier lu dans le mauvais
  // ordre — c'est exactement ce qu'on veut voir échouer.
  [[nodiscard]] bool IndexedAssignment(const std::string& name) {
    const int key_line = current_.line;
    LuaValue key;
    if (!Advance() || !Expression(key, 0) || !Expect("]") || !Expect("=")) {
      return false;
    }
    LuaValue value;
    if (!Expression(value, 0)) {
      return false;
    }
    if (key.is_nil()) {
      // ⚠ MÊME RÈGLE QUE DANS UN CONSTRUCTEUR : une clé que rien n'a nommée
      // refuse le fichier.
      return FailAt(key_line, ErrorCode::kCorruptData,
                    "clé nulle : un identifiant que rien n'a nommé");
    }
    LuaValue* target = lua_internal::FindMutable(globals_, name);
    if (target == nullptr || target->kind != LuaValue::Kind::kTable || target->table == nullptr) {
      return FailAt(key_line, ErrorCode::kCorruptData,
                    "« " + name + " » n'est pas une table : rien ne l'a nommée avant");
    }
    if (!Assign(*target->table, key, std::move(value))) {
      return FailAt(key_line, ErrorCode::kCorruptData,
                    "clé d'un type que l'on ne sait pas retrouver");
    }
    return true;
  }

  // GARDE un appel de premier niveau, au lieu de l'exécuter ou de le jeter.
  //
  // ⚠ CERTAINS DE CES APPELS SONT DE LA DONNÉE : `skilltreeview` nomme les
  // onglets du grimoire par des appels, et sa grille vit dans le même fichier.
  // Le refuser perdrait les deux ; le sauter perdrait les noms en silence.
  //
  // Un argument se lit comme n'importe quelle valeur — constante, table, ou
  // globale déjà nommée. Un argument CALCULÉ refuse le fichier, comme partout
  // ailleurs : on ne devine pas ce qu'on ne sait pas évaluer.
  [[nodiscard]] bool RecordCall(std::string name) {
    for (int depth = 0; IsSymbol(".") || IsSymbol(":"); ++depth) {
      if (depth >= kDeepestCallName) {
        return OutOfScope("nom d'appel sans fin");
      }
      name += '.';
      if (!Advance()) {
        return false;
      }
      if (current_.kind != Token::Kind::kName) {
        return OutOfScope("appel sans nom de méthode");
      }
      name += current_.text;
      if (!Advance()) {
        return false;
      }
    }
    if (!IsSymbol("(")) {
      return OutOfScope("instruction hors du cadre");
    }
    if (!Advance()) {
      return false;
    }
    LuaCall call;
    call.name = std::move(name);
    while (!IsSymbol(")")) {
      // Un argument peut être enveloppé de parenthèses — `setmetatable(T, (T))`
      // s'écrit ainsi dans les listes d'héritage. Elles n'ajoutent rien à la
      // valeur, mais il faut les refermer.
      int wrapped = 0;
      while (IsSymbol("(")) {
        if (wrapped >= kDeepestCallName) {
          return OutOfScope("parenthèses sans fin");
        }
        ++wrapped;
        if (!Advance()) {
          return false;
        }
      }
      LuaValue argument;
      if (!Expression(argument, 0)) {
        return false;
      }
      for (; wrapped > 0; --wrapped) {
        if (!Expect(")")) {
          return false;
        }
      }
      call.arguments.push_back(std::move(argument));
      if (IsSymbol(",")) {
        if (!Advance()) {
          return false;
        }
        continue;
      }
      if (!IsSymbol(")")) {
        // Fin de fichier comprise : l'appel n'est pas hors cadre, il est ABÎMÉ,
        // et les deux ne se rattrapent pas de la même façon.
        return Fail(ErrorCode::kCorruptData, "appel sans parenthèse fermante");
      }
    }
    globals_.calls.push_back(std::move(call));
    return Advance();
  }

  // Saute tout jusqu'au `end` qui ferme la fonction dont le mot-clé vient d'être
  // lu. Les chaînes et les commentaires sont déjà hors jeu : le lexeur les a
  // absorbés, et un `end` qu'ils contiendraient ne compte pas.
  [[nodiscard]] bool SkipFunctionBody() {
    int depth = 1;
    while (depth > 0) {
      if (current_.kind == Token::Kind::kEnd) {
        return Fail(ErrorCode::kCorruptData, "fonction sans end");
      }
      if (current_.kind == Token::Kind::kKeyword) {
        if (Contains(kBlockOpeners, current_.text)) {
          ++depth;
        } else if (Contains(kBlockClosers, current_.text)) {
          --depth;
        }
      }
      if (!Advance()) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool Expression(LuaValue& out, int depth) {
    out = LuaValue{};
    // ⚠ LE PLAFOND SE TESTE ICI, et non dans le seul constructeur de table :
    // `a[a[a[…` imbrique des expressions sans ouvrir aucune accolade, et chaque
    // niveau coûte un cadre de pile. Un fichier de données ne doit pas pouvoir
    // la faire déborder.
    if (depth > kMaxLuaTableDepth) {
      return Fail(ErrorCode::kCorruptData, "expressions imbriquées au-delà du plafond");
    }
    switch (current_.kind) {
      case Token::Kind::kEnd:
        return Fail(ErrorCode::kCorruptData, "expression attendue, fin du fichier");
      case Token::Kind::kNumber:
        out.kind = LuaValue::Kind::kNumber;
        out.number = current_.number;
        if (!Advance()) {
          return false;
        }
        break;
      case Token::Kind::kString:
        out.kind = LuaValue::Kind::kString;
        out.text = std::move(current_.bytes);
        if (!Advance()) {
          return false;
        }
        break;
      case Token::Kind::kKeyword:
        if (IsKeyword("nil")) {
          if (!Advance()) {
            return false;
          }
        } else if (IsKeyword("true") || IsKeyword("false")) {
          out.kind = LuaValue::Kind::kBoolean;
          out.boolean = IsKeyword("true");
          if (!Advance()) {
            return false;
          }
        } else if (IsKeyword("function")) {
          if (!Advance() || !SkipFunctionBody()) {
            return false;
          }
          out.kind = LuaValue::Kind::kFunction;
        } else {
          return OutOfScope("expression hors du cadre");
        }
        break;
      case Token::Kind::kSymbol:
        if (IsSymbol("-")) {
          // Le seul calcul admis : un nombre négatif, que Lua écrit ainsi.
          if (!Advance()) {
            return false;
          }
          if (current_.kind != Token::Kind::kNumber) {
            return OutOfScope("calcul");
          }
          out.kind = LuaValue::Kind::kNumber;
          out.number = -current_.number;
          if (!Advance()) {
            return false;
          }
        } else if (IsSymbol("{")) {
          if (!TableConstructor(out, depth + 1)) {
            return false;
          }
        } else {
          return OutOfScope("expression hors du cadre");
        }
        break;
      case Token::Kind::kName: {
        const std::string name(current_.text);
        if (!Advance() || !ReferenceFrom(name, out, depth)) {
          return false;
        }
        break;
      }
    }
    if (current_.kind == Token::Kind::kSymbol && Contains(kBinaryOperators, current_.text)) {
      return OutOfScope("calcul");
    }
    if (IsKeyword("and") || IsKeyword("or")) {
      return OutOfScope("calcul");
    }
    return true;
  }

  // `Nom`, `Nom.champ`, `Nom[clé]`, en chaîne : une lecture dans les globales
  // déjà posées. Indexer ce qui n'est pas une table est une erreur, comme en
  // Lua — c'est ainsi que jobName sans NPCIdentity se refuse.
  [[nodiscard]] bool ReferenceFrom(const std::string& name, LuaValue& out, int depth) {
    const int line = current_.line;
    const LuaValue* found = globals_.Find(name);
    LuaValue value = found != nullptr ? *found : LuaValue{};
    std::string described = name;
    while (true) {
      LuaValue key;
      if (IsSymbol(".")) {
        if (!Advance()) {
          return false;
        }
        if (current_.kind != Token::Kind::kName) {
          return Fail(ErrorCode::kCorruptData, "nom attendu après le point");
        }
        key.kind = LuaValue::Kind::kString;
        key.text = std::string(current_.text);
        if (!Advance()) {
          return false;
        }
      } else if (IsSymbol("[")) {
        if (!Advance() || !Expression(key, depth + 1) || !Expect("]")) {
          return false;
        }
      } else {
        break;
      }
      if (value.kind != LuaValue::Kind::kTable || value.table == nullptr) {
        return FailAt(line, ErrorCode::kCorruptData,
                      "« " + described + " » n'est pas une table : rien ne l'a nommée avant");
      }
      const LuaValue* member = nullptr;
      if (key.kind == LuaValue::Kind::kString) {
        member = value.table->Find(key.text);
        described += "." + key.text;
      } else if (key.kind == LuaValue::Kind::kNumber) {
        member = value.table->Find(key.number);
        described += "[" + std::to_string(key.number) + "]";
      } else {
        return Fail(ErrorCode::kCorruptData,
                    "clé de table d'un type que l'on ne sait pas retrouver");
      }
      value = member != nullptr ? *member : LuaValue{};
    }
    out = std::move(value);
    return true;
  }

  // `{ [clé] = valeur, nom = valeur, valeur, ... }`, séparés par des virgules
  // ou des points-virgules. Les valeurs sans clé se numérotent à partir de UN,
  // et une valeur nulle consomme son rang sans rien poser, comme en Lua.
  [[nodiscard]] bool TableConstructor(LuaValue& out, int depth) {
    if (depth > kMaxLuaTableDepth) {
      return Fail(ErrorCode::kCorruptData, "tables imbriquées au-delà du plafond");
    }
    if (!Expect("{")) {
      return false;
    }
    auto table = std::make_shared<LuaTable>();
    double next_index = 1.0;
    while (!IsSymbol("}")) {
      if (current_.kind == Token::Kind::kEnd) {
        return Fail(ErrorCode::kCorruptData, "table sans accolade fermante");
      }
      if (IsSymbol("[")) {
        const int key_line = current_.line;
        LuaValue key;
        LuaValue value;
        if (!Advance() || !Expression(key, depth) || !Expect("]") || !Expect("=") ||
            !Expression(value, depth)) {
          return false;
        }
        if (key.is_nil()) {
          return FailAt(key_line, ErrorCode::kCorruptData,
                        "clé de table nulle : un identifiant que rien n'a nommé");
        }
        if (!Assign(*table, key, std::move(value))) {
          return Fail(ErrorCode::kCorruptData,
                      "clé de table d'un type que l'on ne sait pas retrouver");
        }
      } else if (current_.kind == Token::Kind::kName) {
        const std::string name(current_.text);
        if (!Advance()) {
          return false;
        }
        LuaValue value;
        if (IsSymbol("=")) {
          if (!Advance() || !Expression(value, depth)) {
            return false;
          }
          SetByName(*table, name, std::move(value));
        } else {
          if (!ReferenceFrom(name, value, depth)) {
            return false;
          }
          Append(*table, next_index, std::move(value));
        }
      } else {
        LuaValue value;
        if (!Expression(value, depth)) {
          return false;
        }
        Append(*table, next_index, std::move(value));
      }
      if (IsSymbol(",") || IsSymbol(";")) {
        if (!Advance()) {
          return false;
        }
      } else if (!IsSymbol("}")) {
        return Fail(ErrorCode::kCorruptData, "virgule ou accolade fermante attendue");
      }
    }
    if (!Advance()) {
      return false;
    }
    out.kind = LuaValue::Kind::kTable;
    out.table = std::move(table);
    return true;
  }

  static void Append(LuaTable& table, double& next_index, LuaValue value) {
    if (!value.is_nil()) {
      SetByIndex(table, next_index, std::move(value));
    }
    next_index += 1.0;
  }

  Lexer lexer_;
  Token current_;
  LuaTable globals_;
  Error error_;
};

}  // namespace

core::Result<LuaTable> ReadLuaSource(std::string_view source, LuaTable globals) {
  Parser parser(source, std::move(globals));
  return parser.Run();
}

}  // namespace ro::formats
