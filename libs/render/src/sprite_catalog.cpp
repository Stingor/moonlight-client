#include "ro/render/sprite_catalog.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <utility>

#include "ro/render/shown_job.h"
#include "ro/vfs/encoding.h"

#include "client_tables.h"
#include "sprite_catalog_internal.h"

namespace ro::render {

using catalog_internal::BodyClass;
using catalog_internal::DualWieldClass;
using catalog_internal::IsDoram;
using catalog_internal::Join;
using catalog_internal::kAxeClass;
using catalog_internal::kAxePairClass;
using catalog_internal::kDaggerAxeClass;
using catalog_internal::kDaggerClass;
using catalog_internal::kDaggerPairClass;
using catalog_internal::kDaggerSwordClass;
using catalog_internal::kDoramHairFirst;
using catalog_internal::kDoramHairLast;
using catalog_internal::kDoramRace;
using catalog_internal::kFemaleToken;
using catalog_internal::kHeadFolder;
using catalog_internal::kHeadgearFolder;
using catalog_internal::kHumanRace;
using catalog_internal::kKnuckleClass;
using catalog_internal::kMaceClass;
using catalog_internal::kMaleToken;
using catalog_internal::kRodClass;
using catalog_internal::kSeparator;
using catalog_internal::kSpriteRoot;
using catalog_internal::kSwordAxeClass;
using catalog_internal::kSwordClass;
using catalog_internal::kSwordPairClass;
using catalog_internal::kTwoHandAxeClass;
using catalog_internal::kTwoHandSpearClass;
using catalog_internal::PlayerBodyName;
using catalog_internal::SexToken;
using catalog_internal::TableName;
using catalog_internal::WeaponClassOf;
using catalog_internal::WeaponFamilyClass;
using catalog_internal::WearsAlternateOutfit;

namespace {

// ── Les dossiers du client, tels que ses gabarits les nomment ─────────────────
//
// Le corps, le costume, la tête, les coiffes, l'arme et sa traînée, le
// bouclier et les corps non joueurs. Le client les porte en CP949 ; ici en
// UTF-8, et c'est le VFS qui transcode.
constexpr std::string_view kBodyFolder = "몸통";
constexpr std::string_view kCostumeFolderPrefix = "costume_";
// Le dossier des objets au sol.
constexpr std::string_view kItemFolder = "아이템";
constexpr std::string_view kDoramHeadgearSuffix = "_doram";
constexpr std::string_view kShieldFolder = "방패";
constexpr std::string_view kShieldSuffix = "_방패";
constexpr std::string_view kWeaponTrailSuffix = "_검광";
constexpr std::string_view kNpcFolder = "npc";
constexpr std::string_view kHomunculusFolder = "homun";
constexpr std::string_view kMonsterFolder = "몬스터";

// Les palettes, selon les gabarits du client PATCHÉ de ce serveur —
// BodyPalUnisex, HeadPalUnisex, DoramHeadPalShared : une palette par couleur,
// la même pour tous les métiers et les deux sexes. C'est ce que son GRF porte,
// 554 body_<n>.pal et 252 head_<n>.pal par race, là où le client d'origine
// nommait chaque palette d'après le métier, la coiffure et le sexe.
constexpr std::string_view kPaletteRoot = "data\\palette\\";
constexpr std::string_view kBodyPaletteFolder = "몸";
constexpr std::string_view kHairPaletteFolder = "머리";
constexpr std::string_view kBodyPalettePrefix = "body_";
constexpr std::string_view kHairPalettePrefix = "head_";
constexpr std::string_view kPaletteExtension = ".pal";

// Les métiers de la race Doram, que le client consulte partout où la race
// change quelque chose.
constexpr std::array<std::uint32_t, 7> kDoramJobs = {4217, 4218, 4219, 4220, 4221, 4308, 4315};

struct ClassRange {
  std::uint32_t first;
  std::uint32_t last;
};

bool Within(const ClassRange& range, std::uint32_t value) {
  return value >= range.first && value <= range.last;
}

template <std::size_t N>
bool WithinAny(const std::array<ClassRange, N>& ranges, std::uint32_t value) {
  return std::ranges::any_of(ranges,
                             [value](const ClassRange& range) { return Within(range, value); });
}

template <std::size_t N>
bool Listed(const std::array<std::uint32_t, N>& values, std::uint32_t value) {
  return std::ranges::find(values, value) != values.end();
}

// Les classes que le client nomme d'après sa table des sprites, par plages. Hors de ces plages, le
// nom reste celui dont la table est remplie d'avance : un Poring. C'est pour cela qu'un monstre
// inconnu du client apparaît en Poring.
constexpr std::array<ClassRange, 5> kTableNamedClasses = {
    {{45, 125}, {400, 999}, {1001, 3998}, {10000, 19998}, {20021, 31999}}};
constexpr std::string_view kUnknownClassName = "Poring";

// Les homoncules et les mercenaires, que la même fonction nomme en dur.
struct NamedClass {
  std::uint32_t job;
  std::string_view name;
};
constexpr std::array<NamedClass, 21> kExecutableNamedClasses = {
    {{6001, "LIF"},        {6002, "AMISTR"},     {6003, "FILIR"},    {6004, "VANILMIRTH"},
     {6005, "LIF2"},       {6006, "AMISTR2"},    {6007, "FILIR2"},   {6008, "VANILMIRTH2"},
     {6009, "LIF_H"},      {6010, "AMISTR_H"},   {6011, "FILIR_H"},  {6012, "VANILMIRTH_H"},
     {6013, "LIF_H2"},     {6014, "AMISTR_H2"},  {6015, "FILIR_H2"}, {6016, "VANILMIRTH_H2"},
     {6048, "MER_EIRA"},   {6049, "MER_BAYERI"}, {6050, "MER_SERA"}, {6051, "MER_DIETER"},
     {6052, "MER_ELEANOR"}}};

// Les mercenaires humains, dix par arme. Leur nom porte déjà le dossier de
// sexe : le client les range sous le dossier des corps, à plat.
struct MercenaryRange {
  ClassRange classes;
  std::string_view name;
};
constexpr std::array<MercenaryRange, 3> kMercenaries = {
    {{{6017, 6026}, "여\\활용병"}, {{6027, 6036}, "남\\창용병"}, {{6037, 6046}, "남\\검용병"}}};

// Les plages qui choisissent le dossier d'un corps non joueur. L'ordre compte : la plage des
// mercenaires humains est testée avant celle des homoncules, qu'elle recouvre.
constexpr ClassRange kNpcClasses = {45, 999};
constexpr ClassRange kPortalClasses = {10001, 20014};
constexpr ClassRange kHumanMercenaryClasses = {6017, 6046};
constexpr ClassRange kHomunculusClasses = {6001, 6052};

// Un nom de la table qui porte ceci désigne un modèle Granny, pas un sprite.
constexpr std::string_view kModelExtension = ".gr2";

// ── Ce que les mains tiennent ─────────────────────────────────────────────────
//
// Le serveur annonce des identifiants d'OBJET ; le client en tire la classe
// d'une arme par sa table des objets, et reconnaît un bouclier à sa plage
// d'identifiants — sa classe d'arme vaut -1.
constexpr std::array<ClassRange, 3> kShieldItems = {
    {{2101, 2199}, {28901, 28998}, {460000, 469999}}};

// Deux armes en main : le couple se nomme d'après les PLAGES d'identifiants
// des deux objets, pas d'après leur classe. Un couple hors de ces plages retombe sur l'arme droite
// seule.
constexpr std::array<ClassRange, 3> kSwordItems = {
    {{1100, 1149}, {13400, 13499}, {500000, 509999}}};
constexpr std::array<ClassRange, 4> kDaggerItems = {
    {{1200, 1249}, {13000, 13099}, {28700, 28899}, {510000, 519999}}};
constexpr std::array<ClassRange, 2> kAxeItems = {{{1300, 1349}, {520000, 529999}}};

// Ceux qui savent tenir deux armes ; les autres reçoivent l'épée à la place
// d'un couple qu'ils ne savent pas dessiner.
constexpr std::array<std::uint32_t, 7> kDualWieldJobs = {12, 4013, 4035, 4059, 4065, 4101, 4254};

// Les prêtres ne montrent pas de poing, ni le cardinal.
constexpr std::array<std::uint32_t, 5> kKnuckleLessJobs = {8, 4009, 4057, 4063, 4099};
constexpr std::uint32_t kCardinal = 4256;

// Un objet que le client ne cherche jamais par sa famille.
constexpr std::uint32_t kItemWithoutFamilyFallback = 1599;

// Les classes dont un couple ou une famille porte une traînée.
constexpr std::array<std::uint32_t, 16> kTrailClasses = {1,  2,  3,  4,  5,  6,  7,  16,
                                                         17, 18, 25, 26, 27, 28, 29, 30};

// La famille d'arme qu'un métier dessine à la place de celle qu'il tient — un voleur tient une
// masse comme une épée, un mage une lance comme un bâton.
constexpr std::array<std::uint32_t, 3> kMaceAsSwordJobs = {6, 4007, 4029};
constexpr std::array<std::uint32_t, 9> kSpearAsRodJobs = {2,    9,    16,   4003, 4010,
                                                          4017, 4025, 4032, 4039};
constexpr std::array<std::uint32_t, 13> kAssassinKindJobs = {
    12, 17, 4013, 4018, 4035, 4040, 4059, 4065, 4072, 4079, 4101, 4108, 4254};
constexpr std::array<std::uint32_t, 3> kRogueKindJobs = {17, 4018, 4040};

// Quelles FORMES de bouclier un métier sait dessiner, et lesquels ne dessinent aucune variante par
// objet.
constexpr std::uint32_t kShieldFormLimit = 2000;
constexpr std::uint32_t kGuardForm = 1;
constexpr std::uint32_t kBucklerForm = 2;
constexpr std::uint32_t kMirrorShieldForm = 4;
constexpr std::array<std::uint32_t, 22> kAllShieldFormsJobs = {
    1,    7,    14,   4002, 4008, 4015, 4024, 4030, 4037, 4054, 4060,
    4066, 4073, 4096, 4102, 4252, 4258, 4280, 4281, 4218, 4220, 4308};
constexpr std::array<std::uint32_t, 43> kGuardAndBucklerJobs = {
    4,    5,    6,    8,    10,   12,   17,   18,   4005, 4006, 4007, 4009, 4011, 4013, 4018,
    4019, 4027, 4028, 4029, 4031, 4033, 4035, 4040, 4041, 4057, 4058, 4059, 4063, 4064, 4065,
    4071, 4072, 4078, 4079, 4099, 4100, 4101, 4107, 4108, 4253, 4254, 4256, 4259};
constexpr std::array<std::uint32_t, 48> kGuardOnlyJobs = {
    0,    2,    3,    9,    11,   16,   19,   20,   23,   4001, 4003, 4004, 4010, 4012, 4017, 4020,
    4021, 4023, 4025, 4026, 4032, 4034, 4039, 4042, 4043, 4045, 4055, 4056, 4061, 4062, 4067, 4068,
    4069, 4074, 4075, 4076, 4097, 4098, 4103, 4104, 4105, 4190, 4191, 4255, 4257, 4261, 4263, 4264};
constexpr std::array<std::uint32_t, 1> kGuardOnlyHyperNovice = {4307};
constexpr std::array<std::uint32_t, 17> kNoItemShieldJobs = {
    13, 21, 22, 26, 27, 28, 29, 30, 4014, 4022, 4036, 4044, 4050, 4051, 4052, 4219, 4278};
constexpr std::array<ClassRange, 3> kNoItemShieldRanges = {
    {{4080, 4095}, {4109, 4112}, {4279, 4281}}};

std::string_view RaceFolder(std::uint32_t job) { return IsDoram(job) ? kDoramRace : kHumanRace; }

client_tables::WeaponSpriteClass WeaponSpriteClassOf(std::uint32_t job) {
  const auto& classes = client_tables::kWeaponSpriteClasses;
  const auto found =
      std::ranges::lower_bound(classes, job, {}, &client_tables::WeaponSpriteClass::job);
  if (found == classes.end() || found->job != job) {
    return client_tables::WeaponSpriteClass{job, client_tables::kDefaultWeaponSpriteClass,
                                            client_tables::kDefaultWeaponSpriteClass};
  }
  return *found;
}

std::string HairFileNumber(const ActorLook& look) {
  // Au-delà de la table humaine, le numéro tel quel, Doram compris.
  if (look.hair >= client_tables::kMaleHairFiles.size()) {
    return std::to_string(look.hair);
  }
  if (IsDoram(look.job)) {
    const std::uint16_t hair =
        look.hair < kDoramHairFirst || look.hair > kDoramHairLast ? kDoramHairLast : look.hair;
    const auto& table =
        look.sex != 0 ? client_tables::kDoramMaleHairFiles : client_tables::kDoramFemaleHairFiles;
    return std::string(table[hair]);
  }
  const auto& table =
      look.sex != 0 ? client_tables::kMaleHairFiles : client_tables::kFemaleHairFiles;
  return std::string(table[look.hair]);
}

// Le nom de ressource d'une classe non joueur, tel que le client le tient.
std::string NonPlayerName(const SpriteCatalog& catalog, std::uint32_t job) {
  for (const NamedClass& named : kExecutableNamedClasses) {
    if (named.job == job) {
      return std::string(named.name);
    }
  }
  for (const MercenaryRange& mercenaries : kMercenaries) {
    if (Within(mercenaries.classes, job)) {
      return std::string(mercenaries.name);
    }
  }
  for (const ClassRange& range : kTableNamedClasses) {
    if (Within(range, job)) {
      if (auto name = TableName(catalog.tables.class_sprites, job); name.has_value()) {
        return std::move(*name);
      }
      break;
    }
  }
  return std::string(kUnknownClassName);
}

bool IsModelName(std::string_view name) {
  // ⚠ LE CLIENT CHERCHE L'EXTENSION N'IMPORTE OÙ DANS LE NOM, et en minuscules
  // seulement : c'est une recherche de sous-chaîne, pas une fin de nom.
  return name.find(kModelExtension) != std::string_view::npos;
}

std::string_view NonPlayerFolder(std::uint32_t job) {
  if (Within(kNpcClasses, job) || Within(kPortalClasses, job)) {
    return kNpcFolder;
  }
  if (Within(kHumanMercenaryClasses, job)) {
    return kHumanRace;
  }
  if (Within(kHomunculusClasses, job)) {
    return kHomunculusFolder;
  }
  return kMonsterFolder;
}

// La vue d'un objet : sa classe pour une arme, sa forme pour un bouclier.
// Zéro quand la table ne la donne pas.
std::uint16_t ItemView(const SpriteCatalog& catalog, std::uint32_t item) {
  return catalog.items.ViewOf(item);
}

// La famille dessinée à la place de la classe tenue, selon le métier.
std::uint32_t DrawnWeaponFamily(std::uint32_t job, std::uint32_t weapon_class) {
  if (Listed(kMaceAsSwordJobs, job)) {
    return weapon_class == kMaceClass || weapon_class == kAxeClass ||
                   weapon_class == kTwoHandAxeClass
               ? kSwordClass
               : weapon_class;
  }
  if (Listed(kSpearAsRodJobs, job)) {
    return weapon_class == kTwoHandSpearClass ? kRodClass : weapon_class;
  }
  if (Listed(kAssassinKindJobs, job)) {
    if (weapon_class == kMaceClass || weapon_class == kTwoHandAxeClass) {
      return kSwordClass;
    }
    if (weapon_class == kAxeClass && Listed(kRogueKindJobs, job)) {
      return kDaggerClass;
    }
  }
  return weapon_class;
}

// ReqWeaponNameByClassNum : le nom propre d'une classe, ou rien.
std::string WeaponNameOfClass(const SpriteCatalog& catalog, int weapon_class) {
  if (weapon_class <= 0) {
    return {};
  }
  return TableName(catalog.tables.weapon_sprites, static_cast<std::uint32_t>(weapon_class))
      .value_or(std::string());
}

// Le nom de la FAMILLE d'une classe — une
// classe étendue, au-delà de trente, se ramène d'abord à sa famille par
// GetRealWeaponId.
std::string WeaponFamilyName(const SpriteCatalog& catalog, std::uint32_t weapon_class) {
  return WeaponNameOfClass(catalog, static_cast<int>(WeaponFamilyClass(catalog, weapon_class)));
}

std::string HeldPath(std::string_view race, const client_tables::WeaponSpriteClass& sprite_class,
                     std::uint8_t sex, std::string_view name, std::string_view trail) {
  return Join({kSpriteRoot, race, kSeparator, sprite_class.folder, kSeparator, sprite_class.prefix,
               "_", SexToken(sex), name, trail});
}

// Au-delà de la limite des formes, la question
// porte sur un OBJET, et la réponse ne dépend que du métier.
bool DrawsShield(std::uint32_t job, std::uint32_t form_or_item) {
  if (form_or_item > kShieldFormLimit) {
    return !Listed(kNoItemShieldJobs, job) && !WithinAny(kNoItemShieldRanges, job);
  }
  if (Listed(kAllShieldFormsJobs, job)) {
    return form_or_item >= kGuardForm && form_or_item <= kMirrorShieldForm;
  }
  if (Listed(kGuardAndBucklerJobs, job)) {
    return form_or_item == kGuardForm || form_or_item == kBucklerForm;
  }
  if (Listed(kGuardOnlyJobs, job) || Listed(kGuardOnlyHyperNovice, job)) {
    return form_or_item == kGuardForm;
  }
  return false;
}

std::string_view ShieldName(std::uint32_t form) {
  const auto& names = client_tables::kShieldNames;
  return form < names.size() ? names[form] : names.front();
}

// Sur une monture cash-shop comme dans un Madogear, les mains se vident.
bool HoldsNothing(const ActorLook& look) {
  return !look.player || look.cash_mount || IsMadogearJob(BodyClass(look)) ||
         (look.weapon == 0 && look.shield == 0);
}

// Le numéro que portent le dossier et le nom d'une tenue alternative. Le client
// n'en connaît qu'un pour un corps d'acteur : les dossiers costume_2 à
// costume_4 de l'installation ne portent que l'armure du meister, qui se
// choisit autrement (le corps MEISTER_MADOGEAR1 y est remplacé selon la
// variante demandée).
constexpr int kAlternateOutfitNumber = 1;

// LA CLASSE D'UN WARP, ET ELLE N'A PAS DE SPRITE.
//
// 🔴 LE CLIENT N'EN DESSINE AUCUN : il pose un EFFET. Un warp (classe 45) pose
// l'effet 321 à son apparition. Voir `app/effects/effect_warp_view.cpp`.
//
// ⚠ ET CE VIDE N'EST PAS UN OUBLI, il est le seul choix juste. La table du
// client nomme cette classe « 1_ETC_01 », dont le sprite EXISTE et montre un
// villageois : le laisser passer poserait un vieil homme à chaque point de
// téléportation. Nous y mettions le sprite du PORTAIL (classe 10007), un anneau
// orangé à colonne montante — une invention, que rien dans le client ne
// justifie.
//
// Le serveur donne cette classe à tous ses warps (JT_WARPNPC, src/map/npc.hpp).
constexpr std::uint32_t kWarpClass = 45;

}  // namespace

bool IsPlayerClass(std::uint32_t job) {
  const auto& names = client_tables::kPlayerBodyNames;
  const auto found = std::ranges::lower_bound(names, job, {}, &client_tables::PlayerBodyName::job);
  return found != names.end() && found->job == job;
}

bool IsHomunculusJob(std::uint32_t job) {
  return Within(kHomunculusClasses, job) && !Within(kHumanMercenaryClasses, job);
}

std::string NonPlayerResourceName(const SpriteCatalog& catalog, std::uint32_t job) {
  return NonPlayerName(catalog, job);
}

bool DrawnAsModel(const SpriteCatalog& catalog, const ActorLook& look) {
  if (look.item || look.player || look.job == kWarpClass) {
    return false;
  }
  return IsModelName(NonPlayerName(catalog, look.job));
}

std::optional<std::string> BodySpritePath(const SpriteCatalog& catalog, const ActorLook& look) {
  if (look.item) {
    // Un objet au sol se dessine par sa ressource, sous le dossier des objets.
    const std::string_view named = catalog.items.ResourceOf(look.job);
    if (named.empty()) {
      return std::nullopt;
    }
    auto resource = vfs::Cp949ToUtf8(named);
    if (!resource) {
      return std::nullopt;
    }
    return Join({kSpriteRoot, kItemFolder, kSeparator, *resource});
  }
  if (look.player) {
    const std::string_view sex = SexToken(look.sex);
    const std::string_view name = catalog_internal::ShownBodyName(look);
    std::string path = Join({kSpriteRoot, RaceFolder(BodyClass(look)), kSeparator, kBodyFolder,
                             kSeparator, sex, kSeparator});
    if (WearsAlternateOutfit(look)) {
      const std::string number = std::to_string(kAlternateOutfitNumber);
      path += Join({kCostumeFolderPrefix, number, kSeparator, name, "_", sex, "_", number});
    } else {
      path += Join({name, "_", sex});
    }
    return path;
  }

  // UN WARP NE REND AUCUN CHEMIN : son image est un effet, pas un sprite.
  if (look.job == kWarpClass) {
    return std::nullopt;
  }
  const std::string name = NonPlayerName(catalog, look.job);
  if (IsModelName(name)) {
    return std::nullopt;
  }
  const std::uint32_t drawn = look.job;
  const std::string_view folder = NonPlayerFolder(drawn);
  if (folder == kHumanRace) {
    return Join({kSpriteRoot, folder, kSeparator, kBodyFolder, kSeparator, name});
  }
  return Join({kSpriteRoot, folder, kSeparator, name});
}

std::vector<std::string> BodySpritePaths(const SpriteCatalog& catalog, const ActorLook& look) {
  std::optional<std::string> path = BodySpritePath(catalog, look);
  if (!path.has_value()) {
    return {};
  }
  std::vector<std::string> candidates{std::move(*path)};
  if (WearsAlternateOutfit(look)) {
    // Une tenue alternative que l'installation n'a pas ne doit pas effacer son
    // porteur : le corps ordinaire du métier qu'elle habille prend le relais.
    ActorLook ordinary = look;
    ordinary.body_style = static_cast<std::uint16_t>(BodyClass(look));
    if (std::optional<std::string> plain = BodySpritePath(catalog, ordinary); plain.has_value()) {
      candidates.push_back(std::move(*plain));
    }
  }
  return candidates;
}

std::string HeadSpritePath(const ActorLook& look) {
  const std::string_view sex = SexToken(look.sex);
  return Join({kSpriteRoot, RaceFolder(look.job), kSeparator, kHeadFolder, kSeparator, sex,
               kSeparator, HairFileNumber(look), "_", sex});
}

std::vector<std::string> HeadgearSpritePaths(const SpriteCatalog& catalog, const ActorLook& look,
                                             std::uint16_t accessory) {
  const std::optional<std::string> name = TableName(catalog.tables.headgear_sprites, accessory);
  if (!name.has_value()) {
    return {};
  }
  const std::string_view sex = SexToken(look.sex);
  std::vector<std::string> candidates;
  if (IsDoram(look.job)) {
    candidates.push_back(Join({kSpriteRoot, kHeadgearFolder, kSeparator, sex, kDoramHeadgearSuffix,
                               kSeparator, sex, *name}));
  }
  candidates.push_back(
      Join({kSpriteRoot, kHeadgearFolder, kSeparator, sex, kSeparator, sex, *name}));
  return candidates;
}

std::vector<std::string> WeaponSpritePaths(const SpriteCatalog& catalog, const ActorLook& look) {
  if (HoldsNothing(look)) {
    return {};
  }
  const client_tables::WeaponSpriteClass sprite_class = WeaponSpriteClassOf(BodyClass(look));
  std::vector<std::string> candidates;

  // La main gauche tient une arme : le couple se dessine en une pièce, nommée
  // par sa famille.
  if (WeaponClassOf(catalog, look.shield) > 0) {
    std::uint32_t pair = DualWieldClass(look.weapon, look.shield);
    if (!Listed(kDualWieldJobs, look.job) &&
        (pair == 0 || pair == kTwoHandAxeClass || pair == kMaceClass)) {
      pair = kSwordClass;
    }
    const std::string family = WeaponFamilyName(catalog, pair);
    if (!family.empty()) {
      candidates.push_back(HeldPath(kHumanRace, sprite_class, look.sex, family, {}));
    }
    return candidates;
  }

  // Le nom propre de la classe d'abord — c'est lui qui distingue une arme à
  // sprite dédié, nommée par son identifiant — puis celui de sa famille. Le
  // client cherche la race pour le premier et l'oublie pour le second.
  const int weapon_class = WeaponClassOf(catalog, look.weapon);
  const std::string own_name = WeaponNameOfClass(catalog, weapon_class);
  if (!own_name.empty()) {
    candidates.push_back(
        HeldPath(RaceFolder(BodyClass(look)), sprite_class, look.sex, own_name, {}));
  }
  const bool knuckle_less = weapon_class == static_cast<int>(kKnuckleClass) &&
                            (Listed(kKnuckleLessJobs, look.job) || look.job == kCardinal);
  if (weapon_class > 0 && look.weapon != kItemWithoutFamilyFallback && !knuckle_less) {
    const std::string family = WeaponFamilyName(
        catalog, DrawnWeaponFamily(look.job, static_cast<std::uint32_t>(weapon_class)));
    std::string path = HeldPath(kHumanRace, sprite_class, look.sex, family, {});
    if (!family.empty() && (candidates.empty() || candidates.front() != path)) {
      candidates.push_back(std::move(path));
    }
  }
  return candidates;
}

std::vector<std::string> WeaponTrailPaths(const SpriteCatalog& catalog, const ActorLook& look) {
  // LE MADOGEAR PORTE LA LUEUR DE SA MACHINE, arme en main ou non.
  if (look.player && !look.cash_mount && IsMadogearJob(BodyClass(look))) {
    client_tables::WeaponSpriteClass machine = WeaponSpriteClassOf(BodyClass(look));
    machine.prefix = catalog_internal::MadogearSuitName(machine.prefix, look.madogear_suit);
    return {HeldPath(kHumanRace, machine, look.sex, {}, kWeaponTrailSuffix)};
  }
  if (HoldsNothing(look)) {
    return {};
  }
  const client_tables::WeaponSpriteClass sprite_class = WeaponSpriteClassOf(BodyClass(look));
  std::vector<std::string> candidates;

  if (WeaponClassOf(catalog, look.shield) > 0) {
    std::uint32_t pair = DualWieldClass(look.weapon, look.shield);
    if (!Listed(kDualWieldJobs, look.job) &&
        (pair == 0 || pair == kTwoHandAxeClass || pair == kMaceClass)) {
      pair = kSwordClass;
    }
    const std::string family = WeaponFamilyName(catalog, pair);
    if (Listed(kTrailClasses, pair) && !family.empty()) {
      candidates.push_back(
          HeldPath(kHumanRace, sprite_class, look.sex, family, kWeaponTrailSuffix));
    }
    return candidates;
  }

  // Par le nom propre, puis par la famille — la traînée ne connaît pas la race.
  const int weapon_class = WeaponClassOf(catalog, look.weapon);
  const std::string own_name = WeaponNameOfClass(catalog, weapon_class);
  if (!own_name.empty()) {
    candidates.push_back(
        HeldPath(kHumanRace, sprite_class, look.sex, own_name, kWeaponTrailSuffix));
  }
  if (weapon_class > 0) {
    const std::string family = WeaponFamilyName(
        catalog, DrawnWeaponFamily(look.job, static_cast<std::uint32_t>(weapon_class)));
    std::string path = HeldPath(kHumanRace, sprite_class, look.sex, family, kWeaponTrailSuffix);
    if (!family.empty() && (candidates.empty() || candidates.front() != path)) {
      candidates.push_back(std::move(path));
    }
  }
  return candidates;
}

std::vector<std::string> ShieldSpritePaths(const SpriteCatalog& catalog, const ActorLook& look) {
  if (HoldsNothing(look) || look.shield == 0 || WeaponClassOf(catalog, look.shield) > 0) {
    return {};
  }
  const client_tables::WeaponSpriteClass sprite_class = WeaponSpriteClassOf(BodyClass(look));
  const std::string_view sex = SexToken(look.sex);
  const std::string stem = Join({kSpriteRoot, kShieldFolder, kSeparator, sprite_class.folder,
                                 kSeparator, sprite_class.prefix, "_", sex});
  std::vector<std::string> candidates;
  // La variante propre à l'OBJET, quand le métier en dessine.
  if (DrawsShield(look.job, look.shield)) {
    candidates.push_back(Join({stem, "_", std::to_string(look.shield), kShieldSuffix}));
  }
  // La forme générique, pour les métiers qui la savent dessiner.
  const std::uint16_t form = ItemView(catalog, look.shield);
  const std::string_view name = ShieldName(form);
  if (DrawsShield(look.job, form) && !name.empty()) {
    candidates.push_back(Join({stem, name}));
  }
  return candidates;
}

std::string BodyPalettePath(const ActorLook& look) {
  if (look.body_palette <= 0) {
    return {};
  }
  const std::string number = std::to_string(catalog_internal::BodyPaletteFileNumber(look));
  // Une tenue alternative cherche sa palette dans son dossier, suffixée de son
  // numéro chez l'humain ; l'installation n'en porte aucune, et rien ne se teint.
  const std::string outfit = std::to_string(kAlternateOutfitNumber);
  const bool dressed = WearsAlternateOutfit(look);
  const std::string folder = dressed ? Join({kCostumeFolderPrefix, outfit, kSeparator}) : "";
  if (IsDoram(look.job)) {
    return Join({kPaletteRoot, kDoramRace, kSeparator, folder, kBodyPalettePrefix, number,
                 kPaletteExtension});
  }
  return Join({kPaletteRoot, kBodyPaletteFolder, kSeparator, folder, kBodyPalettePrefix, number,
               dressed ? "_" + outfit : "", kPaletteExtension});
}

std::string HairPalettePath(const ActorLook& look) {
  if (look.hair_palette <= 0) {
    return {};
  }
  const std::string number = std::to_string(look.hair_palette);
  if (IsDoram(look.job)) {
    return Join({kPaletteRoot, kDoramRace, kSeparator, kHairPaletteFolder, kSeparator,
                 kHairPalettePrefix, number, kPaletteExtension});
  }
  return Join({kPaletteRoot, kHairPaletteFolder, kSeparator, kHairPalettePrefix, number,
               kPaletteExtension});
}

std::vector<ActorPieceFiles> ActorFiles(const SpriteCatalog& catalog, const ActorLook& look) {
  std::vector<ActorPieceFiles> files;
  std::vector<std::string> body = BodySpritePaths(catalog, look);
  if (body.empty()) {
    return files;
  }
  std::string body_act = PetAccessoryAnimationPath(catalog, look, body.front());
  files.push_back(ActorPieceFiles{ActorPart::kBody, std::move(body), BodyPalettePath(look), false,
                                  std::move(body_act)});
  if (!look.player) {
    // ⚠ UN OBJET AU SOL PORTE UN NUMÉRO D'OBJET, qui peut tomber dans la plage
    // des mercenaires : il ne s'habille pas.
    if (!look.item) {
      catalog_internal::AppendMercenaryPieces(look.job, files);
    }
    return files;
  }
  files.push_back(catalog_internal::HeadPieceFiles(look));

  constexpr std::array<ActorPart, 3> kHeadgearParts = {
      ActorPart::kHeadgearLow, ActorPart::kHeadgearMiddle, ActorPart::kHeadgearTop};
  for (std::size_t rank = 0; rank < kHeadgearParts.size(); ++rank) {
    if (look.headgears[rank] == 0) {
      continue;
    }
    std::vector<std::string> candidates = HeadgearSpritePaths(catalog, look, look.headgears[rank]);
    if (candidates.empty()) {
      continue;
    }
    files.push_back(ActorPieceFiles{kHeadgearParts[rank], std::move(candidates), {}});
  }

  // Ce que les mains tiennent : le client sonde, et ne dit rien quand le
  // fichier manque.
  const auto held = [&files](ActorPart part, std::vector<std::string> candidates) {
    if (!candidates.empty()) {
      files.push_back(ActorPieceFiles{part, std::move(candidates), {}, true});
    }
  };
  held(ActorPart::kWeapon, WeaponSpritePaths(catalog, look));
  held(ActorPart::kWeaponTrail, WeaponTrailPaths(catalog, look));
  held(ActorPart::kShield, ShieldSpritePaths(catalog, look));
  // La cape, que le client sonde de même.
  if (std::optional<ActorPieceFiles> garment = GarmentFiles(catalog, look); garment.has_value()) {
    files.push_back(std::move(*garment));
  }
  return files;
}

namespace catalog_internal {

bool IsDoram(std::uint32_t job) { return Listed(kDoramJobs, job); }

std::string_view SexToken(std::uint8_t sex) { return sex != 0 ? kMaleToken : kFemaleToken; }

bool IsDualWielder(std::uint32_t job) { return Listed(kDualWieldJobs, job); }

int WeaponClassOf(const SpriteCatalog& catalog, std::uint32_t item) {
  if (WithinAny(kShieldItems, item)) {
    return -1;
  }
  return ItemView(catalog, item);
}

std::uint32_t DualWieldClass(std::uint32_t right, std::uint32_t left) {
  const std::uint32_t first = right != 0 ? right : left;
  const std::uint32_t second = right != 0 ? left : 0;
  if (first == 0) {
    return 0;
  }
  if (WithinAny(kSwordItems, first)) {
    if (WithinAny(kSwordItems, second)) {
      return kSwordPairClass;
    }
    if (WithinAny(kDaggerItems, second)) {
      return kDaggerSwordClass;
    }
    if (WithinAny(kAxeItems, second)) {
      return kSwordAxeClass;
    }
    return kSwordClass;
  }
  if (WithinAny(kDaggerItems, first)) {
    if (WithinAny(kSwordItems, second)) {
      return kDaggerSwordClass;
    }
    if (WithinAny(kDaggerItems, second)) {
      return kDaggerPairClass;
    }
    if (WithinAny(kAxeItems, second)) {
      return kDaggerAxeClass;
    }
    return kDaggerClass;
  }
  if (WithinAny(kAxeItems, first)) {
    return WithinAny(kAxeItems, second) ? kAxePairClass : kAxeClass;
  }
  return 0;
}

std::uint32_t WeaponFamilyClass(const SpriteCatalog& catalog, std::uint32_t weapon_class) {
  const std::uint32_t* family = FindSpriteEntry(catalog.tables.weapon_families, weapon_class);
  return family != nullptr ? *family : weapon_class;
}

std::string Join(std::initializer_list<std::string_view> parts) {
  std::string joined;
  for (const std::string_view part : parts) {
    joined += part;
  }
  return joined;
}

std::string_view PlayerBodyName(std::uint32_t job) {
  const auto& names = client_tables::kPlayerBodyNames;
  const auto found = std::ranges::lower_bound(names, job, {}, &client_tables::PlayerBodyName::job);
  if (found == names.end() || found->job != job) {
    return client_tables::kDefaultPlayerBodyName;
  }
  return found->name;
}

std::optional<std::string> TableName(const std::unordered_map<std::uint32_t, std::string>& table,
                                     std::uint32_t id) {
  const std::string* name = FindSpriteEntry(table, id);
  if (name == nullptr) {
    return std::nullopt;
  }
  return *name;
}

}  // namespace catalog_internal

}  // namespace ro::render
