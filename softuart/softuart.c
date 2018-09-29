//
// Modifications from original softuart source code are copyright and licensed as follows:
//

/*
 * OTB-IOT - Out of The Box Internet Of Things
 *
 * Copyright (C) 2018 Piers Finlayson
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version. 
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if 0
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#endif
#include "otb.h"
#include "softuart.h"

//array of pointers to instances
Softuart *_Softuart_GPIO_Instances[SOFTUART_GPIO_COUNT];
uint8_t _Softuart_Instances_Count = 0;

//intialize list of gpio names and functions
softuart_reg_t softuart_reg[] =
{
	{ PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0 },	//gpio0
	{ PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO1 },	//gpio1 (uart)
	{ PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 },	//gpio2
	{ PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3 },	//gpio3 (uart)
	{ PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4 },	//gpio4
	{ PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5 },	//gpio5
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12 },	//gpio12
	{ PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13 },	//gpio13
	{ PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14 },	//gpio14
	{ PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15 },	//gpio15
	//@TODO TODO gpio16 is missing (?include)
};

uint8_t ICACHE_FLASH_ATTR Softuart_Bitcount(uint32_t x)
{
	uint8_t count;
 
	for (count=0; x != 0; x>>=1) {
		if ( x & 0x01) {
			return count;
		}
		count++;
	}
	//error: no 1 found!
	return 0xFF;
}

uint8_t ICACHE_FLASH_ATTR Softuart_IsGpioValid(uint8_t gpio_id)
{
	if ((gpio_id > 5 && gpio_id < 12) || gpio_id > 15)
	{
		return 0;
	}
	return 1;
}

void ICACHE_FLASH_ATTR Softuart_SetPinRx(Softuart *s, uint8_t gpio_id)
{ 
	if(! Softuart_IsGpioValid(gpio_id)) {
		os_printf("SOFTUART GPIO not valid %d\r\n",gpio_id);
	} else {
		s->pin_rx.gpio_id = gpio_id;
		s->pin_rx.gpio_mux_name = softuart_reg[gpio_id].gpio_mux_name;
		s->pin_rx.gpio_func = softuart_reg[gpio_id].gpio_func;
	}
}

void ICACHE_FLASH_ATTR Softuart_SetPinTx(Softuart *s, uint8_t gpio_id)
{ 
	if(! Softuart_IsGpioValid(gpio_id)) {
		os_printf("SOFTUART GPIO not valid %d\r\n",gpio_id);
	} else {
		s->pin_tx.gpio_id = gpio_id;
		s->pin_tx.gpio_mux_name = softuart_reg[gpio_id].gpio_mux_name;
		s->pin_tx.gpio_func = softuart_reg[gpio_id].gpio_func;
	}
}

void ICACHE_FLASH_ATTR Softuart_EnableRs485(Softuart *s, uint8_t gpio_id)
{
	os_printf("SOFTUART RS485 init\r\n");

	//enable rs485
	s->is_rs485 = 1;

	//set pin in instance
	s->pin_rs485_tx_enable = gpio_id;

	//enable pin as gpio
	PIN_FUNC_SELECT(softuart_reg[gpio_id].gpio_mux_name,softuart_reg[gpio_id].gpio_func); 

	PIN_PULLUP_DIS(softuart_reg[gpio_id].gpio_mux_name);
	
	//set low for tx idle (so other bus participants can send)
	GPIO_OUTPUT_SET(GPIO_ID_PIN(gpio_id), 0);
	
	os_printf("SOFTUART RS485 init done\r\n");
}

void ICACHE_FLASH_ATTR Softuart_Init(Softuart *s, uint32_t baudrate)
{
	//disable rs485
	s->is_rs485 = 0;

	if(! _Softuart_Instances_Count) {
		os_printf("SOFTUART initialize gpio\r\n");
		//Initilaize gpio subsystem
		gpio_init();
	}

	//set bit time
	if(baudrate <= 0) {
                os_printf("SOFTUART ERROR: Set baud rate (%d)\r\n",baudrate);
        } else {
            s->bit_time = (1000000 / baudrate);
            if ( ((100000000 / baudrate) - (100*s->bit_time)) > 50 ) s->bit_time++;
            os_printf("SOFTUART bit_time is %d\r\n",s->bit_time);
        }


	//init tx pin
	if(!s->pin_tx.gpio_mux_name) {
		os_printf("SOFTUART ERROR: Set tx pin (%d)\r\n",s->pin_tx.gpio_mux_name);
	} else {
		//enable pin as gpio
    	PIN_FUNC_SELECT(s->pin_tx.gpio_mux_name, s->pin_tx.gpio_func);

		//set pullup (UART idle is VDD)
		PIN_PULLUP_EN(s->pin_tx.gpio_mux_name);
		
		//set high for tx idle
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), 1);
		os_delay_us(65535);
		
		os_printf("SOFTUART TX INIT DONE\r\n");
	}

	//init rx pin
	if(!s->pin_rx.gpio_mux_name) {
		os_printf("SOFTUART ERROR: Set rx pin (%d)\r\n",s->pin_rx.gpio_mux_name);
	} else {
		//enable pin as gpio
    	PIN_FUNC_SELECT(s->pin_rx.gpio_mux_name, s->pin_rx.gpio_func);

		//set pullup (UART idle is VDD)
		PIN_PULLUP_EN(s->pin_rx.gpio_mux_name);
		
		//set to input -> disable output
		GPIO_DIS_OUTPUT(GPIO_ID_PIN(s->pin_rx.gpio_id));

		//set interrupt related things

		otb_intr_register(Softuart_Intr_Handler, s, s->pin_rx.gpio_id);

		os_printf("SOFTUART RX INIT DONE\r\n");
	}

	//add instance to array of instances
	_Softuart_GPIO_Instances[s->pin_rx.gpio_id] = s;
	_Softuart_Instances_Count++;
		
	os_printf("SOFTUART INIT DONE\r\n");
}

void ICACHE_FLASH_ATTR Softuart_Intr_Handler(void *arg)
{
	Softuart *s = (Softuart *)arg;
	uint8_t level, gpio_id;
// clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    //ets_printf("in interrupt handler\r\n");

    uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	gpio_id = Softuart_Bitcount(gpio_status);

	//if interrupt was by an attached rx pin
	if (gpio_id != 0xFF)
	{
		//ets_printf("Rx pin: %d\r\n", gpio_id);
		//load instance which has rx pin on interrupt pin attached
		s = _Softuart_GPIO_Instances[gpio_id];

// disable interrupt for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx.gpio_id), GPIO_PIN_INTR_DISABLE);

// Do something, for example, increment whatyouwant indirectly
		//check level
		level = GPIO_INPUT_GET(GPIO_ID_PIN(s->pin_rx.gpio_id));
		if(!level) {
			//ets_printf("pin is low\r\n");
			//pin is low
			//therefore we have a start bit

			//wait till start bit is half over so we can sample the next one in the center
			os_delay_us(s->bit_time/2);	

			//now sample bits
			unsigned i;
			unsigned d = 0;
			unsigned start_time = 0x7FFFFFFF & system_get_time();

			for(i = 0; i < 8; i ++ )
			{
				while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(i+1))))
				{
					//If system timer overflow, escape from while loop
					if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
				}
				//shift d to the right
				d >>= 1;

				//read bit
				if(GPIO_INPUT_GET(GPIO_ID_PIN(s->pin_rx.gpio_id))) {
					//if high, set msb of 8bit to 1
					d |= 0x80;
				}
			}

			//store byte in buffer

			// if buffer full, set the overflow flag and return
			uint8 next = (s->buffer.receive_buffer_tail + 1) % SOFTUART_MAX_RX_BUFF;
			if (next != s->buffer.receive_buffer_head)
			{
			  // save new data in buffer: tail points to where byte goes
			  s->buffer.receive_buffer[s->buffer.receive_buffer_tail] = d; // save new byte
			  s->buffer.receive_buffer_tail = next;
			} 
			else 
			{
			  s->buffer.buffer_overflow = 1;
			}

			//wait for stop bit
			os_delay_us(s->bit_time);	

			//done
		}

		//clear interrupt
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

		// Reactivate interrupts for GPIO
		gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx.gpio_id), 3);

	} else {
		//clear interrupt, no matter from which pin
		//otherwise, this interrupt will be called again forever
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
	}
}



// Read data from buffer
uint8_t ICACHE_FLASH_ATTR Softuart_Read(Softuart *s)
{
  // Empty buffer?
  if (s->buffer.receive_buffer_head == s->buffer.receive_buffer_tail)
    return 0;

  // Read from "head"
  uint8_t d = s->buffer.receive_buffer[s->buffer.receive_buffer_head]; // grab next byte
  s->buffer.receive_buffer_head = (s->buffer.receive_buffer_head + 1) % SOFTUART_MAX_RX_BUFF; 
  return d;
}

// Is data in buffer available?
BOOL ICACHE_FLASH_ATTR Softuart_Available(Softuart *s)
{
	BOOL rc;
	rc = (s->buffer.receive_buffer_tail + SOFTUART_MAX_RX_BUFF - s->buffer.receive_buffer_head) % SOFTUART_MAX_RX_BUFF;
	//ets_printf("Available returning: %d\r\n", rc);
	return rc;
}

static inline u8 chbit(u8 data, u8 bit)
{
    if ((data & bit) != 0)
    {
    	return 1;
    }
    else
    {
    	return 0;
    }
}

// Function for printing individual characters
void ICACHE_FLASH_ATTR Softuart_Putchar(Softuart *s, char data)
{
	unsigned i;
	unsigned start_time = 0x7FFFFFFF & system_get_time();
	uint8_t bit;
	uint8_t parity = 0;

	ETS_INTR_LOCK();

	//if rs485 set tx enable
	if(s->is_rs485 == 1)
	{
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_rs485_tx_enable), 1);
	}

	//Start Bit
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), 0);
	for(i = 0; i < 8; i ++ )
	{
		while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(i+1))))
		{
			//If system timer overflow, escape from while loop
			if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
		}
		bit = chbit(data,1<<i);
		parity += bit;
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), bit);
	}

	// Parity bit (bit 1 - even parity for mbus, which is 1 if odd)
	while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(9))))
	{
		//If system timer overflow, escape from while loop
		if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
	}
	DEBUG("Data: 0x%02x Parity: 0x%02x Parity bit: %d", data, parity, chbit(parity, 1));
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), chbit(parity,1));

	// Stop bit
	while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(10))))
	{
		//If system timer overflow, escape from while loop
		if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
	}
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), 1);

	// Delay after byte, for new sync
	os_delay_us(s->bit_time*6);

	//if rs485 set tx disable 
	if(s->is_rs485 == 1)
	{
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_rs485_tx_enable), 0);
	}

	ETS_INTR_UNLOCK();

}

void ICACHE_FLASH_ATTR Softuart_Puts(Softuart *s, const char *c )
{
	while ( *c ) {
      	Softuart_Putchar(s,( u8 )*c++);
	}
}

uint8_t ICACHE_FLASH_ATTR Softuart_Readline(Softuart *s, char* Buffer, uint8_t MaxLen )
{
	uint8_t NextChar;
	uint8_t len = 0;

	while( Softuart_Available(s) )
	{
		NextChar = Softuart_Read(s);

		if(NextChar == '\r')
		{
			continue;
		} else if(NextChar == '\n') 
		{
			//break only if we already found a character
			//if it was .e.g. only \r, we wait for the first useful character
			if(len > 0) {
				break;
			}
		} else if(len < MaxLen - 1 )
		{
			*Buffer++ = NextChar;
			len++;
		} else {
			break;
		}
	}
	//add string terminator
	*Buffer++ = '\0';

	return len;
}

