/*************	Локальное объявление констант	**************/
#define MAIN_Fosc		22118400L	//Определение главных часов
#define	RX1_Lenth		32			//Длина последовательного принимаемого буфера
#define	BaudRate1		115200UL	//Скорость передачи
#define	Timer1_Reload	(65536UL -(MAIN_Fosc / 4 / BaudRate1))		//Timer 1 значение, соответствующее 300KHZ
#define	Timer2_Reload	(65536UL -(MAIN_Fosc / 4 / BaudRate1))		//Timer 2 значение, соответствующее 300KHZ
#include "STC15Fxxxx.H"
#include <intrins.h>
#define uchar unsigned char
#define uint unsigned int
/*************	Объявление локальных переменных	**************/
u8	idata RX1_Buffer[RX1_Lenth];	//буфер приема
u8	TX1_Cnt;	//Счётчик отправленого
u8	RX1_Cnt;	//Счётчик полученного
bit	B_TX1_Busy;	//Флаг занятости отправленного
/*************	Определение портов вывода **************/
sbit LED1=P1^0;//LED1
sbit LED2=P1^1;//LED2
sbit LED3=P3^7;//LED3
sbit DK1=P3^3;//Реле
sbit BEEP=P3^4;//Зуммер
sbit K1=P1^3;//Кнопка 1
sbit K2=P1^2;//Кнопка 2
sbit K3=P1^4;//Кнопка 3
sbit DQ=P1^6;//Датчик температуры 18B20
char led1bl,led2bl,led3bl;
#define jump_ROM 0xCC                   //Смещение до ROM
#define start    0x44               //Начальная команда запуска
#define read_EEROM 0xBE                 //Команда чтения памяти
uchar TMPH,TMPL;                //Значения температуры
uchar HL;                      //Текущая температура
unsigned char LYMS[13]={0x41,0x54,0x2B,0x43,0x57,0x4D,0x4F,0x44,0x45,0x3D,0x32,0x0D,0x0A};//AT+CWMODE=2 Установить режим маршрутизации
unsigned char SZLY[38]={0x41,0x54,0x2B,0x43,0x57,0x53,0x41,0x50,0x3D,0x22,0x45,0x53,0x50,0x38,0x32,0x36,0x36,0x22,0x2C,0x22,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x22,0x2C,0x31,0x31,0x2C,0x33,0x0D,0x0A};//AT+CWSAP="ESP8266","0123456789",11,0 установки маршрутизации
unsigned char RST[8]={0x41,0x54,0x2B,0x52,0x53,0x54,0x0D,0x0A};//AT+RST перезапуск
unsigned char SZDLJ[13]={0x41,0x54,0x2B,0x43,0x49,0x50,0x4D,0x55,0x58,0x3D,0x31,0x0D,0x0A};//AT+CIPMUX=1 установка нескольких соединений
unsigned char KQFU[21]={0x41,0x54,0x2B,0x43,0x49,0x50,0x53,0x45,0x52,0x56,0x45,0x52,0x3D,0x31,0x2C,0x35,0x30,0x30,0x30,0x0D,0x0A};//AT+CIPSERVER=1,5000 Открытый порты TCP сервиса
unsigned char FSSJ[11]={0x41,0x54,0x2B,0x43,0x49,0x50,0x53,0x45,0x4E,0x44,0x3D};//AT+CIPSEND= передать данные
unsigned char WDSJ[4]={0x00,0x00,0x00,0x00};
void Delay1(unsigned long ms)
{
	unsigned char i, j,k;
	for(k=0;k<ms;k++)
	{
		_nop_();
		_nop_();
		i = 22;
		j = 128;
		do
		{
			while (--j);
		} while (--i);
	}
}
void Delay2(unsigned long cnt)
{
	long i;
 	for(i=0;i<cnt*100;i++);
}
void Delay3(unsigned int N) 
{
	int i;
	for(i=0;i<N*10;i++);
}
//--------------------
// Название функции: Сброс 
// Входные параметры: Нет
// Возвращает deceive_ready
// Функция: Сброс
//--------------------
unsigned char Reset(void) 
{
	unsigned char deceive_ready;
	Delay3(10); 
	DQ=0;                               //Значение на DQ линии вниз
	Delay3(29);                          //Задержка по крайней мере 480us~960 us
	DQ=1;                               //Установка бита в высокое логического значение в DQ линии  
	Delay3(3);                           //Задержка ожидания ответа deceive_ready
	deceive_ready=DQ;                   //Сигнал выборки deceive_ready
	Delay3(25);                          //Ожидание переднего фронта
	return(deceive_ready);              //Возвращает 0 или deceive_ready сигнал, в противном случае
}


