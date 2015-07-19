#include <ifdhandler.h>
#include <pcsclite.h>
#include <reader.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "myson.h"


#define MAX_READER_NUM  16

static reader rd[MAX_READER_NUM];

static void closeDriver ( DWORD Lun ) {
  int readerNum = (Lun & 0xFFFF0000) >> 16;
  libusb_attach_kernel_driver(rd[readerNum].handle,1);
  libusb_close(rd[readerNum].handle);
  libusb_exit(rd[readerNum].context);
}

RESPONSECODE IFDHCreateChannelByName ( DWORD Lun, LPSTR DeviceName )
{
  return IFDHCreateChannel(Lun, 0);
}

RESPONSECODE IFDHCreateChannel ( DWORD Lun, DWORD Channel ) {

    //RESPONSECODE IO_Create_Channel ( DWORD Channel ) {
    //DWORD Lun = 0;

  /* Lun - Logical Unit Number, use this for multiple card slots 
     or multiple readers. 0xXXXXYYYY -  XXXX multiple readers,
     YYYY multiple slots. The resource manager will set these 
     automatically.  By default the resource manager loads a new
     instance of the driver so if your reader does not have more than
     one smartcard slot then ignore the Lun in all the functions.
     Future versions of PC/SC might support loading multiple readers
     through one instance of the driver in which XXXX would be important
     to implement if you want this.
  */
  
  /* Channel - Channel ID.  This is denoted by the following:
     0x000001 - /dev/pcsc/1
     0x000002 - /dev/pcsc/2
     0x000003 - /dev/pcsc/3
     
     USB readers may choose to ignore this parameter and query 
     the bus for the particular reader.
  */

  /* This function is required to open a communications channel to the 
     port listed by Channel.  For example, the first serial reader on COM1 would
     link to /dev/pcsc/1 which would be a sym link to /dev/ttyS0 on some machines
     This is used to help with intermachine independance.
     
     Once the channel is opened the reader must be in a state in which it is possible
     to query IFDHICCPresence() for card status.
 
     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
  */

  syslog(LOG_INFO, "This is the Myson driver");
  srand(time(0));
  
  int readerNum = (Lun & 0xFFFF0000) >> 16;
  
  libusb_context *context;
  if(libusb_init(&context) != 0) //unable to initialize libusb
  {
    syslog(LOG_INFO, "Unable to initialize libusb");
    return IFD_COMMUNICATION_ERROR;
  }
  rd[readerNum].context = context;
  
  rd[readerNum].handle = libusb_open_device_with_vid_pid(context, 0x04cf, 0x9920);
  syslog(LOG_INFO, "Success");
  
  if(rd[readerNum].handle == NULL)
  {
    syslog(LOG_INFO, "Did you connect the Myson?");
    return IFD_COMMUNICATION_ERROR;
  }
  
  libusb_device* dev = libusb_get_device(rd[readerNum].handle);
  
  //avoid conflict with an existing mass storage driver
  //this function, although it is in the documentation, is absent from libusb.h...
  //libusb_set_auto_detach_kernel_driver(rd[readerNum].handle,1);
  
  //we will claim the interface
  if(libusb_kernel_driver_active(rd[readerNum].handle,0) == 1) //then we free it
  {
    if(libusb_detach_kernel_driver(rd[readerNum].handle,0) != 0) //error when freeing?
    {
      syslog(LOG_INFO, "Unable to detach interface from kernel driver");
      libusb_close(rd[readerNum].handle);
      libusb_exit(context);
      return IFD_COMMUNICATION_ERROR;
    }
  }
  if(libusb_claim_interface(rd[readerNum].handle, 0) != 0)
  {
    syslog(LOG_INFO, "Unable to claim interface");
    libusb_close(rd[readerNum].handle);
    libusb_exit(context);
    return IFD_COMMUNICATION_ERROR;
  }

  syslog(LOG_INFO, "Myson successfully initialized");
  int maxsize = libusb_get_max_packet_size(dev, IN_ENDPOINT);
  printf("Max IN packet size: %d\n", maxsize);
  maxsize = libusb_get_max_packet_size(dev, OUT_ENDPOINT);
  printf("Max OUT packet size: %d\n", maxsize);
  
  return IFD_SUCCESS;
}


