#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi3;

SPIPort external_spi{hspi3};

const std::vector<OutputPin*>& getExternalSPI_CSPins() {
    static OutputPin cs1{*SPI3_CS1_GPIO_Port, SPI3_CS1_Pin};
    static OutputPin cs2{*SPI3_CS2_GPIO_Port, SPI3_CS2_Pin};
    static OutputPin cs3{*SPI3_CS3_GPIO_Port, SPI3_CS3_Pin};
    static const std::vector<OutputPin*> cs_pins{&cs1, &cs2, &cs3};

    return cs_pins;
}
