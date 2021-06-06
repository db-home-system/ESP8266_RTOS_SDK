idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "app_main.c" "connect.c" "mqtt_mgr.c"
                    INCLUDE_DIRS "."
                    REQUIRES "tcpip_adapter"
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