RESPONSECODE IFDHCloseChannel ( DWORD Lun ) {
  
  /* This function should close the reader communication channel
     for the particular reader.  Prior to closing the communication channel
     the reader should make sure the card is powered down and the terminal
     is also powered down.

     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR     
  */
  /* TODO: power down the card */
  
  //int readerNum = (Lun & 0xFFFF0000) >> 16;
  
  closeDriver(Lun);
  
  return IFD_SUCCESS;
}


RESPONSECODE IFDHGetCapabilities ( DWORD Lun, DWORD Tag, PDWORD Length, PUCHAR Value ) {
  
  /* This function should get the slot/card capabilities for a particular
     slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
     loading a new driver for each reader then ignore Lun.

     Tag - the tag for the information requested
         example: TAG_IFD_ATR Return the ATR and its size (implementation is mandatory). Done.
         
               TAG_IFD_SLOTNUM Unused/deprecated. Unimplemented.
               
               SCARD_ATTR_ATR_STRING Same as TAG_IFD_ATR but this one is not mandatory. 
               It is defined in Microsoft PC/SC SCardGetAttrib(). Done.
               
               TAG_IFD_SIMULTANEOUS_ACCESS Return the number of sessions (readers) the driver can handle in Value[0]. 
               This is used for multiple readers sharing the same driver. Done.
               
               TAG_IFD_THREAD_SAFE If the driver supports more than one reader (see TAG_IFD_SIMULTANEOUS_ACCESS above) 
               this tag indicates if the driver supports access to multiple readers at the same time.
               Value[0] = 1 indicates the driver supports simultaneous accesses. Unimplemented.
               
               TAG_IFD_SLOTS_NUMBER Return the number of slots in this reader in Value[0]. Done.
               
               TAG_IFD_SLOT_THREAD_SAFE If the reader has more than one slot (see TAG_IFD_SLOTS_NUMBER above) 
               this tag indicates if the driver supports access to multiple slots of the same reader at the same time.
               Value[0] = 1 indicates the driver supports simultaneous slot accesses. Unimplemented.
               
               TAG_IFD_POLLING_THREAD Unused/deprecated. Unimplemented.
               
               TAG_IFD_POLLING_THREAD_WITH_TIMEOUT If the driver provides a polling thread then Value is a pointer 
               to this function. The function prototype is:
                  RESPONSECODE foo(DWORD Lun, int timeout);
               Unimplemented.

               TAG_IFD_POLLING_THREAD_KILLABLE Tell if the polling thread can be killed (pthread_kill()) by pcscd. Unimplemented.
               
               TAG_IFD_STOP_POLLING_THREAD Returns a pointer in Value to the function used to stop the polling thread 
               returned by TAG_IFD_POLLING_THREAD_WITH_TIMEOUT. The function prototype is:
                  RESPONSECODE foo(DWORD Lun);
               Unimplemented.
               
         these tags are defined in ifdhandler.h

     Length - the length of the returned data
     Value  - the value of the data

     returns:
     
     IFD_SUCCESS
     IFD_ERROR_TAG
  */
  
   int readerNum = (Lun & 0xFFFF0000) >> 16;
    switch (Tag) {
      case TAG_IFD_ATR:
         if (rd[readerNum].card.atr.length > 0) {
            *Length = rd[readerNum].card.atr.length;
            memcpy(Value, rd[readerNum].card.atr.data, *Length);
         }
         break;
      case TAG_IFD_SIMULTANEOUS_ACCESS:
         *Length = 1; 
         *Value = 1; //we can only handle a single card reader for now
         break;
      case TAG_IFD_SLOTS_NUMBER:
         *Length = 1; 
         *Value = 1; //single slot card reader
         break;
      case SCARD_ATTR_MAXINPUT:
         *Length = 1;
         *Value = 255;
      default:
         return IFD_ERROR_TAG;
    }
    return IFD_SUCCESS;
}


