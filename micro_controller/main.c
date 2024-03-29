#include "main.h"
#include "ports.h"
#include "adc.h"
#include "utils.h"

int main( void );

/* USART Bluetooth initialization */
void USART_bluetooth_init( void );
/* USART Bluetooth send */
void USART_bluetooth_send( unsigned char );
void USART_bluetooth_send_message( char* );
void USART_bluetooth_send_data( uint8_t );
void USART_bluetooth_send_sample( uint8_t, uint8_t );
/* USART Bluetooth receive */
void USART_bluetooth_recv( unsigned char );
/* USART bluetooth check */
void USART_bluetooth_at_version( void );
void USART_bluetooth_at_echo( char );
void USART_bluetooth_at_discover( char );
void USART_bluetooth_at_response( char );
void USART_bluetooth_at_reset( void );
void USART_bluetooth_at_auto( void );
void USART_bluetooth_at_name( char * );
void USART_bluetooth_at_escape( void );
void bluetooth_hardware_start( void );
void bluetooth_hardware_stop( void );
void bluetooth_hardware_reset( void );

/* USART debug initialization */
void USART_debug_init( void );
/* USART debug send */
void USART_debug_send( unsigned char );
/* USART debug send a buffer */
void USART_debug_send_message( char * );
/* USART debug receive */
void USART_debug_recv( unsigned char );

/* Debug matters */
void debug_init( void );
void debug_click( void );
void debug_interrupt( void );
void debug_data( uint8_t );

void change_state( uint8_t );

/*
Some global stuff
*/
/* The buffer for receiving data from the bluetooth module */
char buffer_bluetooth[buffer_length];
/* Buffer iterator */
uint8_t buffer_i;
/* Received data char */
unsigned char recv_byte;

/**
 * ADC_FINISHED_READ
 *
 * Using it for the indication of finished conversion
 */
uint8_t adc_finished_read;

/**
 * ADC_VALUE
 *
 * Global storage of returned ADC value (8bit)
 */
uint8_t adc_value;

/*
ERROR CODES
E1 -- bluetooth buffer overflow
*/

/*
Global variable for state initialization
*/
uint8_t state = 1;
uint8_t usart_debug_lock = 0;
uint8_t usart_bluetooth_lock = 0;

/*
Main function
The place where all the fun happens
*/
int main() {

  change_state(INITIALIZATION_STATE);

  /* Forever alone loop */
  for (;;) {
  }

  /*
  * The whale of misery
  */
  return 1;
}

/**
* Communication with Bluetooth USART initialization
* Configuration: 19200/8-N-1
* TODO: do a better initialization set-up
***/
void USART_bluetooth_init() {

  /* Interrupt on falling edge */
  // MCUCR = (1 << ISC11 ) | (1 << ISC10);

  /* Turn on interrupts */
  // GICR |= (1 << INT1);
  bluetooth_hardware_start();

  /* Enable received and transmitter */
  UCSR0B |= (1<<RXEN0) | (1<<TXEN0);

  /* Enable receive interrupt */
  UCSR0B |= (1 << RXCIE0);
  /* 8 bit configuration */
  UCSR0B |= (0 << UCSZ02);
  /* No double speed option */
  UCSR0A |= (0 << U2X0);

  /* Use 8 bit configuration sizes */
  UCSR0C |= (0 << URSEL0);
  UCSR0C |= (0 << UMSEL0) | (0<<UPM00) | (0<<UPM01) | (1<<UCSZ00) | (1<<UCSZ01);

  // UBRR0H |= (1 << URSEL0);

  /* Set baud rate */
  /* 19200 -- 16Mhz */
  UBRR0H = (25) >> 8;
  UBRR0L = (25);

  d_set_output(BLUETOOTH_CTS_PIN);
  /* Set low -- make the data transfer happen */
  d_output_low(BLUETOOTH_CTS_PIN);

}

/* USART send byte */ 
void USART_bluetooth_send(unsigned char send_byte) {

  /* Wait until the register is ready */
  while ((UCSR0A & (1<<UDRE0)) == 0) {};

  /* Send the byte */
  UDR0 = send_byte;

}

