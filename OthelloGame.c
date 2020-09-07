/*
 * Othello.c
 *
 * Created: 08-Apr-19 14:51:24
 * Author : Kwstis
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//number of rows and columns for the playable map
static int ROW=8,COLUMN=8;	
//Map of the game							
volatile unsigned char Map[8][8];	
//For ISR of transmit/receive			
volatile unsigned char buffer[15],temporary[15]; 
//values for keep pawns colour for players
volatile unsigned char playercolour='B',avrcolour='W';	
//values for AVR moves(letter for row,number for column)
unsigned char letter,number;							
//offset : for ISR of transmit/receive 
//Moves[20][2] : Keeps possible moves per turn for each player
volatile unsigned int offset = 0,Moves[20][2];	
/*ready : is a flag for each turn
          1 means that PC player send his command
		  0 means that AVR waits something from Keyboard 		  
*/	
//tiles : keeps pawns/(correct moves) for both players
//(black/white)counter : keep counter for each colour 	
volatile unsigned int ready = 0,tiles,blackcounter,whitecounter;
//values for time
volatile uint8_t othellotime=2,seconds=0;				
//tot_overflow : keeps overflows of timer
//flagok : If OK from PC means that AVR played
//flagdead : If OK from PC means that PC quit(after IL,IT) and AVR wins
//flagplay : If OK from PC means that AVR played without had MP
volatile uint8_t tot_overflow,flagok = 0,flagdead = 0,flagplay = 0;		

void USART_Init( void )
{
	UBRRH = 0;									// Set baud rate
	UBRRL = 25;
	
	UCSRB = (1<<RXEN)|(1<<TXEN)|(1<<RXCIE);		// Enable receiver and transmitter
	
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);	//// Set frame format: 8data, 1stop bit
	sei();
}

void TransmitData( unsigned char data )			//this function write only one byte
{
	while(!(UCSRA & (1<<UDRE)));				//Wait For Transmitter to become ready

	UDR=data;									////Now write
}

void print_error()
{
	TransmitData('E');
	TransmitData('R');
	TransmitData('\n');
}

void print_ok()
{
	TransmitData('O');
	TransmitData('K');
	TransmitData('\n');
}

ISR(USART_RXC_vect)
{
	temporary[offset] = UDR;
	
	if(temporary[offset] != 13) // in case we have a letter a number or anything else except carriage return we continue to receive data
	{
		offset = offset + 1;	// increase the index of the buffer to store properly the incoming string
	}
	else
	{
		offset = 0;				// we had carriage return so we set the index again in zero and enable a flag to confirm the end of the receiving string
		ready = 1;
		TCCR0 = (0<<CS00) | (0<<CS02);
	}
}


/*
	-----------------------------FUNCTION-----------------------------------------
	                  CheckingLegalMoves(unsigned char colour)
	Avr calls this function before his move or PC moves.With this function
	knows if the asking move from PC is correct.Also AVR calls the function for 
	himself in order to know the possible moves for his play. 
	----------------------------Parameters-----------------------------------------
	unsigned char colour:takes the colour of player for who will check the possible
	                     moves
    -----------------------------More informations---------------------------------
	The logic for checking is very easy.For the asked colour pawns function checks
	the whole map and finds every pawn with the same colour.When it finds pawn with 
	the same colour the checks every possible direction.Possible directions are:
	same column up,same column down
	same row right,same row left
	main diagonal down/up
	second diagonal down/up
	For each of these directions checks next pawn.If the next pawns have opposite 
	colour and a the end exist empty position put this empty position as a possible
	move.
*/

