
set(FFB_DIR ../../../FFBoard)

idf_component_register(SRC_DIRS "." "${FFB_DIR}/Src" "${FFB_DIR}/UserExtensions/Src"
                       INCLUDE_DIRS "." "${FFB_DIR}/Inc" "${FFB_DIR}/UserExtensions/Inc" 
                      )

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17" "-fexceptions")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wfatal-errors -Wno-ignored-qualifiers -Wno-unused-value)
