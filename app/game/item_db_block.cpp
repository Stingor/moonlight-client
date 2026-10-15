// Le bloc de faits d'un objet, tiré de l'item_db. Voir l'en-tête.

#include "game/item_db_block.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/vfs/item_table.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::vfs::ServerItem;

constexpr std::size_t kLineBytes = 256;
// Le poids se compte en dixièmes.
constexpr std::uint32_t kWeightTenths = 10;
// La clé qui vaut tous les jobs, ou toutes les classes.
constexpr std::string_view kAllKey = "All";
// Ce qui sépare deux emplacements, comme la table : « Upper / Middle ».
constexpr std::string_view kPlaceSeparator = " / ";
constexpr std::string_view kListSeparator = ", ";

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

[[nodiscard]] bool Has(const std::vector<std::string>& keys, std::string_view key) {
  return std::ranges::any_of(keys, [key](const std::string& held) { return SameText(held, key); });
}

// ── LES LIBELLÉS, traduits à la lecture : la langue change en cours de partie

// Les sous-types d'une arme (`SubType` d'un `Weapon`).
[[nodiscard]] const char* WeaponKindLabel(std::string_view key) {
  if (SameText(key, "Fist")) {
    return Tr("Mains nues");
  }
  if (SameText(key, "Dagger")) {
    return Tr("Dague");
  }
  if (SameText(key, "1hSword")) {
    return Tr("Épée à une main");
  }
  if (SameText(key, "2hSword")) {
    return Tr("Épée à deux mains");
  }
  if (SameText(key, "1hSpear")) {
    return Tr("Lance à une main");
  }
  if (SameText(key, "2hSpear")) {
    return Tr("Lance à deux mains");
  }
  if (SameText(key, "1hAxe")) {
    return Tr("Hache à une main");
  }
  if (SameText(key, "2hAxe")) {
    return Tr("Hache à deux mains");
  }
  if (SameText(key, "Mace")) {
    return Tr("Masse");
  }
  if (SameText(key, "2hMace")) {
    return Tr("Masse à deux mains");
  }
  if (SameText(key, "Staff")) {
    return Tr("Bâton");
  }
  if (SameText(key, "2hStaff")) {
    return Tr("Bâton à deux mains");
  }
  if (SameText(key, "Bow")) {
    return Tr("Arc");
  }
  if (SameText(key, "Knuckle")) {
    return Tr("Poing");
  }
  if (SameText(key, "Musical")) {
    return Tr("Instrument de musique");
  }
  if (SameText(key, "Whip")) {
    return Tr("Fouet");
  }
  if (SameText(key, "Book")) {
    return Tr("Livre");
  }
  if (SameText(key, "Katar")) {
    return Tr("Katar");
  }
  if (SameText(key, "Revolver")) {
    return Tr("Revolver");
  }
  if (SameText(key, "Rifle")) {
    return Tr("Fusil");
  }
  if (SameText(key, "Gatling")) {
    return Tr("Gatling");
  }
  if (SameText(key, "Shotgun")) {
    return Tr("Fusil à pompe");
  }
  if (SameText(key, "Grenade")) {
    return Tr("Lance-grenades");
  }
  if (SameText(key, "Huuma")) {
    return Tr("Shuriken Huuma");
  }
  return nullptr;
}

// Les sous-types d'une munition (`SubType` d'un `Ammo`).
[[nodiscard]] const char* AmmoKindLabel(std::string_view key) {
  if (SameText(key, "Arrow")) {
    return Tr("Flèche");
  }
  if (SameText(key, "Dagger")) {
    return Tr("Dague de lancer");
  }
  if (SameText(key, "Bullet")) {
    return Tr("Balle");
  }
  if (SameText(key, "Shell")) {
    return Tr("Cartouche");
  }
  if (SameText(key, "Grenade")) {
    return Tr("Grenade");
  }
  if (SameText(key, "Shuriken")) {
    return Tr("Shuriken");
  }
  if (SameText(key, "Kunai")) {
    return Tr("Kunai");
  }
  if (SameText(key, "CannonBall")) {
    return Tr("Boulet de canon");
  }
  if (SameText(key, "ThrowWeapon")) {
    return Tr("Arme de lancer");
  }
  return nullptr;
}

