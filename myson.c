/* Myson CS881x Driver */

#include <myson.h>
#define MYSON_DEBUG 1

//0x03: out endpoint
//0x84: in endpoint

unsigned char scsi_header_data [] = {
   /* USB Mass Storage Packet */
   0x55, 0x53, 0x42, 0x43, //signature
   0xd8, 0x1b, 0x03, 0x82, //tag
   0x06, 0x00, 0x00, 0x00, //data transfer length
   0x00, //flags (?)
   0x00, //LUN (0 here, single logical unit)
   0x0a, //CDB length
   /* SCSI CDB */
   0x0a, //opcode: 0a = write, 08 = read
   0x10, 0x46, 0x00, //LBA
   0x00, //transfer length
   0x00, //control (?)
   0x53, 0x49, 0x4d, 0x20, //SIM
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //unknown
};

unsigned char reset_data [] = {
   /* USB Mass Storage Packet */
   0x55, 0x53, 0x42, 0x43, //signature
   0xb8, 0x38, 0xf3, 0x81, //tag
   0x40, 0x00, 0x00, 0x00, //data transfer length: 64
   0x80, //flags
   0x00, //LUN
   0x0a, //CDB length
   /* SCSI CDB */
   0x08, //opcode: read
   0x12, 0x46, 0x00, //LBA
   0x40, //length: 64, exceeding what the ATR could be
   0x00, //control (?)
   0x53, 0x49, 0x4d, 0x20, //SIM
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //unknown
};

unsigned char write_cbw [] = {
   /* USB Mass Storage Packet */
   0x55, 0x53, 0x42, 0x43, //signature: USBC (CBW)
   0x00, 0x00, 0x00, 0x00, //tag
   0x00, 0x00, 0x00, 0x00, //data transfer length
   0x00, //flag: direction (7th bit is 0, meaning OUT)
   0x00, //LUN (0 here, single logical unit)
   0x0a, //CDB length: 10 bytes
   /* SCSI CDB */
   0x0a, //opcode: 0a = write
   0x10, 0x46, 0x00, //LBA
   0x00, //transfer length
   0x00, //control (?)
   0x53, 0x49, 0x4d, 0x20, //SIM
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //padding to 31 bytes
};

unsigned char read6_cbw [] = {
   /* USB Mass Storage Packet */
   0x55, 0x53, 0x42, 0x43, //signature: USBC (CBW)
   0x00, 0x00, 0x00, 0x00, //tag
   0x00, 0x00, 0x00, 0x00, //data transfer length
   0x80, //flag: direction (7th bit is 1, meaning IN)
   0x00, //LUN (0 here, single logical unit)
   0x0a, //CDB length
   /* SCSI CDB */
   0x08, //opcode: read 6
   0x10, 0x46, 0x00, //LBA
   0x00, //transfer length
   0x00, //control (?)
   0x53, 0x49, 0x4d, 0x20, //SIM
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //padding to 31 bytes
};

unsigned char read10_cbw [] = {
   /* USB Mass Storage Packet */
   0x55, 0x53, 0x42, 0x43, //signature
   0x00, 0x00, 0x00, 0x00, //tag
   0x00, 0x00, 0x00, 0x00, //data transfer length; 2 bytes only, should not exceed 259
   0x80, //flag: direction (7th bit is 1, meaning IN)
   0x00, //LUN (0 here, single logical unit)
   0x0e, //CDB length
   /* SCSI CDB */
   0x28, //opcode: read 10
   0x00, //control: 000 (rdprotect), 1 (dpo), 0 (fua), 0 (obsolete), 0 (fua_nv), 0 (obsolete): do not put in cache,
   0x00, 0x10, 0x46, 0x00, //LBA
   0x20, //group properties & reserved
   0x00, 0x00, //transfer length
   0x00, //control (?)
   0x53, 0x49, 0x4d, 0x20, //SIM
   0x00, 0x00, 0x00, 0x00 //padding to 31 bytes
};

void msleep(int ms)
{
   usleep(1000 * ms);
}

int min (int a, int b)
{
   if(a < b) return a;
   return b;
}

void print_array(unsigned char* arr, int len)
{
   int i = 0;
   while(i < len)
   {
      printf("%x ",arr[i]);
      i++;
   }
   printf("\n");
}

