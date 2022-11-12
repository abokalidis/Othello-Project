/* 2o_milestone--PROJECT--OTHELLO-- 
 * 
 * ANASTASIOS BOKALIDIS 2014030069
 * MANOLIS KADITIS      2014030136
 * 
 * LAB41140623
 */ 
#include <math.h>
#include <avr/io.h>
#include <string.h>
#include <stdio.h>

#include <util/atomic.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define baud 2400
#define F_CPU 1000000UL
#define BAUD_RATE (((F_CPU / (baud * 16UL))) - 1)
#include <util/delay.h>
#define LED1 PORTB=0b11111110
#define LED2 PORTB=0b11111101
#define LED3 PORTB=0b11111011
#define LED_OFF PORTB=0b11111111
#define TRUE 1
#define FALSE 0

const int ALLDIRECTIONS[8]={-9, -8, -7, -1, 1, 7, 8, 9};
const int BOARDSIZE=64;

/* Each array/board position can have one of 3 values */
const int EMPTY=0;
const int BLACK=1;
const int WHITE=2; 

volatile int MyColour=0;
volatile int OpponentColour=0;
volatile int NumOfLegalMoves=0;
/*    FLAGS    */
volatile int TurnFlag=0;
volatile int WinFlag=0;
volatile int TieFlag=0;
volatile int LooseFlag = 0;
volatile int PassFlagPC = 0;
volatile int PassFlagAVR = 0;
volatile int BOARD[64];
volatile int ovf_count=0;
volatile int TimerValue = 2;
volatile unsigned char data_in[12];
volatile unsigned char command_in[12];
volatile unsigned data_count=0;
volatile unsigned char command_ready;
volatile int k=0,l=0; /**/

void USART_Transmit(unsigned char data)
{
	// Wait for empty transmit buffer
	while (!( UCSRA & (1<<UDRE)));

	// Put data into buffer, sends the data
	UDR = data;
} 

char USART_Receive( void )
{
	/* Wait for data to be received */
	while ( !(UCSRA & (1<<RXC)) );
	/* Get and return received data from buffer */
	return UDR;
}

//we send a whole string 

void usart_msg(char *p) 
{
	while(*p != 0)
	{
		USART_Transmit(*p++);
	}
}

//we check if a char is number

