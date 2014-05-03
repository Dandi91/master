// Library for RFT MRF49XA

#include "mrf49xa.h"
#include "spi_low.h"

#include "crypto_functions.h"

static uint16_t register_state[11] = {
                0x8008, // GENCREG
                0xC4F7, // AFCCREG
                0x9800, // TXCREG
                0x9080, // RXCREG
                0xC22C, // BBFCREG
                0xCA80, // FIFORSTREG
                0xC623, // DRSREG
                0x8208, // PMCREG
                0xC80E, // DCSREG
                0xC000, // BCSREG
                0xCC77  // PLLCREG
};

uint8_t sync_byte = 0xD4;

#define CRC_BYTES_LENGTH 4		// 32 bit CRC - 4 bytes

uint8_t crypto_buffer_out[MAX_PACKET_LOAD + PROTO_BYTES_CNT + sizeof(data_len_t) + SRVC_BYTES_CNT];

typedef enum
{
  SPI_RFT_Proc_Header = 0,
  SPI_RFT_Proc_Data,
	SPI_RFT_Proc_CRC,
	SPI_RFT_Proc_Dummy
} SPI_RFT_Proc_State;

typedef enum
{
	SPI_RFT_Low = 0,
	SPI_RFT_High
} SPI_RFT_Priority;

SPI_RFT_cb_TypeDef SPI_RFT_cb;

uint8_t *rx_buffer, *tx_buffer;

SPI_RFT_Status  status; // RFT module status
SPI_RFT_State   state;  // SPI transaction state

SPI_RFT_Proc_State rx_state = SPI_RFT_Proc_Header, tx_state = SPI_RFT_Proc_Header;
uint8_t *p_out_buffer;
uint16_t rx_len = 0, tx_len = 0;
uint32_t rx_crc;

uint8_t SPI_Busy = 0;
uint8_t status_transmission_flag = 0;

// Forwarded
uint8_t SPI_RFT_SPI_Precall(uint16_t data_type);

//************************************************************************
// Functions for cycle buffer
//************************************************************************

RFT_Cycle_Buffer_Item_TypeDef cycle_buffer[BUFFER_LENGTH];
int16_t cycle_buffer_first = 0, cycle_buffer_last = -1, cycle_buffer_count = 0;

// Trying to send first item in buffer
void SPI_RFT_Try_Send(void)
{
	if ((cycle_buffer_count > 0) && SPI_RFT_SPI_Precall(cycle_buffer[cycle_buffer_first].type) && (!SPI_Busy))
	{
		SPI_Busy = 1;
		SPI_RFT_Raw_Write(cycle_buffer[cycle_buffer_first].data);
	}
}

// Add new item in buffer
int16_t SPI_RFT_Add_Data(RFT_Cycle_Buffer_Item_TypeDef data, SPI_RFT_Priority priority)
{
	int16_t result;
	NVIC_DisableIRQ(SPIx_IRQn);
	if (cycle_buffer_count == 0)
		priority = SPI_RFT_Low;
	if (cycle_buffer_count >= BUFFER_LENGTH)
    result = -1;
	else
	{
		if (priority == SPI_RFT_Low)		// Low priority
		{
			if (++cycle_buffer_last >= BUFFER_LENGTH)
				cycle_buffer_last = 0;
			cycle_buffer[cycle_buffer_last] = data;
			result = ++cycle_buffer_count;
		}
		else														// High priority
		{
			result = cycle_buffer_first;
			if (--cycle_buffer_first < 0)
				cycle_buffer_first = BUFFER_LENGTH - 1;
			if (state == SPI_RFT_TX)
				cycle_buffer[cycle_buffer_first] = data;
			else
			{
				cycle_buffer[cycle_buffer_first] = cycle_buffer[result];
				cycle_buffer[result] = data;
			}
			cycle_buffer_count++;
		}
	}
	if (cycle_buffer_count == 1)
		SPI_RFT_Try_Send();
	NVIC_EnableIRQ(SPIx_IRQn);
  return result;
}

