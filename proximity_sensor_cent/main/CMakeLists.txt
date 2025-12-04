set(srcs "main.c" "calculation.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ".")
