/*
* AquariumControl.c
*
* Created: 20.07.2015 23:25:25
*  Author: Михаил
*/


#define F_CPU 1000000L
#include <avr/io.h>
#include <util/delay.h>
#include "..\..\Lib\USI_TWI_Master\USI_TWI_Master.h"


#define RS PORTD5
#define EN PORTD6
#define DS1307W 0xD0
#define DS1307R 0xD1
#define BUTTON1 0
#define BUTTON2 1
#define BUTTON3 2

#define ALARM_ON PORTB |= _BV(PORTB4)
#define ALARM_OFF PORTB &= ~_BV(PORTB4)

unsigned char mode = 0;//Режим работы устройства 0 - отображение часов, 1-6 настройка
unsigned char buttonsStatus = 0;

// Функция записи команды в ЖКИ
void lcd_com(unsigned char p)
{
	PORTD &= ~(1 << RS); // RS = 0 (запись команд)
	PORTD |= (1 << EN);  // EN = 1 (начало записи команды в LCD)
	PORTB &= 0xF0; PORTB |= (p >> 4); // Выделяем старший нибл
	_delay_us(100);
	PORTD &= ~(1 << EN); // EN = 0 (конец записи команды в LCD)
	_delay_us(100);
	PORTD |= (1 << EN); // EN = 1 (начало записи команды в LCD)
	PORTB &= 0xF0; PORTB |= (p & 0x0F); // Выделяем младший нибл
	_delay_us(100);
	PORTD &= ~(1 << EN); // EN = 0 (конец записи команды в LCD)
	_delay_us(100);
}

// Функция записи данных в ЖКИ
void lcd_dat(unsigned char p)
{
	PORTD |= (1 << RS)|(1 << EN); // RS = 1 (запись данных), EN - 1 (начало записи команды в LCD)
	PORTB &= 0xF0; PORTB |= (p >> 4); // Выделяем старший нибл
	_delay_us(100);
	PORTD &= ~(1 << EN); // EN = 0 (конец записи команды в LCD)
	_delay_us(100);
	PORTD |= (1 << EN); // EN = 1 (начало записи команды в LCD)
	PORTB &= 0xF0; PORTB |= (p & 0x0F); // Выделяем младший нибл
	_delay_us(100);
	PORTD &= ~(1 << EN); // EN = 0 (конец записи команды в LCD)
	_delay_us(100);
}

//Вывод строки на дисплей, выводится str и дополняется пробелами до полной строки
void lcd_line(unsigned char* str, unsigned char len){
	for (unsigned char i=0; i<8; i++)
	{
		unsigned char simvol;
		if (i < len){
			simvol = str[i];
		}
		else
		{
			simvol = ' ';
		}
		lcd_dat(simvol);
	}
}

void lcd_send(){
	PORTD |= (1 << EN);
	_delay_us(2);
	PORTD &= ~(1 << EN);
	_delay_us(40);
}

// Функция инициализации ЖКИ
void lcd_init(void)
{
	DDRD |= _BV(PORTD5)|_BV(PORTD6); // PD5, PD6 - выходы
	PORTD &= ~(_BV(PORTD5)|_BV(PORTD6)); //На выходах RS и E - 0
	DDRB |= _BV(PORTB0)|_BV(PORTB1)|_BV(PORTB2)|_BV(PORTB3); // порт B(0-4) - выход
	PORTB &= ~(_BV(PORTB0)|_BV(PORTB1)|_BV(PORTB2)|_BV(PORTB3)); //На выходах PB(0-4) - 0
	
	//Инициализация по datasheet
	_delay_ms(40); // Ожидание готовности ЖК-модуля
	PORTB |= (1 << PORTB1) | (1 << PORTB0); //8 байтовый режим
	lcd_send();
	
	_delay_ms(5);
	lcd_send();
	
	_delay_us(100);
	lcd_send();

	// Конфигурирование четырехразрядного режима
	PORTB &= ~(1 << PORTB0);
	lcd_send();
	_delay_ms(5);
	
	lcd_com(0x28); // Шина 4 бит, LCD - 2 строки
	lcd_com(0x08); // Полное выключение дисплея
	lcd_com(0x01); // Очистка дисплея
	_delay_us(100);
	lcd_com(0x06); // Сдвиг курсора вправо
	_delay_ms(10);
	lcd_com(0x0C); // Включение дисплея, курсор не видим
}

