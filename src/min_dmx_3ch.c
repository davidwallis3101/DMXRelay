/*
    Sept 22, 2017

    Jan Wullschleger
    jan.wullschleger@hotmail.com
    Custom Effects LED Solutions Inc
    www.custom-effects.com
    
    DMX512 Decoder
    
    Written to replace software on cheap DMX512 decoders bought from China with STC11L04E mcu.
    The decoders would flicker every ~4s, apparently due to not feeding the watchdog.

    IO pins
    STC11L04E
    IC pin  port    function
    1       P3.6    Status LED
    2       P3.0    DMX512 RxD (also used for programming)
    3       P3.1    TxD (not used, only for programming)
    6       P3.2    Red 
    7       P3.3    Green
    8       P3.4    Blue
    9       P3.5    DIP10
    11      P3.7    DIP1 
    12      P1.0    DIP2
    13      P1.1    DIP3
    14      P1.2    DIP4
    15      P1.3    DIP5
    16      P1.4    DIP6
    17      P1.5    DIP7
    18      P1.6    DIP8
    19      P1.7    DIP9
    
    PWM frequency = ~480Hz
    No-DMX timeout = 1s (output turns off)
    LED -  steady ON - FUN mode
           slow flash - DMX mode with no DMX present
           fast flash - DMX mode with DMX present

    DIP10 OFF - DMX mode - remainder of DIPs set the DMX address
    DIP10 ON - Fun mode
        DIP 8&9 - fun mode
            0 - color set by highest setting of the lower 7 DIPS
            1 - color fade through all colors, speed set by the highest setting of the lower 7 DIPS
            2 - color jump through all colors, speed set by the highest setting of the lower 7 DIPS
            3 - color strobe , lowest 3 DIPS select RGB color (0 is random color), next 4 DIPS set speed by the highest setting of the 4 DIPS
           
    v1.0
        Only FUN mode 0 implemented

    v1.1 TODO
            increase PWM frequency maybe? Bitbang out the lowest couple bits all at once for accurate timing
            add fun modes 1-3
            change fun mode color settings to DIPS 1-3 and use 4-7 for intensity
*/

/*
DW -

Links:
https://www.qlcplus.org/forum/viewtopic.php?t=11071
https://www.custom-effects.com/my_files/mini_dmx_3ch.c
*/

__sfr __at(0x87) PCON;
__sfr __at(0x88) TCON;
__sfr __at(0x89) TMOD;
__sfr __at(0x8a) TL0;
__sfr __at(0x8c) TH0;
__sfr __at(0x8e) AUXR;

__sfr __at(0x90) P1;
__sfr __at(0x92) P1M0;
__sfr __at(0x98) SCON;
__sfr __at(0x99) SBUF;
__sfr __at(0x9c) BRT;

__sfr __at(0xa2) AUXR1;
__sfr __at(0xa8) IE;

__sfr __at(0xb0) P3;
__sfr __at(0xb2) P3M0;

__sfr __at(0xC1) WDT_CONTR;


volatile unsigned long int clocktime;       //used to flash the status LED
volatile unsigned char pwm_bit;             //keep track of the active PWM bit 
volatile unsigned char red,green,blue;      //values used by the PWM generator
volatile int byte_cnt;                      //used in DMX receive interrupt to keep track of current byte (DMX slot)
volatile int last_packet_size;              //size of last DMX packet
volatile unsigned char temp_r,temp_g,temp_b;//used in the DMX interrupt to store the RGB values before the DMX packet is verified as good
volatile int address;                       //DMX address to respond to
volatile unsigned int DMX_timeout;          //timeout counter to determine when there's no more valid DMX data

/*PWM rate above 500Hz (arbitrary choosen frequency based on quick Google search) - maybe increase later into the Khz range to prevent flicker on video recording devices
    variable clock interrupt that interrupts 8 times, each time equal to half the last interrupt and corresponding to the bit of the value that we are outputing
 */
void clockinc(void) __interrupt(1)
{
    //PWM generator
    if(pwm_bit&red)     P3 |= 0x04;//red on
    else                P3 &= ~0x04;//red off 
    if(pwm_bit&green)   P3 |= 0x08;//green on
    else                P3 &= ~0x08;//green off 
    if(pwm_bit&blue)    P3 |= 0x10;//blue on
    else                P3 &= ~0x10;//blue off 

    //THO:THL must be set to (65536-timer_delay) so just take the pwm_bit (which has a single bit set) and invert it.
    //this works out to about 480Hz PWM frequency
	TH0 = ~(pwm_bit>>4);
	TL0 = ~(pwm_bit<<4);

    if(pwm_bit <= 0x01)     //this should keep the pwm_bit in the designed loop no matter the initial start value
    {
        pwm_bit = 0x80;//restart 
        clocktime++;//2.08ms
        DMX_timeout++;
    }
    else    pwm_bit = pwm_bit>>1;//shift 1 down
}

