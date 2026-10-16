#pragma once

// CE QUE LE SCRIPT D'UN OBJET DIT DE LUI, LU PAR MOTIFS EXACTS : le soin
// (`itemheal`, `percentheal`, `heal`), les états levés (`sc_end`), l'élément
// d'attaque et d'armure (`bonus bAtkEle` / `bDefEle`), la capture (`pet`) et la
// durée commune des états posés (`sc_start`).
//
// 🔴 UN FAIT SE LIT ENTIER OU PAS DU TOUT. Une instruction ne compte que posée
// hors de toute accolade, seule, et écrite sous la forme exacte du motif, avec
// des nombres littéraux. Une famille que nomme UNE instruction d'une autre
// forme — conditionnelle, dans un bloc, calculée — ne dit rien : la plage
// montrée serait fausse ou incomplète. Après une rupture de séquence (`end`,
// `return`, `goto`, `break`, `close`, `callsub`, `callfunc`), plus rien ne se
// lit, et toute famille nommée ensuite se tait.
//
// Le lecteur n'interprète rien d'autre : un effet en texte libre reste aux
// lignes d'item_info.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace moonclient {

// Un soin : des points, ou des pour cent du maximum.
struct ScriptHeal {
  bool percent = false;
  std::uint32_t hp_least = 0;
  std::uint32_t hp_most = 0;
  std::uint32_t sp_least = 0;
  std::uint32_t sp_most = 0;
};

// Le rang d'un élément dans l'ordre du serveur, du neutre (0) au mort-vivant
// (9).
inline constexpr std::uint8_t kNoScriptElement = 0xFF;
inline constexpr std::size_t kScriptElementCount = 10;

struct ItemScriptFacts {
  std::optional<ScriptHeal> heal;
  // Les états que l'objet lève, sans `SC_`, dans l'ordre écrit et sans doublon :
  // « POISON », « CURSE »…
  std::vector<std::string> cured;
  std::uint8_t attack_element = kNoScriptElement;
  std::uint8_t defense_element = kNoScriptElement;
  // `pet;` : l'objet sert d'appât, et capture le monstre dont il est le
  // `TameItem`. `pet <item>;` désigne un autre appât.
  bool captures = false;
  // Zéro : l'objet lui-même.
  std::uint32_t lure_item_id = 0;
  // La durée que tous les `sc_start` de l'objet partagent ; zéro sans
  // `sc_start`, ou quand ils diffèrent.
  std::uint32_t effect_duration_ms = 0;
};

[[nodiscard]] ItemScriptFacts ReadItemScript(std::string_view script);

}  // namespace moonclient