//Вывод сообщения об ошибке
void showErr(unsigned char err){
	lcd_com(0xC0);
	lcd_dat('E');
	lcd_dat('R');
	lcd_dat('R');
	lcd_dat(' ');
	lcd_dat(err);
	_delay_ms(1000);
	lcd_com(0x01);//Очистка
}

//Вызов библиотечной функции USI_TWI_Start_Transceiver_With_Data и проверка результата. В случае ошибки вывод ее на дисплей
void local_USI_TWI_Start_Transceiver_With_Data(unsigned char *msg, unsigned char msgSize){
	if (!USI_TWI_Start_Transceiver_With_Data(msg, msgSize)){ //УстановкаСчетчик регистра в секунды
		showErr(USI_TWI_Get_State_Info() + '0');
	}
}

//Инициализация DS1307, проверка запущен ли хронометр, установка 24 часового режима и SQWE бита
void initTimeChip(){
	unsigned char buf[14];
	//Установка указателя регистра в 0
	buf[0] = DS1307W;
	buf[1] = 0x00;
	local_USI_TWI_Start_Transceiver_With_Data(buf, 2);
	
	//Загрузка регистров с 0x00 по 0x0B
	buf[0] = DS1307R;
	local_USI_TWI_Start_Transceiver_With_Data(buf, 13);
	
	//Проверка седьмого бита нулевого регистра
	if (buf[1] & _BV(7))
	{
		//Полная инициализация DS1307
		buf[0] = DS1307W;
		buf[1] = 0x00;
		buf[2] = 0x00;  //Запуск часов
		buf[3] = 0x00;  //00 минут
		buf[4] = 0x00;	//00 часов, 24 часовой формат
		buf[5] = 0x00;
		buf[6] = 0x00;
		buf[7] = 0x00;
		buf[8] = 0x00;
		buf[9] = _BV(4); //SQWE = 1, C вывода out снимаем секундный тактовый импульс
		buf[10] = 0x00;
		buf[11] = 0x00;
		buf[12] = 0x00;
		buf[13] = 0x00;
		local_USI_TWI_Start_Transceiver_With_Data(buf, 14);
	}
	else
	{
		//Проверка надо ли включить сейчас нагрузку
		//Перевод текущего времени, времени включения и выключения в int, чтобы удобнее сравнивать
		unsigned int currentTime = buf[3];
		unsigned int onTime = buf[10];
		unsigned int offTime = buf[12];

		currentTime = (currentTime << 8) | buf[2];
		onTime = (onTime << 8) | buf[9];
		offTime = (offTime << 8) | buf[11];
		
		//Анализируем время чтобы понять сейчас включить или выключить нагрузку
		if (onTime < offTime)
		{
			if (onTime <= currentTime && currentTime < offTime)
			{
				ALARM_ON;
			}
			else
			{
				ALARM_OFF;
			}
		}
		else
		{
			if (offTime <= currentTime && currentTime < onTime)
			{
				ALARM_OFF;
			}
			else
			{
				ALARM_ON;
			}
		}
		
		//Частичная инициализация - пробиваем SQWE бит, вдруг он не стоял по каким то причинам
		buf[0] = DS1307W;
		buf[1] = 0x07;
		buf[2] = _BV(4); //SQWE = 1, C вывода out снимаем секундный тактовый импульс
		local_USI_TWI_Start_Transceiver_With_Data(buf, 3);
	}
}

