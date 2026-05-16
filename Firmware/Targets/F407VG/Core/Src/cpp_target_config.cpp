#include "cpp_target_config.h"
#include <array>
extern SPI_HandleTypeDef hspi2;

static const std::vector<OutputPin> external_spi_cspins{OutputPin(*SPI2_NSS_GPIO_Port, SPI2_NSS_Pin), OutputPin(*SPI2_SS2_GPIO_Port, SPI2_SS2_Pin),OutputPin(*SPI2_SS3_GPIO_Port, SPI2_SS3_Pin)};
SPIPort external_spi{hspi2,external_spi_cspins,42000000,true};


static const std::vector<OutputPin> motor_spi_cspins{OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin), OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),OutputPin(*SPI1_SS3_GPIO_Port, SPI1_SS3_Pin)};
extern SPI_HandleTypeDef hspi1;
SPIPort motor_spi{hspi1,motor_spi_cspins,84000000,false};

#ifdef EXT3_SPI_PORT
static const std::vector<OutputPin> ext3_spi_cspins{OutputPin(*SPI3_SS1_GPIO_Port, SPI3_SS1_Pin), OutputPin(*SPI3_SS2_GPIO_Port, SPI3_SS2_Pin),OutputPin(*SPI3_SS3_GPIO_Port, SPI3_SS3_Pin)};
extern SPI_HandleTypeDef EXT3_SPI_PORT;
SPIPort ext3_spi{hspi3,ext3_spi_cspins,42000000,true};
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
// i2c speed configs
const I2C_InitTypeDef i2cPreset_10000 = {100000,I2C_DUTYCYCLE_2,0,I2C_ADDRESSINGMODE_7BIT,I2C_DUALADDRESS_DISABLE,0,I2C_GENERALCALL_DISABLE,I2C_NOSTRETCH_DISABLE};
const I2C_InitTypeDef i2cPreset_40000 = {400000,I2C_DUTYCYCLE_2,0,I2C_ADDRESSINGMODE_7BIT,I2C_DUALADDRESS_DISABLE,0,I2C_GENERALCALL_DISABLE,I2C_NOSTRETCH_DISABLE};
const auto i2cpresets = std::to_array<I2CPortHardwareConfig::I2CPortHardwareConfig_preset>({{i2cPreset_10000,"100kb/s"},{i2cPreset_40000,"400kb/s"}});
const I2CPortHardwareConfig i2cHwConf = I2CPortHardwareConfig(true, i2cpresets);

extern I2C_HandleTypeDef I2C_PORT;
I2CPort i2cport{I2C_PORT,i2cHwConf};


#endif

#ifdef GPIO_MOTOR
const OutputPin gpMotor{*DRV_GP1_GPIO_Port,DRV_GP1_Pin};
#endif

#ifdef CANBUS
/*
 * Can BTR register for different speed configs
 * 50, 100, 125, 250, 500, 1000 kbit
 */
#include "CANPort2B.h"
const auto canpresetentries = std::to_array<CANPortHardwareConfig::PresetEntry>({{0x001b0037,50000,"50k"},{0x001b001b,100000,"100k"},{0x001c0014,125000,"125k"},{0x001a000b,250000,"250k"},{0x001a0005,500000,"500k"},{0x001a0002,1000000,"1000k"}});
CANPortHardwareConfig canpresets = CANPortHardwareConfig(true,canpresetentries);
const OutputPin canSilentPin = OutputPin(*CAN_S_GPIO_Port, CAN_S_Pin);
CANPort_2B canport_base{hcan1,canpresets,&canSilentPin};
CANPort& canport = static_cast<CANPort&>(canport_base);

#endif
const OutputPin debugpin = OutputPin(*GP1_GPIO_Port, GP1_Pin);

#ifdef PWMDRIVER
const PWMConfig MotorPWM::timerConfig =
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
	.timerFreq = 168000000
};
#endif
