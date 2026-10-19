# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later

# Three sets with different requirements: unit and ui need no hardware and run
# on every build; hardware needs a camera and a loopback device and is selected
# by label.

# Linked into every test binary; does something only under ThreadSanitizer.
set(WBC_TEST_SUPPORT support/SerialOpenCvUnderTsan.cpp)

add_executable(unit_tests
               ${WBC_TEST_SUPPORT}
               unit/SmokeTest.cpp
               unit/ClockTest.cpp
               unit/PipelineStatsTest.cpp
               unit/I420ConverterTest.cpp
               unit/BackgroundImageTest.cpp
               unit/BackgroundVideoTest.cpp
               unit/CompositorTest.cpp
               unit/MaskSmootherTest.cpp
               unit/EdgeRefinerTest.cpp
               unit/MaskRefinerTest.cpp
               unit/OnnxPreprocessTest.cpp
               unit/OnnxPostprocessTest.cpp
               unit/FileModelLoaderTest.cpp
               unit/SelfieOnnxTest.cpp
               unit/DeviceEnumeratorTest.cpp
               unit/FormatNegotiatorTest.cpp
               unit/FakeCameraSourceTest.cpp
               unit/FakeSinkTest.cpp
               unit/PkexecLoopbackAdminTest.cpp
               unit/PipeWireAnnouncingSinkTest.cpp
               unit/ImageConvertTest.cpp
               unit/ModelPathTest.cpp
               unit/OpenCvVideoDecoderTest.cpp
               unit/SettingsTest.cpp
               unit/TranslationsTest.cpp
               unit/PipelineControllerTest.cpp
               unit/CNumericLocaleTest.cpp
               unit/OpenCvThreadsTest.cpp
               unit/HeapTuningTest.cpp)
target_link_libraries(unit_tests PRIVATE wbc_core Qt6::Test GTest::gtest GTest::gmock GTest::gtest_main)
target_include_directories(unit_tests PRIVATE "${CMAKE_SOURCE_DIR}/tests")
# The tests load the model where it was downloaded; the program itself never
# names that path.
target_compile_definitions(unit_tests PRIVATE WBC_MODEL_PATH="${WBC_MODEL_FILE}")
add_test(NAME unit COMMAND unit_tests)
set_tests_properties(unit PROPERTIES LABELS unit)

add_executable(hardware_tests
               ${WBC_TEST_SUPPORT}
               hardware/CameraHardwareTest.cpp
               hardware/LoopbackHardwareTest.cpp
               hardware/EndToEndHardwareTest.cpp
               hardware/HardwareMain.cpp)
target_link_libraries(hardware_tests PRIVATE wbc_core GTest::gtest GTest::gmock)
target_include_directories(hardware_tests PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_definitions(hardware_tests PRIVATE WBC_MODEL_PATH="${WBC_MODEL_FILE}")
add_test(NAME hardware COMMAND hardware_tests)
set_tests_properties(hardware PROPERTIES LABELS hardware SKIP_RETURN_CODE 77)

add_executable(ui_tests ${WBC_TEST_SUPPORT} ui/UiMain.cpp ui/MainWindowTest.cpp
               ui/ScreenFitTest.cpp ui/TranslationsTest.cpp)
target_link_libraries(ui_tests PRIVATE wbc_core Qt6::Test GTest::gtest GTest::gmock)
target_include_directories(ui_tests PRIVATE "${CMAKE_SOURCE_DIR}/tests")
add_test(NAME ui COMMAND ui_tests)
set_tests_properties(ui PROPERTIES LABELS ui ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The root helper, run against a temporary root and a fake modprobe. It is
# shell, not C++, so neither coverage gate measures it; this checks each of
# its exit codes and that a second run changes nothing. Run with sh, which is
# dash on Debian and Ubuntu, so a bashism fails here before it fails there.
add_test(NAME setup_loopback
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/setup-loopback-test.sh"
                    "${CMAKE_BINARY_DIR}/wbc-setup-loopback"
                    "${WBC_MODULE_INSTALL_COMMAND}")
set_tests_properties(setup_loopback PROPERTIES LABELS unit)

add_test(NAME polkit_policy
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/polkit-policy-test.sh"
                    "${CMAKE_BINARY_DIR}/org.wbc.setup-loopback.policy"
                    "${WBC_SETUP_HELPER}")
set_tests_properties(polkit_policy PROPERTIES LABELS unit)

# The install hook and the Debian maintainer scripts, configured a second
# time with the helper, the hook and /etc pointed into the build tree. A
# function, so that the variables changed for it do not leak into the polkit
# test above or the package itself.
function(wbc_add_maintainer_scripts_test)
    set(root "${CMAKE_CURRENT_BINARY_DIR}/maintainer-scripts")
    set(WBC_SETUP_HELPER "${root}/libexec/wbc-setup-loopback")
    set(WBC_INSTALL_HOOK "${root}/libexec/wbc-install-hook")
    set(WBC_ETC "${root}/etc")
    configure_file("${CMAKE_SOURCE_DIR}/packaging/wbc-install-hook.in" "${WBC_INSTALL_HOOK}" @ONLY
                   FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    foreach (script IN ITEMS postinst postrm)
        configure_file("${CMAKE_SOURCE_DIR}/packaging/debian/${script}.in" "${root}/${script}" @ONLY)
    endforeach ()
    add_test(NAME maintainer_scripts
             COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/maintainer-scripts-test.sh"
                        "${WBC_INSTALL_HOOK}" "${root}/postinst" "${root}/postrm"
                        "${WBC_SETUP_HELPER}" "${WBC_ETC}")
    set_tests_properties(maintainer_scripts PROPERTIES LABELS unit)
endfunction()
wbc_add_maintainer_scripts_test()

add_test(NAME rpm_scriptlets
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm-scriptlets-test.sh"
                    "${CMAKE_SOURCE_DIR}/packaging/rpm/wbc.spec")
set_tests_properties(rpm_scriptlets PROPERTIES LABELS unit)

add_test(NAME arch_install
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/arch-install-test.sh"
                    "${CMAKE_SOURCE_DIR}/packaging/arch/wbc.install")
set_tests_properties(arch_install PROPERTIES LABELS unit)

add_test(NAME release_scripts
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/release-scripts-test.sh"
                    "${CMAKE_SOURCE_DIR}/ci")
set_tests_properties(release_scripts PROPERTIES LABELS unit)

add_test(NAME desktop_metadata
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/desktop-metadata-test.sh"
                    "${CMAKE_SOURCE_DIR}/packaging/wbc.desktop"
                    "${CMAKE_SOURCE_DIR}/packaging/io.github.amelikbekyan.wbc.metainfo.xml")
set_tests_properties(desktop_metadata PROPERTIES LABELS unit SKIP_RETURN_CODE 77)
