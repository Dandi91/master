// Library for RFT MRF49XA
#ifndef __MRF49XA_H__
#define __MRF49XA_H__

#include <STM32F37x.h>

#ifndef NULL
#define NULL    0
#endif

typedef uint16_t data_len_t;		// Type used for data length field in packets
#define PROTO_BYTES_CNT 3				// Number of protocol bytes - addresses and commands
#define SRVC_BYTES_CNT 4				// Number of service bytes - preambule and sync. word
#define MAX_PACKET_LOAD 4096		// Max. number of bytes in packet's data field
#define MAX_TRSM_CHAIN 140      // Max. length of transmitter chain

// Cycle buffer item's types
#define RFT_ITEM_CMD	0x0001		// Command item
#define RFT_ITEM_TXD	0x0002		// TX data item
#define RFT_ITEM_RXD	0x0004		// RX data item
#define RFT_ITEM_STS	0x0008		// Interrupt status request

typedef struct RFT_Cycle_Buffer_Item
{
  uint16_t data;
	uint16_t type;
} RFT_Cycle_Buffer_Item_TypeDef;

// Command words for RFT registers
#define RFT_CW_GENCREG     0x8000
#define RFT_CW_AFCCREG     0xC400
#define RFT_CW_TXCREG      0x9800
#define RFT_CW_TXBREG      0xB800
#define RFT_CW_CFSREG      0xA000
#define RFT_CW_RXCREG      0x9000
#define RFT_CW_BBFCREG     0xC200
#define RFT_CW_RXFIFOREG   0xB000
#define RFT_CW_FIFORSTREG  0xCA00
#define RFT_CW_SYNBREG     0xCE00
#define RFT_CW_DRSREG      0xC600
#define RFT_CW_PMCREG      0x8200
#define RFT_CW_WTSREG      0xE000
#define RFT_CW_DCSREG      0xC800
#define RFT_CW_BCSREG      0xC000
#define RFT_CW_PLLCREG     0xCC00

typedef enum
{
  SPI_RFT_Register_GENCREG = 0,
  SPI_RFT_Register_AFCCREG,
  SPI_RFT_Register_TXCREG,
  SPI_RFT_Register_RXCREG,
  SPI_RFT_Register_BBFCREG,
  SPI_RFT_Register_FIFORSTREG,
  SPI_RFT_Register_DRSREG,
  SPI_RFT_Register_PMCREG,
  SPI_RFT_Register_DCSREG,
  SPI_RFT_Register_BCSREG,
  SPI_RFT_Register_PLLCREG
} SPI_RFT_Register;

// STSREG bits
#define TXRXFIFO        0x8000
#define POR             0x4000
#define TXOWRXOF        0x2000
#define WUTINT          0x1000
#define LCEXINT         0x0800
#define LBTD            0x0400
#define FIFOEM          0x0200
#define ATRSSI          0x0100
#define DQDO            0x0080
#define CLKRL           0x0040
#define AFCCT           0x0020
#define OFFSV           0x0010
#define OFFSB_3         0x0008
#define OFFSB_2         0x0004
#define OFFSB_1         0x0002
#define OFFSB_0         0x0001

// GENCREG bits
#define TXDEN           0x0080
#define FIFOEN          0x0040
#define FBS_1           0x0020
#define FBS_0           0x0010
#define LCS_3           0x0008
#define LCS_2           0x0004
#define LCS_1           0x0002
#define LCS_0           0x0001

// AFCCREG bits
#define AUTOMS_1        0x0080
#define AUTOMS_0        0x0040
#define ARFO_1          0x0020
#define ARFO_0          0x0010
#define MFCS            0x0008
#define HAM             0x0004
#define FOREN           0x0002
#define FOFEN           0x0001

// TXCREG bits
#define MODPLY          0x0100
#define MODBW_3         0x0080
#define MODBW_2         0x0040
#define MODBW_1         0x0020
#define MODBW_0         0x0010
#define OTXPWR_2        0x0004
#define OTXPWR_1        0x0002
#define OTXPWR_0        0x0001