void CheckingLegalMoves(unsigned char colour)
{
	int i,j,z,y,a=0;
	unsigned char opcolour;
	
	for (i=0;i<20; i++)
	{
		for (j = 0; j<=1; j++)
		{
			Moves[i][j] = 8;
		}
	}
	
	if (colour == 'B')
	{
		opcolour = 'W';
	}else
	{
		opcolour = 'B';
	}
	
	blackcounter = 0;
	whitecounter = 0;
	
	//With two for loops program access whole Map
	
	for (i = 0; i<=ROW-1; i++)
	{
		for ( j = 0; j<=COLUMN-1; j++)
		{
			
			if ( Map[i][j] == 'B')
			{
				blackcounter = blackcounter + 1;
			}
			else if ( Map[i][j] == 'W')
			{
				whitecounter = whitecounter + 1;
			}
			
			/*
				Here little explaining for this move.The logic
				is same for every direction.If the specific
				Map[i][j] pawn of the map is the same colour with 
				asked colour then looks direction of same row,
				right after the specific pawn.if the next right is 
				opposite colour(meaning is opposite player) then
				goes into a for loop from next of the opposite pawn.
				While program founds opposite colour pawns continue.
				If program finds same colour with asked colour break 
				as it means that this direction is not possible.
				If programs finds empty seat after only opposite 
				colour pawns then keep this empty seat as possible move
				into matrix Moves and break.
			*/
			if ( Map[i][j] == colour)
			{
				//checking right after pawn 
				if (j<6 && Map[i][j+1] == opcolour)
				{
					for (z = j+2; z<COLUMN; z++)
					{
						if (Map[i][z] == colour)
						{
							break;
						}
						if (Map[i][z] == 'E')
						{
							Moves[a][0] = i;
							Moves[a][1] = z;
							a++;
							break;
						}
					}
				}
				
				//checking left after pawn
				if(j>1 && Map[i][j-1] == opcolour)
				{
					for(z = j-2; z>=0; z--)
					{
						if (Map[i][z] == colour)
						{
							break;
						}
						if (Map[i][z] == 'E')
						{
							Moves[a][0] = i;
							Moves[a][1] = z;
							a++;
							break;
						}
					}
				}
				
				//checking down after pawn 
				if ( i<6 && Map[i+1][j] == opcolour)
				{
					for (z = i+2; z<ROW; z++)
					{
						if ( Map[z][j] == colour)
						{
							break;
						}
						if ( Map[z][j] == 'E')
						{
							Moves[a][0] = z;
							Moves[a][1] = j;
							a++;
							break;
						}
					}
				}
				
				//checking up before pawn
				if ( i>1 && Map[i-1][j] == opcolour)
				{
					for(z = i-2; z>=0; z--)
					{
						if ( Map[z][j] == colour)
						{
							break;
						}
						if ( Map[z][j] == 'E')
						{
							Moves[a][0] = z;
							Moves[a][1] = j;
							a++;
							break;
						}
					}
				}
				
				//checking right down diagonal after pawn 
				if (i<6 && j<6 && Map[i+1][j+1] == opcolour)
				{
					z = i+2;
					y = j+2;
					while( z<8 && y<8 && Map[z][y] == opcolour)
					{
						z++;
						y++;
					}
					if(z<8 && y<8 && Map[z][y] == 'E')
					{
						Moves[a][0] = z;
						Moves[a][1] = y;
						a++;
					}
				}
				
				//checking left up diagonal before pawn
				if (i>1 && j>1 && Map[i-1][j-1] == opcolour)
				{
					z = i-2;
					y = j-2;
					while(z>=0 && y>=0 && Map[z][y] == opcolour)
					{
						z--;
						y--;
					}
					if(z>=0 && y>=0 && Map[z][y] == 'E')
					{
						Moves[a][0] = z;
						Moves[a][1] = y;
						a++;
					}
				}
				
				//checking left down diagonal before pawn 
				if (i<6 && j>1 && Map[i+1][j-1] == opcolour)
				{
					z = i+2;
					y = j-2;
					while(z<8 && y>=0 && Map[z][y] == opcolour)
					{
						z++;
						y--;
					}
					if(z<8 && y>=0  && Map[z][y] == 'E')
					{
						Moves[a][0] = z;
						Moves[a][1] = y;
						a++;
					}
				}
				
				//checking right up after pawn 
				if (i>1 && j<6 && Map[i-1][j+1] == opcolour)
				{
					z = i-2;
					y = j+2;
					while(z>=0 && y<8 && Map[z][y] == opcolour)
					{
						z--;
						y++;
					}
					if(z>=0 && y<8 && Map [z][y] == 'E')
					{
						Moves[a][0] = z;
						Moves[a][1] = y;
						a++;
					}
				}
				
			}
		}
	}
	return;
}



/*
	-----------------------------FUNCTION----------------------------------------
	      FlipPawns(unsigned char r,unsigned char c,unsigned char colour)
	Avr calls this function either after his move or PC moves.With this function
	if the move is correct AVR is doing all the necessary flips to the map and put 
	the added move to the map.
	----------------------------Parameters---------------------------------------
	unsigned char r:takes the row of asking move
	unsigned char c:takes the column of asking move
	unsigned char colour: takes the colour of player for who will check the flips
    ------------------------More informations------------------------------------
	The logic for checking is very easy.For every possible direction AVR checks
	from the asking position until he finds the same colour pawn.If he finds that
	for a possible direction exists the same colour understands that player did 
	the specific move with tis direction in his mind and as a result AVR flips
	opponent pawns between the two of them(asking position and existing pawn).
*/