/**
* Bluetooth receive event handler
**/
void USART_bluetooth_recv(unsigned char recv_byte) {
  /* Echo to debug */
  USART_debug_send( recv_byte );
  
  if (state == WAIT_FOR_COMMAND) {
    /*
     * The Device send hello message 
     */
    if (recv_byte == 'H') {
      USART_debug_send_message("H");
      /*
       * Reply with hello 
       */
      USART_bluetooth_send_data(0x48); // [H]ello
    }
    /*
     * The Device send make a sample message
     */
    if (recv_byte == 'S') {
      /*
       * Send back the sample
       * First parameter is the channel number
       * Second paramters is the value 
       */
      
      /**
       * Start the conversion
       */
      ADC_start();
    }
  }

  /* Check if the <cr,>lf bytes are received */
  if (recv_byte == 0x0a) {
    /* Bluetooth returns this after version check */
    if (msgcmp(buffer_bluetooth, "OK", 2) == 1) {
      /* Bluetooth said it is ok, moving to the next state */
      if (state == ERROR_STATE) {
        change_state(BLUETOOTH_CONFIGURE_STATE);
      }
      if (state == BLUETOOTH_CONFIGURE_STATE) {
        change_state(WAIT_FOR_CONNECTION);
      }
      if (state == BLUETOOTH_CHECK_STATE) {
        /* Changing state to wait for bluetooth connection */
        // USART_debug_send_message("OKR");
        change_state(BLUETOOTH_CONFIGURE_STATE);
      }
      if (state == BLUETOOTH_RESET_STATE) {
        change_state(BLUETOOTH_CHECK_STATE);
      }
    }
    if (msgcmp(buffer_bluetooth, "ER", 2) == 1) {
      /*
      If an error is received, switch to error state
      */
      if (state != ERROR_STATE) {
        change_state(ERROR_STATE);
      }
    }
    /* Bluetooth returns connect after successfull conection */
    if (msgcmp(buffer_bluetooth, "CO", 2) == 1) {
      // USART_debug_send_message("COR");
      /* If the system was waiting for connection */
      if (state == WAIT_FOR_CONNECTION) {
        /* Change the state to wait for command */
        if (state != WAIT_FOR_COMMAND) {
          change_state(WAIT_FOR_COMMAND); 
        }
      }
    }
    if (msgcmp(buffer_bluetooth, "DI", 2) == 1) {
      /* 
      On bluetooth disconnect, make the state of the system
      to wait for bluetooth connection
      */
      // USART_debug_send_message("DIR");
      if (state != WAIT_FOR_CONNECTION) {
        change_state(WAIT_FOR_CONNECTION);
      }
    }

    /* Reset the iteration guy */
    buffer_i = 0;
    /* Reseting the buffer */
    memset(buffer_bluetooth, 0, buffer_length);

  } else {

    /* Append receiving byte */
    buffer_bluetooth[buffer_i] = recv_byte;
    /* Check the size of the buffer */
    if (buffer_i < buffer_length) {
      /*
      We still have the space for more bytes
      */
      buffer_i = buffer_i + 1;

    } else {
      /*
      No more spaces are available, need to
      do something about it
      */
      USART_debug_send_message("E1");
      /* Resetting the buffer */
      buffer_i = 0;
      memset(buffer_bluetooth, 0, buffer_length);
      state = 3;
    }
  }
}

/**
* Check if bluetooth is alive
**/
void USART_bluetooth_at_version() {
  /* 
  * AT+VER is a basic query to check
  * if the device is alive 
  */
  USART_bluetooth_send_message("AT+VER");
}

/**
* Echo settings
* Possible options:
* - `+` -- makes echo active
* - `-` -- makes echo not active
**/
void USART_bluetooth_at_echo(char option) {
  char *message;
  /* Check the comming options */
  if (option == '+' || option == '-') {
    message = append("AT+ECHO", option);
    USART_bluetooth_send_message(message);
    free(message);
  }
}

/**
* Discover settings
* Possible options:
* - `+` -- makes discoverable
* - `-` -- makes not discoverable
**/
void USART_bluetooth_at_discover(char option) {
  char *message;
  /* Check the comming options */
  if (option == '+' || option == '-') {
    message = append("AT+DCOV", option);
    USART_bluetooth_send_message(message);
    free(message);
  }
}

/**
* Command response settings
* Possible options:
* - `+` -- makes responses active
* - `-` -- makes responses not active
**/
void USART_bluetooth_at_response(char option) {
  char *message;
  /* Check the comming options */
  if (option == '+' || option == '-') {
    message = append("AT+RESP", option);
    USART_bluetooth_send_message(message);
    free(message);
  }
}

/**
* Complete device reset and return to the 
* hardware settings
**/
void USART_bluetooth_at_reset() {
  USART_bluetooth_send_message("AT+RESET");
}

/**
* Auto command switches from command mode to data mode
**/
void USART_bluetooth_at_auto() {
  USART_bluetooth_send_message("AT+AUTO");
}

/**
* Command using to switch from data mode to command mode
**/
void USART_bluetooth_at_escape() {
  USART_bluetooth_send_message("+++");
}

/**
* USART bluetooth AT name transfer
***/
void USART_bluetooth_at_name( char * name ) {
  char *message;
  message = append_long("AT+NAME=", name);
  USART_bluetooth_send_message(message);
  free(message);
}

/**
* Bluetooth hardware start
***/
void bluetooth_hardware_start() {
  d_set_output(BLUETOOTH_RESET_PIN);
  d_output_high(BLUETOOTH_RESET_PIN);
}