RFT_Cycle_Buffer_Item_TypeDef RFT_Cycle_Buffer_Get_First_Item(void)
{
	RFT_Cycle_Buffer_Item_TypeDef result = {0x00,0xFF};
	NVIC_DisableIRQ(SPIx_IRQn);
	if (cycle_buffer_count > 0)
		result = cycle_buffer[cycle_buffer_first];
	NVIC_EnableIRQ(SPIx_IRQn);
	return result;
}

void SPI_RFT_Delete_Data(void)
{
	NVIC_DisableIRQ(SPIx_IRQn);
  if (cycle_buffer_count > 0)
	{
		cycle_buffer_count--;
		if (++cycle_buffer_first >= BUFFER_LENGTH)
			cycle_buffer_first = 0;
	}
	NVIC_EnableIRQ(SPIx_IRQn);
  return;
}

//************************************************************************
// Function for sending data
//************************************************************************

SPI_RFT_Retval SPI_RFT_Send_Data(uint16_t data, uint16_t type)
{
  int16_t ret;
	RFT_Cycle_Buffer_Item_TypeDef item = {0,0};
	item.data = data;
	item.type = type;
  ret = SPI_RFT_Add_Data(item,SPI_RFT_Low);
  if (ret == -1)  // Buffer is full. Command doesn't added
  {
    if (SPI_RFT_cb.BufferFilled != NULL)
      SPI_RFT_cb.BufferFilled();  // Doing callback to main programm
    return SPI_RFT_Error;
  }
  return SPI_RFT_OK;
}

//************************************************************************
// Main RFT code
//************************************************************************

// SPI Precall
uint8_t SPI_RFT_SPI_Precall(uint16_t data_type)
{
	if (data_type & RFT_ITEM_CMD)
	{	// Command may goes at free state or command state only
		if ((state == SPI_RFT_Free) || (state == SPI_RFT_Cmd))
		{
			state = SPI_RFT_Cmd;
			return 1;
		}
	}
	else if (data_type & RFT_ITEM_TXD)
	{	// Transmission goes after commands or at free state
		if ((state == SPI_RFT_Cmd) || (state == SPI_RFT_Free))
		{
			state = SPI_RFT_TX;
			return 1;
		}
		return state == SPI_RFT_TX;
	}
	else if (data_type & RFT_ITEM_RXD)
	{	// Like as for transmission
		if ((state == SPI_RFT_Cmd) || (state == SPI_RFT_Free))
		{
			state = SPI_RFT_RX;
			return 1;
		}
		return state == SPI_RFT_RX;
	}
	else if (data_type & RFT_ITEM_STS)
		return 1;	// Status request must be sended anyway
	return 0;
}

void SPI_RFT_Rearm_Receiver(void)
{
	state = SPI_RFT_Free;
	SPI_RFT_Restart_Synch_Pattern();
	rx_state = SPI_RFT_Proc_Header;
	rx_len = 0;
}