// Les autres types, au singulier.
[[nodiscard]] const char* ItemKindLabel(std::string_view key) {
  if (SameText(key, "Healing")) {
    return Tr("Soin");
  }
  if (SameText(key, "Usable")) {
    return Tr("Consommable");
  }
  if (SameText(key, "DelayConsume")) {
    return Tr("Consommable différé");
  }
  if (SameText(key, "Etc")) {
    return Tr("Divers");
  }
  if (SameText(key, "PetEgg")) {
    return Tr("Œuf de familier");
  }
  if (SameText(key, "PetArmor")) {
    return Tr("Équipement de familier");
  }
  if (SameText(key, "ShadowGear")) {
    return Tr("Équipement d'ombre");
  }
  if (SameText(key, "Cash")) {
    return Tr("Item de la boutique");
  }
  return nullptr;
}

// Les classes de l'objet (`Classes`) : la naissance du personnage.
[[nodiscard]] const char* ClassLabel(std::string_view key) {
  if (SameText(key, "Normal")) {
    return Tr("Normale");
  }
  if (SameText(key, "Upper")) {
    return Tr("Transcendante");
  }
  if (SameText(key, "Baby")) {
    return Tr("Bébé");
  }
  if (SameText(key, "Third")) {
    return Tr("Troisième");
  }
  if (SameText(key, "Third_Upper")) {
    return Tr("Troisième transcendante");
  }
  if (SameText(key, "Third_Baby")) {
    return Tr("Troisième bébé");
  }
  if (SameText(key, "Fourth")) {
    return Tr("Quatrième");
  }
  return nullptr;
}

// ── LES EMPLACEMENTS ────────────────────────────────────────────────────────

// Une pièce du corps, dans l'ordre où le bloc les écrit.
enum class Piece : std::uint8_t {
  kHeadTop,
  kHeadMid,
  kHeadLow,
  kArmor,
  kWeapon,
  kTwoHands,
  kShield,
  kGarment,
  kShoes,
  kAccessory,
  kAmmo,
};
inline constexpr std::size_t kPieceCount = 11;

// Le libellé d'une pièce, traduit à la lecture.
[[nodiscard]] const char* PieceLabel(Piece piece) {
  switch (piece) {
    case Piece::kHeadTop:
      return Tr("Haut");
    case Piece::kHeadMid:
      return Tr("Milieu");
    case Piece::kHeadLow:
      return Tr("Bas");
    case Piece::kArmor:
      return Tr("Armure");
    case Piece::kWeapon:
      return Tr("Arme");
    case Piece::kTwoHands:
      return Tr("Deux mains");
    case Piece::kShield:
      return Tr("Bouclier");
    case Piece::kGarment:
      return Tr("Cape");
    case Piece::kShoes:
      return Tr("Chaussures");
    case Piece::kAccessory:
      return Tr("Accessoire");
    case Piece::kAmmo:
      return Tr("Munition");
  }
  return "";
}

// L'apparat et l'ombre se portent à part des pièces ordinaires.
enum class Layer : std::uint8_t { kOrdinary, kCostume, kShadow };

struct Place {
  std::string_view key;
  Piece piece = Piece::kArmor;
  Layer layer = Layer::kOrdinary;
};

// 🔴 L'APPARAT DE TOUTE LA TÊTE COUVRE SES TROIS PIÈCES : il paraît trois fois.
constexpr std::array<Place, 27> kPlaces = {{
    {"Head_Top", Piece::kHeadTop, Layer::kOrdinary},
    {"Head_Mid", Piece::kHeadMid, Layer::kOrdinary},
    {"Head_Low", Piece::kHeadLow, Layer::kOrdinary},
    {"Armor", Piece::kArmor, Layer::kOrdinary},
    {"Right_Hand", Piece::kWeapon, Layer::kOrdinary},
    {"Both_Hand", Piece::kTwoHands, Layer::kOrdinary},
    {"Left_Hand", Piece::kShield, Layer::kOrdinary},
    {"Garment", Piece::kGarment, Layer::kOrdinary},
    {"Shoes", Piece::kShoes, Layer::kOrdinary},
    {"Right_Accessory", Piece::kAccessory, Layer::kOrdinary},
    {"Left_Accessory", Piece::kAccessory, Layer::kOrdinary},
    {"Both_Accessory", Piece::kAccessory, Layer::kOrdinary},
    {"Ammo", Piece::kAmmo, Layer::kOrdinary},
    {"Costume_Head_Top", Piece::kHeadTop, Layer::kCostume},
    {"Costume_Head_Mid", Piece::kHeadMid, Layer::kCostume},
    {"Costume_Head_Low", Piece::kHeadLow, Layer::kCostume},
    {"Costume_Head", Piece::kHeadTop, Layer::kCostume},
    {"Costume_Head", Piece::kHeadMid, Layer::kCostume},
    {"Costume_Head", Piece::kHeadLow, Layer::kCostume},
    {"Costume_Garment", Piece::kGarment, Layer::kCostume},
    {"Shadow_Armor", Piece::kArmor, Layer::kShadow},
    {"Shadow_Weapon", Piece::kWeapon, Layer::kShadow},
    {"Shadow_Shield", Piece::kShield, Layer::kShadow},
    {"Shadow_Shoes", Piece::kShoes, Layer::kShadow},
    {"Shadow_Right_Accessory", Piece::kAccessory, Layer::kShadow},
    {"Shadow_Left_Accessory", Piece::kAccessory, Layer::kShadow},
    {"Shadow_Both_Accessory", Piece::kAccessory, Layer::kShadow},
}};

