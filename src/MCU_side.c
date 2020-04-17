#include "stdbool.h"
#include "string.h"
#include "stdint.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/interrupt.h"
#include "jsmn.h"


enum
{
    GET_JSON = 1,
    GET_CRC  = 2,
    SEND_JSON = 3,
    SEND_CRC = 4
};

enum
{
    JSON_HEADER = 11,
    JSON_ENDING = 12,
};

enum
{
    CRC_1 = 21,
    CRC_2 = 22
};

//Buffer Size
#define BUFFER_SIZE 50
//JSON String Size
#define JSON_STRING_SIZE 150
//jsmn_tok array size
#define TOKEN_ARRAY_SIZE 50
//Circular buffer EMPTY condition
#define BUFF_EMPTY 0
//Circular buffer FULL condition
#define BUFF_FULL 1
//Circular buffer NOT FULL condition
#define BUFF_NOT_FULL 2

//Rx Circular Buffer
uint8_t Rx_buffer[BUFFER_SIZE];
//Tx Circular Buffer
uint8_t Tx_buffer[BUFFER_SIZE];

struct Buffer
{
    uint8_t front;
    uint8_t rear;
    uint8_t *arr;
};
typedef struct Buffer BUFFER;
BUFFER buffRx;
BUFFER buffTx;

uint8_t length_of_json = 0;
//Received JSON string
char received_json_str[JSON_STRING_SIZE];
//JSON string
char final_json_str[JSON_STRING_SIZE];
//Array of JSON tokens
jsmntok_t tokens[TOKEN_ARRAY_SIZE];
//2 bytes of CRC16 -- Received
uint8_t Rx_crc16_bytes[2];
//2 bytes of CRC16 -- Transmitted
uint8_t Tx_crc16_bytes[2];
//new message fully received status
bool new_message_received = false;
//new message fully sent status
bool new_message_sent = false;
//sending or receiving status
bool receiving_status = true;

static const uint16_t table[256] =
{
 0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
 0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
 0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
 0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
 0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
 0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
 0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};


void buffer_init(void);
void buffer_add(BUFFER *buff, char);
char buffer_get(BUFFER *buff);
uint8_t buffer_space(BUFFER *buff);

void UART_init(void);
void UART_config(void);
void UART0_Handler(void);

void parse_message(void);
bool validate_message(void);
void record_data(uint8_t incoming_data);
void send_message(void);
void send_data(uint8_t outgoing_data);

uint16_t crc16_ccitt(const char *data, uint8_t len);


int main(void)
{
    //initialize Rx_buffer and Tx_buffer
    buffer_init();

    //initialization and configuration of required modules and peripherals
    UART_init();
    UART_config();

    //Enable interrupts to the processor
    IntMasterEnable();

    while(1)
    {
        if(receiving_status == true)
        {
            parse_message();
            if(new_message_received == true)
            {
                if(validate_message() == true)
                {
                    /*validation successful*/
                    //Copy received string to final JSON string -> memcpy()
                    memcpy(final_json_str, received_json_str, length_of_json);

                    //Parsing JSON string
                    jsmn_parser parser;
                    jsmn_init(&parser);
                    jsmn_parse(&parser, final_json_str, strlen(final_json_str), tokens, TOKEN_ARRAY_SIZE);

                }
                else
                {
                    /*validation failed*/

                }
                new_message_received = false;
                receiving_status = false;//FOR NOW --- TO BE CHANGED LATER
                length_of_json = 0;//for next json string to be received

                uint16_t Tx_crc16 = crc16_ccitt(final_json_str, strlen(final_json_str) + 1);
                Tx_crc16_bytes[0] = (Tx_crc16 & 0xFF);
                Tx_crc16_bytes[1] = ((Tx_crc16 >> 8) & 0xFF);
            }
        }
        else if(receiving_status == false)
        {
            //--do transmitting
            send_message();
            if(new_message_sent == true)
            {
                /*Message sent*/
                new_message_sent = false;
                //--do something
                receiving_status = true;
            }
        }
    }
}


void buffer_init()
{
    buffRx.front = 0;
    buffRx.rear = 0;
    buffRx.arr = Rx_buffer;

    buffTx.front = 0;
    buffTx.rear = 0;
    buffTx.arr = Tx_buffer;
}

void buffer_add(BUFFER *buff, char data)
{
    buff->rear = (buff->rear + 1) % BUFFER_SIZE;
    buff->arr[buff->rear] = data;
}

char buffer_get(BUFFER *buff)
{
    buff->front = (buff->front + 1) % BUFFER_SIZE;
    return buff->arr[buff->front];
}

