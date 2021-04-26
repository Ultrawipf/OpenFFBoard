#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi1;

static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin)};
SPIPort external_spi{hspi2,external_spi_cspins,true};

static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin)};
SPIPort motor_spi{hspi1,motor_spi_cspins,false};

//const std::vector<OutputPin*>& getExternalSPI_CSPins() {
//    static OutputPin cs1{*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin};
//    static const std::vector<OutputPin*> cs_pins{&cs1};
//
//    return cs_pins;
//}