//---------------------------
// Название функции: read_bit
// Входные параметры: Нет
// Возврат полученных данных
// Функция: подпрограмма чтения бита
//---------------------------
unsigned char read_bit(void)
{
	unsigned char i;
	DQ=0;                                 //Значение на DQ линии вниз
	DQ=1;                                 //Установка бита в высокое логического значение в DQ линии                        
	for(i=0;i<3;i++);         //Сначала 15 us задержки времени
	return(DQ);                           //Вернуть значение DQ
}


//---------------------------
// Название функции: write_bit
// Входные параметры: bitval
// Функция: Программа записи бита 
//---------------------------
void write_bit(unsigned char bitval)
{
	DQ=0;                             //Значение на DQ линии вниз
	if(bitval==1)
		DQ=1;                             //Если вы пишете высокое логическое значение (1)
	Delay3(5);                         //Задержка 
	DQ=1;                             //Увеличение DQ линия 
}


//----------------------------
// Название функции: write_byte
// Входные параметры: Val
// Функция: Программа записи байта 
//----------------------------
void write_byte(unsigned char val)
{
	unsigned char i,temp;
	for(i=0;i<8;i++)
	{
		temp=val>>i;                        //Значащий бит сдвигается вправо на номер позиции и помещается в переменную
		temp=temp&0x01;                     //Берём из temp только младший бит
		write_bit(temp);
		Delay3(5);                           //Задержка времени до окончания записи
	}
}


//----------------------------
// Название функции: read_byte
// Вернуть полученное значение данных
// Функция: программа чтения байта
//----------------------------
unsigned char read_byte(void)
{
	unsigned char i,m,receive_data;
	m=1;receive_data=0;                          //инициализация
	for(i=0;i<8;i++)
	{
		if(read_bit()) 
		{
			receive_data=receive_data+(m<<i);
		}                                          //Каждый прочитанный сдвигаем влево на его позицию и суммируем 
		Delay3(6);                                  //Задержка времени окончания чтения
	} 
	return(receive_data);                        //Возвращаем значение
}


