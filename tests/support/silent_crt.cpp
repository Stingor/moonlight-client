// LES ESSAIS NE S'ARRÊTENT JAMAIS SUR UNE BOÎTE DE DIALOGUE.
//
// En Debug, la bibliothèque d'exécution de MSVC signale une assertion, une
// corruption de pile (« Run-Time Check Failure ») ou un `abort` par une boîte
// modale qui attend un clic. Lancés sans personne devant l'écran — par un
// script, par l'intégration —, les essais resteraient suspendus, et quelqu'un
// qui travaille à côté verrait surgir des fenêtres qu'il n'a rien demandé.
//
// Ces rapports vont donc sur la sortie d'erreur et dans le débogueur attaché,
// puis le processus s'arrête : l'échec se lit dans la sortie et dans le code de
// retour, comme celui de n'importe quel essai.
//
// Ce fichier est compilé DANS l'exécutable et non dans une bibliothèque
// statique, où l'éditeur de liens jetterait un objet que rien ne référence.

#if defined(_MSC_VER)

#include <cstdlib>
#include <initializer_list>

#include <crtdbg.h>
#include <windows.h>

namespace {

struct SilentCrt {
  SilentCrt() {
#if defined(_DEBUG)
    // Hors Debug, ces rapports n'existent pas, et ces appels non plus.
    constexpr int kToConsole = _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG;
    for (const int report : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
      _CrtSetReportMode(report, kToConsole);
      _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
    }
#endif
    // `abort` n'ouvre pas sa boîte et ne passe pas par le rapport d'erreur
    // Windows : il rend la main avec son code d'échec.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    // Ni la boîte « le programme a cessé de fonctionner », ni celle d'un
    // lecteur absent.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  }
};

const SilentCrt kSilentCrt;

}  // namespace

#endif
