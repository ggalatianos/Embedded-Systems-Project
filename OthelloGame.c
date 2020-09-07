/*
 * OthelloGame.c
 *
 * Created: 08-Apr-19 14:51:24
 * Author : Kwstis & Galatis
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

//infinity for our program(we say more at the function minimax)
//flagok : If OK from PC means that AVR played
//flagdead : If OK from PC means that PC quit(after IL,IT) and AVR wins
//flagplay : If OK from PC means that AVR played without had MP
//flagquit : AVR saw that eventually he lost the game so he quits
int8_t maximum = +65,  minimum = -65, flagok = 0, flagdead = 0, flagplay = 0, flagquit = 0;
//(black/white)counter : keep counter for each colour
//tiles : keeps pawns/(correct moves) for both players
int8_t tiles, blackcounter, whitecounter;
//number of rows and columns for the playable map
static uint8_t ROW=8,COLUMN=8;	
//Map of the game							
volatile unsigned char Map[8][8];	
//For ISR of transmit/receive			
volatile unsigned char buffer[15],temporary[15]; 
//values for keep pawns colour for players
unsigned char playercolour='B',avrcolour='W';
//values for AVR moves(letter for row,number for column)
unsigned char letter,number;
//offset : for ISR of transmit/receive
//Moves[20][2] : Keeps possible moves per turn for each player
volatile int8_t offset = 0;	
/*ready : is a flag for each turn
          1 means that PC player send his command
		  0 means that AVR waits something from Keyboard
*/
volatile uint8_t ready = 0;
//values for time
volatile uint8_t othellotime=2,seconds=0;
//tot_overflow : keeps overflows of timer
volatile uint8_t tot_overflow;

typedef struct {
	uint8_t M[20][2];
} Moves;

