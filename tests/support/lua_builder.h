#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ro::test_support {

// Fabrique de chunks Lua 5.1 compilés.
//
// Même raison d'être que le constructeur d'archives : aucun asset Gravity n'est
// versionné, donc les cas que le lecteur doit tenir — un opcode hors cadre, un
// registre hors table, un SETLIST par lots — se fabriquent ici plutôt que de se
// chercher dans de vraies données.
//
// C'est un ASSEMBLEUR, pas un compilateur : le test écrit les instructions
// qu'il veut voir exécuter. Passer par une syntaxe de plus haut niveau cacherait
// justement les opcodes que l'on cherche à éprouver.
//
// La disposition des champs vient de lundump.c et lopcodes.h : six bits
// d'opcode, huit pour A, neuf pour C, neuf pour B, ou dix-huit pour Bx.
class LuaChunkBuilder {
 public:
  // Le bit qui, dans un argument B ou C, désigne une constante et non un
  // registre. Les tests s'en servent pour écrire RK(k).
  static constexpr std::uint32_t kConstant = 0x100;

  [[nodiscard]] std::uint32_t AddNil();
  [[nodiscard]] std::uint32_t AddBoolean(bool value);
  [[nodiscard]] std::uint32_t AddNumber(double value);
  [[nodiscard]] std::uint32_t AddString(std::string_view value);

  void Move(std::uint32_t destination, std::uint32_t source);
  void LoadConstant(std::uint32_t destination, std::uint32_t constant);
  void LoadBoolean(std::uint32_t destination, bool value, bool skip_next);
  void LoadNil(std::uint32_t first, std::uint32_t last);
  void GetGlobal(std::uint32_t destination, std::uint32_t name_constant);
  void SetGlobal(std::uint32_t source, std::uint32_t name_constant);
  void GetTable(std::uint32_t destination, std::uint32_t table, std::uint32_t key);
  void SetTable(std::uint32_t table, std::uint32_t key, std::uint32_t value);
  void NewTable(std::uint32_t destination);
  void SetList(std::uint32_t table, std::uint32_t count, std::uint32_t batch);
  // Un lot au-delà de ce que C peut porter : le rang s'écrit alors dans le mot
  // suivant, qui n'est pas une instruction.
  void SetListExtended(std::uint32_t table, std::uint32_t count, std::uint32_t batch);
  void Closure(std::uint32_t destination, std::uint32_t proto, std::uint32_t upvalues);
  void Return();
  // Pour éprouver le refus : un opcode que le lecteur ne connaît pas.
  void RawInstruction(std::uint32_t word);

  void set_max_stack(std::uint8_t registers) { max_stack_ = registers; }
  // Nombre de sous-fonctions vides à déclarer, pour que CLOSURE ait une cible.
  void set_proto_count(std::uint8_t count) { proto_count_ = count; }
  // Nombre de valeurs externes déclarées par les sous-fonctions vides.
  void set_proto_upvalues(std::uint8_t count) { proto_upvalues_ = count; }

  [[nodiscard]] std::vector<std::uint8_t> Build() const;

  // Un en-tête seul, pour éprouver les refus. Les valeurs par défaut sont celles
  // que le client produit.
  [[nodiscard]] static std::vector<std::uint8_t> Header(std::uint8_t version = 0x51,
                                                        std::uint8_t endianness = 1,
                                                        std::uint8_t number_size = 8);

 private:
  struct Constant {
    std::uint8_t kind = 0;
    bool boolean = false;
    double number = 0.0;
    std::string text;
  };

  void Emit(std::uint32_t opcode, std::uint32_t a, std::uint32_t b, std::uint32_t c);
  void EmitBx(std::uint32_t opcode, std::uint32_t a, std::uint32_t bx);

  std::vector<std::uint32_t> code_;
  std::vector<Constant> constants_;
  std::uint8_t max_stack_ = 8;
  std::uint8_t proto_count_ = 0;
  std::uint8_t proto_upvalues_ = 0;
};

}  // namespace ro::test_support
