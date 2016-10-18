/**
  ******************************************************************************
  * @file    Project/STM32F4_EVB_Demo/main.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    18-March-2013
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "tm_stm32f4_rtc.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "tcp_echoserver.h"
//#include "usbd_cdc_core.h"
//#include "usbd_usr.h"
//#include "usb_conf.h"
//#include "usbd_desc.h"
#include "stm32f4x7_eth.h"
//#include "netconf.h"
#include "lwip/tcp.h"


/** @addtogroup STM32F4xx_StdPeriph_Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define SYSTEMTICK_PERIOD_MS 	10
#define AUDIO_FILE_SZE			990000
#define AUIDO_START_ADDRESS		58 /* Offset relative to audio file header size */
#define MAX_COMMAND_NUM 8



/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern uint32_t cid, revid;
//USB_OTG_CORE_HANDLE	USB_OTG_dev;
__IO uint32_t LocalTime = 0; /* this variable is used to create a time reference incremented by 10ms */
__IO uint32_t uwVolume = 70;
static __IO uint32_t uwTimingDelay;
uint16_t uhNumDataRead;
TM_RTC_t datatime;
char RxDataBuffer[256] = "";
char ch_return[100] = "Not yet!";
uint8_t ReceiveFlag = 0, LCD_LINE_NUM = 1, BufferEmptyFlag = 1;
uint8_t overflow = 0, output_count = 0;
enum command {
	SET_TIME = 0,
	LED1_ON = 11, LED2_ON = 12, LED3_ON = 13, LED4_ON = 14, 
	LED1_OFF = 15, LED2_OFF = 16, LED3_OFF = 17, LED4_OFF = 18,
	WCS_UPDATE = 21, WCS_LINEUP = 22, WCS_AUTH = 23,
	ROOM_INFO = 31,
	WELCOME = 99
};
struct pbuf *p;
struct WashingClothesServer WCS;
bool welFlag = false, clockFlag = false;
int WCS_lineup_len;
struct WashingClothesServer {
	char lineup_time[9];
	char *lineup_name;
	bool washing;
	struct WashingClothesServer* next;
};
struct WashingClothesServer* WCS_first;
/* Private functions ---------------------------------------------------------*/

/**
  * @brief   Main program
  * @param  None
  * @retval None
  */
int main(void)
{
	/* SysTick end of count event each 10ms */
	RCC_ClocksTypeDef RCC_Clocks;
	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 100);
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
	RNG_Cmd(ENABLE);
	
	STM324xG_LCD_Init();
	BTinit();
	DistanceSensorinit();
	Timinit();
	LEDinit();
	BTinit();
	
	LCD_Clear(Black);
  LCD_SetBackColor(Black);
  LCD_SetTextColor(White);
	
	
	char time[20], date[20], WCS_time[8];
	
	WCS_lineup_len = 0;
	
	ETH_BSP_Config();
		
	LwIP_Init();
	
	tcp_echoserver_init();
	
	LCD_SetFont(&Font12x12);
	while(1)  {
		/*if(ETH_CheckFrameReceived())  {
			LwIP_Pkt_Handle();
		}
		LwIP_Periodic_Handle(LocalTime);*/
			
		/*  Display Clock  */
		if(!clockFlag && welFlag) {
			Clockinit();
			clockFlag = true;
		}
		else if(clockFlag) {
			TM_RTC_GetDateTime(&datatime, TM_RTC_Format_BIN);
			sprintf(time, "%02d:%02d:%02d", datatime.hours, datatime.minutes, datatime.seconds);
			sprintf(date, "20%02d/%02d/%02d", datatime.year, datatime.month, datatime.date);
			sprintf(WCS_time, "%02d%02d%02d%02d", datatime.month, datatime.date, datatime.hours, datatime.minutes);
				
			if(WCS_lineup_len > 0)
				if(WCS_first->washing)
					if(if_time_more_than(WCS_time, WCS_first->lineup_time))
						WCS_Finished();
						
			LCD_LINE_NUM = 3;
			LCD_DisplayStringLine(LINE(LCD_LINE_NUM++), (uint8_t*)time);
			LCD_DisplayStringLine(LINE(LCD_LINE_NUM++), (uint8_t*)date);
			welFlag = false;
		}
			
			
		/*  BLE Listen Using Loop  */
		if(USART_GetITStatus(USART2, USART_IT_RXNE) == RESET && BufferEmptyFlag == 0) {
			LCD_LINE_NUM = 1;
			LCD_Clear(Black);
			if(!isCommand(RxDataBuffer))	printUartData();
		}
	}
		
}

