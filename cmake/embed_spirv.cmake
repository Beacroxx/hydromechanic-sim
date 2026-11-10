function(_generate_embedded_spirv)
  foreach(required_var SLANGC_EXECUTABLE SHADER_SPIRV_FILE SHADER_CPP_FILE SHADER_SOURCES)
    if(NOT DEFINED ${required_var})
      message(FATAL_ERROR "Missing required variable: ${required_var}")
    endif()
  endforeach()

  execute_process(
    COMMAND "${SLANGC_EXECUTABLE}" ${SHADER_SOURCES}
      -target spirv
      -profile spirv_1_0
      -emit-spirv-directly
      -fvk-use-entrypoint-name
      -o "${SHADER_SPIRV_FILE}"
    RESULT_VARIABLE SLANGC_RESULT
    OUTPUT_VARIABLE SLANGC_OUTPUT
    ERROR_VARIABLE SLANGC_ERROR
  )
  if(NOT SLANGC_RESULT EQUAL 0)
    file(REMOVE "${SHADER_SPIRV_FILE}" "${SHADER_CPP_FILE}")
    message(FATAL_ERROR "Slang shader compilation failed:\n${SLANGC_OUTPUT}${SLANGC_ERROR}")
  endif()

  file(READ "${SHADER_SPIRV_FILE}" SHADER_SPIRV_HEX HEX)
  file(REMOVE "${SHADER_SPIRV_FILE}")
  string(LENGTH "${SHADER_SPIRV_HEX}" SHADER_SPIRV_HEX_LENGTH)
  if(SHADER_SPIRV_HEX_LENGTH EQUAL 0)
    message(FATAL_ERROR "Slang produced an empty SPIR-V module")
  endif()
  math(EXPR SHADER_SPIRV_REMAINDER "${SHADER_SPIRV_HEX_LENGTH} % 8")
  if(SHADER_SPIRV_REMAINDER)
    message(FATAL_ERROR "Compiled SPIR-V size is not a multiple of 4 bytes")
  endif()

  set(SHADER_SPIRV_WORDS)
  math(EXPR SHADER_SPIRV_LAST_OFFSET "${SHADER_SPIRV_HEX_LENGTH} - 8")
  foreach(SHADER_SPIRV_OFFSET RANGE 0 ${SHADER_SPIRV_LAST_OFFSET} 8)
    string(SUBSTRING "${SHADER_SPIRV_HEX}" ${SHADER_SPIRV_OFFSET} 8 SHADER_SPIRV_WORD_HEX)
    string(SUBSTRING "${SHADER_SPIRV_WORD_HEX}" 0 2 SHADER_SPIRV_BYTE_0)
    string(SUBSTRING "${SHADER_SPIRV_WORD_HEX}" 2 2 SHADER_SPIRV_BYTE_1)
    string(SUBSTRING "${SHADER_SPIRV_WORD_HEX}" 4 2 SHADER_SPIRV_BYTE_2)
    string(SUBSTRING "${SHADER_SPIRV_WORD_HEX}" 6 2 SHADER_SPIRV_BYTE_3)
    list(APPEND SHADER_SPIRV_WORDS
      "0x${SHADER_SPIRV_BYTE_3}${SHADER_SPIRV_BYTE_2}${SHADER_SPIRV_BYTE_1}${SHADER_SPIRV_BYTE_0}"
    )
  endforeach()

  list(LENGTH SHADER_SPIRV_WORDS SHADER_SPIRV_WORD_COUNT)
  math(EXPR SHADER_SPIRV_LAST_INDEX "${SHADER_SPIRV_WORD_COUNT} - 1")
  set(SHADER_SPIRV_INITIALIZER "")
  foreach(SHADER_SPIRV_INDEX RANGE 0 ${SHADER_SPIRV_LAST_INDEX})
    list(GET SHADER_SPIRV_WORDS ${SHADER_SPIRV_INDEX} SHADER_SPIRV_WORD)
    if(SHADER_SPIRV_INDEX GREATER 0)
      string(APPEND SHADER_SPIRV_INITIALIZER ",")
    endif()
    math(EXPR SHADER_SPIRV_COLUMN "${SHADER_SPIRV_INDEX} % 8")
    if(SHADER_SPIRV_COLUMN EQUAL 0)
      string(APPEND SHADER_SPIRV_INITIALIZER "\n  ")
    else()
      string(APPEND SHADER_SPIRV_INITIALIZER " ")
    endif()
    string(APPEND SHADER_SPIRV_INITIALIZER "${SHADER_SPIRV_WORD}")
  endforeach()

  file(WRITE "${SHADER_CPP_FILE}"
    "#include \"shader_spirv.hpp\"\n\n"
    "namespace Shader {\n"
    "extern const std::uint32_t spirv[] = {${SHADER_SPIRV_INITIALIZER}\n};\n"
    "extern const std::size_t spirvWordCount = ${SHADER_SPIRV_WORD_COUNT};\n"
    "} // namespace Shader\n"
  )
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
  _generate_embedded_spirv()
  return()
endif()

function(embed_spirv target)
  cmake_parse_arguments(PARSE_ARGV 1 SHADER "" "" "SOURCES")
  if(NOT SHADER_SOURCES)
    message(FATAL_ERROR "embed_spirv requires at least one source in SOURCES")
  endif()

  set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(shader_spirv_file "${generated_dir}/shader.spv")
  set(shader_cpp_file "${generated_dir}/shader_spirv.cpp")
  file(MAKE_DIRECTORY "${generated_dir}")

  add_custom_command(
    OUTPUT "${shader_cpp_file}"
    COMMAND "${CMAKE_COMMAND}"
      "-DSLANGC_EXECUTABLE=${SLANGC_EXECUTABLE}"
      "-DSHADER_SPIRV_FILE=${shader_spirv_file}"
      "-DSHADER_CPP_FILE=${shader_cpp_file}"
      "-DSHADER_SOURCES=${SHADER_SOURCES}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    DEPENDS ${SHADER_SOURCES}
    COMMENT "Compiling Slang shaders and embedding SPIR-V"
    VERBATIM
  )
  target_sources("${target}" PRIVATE "${shader_cpp_file}")
endfunction()