void FlipPawns(unsigned char r,unsigned char c,unsigned char colour)
{
	int flagleft = 0,flagright = 0; //variables for row checking
	int flagdown = 0,flagup = 0;    //variables for column checking
	int flagdiagdown = 0,flagdiagup = 0; //variables for diagonal checking
	int i,j,z,y;
	unsigned char opcolour;
	
	if (colour == 'B')
	{
		opcolour = 'W';
	}else
	{
		opcolour = 'B';
	}
	
	/*
		Here little explaining for this move.The logic
		is same for every direction.We have the asked 
		playable move in values r,c.As we already said
		r is for number of row and c is for number of
		column that player asked to play.Then in this 
		move from asked position, program is checking
		down of the same column.If the next down is 
		opposite colour goes into a for loop while finds
		opposite colour pawns.If program finds empty 
		then means that player doesn't played this move 
		for this direction and as a result breaks and 
		continues to the other directions.Although if 
		program finds same colour with the player and
		between only opposite colour then makes the 
		proportionate flag(here flagdown) to 1 and 
		break.Now we have in value (i) the seat of the
		pawn of player.So with a new value (j) from 
		asked position until (i-1) program puts player
		move to the Map and flips the opposite pawns. 
	*/
	
	//-------------CHECK COLUMN---------------
	if ((r%65)<7 && Map[(r%65)+1][c%49] == opcolour) 
	{
		//checking same column down
		for ( i = (int)((r%65)+2); i< ROW; i++ )
		{
			if (Map[i][c%49] == colour)
			{
				flagdown = 1;
				break;
			}
			
			if (Map[i][c%49] == 'E')
			{
				break;
			}
		}
		
		if ( flagdown == 1 )
		{
			for ( j = (int)(r%65); j<i; j++)
			{
				Map[j][c%49] = colour;
			}
		}
	}

	
	if ((r%65)>1 && Map[(r%65)-1][c%49] == opcolour) 
	{
		//checking same column up
		for ( i = (int)(r%65-2); i>= 0; i--)
		{
			if (Map[i][c%49] == colour)
			{
				flagup = 1;
				break;
			}
			
			if (Map[i][c%49] == 'E')
			{
				break;
			}
		}
		
		if ( flagup == 1 )
		{
			for ( j = (int)(r%65); j>i; j--)
			{
				Map[j][c%49] = colour;
			}
		}
	}
	
	//-------------CHECK ROW--------------- 
	
	if ((c%49)<6 && Map[r%65][(c%49)+1] == opcolour)
	{
		//checking right after asking position
		for ( j = (int)(c%49+2); j<COLUMN; j++)
		{
			if (Map[r%65][j] == colour)
			{
				flagright = 1;
				break;
			}
			
			if (Map[r%65][j] == 'E')
			{
				break;
			}
		}
		
		if ( flagright == 1 )
		{
			for ( z = (int)(c%49); z<j; z++)
			{
				Map[r%65][z] = colour;
			}
		}
	}
	
	if((c%49)> 1 && (Map[r%65][(c%49)-1] == opcolour) )
	{
		//checking left before asking position
		for ( j = (int)(c%49-2); j>=0; j--)
		{
			if ( Map[r%65][j] == colour)
			{
				flagleft = 1;
				break;
			}
			
			if (Map[r%65][j] == 'E')
			{
				break;
			}
		}
		
		if ( flagleft == 1 )
		{
			for ( z = (int)(c%49); z>j; z--)
			{
				Map[r%65][z] = colour;
			}
		}
		
	}
	
	
	//-------------CHECK DIAGONAL---------------
	
	//checking right down diagonal after pawn
	if( (Map[(r%65)+1][(c%49)+1] == opcolour) )
	{
		
		z = (int)((r%65)+2);
		y = (int)((c%49)+2);
		while( z<8 && y<8 && Map[z][y] == opcolour)
		{
			z++;
			y++;
		}
		if(z<8 && y<8 && Map[z][y] == colour)
		{
			flagdiagdown = 1;
		}
	}
	
	if ( flagdiagdown == 1 )
	{
		Map[r%65][c%49]=colour;
		z = (int)((r%65)+1);
		y = (int)((c%49)+1);
		while(Map[z][y] == opcolour)
		{
			Map[z][y] = colour;
			z++;
			y++;
		}
	}
	flagdiagdown = 0;

	//checking left down diagonal before pawn
	if(Map[(r%65)+1][(c%49)-1] == opcolour)
	{
		z = (int)((r%65)+2);
		y = (int)((c%49)-2);
		while( z<8 && y>=0 && Map[z][y] == opcolour)
		{
			z++;
			y--;
		}
		if(z<8 && y>=0 && Map[z][y] == colour)
		{
			flagdiagdown = 1;
		}
	}
	
	if ( flagdiagdown == 1 )
	{
		Map[r%65][c%49]=colour;
		z = (int)((r%65)+1);
		y = (int)((c%49)-1);
		while(Map[z][y] == opcolour)
		{
			Map[z][y] = colour;
			z++;
			y--;
		}
	}
	flagdiagdown = 0;
	
	//checking right up diagonal after pawn
	if( (Map[(r%65)-1][(c%49)+1] == opcolour) )
	{
		z = (int)((r%65)-2);
		y = (int)((c%49)+2);
		while( z>=0 && y<8 && Map[z][y] == opcolour)
		{
			z--;
			y++;
		}
		if(z>=0 && y<8 && Map[z][y] == colour)
		{
			flagdiagup = 1;
		}
	}
	
	if ( flagdiagup == 1 )
	{
		Map[r%65][c%49]=colour;
		z = (int)((r%65)-1);
		y = (int)((c%49)+1);
		while(Map[z][y] == opcolour)
		{
			Map[z][y] = colour;
			z--;
			y++;
		}
	}
	flagdiagup = 0;
	//checking left up diagonal before pawn
	if(Map[(r%65)-1][(c%49)-1] == opcolour) 
	{
		z = (int)((r%65)-2);
		y = (int)((c%49)-2);
		while( z>=0 && y>=0 && Map[z][y] == opcolour)
		{
			z--;
			y--;
		}
		if(z>=0 && y>=0 && Map[z][y] == colour)
		{
			flagdiagup = 1;
		}
	}
	
	if ( flagdiagup == 1 )
	{
		Map[r%65][c%49]=colour;
		z = (int)((r%65)-1);
		y = (int)((c%49)-1);
		while(Map[z][y] == opcolour)
		{
			Map[z][y] = colour;
			z--;
			y--;
		}
	}
	flagdiagup = 0;
	return;
}



