#include <iostream>
#include <unistd.h>
#include "json.hpp"
#include "serial.h"

using json = nlohmann::json;

//1sec(=1000ms) read/write timeout
#define RW_TIMEOUT 1000

enum
{
	SEND_JSON = 1,
	SEND_CRC  = 2,
	GET_JSON  = 3,
	GET_CRC   = 4
};

enum
{
	JSON_HEADER = 11,
	JSON_ENDING = 12
};

enum
{
	CRC_1 = 21,
	CRC_2 = 22
};

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

uint16_t crc16_checkvalue;
uint8_t crc16_Tx_bytes[2];
uint8_t crc16_Rx_bytes[2];
bool data_send_status = true;
bool new_message_sent = false;
bool new_message_received = false;
std::string received_json_string;
std::string final_json_string;
json json_obj;

class Data_Transfer{
private:
    /*
    Suppose we are controlling a motor.
    Type = 0 ->write operation
    Subsystem = 1 ->motor subystem
    ID = 2 ->to control motor2
    Value = 60 ->motor speed value
    */
/*
    json sample_msg = {
        {"Type", 0},
        {"Subsystem", 1},
        {"ID", 2},
        {"Value", 60}
    };
*/
    json sample_msg = {
        {"Ty", 0},
        {"ID", 2}
    };

public:
    std::string data_str = sample_msg.dump();//serializing JSON object to be sent to MCU using UART protocol
    void send_message(void);
    void parse_message(void);
    void record_data(char incoming_data);
};

class CRC16{
private:

public:
    //CRC16 checkvalue generation
    uint16_t crc16_ccitt(const std::string data_str, uint8_t len);
    bool validate_message(void);
};

//opening serial port
serial::Serial my_serial("/dev/ttyACM0", 2400, serial::Timeout::simpleTimeout(RW_TIMEOUT), serial::eightbits, serial::parity_none, serial::stopbits_one, serial::flowcontrol_none);

int main()
{
    CRC16 crc;
    Data_Transfer dataTrans;

    dataTrans.data_str += '\0';//to add NULL character at the end of JSON string
    //std::cout << dataTrans.data_str << std::endl;
    crc16_checkvalue = crc.crc16_ccitt(dataTrans.data_str, dataTrans.data_str.length());
    crc16_Tx_bytes[0] = ((crc16_checkvalue) & (0xFF));//lower byte
    crc16_Tx_bytes[1] = ((crc16_checkvalue >> 8) & (0xFF));//higher byte

    //while(1)
    {
        if(data_send_status == true)
        {
            std::cout << "Transmitting" << std::endl;
            dataTrans.send_message();
            if(new_message_sent == true)
            {
                //--do something
                new_message_sent = false;
                data_send_status = false;//to receive data next
            }
        }
        else if(data_send_status == false)
        {
            //--do the receiving part
            dataTrans.parse_message();
            if(new_message_received == true)
            {
                if(crc.validate_message() == true)
                {
                    //validation successful
                    std::cout << "Validation success!" << std::endl;
                    final_json_string = received_json_string;
                    json_obj = json::parse(final_json_string);
                    std::cout << json_obj << std::endl;

                    //clear the string for next round of data to be received
                    received_json_string.clear();
                }
                else
                {
                    //validation failed
                    std::cout << "Validation failed!" << std::endl;
                    //FOR NOW --- TO BE CHANGED LATER
                    final_json_string = received_json_string;
                    json_obj = json::parse(final_json_string);
                    std::cout << json_obj << std::endl;

                    //clear the string for next round of data to be received
                    received_json_string.clear();
                }
                new_message_received = false;
                data_send_status = true;
            }
        }
    }

    return 0;
}

void Data_Transfer::send_message(void)
{
	static uint8_t msg_send_state = SEND_JSON;
	static uint8_t json_msg_state = JSON_HEADER;
	static uint8_t crc_state = CRC_1;
    static uint8_t json_str_index = 0;

    switch(msg_send_state)
    {
        case SEND_JSON:
        {
            uint8_t outgoing_data;
            switch(json_msg_state)
            {
                case JSON_HEADER:
                {
                    if((outgoing_data = data_str.at(json_str_index)) == '{')
                    {
                        my_serial.write(&outgoing_data, 1);
                        json_str_index++;
                        json_msg_state = JSON_ENDING;
                    }
                }
                break;
                case JSON_ENDING:
                {
                    outgoing_data = data_str.at(json_str_index);
                    if(outgoing_data == '\0')
                    {
                        my_serial.write(&outgoing_data, 1);
                        json_str_index = 0;
                        json_msg_state = JSON_HEADER;
                        msg_send_state = SEND_CRC;
                    }
                    else
                    {
                        my_serial.write(&outgoing_data, 1);
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
                    my_serial.write(&crc16_Tx_bytes[0], 1);
                    crc_state = CRC_2;
                }
                break;
                case CRC_2:
                {
                    my_serial.write(&crc16_Tx_bytes[1], 1);
                    crc_state = CRC_1;

                    new_message_sent = true;

                    msg_send_state = SEND_JSON;//Ready for new message to send
                }
                break;
            }
        }
        break;
    }
}

void Data_Transfer::record_data(char incoming_data)
{
    received_json_string += incoming_data;
}

void Data_Transfer::parse_message(void)
{
	static uint8_t msg_parse_state = GET_JSON;
	static uint8_t json_msg_state = JSON_HEADER;
	static uint8_t crc_state = CRC_1;

	if(my_serial.available() > 0)
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
                        my_serial.read(&incoming_data, 1);
						if(incoming_data == '{')
						{
							record_data(incoming_data);
							json_msg_state = JSON_ENDING;
						}
					}
					break;
					case JSON_ENDING:
					{
                        my_serial.read(&incoming_data, 1);
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
						my_serial.read(&crc16_Rx_bytes[0], 1);
						crc_state = CRC_2;
					}
					break;
					case CRC_2:
					{
						my_serial.read(&crc16_Rx_bytes[1], 1);
						crc_state = CRC_1;

						new_message_received = true;
						//Ready for new message
						msg_parse_state = GET_JSON;
						return;
					}
					break;
				}
			}
			break;
		}
	}
}

uint16_t CRC16::crc16_ccitt(const std::string data_str, uint8_t len)
{
	uint16_t crc;
	uint8_t index = 0;
	crc = 0xFFFF ^ 0xFFFF;
	while (len > 0)
	{
		crc = table[data_str.at(index) ^ (uint8_t)crc] ^ (crc >> 8);
		index++;
		len--;
	}
	crc = crc ^ 0xFFFF;
	return crc;
}

bool CRC16::validate_message(void)
{
    uint16_t received_crc = ((crc16_Rx_bytes[1] << 8) | (crc16_Rx_bytes[0]));
	uint16_t calculated_crc = crc16_ccitt(received_json_string, received_json_string.length());
	if(received_crc == calculated_crc)
	{
		return true;
	}
	else
	{
		return false;
	}
}
