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
#define A_DIGIT     0b10001000
#define B_DIGIT     0b10000011
#define C_DIGIT     0b11000110
#define D_DIGIT     0b10100001
#define E_DIGIT     0b10000110
#define F_DIGIT     0b10001110
#define DP_DIGIT    0b01111111
#define COLON_DIGIT 0b00000100
#define F_CPU 16000000 // cpu speed in hertz 
#define ON 1
#define OFF 0
#define CW 1
#define CCW -1
#define SNOOZEOFF 0
#define SNOOZEON 1
#define SNOOZEALARM 2
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// holds data to be sent to the segments. logic zero turns segment on
uint8_t segment_data[5] = {255,255,255,255,255}; 

// decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12] = {ZERO_DIGIT, ONE_DIGIT, TWO_DIGIT, THREE_DIGIT, FOUR_DIGIT, FIVE_DIGIT, SIX_DIGIT, SEVEN_DIGIT, EIGHT_DIGIT, NINE_DIGIT, DP_DIGIT, COLON_DIGIT}; 

uint8_t hex_to_7seg[16] = {ZERO_DIGIT, ONE_DIGIT, TWO_DIGIT, THREE_DIGIT, FOUR_DIGIT, FIVE_DIGIT, SIX_DIGIT, SEVEN_DIGIT, EIGHT_DIGIT, NINE_DIGIT, A_DIGIT, B_DIGIT, C_DIGIT, D_DIGIT, E_DIGIT, F_DIGIT};

// Flag indicating when interrupt was triggered
volatile uint8_t secondsFlag=1;

// Flag indicating when snooze was triggered
volatile uint8_t snoozeFlag=0;

// Holds value that stores the time of the clock
volatile uint16_t currentTime = 0;

uint8_t DecHex = 10;
/*********************************************************************/
//              SPI_read(uint8_t currentBarGraph)
// Sends the bar graph data and then reads the SPI port.
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
// Checks the state of the button number passed to it. It shifts in ones till   
// the button is pushed. Function returns a 1 only once per debounced button    
// push so a debounce and toggle function can be implemented at the same time.  
// Adapted to check all buttons from Ganssel's "Guide to Debouncing"            
// Expects active low pushbuttons on PINA port.  Debounce time is determined by 
// external loop delay times 12. 
// Edited to have a state array of size 8 for each button
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
// takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
// BCD segment code in the array segment_data for display.                       
// array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
//***********************************************************************************
void segsum(uint16_t sum) {
  
  uint8_t  i = 0;
  uint8_t  digitNum = 1;
  uint16_t sumPlaceHolder = sum;
  while(i < 4 && sumPlaceHolder > (DecHex-1))	
  {
    sumPlaceHolder /= DecHex;
    digitNum++;
    i++;
  } 
  // Parses 0-4 digits into seperate segment_data[] locations 
  switch(digitNum)
  {
    case 1:
      segment_data[4] = hex_to_7seg[sum];
      segment_data[3] = hex_to_7seg[0];// 0xFF;
      segment_data[1] = hex_to_7seg[0];//0xFF;
      segment_data[0] = hex_to_7seg[0];//0xFF;
      break;
    case 2:
      segment_data[4] = hex_to_7seg[(sum % DecHex)];
      segment_data[3] = hex_to_7seg[(sum / DecHex)];
      segment_data[1] = hex_to_7seg[0];//0xFF;
      segment_data[0] = hex_to_7seg[0];// 0xFF;
      break;
    case 3:
      segment_data[4] = hex_to_7seg[sum % DecHex];
      segment_data[3] = hex_to_7seg[(sum/DecHex)%DecHex];
      segment_data[1] = hex_to_7seg[(sum/(DecHex*DecHex))];
      segment_data[0] = hex_to_7seg[0];//0xFF;
      break;
    case 4:
      segment_data[4] = hex_to_7seg[sum % DecHex];
      segment_data[3] = hex_to_7seg[(sum/DecHex)%DecHex];
      segment_data[1] = hex_to_7seg[(sum/(DecHex*DecHex))%DecHex];
      segment_data[0] = hex_to_7seg[sum/(DecHex*DecHex*DecHex)];
    default:
      break;
  }
}

//***********************************************************************************
//                                   displaySwitch                                    
// Takes the segment_data[] array that has the #_DIGIT values and displays it to the 
// current LED digit (displayValue) and returns the next value that will be used for displaying
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
// Function for when the interrupt was triggered, executing next main loop
// Takes in the current value outputted and returns the adjusted value based on the number
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
  while(buttonLoop < 3)
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
//                     HexConversion(uint8_t currentButtonsPressed)                                    
// Checks if the 3rd button is pressed for changing HEX and DEC mode
//***********************************************************************************
uint8_t HexConversion(uint8_t currentButtonsPressed)
{
  if(0 == (currentButtonsPressed & 0x04))
  { 
    return 0; 
  }
  return 1;
}
//***********************************************************************************
//                     ClockDisplayMode(uint8_t buttonMode)                                    
// Changes the Mode the display value should be in
// 1 -> Set/Select Alarm 
// Takes in the current value outputted and returns the adjusted value based on the number
//***********************************************************************************
uint8_t ClockDisplayMode(uint8_t buttonMode)
{
  switch(buttonMode)
  {
    case 0:
      return 1;
    /*
    case 1: 
      return 2;
    case 2:
      return 4;
    case 3:
      return 0;
    */
  }
  return 5;
}