/*  Send request to device to get current time.
 *  It can use BLE or Internet to get current time from devices.
 */
void Clockinit(void)
{
	LCD_LINE_NUM = 1;
	
#ifdef USE_BLE
	
	USART2_SendData("AT\r\n");
	Delay(50);
	while(strcmp(RxDataBuffer, "OK\r\n") == 0) {
		sprintf(RxDataBuffer, "%s", "");
		BufferEmptyFlag = 1;
		LCD_SetTextColor(Yellow);
		LCD_DisplayStringLine(LINE(LCD_LINE_NUM), (uint8_t*)"Wait for BT Connect");
		USART2_SendData("AT\r\n");
		Delay(50);
	}
	
	LCD_Clear(Black);
	// Time request
	USART2_SendData("&&00");
	Delay(50);
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == RESET && BufferEmptyFlag == 0) {
			LCD_LINE_NUM = 1;
			LCD_Clear(Black);
			isCommand(RxDataBuffer);
	}
	else {
		LCD_SetTextColor(Red);
		LCD_DisplayStringLine(LINE(LCD_LINE_NUM), (uint8_t*)"Clock Setting Error!!");
	}
	
#elif defined USE_INTERNET
	
	p = pbuf_alloc(PBUF_IP, 5, PBUF_POOL);
	sprintf(p->payload, "<<00");
	mes.p = p;
	tcp_echoserver_send(&client_tpcb, &mes);
	pbuf_free(p);
	
#else
	
  LCD_SetTextColor(Red);
	LCD_DisplayStringLine(LINE(LCD_LINE_NUM), (uint8_t*)"Clock Setting Error!!");
	
#endif
}
/*  Init LED onBoard.
 */
void LEDinit(void)
{
	STM_EVAL_LEDInit(LED1);
	STM_EVAL_LEDInit(LED2);
	STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED4);
}

/*  Init Timer (For Distance Sensor)
 */
void Timinit(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructer;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	TIM_DeInit(TIM2);
	TIM_TimeBaseInitStructer.TIM_Period=999;
	TIM_TimeBaseInitStructer.TIM_Prescaler=71;
	TIM_TimeBaseInitStructer.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseInitStructer.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructer);
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE);
	TIM_Cmd(TIM2, DISABLE);
	
	NVIC_EnableIRQ(TIM2_IRQn);
}


/*  Init Distance Sensor.
 */
void DistanceSensorinit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	// Distance Senser ECHO CN4 PIN8
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	// Distance Senser TRIG CN4 PIN9
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/*  Init Bluetooth Module.
 */
void BTinit(void)
{
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
	
	// TX  PA2  CN1 PIN37
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// RX  PA3  CN2 PIN3
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART2, &USART_InitStructure);
	USART_Cmd(USART2, ENABLE);
	
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	NVIC_EnableIRQ(USART2_IRQn);
		
	USART_ClearFlag(USART2, USART_FLAG_TC);
}

/*  Adjust the time of the board.
 */
void setTime(void)
{
	char datatime[25];
	for(int i = 4; i < 23; i++) {
		datatime[i - 4] = echo_payload[i];
	}
	// RTC Setting
	if(!TM_RTC_Init(TM_RTC_ClockSource_Internal)) {
		
	}
	TM_RTC_SetDateTimeString(datatime);
}

/*  Get distance between Sensor and the Things.
 *  Return double (Unit: Centermeter).
 */
double DistanceSenser(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_8);
	GPIO_SetBits(GPIOB, GPIO_Pin_8);
	Delay(30);
	GPIO_ResetBits(GPIOB, GPIO_Pin_8);
	TIM_SetCounter(TIM2, 0);
	
	// Wait for ECHO
	while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) == RESET);
	
	// ENABLE TIM2
	TIM_Cmd(TIM2, ENABLE);
	
	// Wait for ECHO
	while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) == SET);
	
	// DISABLE TIM2
	TIM_Cmd(TIM2, DISABLE);
	
	// return length
	return (TIM_GetCounter(TIM2)+overflow*1000)/58.0;
}

void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		overflow++;
	}
}

/*  Find and Run the command.
 */
