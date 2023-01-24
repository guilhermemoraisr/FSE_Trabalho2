#include "../inc/crc16.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static int uart0_filestream = -1;

void uart_init(){
    uart0_filestream = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY); //Open in non blocking read/write mode

    if (uart0_filestream == -1){
        printf("Erro - Não foi possível iniciar a UART.\n");
    }
    else{
        printf("UART iniciada com sucesso.\n");
    }

    struct termios options;
    tcgetattr(uart0_filestream, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;// <Set baud rate
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart0_filestream, TCIFLUSH);
    tcsetattr(uart0_filestream, TCSANOW, &options);
}

float solicita_temperatura(char protocolo[]){
    unsigned char tx_buffer[20];
    unsigned char *p_tx_buffer;

    p_tx_buffer = &tx_buffer[0];
    *p_tx_buffer++ = protocolo[0];
    *p_tx_buffer++ = protocolo[1];
    *p_tx_buffer++ = protocolo[2];
    *p_tx_buffer++ = protocolo[3];
    *p_tx_buffer++ = protocolo[4];
    *p_tx_buffer++ = protocolo[5];
    *p_tx_buffer++ = protocolo[6];


    short crc = calcula_CRC(&tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
    *p_tx_buffer++ = crc & 0xFF;
    *p_tx_buffer++ = (crc >> 8) & 0xFF;

    if (uart0_filestream != -1){
        int escreve_uart = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
        if (escreve_uart < 0){
            printf("UART TX error\n");
            return 0;
        }
    }

    usleep(700000);

    if (uart0_filestream != -1){
        unsigned char rx_buffer[9];
        int rx_length = read(uart0_filestream, rx_buffer, 9);
        //printf("Lendo!\n");

        short crc = calcula_CRC(rx_buffer, 7);
        if((crc & 0xFF) != rx_buffer[7] && ((crc >> 8) & 0xFF) != rx_buffer[8]){
            printf("Erro de CRC\n");
            solicita_temperatura(protocolo);
        }

        if (rx_length < 0){
            return 0;
        }
        else if (rx_length == 0){
            return 0;
        }
        else{
            unsigned char temp[4] = {rx_buffer[3], rx_buffer[4], rx_buffer[5], rx_buffer[6]};
            float temperatura = *((float *)&temp);
            return temperatura;
        }
    }
}

int solicita_estado_sistema(char protocolo[]){
    unsigned char tx_buffer[20];
    unsigned char *p_tx_buffer;

    p_tx_buffer = &tx_buffer[0];
    *p_tx_buffer++ = protocolo[0];
    *p_tx_buffer++ = protocolo[1];
    *p_tx_buffer++ = protocolo[2];
    *p_tx_buffer++ = protocolo[3];
    *p_tx_buffer++ = protocolo[4];
    *p_tx_buffer++ = protocolo[5];
    *p_tx_buffer++ = protocolo[6];
    *p_tx_buffer++ = protocolo[7];

    short crc = calcula_CRC(&tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
    *p_tx_buffer++ = crc & 0xFF;
    *p_tx_buffer++ = (crc >> 8) & 0xFF;

    if (uart0_filestream != -1){
        int escreve_uart = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
        if (escreve_uart < 0){
            printf("UART TX error\n");
            return 0;
        }
    }

    usleep(700000);

    if (uart0_filestream != -1){
        unsigned char rx_buffer[10];
        int rx_length = read(uart0_filestream, rx_buffer, 10);

        short crc = calcula_CRC(rx_buffer, 7);
        if((crc & 0xFF) != rx_buffer[7] && ((crc >> 8) & 0xFF) != rx_buffer[8]){
            printf("Erro de CRC\n");
            solicita_estado_sistema(protocolo);
        }

        if (rx_length < 0){
            return 0;
        }
        else if (rx_length == 0){
            return 0;
        }
        else{
            unsigned char estado = rx_buffer[3] + (rx_buffer[4] << 8) + (rx_buffer[5] << 16) + (rx_buffer[6] << 24);
            if (estado == protocolo[7]){
                return 1;
            }
        }
    }
}

int solicita_comando_usuario(char protocolo[]){
    unsigned char tx_buffer[20];
    unsigned char *p_tx_buffer;

    p_tx_buffer = &tx_buffer[0];
    *p_tx_buffer++ = protocolo[0];
    *p_tx_buffer++ = protocolo[1];
    *p_tx_buffer++ = protocolo[2];
    *p_tx_buffer++ = protocolo[3];
    *p_tx_buffer++ = protocolo[4];
    *p_tx_buffer++ = protocolo[5];
    *p_tx_buffer++ = protocolo[6];

    short crc = calcula_CRC(&tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
    *p_tx_buffer++ = crc & 0xFF;
    *p_tx_buffer++ = (crc >> 8) & 0xFF;

    if (uart0_filestream != -1){
        int escreve_uart = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
        if (escreve_uart < 0){
            printf("UART TX error\n");
            return 0;
        }
    }

    usleep(700000);

    if (uart0_filestream != -1){
        unsigned char rx_buffer[10];
        int rx_length = read(uart0_filestream, rx_buffer, 10);

        short crc = calcula_CRC(rx_buffer, 7);
        if((crc & 0xFF) != rx_buffer[7] && ((crc >> 8) & 0xFF) != rx_buffer[8]){
            printf("Erro de CRC\n");
            solicita_comando_usuario(protocolo);
        }

        if (rx_length < 0){
            return 0;
        }
        else if (rx_length == 0){
            return 0;
        }
        else{
            int comando = rx_buffer[3] + (rx_buffer[4] << 8) + (rx_buffer[5] << 16) + (rx_buffer[6] << 24);
            return comando;
        }
    }
}

int envia_sinal_controle(char protocolo[], int s_c){
    unsigned char tx_buffer[50];
    unsigned char *p_tx_buffer;

    p_tx_buffer = &tx_buffer[0];
    *p_tx_buffer++ = protocolo[0];
    *p_tx_buffer++ = protocolo[1];
    *p_tx_buffer++ = protocolo[2];
    *p_tx_buffer++ = protocolo[3];
    *p_tx_buffer++ = protocolo[4];
    *p_tx_buffer++ = protocolo[5];
    *p_tx_buffer++ = protocolo[6];
    
    *p_tx_buffer++ = s_c & 0xFF;
    *p_tx_buffer++ = (s_c >> 8) & 0xFF;
    *p_tx_buffer++ = (s_c >> 16) & 0xFF;
    *p_tx_buffer++ = (s_c >> 24) & 0xFF;

    short crc = calcula_CRC(&tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
    *p_tx_buffer++ = crc & 0xFF;
    *p_tx_buffer++ = (crc >> 8) & 0xFF;

    if (uart0_filestream != -1){
        int escreve_uart = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
        if (escreve_uart < 0){
            printf("UART TX error\n");
            return 0;
        }
        else{
            return 1;
        }
    }

    usleep(700000);
}

// int envia_sinal_float(char protocolo[], float s_c){
//     unsigned char tx_buffer[50];
//     unsigned char *p_tx_buffer;

//     p_tx_buffer = &tx_buffer[0];
//     *p_tx_buffer++ = protocolo[0];
//     *p_tx_buffer++ = protocolo[1];
//     *p_tx_buffer++ = protocolo[2];
//     *p_tx_buffer++ = protocolo[3];
//     *p_tx_buffer++ = protocolo[4];
//     *p_tx_buffer++ = protocolo[5];
//     *p_tx_buffer++ = protocolo[6];
    
//     *p_tx_buffer++ = s_c & 0xFF;
//     *p_tx_buffer++ = (s_c >> 8) & 0xFF;
//     *p_tx_buffer++ = (s_c >> 16) & 0xFF;
//     *p_tx_buffer++ = (s_c >> 24) & 0xFF;

//     //*p_tx_buffer++ = s_c;

//     short crc = calcula_CRC(&tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
//     *p_tx_buffer++ = crc & 0xFF;
//     *p_tx_buffer++ = (crc >> 8) & 0xFF;

//     if (uart0_filestream != -1){
//         int escreve_uart = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));
//         if (escreve_uart < 0){
//             printf("UART TX error\n");
//             return 0;
//         }
//         else{
//             return 1;
//         }
//     }

//     usleep(700000);
// }


void uart_close(){
    close(uart0_filestream);
}
