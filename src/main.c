#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../inc/uart.h"
#include "../inc/pid.h"
#include "../inc/crc16.h"
#include "../inc/bme280.h"


// Códigos do Protocolo de Comunicação
unsigned char solicita_temp_interna[7] = {0x01, 0x23, 0xC1, 1, 6, 1, 7};
unsigned char solicita_temp_referencia[7] = {0x01, 0x23, 0xC2, 1, 6, 1, 7};
unsigned char le_comando_usuario[7] = {0x01, 0x23, 0xC3, 1, 6, 1, 7};
unsigned char sinal_controle[7] = {0x01, 0x16, 0xD1, 1, 6, 1, 7};
unsigned char solicita_temperatura_ambiente[7] = {0x01, 0x16, 0xD6, 1, 6, 1, 7};
unsigned char sinal_referencia_FLOAT[7] = {0x01, 0x16, 0xD2, 1, 6, 1, 7};
unsigned char sinal_temp_amb_FLOAT[7] = {0x01, 0x16, 0xD6, 1, 6, 1, 7};
unsigned char estado_sistema_ligado[8] = {0x01, 0x16, 0xD3, 1, 6, 1, 7, 1};
unsigned char estado_sistema_desligado[8] = {0x01, 0x16, 0xD3, 1, 6, 1, 7, 0};
unsigned char estado_sistema_funcionando[8] = {0x01, 0x16, 0xD5, 1, 6, 1, 7, 1};
unsigned char estado_sistema_parado[8] = {0x01, 0x16, 0xD5, 1, 6, 1, 7, 0};

int entrada_usuario = 0, forno_ligado = 0, forno_funcionando = 0, tempo = 1, menu = 0, forno_aquecido = 0, forno_resfriado = 1;
double controle_pid = 0.0;
float temperatura_interna = 0.0, temperatura_referencia = 0.0, temperatura_ambiente = 0.0;
const int GPIO_resistor = 4, GPIO_vetoinha = 5;

struct identifier
{
    uint8_t dev_addr;
    int8_t fd;
};

void liga_funciona_forno(int modo);
void desliga_para_forno(int modo);
void desliga_sistema(int sig);
void user_delay_us(uint32_t period, void *intf_ptr);
float print_sensor_data(struct bme280_data *comp_data);
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t len, void *intf_ptr);
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr);
float stream_sensor_data_forced_mode(struct bme280_dev *dev);

