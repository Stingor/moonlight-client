#include "effects/effect_judge.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <ios>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_case_table.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_table.h"
#include "ro/render/terrain_mesh.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

// L'instant où chaque essai commence, en millisecondes.
//
// ⚠ IL N'EST PAS NUL, ET IL LE FAUT. Le premier `Drawn` d'une troupe ne
// rattrape rien — c'est lui qui pose le battement — et un battement remis à zéro
// vaut « pas encore d'instant ». Partir de zéro ferait donc sauter le premier
// tic de tous les essais sauf le premier.
constexpr std::uint32_t kJudgeStartMs = 1000;

// COMBIEN DE TICS LE JUGE FAIT BATTRE, et le nombre est MESURÉ.
//
// ⚠ IL A VALU VINGT-QUATRE FAUX MUETS. Le juge bornait d'abord sa fenêtre par la
// vie RELEVÉE de l'effet, seize tics au minimum — et une famille infinie n'a
// aucune vie relevée. Or la rafale de quads naît avec une hauteur tirée entre
// zéro et cent, et remonte de 0,15 par tic : il lui faut jusqu'à **667 tics**,
// près de onze secondes, avant que sa première particule ne se peigne. Jugée sur
// seize, elle ne rendait rien, et le rapport l'accusait d'une figure muette.
//
// Sept cents tics couvrent ce cas — le plus lent qui soit relevé — et la même
// fenêtre sert à TOUS : elle rend la comparaison d'un effet à l'autre honnête,
// et ne coûte que trois secondes sur l'exécution entière, parce que presque tous
// s'éteignent bien avant.
//
// ⚠ CE N'EST PAS UNE MARGE DE CONFORT. Le jour où un semis plus lent
// apparaîtra, ce nombre devra être remesuré plutôt qu'augmenté au jugé : c'est
// l'arithmétique du semis qui le fixe, pas l'envie de voir le compte monter.
constexpr std::int32_t kJudgeTicks = 700;

// L'écran sous lequel les quads d'ÉCRAN se composent. Leur nombre n'en dépend
// pas — ils sont posés par le nœud, pas découpés par le bord —, mais la
// projection de leur porteur si, et un écran nul la ferait dégénérer.
constexpr float kJudgeViewportWidth = 1600.0F;
constexpr float kJudgeViewportHeight = 900.0F;

// Où le juge place la VICTIME de l'effet, en unités du monde : cinq cases de
// terrain plus loin, la portée d'un coup lancé.
//
// ⚠ SANS CIBLE, TOUTE UNE FAMILLE NE SÈME RIEN, et le juge l'a découvert sur
// lui-même : le bouclier-boumerang, le trait tendu et la traînée qui s'efface
// accrochent leur bout lointain à quelqu'un, et rendent un semis vide quand ce
// quelqu'un manque. C'est le bon comportement — le client ne montre rien non
// plus d'une cible introuvable —, mais un juge qui n'en donne jamais déclarerait
// muettes des figures qui marchent.
constexpr float kJudgeTargetDistance = 5.0F * ro::render::kWorldUnitsPerCube;

// La caméra du JEU, et non celle de la partie en cours : un verdict ne doit pas
// dépendre de l'endroit où l'on se tient quand on le demande. Les figures dont
// le mouvement bâtit sa matrice à partir de la caméra en ont besoin, et une
// caméra par défaut les poserait de travers.
[[nodiscard]] ro::render::Camera JudgeCamera() {
  ro::render::Camera camera;
  camera.fov_degrees = ro::render::kGameFovDegrees;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.distance = ro::render::kGameViewDistance;
  return camera;
}

