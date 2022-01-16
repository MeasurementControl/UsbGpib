
#include "boards.h"
#include "gpib_pin.h"
#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define GPIB_DEVICE_CONNECTSTATE_UNKNOWN      (0)
#define GPIB_DEVICE_CONNECTSTATE_DISCONNECTED (1)
#define GPIB_DEVICE_CONNECTSTATE_CONNECTED    (2)


/**********************************************************************************************************
 * STATIC functions
 **********************************************************************************************************/
static uint8_t  volatile timer0_100mscounter;
static uint8_t  timer0_div;
static uint8_t  s_device_state = GPIB_DEVICE_CONNECTSTATE_UNKNOWN;
static uint8_t s_gpib_disconnect_counter;
static volatile bool     s_gpib_transaction_active = false; /* TRUE, if a device is addressed as talker or listener */

static char s_terminator = '\0'; /* \0 = no termination character - EOI only, other options are '\n' or '\r' */
 
static void gpib_recover(void)
{
	gpib_init();
}

static bool gpib_tx(uint8_t dat, bool iscommand, gpibtimeout_t ptimeoutfunc)
{
	bool timedout;
	
	GPIB_ATN_OUT;
	GPIB_DAV_OUT;
	GPIB_NRFD_OUT;
	GPIB_NDAC_OUT;
	GPIB_NRFD_IN;
	GPIB_DATA_OUT;
	GPIB_DAV_HIGH;
	GPIB_NRFD_HIGH;
	GPIB_NDAC_HIGH;  /* they should be already high, but let's enforce it */
	
	if (iscommand)
		GPIB_ATN_LOW;
	else
		GPIB_ATN_HIGH;

	GPIB_DATA_WRITE = dat;   /* set Data to data bus */
	_delay_us(1); /* wait for data to settle */
		
	/* wait until ready for data acceptance (NRFD=H, NDAC=L)*/
	do
	{
		timedout = ptimeoutfunc();
	}
	while ( (GPIB_NRFD_STATE == 0) && !timedout); /* wait until ready for data (NRFD to get high) */

	if (!timedout)
	{
		GPIB_DAV_LOW;
		do
		{
			timedout = ptimeoutfunc();
		}
		while ( (GPIB_NDAC_STATE == 0) && !timedout ); /* wait until NDAC gets high*/
		GPIB_DAV_HIGH; 
	}
	
	GPIB_DATA_IN; /* release data bus */
	GPIB_ATN_HIGH;	 
	
	if (timedout)
	{
		gpib_recover();
	}
	return timedout;
}

static bool gpib_dat(uint8_t dat, gpibtimeout_t ptimeoutfunc)
{
	return gpib_tx(dat, false, ptimeoutfunc);
}

// Listener address
static bool gpib_cmd_LAG(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool result;
	result = gpib_tx((addr & 0x1f) | 0x20, true, ptimeoutfunc);
	if (addr & 0xe0)
	{ /* send a secondary address? */
		result = gpib_tx(0x60, true, ptimeoutfunc);        // SAG (SA0)
	}
	return result;
}

static bool gpib_cmd_SECADDR(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	return gpib_tx(addr | 0x60, true, ptimeoutfunc);
}

// Talk address
static bool gpib_cmd_TAG(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool result;
	result = gpib_tx((addr & 0x1f) | 0x40, true, ptimeoutfunc);
	if (addr & 0xe0)
	{ /* send a secondary address? */
		result = gpib_tx(0x60, true, ptimeoutfunc);        // SAG (SA0)	
	}
	return result;
}

static bool gpib_cmd_UNL(gpibtimeout_t ptimeoutfunc)
{
	return gpib_tx(0x3F, true, ptimeoutfunc);
}

static bool gpib_cmd_UNT(gpibtimeout_t ptimeoutfunc)
{
	return gpib_tx(0x5F, true, ptimeoutfunc);
}

