// lab3.c 
// Conor Wolfin
// 10.26.2018

//  HARDWARE SETUP:
//  PORTA is connected to the segments of the LED display. and to the pushbuttons.
//  PORTA.0 corresponds to segment a, PORTA.1 corresponds to segement b, etc.
//  PORTB bits 4-6 go to a,b,c inputs of the 74HC138.
//  PORTB bit 7 goes to the PWM transistor base.

#define ZERO_DIGIT  0b11000000
#define ONE_DIGIT   0b11111001
#define TWO_DIGIT   0b10100100
#define THREE_DIGIT 0b10110000
#define FOUR_DIGIT  0b10011001
#define FIVE_DIGIT  0b10010010
#define SIX_DIGIT   0b10000011
#define SEVEN_DIGIT 0b11111000
#define EIGHT_DIGIT 0b10000000
#define NINE_DIGIT  0b10011000
#define DP_DIGIT    0b01111111
#define COLON_DIGIT 0b00000000
#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#define CW 0
#define CWW 1
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

//holds data to be sent to the segments. logic zero turns segment on
uint8_t segment_data[5] = {255,255,255,255,255}; 

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12] = {ZERO_DIGIT, ONE_DIGIT, TWO_DIGIT, THREE_DIGIT, FOUR_DIGIT, FIVE_DIGIT, SIX_DIGIT, SEVEN_DIGIT, EIGHT_DIGIT, NINE_DIGIT, DP_DIGIT, COLON_DIGIT}; 

//Flag indicating when interrupt was triggered
volatile uint8_t flag=0;

/*********************************************************************/
//              SPI_read(uint8_t currentBarGraph)
//Sends the bar graph data and then reads the SPI port.
/*********************************************************************/
uint16_t SPI_read(uint8_t currentBarGraph){
  PORTB &= 0x7F;
  SPDR = currentBarGraph;
  while (bit_is_clear(SPSR,SPIF)){} //wait till 8 bits have been sent
  PORTD = 0xFF;
  PORTD = 0x00;
  PORTE = 0x00;
  PORTE = 0xFF;
  return(SPDR); //return incoming data from SPDR
}
//******************************************************************************
//                            chk_buttons(uint8_t buttons)                     
//Checks the state of the button number passed to it. It shifts in ones till   
//the button is pushed. Function returns a 1 only once per debounced button    
//push so a debounce and toggle function can be implemented at the same time.  
//Adapted to check all buttons from Ganssel's "Guide to Debouncing"            
//Expects active low pushbuttons on PINA port.  Debounce time is determined by 
//external loop delay times 12. 
//Edited to have a state array of size 8 for each button
//******************************************************************************
uint8_t chk_buttons(uint8_t buttons) {
  //Gansels debounce with the state as an array that is used to check against the values that buttons is at
  static uint16_t state[8] = {0}; //holds present state
  state[buttons] = (state[buttons] << 1) | (! bit_is_clear(PINA, buttons)) | 0xE000;
  if (state[buttons] == 0xF000) return 1;
  return 0;
}

//***********************************************************************************
//                                   segment_sum                                    
//takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
//BCD segment code in the array segment_data for display.                       
//array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
//***********************************************************************************
void segsum(uint16_t sum) {
  
  uint8_t  i = 0;
  uint8_t  digitNum = 1;
  uint16_t sumPlaceHolder = sum;
  while(i < 4 && sumPlaceHolder > 9)	
  {
    sumPlaceHolder /= 10;
    digitNum++;
    i++;
  } 
  //Parses 0-4 digits into seperate segment_data[] locations 
  switch(digitNum)
  {
    case 1:
      segment_data[4] = dec_to_7seg[sum];
      segment_data[3] = 0xFF;
      segment_data[1] = 0xFF;
      segment_data[0] = 0xFF;
      break;
    case 2:
      segment_data[4] = dec_to_7seg[(sum % 10)];
      segment_data[3] = dec_to_7seg[(sum / 10)];
      segment_data[1] = 0xFF;
      segment_data[0] = 0xFF;
      break;
    case 3:
      segment_data[4] = dec_to_7seg[sum % 10];
      segment_data[3] = dec_to_7seg[(sum/10)%10];
      segment_data[1] = dec_to_7seg[(sum/100)];
      segment_data[0] = 0xFF;
      break;
    case 4:
      segment_data[4] = dec_to_7seg[sum % 10];
      segment_data[3] = dec_to_7seg[(sum/10)%10];
      segment_data[1] = dec_to_7seg[(sum/100)%10];
      segment_data[0] = dec_to_7seg[sum/1000];
    default:
      break;
  }
}