// Ce qu'un effet EN FICHIER montre : ses quads, toutes images additionnées, et
// seulement ceux dont la texture s'est lue.
//
// ⚠ LA TEXTURE COMPTE, et c'est ce qui distingue ce juge d'une lecture de table.
// `ComposeEffect` ne connaît que la description : un fichier dont pas une seule
// image ne se lit compose ses quads comme un autre, et ne montre pourtant rien.
// Les compter sans regarder leur texture donnerait 942 effets vus.
void JudgeFile(ro::vfs::Vfs& vfs, EffectVerdict& verdict) {
  verdict.file = ro::render::EffectFile(verdict.effect_id);
  if (verdict.file.empty()) {
    return;
  }
  // UN CATALOGUE PAR ESSAI, et non un pour tous : chaque fichier n'est jugé
  // qu'une fois, si bien que le cache ne fait rien gagner, là où garder les
  // textures de neuf cents effets décodées ensemble coûterait tout le tas.
  ro::render::EffectCatalog catalog(vfs);
  const ro::render::LoadedEffect* loaded =
      catalog.Load(std::string(ro::render::kEffectFolder) + verdict.file);
  if (loaded == nullptr) {
    // NOMMÉ ET ABSENT : nos tables tiennent son chemin, l'installation n'a pas
    // le fichier. Ce n'est pas notre défaut — le client natif ne montre rien
    // non plus — et le juge doit le dire plutôt que de le compter comme une
    // panne à corriger.
    verdict.file_missing = true;
    return;
  }
  for (std::int32_t frame = 0; frame <= loaded->prepared.last_frame; ++frame) {
    for (const ro::render::EffectQuad& quad : ro::render::ComposeEffect(loaded->prepared, frame)) {
      if (quad.texture < loaded->textures.size() && !loaded->textures[quad.texture].empty()) {
        ++verdict.file_quads;
      }
    }
  }
}

// Relève ce que la troupe atteint de mieux, un tic après l'autre.
//
// ⚠ LE MIEUX, ET NON L'ÉTAT COURANT. Un nœud de cette famille naît alpha nul et
// rayon nul — c'est la règle, et c'est son mouvement qui l'ouvre. Regarder un
// seul instant accuserait donc tout le monde, y compris ce qui marche.
void Observe(std::span<const ro::render::PooledEffectNode> nodes, FigureTrace& trace) {
  trace.most_nodes = std::max(trace.most_nodes, nodes.size());
  for (const ro::render::PooledEffectNode& node : nodes) {
    trace.best_node_alpha = std::max(trace.best_node_alpha, node.node.alpha);
    const auto forme = static_cast<std::uint8_t>(node.shape);
    const std::span<const std::uint8_t> seen(trace.shapes.data(), trace.shape_count);
    if (trace.shape_count < trace.shapes.size() && std::ranges::find(seen, forme) == seen.end()) {
      trace.shapes[trace.shape_count] = forme;
      ++trace.shape_count;
    }
    for (const ro::render::EffectTrack& track : node.tracks) {
      // ⚠ SEULES LES PISTES ALLUMÉES COMPTENT pour l'alpha et le rayon : une
      // piste éteinte garde les valeurs de sa graine sans jamais les montrer,
      // et les relever ferait croire que la figure avait de quoi dessiner.
      if (!track.active) {
        continue;
      }
      trace.any_track_active = true;
      // ⚠ ZÉRO EST LE DÉPART, et non un. Une piste sans retard naît à zéro et
      // avance dès son premier tic ; exiger un strictement positif déclarerait
      // « jamais commencée » une piste qui a peint tout du long.
      trace.any_track_started = trace.any_track_started || track.ticks >= 0;
      trace.best_track_alpha = std::max(trace.best_track_alpha, track.alpha);
      trace.best_radius = std::max(trace.best_radius, track.radius_at_anchor);
    }
  }
}

