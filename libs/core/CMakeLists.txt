# ro_core — types de base, erreurs, lecture d'octets bornée, empreintes, log.
#
# Cette couche ne connaît RIEN du jeu : ni format, ni paquet, ni rendu. C'est ce
# qui lui permet d'être liée par tout le monde sans jamais créer de cycle.
#
# Sources listées explicitement, pas de glob : un fichier ajouté sans être
# déclaré doit provoquer une erreur, pas disparaître silencieusement du build.

add_library(ro_core STATIC
    src/ascii.cpp
    src/byte_sink.cpp
    src/byte_source.cpp
    src/case_fold.cpp
    src/console.cpp
    src/crash_report.cpp
    src/error.cpp
    src/file_write.cpp
    src/i18n.cpp
    src/ink.cpp
    src/log.cpp
    src/printed.cpp
    src/program_arguments.cpp
    src/search_key.cpp
    src/sha256.cpp
    src/startup_report.cpp
    src/text.cpp
    src/web_address.cpp
    src/web_preview_policy.cpp
    generated/case_fold_table.cpp)
add_library(ro::core ALIAS ro_core)

# LES OUTILS DE DÉVELOPPEMENT, en Debug seulement : voir `ro/core/build_flavor.h`.
# La mise au point du journal (`LogDebug`) en dépend aussi. PUBLIC, parce que
# chaque couche qui en porte un lit la même valeur.
target_compile_definitions(ro_core PUBLIC
    MOONCLIENT_DEV_TOOLS=$<IF:$<CONFIG:Debug>,1,0>)

target_include_directories(ro_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_core
    PUBLIC spdlog::spdlog
    PRIVATE ro_warnings)

# L'identite du binaire est PRODUITE par CMake, dans l'arbre de build : elle
# depend de la revision, donc elle ne peut pas vivre dans les sources.
target_include_directories(ro_core PUBLIC "${CMAKE_BINARY_DIR}/generated")

# LE RAPPORT DE PLANTAGE parle au systeme : il lui demande les registres, les
# modules charges, et un vidage qu'un debogueur rouvrira. Les trois viennent de
# bibliotheques de Windows, et d'elles seules -- ailleurs, le rapport se
# contente des signaux, qui sont du C standard.
if(WIN32)
    target_link_libraries(ro_core PRIVATE dbghelp)
endif()

# `std::stacktrace` N'EST PAS DANS LA libstdc++ ORDINAIRE : GCC 14 le range dans
# `libstdc++exp`, que rien ne lie d'office. Sans elle, l'en-tete compile et
# l'edition de liens echoue sur `_M_populate`. Clang sous Linux emploie la meme
# bibliotheque standard, donc la meme exigence.
if(NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(ro_core PRIVATE stdc++exp)
endif()