//---------------------------
// Название функции: Get_temp
// Возвращаем полученную Tmpl данных, TMPH
// Функция: Удалить значение температуры
//---------------------------
uint Get_temp(void)
{
	unsigned int T;
	//EA = 0;
	Reset();
	write_byte(jump_ROM);       //Команда перехода на ROM
	write_byte(start);          //Команда запуска процесса
	Reset();
	write_byte(jump_ROM);       //Команда перехода на ROM
	write_byte(read_EEROM);     //Команда перехода на ROM
	TMPL=read_byte();           //Читать младшие восемь бит температуры
	TMPH=read_byte();           //Читать старшие восемь бит температуры
	
	//EA = 0;
	T=TMPL+TMPH*256;     
	if(T==0xFFFF) return 0xFFFF;
	if(T>0x8000)   //Температура отрицательная
	{
		T=~T+1;
		return(0x8000+T*5/8);
	}
	else     //Положительное значение температуры
	{
		return(T*5/8);
	}
}
void UARTSendByte(unsigned char byte)//?????????
{
	SBUF=byte;              //???????????
	while(TI==0);          //??????,TI?????1
	TI=0;                    //?????????
}
void DisplayTemp(unsigned int temp)
{
	uchar i=0;
	uchar TmpTable[3] = {0};
	TmpTable[ 0 ] = (temp%1000)/100;
	TmpTable[ 1 ] = (temp%100)/10;
	TmpTable[ 2 ] = (temp%10);

//	UARTSendByte(TmpTable[0] + 0x30);Delay3(9);
//	UARTSendByte(TmpTable[1] + 0x30);Delay3(9);
//	UARTSendByte('.');Delay3(9);
//	UARTSendByte(TmpTable[2] + 0x30);Delay3(9);	
//	UARTSendByte(0x0d);Delay3(9);	
//	UARTSendByte(0x0a);Delay3(9);
	WDSJ[0]=(TmpTable[0] + 0x30);
	WDSJ[1]=(TmpTable[1] + 0x30);
	WDSJ[2]=('.');
	WDSJ[3]=(TmpTable[2] + 0x30);	
}
void main(void)
{
	char i=0;
	B_TX1_Busy = 0;
	RX1_Cnt = 0;
	TX1_Cnt = 0;
	S1_8bit();				//8 бит данных
	S1_USE_P30P31();		//UART1 использует порт по умолчанию P30 P31 
	AUXR &= ~(1<<4);	//Timer stop		Скорость передачи данных генерируется с использованием Тimer2
	AUXR |= 0x01;		//S1 BRT Use Timer2;
	AUXR |=  (1<<2);	//Timer2 set as 1T mode
	TH2 = (u8)(Timer2_Reload >> 8);
	TL2 = (u8)Timer2_Reload;
	AUXR |=  (1<<4);	//Timer run enable
	REN = 1;	//Разрешено получать
	ES  = 1;	//Разрешить прерывание
	EA = 1;		//Включить глобальные прерывания
	P3M1 = 0x00;
  P3M0 = 0xFF;
	RX1_Cnt=0;
	DK1=0;
	BEEP=0;
	Delay2(1000);
	for(i=0;i<13;i++)//AT+CWMODE=2 Установить режим маршрутизации
	{
		SBUF=LYMS[i];Delay1(1);
	}
	Delay2(1000);
	for(i=0;i<38;i++)//AT+CWSAP="ESP8266","0123456789",11,0 Установка маршрутизации
	{
		SBUF=SZLY[i];Delay1(1);
	}
	Delay2(1000);
	for(i=0;i<8;i++)//AT+RST перезапуск
	{
		SBUF=RST[i];Delay1(1);
	}
	Delay2(5000);
	for(i=0;i<13;i++)//AT+CIPMUX=1 Установка нескольких соединений
	{
		SBUF=SZDLJ[i];Delay1(1);
	}
	Delay2(2000);
	for(i=0;i<21;i++)//AT+CIPSERVER=1,5000 Открыть порт TCP сервиса
	{
		SBUF=KQFU[i];Delay1(1);
	}
	Delay2(2000);
	Get_temp();
	Delay3(50000);
	Get_temp();
	Delay3(50000);
	while (1)
	{
		DisplayTemp(Get_temp());//Обновить 18B20
		if(K1==0)
		{
			Delay1(1);
			if(K1==0)
			{
				LED1=!LED1;
			}
			while(K1==0);
		}
		if(K2==0)
		{
			Delay1(1);
			if(K2==0)
			{
				LED2=!LED2;
			}
			while(K2==0);
		}
		if(K3==0)
		{
			Delay1(1);
			if(K3==0)
			{
				LED3=!LED3;
			}
			while(K3==0);
		}
	}
}