unsigned char* input(libusb_device_handle* handle, int len, int* act_read)
{
   unsigned char expected = (unsigned char) len;
   //unsigned char send[31];
   //memcpy(send, scsi_header_data, 31);
   unsigned char* send;
   int sendlen = 0;
   int lenToRead = len;
   
   unsigned char tag1 = (unsigned char)(rand() % 256);
   unsigned char tag2 = (unsigned char)(rand() % 256);
   unsigned char tag3 = (unsigned char)(rand() % 256);
   unsigned char tag4 = (unsigned char)(rand() % 256);
   
   if(len > 255)
   {  
/*     
 * the following is the right way to read more than 256 bytes (it would go up to 259 bytes), yet I can't get it to work...
 * (using a read 10 command instead of a read 6, which only gives up to 256 bytes)
 * there would be no problem if there wasn't this extra byte at the very beginning of the string...
      sendlen = 31;
      
      send = (unsigned char*)malloc(sendlen * sizeof(unsigned char));
      memcpy(send, read10_cbw, sendlen);
      
      send[4] = tag1;
      send[5] = tag2;
      send[6] = tag3;
      send[7] = tag4;
      send[8] = (unsigned char)(len & 0xFF);
      send[9] = (unsigned char)((len >> 8) & 0xFF);
      send[22] = (unsigned char)((len >> 8) & 0xFF);
      send[23] = (unsigned char)(len & 0xFF);
*/
      sendlen = 31;
      lenToRead = 255;
      send = (unsigned char*)malloc(sendlen * sizeof(unsigned char));
      memcpy(send, read6_cbw, sendlen);
      send[4] = tag1;
      send[5] = tag2;
      send[6] = tag3;
      send[7] = tag4;
      send[8] = (unsigned char)0xFF; //request 256
      send[19] = (unsigned char)0xFF; //256, no more!


   }
   else
   {
      sendlen = 31;
      
      send = (unsigned char*)malloc(sendlen * sizeof(unsigned char));
      memcpy(send, read6_cbw, sendlen);
      send[4] = tag1;
      send[5] = tag2;
      send[6] = tag3;
      send[7] = tag4;
      send[8] = expected; //quantity of data being passed
      send[19] = expected; //quantity of data requested - must be the same as the above
      
   }
   int actual_length;
   
   unsigned char* data;
   data = (unsigned char*)malloc(lenToRead * sizeof(unsigned char));
   
   bool err = false;
   int r = libusb_bulk_transfer(handle, OUT_ENDPOINT, send, sendlen, &actual_length, 0);
   if (r == 0 && actual_length == sizeof(scsi_header_data)) {
      r = libusb_bulk_transfer(handle, IN_ENDPOINT, data, lenToRead, &actual_length, 0);
      if (!(r == 0 || r & LIBUSB_TRANSFER_COMPLETED))
      {
         err = true;
         printf("Read failed, %d bytes received, error: %s\n", actual_length, libusb_error_name(r));
      }
   }
   else {
      err = true;
      printf("Sending failed, error: %s\n", libusb_error_name(r));
   }
   
   //Closure
   unsigned char transfer_ack_data[13];
   int transfer_ack_data_length;
   r = libusb_bulk_transfer(handle, IN_ENDPOINT, transfer_ack_data, sizeof(transfer_ack_data), &transfer_ack_data_length, 0);
   if (!(r == 0 || r & LIBUSB_TRANSFER_COMPLETED))
   {
      printf("Close failed, error: %s\n", libusb_error_name(r));
      err = true;
   }
   
   if(err)
   {
      r = libusb_reset_device(handle);
      if(r != 0)
         printf("Error on device reset: %s\n", libusb_error_name(r));
#ifdef MYSON_DEBUG
      else
         printf("Reset successful\n");
#endif
   }
   
   if(actual_length == 0 || transfer_ack_data_length == 0)
   {
      *act_read = 0;
      return NULL;
   }
   *act_read = actual_length;
   return data;
   
}