// SPI Callback
void SPI_RFT_SPI_Callback(uint16_t result)
{
	RFT_Cycle_Buffer_Item_TypeDef item;
	static uint8_t CRC_Data_Index;

	item = RFT_Cycle_Buffer_Get_First_Item();	// Item is the last sended command
	SPI_RFT_Delete_Data();
	SPI_Busy = 0;

	if (item.type & RFT_ITEM_CMD)
	{
		if (cycle_buffer_count < 1)
		{
			state = SPI_RFT_Free;
			if (SPI_RFT_cb.BufferEmpty != NULL)
				SPI_RFT_cb.BufferEmpty();
		}
	}

	else if (item.type & RFT_ITEM_STS)
	{ // State machine here
		status_transmission_flag = 1;
		if (result & TXRXFIFO)
		{
			if (status == SPI_RFT_TX_On)  // If transmit
			{
				if (cycle_buffer_count < 1)	// There is no items in buffer
				{
					switch (tx_state)
					{
						case SPI_RFT_Proc_Header:
						{
							if (tx_len == 0)
							{
								tx_buffer = crypto_buffer_out;
								CRC_ResetDR();
							}
							SPI_RFT_Send_Data(RFT_CW_TXBREG | *tx_buffer++,RFT_ITEM_TXD);
							tx_len++;
							if (tx_len > SRVC_BYTES_CNT - 1)
							{
								tx_state = SPI_RFT_Proc_Data;
								tx_len = *(data_len_t*)&crypto_buffer_out[SRVC_BYTES_CNT];
								CRC_Data_Index = 0;
							}
							break;
						}
						case SPI_RFT_Proc_Data:
						{
							SPI_RFT_Send_Data(RFT_CW_TXBREG | *tx_buffer++,RFT_ITEM_TXD);
							if (++CRC_Data_Index == CRC_BYTES_LENGTH)		// Get a dword
							{
								CRC_CalcCRC(*(uint32_t*)(&tx_buffer[-CRC_BYTES_LENGTH]));
								CRC_Data_Index = 0;
							}
							if (--tx_len == 0)	// Whole packet
							{
								if (CRC_Data_Index)			// This conctruction adds tailing zero-bytes to ending bytes of buffer
									CRC_CalcCRC((*(uint32_t*)(&tx_buffer[-CRC_Data_Index])) & (0xFFFFFFFF >> ((CRC_BYTES_LENGTH - CRC_Data_Index) * 8)));
								tx_state = SPI_RFT_Proc_CRC;
								CRC_Data_Index = 0;
							}
							break;
						}
						case SPI_RFT_Proc_CRC:
						{
							if (CRC_Data_Index == CRC_BYTES_LENGTH)
							{
								tx_state = SPI_RFT_Proc_Dummy;
								SPI_RFT_Send_Data(RFT_CW_TXBREG,RFT_ITEM_TXD);	// Send dummy byte
								break;
							}
							if (CRC_Data_Index++ == 0)
								tx_buffer = (uint8_t*)&CRC->DR;
							SPI_RFT_Send_Data(RFT_CW_TXBREG | *tx_buffer++,RFT_ITEM_TXD);
							break;
						}
						case SPI_RFT_Proc_Dummy:
						{
							state = SPI_RFT_Free;	// Tramsmission complete. Turn off transmitter and callback to main code
							SPI_RFT_Disable_Transmitter();
							if (SPI_RFT_cb.TxComplete != NULL)
								SPI_RFT_cb.TxComplete();
							tx_len = 0;
							tx_state = SPI_RFT_Proc_Header;
							break;
						}
					}
				}
				else
					SPI_RFT_Try_Send();
				return;
			}
			if (status == SPI_RFT_RX_On) // If receive
			{
				if (rx_state == SPI_RFT_Proc_Header) // if header
				{
					if (rx_len == 0) // First byte
					{
						CRC_ResetDR();								// Setup state and CRC32
						rx_buffer = crypto_buffer_out;
						state = SPI_RFT_RX;
					}
				}
				SPI_RFT_Send_Data(RFT_CW_RXFIFOREG,RFT_ITEM_RXD);	// Read from RX register
				return;
			}
		}
		if (SPI_RFT_cb.EventOccurred != NULL)
		{
			if (result & TXRXFIFO)
			{
				if (status == SPI_RFT_TX_On)
					SPI_RFT_cb.EventOccurred(SPI_RFT_Event_TX);
				if (status == SPI_RFT_RX_On)
					SPI_RFT_cb.EventOccurred(SPI_RFT_Event_RX);
			}
			if (result & POR)
				SPI_RFT_cb.EventOccurred(SPI_RFT_Event_PowerOnReset);
			if (result & TXOWRXOF)
			{
				if (status == SPI_RFT_TX_On)
					SPI_RFT_cb.EventOccurred(SPI_RFT_Event_Underrun);
				if (status == SPI_RFT_RX_On)
					SPI_RFT_cb.EventOccurred(SPI_RFT_Event_Overflow);
			}
			if (result & WUTINT)
				SPI_RFT_cb.EventOccurred(SPI_RFT_Event_WakeupTimer);
			if (result & LCEXINT)
				SPI_RFT_cb.EventOccurred(SPI_RFT_Event_EXTI);
			if (result & LBTD)
				SPI_RFT_cb.EventOccurred(SPI_RFT_Event_LowVoltage);
		}
	}

	else if (item.type & RFT_ITEM_RXD)
	{
		switch (rx_state)
		{
			case SPI_RFT_Proc_Header:
			{
				*rx_buffer++ = result & 0xFF;
				rx_len++;
				if (rx_len == sizeof(data_len_t))
				{
					// Data length received
					rx_len = *(data_len_t*)crypto_buffer_out;
					if (rx_len > (MAX_PACKET_LOAD + PROTO_BYTES_CNT + sizeof(data_len_t)))
					{
						SPI_RFT_Rearm_Receiver();
						break;
					}
					rx_state = SPI_RFT_Proc_Data;
					if (SPI_RFT_cb.RxBegin != NULL)
						p_out_buffer = SPI_RFT_cb.RxBegin(rx_len - sizeof(data_len_t)); // Callback
					if (p_out_buffer == NULL)
					{
						SPI_RFT_Rearm_Receiver();
						break;
					}
					rx_len = rx_len - sizeof(data_len_t);
					CRC_Data_Index = sizeof(data_len_t);
				}
				break;
			}
			case SPI_RFT_Proc_Data:  // if packet data
			{
				*rx_buffer++ = result & 0xFF;
				if (++CRC_Data_Index == CRC_BYTES_LENGTH)		// Get a dword
				{
					CRC_CalcCRC(*(uint32_t*)(&rx_buffer[-CRC_BYTES_LENGTH]));
					CRC_Data_Index = 0;
				}
				if (--rx_len == 0)  // Whole packet
				{
					if (CRC_Data_Index)			// This conctruction adds tailing zero-bytes to ending bytes of buffer
						CRC_CalcCRC((*(uint32_t*)(&rx_buffer[-CRC_Data_Index])) & (0xFFFFFFFF >> ((CRC_BYTES_LENGTH - CRC_Data_Index) * 8)));
					rx_state = SPI_RFT_Proc_CRC;
					CRC_Data_Index = 0;
				}
				break;
			}
			case SPI_RFT_Proc_CRC:
			{
				if (CRC_Data_Index++ == 0)
					rx_buffer = (uint8_t*)&rx_crc;
				*rx_buffer++ = result & 0xFF;
				if (CRC_Data_Index == CRC_BYTES_LENGTH)	// Whole CRC received
				{
					SPI_RFT_Rearm_Receiver();
					if (rx_crc != CRC_GetCRC())
					{
						if (SPI_RFT_cb.EventOccurred != NULL)
							SPI_RFT_cb.EventOccurred(SPI_RFT_Event_CRCError);
					}
					else
					{
            if (Crypt_Bytes(&crypto_buffer_out[sizeof(data_len_t)],p_out_buffer,*(data_len_t*)&crypto_buffer_out - sizeof(data_len_t)) == AES_SUCCESS)
            {
              if (SPI_RFT_cb.RxComplete != NULL)
                SPI_RFT_cb.RxComplete();  // Callback
            }
            else
              if (SPI_RFT_cb.EventOccurred != NULL)
                SPI_RFT_cb.EventOccurred(SPI_RFT_Event_AESError);
					}
				}
				break;
			}
			case SPI_RFT_Proc_Dummy:
			{
				break;
			}
		}
	}
	SPI_RFT_Try_Send();
}