struct Worn {
  std::array<bool, kPieceCount> pieces{};
  bool costume = false;
  bool shadow = false;
  [[nodiscard]] bool On(Piece piece) const { return pieces[static_cast<std::size_t>(piece)]; }
  [[nodiscard]] bool Head() const {
    return On(Piece::kHeadTop) || On(Piece::kHeadMid) || On(Piece::kHeadLow);
  }
};

[[nodiscard]] Worn WornOf(const ServerItem& item) {
  Worn worn;
  const auto wear = [&worn](Piece piece, Layer layer) {
    worn.pieces[static_cast<std::size_t>(piece)] = true;
    worn.costume = worn.costume || layer == Layer::kCostume;
    worn.shadow = worn.shadow || layer == Layer::kShadow;
  };
  for (const std::string& location : item.locations) {
    for (const Place& place : kPlaces) {
      if (SameText(place.key, location)) {
        wear(place.piece, place.layer);
      }
    }
  }
  return worn;
}

// « Haut / Milieu », « Armure », dans l'ordre des pièces.
[[nodiscard]] std::string PositionOf(const Worn& worn) {
  std::string text;
  for (std::size_t rank = 0; rank < kPieceCount; ++rank) {
    if (worn.pieces[rank]) {
      if (!text.empty()) {
        text += kPlaceSeparator;
      }
      text += PieceLabel(static_cast<Piece>(rank));
    }
  }
  return text;
}

// Où une carte se sertit : la tête entière est un couvre-chef, les deux mains
// une arme.
[[nodiscard]] std::string CompoundOf(Worn worn) {
  const bool head = worn.Head();
  worn.pieces[static_cast<std::size_t>(Piece::kHeadMid)] = false;
  worn.pieces[static_cast<std::size_t>(Piece::kHeadLow)] = false;
  worn.pieces[static_cast<std::size_t>(Piece::kHeadTop)] = false;
  if (worn.On(Piece::kTwoHands)) {
    worn.pieces[static_cast<std::size_t>(Piece::kTwoHands)] = false;
    worn.pieces[static_cast<std::size_t>(Piece::kWeapon)] = true;
  }
  std::string text = head ? std::string(Tr("Couvre-chef")) : std::string();
  if (std::string rest = PositionOf(worn); !rest.empty()) {
    if (!text.empty()) {
      text += kPlaceSeparator;
    }
    text += rest;
  }
  return text;
}

// Le type d'une armure se lit à ce qu'elle habille, comme la table l'écrit.
[[nodiscard]] const char* ArmorKindOf(const Worn& worn) {
  if (worn.costume) {
    return Tr("Costume");
  }
  if (worn.shadow) {
    return Tr("Équipement d'ombre");
  }
  if (worn.Head()) {
    return Tr("Couvre-chef");
  }
  constexpr std::array<Piece, 6> kNamedPieces = {Piece::kArmor, Piece::kShield,    Piece::kGarment,
                                                 Piece::kShoes, Piece::kAccessory, Piece::kWeapon};
  for (const Piece piece : kNamedPieces) {
    if (worn.On(piece)) {
      return PieceLabel(piece);
    }
  }
  return Tr("Armure");
}

[[nodiscard]] bool IsType(const ServerItem& item, std::string_view type) {
  return SameText(item.type, type);
}