// RXCREG bits
#define FINTDIO         0x0400
#define DIORT_1         0x0200
#define DIORT_0         0x0100
#define RXBW_2          0x0080
#define RXBW_1          0x0040
#define RXBW_0          0x0020
#define RXLNA_1         0x0010
#define RXLNA_0         0x0008
#define DRSSIT_2        0x0004
#define DRSSIT_1        0x0002
#define DRSSIT_0        0x0001

// BBFCREG bits
#define ACRLC           0x0080
#define MCRLC           0x0040
#define FTYPE           0x0010
#define DQTI_2          0x0004
#define DQTI_1          0x0002
#define DQTI_0          0x0001

// FIFORSTREG bits
#define FFBC_3          0x0080
#define FFBC_2          0x0040
#define FFBC_1          0x0020
#define FFBC_0          0x0010
#define SYCHLEN         0x0008
#define FFSC            0x0004
#define FSCF            0x0002
#define DRSTM           0x0001

// DRSREG bits
#define DRPE            0x0080

// PMCREG bits
#define RXCEN           0x0080
#define BBCEN           0x0040
#define TXCEN           0x0020
#define SYNEN           0x0010
#define OSCEN           0x0008
#define LBDEN           0x0004
#define WUTEN           0x0002
#define CLKOEN          0x0001

// DCSREG bits
#define DCMEN           0x0001

// BCSREG bits
#define COFSB_2         0x0080
#define COFSB_1         0x0040
#define COFSB_0         0x0020
#define LBDVB_3         0x0008
#define LBDVB_2         0x0004
#define LBDVB_1         0x0002
#define LBDVB_0         0x0001

// PLLCREG bits
#define CBTC_1          0x0040
#define CBTC_0          0x0020
#define PDDS            0x0008
#define PLLDD           0x0004
#define PLLBWB          0x0001

// Status register flags
typedef enum
{
	SPI_RFT_Event_TX = 0,
	SPI_RFT_Event_RX,
  SPI_RFT_Event_PowerOnReset,
	SPI_RFT_Event_Overflow,
	SPI_RFT_Event_Underrun,
  SPI_RFT_Event_WakeupTimer,
  SPI_RFT_Event_EXTI,
  SPI_RFT_Event_LowVoltage,
	SPI_RFT_Event_CRCError,
  SPI_RFT_Event_AESError
} SPI_RFT_Event;

// Frequency Band Select bits
typedef enum
{
  SPI_RFT_Freq_Band_433_MHz = 1,
  SPI_RFT_Freq_Band_868_MHz,
  SPI_RFT_Freq_Band_915_MHz
} SPI_RFT_Freq_Band;

// Load Capacitance Select bits
typedef enum
{
  SPI_RFT_Load_Cap_85 = 0,
  SPI_RFT_Load_Cap_90,
  SPI_RFT_Load_Cap_95,
  SPI_RFT_Load_Cap_100,
  SPI_RFT_Load_Cap_105,
  SPI_RFT_Load_Cap_110,
  SPI_RFT_Load_Cap_115,
  SPI_RFT_Load_Cap_120,
  SPI_RFT_Load_Cap_125,
  SPI_RFT_Load_Cap_130,
  SPI_RFT_Load_Cap_135,
  SPI_RFT_Load_Cap_140,
  SPI_RFT_Load_Cap_145,
  SPI_RFT_Load_Cap_150,
  SPI_RFT_Load_Cap_155,
  SPI_RFT_Load_Cap_160
} SPI_RFT_Load_Cap;

// Modulation Bandwidth bits
typedef enum
{
  SPI_RFT_Modulation_Bandwidth_15kHz = 0,
  SPI_RFT_Modulation_Bandwidth_30kHz,
  SPI_RFT_Modulation_Bandwidth_45kHz,
  SPI_RFT_Modulation_Bandwidth_60kHz,
  SPI_RFT_Modulation_Bandwidth_75kHz,
  SPI_RFT_Modulation_Bandwidth_90kHz,
  SPI_RFT_Modulation_Bandwidth_105kHz,
  SPI_RFT_Modulation_Bandwidth_120kHz,
  SPI_RFT_Modulation_Bandwidth_135kHz,
  SPI_RFT_Modulation_Bandwidth_150kHz,
  SPI_RFT_Modulation_Bandwidth_165kHz,
  SPI_RFT_Modulation_Bandwidth_180kHz,
  SPI_RFT_Modulation_Bandwidth_195kHz,
  SPI_RFT_Modulation_Bandwidth_210kHz,
  SPI_RFT_Modulation_Bandwidth_225kHz,
  SPI_RFT_Modulation_Bandwidth_240kHz
} SPI_RFT_Modulation_Bandwidth;