//***********************************************************************************
//                     ClockCounterCorrection(uint16_t displayValue)                                    
// Takes in a value and ensures it is in the format a clock would use
// Returns value in the format HH:MM 
//***********************************************************************************
uint16_t ClockCounterCorrection(uint16_t displayValue)
{
  static uint8_t displayValueHours;
  static uint8_t displayValueMins;
  
  displayValueHours = (displayValue / 60);
  displayValueMins  = (displayValue - (60 * displayValueHours));
  displayValue      = ((displayValueHours * 100) + displayValueMins);
  
  return displayValue;
}
//***********************************************************************************
//     int8_t EncoderValueDirection(uint8_t currentEncoderValue, uint8_t urrentAdjustment)                                    
// Checks direction of encoders turning and returns if the turn was CW or CCW
// Returns positive currentAdjustment value (CW) or negative currentAdjustment value (CCW)      
//*********************************************************************************
int8_t EncoderValueDirection(uint8_t currentEncoderValue)
{
  //Tests current encoder value against previous encoder value 
  //Tests if forward by value 0x00 --> 0x01, returns pos adjustment value
  //Tests else if reverse, 0x01 --> 0x00, returns neg adjustment value
  //First If statment checks   0B000000__ 
  //Second If statment checks  0B0000__00 
  
  static uint8_t previousEncoderValue = 0x0F;

  if((previousEncoderValue & 0x03) == 0x00 && (currentEncoderValue & 0x03) == 0x01)
  {
    previousEncoderValue = (currentEncoderValue & 0x0F);
    return CW;
  }
  else if((previousEncoderValue & 0x03) == 0x01 && (currentEncoderValue & 0x03) == 0x00)
  {
    previousEncoderValue = (currentEncoderValue & 0x0F);
    return CCW;   
  }
  
  //Checks the second Encoder
  if((previousEncoderValue & 0x0C) == 0x00 && (currentEncoderValue & 0x0C) == 0x04)
  {
    previousEncoderValue = (currentEncoderValue & 0x0F);
    return CW;
  }
  else if((previousEncoderValue & 0x0C) == 0x04 && (currentEncoderValue & 0x0C) == 0x00)
  {
    previousEncoderValue = (currentEncoderValue & 0x0F);
    return CCW;   
  }
  previousEncoderValue = currentEncoderValue;
  return 0;
}

//***********************************************************************************
//                                   uint8_t ButtonUtility()                                    
// Toggles the button presses by the user
// Decides functionality of each button press
//*********************************************************************************
void ButtonUtility()
{

}

//***********************************************************************************
//                                   ISR(TIMER0_OVF_vect)                                    
// Triggered when TimerCounter0 overflows (every second)
// Toggles COLON bits
// Counts Seconds, rolls over every 60, increments and rolls clock over 
// Counts up too 255 (which inidcates 1 sec with 32Khz clk & 128 prescale)
//*********************************************************************************
ISR(TIMER0_OVF_vect)
{ 
  static uint8_t currentSeconds = 0;
  static uint8_t snoozeTimer = 0;
  segment_data[2] ^= 0x03;   
  if(currentSeconds < 60)
  {
    if(snoozeFlag == SNOOZEON)
    {
      snoozeTimer++;
      segment_data[2] ^= 0x04;
    }
    currentSeconds++;
  }else
  {
    currentTime++;
    currentSeconds = 0;
  }

  if(snoozeTimer == 10)
  {
    snoozeFlag = SNOOZEALARM;
    snoozeTimer = 0;
  }
}

//***********************************************************************************
//                                   ISR(TIMER1_OVF_vect)                                    
// Triggered when TimerCounter1 overflows
//
//*********************************************************************************
ISR(TIMER1_OVF_vect)
{ 
  //secondsFlag ^= 0x01;
}
//***********************************************************************************
//                                   uint16_t AlarmSet(uint8_t)                                    
// Function entered when the user presses the first button on the button board
// loops until user to inputs time (w/ encoder)
// Once user presses same button, Alarm is set and function is exitted// 
//*********************************************************************************
uint16_t AlarmSetMode()
{
  static uint8_t  currentEncoderValue  = 0;
  static uint16_t encodersDisplayValue  = 0;
  int8_t currentAdjustmentValue = 0;
  
  currentEncoderValue = (SPI_read(currentAdjustmentValue));  
  currentAdjustmentValue = EncoderValueDirection(currentEncoderValue);
  encodersDisplayValue += currentAdjustmentValue;
  // Checks if the clock will roll backwards behind 0
  // 1439 is clock time for 23 : 59
  if((encodersDisplayValue == 0) && (currentAdjustmentValue == CCW)) 
  {
    encodersDisplayValue = 1439;
  }else if(encodersDisplayValue > 1439)
  {
    encodersDisplayValue = 0;
  }
  return encodersDisplayValue;
}