void initIO(void){
	DDRD &= ~(_BV(PIND0) | _BV(PIND1) | _BV(PIND2));//PD0-PD2 входы
	PORTD |=_BV(PORTD0) | _BV(PORTD1) | _BV(PORTD2);   //Подтягиваем пины PD0-PD2 к питанию
	DDRB |= _BV(PORTB4); //Пин PB4 на выход
	PORTB &= ~(_BV(PORTB4));//Нагрузка выключена
}

//Отображение содержимого регистра
void lcd_reg(unsigned char addr){
	unsigned char buf[2];
	lcd_com(0xC0); //Курсор в начало второй строки
	buf[0] = DS1307W;
	buf[1] = addr;
	local_USI_TWI_Start_Transceiver_With_Data(buf, 2); //УстановкаСчетчик регистра в addr
	
	buf[0] = DS1307R;
	USI_TWI_Start_Transceiver_With_Data(buf, 2); //Зачитываем 3 первых регистра
	
	lcd_dat(((buf[1] & 0xF0) >> 4) + '0'); // Выделяем десятки из регистра
	lcd_dat((buf[1] & 0x0F) + '0'); // Выделяем единицы из регистра
}

unsigned char to10base(unsigned char op){
	return ((op & 0xF0) >> 4) * 10 + (op & 0x0F);
}

unsigned char to2base(unsigned char op){
	return ((op / 10) << 4) | (op % 10);
}


void regUp(int inc){
	unsigned char buf[3];
	buf[0] = DS1307W;
	unsigned char addr;
	unsigned char val;
	unsigned char hour;
	hour = 0;
	switch (mode){
		case 1:
		addr = 0x02;
		hour = 1;
		break;
		case 2:
		addr = 0x01;
		break;
		case 3:
		addr = 0x09;
		hour = 1;
		break;
		case 4:
		addr = 0x08;
		break;
		case 5:
		addr = 0x0B;
		hour = 1;
		break;
		case 6:
		addr = 0x0A;
		break;
	}
	//Позиционируем
	buf[1] = addr;
	USI_TWI_Start_Transceiver_With_Data(buf, 2);
	//Зачитываем
	buf[0] = DS1307R;
	USI_TWI_Start_Transceiver_With_Data(buf, 2);
	//Инкрементируем
	val = buf[1];
	val = to10base(val);

	if (hour && (inc > 0) && (val == 23))
	{
		val = 0;
	}
	else if (hour && (inc < 0) && (val==0))
	{
		val = 23;
	}
	else if (!hour && (inc > 0) && (val == 59))
	{
		val = 0;
	}
	else if (!hour && (inc < 0) && (val==0))
	{
		val = 59;
	}
	else
	{
		val = val + inc;
	}
	
	val = to2base(val);
	//Записываем
	buf[0] = DS1307W;
	buf[1] = addr;
	buf[2] = val;
	USI_TWI_Start_Transceiver_With_Data(buf, 3);
}

void chekButtons(void){
	if (!(PIND & _BV(PIND0)))
	{
		_delay_ms (50);
		if ((!(PIND & _BV(PIND0))) && !(buttonsStatus & _BV(BUTTON1)))
		{
			buttonsStatus |= _BV(BUTTON1);
			// Нажата кнопка 1, перевод часов в режим редактирования или обратно
			// 0-работа часов
			// 1-коррекция часов
			// 2 коррекция минут
			// 3 коррекция часов включения нагрузки
			// 4 коррекция минут включения нагрузки
			// 5 коррекция часов выключения нагрузки
			// 6 коррекция минут выключения нагрузки
			if (mode == 6)
			{
				mode = 0;
				}else{
				mode += 1;
			}
		}
	}
	else
	{
		if (buttonsStatus & _BV(BUTTON1))
		{
			_delay_ms (50);
			buttonsStatus &= ~_BV(BUTTON1);
		}
	}
	
	if (!(PIND & _BV(PIND1))){
		_delay_ms (50);
		if ((!(PIND & _BV(PIND1))) && !(buttonsStatus & _BV(BUTTON2)))
		{
			buttonsStatus |= _BV(BUTTON2);
			regUp(1);
		}
	}
	else
	{
		if (buttonsStatus & _BV(BUTTON2))
		{
			buttonsStatus &= ~_BV(BUTTON2);
		}
	}

	if (!(PIND & _BV(PIND2)))
	{
		_delay_ms (50);
		if ((!(PIND & _BV(PIND2))) && !(buttonsStatus & _BV(BUTTON3)))
		{
			buttonsStatus |= _BV(BUTTON3);
			regUp(-1);
		}
	}
	else
	{
		if (buttonsStatus & _BV(BUTTON3))
		{
			buttonsStatus &= ~_BV(BUTTON3);
		}
	}

}