bool runCommand(int cmd)
{
	switch(cmd) {
			case SET_TIME:
				setTime();
				strcat(echo_payload, ":OK");
				return true;
			case LED1_ON:
				STM_EVAL_LEDOn(LED1);
				strcat(echo_payload, ":OK");
				return true;
			case LED2_ON:
				STM_EVAL_LEDOn(LED2);
				strcat(echo_payload, ":OK");
				return true;
			case LED3_ON:
				STM_EVAL_LEDOn(LED3);
				strcat(echo_payload, ":OK");
				return true;
			case LED4_ON:
				STM_EVAL_LEDOn(LED4);
				strcat(echo_payload, ":OK");
				return true;
			case LED1_OFF:
				STM_EVAL_LEDOff(LED1);
				strcat(echo_payload, ":OK");
				return true;
			case LED2_OFF:
				STM_EVAL_LEDOff(LED2);
				strcat(echo_payload, ":OK");
				return true;
			case LED3_OFF:
				STM_EVAL_LEDOff(LED3);
				strcat(echo_payload, ":OK");
				return true;
			case LED4_OFF:
				STM_EVAL_LEDOff(LED4);
				strcat(echo_payload, ":OK");
				return true;
			case WCS_UPDATE:
				WCS_Update_func();
				return true;
			case WCS_LINEUP:
				WCS_Lineup_func();
				return true;
			case WCS_AUTH:
				WCS_auth();
				return true;
			case ROOM_INFO:
				overflow = 0;
				sprintf(echo_payload, "<<31%.3lf:%02d%02d%02d%02d", DistanceSenser(), datatime.month, datatime.date, datatime.hours, datatime.minutes);
				return true;
			case WELCOME:
				welFlag = true;
				sprintf(echo_payload, "<<99welcome");
				return true;
			default:
				strcat(echo_payload, ":Command not found");
				return false;
		}
}

/*  Check if the string is "Command like" then call runCommand() function to find command.
 *	Return true if the string is "Command like", else Return false;
 */
bool isCommand(char str[]) 
{
	int cmd;
	if(str[0] == '>' && str[1] == '>') {
		if(str[2] == '\0' || str[3] == '\0')  return false;
		else if(str[2] > '9' || str[2] < '0' || str[3] > '9' || str[3] < '0')  return false;
		cmd = (str[2] - '0')*10 + str[3] - '0';
		if(runCommand(cmd)) {
			sprintf(RxDataBuffer, "%s", "");
			BufferEmptyFlag = 1;
			return true;
		}
		return false;
	}
	strcat(echo_payload, ":Not a command");
	return false;
}

/*  Send back WCS line up information.
 */
void WCS_Update_func()
{
	struct WashingClothesServer *WCS = WCS_first;
	
	p = pbuf_alloc(PBUF_IP, 14 + strlen(WCS->lineup_name), PBUF_POOL);
	sprintf(p->payload, "<<21%d", WCS_lineup_len);
	mes.p = p;
	tcp_echoserver_send(&client_tpcb, &mes);
	pbuf_free(p);
	
	while(WCS != NULL) {
		p = pbuf_alloc(PBUF_IP, 14 + strlen(WCS->lineup_name), PBUF_POOL);
		sprintf(p->payload, "<<21,%s:%d:%s", WCS->lineup_name, WCS->washing, WCS->lineup_time);
		WCS = WCS->next;
		mes.p = p;
		tcp_echoserver_send(&client_tpcb, &mes);
		pbuf_free(p);
	}
	sprintf(echo_payload, "<<21,&&Finish:%02d%02d%02d%02d", datatime.month, datatime.date, datatime.hours, datatime.minutes);
}

/*  Add the device to WCS line.
 */
void WCS_Lineup_func()
{
	struct WashingClothesServer *WCS = WCS_first;
	char name[echo_payload[4] - '0' + 1];
	for(int i = 0; i < (echo_payload[4]-'0'); i++) {
		name[i] = echo_payload[i+5];
	}
	name[echo_payload[4] - '0'] = 0;
	if(WCS_Exist(name) == NULL) {
		if(WCS != NULL) {
			while(WCS->next != NULL)  WCS = WCS->next;
			WCS->next = malloc(sizeof(*WCS->next));
			WCS->next->next = NULL;
			WCS = WCS->next;
		}
		else {
			WCS_first = malloc(sizeof(*WCS_first));
			WCS_first->next = NULL;
			WCS = WCS_first;
		}
		WCS_lineup_len++;
		WCS->lineup_name = malloc(sizeof(char) * (echo_payload[4] - '0'));
		sprintf(WCS->lineup_name, "%s", name);
		sprintf(WCS->lineup_time, "00000000");
		WCS->washing = false;
		strcat(echo_payload, " : OK");
	}
	else {
		strcat(echo_payload, " : Device has already exist");
	}
}

/*  WCS
 */
