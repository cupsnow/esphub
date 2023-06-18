set(srcs
   "aloe_util.c"
   "aloe_sys.c"
   "aloe_unitest.c"
   "aloe_esp32/aloe_sys_esp32.c"
)

set(incs
   "."
)

idf_component_register(SRCS "${srcs}"
  INCLUDE_DIRS "${incs}"
)