[[nodiscard]] std::string KindOf(const ServerItem& item, const Worn& worn) {
  const char* label = nullptr;
  if (IsType(item, "Weapon")) {
    label = WeaponKindLabel(item.sub_type);
  } else if (IsType(item, "Ammo")) {
    label = AmmoKindLabel(item.sub_type);
  } else if (IsType(item, "Armor")) {
    label = ArmorKindOf(worn);
  } else if (IsType(item, "Card")) {
    label = SameText(item.sub_type, "Enchant") ? Tr("Enchantement") : Tr("Carte");
  } else {
    label = ItemKindLabel(item.type);
  }
  // Un nom que les tables ne connaissent pas s'écrit tel que la base l'écrit.
  if (label != nullptr) {
    return label;
  }
  return item.sub_type.empty() ? item.type : item.sub_type;
}

// ── LES JOBS ────────────────────────────────────────────────────────────────

// Une lignée : le premier job et ceux qu'il devient.
inline constexpr std::uint8_t kNoLineage = 0xFF;

struct Job {
  std::string_view key;
  // Le nom du jeu, celui de `job_names.yml` : le jargon ne se traduit pas.
  std::string_view shown;
  std::uint8_t lineage = kNoLineage;
};

// 🔴 LES JOBS D'ORIGINE, dans l'ordre où le résumé les écrit : premier job,
// puis ceux qu'il devient. Ce sont eux que « Tous les jobs sauf » complète.
constexpr std::array<Job, 25> kOriginalJobs = {{
    {"Novice", "Novice", 0},
    {"SuperNovice", "Super Novice", 0},
    {"Swordman", "Swordman", 1},
    {"Knight", "Knight", 1},
    {"Crusader", "Crusader", 1},
    {"Mage", "Magician", 2},
    {"Wizard", "Wizard", 2},
    {"Sage", "Sage", 2},
    {"Archer", "Archer", 3},
    {"Hunter", "Hunter", 3},
    {"BardDancer", "Bard / Dancer", 3},
    {"Acolyte", "Acolyte", 4},
    {"Priest", "Priest", 4},
    {"Monk", "Monk", 4},
    {"Merchant", "Merchant", 5},
    {"Blacksmith", "Blacksmith", 5},
    {"Alchemist", "Alchemist", 5},
    {"Thief", "Thief", 6},
    {"Assassin", "Assassin", 6},
    {"Rogue", "Rogue", 6},
    {"Taekwon", "Taekwon", 7},
    {"StarGladiator", "Star Gladiator", 7},
    {"SoulLinker", "Soul Linker", 7},
    {"Gunslinger", "Gunslinger", 8},
    {"Ninja", "Ninja", 9},
}};
inline constexpr std::uint8_t kLineageCount = 10;

// Les jobs venus après, que la base peut nommer.
constexpr std::array<Job, 4> kLaterJobs = {{
    {"KagerouOboro", "Kagerou / Oboro"},
    {"Rebellion", "Rebellion"},
    {"Summoner", "Summoner"},
    {"Spirit_Handler", "Spirit Handler"},
}};

[[nodiscard]] std::string_view ShownJob(std::string_view key) {
  for (const Job& job : kOriginalJobs) {
    if (SameText(job.key, key)) {
      return job.shown;
    }
  }
  for (const Job& job : kLaterJobs) {
    if (SameText(job.key, key)) {
      return job.shown;
    }
  }
  return key;
}

[[nodiscard]] bool IsOriginal(std::string_view key) {
  return std::ranges::any_of(kOriginalJobs,
                             [key](const Job& job) { return SameText(job.key, key); });
}

[[nodiscard]] std::string Joined(const std::vector<std::string>& parts, std::string_view between) {
  std::string joined;
  for (const std::string& part : parts) {
    if (!joined.empty()) {
      joined += between;
    }
    joined += part;
  }
  return joined;
}

// Des jobs en morceaux : les lignées complètes d'un côté, les jobs seuls de
// l'autre. `pieces` compte ce que le joueur lira.
struct JobList {
  std::vector<std::string> lineages;
  std::vector<std::string> singles;
  [[nodiscard]] std::size_t pieces() const { return lineages.size() + singles.size(); }
  [[nodiscard]] std::string Text() const {
    std::string text;
    if (!lineages.empty()) {
      const std::string names = Joined(lineages, kListSeparator);
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(), Tr("classes %s"), names.c_str());
      text = line.data();
    }
    for (const std::string& single : singles) {
      if (!text.empty()) {
        text += kListSeparator;
      }
      text += single;
    }
    return text;
  }
};

