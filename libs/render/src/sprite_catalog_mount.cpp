// LE CORPS D'UN JOUEUR : le style de corps du serveur, la monture cash-shop et
// la combinaison de Madogear.
//
// Le métier montré (`ro/render/shown_job.h`) a déjà choisi le Peco, le dragon,
// le loup ou la machine. Il reste ce que le métier ne dit pas : le style de
// corps, qui peut prêter au joueur le corps d'un AUTRE métier ; la monture
// cash-shop, un état qui prête la classe montée ; et la combinaison, qui
// habille le corps du Madogear d'une armure.

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/core/sorted_table.h"
#include "ro/render/shown_job.h"
#include "ro/render/sprite_catalog.h"

#include "client_tables.h"
#include "sprite_catalog_internal.h"

namespace ro::render::catalog_internal {
namespace {

// La machine, et l'armure qui la remplace sous la combinaison.
constexpr std::string_view kMadogearMachine = "마도기어";
constexpr std::string_view kMadogearArmor = "마도아머";
constexpr std::string_view kMeisterMachine = "MEISTER_MADOGEAR1";
constexpr std::string_view kMeisterArmor = "MEISTER_MADOGEAR2";

// La plage des classes de TENUE ALTERNATIVE.
//
// Le style de corps qu'annonce le serveur n'est pas un numéro de costume :
// c'est une CLASSE, et le serveur y met la classe du personnage lui-même quand
// il n'en porte aucune. Le client ne cherche le dossier des costumes que dans
// cette plage : dix-huit valeurs à partir de 4332. Hors d'elle, le style se lit
// comme un métier, voir `BodyClass`.
constexpr std::uint16_t kFirstAlternateOutfitClass = 4332;
constexpr std::uint16_t kAlternateOutfitClassCount = 18;

// Un corps et celui qui le remplace, triés sur le premier.
struct BodySwap {
  std::uint32_t from;
  std::uint32_t to;
};

// LES TENUES ALTERNATIVES, et le métier dont chacune habille le corps. Une
// tenue hors de cette table se lit comme un métier.
constexpr std::array<BodySwap, 13> kAlternateOutfitJobs = {{{4332, 4060},
                                                            {4333, 4064},
                                                            {4334, 4065},
                                                            {4335, 4061},
                                                            {4336, 4063},
                                                            {4337, 4062},
                                                            {4338, 4073},
                                                            {4339, 4078},
                                                            {4340, 4079},
                                                            {4341, 4074},
                                                            {4342, 4077},
                                                            {4343, 4075},
                                                            {4344, 4076}}};

// LES CORPS MONTÉS qui prêtent leur monture au corps emprunté : Peco, griffon,
// loup et dragon VERT. Un dragon d'une autre couleur n'en prête aucune.
constexpr std::array<std::uint32_t, 18> kMountedBodies = {13,   21,   4014, 4022, 4036, 4044,
                                                          4080, 4081, 4082, 4083, 4084, 4085,
                                                          4109, 4110, 4111, 4278, 4280, 4281};
// Le corps à pied, et le même en selle.
constexpr std::array<BodySwap, 18> kMountOf = {{{7, 13},
                                                {14, 21},
                                                {4008, 4014},
                                                {4015, 4022},
                                                {4030, 4036},
                                                {4037, 4044},
                                                {4054, 4080},
                                                {4056, 4084},
                                                {4060, 4081},
                                                {4062, 4085},
                                                {4066, 4082},
                                                {4073, 4083},
                                                {4096, 4109},
                                                {4098, 4111},
                                                {4102, 4110},
                                                {4252, 4280},
                                                {4257, 4278},
                                                {4258, 4281}}};

// LES CORPS DE MADOGEAR, et la machine de chaque corps à pied.
constexpr std::array<std::uint32_t, 4> kMadogearBodies = {4086, 4087, 4112, 4279};
constexpr std::array<BodySwap, 4> kMadogearOf = {
    {{4058, 4086}, {4064, 4087}, {4100, 4112}, {4253, 4279}}};

std::uint32_t Swapped(std::span<const BodySwap> table, std::uint32_t job) {
  const BodySwap* found = core::RowWithKey(table, job, &BodySwap::from);
  return found == nullptr ? job : found->to;
}

const client_tables::CashMountClass* CashMountOf(std::uint32_t job) {
  return core::RowWithKey(client_tables::kCashMountClasses, job,
                          &client_tables::CashMountClass::job);
}

std::uint32_t CashMounted(std::uint32_t job) {
  const client_tables::CashMountClass* mount = CashMountOf(job);
  return mount == nullptr ? job : mount->mounted;
}

}  // namespace

bool WearsAlternateOutfit(const ActorLook& look) {
  return look.body_style >= kFirstAlternateOutfitClass &&
         look.body_style < kFirstAlternateOutfitClass + kAlternateOutfitClassCount;
}

std::uint32_t BodyClass(const ActorLook& look) {
  const std::uint32_t shown = look.job;
  if (!look.player || look.body_style == 0 || IsCostumeJob(shown)) {
    return shown;
  }
  const std::uint32_t body = Swapped(kAlternateOutfitJobs, look.body_style);
  // Le corps du métier lui-même : le métier montré dit déjà sa monture, la
  // couleur du dragon comprise.
  if (body == shown || body == FootJobOf(shown)) {
    return shown;
  }
  if (look.cash_mount) {
    return body;
  }
  // Un métier qui n'a aucune monture prête au corps emprunté celle du shop.
  if (CashMountOf(shown) == nullptr && FootJobOf(shown) == shown) {
    return CashMounted(body);
  }
  if (std::ranges::find(kMountedBodies, shown) != kMountedBodies.end()) {
    return Swapped(kMountOf, body);
  }
  if (std::ranges::find(kMadogearBodies, shown) != kMadogearBodies.end()) {
    return Swapped(kMadogearOf, body);
  }
  return body;
}

std::uint32_t BodyJob(const ActorLook& look) {
  const std::uint32_t body = BodyClass(look);
  return look.cash_mount ? CashMounted(body) : body;
}

std::string_view MadogearSuitName(std::string_view machine_name, bool suit) {
  if (!suit) {
    return machine_name;
  }
  if (machine_name == kMadogearMachine) {
    return kMadogearArmor;
  }
  if (machine_name == kMeisterMachine) {
    return kMeisterArmor;
  }
  return machine_name;
}

std::string_view ShownBodyName(const ActorLook& look) {
  return MadogearSuitName(PlayerBodyName(BodyJob(look)), look.madogear_suit);
}

}  // namespace ro::render::catalog_internal