//Serial Interrupt
void SerialInt(void) __interrupt(4)
{
    if(SCON & 0x01)//RI Receive Interrupt set
    {
        if(SCON & 0x80)//Frame Error (happens during the break in the DMX packet)
        {
            if(SBUF == 0)//must be 0 for this to be the BREAK in the DMX packet
            {
                if(byte_cnt != last_packet_size)//If this frame is not the same size as the last frame then discard.  It will take 2 good frames in consecutive order to restart again.
                {
                    last_packet_size = byte_cnt;//maybe the DMX control software changed the DMX frame size
                    PCON &= ~0x04;//clear the Good Packet Bit
                    //DMX packet size error
                }
                
                if(PCON & 0x04)//Good Packet Bit
                {   
                    DMX_timeout = 0;//reset timeout if there was a good bit
                    red = temp_r;
                    green = temp_g;
                    blue = temp_b;
                }
                //clear the temporary buffers
                temp_r = 0;
                temp_g = 0;
                temp_b = 0;

                PCON |= 0x04;//set GF0 (general purpose flag 0) Good Packet Bit
                byte_cnt = -1;//this helps us skip over the first byte (start code)
            }
            else//just a frame error, continue with the packet but clear the Good Packet Bit
                PCON &= ~0x04;//clear GF0 (general purpose flag 0)
        }
        else
        {
            if(byte_cnt == -1)//start code
            {   //set the Good Packet Bit
                if(SBUF != 0)//start code not 0
                    PCON &= ~0x04;//clear GF0 (general purpose flag 0)
            }
            if(byte_cnt == address)         temp_r = SBUF;//red channel
            else if(byte_cnt == (address+1))temp_g = SBUF;//green channel
            else if(byte_cnt == (address+2))temp_b = SBUF;//blue channel
            byte_cnt++;
        }
        SCON &= ~0x81;//clear FE (if set) and RI
    }
}

void main(void)
{
    char mode;
    int temp;

    //set output pins
    P3M0 = 0x1C;//set the RGB outputs to push-pull output

    //set timer
	TH0 = (65536 - 2000) / 256;//wait 1ms for first timer interrupt
	TL0 = (65536 - 2000) % 256;
	TMOD = 0x01;
	TCON |= 0x10; // Start timer
    
    //set baud rate to 250,000 baud
    PCON |= 0xC0;//set SMOD and SMOD0, this doubles the baud rate and sets the FE function for SCON
    AUXR |= 0x15;//set BRTR(baud-rate generator enabled) BRTx12 (multiply clock by 1) S1BRS(release Timer1)
    BRT = 250;//256-6   (BAUD = 2/32 * BRToverflow_rate) BRToverflow_rate = 250kbaud*16 = 4Mhz.  With a clock of 24Mhz set to overflow every 6 cycles
    SCON = 0x50;//set to mode 1 and enable receiver (REN)
//    AUXR1 = 0x80;//set to UART on PORT1 - for testing / programming while DMX512 is active - communication to PC is via PORT3
    AUXR1 = 0x00;//set to UART on PORT3 - normal operation

    //enable interrupts
    IE = 0x92;//enable timer interrupts ET0 and ES

    //initialize some variables
    DMX_timeout = 480;//so that system doesn't automatically think there's valid DMX for the first ~1s
    
    while(1)//mainloop
	{
        WDT_CONTR |= 0x10;//feed the watchdog before he get's hungry - approx 4sec
        
        //make sure to invert the inputs since they are grounded when ON
        mode = (P3 & 0x20);//bit P3.5
        temp = ~((P1 << 1) + (P3>>7));//P3.7 is the lowest bit of the address
        temp &= 0x1FF;//only 9 valid bits
//        temp &= ~0x80;//blank out bit7 temporarily because we're using that pin as the DMX input pin for testing
        address = temp;
        if(mode)//DMX MODE
        {
            IE = 0x92;//enable timer interrupts ET0 and ES
            if(DMX_timeout >= 480)//approx 1 sec at 480Hz
            {//clear the outputs if there's no valid DMX for more than a second
                //status LED 
                if(clocktime & 0x200)   P3 |= 0x40;//LED off  //check bit 10 of clocktime for slow blink
                else                    P3 &= ~0x40;//LED on

                DMX_timeout = 480;//keep it set to this value so it doesn't loop back to zero (and flash the status LED)
                red = 0;
                green = 0;
                blue = 0;
            }
            else//flash status LED fast indicating proper DMX operation
            {
                //status LED 
                if(clocktime & 0x40)P3 |= 0x40;//LED off  //check bit 6 of clocktime for fast blink
                else                P3 &= ~0x40;//LED on
            }
        }
        else
        {//FUN MODE
            IE = 0x82;//enable timer interrupt only, no ES interrupt
            P3 &= ~0x40;//LED on fulltime

            if(address&0x0180)//if bit 8 and/or bit 9 are set then in some internal program
            {
                red = 32;
                green = 32;
                blue = 32;
            }
            else
            {
                if(address&0x40)
                {
                    red = 255;
                    green = 255;
                    blue = 255;
                }
                else if(address&0x20)
                {
                    red = 0;
                    green = 255;
                    blue = 255;
                }
                else if(address&0x10)
                {
                    red = 255;
                    green = 0;
                    blue = 255;
                }
                else if(address&0x08)
                {
                    red = 255;
                    green = 255;
                    blue = 0;
                }
                else if(address&0x04)
                {
                    red = 0;
                    green = 0;
                    blue = 255;
                }
                else if(address&0x02)
                {
                    red = 0;
                    green = 255;
                    blue = 0;
                }
                else if(address&0x01)
                {
                    red = 255;
                    green = 0;
                    blue = 0;
                }
                else
                {
                    red = 0;
                    green = 0;
                    blue = 0;
                }
            }
        }
    }
}