// Ce qu'un effet PROCÉDURAL montre sur toute sa vie.
//
// La troupe est celle du juge et non celle de la partie : l'essai ne doit rien
// ajouter à ce que le joueur voit, ni rien devoir à ce qui s'y joue déjà.
// Rend vrai quand le semis a posé au moins un nœud.
[[nodiscard]] bool JudgeProcedural(EffectNodesInPlay& nodes, const ro::render::Camera& camera,
                                   EffectVerdict& verdict) {
  nodes.Forget();
  if (!EffectNodesInPlay::KnowsProcedural(verdict.effect_id)) {
    return false;
  }
  verdict.knows_pattern = true;
  // ⚠ UN SEMIS MUET AU PREMIER TIC N'EST PAS UN SEMIS VIDE, et abandonner ici
  // accusait des figures irréprochables. Un effet peut n'avoir AUCUN nœud au tic
  // zéro et tout semer au tic un : son horloge est posée dès la construction et
  // ne demande qu'à battre. 312 et 320 sortent du même bâtisseur avec le même
  // horaire, et seul 312 passait — parce qu'un de ses nœuds naît au tic zéro.
  //
  // « Rien n'a été semé » se prononce donc à LA FIN DE LA FENÊTRE, sur la troupe
  // qui a vécu, et non sur ce que le premier tic a rendu.
  bool was_sown = nodes.PlayProcedural(verdict.effect_id, glm::vec3(0.0F, 0.0F, 0.0F),
                                       glm::vec3(kJudgeTargetDistance, 0.0F, 0.0F));
  std::uint32_t now_ms = kJudgeStartMs;
  for (std::int32_t tick = 0; tick <= kJudgeTicks; ++tick) {
    // ⚠ `Drawn` COMPOSE TOUT, et les trois autres ne font que rendre ce qu'il
    // vient de bâtir : les appeler sans lui rendrait l'image précédente.
    for (const ro::render::DrawnActor& drawn : nodes.Drawn(now_ms, camera)) {
      if (drawn.actor != nullptr) {
        verdict.node_sprites += drawn.actor->quads.size();
      }
    }
    for (const ro::render::DrawnWorldQuads& lot : nodes.WorldQuads()) {
      verdict.node_quads += lot.quads.size();
    }
    for (const DrawnGroundCells& lot : nodes.GroundCells()) {
      verdict.ground_cells += lot.cells.size();
    }
    // ⚠ LES DEUX PASSES D'ÉCRAN : celle du voile et de la brume est à part, et
    // ne compter que l'autre les déclarait muets.
    for (const bool over_world : {false, true}) {
      for (const ro::render::DrawnScreenQuads& lot :
           nodes.ScreenQuads(camera, kJudgeViewportWidth, kJudgeViewportHeight, over_world)) {
        verdict.screen_quads += lot.quads.size();
      }
    }
    verdict.nodes_without_texture += nodes.nodes_without_texture();
    verdict.nodes_without_quad += nodes.nodes_without_quad();
    Observe(nodes.living_nodes(), verdict.trace);
    was_sown = was_sown || nodes.living() != 0;
    now_ms += static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  }
  return was_sown;
}

// POURQUOI un essai est resté muet, par ordre de ce qui accuse le plus près.
//
// ⚠ L'ORDRE EST LA DÉCISION. Un effet peut réunir plusieurs causes — un fichier
// absent ET un patron qui ne sème rien —, et le juge n'en garde qu'une : celle
// qui envoie où il faut lire. L'absence d'un fichier passe en premier parce
// qu'elle clôt le cas sans rien à corriger ; le silence du semis passe avant
// celui de la figure parce qu'une figure ne peut rien rendre d'une troupe vide.
[[nodiscard]] SilentBecause WhySilent(const EffectVerdict& verdict, bool sown) {
  if (!verdict.silent()) {
    return SilentBecause::kNotSilent;
  }
  if (verdict.file_missing) {
    return SilentBecause::kFileMissing;
  }
  if (!verdict.knows_pattern) {
    return SilentBecause::kFileShowsNothing;
  }
  if (!sown) {
    return SilentBecause::kNothingSown;
  }
  if (verdict.nodes_without_texture != 0) {
    return SilentBecause::kNoTexture;
  }
  // ⚠ AVANT D'ACCUSER LA FIGURE, VÉRIFIER QU'ELLE A ÉTÉ APPELÉE. Des pistes
  // allumées dont AUCUNE n'a jamais atteint son premier tic disent que le nœud
  // est mort pendant leur retard : le dessinateur n'a rien eu à dessiner, et le
  // ranger sous la cause suivante envoie le relire pour rien.
  if (verdict.trace.any_track_active && !verdict.trace.any_track_started) {
    return SilentBecause::kDiedBeforeStart;
  }
  return SilentBecause::kNoQuad;
}

