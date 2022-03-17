set(COMPONENT_SRCS wifi.c)
set(COMPONENT_ADD_INCLUDEDIRS "./include" EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
set(COMPONENT_REQUIRES nvs_flash fatfs mdns app_update esp_https_ota esp-tls)
register_component()