/**
* Bluetooth hardware stop
***/
void bluetooth_hardware_stop() {
  d_output_low(BLUETOOTH_RESET_PIN);
}

/**
* Bluetooth hardware reset
***/
void bluetooth_hardware_reset() {
  bluetooth_hardware_stop();
  bluetooth_hardware_start();
}
/**
* Sending and receiving packages from the Bluetooth
**/
void USART_bluetooth_send_message(char *message) {

  while (usart_bluetooth_lock == 1) {;}
  usart_bluetooth_lock = 1;
  uint8_t i = 0;
  while(message[i] != 0) {
    /* Send the require characters */
    USART_bluetooth_send(message[i]);
    i = i + 1;
  }
  // free(message);
  /* End the message with cr */
  USART_bluetooth_send(0x0d);
  usart_bluetooth_lock = 0;
  /* FLUSH */
  // USART_bluetooth_send(0x00);

}

void USART_bluetooth_send_data( uint8_t data ) {
  /* Send a plain data */
  USART_bluetooth_send(data);
  /* Dont end message with anything */
}

void USART_bluetooth_send_sample(uint8_t channel, uint8_t data) {
  char message[3];
  /**
   * Channel numbers are from 0 to 15
   */
  message[0] = channel;
  message[1] = data;
  
  /**
   * End of string
   */
  message[2] = '\0';

  /**
   * Send the data
   */
  USART_bluetooth_send_message(message);
}


/**
* Communication with debug USART initialization
* Configuration: 19200/8-N-1
* TODO: do a better configuration set-up
***/
void USART_debug_init() {
  
  /* Enable received and transmitter */
  UCSR1B = (1<<RXEN1)|(1<<TXEN1);

  /* Enable receive interrupt */
  UCSR1B |= (1 << RXCIE1);
  UCSR1B |= (0 << UCSZ12);

  /* Normal asynchronous mode */
  UCSR1A |= (0 << U2X1);
  /* Select */
  UCSR1C = (1 << URSEL1) | (0 << UMSEL1) | (0<<UPM10) | (0<<UPM11) | (1<<UCSZ10) | (1<<UCSZ11);

  /* Asynchronous mode */
  // UCSR1C |= (0 << UMSEL1);
  /* Parity mode -- disabled */
  // UCSR1C |= (0 << UPM10);
  // UCSR1C |= (0 << UPM11);
  /* Stop bit -- 1 bit */
  // UCSR1C |= (1 << USBS1);
  /* Use 8bit character sizes */
  // UCSR1C |= (1<<UCSZ10) | (1<<UCSZ11);
  // UBRR1H |= (1 << URSEL1);

  /* Set baud rate */
  /* 19200 -- 8Mhz */
  UBRR1H = (25) >> 8;
  UBRR1L = (25);

}

void USART_debug_send( unsigned char send_byte ) {

  /* Wait until USART1 is ready */
  while (( UCSR1A & (1<<UDRE1)) == 0) {};

  /* Send the byte */
  UDR1 = send_byte;

}

/**
* Sending the received package from the debug
***/
void USART_debug_send_message(char *message) {
  while(usart_debug_lock == 1) {;}
  usart_debug_lock = 1;
  /* Request the line */
  d_output_low( BLUETOOTH_CTS_PIN );
  uint8_t i = 0;
  while (message[i] != '\0') {
    USART_debug_send(message[i]);
    i = i + 1;
  }
  /* END message with cr,lf */
  USART_debug_send(0x0d);
  USART_debug_send(0x0a);
  usart_debug_lock = 0;
}

void USART_debug_recv( unsigned char recv_byte ) {

  /* Echo to the debug port */
  USART_debug_send( recv_byte );
}

/* Events */

/* USART bluetooth receive event */
ISR ( USART0_RXC_vect ) {
  recv_byte = UDR0;
  USART_bluetooth_recv(recv_byte);
}

/* USART debug receive event */
ISR ( USART1_RXC_vect ) {
  recv_byte = UDR1;
  USART_debug_recv( recv_byte );
}

/**
* Interrupts on ADC finish 
**/
ISR ( ADC_INTERRUPT_INT_VECTOR ) {
  adc_value = ADC_read();
  /**
   * Pass the argument 
   */
  USART_bluetooth_send_sample(0x01, adc_value);
}

/**
* RTS line request
**/
ISR( BLUETOOTH_RTS_INTERRUPT_VECTOR ) {
  d_output_low( BLUETOOTH_CTS_PIN );
}

