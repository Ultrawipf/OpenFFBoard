#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi2;

SPIPort external_spi{hspi2};

const std::vector<OutputPin*>& getExternalSPI_CSPins() {
    static OutputPin cs1{*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin};
    static const std::vector<OutputPin*> cs_pins{&cs1};

    return cs_pins;
}