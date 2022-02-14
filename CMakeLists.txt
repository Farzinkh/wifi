set(COMPONENT_SRCS wifi.c)
set(COMPONENT_ADD_INCLUDEDIRS "./include")
set(COMPONENT_REQUIRES nvs_flash fatfs mdns)
register_component()