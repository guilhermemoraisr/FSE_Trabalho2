#ifndef _UART_
#define _UART

void uart_init();
float solicita_temperatura(char protocolo[]);
int solicita_estado_sistema(char protocolo[]);
int solicita_comando_usuario(char protocolo[]);
int envia_sinal_controle(char protocolo[], int s_c);
//int envia_sinal_float(char protocolo[], float s_c);
void uart_close();

#endif