// TOUT CE QUE LE CLIENT PEUT MONTRER, par identifiant croissant.
//
// ⚠ L'UNION DE TROIS TABLES, ET NON LA SEULE TABLE DES CAS. La mesure a défait
// l'hypothèse : sur les 1 157 identifiants auxquels les deux tables de fichiers
// du client donnent un `.str`, 227 seulement ont aussi un cas procédural. Juger
// sur les seuls cas ferait passer pour introuvables 930 effets que le client
// montre très bien — c'est exactement le défaut que le banc a déjà payé sur
// Firewall, à l'échelle de la famille entière.
[[nodiscard]] std::vector<std::uint16_t> EverythingTheClientCanShow() {
  std::set<std::uint16_t> identifiants;
  for (const ro::render::EffectCaseEntry& entry : ro::render::EffectCaseTable()) {
    identifiants.insert(entry.effect_id);
  }
  for (const std::span<const ro::render::EffectEntry> table :
       {ro::render::EffectTable(), ro::render::EffectPrimitiveTable()}) {
    for (const ro::render::EffectEntry& entry : table) {
      identifiants.insert(entry.id);
    }
  }
  return {identifiants.begin(), identifiants.end()};
}

}  // namespace

std::string_view DescribeSilence(SilentBecause cause) {
  switch (cause) {
    case SilentBecause::kNotSilent:
      return "-";
    case SilentBecause::kFileMissing:
      return "fichier absent de l'installation";
    case SilentBecause::kFileShowsNothing:
      return "fichier lu, aucun quad visible";
    case SilentBecause::kNothingSown:
      return "patron connu, rien de semé";
    case SilentBecause::kNoTexture:
      return "nœuds semés, image illisible";
    case SilentBecause::kDiedBeforeStart:
      return "nœud mort avant que sa piste parte";
    case SilentBecause::kNoQuad:
      return "nœuds semés, figure sans quad";
  }
  return "-";
}

std::array<bool, 3> FigureSignatures(const FigureTrace& trace) {
  return {!trace.any_track_active, trace.best_track_alpha == 0 && trace.best_node_alpha <= 0.0F,
          trace.best_radius <= 0.0F};
}

std::string DescribeFigure(const FigureTrace& trace) {
  // ⚠ AUCUNE SIGNATURE DE PISTE N'ACCUSE, et c'est une mesure, pas une
  // prudence : les trois essayées — piste jamais allumée, alpha jamais monté,
  // rayon resté nul — sont portées par 254, 57 et 314 effets qui SE VOIENT.
  // La raison en est simple une fois vue : la moitié des formes ne tire rien de
  // ses pistes. Un critère pris sur elles ne sépare donc rien.
  //
  // CE QUI RESTE EST UN FAIT : la FORME du nœud, donc son dessinateur. C'est ce
  // qui range les muets par ce qu'il faut aller lire.
  if (trace.most_nodes == 0) {
    return "la troupe s'est vidée avant tout dessin";
  }
  std::string dit = "forme";
  for (std::uint8_t rang = 0; rang < trace.shape_count; ++rang) {
    dit += ' ';
    dit += std::to_string(trace.shapes[rang]);
  }
  return dit;
}

