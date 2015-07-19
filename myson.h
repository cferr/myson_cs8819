//Myson definitions

#include <stdlib.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_ATR_SIZE 33
#define ATR_PROTOCOL_TYPE_T0    0       /* Protocol type T=0 */
#define ATR_PROTOCOL_TYPE_T1    1       /* Protocol type T=1 */
#define ATR_PROTOCOL_TYPE_T2    2       /* Protocol type T=2 */
#define ATR_PROTOCOL_TYPE_T3    3       /* Protocol type T=3 */
#define ATR_PROTOCOL_TYPE_T14   14      /* Protocol type T=14 */
#define ATR_PROTOCOL_TYPE_T15   15      /* Protocol type T=15 */
#define ATR_PROTOCOL_TYPE_RAW   16      /* Memory Cards Raw Protocol */

#define IN_ENDPOINT 0x84
#define OUT_ENDPOINT 0x03


typedef struct {
    unsigned char data[MAX_ATR_SIZE];    
    unsigned length;
} ATR;

typedef struct {
   ATR atr;
   int protocol;
   int powerStatus;
   int present;
} card;


typedef struct {
   libusb_device_handle* handle;
   libusb_context *context;
   card card;
} reader;

unsigned char* input(libusb_device_handle* handle, int len, int* act_read);
void output(libusb_device_handle* handle, int len, unsigned char* in_data, bool add_zero, int r_len);

bool testCardPresence(unsigned char* ATR);
void populateAtr(reader* rd);

unsigned char* myson_write_t0(reader* r, int txlength, unsigned char* txbuffer, int* rxlength);
void writeT0Command(libusb_device_handle *handle, unsigned char* txbuffer, int len, int expect);

void print_array(unsigned char* arr, int len);
int min (int a, int b);

void msleep(int ms);