//***********************************************************************************
//                                   displaySwitch                                    
//Takes the segment_data[] array that has the #_DIGIT values and displays it to the 
//current LED digit (displayValue) and returns the next value that will be used for displaying
//***********************************************************************************
uint8_t displaySwitch(uint8_t displayValue)
{
  switch(displayValue){
    case 0:
      PORTB = 0x07;
      PORTA = segment_data[4];
      break;
    case 1:
      PORTB = 0x17;
      PORTA = segment_data[3];
      break;
    case 2:
      PORTB = 0x27;
      PORTA = segment_data[2];
      break;
    case 3:
      PORTB = 0x37;
      PORTA = segment_data[1];
      break;
    case 4:
      PORTB = 0x47;
      PORTA = segment_data[0];
      break;
    default:
      break;
  } 
  _delay_ms(1);				//Adds delay for screen congruency
  if(displayValue == 4) return 0; 	//Starts display back to 0
  return ++displayValue; 
} 

//***********************************************************************************
//                     ButtonCheck(uint8_t buttonMode)                                    
//Function for when the interrupt was triggered, executing next main loop
//Takes in the current value outputted and returns the adjusted value based on the number
//***********************************************************************************
uint8_t ButtonCheck(uint8_t buttonMode)
{
  //PORTA to input w/ pullups 
  DDRA  = 0x00;	
  PORTA = 0xFF;
  //enable tristate buffer for pushbutton switches via DEC7 on the encoder
  PORTB = 0x70; 
  uint8_t buttonLoop = 0;
  _delay_us(10);		//BUG"Added delay to get first button to work, need better fix
  while(buttonLoop < 2)
  {     
    if(chk_buttons(buttonLoop))
    {
      buttonMode ^= (1<<buttonLoop); 
    }
    buttonLoop++;
  }
  DDRA = 0xFF;
  return buttonMode;
}

//***********************************************************************************
//                     SetAdjustmentValue(uint8_t buttonMode)                                    
//Changes the output to desired 
//Takes in the current value outputted and returns the adjusted value based on the number
//***********************************************************************************
uint8_t SetAdjustmentValue(uint8_t buttonMode)
{
  switch(buttonMode)
  {
    case 0:
      return 1;
    case 1: 
      return 2;
    case 2:
      return 4;
    case 3:
      return 0;
  }
  return 5;
}

