#include <stdint.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/pcnt_ll.h"
#include "hal/gpio_ll.h"
#include "nvs_flash.h"
#include "glue.h"
#include "target_constants.h"

static const char *TAG = "glue";

ADC_HandleTypeDef hadc1;
SPI_HandleTypeDef hspi1;
#if defined(SHIFTERBUTTONS) || defined(SPIBUTTONS)
SPI_HandleTypeDef hspi2;
#endif
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
CAN_HandleTypeDef hcan1;
TIM_HandleTypeDef htim1;
I2C_HandleTypeDef hi2c1;


void HAL_GPIO_Init(GPIO_TypeDef  *GPIOx, GPIO_InitTypeDef *GPIO_Init)
{
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_Init->Mode;
    io_conf.pin_bit_mask = (BIT64(LED_CLIP_Pin) | BIT64(LED_ERR_Pin) \
                            | BIT64(LED_SYS_Pin) | BIT64(DRV_ENABLE_Pin) \
                            | BIT64(DRV_BRAKE_Pin) | BIT64(CAN_S_Pin));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    return gpio_ll_get_level(&GPIO, GPIO_Pin);
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
    gpio_ll_set_level(&GPIO, GPIO_Pin, PinState);
}

void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    gpio_dev_t *hw = &GPIO;
    if (GPIO_Pin < 32) {
        hw->out ^= (1 << GPIO_Pin);

    } else {
        hw->out1.data ^= (1 << (GPIO_Pin - 32));
    }
}

void glue_pcnt_init(void)
{
    ESP_LOGI(TAG, "%s", __FUNCTION__);

#define PCNT_UNIT           PCNT_UNIT_0
#define PCNT_H_LIM_VAL      30000
#define PCNT_L_LIM_VAL     -30000
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config;

    // ch0
    pcnt_config.pulse_gpio_num  = ENCODER_A_Pin;
    pcnt_config.ctrl_gpio_num   = ENCODER_B_Pin;
    pcnt_config.channel         = PCNT_CHANNEL_0;
    pcnt_config.pos_mode        = PCNT_COUNT_INC;       // Count up on the positive edge
    pcnt_config.neg_mode        = PCNT_COUNT_DEC;       // Keep the counter value on the negative edge

    pcnt_config.lctrl_mode      = PCNT_MODE_REVERSE;    // Reverse counting direction if low
    pcnt_config.hctrl_mode      = PCNT_MODE_KEEP;       // Keep the primary counter mode if high
    pcnt_config.counter_h_lim   = PCNT_H_LIM_VAL;
    pcnt_config.counter_l_lim   = PCNT_L_LIM_VAL;
    pcnt_config.unit            = PCNT_UNIT;
    pcnt_unit_config(&pcnt_config);

    // ch1
    pcnt_config.pulse_gpio_num  = ENCODER_B_Pin;
    pcnt_config.ctrl_gpio_num   = ENCODER_A_Pin;
    pcnt_config.channel         = PCNT_CHANNEL_1;
    pcnt_config.pos_mode        = PCNT_COUNT_DEC;       // Count up on the positive edge
    pcnt_config.neg_mode        = PCNT_COUNT_INC;       // Keep the counter value on the negative edge
    pcnt_unit_config(&pcnt_config);


    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_UNIT, 100);
    pcnt_filter_enable(PCNT_UNIT);

    /* Enable events on zero, maximum and minimum limit values */
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_ZERO);
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_L_LIM);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_UNIT);
}

int16_t glue_pcnt_get_delta_value(void)
{
    int16_t count;
    pcnt_get_counter_value(PCNT_UNIT, &count);
    pcnt_counter_clear(PCNT_UNIT);
    return count;
}

void glue_pcnt_deinit(void)
{
    // pcnt_intr_disable(PCNT_UNIT);
    pcnt_counter_pause(PCNT_UNIT);
}


HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *hspi)
{
    if (hspi != &hspi1) {
        ESP_LOGE(TAG, "Can't init spi2 on esp32s2, don't have enough gpio");
        return HAL_ERROR;
    }

    esp_err_t ret;
    spi_bus_config_t buscfg1 = {
        .miso_io_num = SPI1_MISO_Pin,
        .mosi_io_num = SPI1_MOSI_Pin,
        .sclk_io_num = SPI1_SCK_Pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    hspi->Init.BaudRatePrescaler >>= 3;
    uint32_t freq = (84 * 1000 * 1000) / (uint32_t)(powf(2, hspi->Init.BaudRatePrescaler + 1));
    ESP_LOGI(TAG, "%s, freq=%d, Prescaler=%d", __FUNCTION__, freq, hspi->Init.BaudRatePrescaler);
    spi_device_interface_config_t devcfg1 = {
        .clock_speed_hz = freq,
        .mode = ((hspi->Init.CLKPolarity) << 1) | (hspi->Init.CLKPhase),
        .spics_io_num = SPI1_SS1_Pin,             //CS pin
        .queue_size = 7,                        //We want to be able to queue 7 transactions at a time
    };

    //Initialize the SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg1, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg1, &hspi1.Instance);
    ESP_ERROR_CHECK(ret);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
    t.length = 8 * Size;             //Command is 8 bits
    t.tx_buffer = pData;             //The data is the cmd itself
    ESP_LOGD(TAG, "%s, len=%d", __FUNCTION__, Size);
    ret = spi_device_polling_transmit(hspi->Instance, &t); //Transmit!
    assert(ret == ESP_OK);          //Should have had no issues.
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size)
{
    return HAL_SPI_Transmit(hspi, pData, Size, 500);
}

HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
    t.length = 8 * Size;               //Command is 8 bits
    t.tx_buffer = pTxData;             //The data is the cmd itself
    t.rx_buffer = pRxData;
    ESP_LOGD(TAG, "%s, len=%d", __FUNCTION__, Size);
    ret = spi_device_polling_transmit(hspi->Instance, &t); //Transmit!
    assert(ret == ESP_OK);          //Should have had no issues.
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *huart)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *huart)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *hi2c)
{
    hi2c->Instance = I2C_NUM_0;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DIN2_Pin,
        .scl_io_num = DIN3_Pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = hi2c->Init.ClockSpeed,
    };

    i2c_param_config(hi2c->Instance, &conf);