RESPONSECODE IFDHSetCapabilities ( DWORD Lun, DWORD Tag, DWORD Length, PUCHAR Value ) {

  /* This function should set the slot/card capabilities for a particular
     slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
     loading a new driver for each reader then ignore Lun.

     Tag - the tag for the information needing set

     Length - the length of the returned data
     Value  - the value of the data

     returns:
     
     IFD_SUCCESS
     IFD_ERROR_TAG
     IFD_ERROR_SET_FAILURE
     IFD_ERROR_VALUE_READ_ONLY
  */

 // int readerNum = (Lun & 0xFFFF0000) >> 16;
  return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHSetProtocolParameters ( DWORD Lun, DWORD Protocol, UCHAR Flags, UCHAR PTS1, UCHAR PTS2, UCHAR PTS3) {

  /* This function should set the PTS of a particular card/slot using
     the three PTS parameters sent

     Protocol  - 0 .... 14  T=0 .... T=14
     Flags     - Logical OR of possible values:
     IFD_NEGOTIATE_PTS1 IFD_NEGOTIATE_PTS2 IFD_NEGOTIATE_PTS3
     to determine which PTS values to negotiate.
     PTS1,PTS2,PTS3 - PTS Values.

     returns:

     IFD_SUCCESS
     IFD_ERROR_PTS_FAILURE
     IFD_COMMUNICATION_ERROR
     IFD_PROTOCOL_NOT_SUPPORTED
  */

    int readerNum = (Lun & 0xFFFF0000) >> 16;
    
    if (Flags & IFD_NEGOTIATE_PTS1 || Flags & IFD_NEGOTIATE_PTS2 || Flags & IFD_NEGOTIATE_PTS3)
    {
      unsigned char pts [3] = { PTS1, PTS2, PTS3 };
      printf("PPS challenge: ");
      print_array(pts, 3);
      output(rd[readerNum].handle, 3, pts, true, 3);
      int act_read = 0;
      unsigned char* resp = input(rd[readerNum].handle, 3, &act_read);
      printf("PPS response: ");
      print_array(resp, 3);
      free(resp);
    }
    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHPowerICC ( DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD AtrLength ) {

  /* This function controls the power and reset signals of the smartcard reader
     at the particular reader/slot specified by Lun.

     Action - Action to be taken on the card.

     IFD_POWER_UP - Power and reset the card if not done so 
     (store the ATR and return it and it's length).
 
     IFD_POWER_DOWN - Power down the card if not done already 
     (Atr/AtrLength should
     be zero'd)
 
    IFD_RESET - Perform a quick reset on the card.  If the card is not powered
     power up the card.  (Store and return the Atr/Length)

     Atr - Answer to Reset of the card.  The driver is responsible for caching
     this value in case IFDHGetCapabilities is called requesting the ATR and it's
     length.  This should not exceed MAX_ATR_SIZE.

     AtrLength - Length of the Atr.  This should not exceed MAX_ATR_SIZE.

     Notes:

     Memory cards without an ATR should return IFD_SUCCESS on reset
     but the Atr should be zero'd and the length should be zero

     Reset errors should return zero for the AtrLength and return 
     IFD_ERROR_POWER_ACTION.

     returns:

     IFD_SUCCESS
     IFD_ERROR_POWER_ACTION
     IFD_COMMUNICATION_ERROR
     IFD_NOT_SUPPORTED
  */
  int readerNum = (Lun & 0xFFFF0000) >> 16;
  switch(Action)
  {
     case IFD_RESET:
        populateAtr(&(rd[readerNum]));
        //there's no break here: intentional!
     case IFD_POWER_UP:
        /* ATR should already be set */
        *AtrLength = rd[readerNum].card.atr.length;
        memcpy(Atr, rd[readerNum].card.atr.data, *AtrLength);
        rd[readerNum].card.powerStatus = 1;
        break;
        
     case IFD_POWER_DOWN:
        *AtrLength = 0;
        *Atr = 0;
        rd[readerNum].card.powerStatus = 0;
        break;
     
  }
  
  return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHTransmitToICC ( DWORD Lun, SCARD_IO_HEADER SendPci, 
				                 PUCHAR TxBuffer, DWORD TxLength, 
				                 PUCHAR RxBuffer, PDWORD RxLength, 
				                 PSCARD_IO_HEADER RecvPci ) {
  
  /* This function performs an APDU exchange with the card/slot specified by
     Lun.  The driver is responsible for performing any protocol specific exchanges
     such as T=0/1 ... differences.  Calling this function will abstract all protocol
     differences.

     SendPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F 0x00)
     TxLength - Length of this buffer.
     RxBuffer - Receive APDU example (0x61 0x14)
     RxLength - Length of the received APDU.  This function will be passed
     the size of the buffer of RxBuffer and this function is responsible for
     setting this to the length of the received APDU.  This should be ZERO
     on all errors.  The resource manager will take responsibility of zeroing
     out any temporary APDU buffers for security reasons.
  
     RecvPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     Notes:
     The driver is responsible for knowing what type of card it has.  If the current
     slot/card contains a memory card then this command should ignore the Protocol
     and use the MCT style commands for support for these style cards and transmit 
     them appropriately.  If your reader does not support memory cards or you don't
     want to then ignore this.

     RxLength should be set to zero on error.

     returns:
     
     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
     IFD_RESPONSE_TIMEOUT
     IFD_ICC_NOT_PRESENT
     IFD_PROTOCOL_NOT_SUPPORTED
  */
  int readerNum = (Lun & 0xFFFF0000) >> 16;
      if (TxBuffer == NULL || TxLength == 0) {
        syslog(LOG_INFO, "No data to be sent");
        if (RxLength)
                *RxLength = 0;
                return IFD_COMMUNICATION_ERROR;
    }

    switch (SendPci.Protocol) {
        case ATR_PROTOCOL_TYPE_T0:
           if(rd[readerNum].card.present == 0) {
              if (RxLength)
                  *RxLength = 0;
              return IFD_ICC_NOT_PRESENT;
           }
           if (rd[readerNum].card.powerStatus == 0) {
              if (RxLength)
                  *RxLength = 0;
              return IFD_COMMUNICATION_ERROR; 
           }
           if (RecvPci)
              RecvPci->Protocol = ATR_PROTOCOL_TYPE_T0;  
                   
           int readLength = 0;
           unsigned char* response = myson_write_t0(&(rd[readerNum]), TxLength, TxBuffer, &readLength);
           if(readLength > 0)
           {
               *RxLength = readLength;
               memcpy(RxBuffer, response, *RxLength);
               free(response);
           }
           else 
           {
              *RxLength = 0;
              return IFD_COMMUNICATION_ERROR;
           }
           
           break;
        case ATR_PROTOCOL_TYPE_T1:
           //T1 is not supported for now. It should be whenever implemented.
           *RxLength = 0;
           return IFD_PROTOCOL_NOT_SUPPORTED;
           break;
        default:
            if (RxLength)
                *RxLength = 0;
            return IFD_PROTOCOL_NOT_SUPPORTED;
    }

  return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHControl ( DWORD Lun, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, PDWORD RxLength )
{

  /* This function performs a data exchange with the reader (not the card)
     specified by Lun.  Here XXXX will only be used.
     It is responsible for abstracting functionality such as PIN pads,
     biometrics, LCD panels, etc.  You should follow the MCT, CTBCS 
     specifications for a list of accepted commands to implement.

     TxBuffer - Transmit data
     TxLength - Length of this buffer.
     RxBuffer - Receive data
     RxLength - Length of the received data.  This function will be passed
     the length of the buffer RxBuffer and it must set this to the length
     of the received data.

     Notes:
     RxLength should be zero on error.
  */
   // int readerNum = (Lun & 0xFFFF0000) >> 16;
    *RxLength = 0; //we have nothing to control. The reader can only talk with the card.
    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHICCPresence( DWORD Lun ) 
{
  /* This function returns the status of the card inserted in the 
     reader/slot specified by Lun.  It will return either:

     returns:
     IFD_ICC_PRESENT
     IFD_ICC_NOT_PRESENT
     IFD_COMMUNICATION_ERROR
  */
  
    int readerNum = (Lun & 0xFFFF0000) >> 16;
    if(rd[readerNum].card.present == 1 && rd[readerNum].card.powerStatus == 1)
       return IFD_ICC_PRESENT;
   

    populateAtr(&(rd[readerNum]));
    int retval = (rd[readerNum].card.present == 1)?IFD_ICC_PRESENT:IFD_ICC_NOT_PRESENT;

    return retval;
}


