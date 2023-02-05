#include "cpp_target_config.h"

#if defined(SHIFTERBUTTONS) || defined(SPIBUTTONS)
extern SPI_HandleTypeDef hspi2;
static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin), OutputPin(*SPI2_SS2_GPIO_Port, SPI2_SS2_Pin),OutputPin(*SPI2_SS3_GPIO_Port, SPI2_SS3_Pin)};
SPIPort external_spi{hspi2,external_spi_cspins,true};
#endif

static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin), OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),OutputPin(*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin)};
extern SPI_HandleTypeDef hspi1;
SPIPort motor_spi{hspi1,motor_spi_cspins,false};

#ifdef EXT3_SPI_PORT
static const std::vector<OutputPin> ext3_spi_cspins{OutputPin(*SPI3_SS1_GPIO_Port, SPI3_SS1_Pin), OutputPin(*SPI3_SS2_GPIO_Port, SPI3_SS2_Pin),OutputPin(*SPI3_SS3_GPIO_Port, SPI3_SS3_Pin)};
extern SPI_HandleTypeDef EXT3_SPI_PORT;
SPIPort ext3_spi{hspi3,ext3_spi_cspins,true};
#endif

#ifdef UART_PORT_MOTOR
extern UART_HandleTypeDef UART_PORT_MOTOR;
UARTPort motor_uart{UART_PORT_MOTOR};
#endif

#ifdef UART_PORT_EXT
extern UART_HandleTypeDef UART_PORT_EXT;
UARTPort external_uart{UART_PORT_EXT};
#endif

#ifdef I2C_PORT
extern I2C_HandleTypeDef I2C_PORT;
I2CPort i2cport{I2C_PORT};
#endif

#ifdef CANBUS
/*
 * Can BTR register for different speed configs
 * 50, 100, 125, 250, 500, 1000 kbit
 */
CANPort canport{CANPORT};

#endif

#ifdef PWMDRIVER
// CCR and channels must match!
const PWMConfig pwmTimerConfig =
{
	.channel_1 = TIM_CHANNEL_1,
	.channel_2 = TIM_CHANNEL_2,
	.channel_3 = TIM_CHANNEL_3,
	.channel_4 = TIM_CHANNEL_4,

	.pwm_chan = 1,
	.dir_chan = 3,
	.dir_chan_n = 4,

	.centerpwm_chan = 1,

	.rcpwm_chan = 1,

	.dualpwm1 = 1,
	.dualpwm2 = 2,

	.timer = &TIM_PWM,
	.timerFreq = APB_CLK_FREQ
};
#endif
