// LES CLASSES QUE « @job » ACCEPTE SUR CE SERVEUR.
//
// 🔴 LES MÉTIERS VIENNENT DU SERVEUR : ceux de `ServerJobChangeTargets()`, la
// table que `scripts/gen_server_jobchange.py` tire de la règle de la commande —
// la même que propose la complétion de « @jobchange » dans la discussion. Une
// classe que le serveur refuse ne se propose pas : elle n'offrirait qu'un refus.
//
// ⚠ ICI NE VIVENT QUE LA FAMILLE ET L'ORDRE de présentation. Une classe d'ici
// que la table du serveur ignore ne se montre pas ; l'essai exige que les deux
// listes portent exactement les mêmes numéros.
//
// 🔴 ET ELLE VIT DANS `app`, PAS DANS LA FENÊTRE. C'est une connaissance du
// SERVEUR : quels identifiants il accepte. `ui` en reçoit une vue en lecture,
// comme pour tout le reste.
//
// ⚠ LE NOM VIENT DE `job_names.yml` ; celui du serveur le suit quand il
// diffère, et le remplace quand la table ne nomme pas la classe.

#include "character/staff_jobs.h"

#include <algorithm>

#include "ro/core/ascii.h"

#include "chat/server_jobchange.h"

namespace moonclient {
namespace {

using ro::ui::StaffJobFamily;

struct JobEntry {
  std::uint32_t id;
  StaffJobFamily family;
};

constexpr std::array<JobEntry, 146> kJobs = {{
    JobEntry{0, StaffJobFamily::kNovice},          JobEntry{1, StaffJobFamily::kNovice},
    JobEntry{2, StaffJobFamily::kNovice},          JobEntry{3, StaffJobFamily::kNovice},
    JobEntry{4, StaffJobFamily::kNovice},          JobEntry{5, StaffJobFamily::kNovice},
    JobEntry{6, StaffJobFamily::kNovice},          JobEntry{7, StaffJobFamily::kSecond},
    JobEntry{8, StaffJobFamily::kSecond},          JobEntry{9, StaffJobFamily::kSecond},
    JobEntry{10, StaffJobFamily::kSecond},         JobEntry{11, StaffJobFamily::kSecond},
    JobEntry{12, StaffJobFamily::kSecond},         JobEntry{14, StaffJobFamily::kSecond},
    JobEntry{15, StaffJobFamily::kSecond},         JobEntry{16, StaffJobFamily::kSecond},
    JobEntry{17, StaffJobFamily::kSecond},         JobEntry{18, StaffJobFamily::kSecond},
    JobEntry{19, StaffJobFamily::kSecond},         JobEntry{20, StaffJobFamily::kSecond},
    JobEntry{4001, StaffJobFamily::kTransFirst},   JobEntry{4002, StaffJobFamily::kTransFirst},
    JobEntry{4003, StaffJobFamily::kTransFirst},   JobEntry{4004, StaffJobFamily::kTransFirst},
    JobEntry{4005, StaffJobFamily::kTransFirst},   JobEntry{4006, StaffJobFamily::kTransFirst},
    JobEntry{4007, StaffJobFamily::kTransFirst},   JobEntry{4008, StaffJobFamily::kTransSecond},
    JobEntry{4009, StaffJobFamily::kTransSecond},  JobEntry{4010, StaffJobFamily::kTransSecond},
    JobEntry{4011, StaffJobFamily::kTransSecond},  JobEntry{4012, StaffJobFamily::kTransSecond},
    JobEntry{4013, StaffJobFamily::kTransSecond},  JobEntry{4015, StaffJobFamily::kTransSecond},
    JobEntry{4016, StaffJobFamily::kTransSecond},  JobEntry{4017, StaffJobFamily::kTransSecond},
    JobEntry{4018, StaffJobFamily::kTransSecond},  JobEntry{4019, StaffJobFamily::kTransSecond},
    JobEntry{4020, StaffJobFamily::kTransSecond},  JobEntry{4021, StaffJobFamily::kTransSecond},
    JobEntry{4054, StaffJobFamily::kThird},        JobEntry{4055, StaffJobFamily::kThird},
    JobEntry{4056, StaffJobFamily::kThird},        JobEntry{4057, StaffJobFamily::kThird},
    JobEntry{4058, StaffJobFamily::kThird},        JobEntry{4059, StaffJobFamily::kThird},
    JobEntry{4066, StaffJobFamily::kThird},        JobEntry{4067, StaffJobFamily::kThird},
    JobEntry{4068, StaffJobFamily::kThird},        JobEntry{4069, StaffJobFamily::kThird},
    JobEntry{4070, StaffJobFamily::kThird},        JobEntry{4071, StaffJobFamily::kThird},
    JobEntry{4072, StaffJobFamily::kThird},        JobEntry{4060, StaffJobFamily::kThirdTrans},
    JobEntry{4061, StaffJobFamily::kThirdTrans},   JobEntry{4062, StaffJobFamily::kThirdTrans},
    JobEntry{4063, StaffJobFamily::kThirdTrans},   JobEntry{4064, StaffJobFamily::kThirdTrans},
    JobEntry{4065, StaffJobFamily::kThirdTrans},   JobEntry{4073, StaffJobFamily::kThirdTrans},
    JobEntry{4074, StaffJobFamily::kThirdTrans},   JobEntry{4075, StaffJobFamily::kThirdTrans},
    JobEntry{4076, StaffJobFamily::kThirdTrans},   JobEntry{4077, StaffJobFamily::kThirdTrans},
    JobEntry{4078, StaffJobFamily::kThirdTrans},   JobEntry{4079, StaffJobFamily::kThirdTrans},
    JobEntry{4252, StaffJobFamily::kFourth},       JobEntry{4253, StaffJobFamily::kFourth},
    JobEntry{4254, StaffJobFamily::kFourth},       JobEntry{4255, StaffJobFamily::kFourth},
    JobEntry{4256, StaffJobFamily::kFourth},       JobEntry{4257, StaffJobFamily::kFourth},
    JobEntry{4258, StaffJobFamily::kFourth},       JobEntry{4259, StaffJobFamily::kFourth},
    JobEntry{4260, StaffJobFamily::kFourth},       JobEntry{4261, StaffJobFamily::kFourth},
    JobEntry{4262, StaffJobFamily::kFourth},       JobEntry{4263, StaffJobFamily::kFourth},
    JobEntry{4264, StaffJobFamily::kFourth},       JobEntry{23, StaffJobFamily::kExpanded},
    JobEntry{24, StaffJobFamily::kExpanded},       JobEntry{25, StaffJobFamily::kExpanded},
    JobEntry{4045, StaffJobFamily::kExpanded},     JobEntry{4046, StaffJobFamily::kExpanded},
    JobEntry{4047, StaffJobFamily::kExpanded},     JobEntry{4049, StaffJobFamily::kExpanded},
    JobEntry{4050, StaffJobFamily::kExpanded},     JobEntry{4051, StaffJobFamily::kExpanded},
    JobEntry{4052, StaffJobFamily::kExpanded},     JobEntry{4190, StaffJobFamily::kExpanded},
    JobEntry{4191, StaffJobFamily::kExpanded},     JobEntry{4211, StaffJobFamily::kExpanded},
    JobEntry{4212, StaffJobFamily::kExpanded},     JobEntry{4215, StaffJobFamily::kExpanded},
    JobEntry{4218, StaffJobFamily::kExpanded},     JobEntry{4239, StaffJobFamily::kExpanded},
    JobEntry{4240, StaffJobFamily::kExpanded},     JobEntry{4302, StaffJobFamily::kExpanded},
    JobEntry{4303, StaffJobFamily::kExpanded},     JobEntry{4304, StaffJobFamily::kExpanded},
    JobEntry{4305, StaffJobFamily::kExpanded},     JobEntry{4306, StaffJobFamily::kExpanded},
    JobEntry{4023, StaffJobFamily::kBabyFirst},    JobEntry{4024, StaffJobFamily::kBabyFirst},
    JobEntry{4025, StaffJobFamily::kBabyFirst},    JobEntry{4026, StaffJobFamily::kBabyFirst},
    JobEntry{4027, StaffJobFamily::kBabyFirst},    JobEntry{4028, StaffJobFamily::kBabyFirst},
    JobEntry{4029, StaffJobFamily::kBabyFirst},    JobEntry{4030, StaffJobFamily::kBabySecond},
    JobEntry{4031, StaffJobFamily::kBabySecond},   JobEntry{4032, StaffJobFamily::kBabySecond},
    JobEntry{4033, StaffJobFamily::kBabySecond},   JobEntry{4034, StaffJobFamily::kBabySecond},
    JobEntry{4035, StaffJobFamily::kBabySecond},   JobEntry{4037, StaffJobFamily::kBabySecond},
    JobEntry{4038, StaffJobFamily::kBabySecond},   JobEntry{4039, StaffJobFamily::kBabySecond},
    JobEntry{4040, StaffJobFamily::kBabySecond},   JobEntry{4041, StaffJobFamily::kBabySecond},
    JobEntry{4042, StaffJobFamily::kBabySecond},   JobEntry{4043, StaffJobFamily::kBabySecond},
    JobEntry{4096, StaffJobFamily::kBabyThird},    JobEntry{4097, StaffJobFamily::kBabyThird},
    JobEntry{4098, StaffJobFamily::kBabyThird},    JobEntry{4099, StaffJobFamily::kBabyThird},
    JobEntry{4100, StaffJobFamily::kBabyThird},    JobEntry{4101, StaffJobFamily::kBabyThird},
    JobEntry{4102, StaffJobFamily::kBabyThird},    JobEntry{4103, StaffJobFamily::kBabyThird},
    JobEntry{4104, StaffJobFamily::kBabyThird},    JobEntry{4105, StaffJobFamily::kBabyThird},
    JobEntry{4106, StaffJobFamily::kBabyThird},    JobEntry{4107, StaffJobFamily::kBabyThird},
    JobEntry{4108, StaffJobFamily::kBabyThird},    JobEntry{4220, StaffJobFamily::kBabyExpanded},
    JobEntry{4222, StaffJobFamily::kBabyExpanded}, JobEntry{4223, StaffJobFamily::kBabyExpanded},
    JobEntry{4224, StaffJobFamily::kBabyExpanded}, JobEntry{4225, StaffJobFamily::kBabyExpanded},
    JobEntry{4226, StaffJobFamily::kBabyExpanded}, JobEntry{4227, StaffJobFamily::kBabyExpanded},
    JobEntry{4228, StaffJobFamily::kBabyExpanded}, JobEntry{4229, StaffJobFamily::kBabyExpanded},
    JobEntry{4241, StaffJobFamily::kBabyExpanded}, JobEntry{4242, StaffJobFamily::kBabyExpanded},
}};

}  // namespace

std::span<const ro::ui::StaffJobView> StaffJobs::Rebuild(const ro::vfs::JobNames* names) {
  if (!rows_.empty()) {
    return rows_;
  }
  // ⚠ LES DEUX TAMPONS SE REMPLISSENT ENTIÈREMENT AVANT QU'ON Y POINTE : une vue
  // posée sur un `std::string` que la croissance du vecteur déplacerait pendrait.
  const auto targets = ServerJobChangeTargets();
  std::vector<const JobEntry*> accepted;
  std::vector<std::string_view> server_names;
  for (const JobEntry& one : kJobs) {
    const auto found = std::ranges::find(targets, one.id, &ServerJobChangeTarget::job_id);
    if (found != targets.end()) {
      accepted.push_back(&one);
      server_names.push_back(found->server_name);
    }
  }
  labels_.reserve(accepted.size());
  for (std::size_t rank = 0; rank < accepted.size(); ++rank) {
    const std::string_view named =
        names != nullptr ? names->Find(accepted[rank]->id) : std::string_view();
    labels_.emplace_back(named.empty() ? server_names[rank] : named);
  }
  rows_.reserve(accepted.size());
  for (std::size_t rank = 0; rank < accepted.size(); ++rank) {
    const std::string_view detail = ro::core::AsciiEqualsFolded(labels_[rank], server_names[rank])
                                        ? std::string_view()
                                        : server_names[rank];
    rows_.push_back(ro::ui::StaffJobView{.id = accepted[rank]->id,
                                         .label = labels_[rank],
                                         .detail = detail,
                                         .family = accepted[rank]->family});
  }
  return rows_;
}

}  // namespace moonclient
