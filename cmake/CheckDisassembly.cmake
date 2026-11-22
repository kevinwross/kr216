# Runs kr216-dis on a committed image and checks the text a reader relies on.
#
# Required: DISASSEMBLER, IMAGE

if(NOT DEFINED DISASSEMBLER OR NOT DEFINED IMAGE)
    message(FATAL_ERROR "DISASSEMBLER and IMAGE are required")
endif()
if(NOT EXISTS "${DISASSEMBLER}")
    message(FATAL_ERROR "disassembler not found: ${DISASSEMBLER}")
endif()
if(NOT EXISTS "${IMAGE}")
    message(FATAL_ERROR "image not found: ${IMAGE}")
endif()

execute_process(
    COMMAND "${DISASSEMBLER}" "${IMAGE}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "kr216-dis failed (${result}): ${error}")
endif()

string(REPLACE "\r\n" "\n" output "${output}")
if(NOT output MATCHES "; \\.text at 0200, 4 bytes")
    message(FATAL_ERROR "missing .text header in:\n${output}")
endif()
if(NOT output MATCHES "0200: 52 2A 00 +ld r2, #0x2A")
    message(FATAL_ERROR "missing ld instruction in:\n${output}")
endif()
if(NOT output MATCHES "0203: 73 +sys")
    message(FATAL_ERROR "missing sys instruction in:\n${output}")
endif()
