file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${JAVA_COMPILER}" --release 17 -d "${OUTPUT_DIR}"
    "${SOURCE_DIR}/android/package/src/io/github/jatayuos4u/jatayumediaplayer/PlaybackPolicy.java"
    "${SOURCE_DIR}/tests/java/PlaybackPolicyTest.java" RESULT_VARIABLE compile_result)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "Playback policy compile failed")
endif()
execute_process(COMMAND "${JAVA_RUNTIME}" -cp "${OUTPUT_DIR}" io.github.jatayuos4u.jatayumediaplayer.PlaybackPolicyTest RESULT_VARIABLE test_result)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "Playback policy tests failed")
endif()