[[nodiscard]] JobList Grouped(const std::vector<std::string>& keys) {
  JobList list;
  for (std::uint8_t lineage = 0; lineage < kLineageCount; ++lineage) {
    std::vector<const Job*> members;
    std::vector<const Job*> present;
    for (const Job& job : kOriginalJobs) {
      if (job.lineage == lineage) {
        members.push_back(&job);
        if (Has(keys, job.key)) {
          present.push_back(&job);
        }
      }
    }
    if (members.size() > 1 && present.size() == members.size()) {
      list.lineages.emplace_back(members.front()->shown);
      continue;
    }
    for (const Job* job : present) {
      list.singles.emplace_back(job->shown);
    }
  }
  for (const std::string& key : keys) {
    if (!IsOriginal(key) && !SameText(key, kAllKey)) {
      list.singles.emplace_back(ShownJob(key));
    }
  }
  return list;
}

[[nodiscard]] std::string AllBut(const JobList& left_out) {
  const std::string names = left_out.Text();
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), Tr("Tous les jobs sauf %s"), names.c_str());
  return line.data();
}

// ── LES LIGNES ──────────────────────────────────────────────────────────────

[[nodiscard]] std::string Labelled(const char* label, std::string_view value) {
  std::string line(kItemDbLabelInk);
  line += label;
  line += kItemDbPlainInk;
  if (!value.empty()) {
    line += ' ';
    line += value;
  }
  return line;
}

[[nodiscard]] std::string WeightText(std::uint32_t tenths) {
  std::array<char, kLineBytes> text{};
  if (tenths % kWeightTenths == 0) {
    std::snprintf(text.data(), text.size(), "%u", static_cast<unsigned>(tenths / kWeightTenths));
  } else {
    std::snprintf(text.data(), text.size(), Tr("%u,%u"),
                  static_cast<unsigned>(tenths / kWeightTenths),
                  static_cast<unsigned>(tenths % kWeightTenths));
  }
  return text.data();
}

// Le niveau requis, vide quand il ne borne rien.
[[nodiscard]] std::string LevelText(const ServerItem& item) {
  std::array<char, kLineBytes> text{};
  if (item.equip_level_max != 0 && item.equip_level_max >= item.equip_level_min) {
    std::snprintf(text.data(), text.size(), Tr("Niveau de base %u à %u"),
                  static_cast<unsigned>(item.equip_level_min),
                  static_cast<unsigned>(item.equip_level_max));
  } else if (item.equip_level_min > ro::vfs::kServerItemDefaultEquipLevelMin) {
    std::snprintf(text.data(), text.size(), Tr("Niveau de base %u"),
                  static_cast<unsigned>(item.equip_level_min));
  }
  return text.data();
}

// Les classes, vides quand toutes peuvent porter l'objet.
[[nodiscard]] std::string ClassesText(const ServerItem& item) {
  if (Has(item.classes, kAllKey)) {
    return {};
  }
  if (item.classes.empty()) {
    return Tr("Classes : aucune");
  }
  std::vector<std::string> names;
  for (const std::string& kind : item.classes) {
    const char* const label = ClassLabel(kind);
    names.emplace_back(label != nullptr ? std::string(label) : kind);
  }
  const std::string joined = Joined(names, kListSeparator);
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), Tr("Classes : %s"), joined.c_str());
  return line.data();
}

[[nodiscard]] std::string NumberText(std::uint32_t value) { return std::to_string(value); }

}  // namespace

bool JobsRestricted(const ServerItem& item) {
  return !Has(item.jobs, kAllKey) || !item.jobs_lowered.empty();
}

std::string JobsSummaryOf(const ServerItem& item) {
  // Un `Jobs` sans clé levée n'ouvre l'objet à personne.
  if (item.jobs.empty()) {
    return Tr("Aucun job");
  }
  if (Has(item.jobs, kAllKey)) {
    return item.jobs_lowered.empty() ? std::string(Tr("Tous les jobs"))
                                     : AllBut(Grouped(item.jobs_lowered));
  }
  std::vector<std::string> left_out;
  for (const Job& job : kOriginalJobs) {
    if (!Has(item.jobs, job.key)) {
      left_out.emplace_back(job.key);
    }
  }
  const bool originals_only = std::ranges::all_of(item.jobs, IsOriginal);
  if (originals_only && left_out.empty()) {
    return Tr("Tous les jobs");
  }
  const JobList named = Grouped(item.jobs);
  if (originals_only) {
    const JobList others = Grouped(left_out);
    if (others.pieces() < named.pieces()) {
      return AllBut(others);
    }
  }
  return named.Text();
}

