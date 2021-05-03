#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi1;

static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin)};
SPIPort external_spi{hspi2,external_spi_cspins,true};

static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin), OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),OutputPin(*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin)};
SPIPort motor_spi{hspi1,motor_spi_cspins,false};
