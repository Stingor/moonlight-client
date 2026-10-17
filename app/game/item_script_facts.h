#pragma once

// CE QUE LE SCRIPT D'UN OBJET DIT DE LUI, LU PAR MOTIFS EXACTS : le soin
// (`itemheal`, `percentheal`, `heal`), les états levés (`sc_end`), l'élément
// d'attaque et d'armure (`bonus bAtkEle` / `bDefEle`), la capture (`pet`), la
// durée commune des états posés (`sc_start`) et les valeurs que chacun reçoit,
// les objets donnés (`getitem`, `getgroupitem`, `getrandgroupitem`) et les
// monstres invoqués (`monster "this",…`).
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

#include <array>
#include <cstddef>
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

// Le plus de valeurs qu'un état reçoit à sa pose : val1 à val4.
inline constexpr std::size_t kScriptStartValueCount = 4;

// Un état posé par `sc_start`, `sc_start2` ou `sc_start4`, avec les valeurs
// qu'il reçoit ; celles que la forme ne donne pas valent zéro.
struct ScriptStart {
  // La constante sans `SC_`, en majuscules : « PUTTI_TAILS_NOODLES ».
  std::string status;
  std::array<std::int32_t, kScriptStartValueCount> values{};
};

// CE QU'UN OBJET DONNE : un objet nommé (`getitem <id | "Aegis">,<n>`), les
// objets d'un groupe (`getgroupitem(IG_X)`, le groupe fixe leur nombre), ou n
// objets tirés dans un groupe (`getrandgroupitem(IG_X,n)`).
enum class ScriptGiftKind : std::uint8_t { kItem, kGroup, kRandomGroup };

struct ScriptGift {
  ScriptGiftKind kind = ScriptGiftKind::kItem;
  // L'objet donné ; zéro quand le script le nomme par son nom Aegis.
  std::uint32_t item_id = 0;
  std::string aegis_name;
  // La constante du groupe sans `IG_` : « BlueBox ».
  std::string group;
  // Zéro pour `getgroupitem`.
  std::uint32_t count = 0;
};

// UN MONSTRE INVOQUÉ par `monster "this",x,y,"nom",<monstre>,<n>` : un
// identifiant, ou un tirage dans un groupe (`-1-MOBG_X`).
struct ScriptSummon {
  // Zéro pour un tirage.
  std::uint32_t monster_id = 0;
  // La constante du groupe sans `MOBG_`, telle qu'écrite.
  std::string group;
  std::uint32_t count = 0;
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
  // Les états posés, dans l'ordre écrit ; vide quand un seul `sc_start` ne se
  // lit pas, comme la durée.
  std::vector<ScriptStart> starts;
  // Les dons et les invocations, dans l'ordre écrit ; chaque liste est vide
  // quand une seule instruction de sa famille ne se lit pas.
  std::vector<ScriptGift> gifts;
  std::vector<ScriptSummon> summons;
};

[[nodiscard]] ItemScriptFacts ReadItemScript(std::string_view script);

}  // namespace moonclient
