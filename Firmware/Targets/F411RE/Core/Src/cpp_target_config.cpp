#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi2;

SPIPort external_spi{hspi2};
OutputPin external_spi_cs1{*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin};
OutputPin external_spi_cs2{*SPI2_SS2_GPIO_Port, SPI2_SS2_Pin};
OutputPin external_spi_cs3{*SPI2_SS3_GPIO_Port, SPI2_SS3_Pin};