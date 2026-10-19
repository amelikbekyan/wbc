# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later

# Debian package, built with `cpack -G DEB` in a configured and built build
# directory; the README has the commands. Included after the install rules.
#
# CPack installs under CMAKE_INSTALL_PREFIX, the same prefix whose paths are
# compiled into the program and the polkit action, so the two cannot
# disagree. A package to distribute is configured with
# -DCMAKE_INSTALL_PREFIX=/usr: polkit reads actions only from /usr/share.

set(WBC_PACKAGE_MAINTAINER "Arthur Melikbekyan <arthur.melikbekyan@protonmail.com>"
    CACHE STRING "The Maintainer field of the Debian package")

# The label is read from the source rather than written out again, so that
# the postinst cannot create a device the window does not recognise as its
# own.
file(STRINGS "${CMAKE_SOURCE_DIR}/src/core/Types.h" wbc_label_line
     REGEX "kVirtualCameraLabel = \"")
string(REGEX REPLACE ".*kVirtualCameraLabel = \"([^\"]*)\".*" "\\1"
       WBC_VIRTUAL_CAMERA_LABEL "${wbc_label_line}")
if (WBC_VIRTUAL_CAMERA_LABEL STREQUAL "" OR WBC_VIRTUAL_CAMERA_LABEL STREQUAL wbc_label_line)
    message(FATAL_ERROR "kVirtualCameraLabel could not be read from src/core/Types.h")
endif ()
# PkexecLoopbackAdmin's default videoNumber.
set(WBC_VIRTUAL_CAMERA_NUMBER 10)

# The install-time setup every package format runs; see the hook's comment.
set(WBC_INSTALL_HOOK "${CMAKE_INSTALL_FULL_LIBEXECDIR}/wbc/wbc-install-hook")
configure_file("${CMAKE_SOURCE_DIR}/packaging/wbc-install-hook.in"
               "${CMAKE_BINARY_DIR}/wbc-install-hook" @ONLY)
install(PROGRAMS "${CMAKE_BINARY_DIR}/wbc-install-hook"
        DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/wbc")

set(WBC_ETC "/etc")
foreach (script IN ITEMS postinst postrm)
    configure_file("${CMAKE_SOURCE_DIR}/packaging/debian/${script}.in"
                   "${CMAKE_BINARY_DIR}/debian/${script}" @ONLY
                   FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach ()

set(CPACK_PACKAGE_NAME "wbc")
set(CPACK_PACKAGE_VENDOR "Arthur Melikbekyan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Replaces the background of a webcam and offers the result as a virtual camera")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_STRIP_FILES TRUE)
# Directories as Debian expects them, 755, whatever the umask of the build:
# /usr/libexec/wbc holds a program that runs as root, and a directory
# writable by more than its owner would let others replace it.
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# The same version is built for more than one distribution, and each needs a
# file of its own: wbc_0.1.0-1~ubuntu26.04_amd64.deb. The tilde sorts the
# package below a plain 0.1.0-1, as Debian does for backports. Empty keeps the
# name a build from source has always had.
set(WBC_PACKAGE_DISTRO "" CACHE STRING
    "The distribution a Debian package is built for, e.g. ubuntu26.04, put in its release")
if (WBC_PACKAGE_DISTRO)
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1~${WBC_PACKAGE_DISTRO}")
endif ()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${WBC_PACKAGE_MAINTAINER}")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Recommends rather than Depends: a machine that builds the module some other
# way, or ships it in its kernel, should still be able to install this.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "v4l2loopback-dkms")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_BINARY_DIR}/debian/postinst"
                                       "${CMAKE_BINARY_DIR}/debian/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

if (NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(STATUS "The Debian package will install under ${CMAKE_INSTALL_PREFIX}; "
                   "configure with -DCMAKE_INSTALL_PREFIX=/usr for one to distribute")
endif ()

# TODO: ONNX Runtime is not in the Ubuntu 24.04 archive; ci/install-deps-ubuntu.sh
# puts it in /opt/onnxruntime. A package built against that copy neither
# depends on ONNX Runtime nor carries it, so it installs anywhere and then the
# program fails to start on a machine without the same /opt/onnxruntime.
# Bundling the library, or packaging it separately, is a change of its own.
# Until then dpkg-shlibdeps is only pointed at the directory so that it can
# find the library and the package still builds -- which is what CI checks --
# and the only package fit to distribute is one built where ONNX Runtime comes
# from the system, as on Ubuntu 26.04.
get_filename_component(wbc_ort_dir "${OnnxRuntime_LIBRARY}" DIRECTORY)
if (NOT wbc_ort_dir MATCHES "^/usr/lib")
    message(WARNING "ONNX Runtime is not the system's (${wbc_ort_dir}); a Debian package "
                    "built here will not declare or carry it. See cmake/Packaging.cmake.")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${wbc_ort_dir}")
endif ()

include(CPack)
