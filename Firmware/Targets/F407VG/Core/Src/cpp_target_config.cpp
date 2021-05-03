#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi2;

static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin), OutputPin(*SPI2_SS2_GPIO_Port, SPI2_SS2_Pin),OutputPin(*SPI2_SS3_GPIO_Port, SPI2_SS3_Pin)};
SPIPort external_spi{hspi2,external_spi_cspins,true};

//const std::vector<OutputPin*>& getExternalSPI_CSPins() {
//    static OutputPin cs1{*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin};
//    static OutputPin cs2{*SPI2_SS2_GPIO_Port, SPI2_SS2_Pin};
//    static OutputPin cs3{*SPI2_SS3_GPIO_Port, SPI2_SS3_Pin};
//    static const std::vector<OutputPin*> cs_pins{&cs1, &cs2, &cs3};
//
//    return cs_pins;
//}

static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin), OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),OutputPin(*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin)};
extern SPI_HandleTypeDef hspi1;
SPIPort motor_spi{hspi1,motor_spi_cspins,false};
//const std::vector<OutputPin*>& getMotorSPI_CSPins() {
//    static OutputPin cs1{*SPI1_NSS_GPIO_Port, SPI1_NSS_Pin};
//    static OutputPin cs2{*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin};
//    static OutputPin cs3{*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin};
//    static const std::vector<OutputPin*> cs_pins{&cs1, &cs2, &cs3};
//
//    return cs_pins;
//}


#ifdef CANBUS
/*
 * Can BTR register for different speed configs
 * 50, 100, 125, 250, 500, 1000 kbit
 */

const uint32_t canSpeedBTR_preset[] = { 0x001b0037,0x001b001b,0x001c0014,0x001a000b,0x001a0005,0x001a0002};

#endif