static bool gpib_cmd_LLO(gpibtimeout_t ptimeoutfunc) // local lockout
{
	return gpib_tx(0x11, true, ptimeoutfunc);
}

static bool gpib_cmd_GTL(gpibtimeout_t ptimeoutfunc) // goto local
{
	return gpib_tx(0x01, true, ptimeoutfunc);
}


static bool gpib_cmd_SPE(gpibtimeout_t ptimeoutfunc) // serial poll enable
{
	return gpib_tx(0x18, true, ptimeoutfunc);
}

static bool gpib_cmd_SPD(gpibtimeout_t ptimeoutfunc) // serial poll disable
{
	return gpib_tx(0x19, true, ptimeoutfunc);
}

static bool gpib_cmd_GET(gpibtimeout_t ptimeoutfunc) // group execute trigger (addressed command)
{
	return gpib_tx(0x08, true, ptimeoutfunc);
}


uint8_t gpib_readStatusByte(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool timedout, eoi;
	uint8_t status;
	
	timedout = false;
	status = 0;
	
	if (!timedout)
		timedout = gpib_cmd_SPE(ptimeoutfunc);
	if (!timedout)
		timedout = gpib_cmd_TAG(addr, ptimeoutfunc);
	GPIB_ATN_OUT;
	GPIB_NDAC_OUT;
	GPIB_ATN_HIGH; /* make ATN H */	
	GPIB_NDAC_LOW;   /* make NDAC L */

	if (!timedout)
		status = gpib_readdat(&eoi, &timedout, ptimeoutfunc);
	
	if (!timedout)
		timedout = gpib_cmd_UNT(ptimeoutfunc); 
	if (!timedout)
		timedout = gpib_cmd_SPD(ptimeoutfunc);
	if (timedout)
		gpib_recover();	
	return status;
}

bool gpib_localLockout(gpibtimeout_t ptimeoutfunc)
{
	bool timedout;

	timedout = gpib_cmd_LLO(ptimeoutfunc);
	if (timedout)
		gpib_recover();
	return timedout;
}

bool gpib_gotoLocal(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool timedout;
	
	timedout = gpib_cmd_LAG(addr, ptimeoutfunc); 
	if (!timedout)
		timedout = gpib_cmd_GTL(ptimeoutfunc);
		
	if (!timedout)
		timedout = gpib_cmd_UNL(ptimeoutfunc);
		
	if (timedout)
		gpib_recover();

}


bool gpib_trigger(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool timedout;
	
	timedout = gpib_cmd_LAG(addr, ptimeoutfunc); 
	if (!timedout)
		timedout = gpib_cmd_GET(ptimeoutfunc);
	if (!timedout)
		timedout = gpib_cmd_UNL(ptimeoutfunc);
		
	if (timedout)
		gpib_recover();
	return timedout;
}


/*
static bool gpib_cmd_DCL(gpibtimeout_t ptimeoutfunc) // device clear
{
	return gpib_tx(0x14, true, ptimeoutfunc);
}

static bool gpib_cmd_SDC(gpibtimeout_t ptimeoutfunc) // selective device clear
{
	return gpib_tx(0x04, true, ptimeoutfunc);
}
*/

static void timer_init(void)
{
	TCCR0B = 5; // Prescaler 1024 = 15625 Hz
	// Overflow Interrupt erlauben
	TIMSK0 |= (1<<TOIE0);
	timer0_div = 0;
	timer0_100mscounter = 0;
}


