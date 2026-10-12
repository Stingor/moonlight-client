#include "ro/core/crash_report.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <fstream>
#include <optional>
#include <stacktrace>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/build_stamp.h"
#include "ro/core/log.h"

#if defined(_WIN32)
#include <windows.h>
// ⚠ APRÈS `windows.h`, et cet ordre n'est pas négociable : `dbghelp.h` emploie
// des types que le premier définit, et l'inverse ne compile pas.
#include <dbghelp.h>
// `EnumProcessModules` et `MODULEINFO` vivent ici, et non dans `windows.h`.
#include <psapi.h>
#endif

namespace ro::core {
namespace {

// Le dossier où déposer les rapports. Vide tant que rien n'est installé.
std::filesystem::path& ReportFolder() {
  static std::filesystem::path folder;
  return folder;
}

// « 20260914-161530 » : de quoi nommer deux plantages de la même partie sans
// que le second efface le premier.
[[nodiscard]] std::string NowStamp() {
  const auto now = std::chrono::system_clock::now();
  return std::format("{:%Y%m%d-%H%M%S}", std::chrono::floor<std::chrono::seconds>(now));
}

[[nodiscard]] std::filesystem::path ReportPath(const std::string& stamp, const char* suffix) {
  return ReportFolder() / ("plantage-" + stamp + suffix);
}

// LA PILE, une image par ligne.
//
// ⚠ ELLE ALLOUE, ce qui est interdit en théorie dans un gestionnaire de faute —
// le tas peut être celui qu'on vient de corrompre. On le fait quand même, et
// c'est un choix : le programme est perdu de toute façon, et une pile obtenue
// neuf fois sur dix vaut mieux qu'un silence dix fois sur dix.
void WriteStack(std::ostream& into) {
  const std::stacktrace pile = std::stacktrace::current();
  if (pile.empty()) {
    into << "  (indisponible)\n";
    return;
  }
  for (const std::stacktrace_entry& image : pile) {
    into << std::format("  {}\n", image.description());
    if (!image.source_file().empty()) {
      into << std::format("      {}:{}\n", image.source_file(), image.source_line());
    }
  }
}

// LES DERNIÈRES LIGNES DU JOURNAL, et c'est ce que le rapport apporte de plus.
//
// La faute dit OÙ le programme est mort ; ces lignes disent ce qu'il faisait.
// Elles sont déjà en mémoire — voir `RecentLogLines` — donc les écrire ne
// demande aucune lecture de fichier au pire moment.
constexpr std::size_t kReportedLogLines = 200;

//
// Rend faux si le journal était en cours d'écriture au moment de la faute :
// voir `TryRecentLogLines`. Journaliser ensuite bloquerait de même.
[[nodiscard]] bool WriteRecentLog(std::ostream& into) {
  const std::optional<std::vector<LoggedLine>> lines = TryRecentLogLines(kReportedLogLines);
  if (!lines.has_value()) {
    into << "  (indisponible : la faute est survenue pendant une ecriture du journal)\n";
    return false;
  }
  for (const LoggedLine& line : *lines) {
    into << std::format("  {}.{:03} {}\n", line.since_start_ms / 1000U, line.since_start_ms % 1000U,
                        line.text);
  }
  return true;
}

// DIRE AU JOUEUR QU'UN RAPPORT EXISTE, et où.
//
// ⚠ SANS ÇA, TOUT CE QUI PRÉCÈDE NE SERT À RIEN. La fenêtre meurt avec le
// processus et le joueur ne voit qu'une disparition : il dira « ça a planté »,
// et le fichier restera sur son disque sans que personne ne le demande. Le
// rapport le mieux écrit ne vaut que s'il est ENVOYÉ.
//
// La boîte du système, et non la nôtre : la nôtre demanderait un contexte
// graphique, celui-là même qui vient peut-être de tomber.
void TellThePlayer(const std::filesystem::path& path, const std::string& fault) {
#if defined(_WIN32)
  const std::wstring said =
      L"moonclient s'est arrêté : " + std::filesystem::path(fault).wstring() +
      L"\n\nUn rapport a été écrit à côté du jeu :\n" + path.filename().wstring() +
      L"\n\nL'envoyer avec une description de ce que vous faisiez est ce qui permet "
      L"de corriger la panne. Le fichier .dmp qui l'accompagne aide aussi.";
  MessageBoxW(nullptr, said.c_str(), L"moonclient", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#else
  // Ailleurs, la console tient lieu de boîte : c'est là qu'on lance.
  static_cast<void>(path);
  static_cast<void>(fault);
#endif
}

void WriteHeader(std::ostream& into, const std::string& stamp, const std::string& fault) {
  into << "moonclient - rapport de plantage\n";
  into << "date     : " << stamp << "\n";
  // ⚠ SANS CES DEUX LIGNES, LE RESTE DU RAPPORT NE SERT PRESQUE À RIEN. Une
  // pile d'appels ne se lit qu'avec les symboles de SON binaire ; appliquer
  // ceux d'une autre version donne des noms de fonctions faux, ce qui est pire
  // que pas de noms du tout. C'est par la révision qu'on retrouve les bons.
  into << "version  : " << kBuildVersion << "\n";
  into << "revision : " << kBuildCommit << " (arbre " << kBuildTree << ")\n";
  into << "faute    : " << fault << "\n\n";
}

// Le rapport commun aux deux voies : celle du signal et celle du filtre.
void WriteReport(const std::string& stamp, const std::string& fault, const std::string& detail) {
  const std::filesystem::path path = ReportPath(stamp, ".txt");
  std::ofstream report(path, std::ios::binary);
  if (!report) {
    // 🔴 CETTE LIGNE PORTE LA FAUTE, parce qu'elle est la SEULE. Sur ce chemin
    // on rend la main tout de suite : ni la ligne du bas ni la boîte du joueur
    // ne s'exécutent, et un journal qui dirait seulement « rapport impossible »
    // ne dirait même pas qu'il y a eu plantage.
    LogError("PLANTAGE : {} — le rapport n'a pas pu s'écrire dans {}, il ne reste que ce journal",
             fault, path.string());
    return;
  }
  WriteHeader(report, stamp, fault);
  if (!detail.empty()) {
    report << detail << "\n";
  }
  // ⚠ LES PREMIÈRES IMAGES SONT CELLES DU RAPPORTEUR, et c'est inévitable : la
  // pile se prend DEPUIS le gestionnaire. L'instruction fautive est celle que
  // RIP nomme plus haut ; ce qui la précède est le chemin par lequel le système
  // nous a appelés. Le dire vaut mieux que de couper un nombre d'images fixe,
  // qui varierait avec le compilateur.
  report << "PILE D'APPELS (les premieres images sont celles du rapporteur)\n";
  WriteStack(report);
  report << "\nDERNIERES LIGNES DU JOURNAL\n";
  const bool log_free = WriteRecentLog(report);
  // ⚠ APRÈS LA FERMETURE, et ce n'est pas du rangement : journaliser alloue et
  // prend un verrou, dans un gestionnaire de faute. Une seconde faute ici
  // perdait un rapport entièrement écrit mais jamais vidé — `std::_Exit` ne vide
  // aucun flux. Fermer d'abord ne coûte rien.
  report.close();
  // 🔴 ET SEULEMENT SI LE JOURNAL EST LIBRE : tenu par ce fil, le reprendre
  // figerait le client avant que la boîte ne dise au joueur où est le rapport.
  if (log_free) {
    LogError("PLANTAGE : {} — rapport dans {}", fault, path.string());
  }
  TellThePlayer(path, fault);
}

void ReportSignal(int number) {
  // ⚠ LE GESTIONNAIRE SE RETIRE AVANT D'ÉCRIRE : si l'écriture refaute, le
  // second signal doit tuer le programme au lieu de nous rappeler en boucle
  // jusqu'à épuiser la pile.
  std::signal(number, SIG_DFL);
  WriteReport(NowStamp(), std::format("signal {}", number), {});
  std::_Exit(EXIT_FAILURE);
}

void ReportTerminate() {
  std::string detail = "EXCEPTION NON RATTRAPEE\n";
  // Ce qu'elle disait, quand il y en a une : `terminate` est appelé aussi pour
  // d'autres raisons — un `noexcept` violé, une double faute — et il n'y a
  // alors rien à relancer.
  if (const std::exception_ptr thrown = std::current_exception(); thrown != nullptr) {
    try {
      std::rethrow_exception(thrown);
    } catch (const std::exception& problem) {
      detail += std::format("  {}\n", problem.what());
    } catch (...) {
      detail += "  d'un type qui ne dit rien\n";
    }
  }
  WriteReport(NowStamp(), "exception non rattrapee", detail);
  std::_Exit(EXIT_FAILURE);
}

#if defined(_WIN32)

// LES REGISTRES, comme le rapport du client officiel les donne.
//
// Ils ne servent qu'avec le désassemblage sous les yeux, mais c'est justement
// le cas où l'on n'a plus que le rapport : RIP nomme l'instruction fautive, et
// les autres disent avec quoi elle travaillait.
[[nodiscard]] std::string DescribeRegisters(const CONTEXT& state) {
  return std::format(
      "REGISTRES\n"
      "  RIP={:016x} RSP={:016x} RBP={:016x}\n"
      "  RAX={:016x} RBX={:016x} RCX={:016x} RDX={:016x}\n"
      "  RSI={:016x} RDI={:016x} R8 ={:016x} R9 ={:016x}\n",
      state.Rip, state.Rsp, state.Rbp, state.Rax, state.Rbx, state.Rcx, state.Rdx, state.Rsi,
      state.Rdi, state.R8, state.R9);
}

// LES MODULES CHARGÉS et leur adresse de base.
//
// C'est ce qui rend une pile lisible quand les symboles manquent : une adresse
// moins la base de son module donne un déplacement qu'on retrouve dans un
// désassembleur.
[[nodiscard]] std::string DescribeModules() {
  std::string listed = "MODULES\n";
  HMODULE handles[256] = {};
  DWORD used = 0;
  if (EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &used) == 0) {
    return listed + "  (indisponible)\n";
  }
  const std::size_t count = used / sizeof(HMODULE);
  for (std::size_t index = 0; index < count; ++index) {
    char name[MAX_PATH] = {};
    if (GetModuleFileNameA(handles[index], name, sizeof(name)) == 0) {
      continue;
    }
    MODULEINFO about = {};
    if (GetModuleInformation(GetCurrentProcess(), handles[index], &about, sizeof(about)) == 0) {
      continue;
    }
    listed +=
        std::format("  {:016x} {:>10} {}\n", reinterpret_cast<std::uintptr_t>(about.lpBaseOfDll),
                    about.SizeOfImage, name);
  }
  return listed;
}

// LE VIDAGE, qu'un débogueur rouvre des semaines plus tard.
//
// C'est la pièce que le rapport en texte ne remplace pas : elle porte la pile
// de TOUS les fils et les variables locales, là où le texte ne porte que ce
// qu'on a pensé à écrire.
void WriteDump(const std::string& stamp, EXCEPTION_POINTERS* report) {
  const std::filesystem::path path = ReportPath(stamp, ".dmp");
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  MINIDUMP_EXCEPTION_INFORMATION about = {};
  about.ThreadId = GetCurrentThreadId();
  about.ExceptionPointers = report;
  about.ClientPointers = FALSE;
  // ⚠ `WithIndirectlyReferencedMemory` ET RIEN DE PLUS : un vidage complet du
  // tas d'un client de jeu pèse des centaines de mégaoctets — personne ne
  // l'enverra. Celui-ci porte les piles et ce qu'elles désignent, soit quelques
  // mégaoctets, et cela suffit à lire les variables d'une trame fautive.
  constexpr MINIDUMP_TYPE kKept = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithThreadInfo);
  MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, kKept, &about, nullptr,
                    nullptr);
  CloseHandle(file);
}

