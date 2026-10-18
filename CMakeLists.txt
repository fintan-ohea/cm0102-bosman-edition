cmake_minimum_required(VERSION 3.20)
project(cm0102-mac VERSION 0.5.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

add_executable(cm0102
    src/main.c
    src/demo.c
    src/si/display.c
    src/si/os_compat.c
    src/si/sound.c
    src/si/input.c
    src/si/rgn_loader.c
    src/si/gui_utils.c
    src/si/gui_glyphs.c
    src/si/gui_font.c
    src/si/gui_text.c
    src/si/guio.c
    src/si/area.c
    src/si/scrman.c
    src/si/screen_stack.c
    src/si/scrman_input.c
    src/si/langlib.c
    src/si/gui_menu.c
    src/si/progress_box.c
    src/si/utils.c
    src/si/date.c
    src/si/exe_image.c
    src/si/random.c
    src/cm3/background.c
    src/cm3/menubar.c
    src/cm3/file_screens.c
    src/cm3/setup.c
    src/cm3/database.c
    src/cm3/database_links.c
    src/cm3/key_index.c
    src/cm3/key_ids_table.c
    src/cm3/key_nation.c
)

target_include_directories(cm0102 PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_libraries(cm0102 PRIVATE ${SDL2_LIBRARIES})

# Runs functions of the port for the differential test in tests/diff.
add_executable(port_driver
    tests/diff/port_driver.c
    tests/diff/port_scene.c
    tests/diff/port_stack.c
    tests/diff/port_helpers.c
    tests/diff/port_start.c
    tests/diff/port_leagues.c
    tests/diff/port_progress.c
    tests/diff/port_scroll.c
    tests/diff/port_select.c
    tests/diff/port_dates.c
    tests/diff/port_entry.c
    tests/diff/port_database.c
    src/si/display.c
    src/si/rgn_loader.c
    src/si/gui_utils.c
    src/si/gui_glyphs.c
    src/si/gui_font.c
    src/si/gui_text.c
    src/si/os_compat.c
    src/si/guio.c
    src/si/input.c
    src/si/area.c
    src/si/scrman.c
    src/si/screen_stack.c
    src/si/scrman_input.c
    src/si/langlib.c
    src/si/gui_menu.c
    src/si/progress_box.c
    src/si/utils.c
    src/si/date.c
    src/si/exe_image.c
    src/si/random.c
    src/cm3/background.c
    src/cm3/menubar.c
    src/cm3/file_screens.c
    src/cm3/setup.c
    src/cm3/database.c
    src/cm3/database_links.c
    src/cm3/key_index.c
    src/cm3/key_ids_table.c
    src/cm3/key_nation.c
)
target_include_directories(port_driver PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_libraries(port_driver PRIVATE ${SDL2_LIBRARIES})

if(APPLE)
    set_target_properties(cm0102 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Championship Manager 01/02"
        MACOSX_BUNDLE_GUI_IDENTIFIER "uk.co.sigames.cm0102"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# Test of the settings file reader of the recompiled game: ctest, or build and run options_test.
if(NOT WIN32)
    enable_testing()
    add_executable(options_test tests/options/test_options.c src/recomp_rt/win/options.c
        src/recomp_rt/host/posix.c)
    target_include_directories(options_test PRIVATE src/recomp_rt/win)
    add_test(NAME options COMMAND options_test)
    add_executable(dbfolders_test tests/dbfolders/test_dbfolders.c src/recomp_rt/win/dbfolders.c
        src/recomp_rt/host/posix.c)
    target_include_directories(dbfolders_test PRIVATE src/recomp_rt/win)
    add_test(NAME dbfolders COMMAND dbfolders_test)
    add_executable(winfit_test tests/winfit/test_winfit.c)
    target_include_directories(winfit_test PRIVATE src/recomp_rt/win)
    add_test(NAME winfit COMMAND winfit_test)
    add_executable(savename_test tests/savename/test_cp1252.c)
    target_include_directories(savename_test PRIVATE src/recomp_rt/win)
    add_test(NAME savename COMMAND savename_test)
    add_executable(draw_test tests/draw/test_draw.c src/recomp_rt/win/draw.c)
    target_include_directories(draw_test PRIVATE src/recomp_rt/win)
    add_test(NAME draw COMMAND draw_test)
    add_executable(pickscore_test tests/picker/test_pick_score.c)
    target_include_directories(pickscore_test PRIVATE src/recomp_rt/win)
    add_test(NAME pickscore COMMAND pickscore_test)
    add_executable(pdf_test tests/pdfprint/test_pdf.c src/recomp_rt/win/pdf.c)
    target_include_directories(pdf_test PRIVATE src/recomp_rt/win)
    add_test(NAME pdf COMMAND pdf_test)
    add_executable(groups_test tests/groups/test_groups.c src/recomp_rt/win/groups_layout.c)
    target_include_directories(groups_test PRIVATE src/recomp_rt/win)
    add_test(NAME groups COMMAND groups_test)
    add_executable(compare_test tests/compare/test_compare.c src/recomp_rt/win/compare_layout.c
        src/recomp_rt/win/groups_layout.c)
    target_include_directories(compare_test PRIVATE src/recomp_rt/win)
    target_link_libraries(compare_test PRIVATE m)
    add_test(NAME compare COMMAND compare_test)
endif()

# The recompiled original (docs/specs/2026-10-14-recompile-spec.md). tools/recomp translates the
# player's own cm0102.exe into C in the build folder; that C is a build product and stays out of git.
# By default the first cm0102.exe in a folder above the repo (a git worktree sits deeper).
set(CM0102_EXE_DEFAULT "${CMAKE_SOURCE_DIR}/../cm0102.exe")
set(dir "${CMAKE_SOURCE_DIR}")
foreach(level RANGE 6)
    get_filename_component(dir "${dir}" DIRECTORY)
    if(EXISTS "${dir}/cm0102.exe")
        set(CM0102_EXE_DEFAULT "${dir}/cm0102.exe")
        break()
    endif()
endforeach()
set(CM0102_EXE "${CM0102_EXE_DEFAULT}" CACHE FILEPATH "The player's cm0102.exe")
if(EXISTS "$ENV{HOME}/.venvs/cm0102/bin/python")
    set(RECOMP_PYTHON_DEFAULT "$ENV{HOME}/.venvs/cm0102/bin/python")
else()
    set(RECOMP_PYTHON_DEFAULT "python3")
endif()
set(RECOMP_PYTHON "${RECOMP_PYTHON_DEFAULT}" CACHE FILEPATH "A Python with capstone, for tools/recomp")

if(EXISTS "${CM0102_EXE}")
    set(RECOMP_SHARDS 128)
    set(RECOMP_OUT "${CMAKE_BINARY_DIR}/recomp")
    set(RECOMP_GEN "${RECOMP_OUT}/gen/table.c" "${RECOMP_OUT}/gen/functions.h")
    math(EXPR RECOMP_LAST "${RECOMP_SHARDS} - 1")
    foreach(i RANGE ${RECOMP_LAST})
        string(LENGTH "${i}" n)
        if(n EQUAL 1)
            set(i "00${i}")
        elseif(n EQUAL 2)
            set(i "0${i}")
        endif()
        list(APPEND RECOMP_GEN "${RECOMP_OUT}/gen/fn_${i}.c")
    endforeach()
    file(GLOB RECOMP_WIN CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/recomp_rt/win/*.c")
    file(GLOB RECOMP_TOOL CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/recomp/*.py")
    set(RECOMP_RUNTIME_ARGS "")
    foreach(f ${RECOMP_WIN})
        list(APPEND RECOMP_RUNTIME_ARGS --runtime "${f}")
    endforeach()
    add_custom_command(
        OUTPUT "${RECOMP_OUT}/recomp.stamp"
        BYPRODUCTS ${RECOMP_GEN}
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/recomp/recomp.py"
            --exe "${CM0102_EXE}" --out "${RECOMP_OUT}"
            --functions "${CMAKE_SOURCE_DIR}/tools/recomp/functions.txt" --all --entries 0x947a13
            --shards ${RECOMP_SHARDS} ${RECOMP_RUNTIME_ARGS} --stamp "${RECOMP_OUT}/recomp.stamp"
        DEPENDS ${RECOMP_TOOL} "${CMAKE_SOURCE_DIR}/tools/recomp/functions.txt" ${RECOMP_WIN}
            "${CMAKE_SOURCE_DIR}/src/recomp_rt/rt.c" "${CMAKE_SOURCE_DIR}/src/recomp_rt/heap.c"
        COMMENT "Recompiling cm0102.exe to C"
        VERBATIM)
    add_custom_target(recomp_gen DEPENDS "${RECOMP_OUT}/recomp.stamp")

    # The host functions (src/recomp_rt/host.h): Windows when cross-compiled with mingw-w64, else POSIX.
    if(WIN32)
        set(RECOMP_HOST src/recomp_rt/host/win32.c)
    else()
        set(RECOMP_HOST src/recomp_rt/host/posix.c)
    endif()
    add_executable(cm0102_recomp ${RECOMP_GEN}
        src/recomp_rt/rt.c
        src/recomp_rt/heap.c
        ${RECOMP_HOST}
        ${RECOMP_WIN}
    )
    add_dependencies(cm0102_recomp recomp_gen)
    target_include_directories(cm0102_recomp PRIVATE src/recomp_rt src/recomp_rt/win "${RECOMP_OUT}/gen"
        ${SDL2_INCLUDE_DIRS})
    target_compile_definitions(cm0102_recomp PRIVATE CM0102_DEFAULT_EXE="${CM0102_EXE}"
        CM0102_VERSION="${PROJECT_VERSION}")
    # The generated C: exact integer wrap, no strict aliasing, no warnings (it is not read by people).
    set_source_files_properties(${RECOMP_GEN} PROPERTIES GENERATED TRUE COMPILE_OPTIONS "-O1;-w")
    target_compile_options(cm0102_recomp PRIVATE -fno-strict-aliasing -fwrapv -O1)
    # Every call of the original is a host call: give the main thread a deep stack.
    if(WIN32)
        include(packaging/windows/windows.cmake)
    else()
        if(APPLE)
            target_link_options(cm0102_recomp PRIVATE "LINKER:-stack_size,0x10000000")
        endif()
        target_link_libraries(cm0102_recomp PRIVATE ${SDL2_LIBRARIES} m)
    endif()
else()
    message(STATUS "cm0102_recomp: ${CM0102_EXE} not found, so the recompiled game is not built")
endif()

# The Mac download (docs/specs/2026-10-14-re-signed-edition-release-spec.md, slice 5).
#   cmake -B build -DCM0102_GAME_DIR=<clean files folder> [-DCM0102_ICNS=<icon.icns>] ...
#   cmake --build build --target mac_dmg
# tools/mac/make_app.sh builds the app for arm64 and x86_64, joins them with lipo, bundles SDL2, signs ad
# hoc and writes the .dmg. tools/mac/check_package.py checks the result (target mac_check).
if(APPLE AND TARGET cm0102_recomp)
    set(CM0102_GAME_DIR "" CACHE PATH "Folder with the clean files (cm0102.exe, Data, Pictures) for the Mac app")
    set(CM0102_ICNS "" CACHE FILEPATH "Optional .icns file for the app icon. No icon is committed.")
    set(CM0102_SDL2_PREFIX_ARM64 "/opt/homebrew" CACHE PATH "SDL2 prefix (lib and include) built for arm64")
    set(CM0102_SDL2_PREFIX_X86_64 "" CACHE PATH "SDL2 prefix built for x86_64. Empty: the app is arm64 only")
    set(CM0102_APP_OUT "${CMAKE_BINARY_DIR}/mac" CACHE PATH "Output folder of the .app and .dmg")
    add_custom_target(mac_dmg
        COMMAND "${CMAKE_SOURCE_DIR}/tools/mac/make_app.sh"
            --game "${CM0102_GAME_DIR}" --exe "${CM0102_EXE}" --icns "${CM0102_ICNS}"
            --sdl2-arm64 "${CM0102_SDL2_PREFIX_ARM64}" --sdl2-x86_64 "${CM0102_SDL2_PREFIX_X86_64}"
            --out "${CM0102_APP_OUT}" --version "${PROJECT_VERSION}"
        COMMENT "Making the Mac app and .dmg"
        VERBATIM USES_TERMINAL)
    set(CM0102_CHECK_ARCHS "")
    if(NOT CM0102_SDL2_PREFIX_X86_64)
        set(CM0102_CHECK_ARCHS --archs arm64)
    endif()
    add_custom_target(mac_check
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/mac/check_package.py"
            "${CM0102_APP_OUT}/CM-01-02-Bosman-Edition-${PROJECT_VERSION}-mac.dmg"
            --version "${PROJECT_VERSION}" ${CM0102_CHECK_ARCHS}
        COMMENT "Checking the Mac download"
        VERBATIM USES_TERMINAL)
endif()