int isNumber(unsigned char temp)  
{ 
	if (temp <= '9' && temp >= '0')
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

//we check if we received position A-H

int isA_H(unsigned char temp)  
{
	if (temp >= 'A' && temp <= 'H' )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void copy_command ()  //copy received data to command in so as to make our computes without loosing any info
{
	// The USART might interrupt this - don't let that happen!
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		// Copy the contents of data_in into command_in
		memcpy(command_in, data_in, 12);

		// Now clear data_in, the USART can reuse it now
		memset(data_in, 0, 12);
	}
}

void init_USART() //initialize USART communication
{
	//enable Transmitter and Receiver
	UCSRB=(1<<TXEN) | (1 << RXEN) | (1 << RXCIE);
	//control UCSRC -> URSEL=1
	//asynchronous operation -> UMSEL = 0
	//no parity -> UPM1:0 = 00
	//stop bit 1 -> USBS = 0
	//character size 8bit -> UCSZ2:0 = 011
	UCSRC=(1<<URSEL) | (1<<UCSZ0) | (1<<UCSZ1) ;
	//set up baud rate
	UBRRH = (unsigned char)(BAUD_RATE>>8);
	UBRRL = (unsigned char)BAUD_RATE;
	//sei();
}

ISR(TIMER0_OVF_vect)  //interrupt of timer when it is overflowed
{
	ovf_count++; //keep the number of ovfs
}

void timer0_init()  //initialize timer 8 bit
{
	TCCR0 |= (1 << CS02) | (1 << CS00); // set prescaler to 1024 and start the timer
	TIMSK |= (1 << TOIE0); //enable ovf interrupts
	sei(); //enable global interrupts
	ovf_count = 0; //count ovfs
	TCNT0 = 0; //set 0 on Timer0
	PORTB = 0b11111111;
}

void run_timer()  //run timer
{ 
	    timer0_init();
		if(ovf_count == 2 && TCNT0 == 74)
		{
			PORTB = ~(0b10000000);	// Turn ON LEDs
		}
		if(ovf_count == 4 && TCNT0 == 246)
		{
			PORTB = ~(0b01000000);	// Turn ON LEDs
		}
		if(ovf_count >= 7 && TCNT0 >= 162) //check when we will have 7 ovfs
		{
			PORTB = ~(0b00100000);	// Turn ON LEDs
			_delay_ms(600);
			//timer0_init();
		}
}

/*
Translate letters to numbers: 
A...0
B...1
C...2
D...3
.
.
.
H...7
*/
int opponent (int player) {
	switch (player) {
		case 1: return 2;
		case 2: return 1;
		default: return 0;
	}
}

int validp (int move) {
	if ((move >= 0) && (move <= 63))
	return 1;
	else return 0;
}

int findbracketingpiece(int square, int player, int dir) 
{
	while (BOARD[square] == opponent(player)) square = square + dir;
	if (BOARD[square] == player) return square;
	else return 0;
}

int wouldflip (int move, int player, int dir) 
{
	int c;
	c = move + dir;
	if (BOARD[c] == opponent(player)){
	return findbracketingpiece(c+dir, player, dir);
	}
	else return 0;
}

int legalp (int move, int player) 
{
	int i;
	if (!validp(move)) return 0;
	if (BOARD[move]==EMPTY) {
		i=0;
		while (i<=7 && !wouldflip(move, player, ALLDIRECTIONS[i])) i++;
		if (i==8) return 0; else return 1;
	}
	else return 0;
}

void makeflips (int move, int player, int dir) 
{
	int bracketer, c;
	bracketer = wouldflip(move, player, dir);
	if (bracketer) {
		c = move + dir;
		do {
			BOARD[c] = player;
			c = c + dir;
		} while (c != bracketer);
	}
}

void makemove (int move, int player) 
{
	int i;
	BOARD[move] = player;
	for (i=0; i<=7; i++) makeflips(move, player, ALLDIRECTIONS[i]);
}

int anylegalmove (int player) {
	int move;
	move = 0;
	while (move <= 63 && !legalp(move, player)) move++;
	if (move <= 63) return 1; else return 0;
}

int  legalmoves (int player) 
{
	int move, i,r;
	int moves[64];
	
	i = 0;
	for (move=0; move<=63; move++){
		if (legalp(move, player)) 
		{
			i++;
			moves[i]=move;
		}
	}
	moves[0]=i;
	r = moves[(rand() % moves[0]) + 1];
	return r;
}

int returnposition(unsigned char x,unsigned char y)
{
	int z,w,p;
	
	w = y - '0';
	z = (x - 'A') * 8;
	p=(z+w)-1;
	return p;	
}

void printposition(int x)
{
	int row,col;
	
	row = (x / 8) ;
	col = (x % 8) + 1;
	
 	USART_Transmit('M');
 	USART_Transmit('M');
    USART_Transmit(' ');
 	USART_Transmit(row+'A');
    USART_Transmit(col+'0');
	USART_Transmit('\r');		
}

//initialize the BOARD

void initialboard () 
{
	int i;
	
	for(i=0;i<64;i++)
	{
			BOARD[i]=EMPTY;
	}
	BOARD[27]=WHITE;
	BOARD[28]=BLACK;
	BOARD[36]=WHITE;	
	BOARD[35]=BLACK;
}

//Printing the board

void print_board(){
	
	int i,j;
	 	for(i=0;i<8;i++)
	 	{
			 for (j=0;j<8;j++)
			 {
				 USART_Transmit(BOARD[(8*i)+j] +'0');
			 }
			USART_Transmit('\n');
	 	}
}

int count (int player) {
	int i, cnt;
	cnt=0;
	for (i=0; i<=63; i++)
	if (BOARD[i] == player) cnt++;
	return cnt;
}

void CountAndFinish()
{
	int x,y;
	
	x=count(MyColour);
	y=count(OpponentColour);
	
	if(x>y)
	{
		WinFlag = 1;
	}else if(x<y)
	{
		LooseFlag = 1;
	}else {TieFlag = 1;}
}

void command_menu() //commands and their operations 
{	
	int m,leg;
	switch(command_in[0]) {
		case 'A': //AT
		    if(command_in[1] == 'T' && command_in[2] == 13)
		    {
			   usart_msg("OK\r");
			}			
			break;
		case 'R': //RST
			if(command_in[1] == 'S' && command_in[2] == 'T' && command_in[3] == 13)   //MW X Y
			{
					initialboard();
					usart_msg("OK\r");
			}
			break;
		case 'S':  //SP && ST
			if(command_in[1] == 'P' && command_in[2] == 32 && (command_in[3] == 'B' || command_in[3] == 'W') && command_in[4] == 13)  
					{
						if(command_in[3]== 'W')
						{
							MyColour = BLACK;
							OpponentColour = WHITE;
							USART_Transmit('\r');
							
							m = legalmoves(MyColour);
							makemove(m,MyColour);
							printposition(m);				
														
						}else if (command_in[3]== 'B')
						{
							MyColour = WHITE;
							OpponentColour = BLACK;
						}
						                   
					}
			else if(command_in[1] == 'T' && command_in[2] == 32 && isNumber(command_in[3]) == 1 && command_in[4] == 13)
		           {					
					   TimerValue = command_in[3] - '0';   
					   usart_msg("OK\r");
				   }
			break;
		case 'N':  //NewGame
		    if(command_in[1] == 'G' && command_in[2] == 13)
			{
				usart_msg("OK\r");
				initialboard();
				MyColour = 0;
				OpponentColour =0;
			}
			break;
		case 'E':  //EndGame
		    if(command_in[1] == 'G' && command_in[2] == 13)
		    {
			    usart_msg("OK\r");
				CountAndFinish();
		    }
			break;
		case 'M': //MV
		    if(command_in[1] == 'V' && command_in[2] == 32 && isA_H(command_in[3]) == 1 
			   && isNumber(command_in[4]) == 1 && command_in[5] == 13)
			  {
				  int n=returnposition(command_in[3],command_in[4]);
				  int k=anylegalmove(OpponentColour);
				  if(legalp(n,OpponentColour) == 1 && k == 1)
				  {
					   makemove(n,OpponentColour);
					   usart_msg("OK\r");
					   TurnFlag = 1;
				  }else {usart_msg("IL\r"); WinFlag = 1;}
			  }
			  break;
		case 'W': //WN
		     if(command_in[1] == 'N' && command_in[2] == 13)
		     {
				 WinFlag = 1;
			     usart_msg("OK\r");
		     }
		     break;
		case 'P': //PS
			 if(command_in[1] == 'S' && command_in[2] == 13)
			 {
				 usart_msg("OK\r");
			 }
		     break;
		case 'O':
		  	 if(command_in[1] == 'K' && command_in[2] == 13 && TurnFlag == 1)
		  	 {
					   TurnFlag = 0;
					   leg = anylegalmove(MyColour);
					   if(leg == 1)
					   {
						    m = legalmoves(MyColour);
						    makemove(m,MyColour);
						    printposition(m);
					   }else {usart_msg("MP\r");}
					  			  	 			 
		  	 }	
			 break;			
		default:
			break;
		}
	
		if(WinFlag == 1)
		{
			usart_msg("WN\r");
			LED1;
			_delay_ms(1000);
			WinFlag=0;
		}
		
		if(LooseFlag == 1)
		{
			usart_msg("LS\r");
			LED2;
			_delay_ms(1000);
			LooseFlag=0;
		}
		
		if(TieFlag == 1)
		{
			usart_msg("TE\r");
			LED3;
			_delay_ms(1000);
			TieFlag=0;
		}
} 
					
ISR(USART_RXC_vect)  //interrupt from RX 
{
	// Get data from the USART in register
	data_in[data_count] = UDR;

	// End of line!
	if (data_in[data_count] == '\n' && data_in[data_count-1] == '\r') {
		command_ready = TRUE;
		// Reset to 0, ready to go again
		data_count = 0;
		} else {
		data_count++;
	}
	sei();
}

int main(void)
{
	DDRB = 0b11111111;		// Configure port B as output
	PORTB = 0xFF;	        // Turn OFF LEDs
	init_USART();           //Initialization of USART
	//timer0_init(); 
	sei();
	while(1)
	{
// 		ATOMIC_BLOCK(ATOMIC_FORCEON)
// 		{
// 			run_timer();
// 		}
		if (command_ready == TRUE) 
		{
			copy_command();
			command_menu();
			command_ready = FALSE;
		}
	}
	
}