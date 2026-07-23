idf_component_register(
  SRCS
    "main.cpp"
    "Opt4001IdfI2cTransport.cpp"
  PRIV_INCLUDE_DIRS "."
  REQUIRES OPT4001 esp_driver_i2c esp_driver_gpio esp_timer freertos vfs
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