void USART_Init( void )
{
	UBRRH = 0;									// Set baud rate
	UBRRL = 64;
	
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

int8_t max(int8_t x, int8_t y)
{//Returns the biggest number between two numbers
	if (x>y)
	{
		return x;
	}
	else
	{
		return y;
	}
}

int8_t min(int8_t x, int8_t y)
{//Returns the smallest number between two numbers
	if (x<y)
	{
		return x;
	}
	else
	{
		return y;
	}
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

ISR(TIMER0_OVF_vect)
{
	sei();
	TIMSK |= (1 << TOIE0);
	
	tot_overflow = tot_overflow + 1;			// keep a track of number of overflows
	if (tot_overflow>=39)
	{
		seconds = seconds + 1;
		tot_overflow = 0;
		TCNT0 = 0x00;
	}
}

void timer_init()
{
	TCCR0 = (1<<CS00) | (1<<CS02);	//prescaling 1024
	TCNT0 = 0x00;					// initialize counter
	TIMSK |= (1 << TOIE0);			// enable overflow interrupt
	sei();							// enable global interrupts
	tot_overflow = 0;				// initialize overflow counter variable
	seconds = 0;
}

/*
	-----------------------------FUNCTION-----------------------------------------
				 uint8_t duplicateMove(int r,int c,Moves Matrix)
	This function is used so when function checking legal Moves find a possible 
	position that player can play checks if this seat already have found so not
	to write it again.
	----------------------------Parameters-----------------------------------------
	int r: This variable takes the row of possible move that has found
	int c: This variable takes the column of the possible move that has found
	Moves Matrix: Here we have the struct of the array of Moves.So function
				  takes the array of Moves and checks if this move already exists.		   
    -----------------------------More informations---------------------------------
	We can see that as return we send a variable of uint8_t.More simple if the 
	possible move does not exist function sends back 1 otherwise function sends 0.
*/

uint8_t duplicateMove(int r,int c,Moves Matrix)
{
	uint8_t i=0;
	
	while(Matrix.M[i][0]<8)
	{
		if(Matrix.M[i][0] == r && Matrix.M[i][1] == c)
		{
			return 0;
		}
		i++;
	}
	return 1;
}

/*
	-----------------------------FUNCTION-----------------------------------------
	      CheckingLegalMoves(unsigned char colour,unsigned char table[8][8])
	Avr calls this function before his move or PC moves.With this function
	knows if the asking move from PC is correct.Also AVR calls the function for 
	himself in order to know the possible moves for his play. 
	----------------------------Parameters-----------------------------------------
	unsigned char colour:takes the colour of player for who will check the possible
	                     moves
	unsigned char table[8][8]: This variable helps to know which map avr is looking
								as we have two maps(the main map of the game and the
								back up map).When we say back up map ,we mean the 
								maps for minimax to go ahead moves.			   
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
	move. At he end this function return the array with the possible moves.
*/

Moves CheckingLegalMoves(unsigned char colour,unsigned char table[8][8])
{
	int8_t i,j,z,y,a=0;
	Moves Mov;
	unsigned char opcolour;
	
	for (i=0;i<20; i++)
	{
		for (j = 0; j<=1; j++)
		{
			Mov.M[i][j] = 8;
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
			
			if ( table[i][j] == 'B')
			{
				blackcounter = blackcounter + 1;
			}
			else if ( table[i][j] == 'W')
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
			if ( table[i][j] == colour)
			{
				//checking right after pawn 
				if (j<6 && table[i][j+1] == opcolour)
				{
					for (z = j+2; z<COLUMN; z++)
					{
						if (table[i][z] == colour)
						{
							break;
						}
						if (table[i][z] == 'E')
						{
							if (duplicateMove(i,z,Mov))
							{
								Mov.M[a][0] = i;
								Mov.M[a][1] = z;
								a++;
								break;
							}
							break;
						}
					}
				}
				
				//checking left after pawn
				if(j>1 && table[i][j-1] == opcolour)
				{
					for(z = j-2; z>=0; z--)
					{
						if (table[i][z] == colour)
						{
							break;
						}
						if (table[i][z] == 'E')
						{
							if (duplicateMove(i,z,Mov))
							{
								Mov.M[a][0] = i;
								Mov.M[a][1] = z;
								a++;
								break;
							}
							break;
						}
					}
				}
				
				//checking down after pawn 
				if ( i<6 && table[i+1][j] == opcolour)
				{
					for (z = i+2; z<ROW; z++)
					{
						if ( table[z][j] == colour)
						{
							break;
						}
						if ( table[z][j] == 'E')
						{
							if (duplicateMove(z,j,Mov))
							{
								Mov.M[a][0] = z;
								Mov.M[a][1] = j;
								a++;
								break;
							}
							break;
						}
					}
				}
				
				//checking up before pawn
				if ( i>1 && table[i-1][j] == opcolour)
				{
					for(z = i-2; z>=0; z--)
					{
						if ( table[z][j] == colour)
						{
							break;
						}
						if ( table[z][j] == 'E')
						{
							if (duplicateMove(z,j,Mov))
							{
								Mov.M[a][0] = z;
								Mov.M[a][1] = j;
								a++;
								break;
							}
							break;
						}
					}
				}
				
				//checking right down diagonal after pawn 
				if (i<6 && j<6 && table[i+1][j+1] == opcolour)
				{
					z = i+2;
					y = j+2;
					while( z<8 && y<8 && table[z][y] == opcolour)
					{
						z++;
						y++;
					}
					if(z<8 && y<8 && table[z][y] == 'E')
					{
						if (duplicateMove(z,y,Mov))
						{
							Mov.M[a][0] = z;
							Mov.M[a][1] = y;
							a++;
						}
					}
				}
				
				//checking left up diagonal before pawn
				if (i>1 && j>1 && table[i-1][j-1] == opcolour)
				{
					z = i-2;
					y = j-2;
					while(z>=0 && y>=0 && table[z][y] == opcolour)
					{
						z--;
						y--;
					}
					if(z>=0 && y>=0 && table[z][y] == 'E')
					{
						if (duplicateMove(z,y,Mov))
						{
							Mov.M[a][0] = z;
							Mov.M[a][1] = y;
							a++;
						}
					}
				}
				
				//checking left down diagonal before pawn 
				if (i<6 && j>1 && table[i+1][j-1] == opcolour)
				{
					z = i+2;
					y = j-2;
					while(z<8 && y>=0 && table[z][y] == opcolour)
					{
						z++;
						y--;
					}
					if(z<8 && y>=0  && table[z][y] == 'E')
					{
						if (duplicateMove(z,y,Mov))
						{
							Mov.M[a][0] = z;
							Mov.M[a][1] = y;
							a++;
						}
					}
				}
				
				//checking right up after pawn 
				if (i>1 && j<6 && table[i-1][j+1] == opcolour)
				{
					z = i-2;
					y = j+2;
					while(z>=0 && y<8 && table[z][y] == opcolour)
					{
						z--;
						y++;
					}
					if(z>=0 && y<8 && table[z][y] == 'E')
					{
						if (duplicateMove(z,y,Mov))
						{
							Mov.M[a][0] = z;
							Mov.M[a][1] = y;
							a++;
						}
					}
				}
				
			}
		}
	}
	return Mov;
}

/*
	--------------------------------------FUNCTION--------------------------------------------
	FlipPawns(unsigned char r,unsigned char c,unsigned char colour,unsigned char table[8][8])
		Avr calls this function either after his move or PC moves.With this function
		if the move is correct AVR is doing all the necessary flips to the map and put 
		the added move to the map.
	------------------------------------Parameters--------------------------------------------
		unsigned char r:takes the row of asking move
		unsigned char c:takes the column of asking move
		unsigned char colour: takes the colour of player for who will check the flips
		unsigned char table[8][8]: Here FlipPawns takes the map of the game that we are doing 
		                           the flips.We have this variable so when minimax go ahead 
								   moves do not change the main map of the game but the back up
								   map.
    ---------------------------------More informations----------------------------------------
		The logic for checking is very easy.For every possible direction AVR checks
		from the asking position until he finds the same colour pawn.If he finds that
		for a possible direction exists the same colour understands that player did 
		the specific move with tis direction in his mind and as a result AVR flips
		opponent pawns between the two of them(asking position and existing pawn).
*/

void FlipPawns(unsigned char r,unsigned char c,unsigned char colour,unsigned char table[8][8])
{
	int8_t flagleft = 0,flagright = 0; //variables for row checking
	int8_t flagdown = 0,flagup = 0;    //variables for column checking
	int8_t flagdiagdown = 0,flagdiagup = 0; //variables for diagonal checking
	int8_t i,j,z,y,counter = 0;
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
	if ((r%65)<7 && table[(r%65)+1][c%49] == opcolour) 
	{
		//checking same column down
		for ( i = (int)((r%65)+2); i< ROW; i++ )
		{
			if (table[i][c%49] == colour)
			{
				flagdown = 1;
				break;
			}
			
			if (table[i][c%49] == 'E')
			{
				break;
			}
		}
		
		if ( flagdown == 1 )
		{
			table[r%65][c%49] = colour;
			for ( j = (int)(r%65+1); j<i; j++)
			{
				table[j][c%49] = colour;
				counter=counter + 1;
			}
		}
	}

	
	if ((r%65)>1 && table[(r%65)-1][c%49] == opcolour) 
	{
		//checking same column up
		for ( i = (int)(r%65-2); i>= 0; i--)
		{
			if (table[i][c%49] == colour)
			{
				flagup = 1;
				break;
			}
			
			if (table[i][c%49] == 'E')
			{
				break;
			}
		}
		
		if ( flagup == 1 )
		{
			table[r%65][c%49] = colour;
			for ( j = (int)(r%65-1); j>i; j--)
			{
				table[j][c%49] = colour;
				counter=counter + 1;
			}
		}
	}
	
	//-------------CHECK ROW--------------- 
	
	if ((c%49)<6 && table[r%65][(c%49)+1] == opcolour)
	{
		//checking right after asking position
		for ( j = (int)(c%49+2); j<COLUMN; j++)
		{
			if (table[r%65][j] == colour)
			{
				flagright = 1;
				break;
			}
			
			if (table[r%65][j] == 'E')
			{
				break;
			}
		}
		
		if ( flagright == 1 )
		{
			table[r%65][c%49] = colour;
			for ( z = (int)(c%49+1); z<j; z++)
			{
				table[r%65][z] = colour;
				counter=counter + 1;
			}
		}
	}
	
	if((c%49)> 1 && (table[r%65][(c%49)-1] == opcolour) )
	{
		//checking left before asking position
		for ( j = (int)(c%49-2); j>=0; j--)
		{
			if ( table[r%65][j] == colour)
			{
				flagleft = 1;
				break;
			}
			
			if (table[r%65][j] == 'E')
			{
				break;
			}
		}
		
		if ( flagleft == 1 )
		{
			table[r%65][c%49] = colour;
			for ( z = (int)(c%49-1); z>j; z--)
			{
				table[r%65][z] = colour;
				counter=counter + 1;
			}
		}
		
	}
	
	
	//-------------CHECK DIAGONAL---------------
	
	//checking right down diagonal after pawn
	if( (table[(r%65)+1][(c%49)+1] == opcolour) )
	{
		
		z = (int)((r%65)+2);
		y = (int)((c%49)+2);
		while( z<8 && y<8 && table[z][y] == opcolour)
		{
			z++;
			y++;
		}
		if(z<8 && y<8 && table[z][y] == colour)
		{
			flagdiagdown = 1;
		}
	}
	
	if ( flagdiagdown == 1 )
	{
		table[r%65][c%49]=colour;
		z = (int)((r%65)+1);
		y = (int)((c%49)+1);
		while(table[z][y] == opcolour)
		{
			table[z][y] = colour;
			counter=counter + 1;
			z++;
			y++;
		}
	}
	flagdiagdown = 0;

	//checking left down diagonal before pawn
	if(table[(r%65)+1][(c%49)-1] == opcolour)
	{
		z = (int)((r%65)+2);
		y = (int)((c%49)-2);
		while( z<8 && y>=0 && table[z][y] == opcolour)
		{
			z++;
			y--;
		}
		if(z<8 && y>=0 && table[z][y] == colour)
		{
			flagdiagdown = 1;
		}
	}
	
	if ( flagdiagdown == 1 )
	{
		table[r%65][c%49]=colour;
		z = (int)((r%65)+1);
		y = (int)((c%49)-1);
		while(table[z][y] == opcolour)
		{
			table[z][y] = colour;
			counter=counter + 1;
			z++;
			y--;
		}
	}
	flagdiagdown = 0;
	
	//checking right up diagonal after pawn
	if( (table[(r%65)-1][(c%49)+1] == opcolour) )
	{
		z = (int)((r%65)-2);
		y = (int)((c%49)+2);
		while( z>=0 && y<8 && table[z][y] == opcolour)
		{
			z--;
			y++;
		}
		if(z>=0 && y<8 && table[z][y] == colour)
		{
			flagdiagup = 1;
		}
	}
	
	if ( flagdiagup == 1 )
	{
		table[r%65][c%49]=colour;
		z = (int)((r%65)-1);
		y = (int)((c%49)+1);
		while(table[z][y] == opcolour)
		{
			table[z][y] = colour;
			counter=counter + 1;
			z--;
			y++;
		}
	}
	flagdiagup = 0;
	//checking left up diagonal before pawn
	if(table[(r%65)-1][(c%49)-1] == opcolour) 
	{
		z = (int)((r%65)-2);
		y = (int)((c%49)-2);
		while( z>=0 && y>=0 && table[z][y] == opcolour)
		{
			z--;
			y--;
		}
		if(z>=0 && y>=0 && table[z][y] == colour)
		{
			flagdiagup = 1;
		}
	}
	
	if ( flagdiagup == 1 )
	{
		table[r%65][c%49]=colour;
		z = (int)((r%65)-1);
		y = (int)((c%49)-1);
		while(table[z][y] == opcolour)
		{
			table[z][y] = colour;
			counter=counter + 1;
			z--;
			y--;
		}
	}
	counter = counter + 1;//Here we add the move as counter until now has only flips
	if ( colour == 'B')
	{
		blackcounter = blackcounter + counter;
		whitecounter = whitecounter - (counter-1);
	}
	else
	{
		whitecounter = whitecounter + counter;
		blackcounter = blackcounter - (counter -1);
	}
	flagdiagup = 0;
	return;
}

/*
	----------------------------FUNCTION-----------------------------------
	  minimax(letter,number,depth,alpha,beta,maximizingplayer,table[8][8])
	This function is used for avr to find the best possible move each time
	by going ahead moves.How far will go is defined with the variable depth.
	Also it is very important to emphasize that we have used the logic of
	alpha-beta strategy algorithm.Aplha-beta is based on minmax algorithm 
	with the advantage of pruning some childs.This helps both for time
	execution but also memory allocation something that is very important
	in our case as we do not have unlimited memory.
	----------------------------Parameters--------------------------------- 
	unsigned char letter: takes the letter(row) of possible move
	unsigned char number: takes the number(column) of possible move
	int8_t depth: takes the depth of the recursive.When we say depth 
				   we mean how man moves will check ahead.
	int8_t alpha: takes the max infinity
	int8_t beta: takes the min infinity
	bool maximizingplayer: this boolean variable is used so the algorithm
						   of alpha-beta knows for which player is checking.
	unsigned char table[8][8]: Takes the each map that we are looking ahead 
							   every time.					   
    --------------------------More informations-----------------------------
	We call minimax from function avrplaying.The first time is after we have
	found the possible moves for the specific time playing.For each of these 
	possible moves minimax go ahead and scores the result(score).When the 
	depth has come to zero or we see that after some moves the map is fulled
	so the game has come to an end this algorithm returns the scoring.
	More simple minimax checks the best move for avr not only based on score for
	each move but also on what possible moves can give to the opposite player.
	As a result the best move is the one that gives the best score for AVR and 
	at the same time the minimum(bad) score for the opposite player.
*/

int8_t minimax(unsigned char letter, unsigned char number, int8_t depth, int8_t alpha, int8_t beta, bool maximizingplayer, volatile unsigned char table[8][8], int8_t end_of_game)
{
	int8_t maxEval, minEval, evaluation, static_evaluation, i;
	unsigned char board[8][8];
	Moves LegalMoving;
	
	memcpy(board,table,64);
	
	if (maximizingplayer)
	{
		FlipPawns(letter,number,avrcolour,board);
	} 
	else
	{
		FlipPawns(letter,number,playercolour,board);
	}
	
	if(depth==0)
	{
		if (avrcolour=='B')
		{
			static_evaluation = blackcounter - whitecounter;
			return static_evaluation;
		}
		else
		{
			static_evaluation =  whitecounter - blackcounter;
			return static_evaluation;
		}
	}
	else if(end_of_game==64)
	{
		
		if (avrcolour=='B')
		{
			static_evaluation = blackcounter - whitecounter;
			if (static_evaluation<0)
			{
				flagquit = 1;
				return static_evaluation;
			}
			else
			{
				return static_evaluation;
			}			
		}
		else
		{
			static_evaluation =  whitecounter - blackcounter;
			if (static_evaluation<0)
			{
				flagquit = 1;
				return 100;
			}
			else
			{
				return static_evaluation;
			}
		}
	}
	
	if (maximizingplayer)
	{
		// worst case for any of the players is to lose 63-1 (very rare situation) so 62/-62 is considered
		//infinity and minus infinity respectively for our game.
		// any value above ore beneath is also infinite. 65 is for safety reasons
		maxEval = minimum;		
		LegalMoving = CheckingLegalMoves(playercolour,board);
		
		i=0;
		while(LegalMoving.M[i][0]<8)
		{
			evaluation = minimax((unsigned char)(LegalMoving.M[i][0]+65), (unsigned char)(LegalMoving.M [i][1]+49), depth-1, alpha, beta, false, board, end_of_game+1);
			maxEval = max(maxEval, evaluation);
			alpha = max(alpha, maxEval);
			if (beta <= alpha)
			{
				break;
			}
			i = i + 1;
		}
		return maxEval;
	}
	else
	{
		minEval = maximum;
		LegalMoving = CheckingLegalMoves(avrcolour,board);
		
		i=0;
		while(LegalMoving.M[i][0]<8)
		{
			evaluation = minimax((unsigned char)(LegalMoving.M[i][0]+65), (unsigned char)(LegalMoving.M [i][1]+49), depth-1, alpha, beta, true, board, end_of_game+1);
			minEval = min(minEval, evaluation);
			beta = min(beta, minEval);
			if (beta <= alpha)
			{
				break;
			}
			i = i + 1;
		}
		return minEval;
	}
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

void avrplaying(int8_t choice)
{
	Moves Mov;
	int8_t i, evaluation = 0, bestEvaluation = minimum;
	
	if (choice == 1)
	{
		Mov = CheckingLegalMoves(avrcolour,Map);	
		//Here we print the possible moves for AVR had before he played his move
		/*	
		TransmitData('A');
		TransmitData('V');
		TransmitData('R');
		TransmitData('\n');
		i=0;
		while( Mov.M[i][0] <8)
		{
			TransmitData((unsigned char)Mov.M[i][0]+65);
			TransmitData((unsigned char)Mov.M[i][1]+49);
			TransmitData('\n');
			i = i+1;
		}	
		*/
		if ( (avrcolour=='B') && (tiles==4) )
		{
			i = rand() % 4;
			letter = (unsigned char)(Mov.M[i][0]+65);
			number = (unsigned char)(Mov.M[i][1]+49);
			flagplay = 1;
			TransmitData('M');
			TransmitData('M');
			TransmitData(' ');
			TransmitData(letter);
			TransmitData(number);
			TransmitData('\n');
		}
		else if (Mov.M[0][0] < 8)
		{
			i=0;
			while(Mov.M[i][0] < 8)
			{				
				evaluation = minimax((unsigned char)(Mov.M[i][0]+65),(unsigned char)(Mov.M[i][1]+49),2,minimum,maximum,false,Map,tiles+1);
				
				if ((seconds == (othellotime-1)) && (tot_overflow >= 30)  )
				{
					seconds = 0;
					break;
				}
				
				if (flagquit==1)
				{
					TransmitData('Q');
					TransmitData('T');
					TransmitData('\n');
					return;
				}
				else if (bestEvaluation<evaluation)
				{
					bestEvaluation = evaluation;
					letter = (unsigned char)(Mov.M[i][0]+65);
					number = (unsigned char)(Mov.M[i][1]+49);
				}
				i++;
			}
			
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
			}
			else
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
	int8_t flag = 0, i, j;
	Moves Mov;
	
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
						PORTA = 0xff;
						tiles = 0;
						for (i = 0; i < ROW; i++)
						{
							for (j = 0; j < COLUMN; j++)
							{
								Map[i][j] = 'E';
							}
						}
						flagdead=0;
						flagok=0;
						flagquit=0;
						flagplay=0;
						tot_overflow=0;
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
							print_ok();
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
					othellotime = 2;
					for (i = 0; i < ROW; i++)
					{
						for (j = 0; j < COLUMN; j++)
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
					tiles = 0;
					for (i = 0; i < ROW; i++)
					{
						for (j = 0; j < COLUMN; j++)
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
					print_ok();
					timer_init();
					avrplaying(1);
					return;
				}
			}
			if(buffer[1] == 'L')
			{
				if (buffer[2] == 13)
				{
					print_ok();
					flagdead = 0;
					seconds = 0;
					timer_init();
					return;
				}
			}
			print_error();
			break;
			//******************************************************//
			//State that PC gives a move
			case 'M':
			//Before AVR play PC move has to check if PC gave a move into the amount of time that moves have.
			if ( seconds > othellotime )
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
								
								Mov = CheckingLegalMoves(playercolour,Map);	
								//Here we print the possible moves that PC had before it played the incoming move
								/*
								i=0;
								TransmitData('P');
								TransmitData('C');
								TransmitData('\n');
								while( Mov.M[i][0] <8)
								{
									TransmitData((unsigned char)Mov.M[i][0]+65);
									TransmitData((unsigned char)Mov.M[i][1]+49);
									TransmitData('\n');
									i = i+1;
								}
								*/
															
								
								//AVR now use for for loop to check if PC gave a legal move
								for (i=0; i<=20; i++)
								{
									if ((unsigned int)(buffer[3]%65) == Mov.M[i][0] && (unsigned int)(buffer[4]%49) == Mov.M[i][1])
									{
										FlipPawns(buffer[3],buffer[4],playercolour,Map);
										tiles = tiles + 1;
										flag = 1;
										break;
									}
								}
								timer_init();
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
							tiles = tiles + 1;
							FlipPawns(letter,number,avrcolour,Map);
						}
						flagok = 0;
					}
					else if (flagdead == 1)		//flagdead is a flag to shows to AVR that OK means PC received IL or IT and wants to quit so AVR wins.
					{
						TransmitData('W');
						TransmitData('N');
						TransmitData('\n');
						PORTA = 0xFE;
						tiles = 0;
						flagdead = 0;
					}
					else if (flagquit==1)
					{
						TransmitData('L');
						TransmitData('S');
						TransmitData('\n');
						PORTA = 0xFD;
						flagquit=0;	
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
			win();
			
			/*
			We have this <printing> in comments so whoever wants
			to play and check the algorithm can see the map of the
			game after every move.
			for (int i = 0; i < ROW; i++)
			{
				for (int j = 0; j < COLUMN; j++)
				{
					TransmitData(Map[i][j]);
				}
				
				TransmitData('\n');
			}
			TransmitData('\n');
			*/
			
		}
	}
}