// Output Transmit Power Range bits
typedef enum
{
  SPI_RFT_Transmit_Power_0 = 0,
  SPI_RFT_Transmit_Power_25,
  SPI_RFT_Transmit_Power_50,
  SPI_RFT_Transmit_Power_75,
  SPI_RFT_Transmit_Power_100,
  SPI_RFT_Transmit_Power_125,
  SPI_RFT_Transmit_Power_150,
  SPI_RFT_Transmit_Power_175
} SPI_RFT_Transmit_Power;

// Receiver Baseband Bandwidth bits
typedef enum
{
  SPI_RFT_Receiver_Baseband_Bandwidth_400kHz = 1,
  SPI_RFT_Receiver_Baseband_Bandwidth_340kHz,
  SPI_RFT_Receiver_Baseband_Bandwidth_270kHz,
  SPI_RFT_Receiver_Baseband_Bandwidth_200kHz,
  SPI_RFT_Receiver_Baseband_Bandwidth_134kHz,
  SPI_RFT_Receiver_Baseband_Bandwidth_67kHz
} SPI_RFT_Receiver_Baseband_Bandwidth;

// Receiver LNA Gain bits
typedef enum
{
  SPI_RFT_Receiver_LNA_Gain_0dB = 0,
  SPI_RFT_Receiver_LNA_Gain_6dB,
  SPI_RFT_Receiver_LNA_Gain_14dB,
  SPI_RFT_Receiver_LNA_Gain_20dB
} SPI_RFT_Receiver_LNA_Gain;

// Digital RSSI Threshold bits
typedef enum
{
  SPI_RFT_Digital_RSSI_Threshold_103dB = 0,
  SPI_RFT_Digital_RSSI_Threshold_97dB,
  SPI_RFT_Digital_RSSI_Threshold_91dB,
  SPI_RFT_Digital_RSSI_Threshold_85dB,
  SPI_RFT_Digital_RSSI_Threshold_79dB,
  SPI_RFT_Digital_RSSI_Threshold_73dB
} SPI_RFT_Digital_RSSI_Threshold;

// Clock Output Frequency Set bits
typedef enum
{
  SPI_RFT_Clock_Output_Freq_1000kHz = 0,
  SPI_RFT_Clock_Output_Freq_1250kHz,
  SPI_RFT_Clock_Output_Freq_1660kHz,
  SPI_RFT_Clock_Output_Freq_2000kHz,
  SPI_RFT_Clock_Output_Freq_2500kHz,
  SPI_RFT_Clock_Output_Freq_3330kHz,
  SPI_RFT_Clock_Output_Freq_5000kHz,
  SPI_RFT_Clock_Output_Freq_10000kHz
} SPI_RFT_Clock_Output_Freq;

// Clock Buffer Time Control bits
typedef enum
{
  SPI_RFT_Clock_Buffer_Time_2500kHz_and_less = 1,
  SPI_RFT_Clock_Buffer_Time_3300kHz,
  SPI_RFT_Clock_Buffer_Time_5MHz_to_10MHz
} SPI_RFT_Clock_Buffer_Time;

// RFT Return Values
typedef enum
{
  SPI_RFT_OK = 0,
  SPI_RFT_Busy,
  SPI_RFT_Error
} SPI_RFT_Retval;

// RFT Status
typedef enum
{
  SPI_RFT_RXTX_Off = 0,
  SPI_RFT_TX_On,
  SPI_RFT_RX_On,
	SPI_RFT_SLEEP_MODE
} SPI_RFT_Status;