// Deux fautes que le système nomme sans que `windows.h` ne les définisse : elles
// vivent dans `ntstatus.h`, qui ne s'inclut pas à côté de lui sans conflits.
constexpr DWORD kHeapCorruptionCode = 0xC0000374;
constexpr DWORD kStackBufferOverrunCode = 0xC0000409;

// ⚠ UN SEUL RAPPORT PAR PROCESSUS : la veille et le filtre voient la même
// faute, l'une avant l'autre.
std::atomic<bool>& AlreadyReported() {
  static std::atomic<bool> reported{false};
  return reported;
}

LONG WINAPI ReportException(EXCEPTION_POINTERS* report) {
  if (AlreadyReported().exchange(true)) {
    return EXCEPTION_EXECUTE_HANDLER;
  }
  const EXCEPTION_RECORD& fault = *report->ExceptionRecord;
  std::string detail =
      std::format("adresse : {:#x}\n", reinterpret_cast<std::uintptr_t>(fault.ExceptionAddress));
  // ⚠ UNE VIOLATION D'ACCÈS DIT DAVANTAGE : le premier paramètre vaut zéro pour
  // une lecture et un pour une écriture, le second est l'adresse touchée. Zéro
  // y signifie presque toujours un pointeur nul déréférencé, et le savoir
  // épargne une session de débogueur.
  if (fault.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && fault.NumberParameters >= 2) {
    detail += std::format("        {} a {:#x}\n",
                          fault.ExceptionInformation[0] == 0 ? "lecture" : "ecriture",
                          fault.ExceptionInformation[1]);
  }
  detail += "\n" + DescribeRegisters(*report->ContextRecord);
  detail += "\n" + DescribeModules();

  const std::string stamp = NowStamp();
  WriteDump(stamp, report);
  WriteReport(stamp, DescribeFault(fault.ExceptionCode), detail);
  // EXECUTE_HANDLER et non CONTINUE_SEARCH : la recherche mènerait au
  // gestionnaire par défaut de Windows, qui ouvre une boîte de dialogue ou se
  // tait selon la machine. On a dit ce qu'on avait à dire ; on s'arrête.
  return EXCEPTION_EXECUTE_HANDLER;
}