// Initialization
void SPI_RFT_Init(SPI_RFT_cb_TypeDef* pSPI_cb)
{
  SPI_RFT_cb = *pSPI_cb;
  SPI_RFT_Configure(SPI_RFT_SPI_Callback);

  status = SPI_RFT_RXTX_Off;
  state = SPI_RFT_Free;
}

// Status
SPI_RFT_Status SPI_RFT_Get_Status(void)
{
  return status;
}

// IRO Interrupt
void SPI_RFT_IRO_IRQHandler(void)
{
	RFT_Cycle_Buffer_Item_TypeDef item = {0,RFT_ITEM_STS};
	SPI_RFT_Add_Data(item,SPI_RFT_High); // Read STSREG
}

// Function to send data. It automatically turns on TX module before transmission.
SPI_RFT_Retval SPI_RFT_Write_Packet(uint8_t* data, data_len_t data_len)
{
	SPI_RFT_Retval a;

	if (data_len > MAX_PACKET_LOAD)
		return SPI_RFT_Error;

	// Service fields are here
	*(uint32_t*)crypto_buffer_out = 0xAAAA;
  crypto_buffer_out[2] = 0x2D;
	crypto_buffer_out[3] = sync_byte;

	// Packet's length
  *(data_len_t*)&crypto_buffer_out[SRVC_BYTES_CNT] = data_len + PROTO_BYTES_CNT + sizeof(data_len_t);

  Crypt_Bytes(data,crypto_buffer_out + SRVC_BYTES_CNT + sizeof(data_len_t),data_len);

	a = SPI_RFT_Enable_Transmitter();
  if (a != SPI_RFT_OK)
    return a;

	if ((status != SPI_RFT_TX_On) || (state == SPI_RFT_TX))
    return SPI_RFT_Error;
	return SPI_RFT_OK;
}

