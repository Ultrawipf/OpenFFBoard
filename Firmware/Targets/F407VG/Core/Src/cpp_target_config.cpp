#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi3;

SPIPort external_spi{hspi3};
OutputPin external_spi_cs1{*SPI3_CS1_GPIO_Port, SPI3_CS1_Pin};
OutputPin external_spi_cs2{*SPI3_CS2_GPIO_Port, SPI3_CS2_Pin};
OutputPin external_spi_cs3{*SPI3_CS3_GPIO_Port, SPI3_CS3_Pin};