/*
	---------------------- FUNCTION----------------------------
	                avrplaying(int choice)
	Program uses this function so AVR do his move.
	-----------------------Parameters--------------------------
	int choice : AVR see if PC has done a playable move 
	             1 for a correct move from PC
				 0 for a wrong move from PC
    --------------------More informations----------------------
	If choice is 0 then shows to PC that his move was illegal.
	If choice is 1 then checks his playable moves and do the 
	first one.If checkingLegalMoves return with no moves shows
	MP to PC.
*/

void avrplaying(int choice)
{
	
	if ( choice == 1)
	{
		CheckingLegalMoves(avrcolour);
		if(Moves[0][0] <8 )			//We put (  <8 ) as ( 0 <= Moves[i] < 8 ) (0 for A unti 7 for H)
		{
			letter = (unsigned char)(Moves[0][0]+65);	//From Ascii table 0+65=A until 7+65=H for rows
			number = (unsigned char)(Moves[0][1]+49);	//Ascii table 0+49=1 until 7+49=8 for columns
			tiles = tiles + 1;
			flagplay = 1;
			TransmitData('M');
			TransmitData('M');
			TransmitData(' ');
			TransmitData(letter);
			TransmitData(number);
			TransmitData('\n');
		}
		else
		{
			TransmitData('M');
			TransmitData('P');
			TransmitData('\n');
		}
		flagok = 1;
	}
	else
	{
		TransmitData('I');
		TransmitData('L');
		TransmitData('\n');
		flagdead = 1 ;
	}
	
	return;
}

/*
	---------------------- FUNCTION----------------------------
	                          win()
	With this function after every move we check if game
	has come to the end with the value tiles.
	If tiles shows that game ends AVR compares black and white 
	pawns to see who wins.
    --------------------More informations----------------------
	Value tiles counts from number 4 to to 64.
	The logic is that at the start the game has 4 pawns
	(2 black and 2 white).After every move tiles goes up by one 
	as it means that a pawn has added to our map.
*/