ISR (TIMER0_OVF_vect)
{
	// Save ATN direction
	bool attdir = GPIB_ATN_DIR;
	timer0_div++;
	if (timer0_div >= 6) /* are 100ms passed? */
	{
		timer0_100mscounter++;
		timer0_div = 0;
		
		if (!s_gpib_transaction_active) /* only check, if no GPIB transaction is active */
		{
			GPIB_ATN_IN;
			if (!GPIB_ATN_STATE) /* is ATN LOW? This can only happen if no GPIB device is connected/powered */
			{
				if (s_gpib_disconnect_counter == 2)
				{ /* after 100-200ms with ATN low, assume, that there is no GPIB device connected */
					s_device_state = GPIB_DEVICE_CONNECTSTATE_DISCONNECTED;
				}
				else
				{
					s_gpib_disconnect_counter++;
				}
			}
			else
			{ /* device is connected */
				s_gpib_disconnect_counter = 0;
				s_device_state = GPIB_DEVICE_CONNECTSTATE_CONNECTED;
			}
		}
	}
	// Restore ATN direction
	if (attdir)
	{
		GPIB_ATN_OUT;
	}
	else
	{
		GPIB_ATN_IN;
	}
}



/**********************************************************************************************************
 * global API functions
 **********************************************************************************************************/

void gpib_init(void)
{

	GPIB_DATA_IN;
	GPIB_DATA_WRITE = 0x00;	// Disable PullUps

	GPIB_ATN_IN;
	GPIB_ATN_LOW; // Disable PullUp
	GPIB_NDAC_IN;
	GPIB_NDAC_LOW; // Disable PullUp
	GPIB_NRFD_IN;
	GPIB_NRFD_LOW; // Disable PullUp
	GPIB_DAV_IN;
	GPIB_DAV_LOW; // Disable PullUp
	GPIB_EOI_IN;
	GPIB_EOI_LOW; // Disable PullUp
	// System Controller function - check how to handle?
	GPIB_IFC_OUT;
	GPIB_IFC_LOW; // Interface clear - off
	GPIB_REN_OUT;
	GPIB_REN_HIGH; // Remote enable

	s_gpib_transaction_active = false;
	s_gpib_disconnect_counter = 0;
	
	gpib_interface_clear();
	timer_init(); /* init timeout timer */
}

bool gpib_is_connected(void)
{
	return s_device_state == GPIB_DEVICE_CONNECTSTATE_CONNECTED;
}

void gpib_ren(bool enable)
{
	if (enable)
	{
		GPIB_REN_LOW; /* remote enable */
	}
	else
	{
		GPIB_REN_HIGH; /* remote disable */
	}
}

void gpib_interface_clear(void)
{
	GPIB_IFC_LOW; /* interface clear */
	_delay_ms(100);
	GPIB_IFC_HIGH; /* interface clear */
	_delay_ms(10);
	s_gpib_transaction_active = false;
}


uint8_t gpib_readdat(bool *pEoi, bool *ptimedout, gpibtimeout_t ptimeoutfunc)
{
	uint8_t c;
	bool eoi, timedout;	
	
	c = 0;
	eoi = false;	
	
	GPIB_EOI_IN;
	GPID_DAV_IN;
	GPID_NDAC_OUT;
	GPID_NRFD_OUT;

	/* skipping NRFD LOW step, because we are able to handshake and response to data */
	NDAC_LOW;
	NRFD_HIGH;
	
	do
	{
		timedout = ptimeoutfunc();
	}
	while ( (GPIB_DAV_STATE != 0) && !timedout ); /* wait until DAV gets low */
	
	if (!timedout)
	{
		GPIB_NRFD_LOW;
		c = ~GPIB_DATA_READ;
		eoi = (GPIB_EOI_STATE == 0) ;
		GPIB_NDAC_HIGH;
		
		do
		{
			timedout = ptimeoutfunc();
		}
		while ( (GPIB_DAV_STATE == 0) && !timedout ); /* wait until DAV gets high */
	}
	
	if (s_terminator == '\0')
		*pEoi = eoi;
	else
		*pEoi = eoi || (c == s_terminator);

	if (timedout)
	{
		gpib_recover();
	}
	*ptimedout = timedout;
	return c;
};

bool gpib_untalk_unlisten(gpibtimeout_t ptimeoutfunc)
{
	bool timedout;	
	timedout = gpib_cmd_UNL(ptimeoutfunc);
	if (!timedout)
		timedout = gpib_cmd_UNT(ptimeoutfunc);
	if (timedout)
		gpib_recover();
		
	s_gpib_transaction_active = false;
	return timedout;
}