SPI_RFT_Retval SPI_RFT_Start_Polling(void)
{
  SPI_RFT_Retval a;
  a = SPI_RFT_Enable_Receiver();
  if (a != SPI_RFT_OK)
    return a;
  return SPI_RFT_Restart_Synch_Pattern();
}

//************************************************************************
// Sleep low-power mode functions
//************************************************************************
/* For exit - enable TX or RX */

SPI_RFT_Retval SPI_RFT_Enter_Sleep_Mode(void)
{
	if ((state != SPI_RFT_Free) || (status != SPI_RFT_TX_On))
		// Driver in process
		return SPI_RFT_Busy;
	if (!*IRO)
	{
		// Interrupt is not handled yet
		return SPI_RFT_Busy;
	}

	state = SPI_RFT_Entering_Sleep;

	status_transmission_flag = 0;
	SPI_RFT_IRO_IRQHandler(); // Read STSREG
	while (!status_transmission_flag);

	SPI_RFT_Reset_Flags(SPI_RFT_Register_GENCREG,TXDEN | FIFOEN);
	SPI_RFT_Reset_Flags(SPI_RFT_Register_PMCREG,RXCEN | BBCEN | TXCEN |
																							SYNEN | OSCEN | LBDEN | WUTEN);
	SPI_RFT_Set_Flags(SPI_RFT_Register_PMCREG,CLKOEN);

	while (state != SPI_RFT_Free);

	status = SPI_RFT_SLEEP_MODE;
	state = SPI_RFT_Free;
	return SPI_RFT_OK;
}

//************************************************************************
// Here go 4 ON-OFF functions for transmitter and receiver module
//************************************************************************

SPI_RFT_Retval SPI_RFT_Enable_Transmitter(void)
{
  switch (status)
  {
    case SPI_RFT_RX_On:
    {
      if (SPI_RFT_Disable_Receiver() == SPI_RFT_Busy)
        return SPI_RFT_Busy;
    }
		case SPI_RFT_SLEEP_MODE:
    case SPI_RFT_RXTX_Off:
    {
      SPI_RFT_Set_Flags(SPI_RFT_Register_GENCREG,TXDEN);
      SPI_RFT_Set_Flags(SPI_RFT_Register_PMCREG,TXCEN);
      status = SPI_RFT_TX_On;
    }
    case SPI_RFT_TX_On:
      return SPI_RFT_OK;
  }
  return SPI_RFT_Error;
}