void WCS_auth()
{
#ifdef WCS_AUTH_INTERNET
	
		char name[echo_payload[4] - '0' + 1];
		for(int i = 0; i < (echo_payload[4]-'0'); i++) {
			name[i] = echo_payload[i+5];
		}
		name[echo_payload[4] - '0'] = 0;
		struct WashingClothesServer *WCS = WCS_Exist(name), *WCS_tmp = WCS_first;
		if(WCS != NULL) {
			while(WCS_tmp != WCS) {
				if(!WCS_tmp->washing) {
					sprintf(echo_payload, "<<233"); // Jump the queue
					return;
				}
				WCS_tmp = WCS_tmp->next;
			}
			if(WCS->washing){
				sprintf(echo_payload, "<<232"); // Washing so doesn't need to auth.
				return;
			}
			char time[8];
			sprintf(time, "%02d%02d%02d%02d", datatime.month, datatime.date, datatime.hours, datatime.minutes);
			time_add(time, 1);
			sprintf(WCS->lineup_time, "%s", time);
			sprintf(echo_payload, "<<231"); // OK
			WCS->washing = true;
		}
		else {
			sprintf(echo_payload, "<<230"); // Device not found
		}
		
#elif defined WCS_AUTH_BLE
		
		char name[RxDataBuffer[4] - '0' + 1];
		for(int i = 0; i < (RxDataBuffer[4]-'0'); i++) {
			name[i] = RxDataBuffer[i+5];
		}
		name[RxDataBuffer[4] - '0'] = 0;
		struct WashingClothesServer *WCS = WCS_Exist(name), *WCS_tmp = WCS_first;
		if(WCS != NULL) {
			while(WCS_tmp != WCS) {
				if(!WCS_tmp->washing) {
					USART2_SendData("<<233"); // Jump the queue
					return;
				}
				WCS_tmp = WCS_tmp->next;
			}
			if(WCS->washing){
				USART2_SendData("<<232"); // Washing so don't need to auth.
				return;
			}
			char time[8];
			sprintf(time, "%02d%02d%02d%02d", datatime.month, datatime.date, datatime.hours, datatime.minutes);
			time_add(time, 1);
			sprintf(WCS->lineup_time, "%s", time);
			USART2_SendData("<<231"); // OK
			WCS->washing = true;
		}
		else {
			USART2_SendData("<<230"); // Device not found
		}
		
#endif
}

/*  Check if device exist in the WCS queue.
 *  If exist, return the device address. Else return NULL;
 */
struct WashingClothesServer* WCS_Exist(char name[])
{
	struct WashingClothesServer *WCS = WCS_first;
	while(WCS != NULL){
		if(strcmp(WCS->lineup_name, name) == 0)
			return WCS;
		WCS = WCS->next;
	}
	return NULL;
}

/*  Kill the device in the queue.
 */
void WCS_Finished()
{
	if(WCS_lineup_len > 0) {
		struct WashingClothesServer *WCS = WCS_first->next;
		free(WCS_first->lineup_name);
		free(WCS_first);
		WCS_first = WCS;
		WCS_lineup_len--;
	}
}

/*  Time compare func.
 */
bool if_time_more_than(const char a[], const char b[])
{
	for(int i = 0; i < 8; i++) {
		if(a[i] > b[i])
			return true;
	}
	return false;
}

/*  Add time(minutes) to a certain time.
 */
void time_add(char time_orig[], const int time_plus)
{
	int minute = 0, hour = 0, date = 0, month = 0, remainder = 0;
	minute = time_orig[7] - '0' + (time_orig[6] - '0')*10 + time_plus;
	remainder = minute / 60;
	minute %= 60;
	hour = time_orig[5] - '0' + (time_orig[4] - '0')*10 + remainder;
	remainder = hour / 24;
	hour %= 24;
	month = time_orig[1] - '0' + (time_orig[0] - '0')*10;
	date = time_orig[3] - '0' + (time_orig[2] - '0')*10 + remainder;
	while(date > monthGetDate(month)) {
		date -= monthGetDate(month);
		if(month == 12) month = 1;
		else 			      month++;
	}
	
	time_orig[7] = '0' + minute%10;
	time_orig[6] = '0' + minute/10;
	time_orig[5] = '0' + hour%10;
	time_orig[4] = '0' + hour/10;
	time_orig[3] = '0' + date%10;
	time_orig[2] = '0' + date/10;
	time_orig[1] = '0' + month%10;
	time_orig[0] = '0' + month/10;
}

/*  Return how many date in certain month.
 */
