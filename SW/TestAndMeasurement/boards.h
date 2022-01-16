#ifdef USBGPIB
#define GPIB_DATA DDRD
#define GPIB_DATA_PORT PORTD

#define GPIB_ATN_LOW   DDRF |=  (1<<6)
#define GPIB_ATN_HIGH  DDRF &= ~(1<<6)
#define GPIB_ATN_STATE  (PINF & (1<<6))

#define GPIB_NDAC_LOW  DDRC |=  (1<<7)
#define GPIB_NDAC_HIGH DDRC &= ~(1<<7)
#define GPIB_NDAC_STATE (PINC & (1<<7))

#define GPIB_NRFD_LOW  DDRC |=  (1<<6)
#define GPIB_NRFD_HIGH DDRC &= ~(1<<6)
#define GPIB_NRFD_STATE (PINC & (1<<6))

#define GPIB_DAV_LOW   DDRB |=  (1<<6)
#define GPIB_DAV_HIGH  DDRB &= ~(1<<6)
#define GPIB_DAV_STATE  (PINB & (1<<6))

#define GPIB_EOI_LOW   DDRB |=  (1<<4)
#define GPIB_EOI_HIGH  DDRB &= ~(1<<4)
#define GPIB_EOI_STATE  (PINB & (1<<4))

#define GPIB_REN_LOW   DDRB |=  (1<<5)
#define GPIB_REN_HIGH  DDRB &= ~(1<<5)

#define GPIB_IFC_LOW   DDRE |=  (1<<2)
#define GPIB_IFC_HIGH  DDRE &= ~(1<<2)

#endif

#ifdef SUF_V2
#define GPIB_DATA_OUT           DDRB = 0xFF
#define GPIB_DATA_IN            DDRB = 0
#define GPIB_DATA_WRITE         PORTB
#define GPIB_DATA_READ          PINB

#define GPIB_ATN_OUT            DDRD |= (1<<6)
#define GPIB_ATN_IN             DDRD &= ~(1<<6)
#define GPIB_ATN_DIR            ((DDRD & (1<<6))>>6)
#define GPIB_ATN_LOW            PORTD |=  (1<<6)
#define GPIB_ATN_HIGH           PORTD &= ~(1<<6)
#define GPIB_ATN_STATE          (PIND & (1<<6))

#define GPIB_NDAC_OUT           DDRD |=  (1<<5)
#define GPIB_NDAC_IN            DDRD &= ~(1<<5)
#define GPIB_NDAC_LOW           PORTD |=  (1<<5)
#define GPIB_NDAC_HIGH          PORTD &= ~(1<<5)
#define GPIB_NDAC_STATE         (PIND & (1<<5))

#define GPIB_NRFD_OUT           DDRD |=  (1<<1)
#define GPIB_NRFD_IN            DDRD &= ~(1<<1)
#define GPIB_NRFD_LOW           PORTD |=  (1<<1)
#define GPIB_NRFD_HIGH          PORTD &= ~(1<<1)
#define GPIB_NRFD_STATE         (PIND & (1<<1))

#define GPIB_DAV_OUT            DDRD |=  1
#define GPIB_DAV_IN             DDRD &= ~1
#define GPIB_DAV_LOW            PORTD |=  1
#define GPIB_DAV_HIGH           PORTD &= ~1
#define GPIB_DAV_STATE          (PIND & 1)

#define GPIB_EOI_OUT            DDRE |=  (1<<6)
#define GPIB_EOI_IN             DDRE &= ~(1<<6)
#define GPIB_EOI_LOW            PORTE |=  (1<<6)
#define GPIB_EOI_HIGH           PORTE &= ~(1<<6)
#define GPIB_EOI_STATE          (PINE & (1<<6))

#define GPIB_REN_OUT            DDRD |=  (1<<6)
#define GPIB_REN_IN             DDRD &= ~(1<<6)
#define GPIB_REN_LOW            PORTD |=  (1<<6)
#define GPIB_REN_HIGH           PORTD &= ~(1<<6)

#define GPIB_IFC_OUT            DDRD |=  (1<<4)
#define GPIB_IFC_IN             DDRD &= ~(1<<4)
#define GPIB_IFC_LOW            PORTD |=  (1<<4)
#define GPIB_IFC_HIGH           PORTD &= ~(1<<4)

#endif