// RFT State
typedef enum
{
  SPI_RFT_Free = 0,
  SPI_RFT_Cmd,
  SPI_RFT_TX,
  SPI_RFT_RX,
	SPI_RFT_Entering_Sleep
} SPI_RFT_State;

// RFT Callback Structure
typedef struct
{
  void (*BufferEmpty)(void);
	void (*TxComplete)(void);
	void (*RxComplete)(void);
	// Gets length for packet, returns pointer to buffer
  uint8_t* (*RxBegin)(data_len_t length);
	void (*EventOccurred)(SPI_RFT_Event event);
  void (*BufferFilled)(void);
} SPI_RFT_cb_TypeDef;

void SPI_RFT_Init(SPI_RFT_cb_TypeDef* pSPI_cb);

void SPI_RFT_IRO_IRQHandler(void);

SPI_RFT_Status SPI_RFT_Get_Status(void);

SPI_RFT_Retval SPI_RFT_Enable_Transmitter(void);
SPI_RFT_Retval SPI_RFT_Enable_Receiver(void);
SPI_RFT_Retval SPI_RFT_Disable_Transmitter(void);
SPI_RFT_Retval SPI_RFT_Disable_Receiver(void);
SPI_RFT_Retval SPI_RFT_Enter_Sleep_Mode(void);

SPI_RFT_Retval SPI_RFT_Start_Polling(void);
SPI_RFT_Retval SPI_RFT_Write_Packet(uint8_t* data, data_len_t data_len);

SPI_RFT_Retval SPI_RFT_Set_Flags(SPI_RFT_Register reg_index, uint16_t flags);
SPI_RFT_Retval SPI_RFT_Reset_Flags(SPI_RFT_Register reg_index, uint16_t flags);

SPI_RFT_Retval SPI_RFT_Restart_Synch_Pattern(void);

SPI_RFT_Retval SPI_RFT_Set_Frequency_Band(SPI_RFT_Freq_Band band);
SPI_RFT_Retval SPI_RFT_Set_Load_Capacitance(SPI_RFT_Load_Cap load_cap);
SPI_RFT_Retval SPI_RFT_Set_Modulation_Bandwidth(SPI_RFT_Modulation_Bandwidth mod_bandwidth);
SPI_RFT_Retval SPI_RFT_Set_Transmit_Power(SPI_RFT_Transmit_Power tx_power);
SPI_RFT_Retval SPI_RFT_Set_Center_Frequency(uint16_t cent_freq);
SPI_RFT_Retval SPI_RFT_Set_Receiver_Baseband_Bandwidth(SPI_RFT_Receiver_Baseband_Bandwidth baseband_bandwith);
SPI_RFT_Retval SPI_RFT_Set_Receiver_LNA_Gain(SPI_RFT_Receiver_LNA_Gain LNA_gain);
SPI_RFT_Retval SPI_RFT_Set_Digital_RSSI_Threshold(SPI_RFT_Digital_RSSI_Threshold RSSI_threshold);
SPI_RFT_Retval SPI_RFT_Set_Data_Quality_Threshold(uint8_t quality_threshold);
SPI_RFT_Retval SPI_RFT_Set_FIFO_Fill_Count(uint8_t fill_count);
SPI_RFT_Retval SPI_RFT_Set_Synch_Byte(uint8_t synch_byte);
SPI_RFT_Retval SPI_RFT_Set_Data_Rate(uint16_t data_rate);
SPI_RFT_Retval SPI_RFT_Set_Wakeup_Timer(uint8_t interval_multiplier, uint8_t interval_exponent);
SPI_RFT_Retval SPI_RFT_Set_Duty_Cycle(uint8_t duty_cycle);
SPI_RFT_Retval SPI_RFT_Set_Clock_Output_Frequency(SPI_RFT_Clock_Output_Freq clock_freq);
SPI_RFT_Retval SPI_RFT_Set_Low_Battery_Detect(uint8_t threshold);
SPI_RFT_Retval SPI_RFT_Set_Clock_Buffer_Time(SPI_RFT_Clock_Buffer_Time clock_buffer_time);

#endif