SPI_RFT_Retval SPI_RFT_Enable_Receiver(void)
{
  switch (status)
  {
    case SPI_RFT_TX_On:
    {
      if (SPI_RFT_Disable_Transmitter() == SPI_RFT_Busy)
        return SPI_RFT_Busy;
    }
		case SPI_RFT_SLEEP_MODE:
    case SPI_RFT_RXTX_Off:
    {
      SPI_RFT_Set_Flags(SPI_RFT_Register_GENCREG,FIFOEN);
      SPI_RFT_Set_Flags(SPI_RFT_Register_PMCREG,RXCEN);
      status = SPI_RFT_RX_On;
    }
    case SPI_RFT_RX_On:
      return SPI_RFT_OK;
  }
  return SPI_RFT_Error;
}

SPI_RFT_Retval SPI_RFT_Disable_Transmitter(void)
{
  switch (status)
  {
    case SPI_RFT_TX_On:
    {
      if (state == SPI_RFT_TX)
        return SPI_RFT_Busy;
      SPI_RFT_Reset_Flags(SPI_RFT_Register_PMCREG,TXCEN);
      SPI_RFT_Reset_Flags(SPI_RFT_Register_GENCREG,TXDEN);
      status = SPI_RFT_RXTX_Off;
    }
		case SPI_RFT_SLEEP_MODE:
    case SPI_RFT_RXTX_Off:
    case SPI_RFT_RX_On:
      return SPI_RFT_OK;
  }
  return SPI_RFT_Error;
}

SPI_RFT_Retval SPI_RFT_Disable_Receiver(void)
{
  switch (status)
  {
    case SPI_RFT_RX_On:
    {
      if (state == SPI_RFT_RX)
        return SPI_RFT_Busy;
      SPI_RFT_Reset_Flags(SPI_RFT_Register_PMCREG,RXCEN);
      SPI_RFT_Reset_Flags(SPI_RFT_Register_GENCREG,FIFOEN);
      status = SPI_RFT_RXTX_Off;
    }
		case SPI_RFT_SLEEP_MODE:
    case SPI_RFT_RXTX_Off:
    case SPI_RFT_TX_On:
      return SPI_RFT_OK;
  }
  return SPI_RFT_Error;
}

//************************************************************************
// Here goes command functions for RFT configuration. All of it work through SPI_RFT_Send_Cmd
//************************************************************************

SPI_RFT_Retval SPI_RFT_Try_Send_Command(uint16_t* rft_register, uint16_t bits_clear, uint16_t bits_set)
{
  uint16_t temp_reg;
  temp_reg = *rft_register & ~(bits_clear);
  temp_reg |= bits_set;
	if (*rft_register == temp_reg)
		return SPI_RFT_OK;
  if (SPI_RFT_Send_Data(temp_reg,RFT_ITEM_CMD) == SPI_RFT_OK)
  {
    *rft_register = temp_reg;
    return SPI_RFT_OK;
  }
  else
    return SPI_RFT_Error;
}

SPI_RFT_Retval SPI_RFT_Set_Flags(SPI_RFT_Register reg_index, uint16_t flags)
{
  return SPI_RFT_Try_Send_Command(&register_state[reg_index],0x00,flags);
}

SPI_RFT_Retval SPI_RFT_Reset_Flags(SPI_RFT_Register reg_index, uint16_t flags)
{
  return SPI_RFT_Try_Send_Command(&register_state[reg_index],flags,0x00);
}

SPI_RFT_Retval SPI_RFT_Restart_Synch_Pattern(void)
{
  if (SPI_RFT_Reset_Flags(SPI_RFT_Register_FIFORSTREG,FSCF) != SPI_RFT_OK)
    return SPI_RFT_Error;
  if (SPI_RFT_Set_Flags(SPI_RFT_Register_FIFORSTREG,FSCF) != SPI_RFT_OK)
    return SPI_RFT_Error;
  return SPI_RFT_OK;
}

SPI_RFT_Retval SPI_RFT_Set_Frequency_Band(SPI_RFT_Freq_Band band)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_GENCREG],FBS_0 | FBS_1,(uint8_t)band << 4);
}

SPI_RFT_Retval SPI_RFT_Set_Load_Capacitance(SPI_RFT_Load_Cap load_cap)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_GENCREG],LCS_0 | LCS_1 | LCS_2 | LCS_3,(uint8_t)load_cap);
}