#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */

    esp_err_t ret = i2c_driver_install(hi2c->Instance, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    return ESP_OK == ret ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *hi2c)
{
    i2c_driver_delete(hi2c->Instance);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DevAddress, true);
    i2c_master_write(cmd, pData, Size, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(hi2c->Instance, cmd, pdMS_TO_TICKS(Timeout));
    i2c_cmd_link_delete(cmd);
    return ESP_OK == ret ? HAL_OK : HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DevAddress, true);
    i2c_master_write(cmd, pData, Size, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(hi2c->Instance, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    extern void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef * hi2c);
    HAL_I2C_MasterTxCpltCallback(hi2c); // call the callback

    return ESP_OK == ret ? HAL_OK : HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DevAddress, true);
    if (Size > 1) {
        i2c_master_read(cmd, pData, Size - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, pData + Size - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(hi2c->Instance, cmd, pdMS_TO_TICKS(Timeout));
    i2c_cmd_link_delete(cmd);
    return ESP_OK == ret ? HAL_OK : HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DevAddress, true);
    if (Size > 1) {
        i2c_master_read(cmd, pData, Size - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, pData + Size - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(hi2c->Instance, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    extern void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef * hi2c);
    HAL_I2C_MasterRxCpltCallback(hi2c); // call the callback
    return ESP_OK == ret ? HAL_OK : HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_ERROR;
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    ledc_stop(htim->speed_mode, htim->channel, 0);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}


void glue_board_init(void)
{
    ESP_LOGI(TAG, "Board name: %s", OPENFFBOARD_NAME);
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (BIT64(LED_CLIP_Pin) | BIT64(LED_ERR_Pin) \
                            | BIT64(LED_SYS_Pin) | BIT64(DRV_ENABLE_Pin) \
                            | BIT64(DRV_BRAKE_Pin) | BIT64(CAN_S_Pin));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pin_bit_mask = BIT64(BUTTON_A_Pin) \
                           | BIT64(DIN2_Pin) | BIT64(DIN1_Pin) | BIT64(DIN0_Pin);
#if DIN3_Pin < SOC_GPIO_PIN_COUNT
    io_conf.pin_bit_mask |= BIT64(DIN3_Pin);
#endif

    gpio_config(&io_conf);

    HAL_GPIO_WritePin(GPIOA, CAN_S_Pin, GPIO_PIN_RESET);
    glue_can_set_speed(&hcan1, 500000);
    HAL_CAN_Start(&hcan1);
}

void glue_ledc_config(ledc_channel_t channel, ledc_timer_bit_t duty_resolution, uint32_t freq_hz)
{
    ESP_LOGI(TAG, "%s, ch=%d, res=%d, f=%d", __FUNCTION__, channel, duty_resolution, freq_hz);
#define LEDC_TIMER              LEDC_TIMER_0
    // Set the LEDC peripheral configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = duty_resolution,
        .freq_hz          = freq_hz,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    const int pwm_pins[4] = {PWM1_Pin, PWM2_Pin, PWM3_Pin, PWM4_Pin};
    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = channel,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = pwm_pins[channel],
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void glue_ledc_set_duty(ledc_channel_t channel, uint32_t duty)
{
    ESP_LOGI(TAG, "%s, ch=%d, d=%d", __FUNCTION__, channel, duty);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

void glue_can_set_speed(CAN_HandleTypeDef *hcan, uint32_t rate)
{
    twai_timing_config_t t_50_config = TWAI_TIMING_CONFIG_50KBITS();
    twai_timing_config_t t_100_config = TWAI_TIMING_CONFIG_100KBITS();
    twai_timing_config_t t_125_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_timing_config_t t_250_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_timing_config_t t_500_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_timing_config_t t_1m_config = TWAI_TIMING_CONFIG_1MBITS();
    switch (rate) {
    case 50000:
        hcan->t_config = t_50_config;
        break;
    case 100000:
        hcan->t_config = t_100_config;
        break;
    case 125000:
        hcan->t_config = t_125_config;
        break;
    case 250000:
        hcan->t_config = t_250_config;
        break;
    case 500000:
        hcan->t_config = t_500_config;
        break;
    case 1000000:
        hcan->t_config = t_1m_config;
        break;

    default:
        ESP_LOGE(TAG, "Unsupported CAN rate");
        break;
    }
    ESP_LOGI(TAG, "set CAN rate to %d", rate);
}


static void twai_receive_task(void *arg)
{
    CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)arg;
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxBuf[8];
    while (1) {
        twai_message_t rx_message;
        //Receive message and print message data
        twai_receive(&rx_message, portMAX_DELAY);
        memcpy(rxBuf, rx_message.data, rx_message.data_length_code);
        rxHeader.DLC = rx_message.data_length_code;
        rxHeader.ExtId = rx_message.identifier;
        rxHeader.StdId = rx_message.identifier;
        rxHeader.RTR = rx_message.rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        rxHeader.Timestamp = HAL_GetTick();
        rxHeader.IDE = rx_message.extd ? CAN_ID_EXT : CAN_ID_STD;

        // for (int i = 0; i < rx_message.data_length_code; i++) {
        //     printf("%X, ", rx_message.data[i] );
        // } printf("\n");
        glue_can_receive_msg(hcan, rxBuf, &rxHeader);
    }
    vTaskDelete(NULL);
}


HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
    twai_status_info_t info;
    twai_get_status_info(&info);
    if (TWAI_STATE_RUNNING == info.state) {
        ESP_LOGW(TAG, "TWAI Driver is already running");
        return HAL_ERROR;
    }

    //Filter
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    //
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_Pin, CAN_RX_Pin, TWAI_MODE_NORMAL);
    //Install TWAI driver
    esp_err_t ret = twai_driver_install(&g_config, &hcan->t_config, &f_config);
    if (ESP_ERR_INVALID_STATE == ret) {
        ESP_LOGW(TAG, "TWAI Driver is already installed");
    }
    ESP_LOGI(TAG, "TWAI Driver installed");
    ESP_ERROR_CHECK(twai_start());
    xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 4096, hcan, 6, &hcan->task, 0);
    ESP_LOGI(TAG, "TWAI Driver started");
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, CAN_FilterTypeDef *sFilterConfig)
{
    ESP_LOGW(TAG, "unsupport config filter");
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_Stop(CAN_HandleTypeDef *hcan)
{
    vTaskDelete(hcan->task);
    ESP_ERROR_CHECK(twai_stop());
    ESP_ERROR_CHECK(twai_driver_uninstall());
    ESP_LOGI(TAG, "Driver uninstalled");
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *pHeader, uint8_t aData[], uint32_t *pTxMailbox)
{
    twai_message_t msg = {0};
    memcpy(msg.data, aData, pHeader->DLC);
    msg.rtr = (pHeader->RTR == CAN_RTR_REMOTE);
    msg.data_length_code = pHeader->DLC;
    msg.extd = (pHeader->IDE == CAN_ID_EXT);
    msg.identifier = pHeader->ExtId;
    ESP_LOGD(TAG, "send id=0x%x, dlc=%d, extd=%d, rtr=%d, ss=%d",
             msg.identifier,
             msg.data_length_code,
             (int)msg.extd,
             (int)msg.rtr,
             (int)msg.ss);
    // printf("send data:");
    // for (int i = 0; i < msg.data_length_code; i++) {
    //     printf("%X, ", msg.data[i] );
    // } printf("\n");
    esp_err_t ret = twai_transmit(&msg, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "twai error %s", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *hcan, uint32_t TxMailboxes)
{
    twai_clear_transmit_queue();
    twai_clear_receive_queue();
    return HAL_OK;
}
HAL_StatusTypeDef HAL_CAN_ResetError(CAN_HandleTypeDef *hcan)
{
    ESP_LOGW(TAG, "%s: Unsupported", __FUNCTION__);
    return HAL_OK;
}

static void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask, adc_channel_t *channel, uint8_t channel_num)
{
#define GET_UNIT(x)        ((x>>3) & 0x1)
    adc_digi_init_config_t adc_dma_config = {
        .max_store_buf_size = channel_num * 4 * 2,
        .conv_num_each_intr = channel_num * 4 * 2,
        .adc1_chan_mask = adc1_chan_mask,
        .adc2_chan_mask = adc2_chan_mask,
    };
    ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));

    adc_digi_configuration_t dig_cfg = {
        .conv_limit_en = 0,
        .conv_limit_num = adc_dma_config.conv_num_each_intr,
        .sample_freq_hz = 1 * 1000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
#ifdef CONFIG_IDF_TARGET_ESP32S3
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
#elif defined CONFIG_IDF_TARGET_ESP32S2
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
#endif
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        uint8_t unit = GET_UNIT(channel[i]);
        uint8_t ch = channel[i] & 0x7;
        adc_pattern[i].atten = ADC_ATTEN_DB_11;
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
}

HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *hadc, uint32_t *pData, uint32_t Length)
{
    if (3 != Length) {
        ESP_LOGE(TAG, "ADC channels error");
        return HAL_ERROR;
    }

    static uint16_t adc1_chan_mask = BIT(0) | BIT(1) | BIT(2);
    adc_channel_t channel[3] = {ADC1_CHANNEL_0, ADC1_CHANNEL_1, ADC1_CHANNEL_2};
    continuous_adc_init(adc1_chan_mask, 0, channel, sizeof(channel) / sizeof(adc_channel_t));
    adc_digi_start();

    // volatile uint32_t* getAnalogBuffer(ADC_HandleTypeDef* hadc,uint8_t* chans);
    // while (1)
    // {
    //     uint8_t c=0;
    //     uint32_t *p = getAnalogBuffer(NULL, &c);
    //     ESP_LOGI(TAG, "%d, %d, %d", p[0], p[1], p[2]);
    //     vTaskDelay(10);
    // }

    return HAL_OK;
}

void Error_Handler(void)
{
    ESP_LOGE(TAG, "Error_Handler!!!");
    assert(false);
}


void RebootDFU(void)
{
    ESP_LOGE(TAG, "Unsupport DFU");
}

/**
  * @brief  Returns the device revision identifier.
  * @retval Device revision identifier
  */
uint32_t HAL_GetREVID(void)
{
    return 0x01;
}

/**
  * @brief  Returns the device identifier.
  * @retval Device identifier
  */
uint32_t HAL_GetDEVID(void)
{
    return FW_DEVID;
}

uint32_t HAL_GetUIDw0(void)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    uint32_t ret = (uint32_t)mac[1] << 8 | mac[0];
    return ret;
}
uint32_t HAL_GetUIDw1(void)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    uint32_t ret = (uint32_t)mac[3] << 8 | mac[2];
    return ret;
}
uint32_t HAL_GetUIDw2(void)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    uint32_t ret = (uint32_t)mac[5] << 8 | mac[4];
    return ret;
}