bool  gpib_make_talker(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool timedout;
	
	s_gpib_transaction_active = true;
	
	timedout = gpib_cmd_UNL(ptimeoutfunc);
	if (!timedout)
		timedout = gpib_cmd_TAG(addr, ptimeoutfunc); /* address as talker*/
	GPIB_ATT_OUT;
	GPIB_NDAC_OUT;
	GPIB_ATN_HIGH; /* make ATN H */	
	GPIB_NDAC_LOW;   /* make NDAC L */
	
	if (timedout)
		gpib_recover();
	return timedout;
}

bool gpib_make_listener(uint8_t addr, gpibtimeout_t ptimeoutfunc)
{
	bool timedout;
	s_gpib_transaction_active = true;
	timedout = gpib_cmd_UNT(ptimeoutfunc);
	if (!timedout)
		timedout = gpib_cmd_UNL(ptimeoutfunc);
	if (!timedout)
		timedout = gpib_cmd_LAG(addr, ptimeoutfunc); /* address target as listener*/
		
	GPIB_ATT_OUT;
	GPIB_ATN_HIGH;    /* make ATN H */
	
	if (timedout)
		gpib_recover();
	return timedout;
}


bool gpib_writedat(uint8_t dat, bool Eoi, gpibtimeout_t ptimeoutfunc)
{
	bool timedout;
	GPIB_EOI_OUT;
	if (Eoi)
	{
		EOI_LOW; /* make EOI L */
	}
	timedout = gpib_dat(dat, ptimeoutfunc);
	GPIB_EOI_HIGH;    /* make EOI H */
	return timedout;
}

void gpib_set_readtermination(char terminator)
{
	switch(terminator)
	{
		case '\n':
			s_terminator = '\n';
			break;
		case '\r':
			s_terminator = '\r';
			break;
		default:
			s_terminator = '\0';
			break;
	}
}


static uint16_t timeout_val;

static void timeout_start(uint16_t timeout)
{
	timeout_val = timeout;
}

static bool is_timedout(void)
{
	_delay_us(10);
	if (timeout_val == 0)
		return true;
		
	timeout_val--;
	return false;
}

uint8_t gpib_search(void)
{
	uint8_t addr, foundaddr;
	
	timeout_start(500);
	gpib_tx(0x3F, true, is_timedout); // UNL
	
	GPIB_NDAC_IN;
	foundaddr = 255;
	addr = 255;
	do
	{
	
		addr++;
		if ((addr & 0x1f) != 31)
		{
			timeout_start(500);
			gpib_cmd_LAG(addr, is_timedout);
			
			GPIB_ATN_OUT;
			GPIB_ATN_HIGH; /* make ATN H */
			_delay_ms(2);
			GPIB_ATN_IN
			if ( (GPIB_NDAC_STATE == 0) && (GPIB_ATN_STATE != 0))
			{
				foundaddr = addr;
			}
		}
		
	}
	while ( (addr < 63) && (foundaddr == 255));
	
	timeout_start(500);
	gpib_tx(0x3F, true, is_timedout); // UNL
	
	/* if the device needs a secondary address, ensure, that it really cannot be addressed without secondary address */
	if (addr >= 32)
	{
		/* address once without SA. If it responds, force it to this primary addressing only! */
		gpib_cmd_LAG(addr & 0x1f, is_timedout);
		GPIB_ATN_OUT;
		ATN_HIGH; /* make ATN H */
		_delay_ms(2);
		GPIB_ATN_IN;
		if ( (GPIB_NDAC_STATE == 0) && (GPIB_ATN_STATE != 0))
		{
			foundaddr = addr & 0x1f;
		}
		timeout_start(500);
		gpib_tx(0x3F, true, is_timedout); // UNL
	}
	
	//return 1;
		
	return foundaddr;
}
