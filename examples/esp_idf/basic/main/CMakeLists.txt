idf_component_register(
  SRCS
    "main.cpp"
    "Opt4001IdfArduinoShim.cpp"
    "Opt4001IdfI2cTransport.cpp"
    "../../../01_basic_bringup_cli/main.cpp"
  INCLUDE_DIRS "." "../../../.."
  REQUIRES OPT4001 esp_driver_i2c esp_driver_gpio esp_driver_uart esp_timer freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