/********************* UART1 Прерывание функции ************************/
void UART1_int (void) interrupt UART1_VECTOR
{
	char i,a,b,c;
	if(RI)
	{
		RI = 0;
		RX1_Buffer[RX1_Cnt] = SBUF;		//Сохранить байт
		if(RX1_Buffer[0]==0x45)
		{
			RX1_Cnt++;
		}
		else
		{
			RX1_Cnt=0;
		}
		if(RX1_Cnt>=10)
		{
			if(RX1_Buffer[0]==0x45&&RX1_Buffer[1]==0x53&&RX1_Buffer[2]==0x50)
			{
				if(RX1_Buffer[4]==0x4C&&RX1_Buffer[5]==0x45&&RX1_Buffer[6]==0x44)//Арбитражный LED
				{
					if(RX1_Buffer[7]==0x31)//Арбитражный LED1
					{
						if(RX1_Buffer[3]==0x4B)//Анализируя on (открыт)
						{
							LED1=0;
						}
						if(RX1_Buffer[3]==0x47)//Анализируя Off (закрыт)
						{
							LED1=1;
						}
					}
					if(RX1_Buffer[7]==0x32)//Арбитражный LED2
					{
						if(RX1_Buffer[3]==0x4B)//Анализируя on (открыт)
						{
							LED2=0;
						}
						if(RX1_Buffer[3]==0x47)//Анализируя Off (закрыт)
						{
							LED2=1;
						}
					}
					if(RX1_Buffer[7]==0x33)//Арбитражный LED3
					{
						if(RX1_Buffer[3]==0x4B)//Анализируя on (открыт)
						{
							LED3=0;
						}
						if(RX1_Buffer[3]==0x47)//Анализируя Off (закрыт)
						{
							LED3=1;
						}
					}
				}
				if(RX1_Buffer[4]==0x4A&&RX1_Buffer[5]==0x44&&RX1_Buffer[6]==0x51)//Проверка реле
				{
					if(RX1_Buffer[7]==0x31)//Арбитражный LED1
					{
						if(RX1_Buffer[3]==0x4B)//Анализируя on (открыт)
						{
							DK1=1;
						}
						if(RX1_Buffer[3]==0x47)//Анализируя Off (закрыт)
						{
							DK1=0;
						}
					}
				}
				if(RX1_Buffer[3]==0x46&&RX1_Buffer[4]==0x4D&&RX1_Buffer[5]==0x51&&RX1_Buffer[6]==0x43&&RX1_Buffer[7]==0x53)//Проверка зумера
				{
					BEEP=1;Delay2(100);BEEP=0;Delay2(100);BEEP=1;Delay2(100);BEEP=0;Delay2(100);
				}
				if(RX1_Buffer[3]==0x43&&RX1_Buffer[4]==0x58&&RX1_Buffer[5]==0x53&&RX1_Buffer[6]==0x4A)//Запрос данных
				{
					if(LED1==0){a=0x4B;}else{a=0x47;}
					if(LED2==0){b=0x4B;}else{b=0x47;}
					if(LED3==0){c=0x4B;}else{c=0x47;}
					for(i=0;i<11;i++)//AT+CIPSEND= передавать данные
					{
						SBUF=FSSJ[i];Delay1(1);
					}
					SBUF=0x30;Delay1(1);
					SBUF=0x2C;Delay1(1);
					SBUF=0x32;Delay1(1);
					SBUF=0x36;Delay1(1);
					SBUF=0x0D;Delay1(1);
					SBUF=0x0A;Delay1(1);
					
					SBUF=0x45;Delay1(1);
					SBUF=0x53;Delay1(1);
					SBUF=0x50;Delay1(1);
					SBUF=0x4C;Delay1(1);
					SBUF=0x45;Delay1(1);
					SBUF=0x44;Delay1(1);
					SBUF=0x31;Delay1(1);
					SBUF=a;Delay1(1);
					SBUF=0x4C;Delay1(1);
					SBUF=0x45;Delay1(1);
					SBUF=0x44;Delay1(1);
					SBUF=0x32;Delay1(1);
					SBUF=b;Delay1(1);
					SBUF=0x4C;Delay1(1);
					SBUF=0x45;Delay1(1);
					SBUF=0x44;Delay1(1);
					SBUF=0x33;Delay1(1);
					SBUF=c;Delay1(1);
					SBUF=WDSJ[0];Delay1(1);
					SBUF=WDSJ[1];Delay1(1);
					SBUF=WDSJ[2];Delay1(1);
					SBUF=WDSJ[3];Delay1(1);
					SBUF=0x50;Delay1(1);
					SBUF=0x53;Delay1(1);
					SBUF=0x45;Delay1(1);
					//45 53 50 4C 45 44 31 4B 4C 45 44 32 4B 4C 45 44 33 4B 50 53 45 
				}
				RX1_Cnt=0;
			}
			else
			{
				RX1_Cnt=0;
			}
			RX1_Cnt=0;
		}
	}
	if(TI)
	{
		TI = 0;
		B_TX1_Busy = 0;		//Готов к передаче - флаг занятости
	}
}