EffectJudgement JudgeEffects(ro::vfs::Vfs& vfs) {
  const auto started = std::chrono::steady_clock::now();
  const ro::render::Camera camera = JudgeCamera();
  EffectNodesInPlay nodes(vfs);

  const std::vector<std::uint16_t> everything = EverythingTheClientCanShow();
  EffectJudgement judgement;
  judgement.verdicts.reserve(everything.size());
  for (const std::uint16_t effect_id : everything) {
    EffectVerdict verdict;
    verdict.effect_id = effect_id;
    if (const ro::render::EffectCaseEntry* entry = ro::render::EffectCaseFor(effect_id);
        entry != nullptr) {
      verdict.stage = entry->stage;
      verdict.client_function = entry->client_function;
    }
    JudgeFile(vfs, verdict);
    const bool was_sown = JudgeProcedural(nodes, camera, verdict);
    verdict.cause = WhySilent(verdict, was_sown);

    if (verdict.shows()) {
      ++judgement.showing;
      if (verdict.file_quads != 0) {
        ++judgement.by_file;
      }
      if (verdict.file_quads != verdict.shown()) {
        ++judgement.by_pattern;
        // LE CONTRÔLE : cet effet-ci se voit PAR SES NŒUDS. Toute signature
        // qu'il porte quand même n'accuse rien, et le diagnostic qui s'en
        // servirait mentirait.
        const std::array<bool, 3> signatures = FigureSignatures(verdict.trace);
        for (std::size_t rang = 0; rang < signatures.size(); ++rang) {
          if (signatures[rang]) {
            ++judgement.showing_with_signature[rang];
          }
        }
      }
    } else if (verdict.silent()) {
      ++judgement.named_but_silent;
      ++judgement.silent_because[static_cast<std::size_t>(verdict.cause)];
    }
    judgement.verdicts.push_back(std::move(verdict));
  }
  // La troupe du juge ne survit pas à son verdict : ce qu'un effet infini y
  // laisse n'a plus rien à faire là.
  nodes.Forget();

  judgement.elapsed_ms =
      static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - started)
                                     .count());
  return judgement;
}

bool WriteEffectJudgement(const EffectJudgement& judgement, const std::filesystem::path& to) {
  std::ofstream file(to, std::ios::binary);
  if (!file) {
    return false;
  }
  const std::size_t total = judgement.verdicts.size();
  file << "LE JUGE DES EFFETS\n\n"
       << "Chaque identifiant que le client peut montrer — l'UNION de ses cas\n"
          "procéduraux et de ses deux tables de fichiers — est JOUÉ par le même\n"
          "chemin que le jeu, son horloge battue jusqu'au bout de sa vie, et ses\n"
          "quads comptés. Un quad dont la texture ne se lit pas n'est PAS compté :\n"
          "il ne se voit pas davantage à l'écran.\n\n"
          "L'étage est celui du répartiteur procédural, et « - » dit qu'il n'y a\n"
          "pas de cas : l'effet ne tient alors son image que d'un fichier.\n\n"
          "LE VERDICT A TROIS ÉTATS, et la différence entre les deux derniers est\n"
          "tout ce qui compte : « oui » se voit ; « MUET » veut dire qu'une de nos\n"
          "tables le nomme et que rien ne sort ; « - » veut dire qu'aucune de nos\n"
          "tables n'en parle, et c'est un portage qui reste à faire.\n\n"
          "⚠ UN MUET N'EST PAS TOUJOURS UN DÉFAUT, et sa CAUSE est ce qui tranche.\n"
          "Deux d'entre elles sont des FAUX TROUS, où le client natif ne montre rien\n"
          "non plus et où il n'y a donc rien à corriger : le fichier absent de\n"
          "l'installation, et le nœud mort avant que sa piste parte. Les autres\n"
          "accusent, et chacune envoie à un endroit différent.\n\n";
  file << std::format("montrent quelque chose  : {:5} sur {}\n", judgement.showing, total);
  file << std::format("   par un fichier       : {:5}\n", judgement.by_file);
  file << std::format("   par un patron        : {:5}\n", judgement.by_pattern);
  file << std::format("nommés mais MUETS       : {:5}\n", judgement.named_but_silent);
  for (std::size_t cause = 1; cause < judgement.silent_because.size(); ++cause) {
    if (judgement.silent_because[cause] == 0) {
      continue;
    }
    file << std::format("   {:<33} {:5}\n", DescribeSilence(static_cast<SilentBecause>(cause)),
                        judgement.silent_because[cause]);
  }
  file << std::format("aucune table n'en parle : {:5}\n",
                      total - judgement.showing - judgement.named_but_silent);
  // ⚠ CONTRÔLE DU DIAGNOSTIC, à lire AVANT de croire ce qu'une ligne accuse.
  // Une signature ne veut dire quelque chose que si AUCUN effet visible ne la
  // porte ; au-dessus de zéro, elle n'accuse rien et le juge s'en abstient.
  {
    constexpr std::array<std::string_view, 3> kNoms = {"piste jamais allumée", "alpha jamais monté",
                                                       "rayon resté nul"};
    file << std::format("\ncontrôle des signatures, sur {} effets vus par un patron :\n",
                        judgement.by_pattern);
    for (std::size_t rang = 0; rang < kNoms.size(); ++rang) {
      const std::size_t count = judgement.showing_with_signature[rang];
      file << std::format("   {:<22} {:5} effet(s) visibles la portent{}\n", kNoms[rang], count,
                          count == 0 ? " — elle ACCUSE" : " — elle n'accuse RIEN");
    }
    file << "\n";
  }
  // ⚠ LA DURÉE N'EST PAS DANS LE RAPPORT, et c'est ce qui le rend comparable :
  // elle change à chaque exécution sans que rien n'ait bougé, et un `diff` la
  // montrerait à chaque fois. Elle se lit sur la sortie de la commande.
  file << "   id ét verdict  fichier sprites   monde  cases  écran  "
          "fonction du client / fichier\n";
  for (const EffectVerdict& verdict : judgement.verdicts) {
    file << std::format(
        "{:5} {:>2} {:7} {:9} {:7} {:7} {:6} {:6}  {}{}{}\n", verdict.effect_id,
        verdict.stage == 0 ? std::string("-") : std::to_string(verdict.stage),
        verdict.shows() ? "oui" : (verdict.silent() ? "MUET" : "-"), verdict.file_quads,
        verdict.node_sprites, verdict.node_quads, verdict.ground_cells, verdict.screen_quads,
        verdict.client_function.empty() ? std::string_view("-") : verdict.client_function,
        verdict.file.empty() ? "" : "  ", verdict.file);
    if (verdict.cause == SilentBecause::kNoQuad) {
      // LA CAUSE ET CE QU'ELLE ACCUSE : « figure sans quad » est un constat, la
      // seconde moitié dit où aller lire.
      file << std::format("                 ↳ {} : {}\n", DescribeSilence(verdict.cause),
                          DescribeFigure(verdict.trace));
    } else if (verdict.cause != SilentBecause::kNotSilent) {
      file << std::format("                 ↳ {}\n", DescribeSilence(verdict.cause));
    }
  }
  return file.good();
}

