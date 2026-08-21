# Runs the host twice and fails unless the two frames differ.
#
# Every other case here asserts something about **one** frame — it drew, it is
# not flat, its ink is inside a band. The three states of a value control cannot
# be asked that way: what has to be true is that they are not the same picture,
# and an outline on a 480x800 panel does not move an integer ink percentage.
#
# `cmake -E compare_files` exits zero when two files match, so this inverts it —
# and an inverted assertion is one that a **missing** file passes. Both frames
# are checked to exist first: two absent paths "differ", so a mistyped argument
# that stops the host writing anything would otherwise turn both of these cases
# green while asserting nothing at all.
#
# Why the callers crop: `Display::writeBmp` in `Display.h`.
#
#   HOST     the binary
#   ARGS_A   semicolon-separated arguments for the first run
#   ARGS_B   for the second
#   OUT_DIR  where the two frames are written

foreach(required HOST ARGS_A ARGS_B OUT_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "frames_differ.cmake: -D ${required}= is required")
  endif()
endforeach()

set(A "${OUT_DIR}/frames_differ_a.bmp")
set(B "${OUT_DIR}/frames_differ_b.bmp")

# Removed first, so a frame left by an earlier case cannot stand in for one this
# run failed to write — which the existence check above would then accept.
file(REMOVE "${A}" "${B}")

execute_process(COMMAND "${HOST}" ${ARGS_A} --out "${A}" RESULT_VARIABLE a_code)
if(NOT a_code EQUAL 0)
  message(FATAL_ERROR "the first run failed with ${a_code}")
endif()

execute_process(COMMAND "${HOST}" ${ARGS_B} --out "${B}" RESULT_VARIABLE b_code)
if(NOT b_code EQUAL 0)
  message(FATAL_ERROR "the second run failed with ${b_code}")
endif()

foreach(frame "${A}" "${B}")
  if(NOT EXISTS "${frame}")
    message(FATAL_ERROR "no frame was written to ${frame}")
  endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${A}" "${B}"
                RESULT_VARIABLE same)
if(same EQUAL 0)
  message(FATAL_ERROR
    "the two runs painted the same frame.\n"
    "  ${ARGS_A}\n  ${ARGS_B}\n"
    "A state a person cannot see is a refresh spent saying nothing.")
endif()
