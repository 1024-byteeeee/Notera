if(NOT APPLE)
    return()
endif()

set(bundle_path "${CPACK_TEMPORARY_DIRECTORY}/Notera.app")
if(NOT EXISTS "${bundle_path}")
    message(FATAL_ERROR "Notera bundle was not staged for signing: ${bundle_path}")
endif()

set(signing_identity "$ENV{NOTERA_CODESIGN_IDENTITY}")
if(signing_identity STREQUAL "")
    set(signing_identity "-")
endif()

file(GLOB_RECURSE signable_dylibs LIST_DIRECTORIES false
    "${bundle_path}/Contents/Frameworks/*.dylib"
    "${bundle_path}/Contents/PlugIns/*.dylib"
    "${bundle_path}/Contents/Resources/qml/*.dylib"
)
file(GLOB signable_frameworks LIST_DIRECTORIES true
    "${bundle_path}/Contents/Frameworks/*.framework"
)

foreach(signable IN LISTS signable_dylibs signable_frameworks)
    execute_process(
        COMMAND /usr/bin/codesign --force --sign "${signing_identity}" "${signable}"
        RESULT_VARIABLE signing_result
        ERROR_VARIABLE signing_error
    )
    if(NOT signing_result EQUAL 0)
        message(FATAL_ERROR "Failed to sign ${signable}: ${signing_error}")
    endif()
endforeach()

execute_process(
    COMMAND /usr/bin/codesign --force --sign "${signing_identity}" "${bundle_path}"
    RESULT_VARIABLE signing_result
    ERROR_VARIABLE signing_error
)
if(NOT signing_result EQUAL 0)
    message(FATAL_ERROR "Failed to sign Notera: ${signing_error}")
endif()

execute_process(
    COMMAND /usr/bin/codesign --verify --deep --strict --verbose=2 "${bundle_path}"
    RESULT_VARIABLE verification_result
    OUTPUT_VARIABLE verification_output
    ERROR_VARIABLE verification_error
)
if(NOT verification_result EQUAL 0)
    message(FATAL_ERROR "Notera signature verification failed: ${verification_output}${verification_error}")
endif()
