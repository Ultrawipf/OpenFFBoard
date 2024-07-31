#include "cpp_target_config.h"

extern SPI_HandleTypeDef hspi1;

static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin)};
SPIPort external_spi{hspi1,external_spi_cspins,48000000,true};

//static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin), OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),OutputPin(*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin)};
//SPIPort motor_spi{hspi1,motor_spi_cspins,96000000,false};

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


#ifdef PWMDRIVER
const PWMConfig pwmTimerConfig =
{
	.channel_1 = TIM_CHANNEL_1,
	.channel_2 = TIM_CHANNEL_2,
	.channel_3 = TIM_CHANNEL_3,
	.channel_4 = TIM_CHANNEL_4,

	.pwm_chan = 4,
	.dir_chan = 3,
	.dir_chan_n = 1,

	.centerpwm_chan = 4,

	.rcpwm_chan = 4,

	.dualpwm1 = 4,
	.dualpwm2 = 3,

	.timer = &TIM_PWM,
	.timerFreq = 96000000
};
#endif