int monthGetDate(int month)
{
	switch(month) {
		case 1:	 return 31;
		case 2:	 return 28;
		case 3:  return 31;
		case 4:  return 30;
		case 5:  return 31;
		case 6:  return 30;
		case 7:  return 31;
		case 8:  return 31;
		case 9:  return 30;
		case 10: return 31;
		case 11: return 30;
		case 12: return 31;
	}
	return -1;
}

/*  BT SendData.
 */
void USART2_SendData(char *str) 
{
	while(*str) {
		USART_SendData(USART2, *str++);
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	}
}

/*  Display receive data from BT on the LCD.
 */
void printUartData(void)
{
	uint16_t BufferIndex = 0;
	if(BufferEmptyFlag)  return;
	sprintf(ch_return, "%s", "");
	while(RxDataBuffer[BufferIndex] != '\0') {
		if(RxDataBuffer[BufferIndex] != '\r' && RxDataBuffer[BufferIndex] != '\n') {
			sprintf(ch_return, "%s%c", ch_return, RxDataBuffer[BufferIndex]);
		}
		else if(RxDataBuffer[BufferIndex] == '\n') {
			LCD_ClearLine(LINE(LCD_LINE_NUM));
			LCD_DisplayStringLine(LINE(LCD_LINE_NUM++), (uint8_t*)ch_return);
			sprintf(ch_return, "%s", "");
		}
		BufferIndex++;
	}
	sprintf(RxDataBuffer, "%s", "");
	BufferEmptyFlag = 1;
}

/*  Store receive data from BT in RxDataBuffer.
 */
void getUartData(void)
{
	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
		char c = USART_ReceiveData(USART2);
		sprintf(RxDataBuffer, "%s%c", RxDataBuffer, c);
		BufferEmptyFlag = 0;
	}
}

/*  IRQ of BT.
 */
void USART2_IRQHandler(void)
{
	getUartData();
}

/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in milliseconds.
  * @retval None
  */
void Delay(__IO uint32_t nTime)
{
	uwTimingDelay = nTime;
 
	while (uwTimingDelay != 0);
}

/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void)
{
	if (uwTimingDelay != 0x00)
	{
		uwTimingDelay--;
	}
}

/**
  * @brief  Updates the system local time
  * @param  None
  * @retval None
  */
void Time_Update(void)
{
	LocalTime += SYSTEMTICK_PERIOD_MS;
}

/*--------------------------------
       Callbacks implementation:
           the callbacks prototypes are defined in the stm32f4_evb_audio_codec.h file
           and their implementation should be done in the user coed if they are needed.
           Below some examples of callback implementations.
                                     --------------------------------------------------------*/
/**
  * @brief  Calculates the remaining file size and new position of the pointer.
  * @param  None
  * @retval None
  */
void EVAL_AUDIO_TransferComplete_CallBack(uint32_t pBuffer, uint32_t Size)
{
#ifdef AUDIO_MAL_MODE_NORMAL  
	/* Replay from the beginning */
#else /* #ifdef AUDIO_MAL_MODE_CIRCULAR */
	/* Display message on the LCD screen */
	LCD_DisplayStringLine(Line8, " All Buffer Reached ");   
#endif /* AUDIO_MAL_MODE_CIRCULAR */
}

/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void EVAL_AUDIO_HalfTransfer_CallBack(uint32_t pBuffer, uint32_t Size)
{  
#ifdef AUDIO_MAL_MODE_CIRCULAR
	/* Display message on the LCD screen */
	LCD_DisplayStringLine(Line8, " 1/2 Buffer Reached "); 
#endif /* AUDIO_MAL_MODE_CIRCULAR */
}

/**
  * @brief  Manages the DMA FIFO error interrupt.
  * @param  None
  * @retval None
  */
void EVAL_AUDIO_Error_CallBack(void* pData)
{
	/* Display message on the LCD screen */
	LCD_SetBackColor(Red);
	LCD_DisplayStringLine(Line8, (uint8_t *)"     DMA  ERROR     ");
	/* Stop the program with an infinite loop */
	while (1) {};
}

/**
  * @brief  Basic management of the timeout situation.
  * @param  None.
  * @retval None.
  */
uint32_t Codec_TIMEOUT_UserCallback(void)
{
	/* Display message on the LCD screen */
	LCD_DisplayStringLine(Line8, (uint8_t *)"  CODEC I2C  ERROR  ");  

	/* Block communication and all processes */
	while (1) {};
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1)
	{
	}
}
#endif

/**
  * @}
  */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