int main(int argc, char* argv[]){

    uart_init();

    signal(SIGINT, desliga_sistema);

    if (wiringPiSetup() == -1)
        exit(1);


    if (softPwmCreate(GPIO_resistor, 0, 100) == 0){
        printf("PWM do resistor criado!\n");
    }
    if (softPwmCreate(GPIO_vetoinha, 0, 100) == 0){
        printf("PWM da ventoinha criado!\n");
    }

    pid_configura_constantes(30.0, 0.2, 400.0);

    desliga_para_forno(1);
    desliga_para_forno(2);

    temperatura_referencia = solicita_temperatura(solicita_temp_referencia);
    pid_atualiza_referencia(temperatura_referencia);

    
    float resultado;
    struct bme280_dev dev;
    struct identifier id;

    int8_t rslt = BME280_OK;

    if (argc < 2) {
        fprintf(stderr, "Missing argument for i2c bus.\n");
        exit(1);
    }

    if ((id.fd = open(argv[1], O_RDWR)) < 0) {
        fprintf(stderr, "Failed to open the i2c bus %s\n", argv[1]);
        exit(1);
    }

    id.dev_addr = BME280_I2C_ADDR_PRIM;

    if (ioctl(id.fd, I2C_SLAVE, id.dev_addr) < 0) {
        fprintf(stderr, "Failed to acquire bus access and/or talk to slave.\n");
        exit(1);
    }

    dev.intf = BME280_I2C_INTF;
    dev.read = user_i2c_read;
    dev.write = user_i2c_write;
    dev.delay_us = user_delay_us;

    dev.intf_ptr = &id;

    rslt = bme280_init(&dev);
    if (rslt != BME280_OK) {
        fprintf(stderr, "Failed to initialize the device (code %+d).\n", rslt);
        exit(1);
    }

    FILE *fpt;

    while(1){


        delay(1000);
        struct tm *data_hora_atual;
        time_t segundos; 
        time(&segundos); 
        data_hora_atual = localtime(&segundos); 

        entrada_usuario = solicita_comando_usuario(le_comando_usuario);

        switch(entrada_usuario){
            case 161:
                liga_funciona_forno(1);
                forno_resfriado = 1;
                break;
            case 162: 
                desliga_para_forno(1);
                desliga_sistema(0);
                break;
            case 163:
                liga_funciona_forno(2);
                forno_resfriado = 1;
                break;
            case 164:
                desliga_para_forno(2);
                forno_aquecido = 0;
                forno_resfriado = 0;
                tempo = 1;
                break;
            default:
                break;
        }

        if(forno_ligado == 1){
            fpt = fopen("data/log.csv", "a+");

            if (fpt == NULL){
                printf("Erro no CSV\n");
            }

            if (forno_funcionando == 1 && forno_resfriado == 1){

                temperatura_referencia = solicita_temperatura(solicita_temp_referencia);
                pid_atualiza_referencia(temperatura_referencia);
                temperatura_interna = solicita_temperatura(solicita_temp_interna);
                temperatura_ambiente = stream_sensor_data_forced_mode(&dev);

                if (temperatura_referencia - temperatura_interna >= -0.1 && temperatura_referencia - temperatura_interna <= 0.1 && forno_aquecido == 0){
                    forno_aquecido = 1;
                    printf("Forno chegou na temperatura desejada!\n");
                }

                controle_pid = pid_controle(temperatura_interna);

                if(controle_pid > -40 && controle_pid < 0)
                    controle_pid = -40;

                envia_sinal_controle(sinal_controle, controle_pid);

                if(controle_pid < 0){
                    softPwmWrite(GPIO_resistor, 0);
                    delay(0.7);
                    softPwmWrite(GPIO_vetoinha, controle_pid*(-1));
                    delay(0.7);
                }
                else if(controle_pid > 0){
                    softPwmWrite(GPIO_vetoinha, 0);
                    delay(0.7);
                    softPwmWrite(GPIO_resistor, controle_pid);
                    delay(0.7);
                }
            }  

            fprintf(fpt, "%d/%d/%d,%d:%d:%d,%.2f,%.2f,%.2f,%.2lf\n", data_hora_atual->tm_mday, data_hora_atual->tm_mon + 1, data_hora_atual->tm_year + 1900, data_hora_atual->tm_hour, data_hora_atual->tm_min, data_hora_atual->tm_sec, temperatura_interna, temperatura_ambiente, temperatura_referencia, controle_pid);
            printf("Controle por Tela - %d/%d/%d,%d:%d:%d - TI = %.2f,TA = %.2f,TR = %.2f, PID = %.2lf\n", data_hora_atual->tm_mday, data_hora_atual->tm_mon + 1, data_hora_atual->tm_year + 1900, data_hora_atual->tm_hour, data_hora_atual->tm_min, data_hora_atual->tm_sec, temperatura_interna, temperatura_ambiente, temperatura_referencia, controle_pid);
        
        }
    }   

    uart_close();
    return 0;
}

void liga_funciona_forno(int modo){
    if(modo == 1){
        if(solicita_estado_sistema(estado_sistema_ligado) == 1){
            printf("Forno ligado.\n");
            forno_ligado = 1;
        }
        else{
            printf("Erro ao ligar forno.\n");
        }
    }
    else if(modo == 2){
        if(solicita_estado_sistema(estado_sistema_funcionando)){
            printf("Forno funcionando.\n");
            forno_funcionando = 1;
        }
        else{
            printf("Falha no funcionamento do forno.\n");
        }
    }
}

