cmake_minimum_required(VERSION 3.15)
project(zircon_generator)

# Set C++ standard. NOTE: this project is also configured STANDALONE (nested
# cmake at build time, see zircon.core's pre-build step), so it must set its own
# standard — it does not inherit the engine root's compiler globals. It is a
# host tool, so it always uses C++20 regardless of KOTEK_CPP_STANDARD/XP rules.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
message(STATUS "KOTEK_DEPS_FOLDER = ${KOTEK_DEPS_FOLDER}")
message(STATUS "KOTEK_NUGET_PLATFORM_NAME = ${KOTEK_NUGET_PLATFORM_NAME}")
message(STATUS "KOTEK_NUGET_COMPILER_NAME = ${KOTEK_NUGET_COMPILER_NAME}")
message(STATUS "KOTEK_NUGET_ARCH_BIT_NUMBER = ${KOTEK_NUGET_ARCH_BIT_NUMBER}")
message(STATUS "CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}/$<CONFIG>")
if ("${KOTEK_DEPS_FOLDER_ENABLE}" STREQUAL "ON")
    if ("${KOTEK_DEPS_FOLDER}" STREQUAL "vcpkg" AND NOT WIN32)
        find_package(Clang REQUIRED CONFIG)
        find_package(OpenSSL REQUIRED)
    elseif ("${KOTEK_DEPS_FOLDER}" STREQUAL "nuget" OR (WIN32 AND "${KOTEK_DEPS_FOLDER}" STREQUAL "vcpkg"))
        message(STATUS "${PROJECT_SOURCE_DIR}")
        file(GLOB nuget_clang_folder RELATIVE ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/Kotek.Packages.Clang.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}*)

        file(GLOB nuget_openssl_folder RELATIVE ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/Kotek.Packages.OpenSSL.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}*)

        file(GLOB nuget_zlib_folder RELATIVE ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/Kotek.Packages.zlib.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}*)

        file(GLOB nuget_zstd_folder RELATIVE ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/Kotek.Packages.Zstd.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}*)

        message(STATUS "nuget_clang_folder = ${nuget_clang_folder}")
        message(STATUS "nuget_openssl_folder = ${nuget_openssl_folder}")
        message(STATUS "nuget_zlib_folder = ${nuget_zlib_folder}")
        message(STATUS "nuget_zstd_folder = ${nuget_zstd_folder}")

        if ("${nuget_clang_folder}" MATCHES "Kotek.Packages.Clang.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}")
            message("[${PROJECT_NAME}][nuget]: package clang exists!")
        else()
            message(FATAL_ERROR "failed to find clang package! (${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER})")
        endif()

        if ("${nuget_openssl_folder}" MATCHES "Kotek.Packages.OpenSSL.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}")
            message("[${PROJECT_NAME}][nuget]: package openssl exists!")
        else()
            message(FATAL_ERROR "failed to find openssl package! (${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER})")
        endif()

        if ("${nuget_zlib_folder}" MATCHES "Kotek.Packages.zlib.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}")
            message("[${PROJECT_NAME}][nuget]: package zlib exists!")
        else()
            message(FATAL_ERROR "failed to find zlib package! (${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER})")
        endif()

        if ("${nuget_zstd_folder}" MATCHES "Kotek.Packages.Zstd.Vcpkg.Runtimes.${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER}")
            message("[${PROJECT_NAME}][nuget]: package zstd exists!")
        else()
            message(FATAL_ERROR "failed to find zstd package! (${KOTEK_NUGET_PLATFORM_NAME}.${KOTEK_NUGET_COMPILER_NAME}.${KOTEK_NUGET_ARCH_BIT_NUMBER})")
        endif()


    else()
        find_package(clang REQUIRED PATHS "../../${KOTEK_DEPS_FOLDER}/llvm/${CMAKE_BUILD_TYPE}/")
        find_package(OpenSSL REQUIRED PATHS "../../${KOTEK_DEPS_FOLDER}/openssl/${CMAKE_BUILD_TYPE}/")
    endif()
endif()

# Include LLVM directories

if ("${KOTEK_DEPS_FOLDER}" STREQUAL "nuget")
    set(LLVM_DEFINITIONS_LIST -D_CRT_SECURE_NO_DEPRECATE;-D_CRT_SECURE_NO_WARNINGS;-D_CRT_NONSTDC_NO_DEPRECATE;-D_CRT_NONSTDC_NO_WARNINGS;-D_SCL_SECURE_NO_DEPRECATE;-D_SCL_SECURE_NO_WARNINGS;-DUNICODE;-D_UNICODE;-D__STDC_CONSTANT_MACROS;-D__STDC_FORMAT_MACROS;-D__STDC_LIMIT_MACROS)
else()
    include_directories(${LLVM_INCLUDE_DIRS})
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

    message(STATUS "LLVM libraries: ${LLVM_AVAILABLE_LIBS}")
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "LLVM include path: ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM library path: ${LLVM_LIBRARY_DIRS}")
    message(STATUS "${CLANG_INCLUDE_DIRS}")
endif()

add_definitions(${LLVM_DEFINITIONS_LIST})

# Build generator executable
add_executable(${PROJECT_NAME} generate_zircon.cpp)