void win()
{
	if ( tiles == 64)
	{
		flagok = 0;
		if (playercolour == 'B')
		{
			if(blackcounter < whitecounter)
			{
				TransmitData('W');
				TransmitData('N');
				TransmitData('\n');
				PORTA = 0xFE;
			}
			else if (blackcounter > whitecounter)
			{
				TransmitData('L');
				TransmitData('S');
				TransmitData('\n');
				PORTA = 0xFD;
			}else
			{
				TransmitData('T');
				TransmitData('E');
				TransmitData('\n');
				PORTA = 0xFB;
			}
		}
		else
		{
			if(blackcounter < whitecounter)
			{
				TransmitData('L');
				TransmitData('S');
				TransmitData('\n');
				PORTA = 0xFD;
			}
			else if (blackcounter > whitecounter)
			{
				TransmitData('W');
				TransmitData('N');
				TransmitData('\n');
				PORTA = 0xFE;
			}else
			{
				TransmitData('T');
				TransmitData('E');
				TransmitData('\n');
				PORTA = 0xFB;
			}
		}
	}
	return;
}



/*
	---------------------FUNCTION-------------------------
	                analyzing_command()
	Is our basic function that as it is name says,
	analyze each command that PC gave,
	in order AVR to do his move.
    -----------------More informations--------------------
	The whole function is inside Atomic_Block operation,
	so not to be distract if something else come from PC.
	We use switch case to choose our state.
*/

void analyzing_command()
{
	int flag = 0;
	
	ATOMIC_BLOCK(ATOMIC_FORCEON)	// we used this for safety reasons. When we analyze the string we've had we don;t want any other interrupt to occur
	{
		memcpy(buffer, temporary, 15);	
		memset(temporary, '0', 15);
		
		switch(buffer[0])			// switch case to determine in which of the instructions we are.
		{							// using the cases we check the sequence of the received string. 
			//*****************************************************//
			case 'A':
			if (buffer[1] == 'T')
			{
				if ( buffer [2] == 13)
				{
					print_ok();
					return;
				}
				else
				{
					print_error();
				}
			}
			break;
			//****************************************************//-
			//If PC send RST(reset) , AVR justs put EMPTY in whole MAP
			case 'R':
			if (buffer[1] == 'S')
			{
				if (buffer[2] == 'T')
				{
					if ( buffer[3] == 13)
					{
						for (int i = 0; i < ROW; i++)
						{
							for (int j = 0; j < COLUMN; j++)
							{
								Map[i][j] = 'E';
							}
						}
						print_ok();
						return;
					}
				}
			}
			print_error();
			break;
			//******************************************************//
			//With this state AVR understands what colour PC player wants so to be the opposite
			//In case PC player doesn't use this state AVR chooses by default to be white.
			case 'S':
			if (buffer[1] == 'P')
			{
				if ( buffer[2] == 32)
				{
					if ( (buffer[3] == 'B') || (buffer[3] == 'W') )
					{
						if (buffer[4] == 13)
						{
							playercolour = buffer[3];
							if (playercolour == 'B')
							{
								avrcolour = 'W';
							}
							else
							{
								avrcolour = 'B';
							}
                            
							if (playercolour == 'W')
							{
								avrplaying(1);
							}
							
							return;
						}						
					}
				}
			}
			//With ST state PC can change the amount of time that he has available in each move
			else if(buffer[1] == 'T')
			{
				if (buffer[2] == 32)
				{
					if ( (buffer[3] >= 48) && (buffer[3] <= 57) )
					{
						if (buffer[4] == 13)
						{
							othellotime = buffer[3]%48;
							print_ok();
							return;
						}
					}
				}
			}
			print_error();
			break;
			//******************************************************//
			//Here AVR initialize the initial state of Othello pawns
			case 'N':
			if (buffer[1] == 'G')
			{
				if (buffer[2] == 13)
				{
					PORTA = 0xff;
					tiles = 4;
					for (int i = 0; i < ROW; i++)
					{
						for (int j = 0; j < COLUMN; j++)
						{
							if ( i == 3 && j == 3 )
							{
								Map[i][j] = 'W';
							}
							else if(i == 3 && j== 4)
							{
								Map[i][j] = 'B';
							}
							else if(i == 4 && j == 3)
							{
								Map[i][j] = 'B';
							}
							else if(i == 4 && j == 4)
							{
								Map[i][j] = 'W';
							}
							else
							{
								Map[i][j] = 'E';
							}
						}
					}
					print_ok();
					return;
				}
			}
			print_error();
			break;
			//******************************************************//
			//The game ends 
			case 'E':
			if (buffer[1] == 'G')
			{
				if (buffer[2] == 13)
				{
					for (int i = 0; i < ROW; i++)
					{
						for (int j = 0; j < COLUMN; j++)
						{
							Map[i][j] = 'E';
						}
					}
					print_ok();
					return;
				}
			}
			print_error();
			break;
			//******************************************************//
			//In this state PC says that he has not any move so AVR plays
			case 'P':
			if ( buffer[1] == 'S')
			{
				if (buffer[2] == 13)
				{
					avrplaying(1);
					return;
				}
			}
			if(buffer[1] == 'L')
			{
				if (buffer[2] == 13)
				{
					print_ok();
					return;
				}
			}
			print_error();
			break;
			//******************************************************//
			//State that PC gives a move
			case 'M':
			//Before AVR play PC move has to check if PC gave a move into the amount of time that moves have.
			if ( seconds > othellotime)
			{
				TransmitData('I');
				TransmitData('T');
				TransmitData('\n');
				flagdead = 1;
				seconds = 0;
				return;
			}
			seconds = 0;
			if (buffer[1] == 'V')
			{
				if (buffer[2] == 32)
				{
					if ( (buffer[3] >= 65) || (buffer[3] <= 72) )
					{
						if ( (buffer[4] >= 49) && (buffer[4] <= 56) )
						{
							if (buffer[5] == 13)
							{
								print_ok();
								
								CheckingLegalMoves(playercolour);
								//AVR now use for for loop to check if PC gave a playable move
								for (int i=0; i<=20; i++)
								{
									if ((unsigned int)(buffer[3]%65) == Moves[i][0] && (unsigned int)(buffer[4]%49) == Moves[i][1])
									{
										FlipPawns(buffer[3],buffer[4],playercolour);
										tiles = tiles + 1;
										flag = 1;
										break;
									}
								}
								
								avrplaying(flag);
								return;
							}
						}
					}
				}
			}
			print_error();
			break;
			//******************************************************//
			//If PC wants to quit and etc. sends WN so AVR understand that he wins
			case 'W':
			if (buffer[1] == 'N')
			{
				if (buffer[2] == 13)
				{
					print_ok();
					PORTA = 0xFE;
					tiles = 0;
					return;
				}
			}
			print_error();
			break;
			//******************************************************//
			//Here AVR has to understand which OK command PC means.
			case 'O':
			if (buffer[1] == 'K')
			{
				if (buffer[2] == 13)
				{
					if (flagok == 1)		//In case AVR is in state that has a move or he has pass
					{
						timer_init();		//AVR initial the timer after he received OK from PC 
						if(flagplay == 1)	//With this flag AVR knows that PC is okay with his move so he is doing the move
						{
							flagplay = 0;
							FlipPawns(letter,number,avrcolour);
						}
						flagok = 0;
					}
					if (flagdead == 1)		//flagdead is a flag to shows to AVR that OK means PC received IL or IT and wants to quit so AVR wins.
					{
						TransmitData('W');
						TransmitData('N');
						TransmitData('\n');
						PORTA = 0xFE;
						tiles = 0;
						flagdead = 0;
					}
					return;
				}
			}
			//******************************************************//
			default:
			print_error();
			break;
		}							//switch end if
		
	}
}