// LA VEILLE, pour la faute que le filtre ne voit pas.
//
// ⚠ UN TAS CORROMPU NE PASSE PAS PAR LE FILTRE : le système lève la faute puis
// tue le processus lui-même, et le client disparaissait sans rapport. Une
// veille la voit PASSER, avant tout gestionnaire ; elle écrit le rapport et
// laisse la faute suivre son cours.
//
// Elle ne retient que ces fautes-là : une veille voit aussi chaque exception
// C++ levée puis rattrapée, qui n'est pas une panne.
LONG WINAPI WatchFatalFaults(EXCEPTION_POINTERS* report) {
  const DWORD code = report->ExceptionRecord->ExceptionCode;
  if (code == kHeapCorruptionCode || code == kStackBufferOverrunCode) {
    ReportException(report);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

// Un paramètre que le runtime C refuse — un format faux, un flux nul. Sans ce
// gestionnaire, il termine le processus sans passer par aucun filtre.
void ReportInvalidParameter(const wchar_t* /*expression*/, const wchar_t* /*function*/,
                            const wchar_t* /*file*/, unsigned int /*line*/,
                            std::uintptr_t /*reserved*/) {
  WriteReport(NowStamp(), "parametre invalide refuse par le runtime C", {});
  std::_Exit(EXIT_FAILURE);
}

// Une fonction virtuelle pure appelée, typiquement depuis un destructeur.
void ReportPureCall() {
  WriteReport(NowStamp(), "appel d'une fonction virtuelle pure", {});
  std::_Exit(EXIT_FAILURE);
}

#endif

}  // namespace

std::string DescribeFault(unsigned long code) {
  // Les fautes qu'un client rencontre vraiment. Les autres se disent par leur
  // numéro : les nommer toutes ferait une table que personne ne lit.
  switch (code) {
#if defined(_WIN32)
    case EXCEPTION_ACCESS_VIOLATION:
      return "violation d'acces";
    case EXCEPTION_STACK_OVERFLOW:
      return "pile debordee";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "division entiere par zero";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "instruction illegale";
    case EXCEPTION_IN_PAGE_ERROR:
      return "page illisible";
    case EXCEPTION_PRIV_INSTRUCTION:
      return "instruction privilegiee";
    case kHeapCorruptionCode:
      return "tas corrompu";
    case kStackBufferOverrunCode:
      return "debordement de tampon detecte";
#endif
    default:
      return std::format("faute {:#x}", code);
  }
}

void InstallCrashReporter(std::filesystem::path folder) {
  ReportFolder() = std::move(folder);

#if defined(_WIN32)
  // ⚠ LE FILTRE, ET SURTOUT PAS DE GESTIONNAIRE DE SIGNAL POUR LES FAUTES.
  //
  // C'est contre-intuitif : sous Windows, une violation
  // d'accès est une EXCEPTION STRUCTURÉE. Le runtime C l'attrape dans le
  // `__try` qui enveloppe `main`, et sa décision dépend de nous — SI un
  // gestionnaire de signal SIGSEGV est installé, il lève le signal et notre
  // filtre n'est JAMAIS appelé.
  //
  // Or le signal ne porte ni registres, ni adresse fautive, ni contexte, donc
  // ni vidage exploitable : la pile mènerait à `seh_filter_exe`, et le rapport
  // ne saurait dire que « signal 11 ».
  //
  // On laisse donc les fautes matérielles au filtre, qui a tout.
  SetUnhandledExceptionFilter(ReportException);
  // En DERNIER parmi les veilles : celles qu'un module tiers poserait passent
  // avant, et peuvent encore rattraper ce qu'elles ont levé.
  AddVectoredExceptionHandler(0, WatchFatalFaults);
  _set_invalid_parameter_handler(ReportInvalidParameter);
  _set_purecall_handler(ReportPureCall);
  // `abort` reste un signal, et aucune exception structurée ne le couvre :
  // c'est par là qu'une assertion violée passe.
  std::signal(SIGABRT, ReportSignal);
#else
  for (const int number : {SIGSEGV, SIGABRT, SIGFPE, SIGILL}) {
    std::signal(number, ReportSignal);
  }
#endif
  std::set_terminate(ReportTerminate);
}

void RearmCrashReporter(std::string_view moment) {
#if defined(_WIN32)
  const LPTOP_LEVEL_EXCEPTION_FILTER previous = SetUnhandledExceptionFilter(ReportException);
  if (previous != ReportException) {
    LogWarning(
        "filet à plantages : un module chargé {} l'avait remplacé — il est reposé, mais ce module "
        "(voir les modules étrangers) peut fermer le client sans rapport",
        moment);
  }
#else
  static_cast<void>(moment);
#endif
}

}  // namespace ro::core