SPI_RFT_Retval SPI_RFT_Set_Modulation_Bandwidth(SPI_RFT_Modulation_Bandwidth mod_bandwidth)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_TXCREG],MODBW_0 | MODBW_1 | MODBW_2 | MODBW_3,(uint8_t)mod_bandwidth << 4);
}

SPI_RFT_Retval SPI_RFT_Set_Transmit_Power(SPI_RFT_Transmit_Power tx_power)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_TXCREG],OTXPWR_0 | OTXPWR_1 | OTXPWR_2,(uint8_t)tx_power);
}

SPI_RFT_Retval SPI_RFT_Set_Center_Frequency(uint16_t cent_freq)
{
  cent_freq &= 0x0FFF;
  return SPI_RFT_Send_Data(RFT_CW_CFSREG | cent_freq,RFT_ITEM_CMD);
}

SPI_RFT_Retval SPI_RFT_Set_Receiver_Baseband_Bandwidth(SPI_RFT_Receiver_Baseband_Bandwidth baseband_bandwith)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_RXCREG],RXBW_0 | RXBW_1 | RXBW_2,(uint8_t)baseband_bandwith << 5);
}

SPI_RFT_Retval SPI_RFT_Set_Receiver_LNA_Gain(SPI_RFT_Receiver_LNA_Gain LNA_gain)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_RXCREG],RXLNA_0 | RXLNA_1,(uint8_t)LNA_gain << 3);
}

SPI_RFT_Retval SPI_RFT_Set_Digital_RSSI_Threshold(SPI_RFT_Digital_RSSI_Threshold RSSI_threshold)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_RXCREG],DRSSIT_0 | DRSSIT_1 | DRSSIT_2,(uint8_t)RSSI_threshold);
}

SPI_RFT_Retval SPI_RFT_Set_Data_Quality_Threshold(uint8_t quality_threshold)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_BBFCREG],DQTI_0 | DQTI_1 | DQTI_2,(uint8_t)quality_threshold);
}

SPI_RFT_Retval SPI_RFT_Set_FIFO_Fill_Count(uint8_t fill_count)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_FIFORSTREG],FFBC_0 | FFBC_1 | FFBC_2 | FFBC_3,(fill_count & 0x0F) << 4);
}

SPI_RFT_Retval SPI_RFT_Set_Synch_Byte(uint8_t synch_byte)
{
	sync_byte = synch_byte;
  return SPI_RFT_Send_Data(RFT_CW_SYNBREG | synch_byte,RFT_ITEM_CMD);
}

SPI_RFT_Retval SPI_RFT_Set_Data_Rate(uint16_t data_rate)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_DRSREG],0x7F,data_rate & 0x7F);
}

SPI_RFT_Retval SPI_RFT_Set_Wakeup_Timer(uint8_t interval_multiplier, uint8_t interval_exponent)
{
  return SPI_RFT_Send_Data(RFT_CW_WTSREG | interval_multiplier | ((interval_exponent & 0x0F) << 8),RFT_ITEM_CMD);
}

SPI_RFT_Retval SPI_RFT_Set_Duty_Cycle(uint8_t duty_cycle)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_DCSREG],0xFE,(duty_cycle & 0x7F) << 1);
}

SPI_RFT_Retval SPI_RFT_Set_Clock_Output_Frequency(SPI_RFT_Clock_Output_Freq clock_freq)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_BCSREG],COFSB_0 | COFSB_1 | COFSB_2,(uint8_t)clock_freq << 5);
}

SPI_RFT_Retval SPI_RFT_Set_Low_Battery_Detect(uint8_t threshold)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_BCSREG],LBDVB_0 | LBDVB_1 | LBDVB_2 | LBDVB_3,threshold);
}

SPI_RFT_Retval SPI_RFT_Set_Clock_Buffer_Time(SPI_RFT_Clock_Buffer_Time clock_buffer_time)
{
  return SPI_RFT_Try_Send_Command(&register_state[SPI_RFT_Register_PLLCREG],CBTC_0 | CBTC_1,(uint8_t)clock_buffer_time);
}
