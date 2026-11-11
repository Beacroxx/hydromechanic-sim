function(_generate_embedded_textures)
  foreach(required_var TEXTURE_CPP_FILE TEXTURE_SOURCES)
    if(NOT DEFINED ${required_var})
      message(FATAL_ERROR "Missing required variable: ${required_var}")
    endif()
  endforeach()

  file(WRITE "${TEXTURE_CPP_FILE}" "#include \"textures.hpp\"\n\n")

  set(TEXTURE_NAMES)
  set(TEXTURE_INITIALIZERS)
  foreach(TEXTURE_SOURCE IN LISTS TEXTURE_SOURCES)
    get_filename_component(TEXTURE_NAME "${TEXTURE_SOURCE}" NAME_WE)
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" TEXTURE_NAME "${TEXTURE_NAME}")
    if(TEXTURE_NAME MATCHES "^[0-9]")
      string(PREPEND TEXTURE_NAME "_")
    endif()

    list(FIND TEXTURE_NAMES "${TEXTURE_NAME}" TEXTURE_NAME_INDEX)
    if(NOT TEXTURE_NAME_INDEX EQUAL -1)
      message(FATAL_ERROR "Texture filenames produce duplicate C++ name: ${TEXTURE_NAME}")
    endif()
    list(APPEND TEXTURE_NAMES "${TEXTURE_NAME}")

    file(READ "${TEXTURE_SOURCE}" TEXTURE_HEX HEX)
    string(LENGTH "${TEXTURE_HEX}" TEXTURE_HEX_LENGTH)
    if(TEXTURE_HEX_LENGTH EQUAL 0)
      message(FATAL_ERROR "Texture image is empty: ${TEXTURE_SOURCE}")
    endif()
    math(EXPR TEXTURE_HEX_REMAINDER "${TEXTURE_HEX_LENGTH} % 2")
    if(TEXTURE_HEX_REMAINDER)
      message(FATAL_ERROR "Texture image has an incomplete byte: ${TEXTURE_SOURCE}")
    endif()

    file(APPEND "${TEXTURE_CPP_FILE}"
      "extern const unsigned char texture_${TEXTURE_NAME}[] = {\n"
    )

    foreach(TEXTURE_OFFSET RANGE 0 ${TEXTURE_HEX_LENGTH} 128)
      if(TEXTURE_OFFSET GREATER_EQUAL TEXTURE_HEX_LENGTH)
        break()
      endif()
      math(EXPR TEXTURE_CHUNK_LENGTH "${TEXTURE_HEX_LENGTH} - ${TEXTURE_OFFSET}")
      if(TEXTURE_CHUNK_LENGTH GREATER 128)
        set(TEXTURE_CHUNK_LENGTH 128)
      endif()
      string(SUBSTRING "${TEXTURE_HEX}" ${TEXTURE_OFFSET} ${TEXTURE_CHUNK_LENGTH} TEXTURE_CHUNK)
      string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1, " TEXTURE_BYTES "${TEXTURE_CHUNK}")
      file(APPEND "${TEXTURE_CPP_FILE}" "  ${TEXTURE_BYTES}\n")
    endforeach()

    file(APPEND "${TEXTURE_CPP_FILE}"
      "};\n"
      "extern const std::size_t texture_${TEXTURE_NAME}_bytes = sizeof(texture_${TEXTURE_NAME});\n\n"
    )
    list(APPEND TEXTURE_INITIALIZERS
      "{ \"${TEXTURE_NAME}\", texture_${TEXTURE_NAME}, texture_${TEXTURE_NAME}_bytes }"
    )
  endforeach()

  list(LENGTH TEXTURE_NAMES TEXTURE_COUNT)
  string(JOIN ",\n  " TEXTURE_INITIALIZER_LIST ${TEXTURE_INITIALIZERS})
  file(APPEND "${TEXTURE_CPP_FILE}"
    "extern const EmbeddedTexture embeddedTextures[] = {\n"
    "  ${TEXTURE_INITIALIZER_LIST}\n"
    "};\n"
    "extern const std::size_t embeddedTextureCount = ${TEXTURE_COUNT};\n"
  )
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
  _generate_embedded_textures()
  return()
endif()

function(embed_textures target)
  cmake_parse_arguments(PARSE_ARGV 1 TEXTURE "" "" "SOURCES")
  if(NOT TEXTURE_SOURCES)
    message(FATAL_ERROR "embed_textures requires at least one source in SOURCES")
  endif()

  set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(texture_cpp_file "${generated_dir}/embedded_textures.cpp")
  file(MAKE_DIRECTORY "${generated_dir}")

  add_custom_command(
    OUTPUT "${texture_cpp_file}"
    COMMAND "${CMAKE_COMMAND}"
      "-DTEXTURE_CPP_FILE=${texture_cpp_file}"
      "-DTEXTURE_SOURCES=${TEXTURE_SOURCES}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    DEPENDS ${TEXTURE_SOURCES}
    COMMENT "Embedding texture images"
    VERBATIM
  )
  target_sources("${target}" PRIVATE "${texture_cpp_file}")
endfunction()
