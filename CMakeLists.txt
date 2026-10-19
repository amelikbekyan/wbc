# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later

# 3.21 is the floor that matters, not the newest available: a published
# project that demands the CMake on the author's machine cannot be built by
# anyone else. Ubuntu 24.04 ships 3.28, Debian 12 ships 3.25.
cmake_minimum_required(VERSION 3.21)
# The version is the Debian package's; nothing else reads it yet.
project(wbc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Without this, clangd and the IDE have no include paths for src/ and report
# every project header as missing, which buries real diagnostics in noise.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warning flags are global and deliberate: without them a "clean build" says
# nothing. -Werror is left off on purpose — it is brittle across compiler
# versions and would halt work on a warning nobody has decided to care about.
add_compile_options(-Wall -Wextra)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(OnnxRuntime REQUIRED)
find_package(GTest REQUIRED)

# The guided filter lives in opencv_contrib. Its absence is survivable: the
# edge refiner falls back to a bilateral filter, with visibly softer edges.
find_package(OpenCV QUIET COMPONENTS ximgproc)
if (TARGET opencv_ximgproc)
    set(WBC_HAVE_XIMGPROC ON)
else ()
    set(WBC_HAVE_XIMGPROC OFF)
    message(WARNING "opencv ximgproc not found; edge refinement falls back to bilateralFilter")
endif ()

# Translations. Only lrelease is needed to build them, and it ships in the
# Qt tools package without the CMake module that qt_add_translations wants,
# so the tool is found directly. Its absence is survivable in the same way
# ximgproc's is: the application then speaks its source language, English.
set(WBC_LANGUAGES ru de fr es)
find_program(WBC_LRELEASE NAMES lrelease lrelease-qt6
             HINTS /usr/lib/qt6/bin /usr/lib/x86_64-linux-gnu/qt6/bin)
if (WBC_LRELEASE)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/i18n")
    foreach (lang IN LISTS WBC_LANGUAGES)
        set(ts "${CMAKE_SOURCE_DIR}/i18n/wbc_${lang}.ts")
        set(qm "${CMAKE_BINARY_DIR}/i18n/wbc_${lang}.qm")
        add_custom_command(OUTPUT "${qm}"
                           COMMAND "${WBC_LRELEASE}" -silent "${ts}" -qm "${qm}"
                           DEPENDS "${ts}"
                           COMMENT "Compiling ${lang} translation"
                           VERBATIM)
        list(APPEND WBC_QM_FILES "${qm}")
    endforeach ()
    set_source_files_properties(${WBC_QM_FILES} PROPERTIES GENERATED TRUE)
else ()
    message(WARNING "lrelease not found; the application will only speak English")
endif ()

include(FetchModel)

# Coverage build type. Kept separate from Debug so that ordinary debugging is
# not slowed down by instrumentation.
set(CMAKE_CXX_FLAGS_COVERAGE "-O0 -g --coverage" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage" CACHE STRING "" FORCE)

# AddressSanitizer build. Several places in this project hand raw pointers
# across an ownership boundary — the QImage built on a local matrix, the
# planar split into an external buffer in Preprocess, the bounded read of
# V4L2's card field, the write() into the loopback device. None of those
# guarantees is expressible as an assertion, so the sanitizer is what
# enforces them.
set(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
    CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined" CACHE STRING "" FORCE)

# ThreadSanitizer. ASan and UBSan do not detect data races, so a green run
# under them says nothing about the worker thread. This is the tool that
# does. Kept separate because TSan and ASan cannot be combined.
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN "-fsanitize=thread" CACHE STRING "" FORCE)

add_library(wbc_core STATIC)
target_include_directories(wbc_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
include(GNUInstallDirs)
set(WBC_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/wbc")

target_compile_definitions(wbc_core PUBLIC
                           WBC_INSTALLED_MODEL_PATH="${WBC_DATA_DIR}/selfie_segmentation.onnx"
                           WBC_HAVE_XIMGPROC=$<BOOL:${WBC_HAVE_XIMGPROC}>)
target_link_libraries(wbc_core PUBLIC
                      Qt6::Core Qt6::Gui Qt6::Widgets
                      ${OpenCV_LIBS}
                      OnnxRuntime::OnnxRuntime)
if (WBC_HAVE_XIMGPROC)
    target_link_libraries(wbc_core PUBLIC opencv_ximgproc)
endif ()

# Sources are appended by later tasks.
target_sources(wbc_core PRIVATE
               src/core/SteadyClock.cpp
               src/core/PipelineStats.cpp
               src/core/CNumericLocale.h
               src/core/CNumericLocale.cpp
               src/core/OpenCvThreads.cpp
               src/core/HeapTuning.cpp
               src/sink/I420Converter.cpp
               src/comp/BackgroundImage.cpp
               src/comp/Compositor.cpp
               src/comp/BackgroundVideo.cpp
               src/comp/OpenCvVideoDecoder.cpp
               src/seg/MaskSmoother.cpp
               src/seg/EdgeRefiner.cpp
               src/seg/MaskRefiner.cpp
               src/seg/OnnxPreprocess.cpp
               src/seg/OnnxPostprocess.cpp
               src/core/FileModelLoader.cpp
               src/seg/SelfieOnnx.cpp
               src/camera/DeviceEnumerator.cpp
               src/camera/FormatNegotiator.cpp
               src/camera/V4l2CameraSource.cpp
               src/sink/V4l2LoopbackSink.cpp
               src/sink/PkexecLoopbackAdmin.cpp
               src/sink/PipeWireAnnouncingSink.cpp
               src/core/ImageConvert.cpp
               src/core/ModelPath.cpp
               src/core/Settings.cpp
               src/core/Translations.cpp
               src/core/IPipelineController.h
               src/core/PipelineController.cpp
               src/ui/MainWindow.cpp
               src/ui/ProgramIcon.cpp)

# The compiled translations are embedded in the binary rather than installed
# beside it, so that moving the executable cannot separate it from them.
if (WBC_LRELEASE)
    qt_add_resources(wbc_core "wbc_translations"
                     PREFIX "/i18n"
                     BASE "${CMAKE_BINARY_DIR}/i18n"
                     FILES ${WBC_QM_FILES})
endif ()

# The icon's PNG sizes, embedded so that the window can show them however the
# program was started. packaging/render-icons.sh makes them from the SVG.
set(WBC_ICON_SIZES 16 24 32 48 64 128 256)
set(WBC_ICON_FILES "")
foreach (size IN LISTS WBC_ICON_SIZES)
    list(APPEND WBC_ICON_FILES "${CMAKE_SOURCE_DIR}/packaging/icons/wbc-${size}.png")
endforeach ()
qt_add_resources(wbc_core "wbc_icons"
                 PREFIX "/icons"
                 BASE "${CMAKE_SOURCE_DIR}/packaging/icons"
                 FILES ${WBC_ICON_FILES})

add_executable(wbc src/main.cpp)
target_link_libraries(wbc PRIVATE wbc_core)

# The model beside the executable in the build tree, where a program run
# without installing it finds it. Copied rather than named by its path in the
# source tree, so that the program carries no path from the machine it was
# built on.
# The executable is written to CMAKE_BINARY_DIR, the directory of this file.
configure_file("${WBC_MODEL_FILE}" "${CMAKE_BINARY_DIR}/selfie_segmentation.onnx" COPYONLY)

# Installation. The model is data, not a resource: it is 250 kB that never
# changes, and keeping it a file means a packager can replace it and a user
# can point WBC_MODEL at another one.
install(TARGETS wbc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${WBC_MODEL_FILE}" DESTINATION "${WBC_DATA_DIR}")
install(FILES "${CMAKE_SOURCE_DIR}/third_party/mediapipe-selfie-segmentation/LICENSE"
              "${CMAKE_SOURCE_DIR}/third_party/mediapipe-selfie-segmentation/NOTICE"
        DESTINATION "${WBC_DATA_DIR}/model-licence")
install(FILES "${CMAKE_SOURCE_DIR}/packaging/wbc.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
# What software centres show: the description, links and requirements.
install(FILES "${CMAKE_SOURCE_DIR}/packaging/io.github.amelikbekyan.wbc.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")
install(FILES "${CMAKE_SOURCE_DIR}/packaging/wbc.svg"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")
# Fixed sizes as well as the scalable one: a desktop picks the size it draws
# at, and a PNG made for 16 pixels is sharper there than an SVG shrunk to it.
foreach (size IN LISTS WBC_ICON_SIZES)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/icons/wbc-${size}.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps"
            RENAME "wbc.png")
endforeach ()
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
# The one thing that runs as root: it makes the virtual camera load at every
# boot, so the password is asked for once rather than after every restart.
# libexec, because nobody is meant to run it from a menu or a PATH lookup.
install(PROGRAMS "${CMAKE_SOURCE_DIR}/packaging/wbc-setup-loopback"
        DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/wbc")

# The polkit action that gives the helper's password prompt its own words.
# polkit reads actions from /usr/share/polkit-1/actions alone, so under any
# other prefix this file is ignored and pkexec shows its generic prompt; the
# helper still runs. The path in it is filled in here, so it is always the
# one the helper was installed to.
set(WBC_SETUP_HELPER "${CMAKE_INSTALL_FULL_LIBEXECDIR}/wbc/wbc-setup-loopback")
configure_file("${CMAKE_SOURCE_DIR}/packaging/org.wbc.setup-loopback.policy.in"
               "${CMAKE_BINARY_DIR}/org.wbc.setup-loopback.policy" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/org.wbc.setup-loopback.policy"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions")
# The window runs the helper from where it was installed, and falls back to
# modprobe when it is not there.
target_compile_definitions(wbc_core PUBLIC WBC_SETUP_HELPER_PATH="${WBC_SETUP_HELPER}")
# The same file reached from the executable, for a tree installed with
# cmake --install --prefix, which moves the files but not the compiled path.
file(RELATIVE_PATH WBC_SETUP_HELPER_FROM_BIN "${CMAKE_INSTALL_FULL_BINDIR}" "${WBC_SETUP_HELPER}")
target_compile_definitions(wbc_core PUBLIC
                           WBC_SETUP_HELPER_FROM_BIN="${WBC_SETUP_HELPER_FROM_BIN}")

# A build run from the IDE or the build directory has no installed helper, and
# its fallback, a one-off modprobe, is lost at the next reboot. Such a build
# may instead run the helper from the source tree. Off for Release, which is
# what gets installed and packaged: an installed program must not reach back
# into the tree it was built from, nor carry that tree's path.
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(wbc_source_helper_default OFF)
else ()
    set(wbc_source_helper_default ON)
endif ()
option(WBC_SOURCE_TREE_HELPER
       "Let a build that is not installed run packaging/wbc-setup-loopback from the source tree"
       ${wbc_source_helper_default})
if (WBC_SOURCE_TREE_HELPER)
    target_compile_definitions(wbc_core PUBLIC
        WBC_SOURCE_SETUP_HELPER_PATH="${CMAKE_SOURCE_DIR}/packaging/wbc-setup-loopback")
endif ()

include(Packaging)

enable_testing()
add_subdirectory(tests)

# Coverage reporting. gcovr is invoked over the build tree, filtered to src/,
# so that neither test code nor system headers inflate the numbers.
#
# Both this target and coverage-check below depend on the test binaries
# having been built AND run: gcov data files only appear once a binary that
# was compiled with --coverage actually executes, so `--target coverage`
# before running the suites silently produces an empty report rather than an
# error. Building is not enough; see the README for the order that makes
# this meaningful.
add_custom_target(coverage
                  COMMAND gcovr --root "${CMAKE_SOURCE_DIR}"
                                --filter "${CMAKE_SOURCE_DIR}/src/"
                                --exclude "${CMAKE_SOURCE_DIR}/src/main.cpp"
                                --html-details "${CMAKE_BINARY_DIR}/coverage/index.html"
                                --print-summary
                  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                  COMMENT "Generating coverage report")
add_dependencies(coverage unit_tests ui_tests hardware_tests)

# Two gates, because the two groups have genuinely different reach. Pure logic
# has no excuse for an untested line. A wrapper around an ioctl has a handful
# of error branches that only a broken driver reaches.
#
# The filter lists (which files belong to which gate) live in
# cmake/coverage-check.sh, not here: the strict group is every .cpp in
# src/core/, src/comp/ and src/seg/ except SelfieOnnx.cpp (which needs a real
# model file to exercise fully and so belongs in the relaxed group), plus
# FormatNegotiator.cpp and I420Converter.cpp, which are pure logic despite
# living next to files that are not. The relaxed group is MainWindow.cpp,
# SelfieOnnx.cpp, the four files that wrap a device or a privileged helper
# process, and PipeWireAnnouncingSink.cpp, whose factory runs pw-dump and
# restarts WirePlumber for real and so is reached only by a hardware test.
#
# This runs both gates unconditionally through one script rather than as two
# add_custom_target COMMANDs: a multi-COMMAND custom target stops at the
# first non-zero exit, which used to hide the relaxed gate's numbers whenever
# the strict gate failed. A gate that hides half its answer on failure is
# worse than one that prints everything, so the script always runs both and
# fails only at the end if either did.
add_custom_target(coverage-check
    COMMAND sh "${CMAKE_SOURCE_DIR}/cmake/coverage-check.sh" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Checking coverage thresholds")
add_dependencies(coverage-check unit_tests ui_tests hardware_tests)

# The half of the check that holds without hardware, which is what CI can ask.
add_custom_target(coverage-check-strict
    COMMAND sh "${CMAKE_SOURCE_DIR}/cmake/coverage-check.sh" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" strict
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Checking the hardware-independent coverage threshold")
add_dependencies(coverage-check-strict unit_tests ui_tests)