void timer_init()
{
	
	TCCR0 = (1<<CS00) | (1<<CS02);	//prescaling 1024
	TCNT0 = 0x2E;					// initialize counter
	TIMSK |= (1 << TOIE0);			// enable overflow interrupt
	sei();							// enable global interrupts
	tot_overflow = 0;				// initialize overflow counter variable
}

ISR(TIMER0_OVF_vect)
{
	sei();
	TIMSK |= (1 << TOIE0);
	
	tot_overflow++;					// keep a track of number of overflows
	if (tot_overflow>=4)
	{
		seconds = seconds + 1;
		tot_overflow = 0;
		TCNT0 = 0x2E;
	}
}

int main(void)
{
	USART_Init();
	DDRA = 0xff;					// setting portA as an output
	PORTA = 0xff;					// setting LEDs to be closed
	
	while(1)
	{		
		if (ready == 1)
		{	
			PORTA = 0xff;			// close our LEDs
			analyzing_command();
			ready = 0;				// resetting the whole operation. analyzing command just finished so we have to wait for another command.
			for (int i = 0; i < ROW; i++)
			{
				for (int j = 0; j < COLUMN; j++)
				{
					TransmitData(Map[i][j]);
				}
				
				TransmitData('\n');
			}
			win();
		}
	}
}