if ("${KOTEK_DEPS_FOLDER}" STREQUAL "nuget" OR (WIN32 AND "${KOTEK_DEPS_FOLDER}" STREQUAL "vcpkg"))
        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include")
			message("[${PROJECT_NAME}]: zstd package included directory")
			target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include)
		endif()

		if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/zstd.lib")
			message("[${PROJECT_NAME}][Debug]: adding zstd.lib to Debug configuration")
			target_link_libraries(${PROJECT_NAME} PUBLIC debug ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/zstd.lib)
		endif()

		if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/zstd.lib")
			message("[${PROJECT_NAME}][None-Debug]: adding zstd.lib to not Debug configurations")
			target_link_libraries(${PROJECT_NAME} PUBLIC optimized ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/zstd.lib)
		endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include")
            message("[${PROJECT_NAME}]: zlib package included directory")
            target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include)
        endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/zlibd.lib")
			message("[${PROJECT_NAME}][Debug]: adding zlibd.lib to Debug configuration")
			target_link_libraries(${PROJECT_NAME} PUBLIC debug ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/zlibd.lib)
		endif()

		if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/zlib.lib")
			message("[${PROJECT_NAME}][None-Debug]: adding zlib.lib to not Debug configurations")
			target_link_libraries(${PROJECT_NAME} PUBLIC optimized ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/zlib.lib)
		endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include")
            message("[${PROJECT_NAME}]: openssl package included directory")
            target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include)
        endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/libcrypto.lib")
            message("[${PROJECT_NAME}][Debug]: adding libcrypto.lib to Debug configuration")
            target_link_libraries(${PROJECT_NAME} PUBLIC debug ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/libcrypto.lib)
        endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/libcrypto.lib")
            message("[${PROJECT_NAME}][None-Debug]: adding libcrypto.lib to not Debug configurations")
            target_link_libraries(${PROJECT_NAME} PUBLIC optimized ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/libcrypto.lib)
        endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include")
            message("[${PROJECT_NAME}]: clang package included directory")
            target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/include)
        endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/libclang.lib")
            message("[${PROJECT_NAME}][Debug]: adding libclang.lib to Debug configuration")
            target_link_libraries(${PROJECT_NAME} PUBLIC debug ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Debug/libclang.lib)
        endif()

        if (EXISTS "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/libclang.lib")
            message("[${PROJECT_NAME}][None-Debug]: adding libclang.lib to not Debug configuration")
            target_link_libraries(${PROJECT_NAME} PUBLIC optimized ${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}/lib/Release/libclang.lib)
        endif()

		set(path_to_bins_zstd "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zstd_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}")
		set(path_to_bins_zlib "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_zlib_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}")
		set(path_to_bins_openssl "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_openssl_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}")
		set(path_to_bins_clang "${PROJECT_SOURCE_DIR}/../../../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/nuget/packages/${nuget_clang_folder}/runtimes/${KOTEK_NUGET_PLATFORM_NAME}/${KOTEK_NUGET_COMPILER_NAME}/${KOTEK_NUGET_ARCH_BIT_NUMBER}")

		set(copy_local_command_debug_zstd_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zstd}/bin/Debug/zstd.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_debug_zstd_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zstd}/bin/Debug/zstd.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)

		set(copy_local_command_release_zstd_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zstd}/bin/Release/zstd.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_release_zstd_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zstd}/bin/Release/zstd.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)

		set(copy_local_command_debug_zlib_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zlib}/bin/Debug/zlibd1.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_debug_zlib_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zlib}/bin/Debug/zlibd.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)

		set(copy_local_command_release_zlib_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zlib}/bin/Release/zlib1.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_release_zlib_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_zlib}/bin/Release/zlib.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)

        set(copy_local_command_debug_openssl_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_openssl}/bin/Debug/libcrypto-3-x64.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_debug_openssl_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_openssl}/bin/Debug/libcrypto-3-x64.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)

		set(copy_local_command_release_openssl_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_openssl}/bin/Release/libcrypto-3-x64.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_release_openssl_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_openssl}/bin/Release/libcrypto-3-x64.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)

        set(copy_local_command_debug_clang_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_clang}/bin/Debug/libclang.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)

		set(copy_local_command_release_clang_1 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_clang}/bin/Release/libclang.dll" ${CMAKE_BINARY_DIR}/$<CONFIG>)
		set(copy_local_command_release_clang_2 ${CMAKE_COMMAND} -E copy_if_different "${path_to_bins_clang}/bin/Release/libclang.pdb" ${CMAKE_BINARY_DIR}/$<CONFIG>)


		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            # Zstd
			COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_zstd_1}>"
			COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_zstd_2}>"

			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_zstd_1}>"
			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_zstd_2}>"

            # Zlib
			COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_zlib_1}>"
			COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_zlib_2}>"

			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_zlib_1}>"
			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_zlib_2}>"

            # OpenSSL
            COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_openssl_1}>"
			COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_openssl_2}>"

			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_openssl_1}>"
			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_openssl_2}>"

            # Clang
            COMMAND "$<$<CONFIG:Debug>:${copy_local_command_debug_clang_1}>"

			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_clang_1}>"
			COMMAND "$<$<CONFIG:Release>:${copy_local_command_release_clang_2}>"

			COMMAND_EXPAND_LISTS

			COMMENT "copying dlls after compilation of ${PROJECT_NAME}"
		)
else()

target_link_libraries(generate_zircon PRIVATE
    libclang
    OpenSSL::SSL
    OpenSSL::Crypto
)

endif()