uint8_t buffer_space(BUFFER *buff)
{
    //the array index where "front" points to is always empty. Therefore, effective buffer space = BUFFER_SIZE - 1
    if(buff->front == (buff->rear + 1) % BUFFER_SIZE)//buffer is full
        return BUFF_FULL;
    else if(buff->rear == buff->front)//buffer is empty
        return BUFF_EMPTY;
    else
        return BUFF_NOT_FULL;
}

void UART_init(void)
{
    //Enable UART module being used. UART0 enabled.
    SYSCTL_RCGCUART_R |= (1 << 0);

    //Enable GPIO for port A
    SYSCTL_RCGCGPIO_R |= (1 << 0);

    //Enable alternate functions for PA0 and PA1
    GPIO_PORTA_AFSEL_R |= 0x03;

    //Configure the PMCn fields in the GPIOPCTL register to assign the UART signals to the appropriate
    //pins
    GPIO_PORTA_PCTL_R |= 0x11;

    //Digital enabling of UART pins PA0 and PA1
    GPIO_PORTA_DEN_R |= 0x03;
}

void UART_config(void)
{
    /*
    HSE=0 bit in UARTCTL register. ClkDiv=16
    Finding baud rate divisor
    BRD = 16,000,000/ (16 * 2400) = 416.6666667
    IBRD = 416
    FBRD => integer(0.6666667 * 64 + 0.5) = 43
    */

    //Disable the UART by clearing the UARTEN bit in the UARTCTL register
    UART0_CTL_R &= ~(1 << 0);

    //The UART is clocked using the system clock divided by 16
    UART0_CTL_R &= ~(1 << 5);

    //The UARTIBRD register is the integer part of the baud-rate divisor value.
    UART0_IBRD_R = 416;

    //The UARTFBRD register is the fractional part of the baud-rate divisor value.
    UART0_FBRD_R = 43;

    //Write the desired serial parameters in UARTLCRH register (UART Line Control)
    UART0_LCRH_R = (0x03 << 5); //8bit data, no parity, 1 stop bit, FIFO buffer disabled

    //Set clock configuration for UART. Default System Clock is PIOSC which has 16MHz frequency. System Clock used here for UART clock source.
    UART0_CC_R = 0x05;

    //Receive interrupt and Transmit interrupt enabled
    UART0_IM_R |= (1 << 4)|(1 << 5);

    // Set the priority to 1
    IntPrioritySet(INT_UART0, 0);

    //Registers a function to be called when an interrupt occurs
    IntRegister(INT_UART0, UART0_Handler);

    //Enable the NVIC for the UART0
    IntEnable(INT_UART0);

    //Enable the UART by setting the UARTEN bit in the UARTCTL register. Also enabling UART Rx and Tx.
    UART0_CTL_R |= (1 << 0)|(1 << 8)|(1 << 9);
}

void UART0_Handler(void)
{
    //Receive Interrupt
    if( (((UART0_MIS_R) & (1 << 4)) == (1 << 4)) )
    {
        if( (((UART0_FR_R) & (1 << 4)) == 0) && (buffer_space(&buffRx) != BUFF_FULL) )//if(Rx_FIFO != EMPTY && Rx_circular_buffer != FULL)
        {
            uint8_t data = UART0_DR_R;
            buffer_add(&buffRx, data);
        }

        UART0_ICR_R |= (1 << 4);//clearing Receive Interrupt flag
        return;
    }
    else if( ((UART0_MIS_R) & (1 << 5)) == (1 << 5) )//Transmit Interrupt
    {
        if(buffer_space(&buffTx) == BUFF_EMPTY)//no data present in the circular buffer to add to the Tx FIFO
        {
            //disabling transmit interrupt
            UART0_IM_R &= ~(1 << 5);
            //clearing Transmit Interrupt Flag
            UART0_ICR_R |= (1 << 5);
            return;
        }
        else
        {
            if( (((UART0_FR_R) & (1 << 5)) != (1 << 5)) )//if(Tx_FIFO != FULL)
            {
                uint8_t data = buffer_get(&buffTx);//add data to Tx FIFO
                UART0_DR_R = data;
            }
            //clearing Transmit Interrupt Flag
            UART0_ICR_R |= (1 << 5);
            return;
        }
    }
}