std::vector<std::string> DescribedJudgement(const EffectJudgement& judgement) {
  const std::size_t total = judgement.verdicts.size();
  std::vector<std::string> lines;
  lines.push_back("juge : " + std::to_string(judgement.showing) + " effets sur " +
                  std::to_string(total) + " montrent quelque chose");
  lines.push_back("   par un fichier " + std::to_string(judgement.by_file) + ", par un patron " +
                  std::to_string(judgement.by_pattern));
  lines.push_back("   nommés mais muets " + std::to_string(judgement.named_but_silent) +
                  ", sans table " +
                  std::to_string(total - judgement.showing - judgement.named_but_silent));
  lines.push_back("   en " + std::to_string(judgement.elapsed_ms) + " ms");
  return lines;
}

std::vector<EffectTickSample> TraceEffectTicks(ro::vfs::Vfs& vfs, std::uint16_t effect_id) {
  const ro::render::Camera camera = JudgeCamera();
  EffectNodesInPlay nodes(vfs);
  nodes.Forget();
  if (!EffectNodesInPlay::KnowsProcedural(effect_id)) {
    return {};
  }
  // Même raison que dans le juge : on bat l'horloge même quand le premier tic
  // n'a rien semé, et c'est la chronologie qui dit s'il s'est passé quelque
  // chose. Abandonner ici rendait une trace VIDE pour un effet qui vit.
  static_cast<void>(nodes.PlayProcedural(effect_id, glm::vec3(0.0F, 0.0F, 0.0F),
                                         glm::vec3(kJudgeTargetDistance, 0.0F, 0.0F)));

  std::vector<EffectTickSample> chronologie;
  chronologie.reserve(static_cast<std::size_t>(kJudgeTicks) + 1);
  std::uint32_t now_ms = kJudgeStartMs;
  for (std::int32_t tick = 0; tick <= kJudgeTicks; ++tick) {
    EffectTickSample pris;
    pris.tick = tick;

    // ⚠ `Drawn` COMPOSE, les trois autres ne font que rendre : même ordre que
    // le juge, sans quoi la trace lirait l'image du tic précédent.
    for (const ro::render::DrawnActor& drawn : nodes.Drawn(now_ms, camera)) {
      if (drawn.actor != nullptr) {
        pris.quads += drawn.actor->quads.size();
      }
    }
    bool first_corner = true;
    glm::vec3 box_min{0.0F, 0.0F, 0.0F};
    glm::vec3 box_max{0.0F, 0.0F, 0.0F};
    for (const ro::render::DrawnWorldQuads& lot : nodes.WorldQuads()) {
      pris.quads += lot.quads.size();
      // L'ÉTENDUE EN ORDONNÉE, coin par coin : c'est elle qui dit si la figure
      // touche le sol de la scène, qui est à zéro. ET CELLE AU SOL avec, dans le
      // même parcours : elle dit jusqu'où la figure PORTE.
      for (const ro::render::EffectWorldQuad& quad : lot.quads) {
        for (const glm::vec3& corner : quad.corners) {
          if (first_corner) {
            box_min = corner;
            box_max = corner;
            first_corner = false;
            continue;
          }
          box_min = glm::min(box_min, corner);
          box_max = glm::max(box_max, corner);
        }
      }
    }
    if (!first_corner) {
      pris.world_y_low = box_min.y;
      pris.world_y_high = box_max.y;
      const float large = box_max.x - box_min.x;
      const float profond = box_max.z - box_min.z;
      pris.world_ground_span = std::sqrt(large * large + profond * profond);
    }
    for (const DrawnGroundCells& lot : nodes.GroundCells()) {
      pris.quads += lot.cells.size();
    }
    for (const bool over_world : {false, true}) {
      for (const ro::render::DrawnScreenQuads& lot :
           nodes.ScreenQuads(camera, kJudgeViewportWidth, kJudgeViewportHeight, over_world)) {
        pris.quads += lot.quads.size();
      }
    }

    const std::span<const ro::render::PooledEffectNode> vivants = nodes.living_nodes();
    pris.nodes = vivants.size();
    bool first_view = false;
    // LA BOÎTE DES PLACES, que toute famille remplit — même celle qui ne compose
    // aucun quad du monde. Voir `EffectTickSample::node_y_low`.
    bool first_node = true;
    glm::vec3 places_basses{0.0F, 0.0F, 0.0F};
    glm::vec3 places_hautes{0.0F, 0.0F, 0.0F};
    for (const ro::render::PooledEffectNode& node : vivants) {
      if (first_node) {
        places_basses = node.ground_position;
        places_hautes = node.ground_position;
        first_node = false;
      } else {
        places_basses = glm::min(places_basses, node.ground_position);
        places_hautes = glm::max(places_hautes, node.ground_position);
      }
      for (const ro::render::EffectTrack& track : node.tracks) {
        if (!track.active) {
          continue;
        }
        ++pris.active_tracks;
        if (!first_view) {
          pris.first_track_ticks = track.ticks;
          first_view = true;
        }
        pris.best_track_alpha = std::max(pris.best_track_alpha, track.alpha);
        pris.best_radius = std::max(pris.best_radius, track.radius_at_anchor);
      }
    }
    if (!first_node) {
      pris.node_y_low = places_basses.y;
      pris.node_y_high = places_hautes.y;
      const float large = places_hautes.x - places_basses.x;
      const float profond = places_hautes.z - places_basses.z;
      pris.node_ground_span = std::sqrt(large * large + profond * profond);
    }
    chronologie.push_back(pris);
    now_ms += static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  }
  return chronologie;
}

}  // namespace moonclient