//***********************************************************************************
//                                   ISR(TIMER0_OVF_vect)                                    
//Triggered when TimerCounter0 overflows
//Sets flag to be utilized in main                       
//*********************************************************************************
ISR(TIMER0_OVF_vect)
{
  if(!flag)
  flag = 1;
}
//***********************************************************************************
int main()
{
DDRB  = 0xFF; 	      //set port B as outputs
DDRD  = (1 << PD2);   //Sets Port pin2 D to output
DDRE  = (1 << PE6);   //Sets Port pin6 E to output
PORTD = 0x00;   //set port D to LOW
PORTB = 0x10;   //set port B to start with LED1  	
PORTE = 0xFF;
SPCR  |= (1 << SPE) | (1 << MSTR);//Enable SPI communication in mastermode
TIMSK |= (1<<TOIE0);              //enable interrupts
TCCR0 |= (1<<CS02) | (1<<CS00);   //normal mode, prescale by 128
sei();
uint8_t  previousEncoderValue1 = 0x03;  //Value to store previous encoder1 value (bits 0 & 1)
uint8_t  previousEncoderValue2 = 0x0C;  //Value to store previous encoder2 value (bits 2 & 3
uint8_t  currentEncoderValue;		//Stores entire encoderValue
uint8_t  currentDisplayDigit = 0;       //Current LED to display on (0 == 1's digit
uint16_t currentValue = 0;	        //Current value stored from pushbuttons
uint8_t  currentButtonsPressed = 0;	//Buttons pressed
uint8_t  currentAdjustmentValue = 1;	//The value to inc/dec by
while(1){
  //Flag set to one when TIMER0 interrupt set
  //Sets the value that will be increased by for bargraph
  //currentAdjustmentValue = 1 | 2 | 4 | 0
  if(flag) 
  {
    currentButtonsPressed = ButtonCheck(currentButtonsPressed);
    currentAdjustmentValue = SetAdjustmentValue(currentButtonsPressed);
    flag = 0;
    currentEncoderValue = (SPI_read(currentAdjustmentValue));
  }

  //Value to inc/dec by
  //---------------------------------------------------------------------------
  //Determining CW vs. CCW
  //0x01 -> 0x00 : CW
  //0x00 -> 0x01 : CCW
  //Sets previousEncoderValueX to currentEncoderValue
  
  //Tests current encoder1 value against previous encoder1 value 
  //Increments by adjustment value, if determined CW
  //bits 0B000000__ Representing First Encoder
  //"CW
  if(previousEncoderValue1 == 0x00 && (currentEncoderValue & 0x03) == 0x01)
  {
    currentValue += currentAdjustmentValue;
  }
  //Tests current encoder1 value against previous encoder1 value 
  //Increments by adjustment value, if determined CCW
  //bits 0B000000__ Representing First Encoder
  //"CCW
  else if(previousEncoderValue1 == 0x01 && (currentEncoderValue & 0x03) == 0x00)
  {
    //If value will be lower than 0 when decremented, revert to 1023
    //If not, decrement as normal
    if(currentValue < currentAdjustmentValue)
    {
      currentValue = 1023;
    }else{	
      currentValue -= currentAdjustmentValue;
    }
  }
  previousEncoderValue1 = (currentEncoderValue & 0x03);
  
  //Tests current encoder2 value against previous encoder2 value 
  //Increments by adjustment value, if determined CW
  //bits 0B0000__00 Representing Second Encoder
  //"CW
  if(previousEncoderValue2 == 0x00 && (currentEncoderValue & 0x0C) == 0x04)
  {
    currentValue += currentAdjustmentValue;
  }
  //Tests current encoder2 value against previous encoder2 value 
  //Increments by adjustment value, if determined CCW
  //bits 0B0000__00 Representing Second Encoder
  //"CCW
  else if(previousEncoderValue2 == 0x04 && (currentEncoderValue & 0x0C) == 0x00)
  {
    //If value will be lower than 0 when decremented, revert to 1023
    //If not, decrement as normal
    if(currentValue < currentAdjustmentValue)
    {
      currentValue = 1023;
    }else{	
      currentValue -= currentAdjustmentValue;
    }
  }
  previousEncoderValue2 = (currentEncoderValue & 0x0C);
  //---------------------------------------------------------------------------
  /*
  
	  // ENCODER VALUE TEST:
	  // outputs decoder values          
   	  //(also need to comment out segsum)
	  currentValue = (SPI_read(currentAdjustmentValue));
	  if(currentValue & 0x01)
	  {
	    segment_data[0] = dec_to_7seg[1];
	  }else{
	    segment_data[0] = dec_to_7seg[0];
	  }  
	  
	  if(currentValue & 0x02)
	  {
	    segment_data[1] = dec_to_7seg[1];
	  }else{
	    segment_data[1] = dec_to_7seg[0];
	  }  
	  //Resets the current value that is incrimented 
	  if(currentValue & 0x04)
	  {
	    segment_data[3] = dec_to_7seg[1];
	  }else{
	    segment_data[3] = dec_to_7seg[0];
	  }  
	  //Resets the digits to avoid leftover numbers
	  if(currentValue & 0x08)
	  {
	    segment_data[4] = dec_to_7seg[1];
	  }else{
	    segment_data[4] = dec_to_7seg[0];
	  } 
  
 */ 

  if(currentValue > 1023)
  { 
    segment_data[0] = 0xFF; 
    segment_data[1] = 0xFF; 
    segment_data[3] = 0xFF; 
    segment_data[4] = 0xFF; 
    currentValue -= 1023;   //Change too = 1 if do not want roll over data   
  } 
  
  segsum(currentValue);						//Divide the decimal value to the segment_data[] array
  currentDisplayDigit = displaySwitch(currentDisplayDigit);	//Display the current values stored in segment_data[] to current LED
  }//while
return 0;
}//main