void output(libusb_device_handle* handle, int len, unsigned char* in_data, bool add_zero, int r_len)
{
   unsigned char expected;
   unsigned char actually_sent;
   unsigned char header[31];
   memcpy(header, write_cbw, 31);
   
   unsigned char* payload;
   int total_elems;
   
   expected = (unsigned char)r_len;
   actually_sent = (unsigned char) len;
   total_elems = len + ((add_zero)?1:0);
   payload = (unsigned char*) malloc(total_elems * sizeof(unsigned char));
   memcpy(payload, in_data, len);
   if(add_zero) payload[len] = 0;

   header[4] = (unsigned char)(rand() % 256);
   header[5] = (unsigned char)(rand() % 256);
   header[6] = (unsigned char)(rand() % 256);
   header[7] = (unsigned char)(rand() % 256);
   
   header[19] = (unsigned char) actually_sent; //see above for the roles
   header[20] = (r_len >= 256)?0:expected;
   //header[20] = 0x20;
   header[8] = (unsigned char) total_elems;
   
   int actual_length;
#ifdef MYSON_DEBUG
   printf("Sending: ");
   print_array(payload, total_elems);
#endif
   bool err = false;
   
   int r = libusb_bulk_transfer(handle, OUT_ENDPOINT, header, sizeof(header), &actual_length, 00);
   if ((r == 0 || r & LIBUSB_TRANSFER_COMPLETED) && actual_length == sizeof(scsi_header_data)) {
      //Good! Now let's header what we got.
      r = libusb_bulk_transfer(handle, OUT_ENDPOINT, payload, total_elems, &actual_length, 00);
      if (!((r == 0 || r & LIBUSB_TRANSFER_COMPLETED) && actual_length == total_elems)) 
      {
         printf("Write failed, error: %s\n", libusb_error_name(r));
         err = true;
      }
   }
   else
   {
      printf("Transmit Error: %s\n", libusb_error_name(r));
      err = true;
   }
   
   //Closure
   unsigned char transfer_ack_data[13];
   int transfer_ack_data_length;
   r = libusb_bulk_transfer(handle, IN_ENDPOINT, transfer_ack_data, sizeof(transfer_ack_data), &transfer_ack_data_length, 00);
   if (!(r == 0 || r & LIBUSB_TRANSFER_COMPLETED))
   {
      printf("Close failed, error: %s\n", libusb_error_name(r));
      err = true;
   }
   
   free(payload); //job done

   if(err)
   {
      r = libusb_reset_device(handle);
      if(r != 0)
         printf("Error on device reset: %s\n", libusb_error_name(r));
#ifdef MYSON_DEBUG
      else
         printf("Reset successful\n");
#endif
      
   }
   /*if(expected > 0)
   {
      unsigned char* data = input(handle, (int)expected);
      printf("Response: ");
      print_array(data, expected);
      free(data);
   }*/
}

bool testCardPresence(unsigned char* ATR)
{
   bool inserted = false;
   int j = 0;
   while(j < 31 && !inserted)
   {
      inserted = (ATR[j] != reset_data[j])?true:false;
      j++;
   }
   return inserted;
}


void populateAtr(reader* r)
{
   /* to be reworked: we get the exact size of the ATR and put it in the struct atr */
   libusb_device_handle *handle = r->handle;
   
   int actual_length;
   unsigned char pre[64];
   
   //bool result = true;
   int res = libusb_bulk_transfer(handle, OUT_ENDPOINT, reset_data, sizeof(reset_data), &actual_length, 0);
   if (res == 0 && actual_length == sizeof(reset_data)) {
      res = libusb_bulk_transfer(handle, IN_ENDPOINT, pre, 64, &actual_length, 00);
     // if(res != 0) result = false;
   }// else result = false;
   
   unsigned char usbs[13];
   int usbs_length;
   res = libusb_bulk_transfer(handle, IN_ENDPOINT, usbs, sizeof(usbs), &usbs_length, 00);
   //if(res != 0) result = false;
   
   if(testCardPresence(pre))
   {
      
      int atrlen = 2;
      int TD = (int)pre[1];
      int histobytes = (TD & 0x0F);
      
      if(histobytes) {
         atrlen += histobytes;
      }
      
      int j = 2;
      int TCK = 0;
      
      while(1)
      {
         if(TD & 0x10) j++; //TAi present
         if(TD & 0x20) j++; //TBi present
         if(TD & 0x40) j++; //TCi present
         if(TD & 0x80) { //TDi present   
            if(TD & 0x0F) TCK = 1; //TCK is to be present;
            TD = pre[j];
            j++;
         }
         else break;
      }
      
      atrlen += (j-2) + TCK;
      
      r->card.atr.length = atrlen;
      memcpy(r->card.atr.data, pre, atrlen);
      r->card.present = 1;
   }
   else
   {
      r->card.present = 0;
      r->card.atr.length = 0;
      int i = 0;
      while(i < MAX_ATR_SIZE)
      {
         r->card.atr.data[i] = 0;
         i++;
      }
   }
   
}

void writeT0Command(libusb_device_handle *handle, unsigned char* txbuffer, int len, int expect)
{
   if(len > 5)
   {
      output(handle, 5, txbuffer, true, 1); //write the command bytes
      int act_read = 0;
      unsigned char* firstStab = input(handle, 1, &act_read);
#ifdef MYSON_DEBUG
      printf("Temp response: %x\n", firstStab[0]);
#endif
      free(firstStab);
      output(handle, len - 5, txbuffer + 5, false, expect); //then the data
   }
   else
   {
      
      output(handle, len, txbuffer, true, expect); //write the whole apdu
   }
   /*
   int act_read = 2;
   unsigned char* firstStab = input(handle, 2, &act_read);
   printf("Stab: ");
   print_array(firstStab, 2);
   free(firstStab);
*/
}

