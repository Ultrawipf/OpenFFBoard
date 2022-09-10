#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi1;

static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin)};
SPIPort external_spi{hspi2,external_spi_cspins,true};

static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin), OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),OutputPin(*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin)};
SPIPort motor_spi{hspi1,motor_spi_cspins,false};

#ifdef UART_PORT_EXT
extern UART_HandleTypeDef UART_PORT_EXT;
UARTPort external_uart{UART_PORT_EXT};
#endif

#ifdef I2C_PORT
extern I2C_HandleTypeDef I2C_PORT;
I2CPort i2cport{I2C_PORT};
#endif

#ifdef DEBUGPIN
const OutputPin debugpin = OutputPin(*GP1_GPIO_Port, GP1_Pin);
#endif