void desliga_para_forno(int modo){
    if(modo == 1){
        if(solicita_estado_sistema(estado_sistema_desligado) == 1){
            printf("Forno desligado.\n");
            forno_ligado = 0;
        }
        else{
            printf("Erro ao desligar forno.\n");
        }
    }
    else if(modo == 2){
        if(solicita_estado_sistema(estado_sistema_parado)){
            printf("Forno parado.\n");
            forno_funcionando = 0;
        }
        else{
            printf("Falha ao parar forno.\n");
        }
    }
}

void desliga_sistema(int sig){      // caso em que aperta ctrl+c
    desliga_para_forno(1);
    desliga_para_forno(2);

    softPwmWrite(GPIO_resistor, 0);
    delay(0.7);
    softPwmWrite(GPIO_vetoinha, 0);
    delay(0.7);

    //ClrLcd();
    printf("Sistema desligado!\n");
    exit(0);
}

int8_t user_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t len, void *intf_ptr)
{
    struct identifier id;

    id = *((struct identifier *)intf_ptr);

    write(id.fd, &reg_addr, 1);
    read(id.fd, data, len);

    return 0;
}

void user_delay_us(uint32_t period, void *intf_ptr)
{
    usleep(period);
}

int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr)
{
    uint8_t *buf;
    struct identifier id;

    id = *((struct identifier *)intf_ptr);

    buf = malloc(len + 1);
    buf[0] = reg_addr;
    memcpy(buf + 1, data, len);
    if (write(id.fd, buf, len + 1) < (uint16_t)len)
    {
        return BME280_E_COMM_FAIL;
    }

    free(buf);

    return BME280_OK;
}

float print_sensor_data(struct bme280_data *comp_data)
{
    float temp, press, hum;

#ifdef BME280_FLOAT_ENABLE
    temp = comp_data->temperature;
    press = 0.01 * comp_data->pressure;
    hum = comp_data->humidity;
#else
#ifdef BME280_64BIT_ENABLE
    temp = 0.01f * comp_data->temperature;
    press = 0.0001f * comp_data->pressure;
    hum = 1.0f / 1024.0f * comp_data->humidity;
#else
    temp = 0.01f * comp_data->temperature;
    press = 0.01f * comp_data->pressure;
    hum = 1.0f / 1024.0f * comp_data->humidity;
#endif
#endif
    //printf("%0.2lf deg C, %0.2lf hPa, %0.2lf%%\n", temp, press, hum);
    //teste
    //printf("Aqui\n");
    return temp;
}

float stream_sensor_data_forced_mode(struct bme280_dev *dev)
{

    float resultado;

    /* Variable to define the result */
    int8_t rslt = BME280_OK;

    /* Variable to define the selecting sensors */
    uint8_t settings_sel = 0;

    /* Variable to store minimum wait time between consecutive measurement in force mode */
    uint32_t req_delay;

    /* Structure to get the pressure, temperature and humidity values */
    struct bme280_data comp_data;

    /* Recommended mode of operation: Indoor navigation */
    dev->settings.osr_h = BME280_OVERSAMPLING_1X;
    dev->settings.osr_p = BME280_OVERSAMPLING_16X;
    dev->settings.osr_t = BME280_OVERSAMPLING_2X;
    dev->settings.filter = BME280_FILTER_COEFF_16;

    settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

    /* Set the sensor settings */
    rslt = bme280_set_sensor_settings(settings_sel, dev);
    if (rslt != BME280_OK)
    {
        fprintf(stderr, "Failed to set sensor settings (code %+d).", rslt);

        return rslt;
    }

    req_delay = bme280_cal_meas_delay(&dev->settings);

    rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
    if (rslt != BME280_OK)
    {
        fprintf(stderr, "Failed to set sensor mode (code %+d).", rslt);
    }

    dev->delay_us(req_delay, dev->intf_ptr);
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
    if (rslt != BME280_OK)
    {
        fprintf(stderr, "Failed to get sensor data (code %+d).", rslt);
    }

    resultado = print_sensor_data(&comp_data);

    return resultado;

}