unsigned char* myson_write_t0(reader* r, int txlength, unsigned char* txbuffer, int* rxlength)
{
   libusb_device_handle *handle = r->handle;
   //Separate here the 4 cases
   if (txlength > 5) //write then get response
   {
      int tmpLen = 4 + 1 + txbuffer[4] + 1; // [CLA INS P1 P2] [Lc] [Lc data bytes] [Le]
      //if [Le] is present then it's case 4
      
      if (txlength == tmpLen) {
#ifdef MYSON_DEBUG
         printf("Found case 4\n");
#endif

         writeT0Command(handle, txbuffer, txlength, 2);

         int act_read = 0;
         unsigned char* pStatusBytes = input(handle, 2, &act_read); //status bytes if any
#ifdef MYSON_DEBUG
         printf("Status bytes before get response: %x %x\n", pStatusBytes[0], pStatusBytes[1]);
#endif
         //int Le = (int)txbuffer[txlength - 1];
         /*
         unsigned char getResponseCommand [5] = {0x00, 0xC0, 0x00, 0x00, 0x00};
         getResponseCommand[0] = txbuffer[0]; //class
         getResponseCommand[4] = pStatusBytes[1]; // (unsigned char)(min((int)pStatusBytes[1], Le)); //the length to be expected
         */
         *rxlength = act_read;
         return pStatusBytes;
         
         /* the get response should not be handled here - it's a case 2 command that applications will send by themselves */

      }
      else
      {
#ifdef MYSON_DEBUG
         printf("Found case 3\n");
#endif
        // int l = txbuffer[txlength - 1]; //how much data we are expecting
         writeT0Command(handle, txbuffer, txlength, 2);
         int act_read = 0;
         unsigned char* data = input(handle, 2, &act_read); //read the response
         *rxlength = act_read;
         return data;
      }
      
   } 
   else if (txlength == 4) //case 1
   {
#ifdef MYSON_DEBUG
      printf("Found case 1\n");
#endif
      writeT0Command(handle, txbuffer, txlength, 2); //pass the command
      int act_read = 0;
      unsigned char* data = input(handle, 2, &act_read); //read the response: the status bytes
      *rxlength = act_read;
      return data;
   }
   else //this is a get response command
   {
#ifdef MYSON_DEBUG
      printf("Found case 2\n");
#endif
      unsigned char* out;
      int l = (int)txbuffer[txlength - 1];
      /*
      if(l > 0 || l < 253)
         writeT0Command(handle, txbuffer, txlength, l + 3); //length can be added 3
      else
         writeT0Command(handle, txbuffer, txlength, 255);
      */
      writeT0Command(handle, txbuffer, txlength, 0);
      int act_read = 0;
      unsigned char* statusBytes = input(handle, 2, &act_read); //status bytes if any

#ifdef MYSON_DEBUG
      printf("Status bytes: ");
      print_array(statusBytes, 2);
#endif
      
      if(l == 0) l = 256;
      
      if(statusBytes)
      {
         if(statusBytes[0] == 0x6C) //length error: correct it before it's too late...
         {
            unsigned char getResponseCommand [5] = {0x00, 0xC0, 0x00, 0x00, 0x00}; //in the case it's needed
            getResponseCommand[0] = txbuffer[0]; //class
            int good_l = (int)statusBytes[1]; // 6C XX indicates how many bytes we could actually read
            if(good_l == 0) good_l = 256; //perhaps there will be an error while setting this
            getResponseCommand[3] = statusBytes[1];
            l = good_l;
            
            writeT0Command(handle, getResponseCommand, 5, l + 3); //issue the get response command
            out = input(handle, l + 3, &act_read); //assuming this is right
         }
         else if(statusBytes[0] >= 0x64 && statusBytes[0] <= 0x6f) //other errors
         {
            *rxlength = 2; //just pass the status bytes
            return statusBytes;
         }
         else out = input(handle, l + 3, &act_read);
         
         free(statusBytes);
         
         if(act_read > 0)
         {
            //the following workaround also seems to be done by the Myson reader software
            //we get 1 extra byte at the beginning of the response, plus the 2 status bytes
            //hence the l + 3 length for reading and the l + 2 length for the rest
            unsigned char* owr = (unsigned char*)malloc( (l + 2) * sizeof(unsigned char));
            //add this if length is 256... we don't get it from the card...
            
            memcpy(owr, out + 1, min(l + 2, act_read));
            
            if(l == 256)
            {
               owr[l] = 0x90;
               owr[l + 1] = 0x00;
            }
            printf("Here we are!\n");
            
            *rxlength = l + 2;
            
            free(out);
            return owr;
         }
         else //error
         {
            *rxlength = 0;
            return 0;
         }
      
      }
      else //error
      {
         *rxlength = 0;
         return 0;
      }
      
   }
   *rxlength = 0;
   return NULL;
}