void parse_message(void)
{
    static uint8_t msg_parse_state = GET_JSON;
    static uint8_t json_msg_state = JSON_HEADER;
    static uint8_t crc_state = CRC_1;

    if(buffer_space(&buffRx) != BUFF_EMPTY)
    {
        switch(msg_parse_state)
        {
            case GET_JSON:
            {
                uint8_t incoming_data;
                switch(json_msg_state)
                {
                    case JSON_HEADER:
                    {
                        if((incoming_data = buffer_get(&buffRx)) == '{')
                        {
                            record_data(incoming_data);
                            json_msg_state = JSON_ENDING;
                        }
                    }
                    break;
                    case JSON_ENDING:
                    {
                        incoming_data = buffer_get(&buffRx);
                        if(incoming_data == '\0')
                        {
                            record_data(incoming_data);
                            json_msg_state = JSON_HEADER;
                            msg_parse_state = GET_CRC;
                        }
                        else
                        {
                            record_data(incoming_data);
                        }
                    }
                    break;
                }
            }
            break;
            case GET_CRC:
            {
                switch(crc_state)
                {
                    case CRC_1:
                    {
                        Rx_crc16_bytes[0] = buffer_get(&buffRx);
                        crc_state = CRC_2;
                    }
                    break;
                    case CRC_2:
                    {
                        Rx_crc16_bytes[1] = buffer_get(&buffRx);
                        crc_state = CRC_1;

                        new_message_received = true;
                        msg_parse_state = GET_JSON;//Ready for new message
                    }
                    break;
                }
            }
            break;
        }
    }
}

void send_message(void)
{
    static uint8_t msg_send_state = SEND_JSON;
    static uint8_t json_msg_state = JSON_HEADER;
    static uint8_t crc_state = CRC_1;
    static uint8_t json_str_index = 0;

    if(buffer_space(&buffTx) != BUFF_FULL)
    {
        switch(msg_send_state)
        {
            case SEND_JSON:
            {
                uint8_t outgoing_data;
                switch(json_msg_state)
                {
                    case JSON_HEADER:
                    {
                        outgoing_data = final_json_str[json_str_index];
                        if(outgoing_data == '{')
                        {
                            send_data(outgoing_data);
                            json_str_index++;
                            json_msg_state = JSON_ENDING;
                        }
                    }
                    break;
                    case JSON_ENDING:
                    {
                        outgoing_data = final_json_str[json_str_index];
                        if(outgoing_data == '\0')
                        {
                            send_data(outgoing_data);
                            json_msg_state = JSON_HEADER;
                            msg_send_state = SEND_CRC;
                            json_str_index = 0;
                        }
                        else
                        {
                            send_data(outgoing_data);
                            json_str_index++;
                        }
                    }
                    break;
                }
            }
            break;
            case SEND_CRC:
            {
                switch(crc_state)
                {
                    case CRC_1:
                    {
                        send_data(Tx_crc16_bytes[0]);
                        crc_state = CRC_2;
                    }
                    break;
                    case CRC_2:
                    {
                        send_data(Tx_crc16_bytes[1]);
                        crc_state = CRC_1;
                        new_message_sent = true;
                        //Ready for new message to send
                        msg_send_state = SEND_JSON;
                    }
                    break;
                }
            }
            break;
        }
    }
}

bool validate_message(void)
{
    uint16_t received_crc = ((Rx_crc16_bytes[1] << 8) | (Rx_crc16_bytes[0]));
    uint16_t calculated_crc = crc16_ccitt(received_json_str, strlen(received_json_str) + 1);
    if(received_crc == calculated_crc)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void record_data(uint8_t incoming_data)
{
    received_json_str[length_of_json] = incoming_data;
    length_of_json++;
}

void send_data(uint8_t outgoing_data)
{
    if(buffer_space(&buffTx) == BUFF_EMPTY)
    {
        if(((UART0_FR_R) & (1 << 5)) != (1 << 5))//if(Tx_FIFO != FULL)
        {
            UART0_DR_R = outgoing_data;
            UART0_IM_R |= (1 << 5);//enable Tx interrupt
        }
        else
        {
            UART0_IM_R &= ~(1 << 5);//disable Tx interrupt
            buffer_add(&buffTx, outgoing_data);
            UART0_IM_R |= (1 << 5);//enable Tx interrupt
        }
    }
    else
    {
        UART0_IM_R &= ~(1 << 5);//disable Tx interrupt
        buffer_add(&buffTx, outgoing_data);
        UART0_IM_R |= (1 << 5);//enable Tx interrupt
    }
}

uint16_t crc16_ccitt(const char *data, uint8_t len)
{
    uint16_t crc;
    crc = 0xFFFF ^ 0xFFFF;
    while (len > 0)
    {
        crc = table[*data ^ (uint8_t)crc] ^ (crc >> 8);
        data++;
        len--;
    }
    crc = crc ^ 0xFFFF;

    return crc;
}

