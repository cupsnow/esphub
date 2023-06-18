set(srcs
  "eh_main.c"
  "eh_led.c"
  "eh_btn.c"
  "dw_util.c"
  "dw_looper.c"
)

set(incs
  ""
)

idf_component_register(
  SRCS "${srcs}"
  INCLUDE_DIRS "${incs}")

idf_build_set_property(COMPILE_DEFINITIONS
  "-DALOE_SYS_ESP32=1" APPEND)

  