//Прверка времени включения и выключения
void checkAlarm(unsigned char* buf){
	//Если время включения и время выключения равны то нагрузка не активна никогда	
	if (buf[9] == buf[11] && buf[10] == buf[12] ) return;
	
	//Проверяем время включения
	if (buf[2] == buf[9] && buf[3] == buf[10]){
		ALARM_ON;
	}
	
	//Проверяем время выключения
	if (buf[2] == buf[11] && buf[3] == buf[12]){
		ALARM_OFF;
	}
}

// Основная программа
int main (void)
{
	initIO();

	lcd_init(); // Инициализация дисплея
	
	USI_TWI_Master_Initialise();
	
	initTimeChip();
	
	unsigned char sec = ':';
	
	unsigned char buf[14];
	
	while (1)
	{
		chekButtons();
		
		if (mode == 0){
			
			lcd_com(0x80);
			buf[0] = DS1307W;
			buf[1] = 0x00;
			local_USI_TWI_Start_Transceiver_With_Data(buf, 2); //УстановкаСчетчик регистра в секунды
			
			
			buf[0] = DS1307R;
			USI_TWI_Start_Transceiver_With_Data(buf, 13); //Зачитываем 12 первых регистров
			
			//Прверка подхода времени включения и выключения нагрузки
			checkAlarm(buf);
			
			lcd_com(0x80); // Вывод в верхнюю левую позицию 1 строки
			
			lcd_dat(((buf[3] & 0xF0) >> 4) + '0'); // Выделяем десятки часов
			lcd_dat((buf[3] & 0x0F) + '0'); // Выделяем единицы часов
			lcd_dat(sec);
			lcd_dat(((buf[2] & 0xF0) >> 4) + '0'); // Выделяем десятки минут
			lcd_dat((buf[2] & 0x0F) + '0'); // Выделяем единицы минут
			lcd_dat(sec);
			lcd_dat(((buf[1] & 0xF0) >> 4) + '0'); // Выделяем десятки секунд
			lcd_dat((buf[1] & 0x0F) + '0'); // Выделяем единицы секунд
			
			lcd_com(0xC0); //Курсор в начало второй строки
			lcd_line("", 0);
			
			_delay_ms(500); // Тут можно поменять задержку вывода символов
			
			//Мигающий разделитель
			if (sec == ':')
			{
				sec = ' ';
			}
			else
			{
				sec = ':';
			}
		}
		else
		{
			lcd_com(0x80);
			switch (mode)
			{
				case 1:
				lcd_line("HOUR", 4);
				lcd_reg(0x02);
				break;
				case 2:
				lcd_line("MIN", 3);
				lcd_reg(0x01);
				break;
				case 3:
				lcd_line("HOUR ON", 7);
				lcd_reg(0x09);
				break;
				case 4:
				lcd_line("MIN ON", 6);
				lcd_reg(0x08);
				break;
				case 5:
				lcd_line("HOUR OFF", 8);
				lcd_reg(0x0B);
				break;
				case 6:
				lcd_line("MIN OFF", 7);
				lcd_reg(0x0A);
				break;
			}
		}
	}
}