//***********************************************************************************
//                                   uint16_t AlarmSet(uint8_t)                                    
// Function entered when the user presses the first button on the button board
// loops until user to inputs time (w/ encoder)
// Once user presses same button, Alarm is set and function is exitted// 
//*********************************************************************************
uint16_t ClockSetMode()
{
  uint8_t  currentEncoderValue  = 0;
  uint16_t encodersDisplayValue  = 0;
  int8_t currentAdjustmentValue = 0;
  
  currentEncoderValue = (SPI_read(currentAdjustmentValue));  
  currentAdjustmentValue = EncoderValueDirection(currentEncoderValue);
  encodersDisplayValue += currentAdjustmentValue;
  // Checks if the clock will roll backwards behind 0
  // 1439 is clock time for 23 : 59
  if((encodersDisplayValue == 0) && (currentAdjustmentValue == CCW)) 
  {
    encodersDisplayValue = 1439;
  }else if(encodersDisplayValue > 1439)
  {
    encodersDisplayValue = 0;
  }
  return encodersDisplayValue;
}
//***********************************************************************************
int main()
{

//TODO:
//Initialize
//  TCNT1 - CTC  Mode | Pick freuquency      | Output too PD7		!Outputs to summing amp, which gets outputted to speaker!
//  TCNT2 - Fast PWM  | Output to PB7 (OC2)  				!Controls brightness of LED Display!
//  TCNT3 - Fast PWM  | Output to PE5 (OC3C)				!Controls volume to Audio Amp!
DDRA  = 0xFF;	      //set port A as input  				
DDRB  = 0xFF; 	      //set port B as outputs
DDRD  = (1 << PD2);   //Sets Port pin2 D to output
DDRE  = (1 << PE6);   //Sets Port pin6 E to output
PORTD = 0x00;   //set port D to LOW
PORTB = 0x10;   //set port B to start with LED1  	
PORTE = 0xFF;
//  TCNT0 - Norm Mode | Using external 32kHz clock | 128 Prescale	!Count to 250 using uint8_t to reach 1 second for clock!

ASSR   |= (1 << AS0);			//Use external 32kHz clock 
SPCR   |= (1 << SPE) | (1 << MSTR);	//Enable SPI communication in mastermode
TIMSK  |= (1 << TOIE0) | (1 << TOIE1); //| (1 << TOIE1);	//enable interrupt on compare & overflow of TCNT1
TCCR0  |= (1 << CS00) | (1 << CS01);//SET CS02	//normal mode, prescale by 128
TCCR1A |= (1 << COM1A1);		//Clear on Compare Match (OC1A)
TCCR1B |= (1 << WGM12); 		//CTC mode clear at TOP immediate
sei();
uint8_t  currentButtonsPressed = 0;	//Stores buttons that are currently pressed (holds value when pressed)
uint8_t  currentDisplayDigit = 0;       //Current LED to display on (0 == 1's digit
uint16_t displayValue = 0;	        //Current value to display on LEDs
uint16_t alarmValue = 1;	        //Current value held by the alarm
uint8_t  alarmActivated = OFF;		//If the Alarm is ON or OFF, initialize to OFF
while(1){
  
  currentButtonsPressed = ButtonCheck(currentButtonsPressed);
  
  // Button Utility
  switch(currentButtonsPressed)
  {
    // Alarm Set Mode
    case 0x01:
      alarmValue     = AlarmSetMode();
      displayValue   = alarmValue; 
      break;

    // Activate Alarm, Set L3 to indicate alarm set
    case 0x03:
      alarmActivated = ON;
      segment_data[2] &= 0xFB;      
      currentButtonsPressed = (0x00);
      break;

    // Snooze Alarm
    case 0x04:
      if(alarmActivated)
      {
        snoozeFlag = SNOOZEON;    
      }
      currentButtonsPressed = (0x00);
      break;
    
    // Deactivate Alarm
    case 0x08:
      alarmActivated = OFF;
      snoozeFlag = SNOOZEOFF;
      segment_data[2] |= (0xFF);      
      currentButtonsPressed = (0x00);
      break;
    
    case 0x10:
      currentTime = ClockSetMode();
      break;

    // Display Time as Normal
    default:
      displayValue = currentTime;
  }

  if(snoozeFlag == SNOOZEALARM)
  {
    alarmActivated = ON;
    //segment_data[2] &= 0xFB;      
  }

  // Alarm is reached and activated
  // Play alarm
  if(alarmActivated && ((currentTime == alarmValue) || (snoozeFlag == SNOOZEALARM)) && (snoozeFlag != SNOOZEON))
  {
    segment_data[2] |= 0x04;      
  }
  
  displayValue = ClockCounterCorrection(displayValue);
  
  segsum(displayValue);						//Divide the decimal value to the segment_data[] array
  currentDisplayDigit = displaySwitch(currentDisplayDigit);	//Display the current values stored in segment_data[] to current LED
  }//while
return 0;
}//main