/**
* Change state
* _state -- in which state the system happens to be
* TODO:
* - Change the interrupts accordingly
**/
void change_state(uint8_t _state) {
  /**
   * Apply to the global variable
   */
  state = _state;
  /*
  Check if the system passed state 1,
  then the all communications was established
  */
  if (_state != 1) {
    char state_change_message[3];
    itoa(_state, state_change_message, 10);
  }
  switch( _state ) {
    case INITIALIZATION_STATE:
      USART_debug_send_message("I");
      /* Initial system state -- setting up all the things */
      delay_ms(100);
      /* Set clock prescale */
      clock_prescale_set(1);
      /* USART bluetooth initialization */
      USART_bluetooth_init();
      /* USART debug intitialization */
      USART_debug_init();
      /* External ADC initialization */
      ADC_init();
      /* Enable all the interrupts */
      sei();
      /* Debugging utilities init */
      debug_init();
      /**
       * Time out wait
       */
      delay_ms(100);
      /* Set up the system for the bluetooth check */
      change_state(DEBUG_STATE);
      break;
    case BLUETOOTH_CHECK_STATE:
      USART_debug_send_message("C");
      /* Bluetooth check state */
      USART_bluetooth_at_version();
      break;
    case BLUETOOTH_CONFIGURE_STATE:
      /* Set the name */
      USART_bluetooth_at_name("patric");
      break;
    case ERROR_STATE:
      USART_debug_send_message("E");
      /**
       * First BLUETOOTH ERROR STATE
       *
       * Try to reset it
       */
      /**
       * Reset the bluetooth via hardware signal
       */
      bluetooth_hardware_reset();
      /**
       * Wait for bluetooth a little
       */
      delay_ms(100);
      /**
       * Change to the bluetooth check state
       */
      change_state(BLUETOOTH_CHECK_STATE);
      break;
    case WAIT_FOR_CONNECTION:
      USART_debug_send_message("W1");
      /* Switch to command mode */
      // USART_bluetooth_at_escape();
      /* Waiting for the bluetooth connection from the host */
      break;
    case WAIT_FOR_COMMAND:
      /* Switch to data mode */
      // USART_bluetooth_at_auto();
      /* Waiting for command from the bluetooth host */
      /* Send a test data package */
      USART_debug_send_message("W2");
      break;
    case DEBUG_STATE:
      /**
       * Send reset
       */
      // bluetooth_hardware_reset();
      /**
       * Wait for it
       */
      USART_bluetooth_send_message("+++");
      break;
    case BLUETOOTH_RESET_STATE:
      bluetooth_hardware_reset();
      change_state(BLUETOOTH_CHECK_STATE);
      break;
  }
}

/**
* Delay utility
**/
void delay_ms( uint8_t ms ) {
  uint16_t delay_count = F_CPU / 14500;

  volatile uint16_t i;

  while ( ms != 0 ) {
    for (i=0; i != delay_count; i++ );
    ms--;
  }
}

/**
* Initialization of debug pins
**/
void debug_init() {
  
  /* Set debug output pin as output */
  b_set_output( DEBUG_OUTPUT_PIN );
  /* Set debug interrupt pin as output */
  b_set_output( DEBUG_INTERRUPT_PIN );
  /* Set debug data pin as output */
  b_set_output( DEBUG_DATA_PIN );

  /* Set debug output pin as low */
  b_output_low( DEBUG_OUTPUT_PIN );
  /* Set debug interrupt pin as low */
  b_output_low( DEBUG_INTERRUPT_PIN );
  /* Set debug data pin as low */
  b_output_low( DEBUG_DATA_PIN );
}

/**
* Makes debug pin `interrupt`
**/
void debug_interrupt() {
  b_output_low( DEBUG_INTERRUPT_PIN );
  b_output_high( DEBUG_INTERRUPT_PIN );
  delay_ms(10);
  b_output_low( DEBUG_INTERRUPT_PIN );
}

/**
* Sets the debug pin to low - high - low
* Indication of the step was passed
**/
void debug_click() {
  b_output_low( DEBUG_OUTPUT_PIN );
  b_output_high( DEBUG_OUTPUT_PIN );
}

/**
* Debug data, coming from the ADC
**/
void debug_data( uint8_t data ) {
  
  /* Mask */
  int8_t mask = 0;
  /* Masked data */
  int8_t masked_data = 0;
  /* Loop variable */
  uint8_t i = 0;

  for (i=0; i < 8; i++) {
    mask = 1 << i;
    masked_data = data & mask;
    if (masked_data >> i == 1) {
      b_output_high( DEBUG_DATA_PIN );
    } else {
      b_output_low( DEBUG_DATA_PIN );
    }
    delay_ms(100);
    b_output_low( DEBUG_DATA_PIN );
    delay_ms(100);
  }

  b_output_low( DEBUG_DATA_PIN );
  delay_ms(100);
  b_output_high( DEBUG_DATA_PIN );
  delay_ms(100);
  b_output_low( DEBUG_DATA_PIN );
}