std::vector<std::string> ItemDbBlockOf(const ServerItem& item) {
  std::vector<std::string> lines;
  std::string header(kItemDbHeaderInk);
  header += Tr("Données du serveur");
  header += kItemDbPlainInk;
  lines.push_back(std::move(header));

  const Worn worn = WornOf(item);
  const bool weapon = IsType(item, "Weapon");
  const bool armor = IsType(item, "Armor") || IsType(item, "ShadowGear");
  const bool card = IsType(item, "Card");
  // Ce qui se porte sur soi : le serveur y regarde les jobs et le raffinement.
  const bool worn_on_body = weapon || armor || IsType(item, "Ammo");
  const bool costume = worn.costume;

  lines.push_back(Labelled(Tr("Type :"), KindOf(item, worn)));
  if (item.attack > 0) {
    lines.push_back(Labelled(Tr("Attaque :"), NumberText(item.attack)));
  }
  if (item.magic_attack > 0) {
    lines.push_back(Labelled(Tr("Attaque magique :"), NumberText(item.magic_attack)));
  }
  // Une armure dit sa défense, nulle comprise, comme la table.
  if ((armor && !costume) || item.defense > 0) {
    lines.push_back(Labelled(Tr("Défense :"), NumberText(item.defense)));
  }
  if (armor) {
    if (const std::string position = PositionOf(worn); !position.empty()) {
      lines.push_back(Labelled(Tr("Position :"), position));
    }
  }
  if (card) {
    if (const std::string compound = CompoundOf(worn); !compound.empty()) {
      lines.push_back(Labelled(Tr("Se sertit sur :"), compound));
    }
  }
  if (weapon && item.range_cells > 0) {
    std::array<char, kLineBytes> range{};
    std::snprintf(range.data(), range.size(), Tr("%u case(s)"),
                  static_cast<unsigned>(item.range_cells));
    lines.push_back(Labelled(Tr("Portée :"), range.data()));
  }
  lines.push_back(Labelled(Tr("Poids :"), WeightText(item.weight_tenths)));
  if (item.weapon_level > 0) {
    lines.push_back(Labelled(Tr("Niveau d'arme :"), NumberText(item.weapon_level)));
  }
  if (item.armor_level > 0) {
    lines.push_back(Labelled(Tr("Niveau d'armure :"), NumberText(item.armor_level)));
  }
  if (item.slots > 0) {
    lines.push_back(Labelled(Tr("Emplacements de carte :"), NumberText(item.slots)));
  }
  if ((weapon || armor) && !costume) {
    lines.push_back(Labelled(Tr("Raffinable :"), item.refineable ? Tr("Oui") : Tr("Non")));
  }

  // ── LES PRÉREQUIS, après un trait, comme la table ─────────────────────────
  std::vector<std::string> needs;
  if (std::string level = LevelText(item); !level.empty()) {
    needs.push_back(std::move(level));
  }
  if (worn_on_body || JobsRestricted(item)) {
    needs.push_back(JobsSummaryOf(item));
  }
  if (std::string classes = ClassesText(item); !classes.empty()) {
    needs.push_back(std::move(classes));
  }
  if (SameText(item.gender, "Female")) {
    needs.emplace_back(Tr("Femmes seulement"));
  } else if (SameText(item.gender, "Male")) {
    needs.emplace_back(Tr("Hommes seulement"));
  }
  if (!needs.empty()) {
    lines.emplace_back(ro::vfs::kDescriptionRule);
    lines.push_back(Labelled(Tr("Prérequis :"), {}));
    lines.insert(lines.end(), needs.begin(), needs.end());
  }
  return lines;
}

void AppendItemDbBlock(std::vector<std::string>& lines, const ro::vfs::ServerDb* db,
                       std::uint32_t item_id) {
  if (db == nullptr || !db->known()) {
    return;
  }
  if (!lines.empty()) {
    lines.emplace_back(ro::vfs::kDescriptionRule);
  }
  const ServerItem* const item = db->FindItem(item_id);
  if (item == nullptr) {
    std::string unknown(kItemDbHeaderInk);
    unknown += Tr("Données du serveur : item inconnu");
    unknown += kItemDbPlainInk;
    lines.push_back(std::move(unknown));
    return;
  }
  const std::vector<std::string> block = ItemDbBlockOf(*item);
  lines.insert(lines.end(), block.begin(), block.end());
}

}  // namespace moonclient
