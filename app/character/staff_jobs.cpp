// LES CLASSES QUE « @job » ACCEPTE SUR CE SERVEUR.
//
// 🔴 UN CATALOGUE, ET IL VIENT DU SERVEUR. La liste est celle que la commande
// publie dans sa propre aide (`conf/atcommands.yml`, entrée « jobchange ») :
// c'est la seule qui fasse foi. Elle omet déjà les classes FANTÔMES — Knight2 et
// Crusader2, qui sont des montures, les tenues de mariage, de Noël, d'été et le
// hanbok — que `ACMD_FUNC(jobchange)` refuse nommément. Les proposer n'offrirait
// qu'un refus.
//
// 🔴 ET ELLE VIT DANS `app`, PAS DANS LA FENÊTRE. C'est une connaissance du
// SERVEUR : quels identifiants il accepte. `ui` en reçoit une vue en lecture,
// comme pour tout le reste.
//
// ⚠ LE NOM VIENT DE `job_names.yml`, PAS D'ICI. Le repli ci-dessous ne sert que
// pour les classes que la table ne nomme pas — et il ne se traduit pas, c'est le
// terme du jeu.

#include "character/staff_jobs.h"

namespace moonclient {
namespace {

using ro::ui::StaffJobFamily;

struct JobEntry {
  std::uint32_t id;
  StaffJobFamily family;
  const char* fallback;
};

constexpr std::array<JobEntry, 145> kJobs = {{
    JobEntry{0, StaffJobFamily::kNovice, "Novice"},
    JobEntry{1, StaffJobFamily::kNovice, "Swordman"},
    JobEntry{2, StaffJobFamily::kNovice, "Magician"},
    JobEntry{3, StaffJobFamily::kNovice, "Archer"},
    JobEntry{4, StaffJobFamily::kNovice, "Acolyte"},
    JobEntry{5, StaffJobFamily::kNovice, "Merchant"},
    JobEntry{6, StaffJobFamily::kNovice, "Thief"},
    JobEntry{7, StaffJobFamily::kSecond, "Knight"},
    JobEntry{8, StaffJobFamily::kSecond, "Priest"},
    JobEntry{9, StaffJobFamily::kSecond, "Wizard"},
    JobEntry{10, StaffJobFamily::kSecond, "Blacksmith"},
    JobEntry{11, StaffJobFamily::kSecond, "Hunter"},
    JobEntry{12, StaffJobFamily::kSecond, "Assassin"},
    JobEntry{14, StaffJobFamily::kSecond, "Crusader"},
    JobEntry{15, StaffJobFamily::kSecond, "Monk"},
    JobEntry{16, StaffJobFamily::kSecond, "Sage"},
    JobEntry{17, StaffJobFamily::kSecond, "Rogue"},
    JobEntry{18, StaffJobFamily::kSecond, "Alchemist"},
    JobEntry{19, StaffJobFamily::kSecond, "Bard"},
    JobEntry{20, StaffJobFamily::kSecond, "Dancer"},
    JobEntry{4001, StaffJobFamily::kTransFirst, "Novice High"},
    JobEntry{4002, StaffJobFamily::kTransFirst, "Swordman High"},
    JobEntry{4003, StaffJobFamily::kTransFirst, "Magician High"},
    JobEntry{4004, StaffJobFamily::kTransFirst, "Archer High"},
    JobEntry{4005, StaffJobFamily::kTransFirst, "Acolyte High"},
    JobEntry{4006, StaffJobFamily::kTransFirst, "Merchant High"},
    JobEntry{4007, StaffJobFamily::kTransFirst, "Thief High"},
    JobEntry{4008, StaffJobFamily::kTransSecond, "Lord Knight"},
    JobEntry{4009, StaffJobFamily::kTransSecond, "High Priest"},
    JobEntry{4010, StaffJobFamily::kTransSecond, "High Wizard"},
    JobEntry{4011, StaffJobFamily::kTransSecond, "Whitesmith"},
    JobEntry{4012, StaffJobFamily::kTransSecond, "Sniper"},
    JobEntry{4013, StaffJobFamily::kTransSecond, "Assassin Cross"},
    JobEntry{4015, StaffJobFamily::kTransSecond, "Paladin"},
    JobEntry{4016, StaffJobFamily::kTransSecond, "Champion"},
    JobEntry{4017, StaffJobFamily::kTransSecond, "Professor"},
    JobEntry{4018, StaffJobFamily::kTransSecond, "Stalker"},
    JobEntry{4019, StaffJobFamily::kTransSecond, "Creator"},
    JobEntry{4020, StaffJobFamily::kTransSecond, "Clown"},
    JobEntry{4021, StaffJobFamily::kTransSecond, "Gypsy"},
    JobEntry{4054, StaffJobFamily::kThird, "Rune Knight"},
    JobEntry{4055, StaffJobFamily::kThird, "Warlock"},
    JobEntry{4056, StaffJobFamily::kThird, "Ranger"},
    JobEntry{4057, StaffJobFamily::kThird, "Arch Bishop"},
    JobEntry{4058, StaffJobFamily::kThird, "Mechanic"},
    JobEntry{4059, StaffJobFamily::kThird, "Guillotine Cross"},
    JobEntry{4066, StaffJobFamily::kThird, "Royal Guard"},
    JobEntry{4067, StaffJobFamily::kThird, "Sorcerer"},
    JobEntry{4068, StaffJobFamily::kThird, "Minstrel"},
    JobEntry{4069, StaffJobFamily::kThird, "Wanderer"},
    JobEntry{4070, StaffJobFamily::kThird, "Sura"},
    JobEntry{4071, StaffJobFamily::kThird, "Genetic"},
    JobEntry{4072, StaffJobFamily::kThird, "Shadow Chaser"},
    JobEntry{4060, StaffJobFamily::kThirdTrans, "Rune Knight T"},
    JobEntry{4061, StaffJobFamily::kThirdTrans, "Warlock T"},
    JobEntry{4062, StaffJobFamily::kThirdTrans, "Ranger T"},
    JobEntry{4063, StaffJobFamily::kThirdTrans, "Arch Bishop T"},
    JobEntry{4064, StaffJobFamily::kThirdTrans, "Mechanic T"},
    JobEntry{4065, StaffJobFamily::kThirdTrans, "Guillotine Cross T"},
    JobEntry{4073, StaffJobFamily::kThirdTrans, "Royal Guard T"},
    JobEntry{4074, StaffJobFamily::kThirdTrans, "Sorcerer T"},
    JobEntry{4075, StaffJobFamily::kThirdTrans, "Minstrel T"},
    JobEntry{4076, StaffJobFamily::kThirdTrans, "Wanderer T"},
    JobEntry{4077, StaffJobFamily::kThirdTrans, "Sura T"},
    JobEntry{4078, StaffJobFamily::kThirdTrans, "Genetic T"},
    JobEntry{4079, StaffJobFamily::kThirdTrans, "Shadow Chaser T"},
    JobEntry{4252, StaffJobFamily::kFourth, "Dragon Knight"},
    JobEntry{4253, StaffJobFamily::kFourth, "Meister"},
    JobEntry{4254, StaffJobFamily::kFourth, "Shadow Cross"},
    JobEntry{4255, StaffJobFamily::kFourth, "Arch Mage"},
    JobEntry{4256, StaffJobFamily::kFourth, "Cardinal"},
    JobEntry{4257, StaffJobFamily::kFourth, "Windhawk"},
    JobEntry{4258, StaffJobFamily::kFourth, "Imperial Guard"},
    JobEntry{4259, StaffJobFamily::kFourth, "Biolo"},
    JobEntry{4260, StaffJobFamily::kFourth, "Abyss Chaser"},
    JobEntry{4261, StaffJobFamily::kFourth, "Elemental Master"},
    JobEntry{4262, StaffJobFamily::kFourth, "Inquisitor"},
    JobEntry{4263, StaffJobFamily::kFourth, "Troubadour"},
    JobEntry{4264, StaffJobFamily::kFourth, "Trouvere"},
    JobEntry{23, StaffJobFamily::kExpanded, "Super Novice"},
    JobEntry{24, StaffJobFamily::kExpanded, "Gunslinger"},
    JobEntry{25, StaffJobFamily::kExpanded, "Ninja"},
    JobEntry{4045, StaffJobFamily::kExpanded, "Super Baby"},
    JobEntry{4046, StaffJobFamily::kExpanded, "Taekwon"},
    JobEntry{4047, StaffJobFamily::kExpanded, "Star Gladiator"},
    JobEntry{4049, StaffJobFamily::kExpanded, "Soul Linker"},
    JobEntry{4190, StaffJobFamily::kExpanded, "Ex. Super Novice"},
    JobEntry{4191, StaffJobFamily::kExpanded, "Ex. Super Baby"},
    JobEntry{4211, StaffJobFamily::kExpanded, "Kagerou"},
    JobEntry{4212, StaffJobFamily::kExpanded, "Oboro"},
    JobEntry{4215, StaffJobFamily::kExpanded, "Rebellion"},
    JobEntry{4218, StaffJobFamily::kExpanded, "Summoner"},
    JobEntry{4239, StaffJobFamily::kExpanded, "Star Emperor"},
    JobEntry{4240, StaffJobFamily::kExpanded, "Soul Reaper"},
    JobEntry{4302, StaffJobFamily::kExpanded, "Sky Emperor"},
    JobEntry{4303, StaffJobFamily::kExpanded, "Soul Ascetic"},
    JobEntry{4304, StaffJobFamily::kExpanded, "Shinkiro"},
    JobEntry{4305, StaffJobFamily::kExpanded, "Shiranui"},
    JobEntry{4306, StaffJobFamily::kExpanded, "Night Watch"},
    JobEntry{4307, StaffJobFamily::kExpanded, "Hyper Novice"},
    JobEntry{4308, StaffJobFamily::kExpanded, "Spirit Handler"},
    JobEntry{4023, StaffJobFamily::kBabyFirst, "Baby Novice"},
    JobEntry{4024, StaffJobFamily::kBabyFirst, "Baby Swordman"},
    JobEntry{4025, StaffJobFamily::kBabyFirst, "Baby Magician"},
    JobEntry{4026, StaffJobFamily::kBabyFirst, "Baby Archer"},
    JobEntry{4027, StaffJobFamily::kBabyFirst, "Baby Acolyte"},
    JobEntry{4028, StaffJobFamily::kBabyFirst, "Baby Merchant"},
    JobEntry{4029, StaffJobFamily::kBabyFirst, "Baby Thief"},
    JobEntry{4030, StaffJobFamily::kBabySecond, "Baby Knight"},
    JobEntry{4031, StaffJobFamily::kBabySecond, "Baby Priest"},
    JobEntry{4032, StaffJobFamily::kBabySecond, "Baby Wizard"},
    JobEntry{4033, StaffJobFamily::kBabySecond, "Baby Blacksmith"},
    JobEntry{4034, StaffJobFamily::kBabySecond, "Baby Hunter"},
    JobEntry{4035, StaffJobFamily::kBabySecond, "Baby Assassin"},
    JobEntry{4037, StaffJobFamily::kBabySecond, "Baby Crusader"},
    JobEntry{4038, StaffJobFamily::kBabySecond, "Baby Monk"},
    JobEntry{4039, StaffJobFamily::kBabySecond, "Baby Sage"},
    JobEntry{4040, StaffJobFamily::kBabySecond, "Baby Rogue"},
    JobEntry{4041, StaffJobFamily::kBabySecond, "Baby Alchemist"},
    JobEntry{4042, StaffJobFamily::kBabySecond, "Baby Bard"},
    JobEntry{4043, StaffJobFamily::kBabySecond, "Baby Dancer"},
    JobEntry{4096, StaffJobFamily::kBabyThird, "Baby Rune Knight"},
    JobEntry{4097, StaffJobFamily::kBabyThird, "Baby Warlock"},
    JobEntry{4098, StaffJobFamily::kBabyThird, "Baby Ranger"},
    JobEntry{4099, StaffJobFamily::kBabyThird, "Baby Arch Bishop"},
    JobEntry{4100, StaffJobFamily::kBabyThird, "Baby Mechanic"},
    JobEntry{4101, StaffJobFamily::kBabyThird, "Baby Guillotine Cross"},
    JobEntry{4102, StaffJobFamily::kBabyThird, "Baby Royal Guard"},
    JobEntry{4103, StaffJobFamily::kBabyThird, "Baby Sorcerer"},
    JobEntry{4104, StaffJobFamily::kBabyThird, "Baby Minstrel"},
    JobEntry{4105, StaffJobFamily::kBabyThird, "Baby Wanderer"},
    JobEntry{4106, StaffJobFamily::kBabyThird, "Baby Sura"},
    JobEntry{4107, StaffJobFamily::kBabyThird, "Baby Genetic"},
    JobEntry{4108, StaffJobFamily::kBabyThird, "Baby Shadow Chaser"},
    JobEntry{4220, StaffJobFamily::kBabyExpanded, "Baby Summoner"},
    JobEntry{4222, StaffJobFamily::kBabyExpanded, "Baby Ninja"},
    JobEntry{4223, StaffJobFamily::kBabyExpanded, "Baby Kagerou"},
    JobEntry{4224, StaffJobFamily::kBabyExpanded, "Baby Oboro"},
    JobEntry{4225, StaffJobFamily::kBabyExpanded, "Baby Taekwon"},
    JobEntry{4226, StaffJobFamily::kBabyExpanded, "Baby Star Gladiator"},
    JobEntry{4227, StaffJobFamily::kBabyExpanded, "Baby Soul Linker"},
    JobEntry{4228, StaffJobFamily::kBabyExpanded, "Baby Gunslinger"},
    JobEntry{4229, StaffJobFamily::kBabyExpanded, "Baby Rebellion"},
    JobEntry{4241, StaffJobFamily::kBabyExpanded, "Baby Star Emperor"},
    JobEntry{4242, StaffJobFamily::kBabyExpanded, "Baby Soul Reaper"},
}};

}  // namespace

std::span<const ro::ui::StaffJobView> StaffJobs::Rebuild(const ro::vfs::JobNames* names) {
  if (!rows_.empty()) {
    return rows_;
  }
  // ⚠ LES DEUX TAMPONS SE REMPLISSENT ENTIÈREMENT AVANT QU'ON Y POINTE : une vue
  // posée sur un `std::string` que la croissance du vecteur déplacerait pendrait.
  labels_.reserve(kJobs.size());
  for (const JobEntry& one : kJobs) {
    const std::string_view named = names != nullptr ? names->Find(one.id) : std::string_view();
    labels_.emplace_back(named.empty() ? one.fallback : std::string(named));
  }
  rows_.reserve(kJobs.size());
  for (std::size_t rank = 0; rank < kJobs.size(); ++rank) {
    rows_.push_back(ro::ui::StaffJobView{
        .id = kJobs[rank].id, .label = labels_[rank], .family = kJobs[rank].family});
  }
  return rows_;
}

}  // namespace moonclient
