/* Myson CS881x Draft Driver */

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <sys/time.h>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

//les defines: des return codes...on se comportera comme un lecteur série

#define ASE_LONG_RESPONSE_PID               0x90
#define ASE_RESPONSE_PID                    0x10
#define ASE_ACK_PID                         0x20
#define ASE_LONG_RESPONSE_WITH_STATUS_PID   0xF0
#define ASE_RESPONSE_WITH_STATUS_PID        0x70

#define STS_PID_OK                  0x00
#define STS_PID_ERROR               0x01
#define STS_CNT_ERROR               0x02
#define STS_TRUNC_ERROR             0x03
#define STS_LEN_ERROR               0x04
#define STS_UNKNOWN_CMD_ERROR       0x05
#define STS_TIMEOUT_ERROR           0x06
#define STS_CS_ERROR                0x07
#define STS_INVALID_PARAM_ERROR     0x08
#define STS_CMD_FAILED_ERROR        0x09
#define STS_NO_CARD_ERROR           0x0A
#define STS_CARD_NOT_POWERED_ERROR  0x0B
#define STS_COMM_ERROR              0x0C
#define STS_EXTRA_WAITING_TIME      0x0D
#define STS_RETRY_FAILED            0x0E


using namespace std;

int ptym_open(char *pts_name, char *pts_name_s , int pts_namesz);
unsigned char* input(libusb_device_handle* handle, int len);
unsigned char* output(libusb_device_handle* handle, int len, unsigned char* in_data, bool add_zero, int r_len);
bool testCardPresence(libusb_device_handle *handle, unsigned char* ATR);
unsigned char* getCardATR(libusb_device_handle *handle);
void add_checksum(unsigned char* array, int len);
void showATR(libusb_device_handle *handle);
unsigned char* cardICCWrite(libusb_device_handle* handle, unsigned char* data, int len);
void keep_array(unsigned char* src, unsigned char* dst, int len);

//0x03: out endpoint
//0x84: in endpoint

unsigned char usbc_data [] = {
  0x55, 0x53, 0x42, 0x43, 0xd8, 0x1b, 0x03, 0x82, 0x06, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0a, 0x0a, 0x10, 0x46, 0x00, 0x00, 0x00, 0x53, 0x49, 0x4d, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
  
unsigned char card_presence_data [] = {
 0x55, 0x53, 0x42, 0x43, 0xb8, 0x38, 0xf3, 0x81, 0x40, 0x00, 0x00, 0x00, 0x80, 0x00, 0x0a, 0x08, 0x12, 0x46, 0x00, 0x40, 0x00, 0x53, 0x49, 0x4d, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

int main(int argc, char* argv[])
{
  cout << "----- MYSON CS881x EXPERIMENTAL DRIVING PROGRAM -----" << endl << endl;
  libusb_context *context;
  if(libusb_init(&context) != 0) //unable to initialize libusb
  {
    cout << "Unable to initialize libusb" << endl;
    return -1;
  }
  
  libusb_device_handle *myson_handle = libusb_open_device_with_vid_pid(context, 0x04cf, 0x9920);
  if(myson_handle == NULL)
  {
    cout << "Did you connect the Myson?" << endl;
    return -1;
  }
  
  libusb_device* dev = libusb_get_device(myson_handle);
  
  //avoid conflict with an existing mass storage driver
  //this function, although it is in the documentation, is absent from libusb.h...
  //libusb_set_auto_detach_kernel_driver(myson_handle,1);
  
  //we will claim the interface
  if(libusb_kernel_driver_active(myson_handle,0) == 1) //then we free it
  {
    if(libusb_detach_kernel_driver(myson_handle,0) != 0) //error when freeing?
    {
      cout << "Unable to detach interface from kernel driver" << endl;
      libusb_close(myson_handle);
      libusb_exit(context);
    }
  }
  if(libusb_claim_interface(myson_handle, 0) != 0)
  {
    cout << "Unable to claim interface" << endl;
    libusb_close(myson_handle);
    libusb_exit(context);
  }
#ifdef DEBUG
  cout << "Myson successfully initialized" << endl;
#endif 
  
  /* Generate a random id */
  
  usbc_data[4] = (unsigned char)(rand() % 255);
  usbc_data[5] = (unsigned char)(rand() % 255);
  usbc_data[6] = (unsigned char)(rand() % 255);
  usbc_data[7] = (unsigned char)(rand() % 255);
  
  card_presence_data[4] = usbc_data[4];
  card_presence_data[5] = usbc_data[5];
  card_presence_data[6] = usbc_data[6];
  card_presence_data[7] = usbc_data[7];
  
  bool cardsThere = false;
  bool poweredOn = false;
  unsigned char* ATR = NULL;
  bool ATRGot = false;
  
  char master[1024];
  char slave[1024];
  fd_set fd_in;
  
  vector<unsigned char> vec;

  int fd;
  char c1;

  fd=ptym_open(master,slave,1024);
  if(fd < 0) {
    cout << "Unable to intialize term" << endl;
    libusb_close(myson_handle);
    libusb_exit(context);
    return -1;
  }
  //virtual terminal - no empty reading, this is common on sim cards, maybe not on other kinds of cards

  int rc;
  unsigned char* prec = NULL;
  int prec_len = 0;

  bool connected = true;
  
  while(connected)
  {    
    FD_ZERO(&fd_in);
    FD_SET(fd, &fd_in);
    
    rc = select(fd + 1, &fd_in, NULL, NULL, NULL);
    
    if(FD_ISSET(fd, &fd_in))
    {
      //we have something to read - get it!
      while(read(fd,&c1,1) == 1) 
      {
	vec.push_back(c1);
      }
    }
    
    if(libusb_get_port_number(dev) == 0)
    {
      connected = false;
      cout << "Device disconnected" << endl;
      
    }
      
    if(!vec.empty() && myson_handle != NULL)
    {
      int sz = vec.size();
      unsigned char* in = (unsigned char*)malloc(sz * sizeof(unsigned char));
      std::copy(vec.begin(), vec.end(), in);
      if(sz > 1)
      {
	if(in[1] == 0x44 && in[2] == 0x0) //retry command: we should never receive this one, so let it die
	{
	  //do nothing
#ifdef DEBUG
	  cout << "Retry instruction caught" << endl;
#endif
	  free(in);
	  in = (unsigned char*)malloc(prec_len * sizeof(unsigned char));
	  std::copy(prec, prec + prec_len, in);
	  sz = prec_len;
	}
      }

      if(in[1] == 0x10 && in[2] == 0x0 && in[3] == in[0] ^ in[1] ^ in[2]) //self test: yes, we're here
      {
#ifdef DEBUG
	cout << "Self test" << endl;
#endif
	char mytext []= { 'M', 'y', 's', 'o', 'n', ' ', 'f', 'a', 'k', 'e', ' ', 'd', 'r', 'i', 'v', 'e', 'r', ' ', 'b', 'y', ' ', 'C', 'o', 'c', 'o', 'd', 'i', 'd', 'o', 'u'};
	int len = strlen(mytext);
	unsigned char response [len + 3];
	response[0] = ASE_RESPONSE_PID; //our status, to escape sendCloseResponseCommand
	response[1] = (unsigned char)len;
	for(int i = 0; i < len; i++)
	{
	  response[i+2] = mytext[i];
	}
	response[12] = 7; //all voltages are supported
	response[16] = 0; //FTOD : 0
	response[17] = 0;
	add_checksum(response, len + 3);
	write(fd, response, len + 3);	    
      }
      else if(in[1] == 0x16 && in[2] == 0x0 && in[3] == in[0] ^ in[1] ^ in[2]) //status request: "tout va bien, madame la marquise"
      {
	/* TODO find something else than an ATR to check the card is present... */
	unsigned char cardPres;
	if(cardsThere && poweredOn)
	  cardPres = 1; //assume the card is still there as it is supposed to be powered on
	else {
	  ATRGot = false;
	  ATR = getCardATR(myson_handle);
	  cardsThere = testCardPresence(myson_handle, ATR);
	  cardPres = cardsThere?1:0;
	  if(cardsThere) ATRGot = true;
	}
	unsigned char response [] = { ASE_RESPONSE_PID, 0x02, cardPres, 0, 0 };
	add_checksum(response, 5);
	write(fd, response, 5);
      }
      else if(in[1] == 0x20 && in[2] == 0x02) //card power on: we don't handle this for the moment; the card is assumed to be automatically powered on with the right voltage
      {
#ifdef DEBUG
	cout << "Power on request" << endl;
#endif
	if(!ATRGot)
	{
	  if(ATR) free(ATR);
	  ATR = getCardATR(myson_handle);
	  ATRGot = true;
	}
	
	unsigned char response [36];
	response[0] = ASE_RESPONSE_PID;
	response[1] = (unsigned char)33;
	std::copy(ATR, ATR + 33, response + 2);
	add_checksum(response, 36);
	write(fd, response, 36);    
	poweredOn = true;
      }
      else if(in[1] == 0x21 && in[2] == 0x00) //card power off: we don't handle this for the moment; the card is assumed to be
      {
#ifdef DEBUG
	cout << "Power off request" << endl;
#endif
	unsigned char response []= { ASE_ACK_PID };
	write(fd, response, 1);
	poweredOn = false;
      }
      else if(in[1] == 0x15 && in[2] == 0x0b) //card parameters: we will see what to do with this later on...
      {
#ifdef DEBUG
	cout << "Card parameters received: accepted but (yet) unparsed" << endl;
#endif
	unsigned char response [] = { ASE_ACK_PID };
	write(fd, response, 1);
      }
      else if(in[1] == 0x22 && in[2] == 0x00) //card reset
      {
#ifdef DEBUG
	cout << "Card reset" << endl;
#endif
	if(!ATRGot)
	{
	  if(ATR) free(ATR);
	  ATR = getCardATR(myson_handle);
	  ATRGot = true;
	}
	unsigned char response [36];
	response[0] = ASE_RESPONSE_PID;
	response[1] = (unsigned char)33;
	std::copy(ATR, ATR + 33, response + 2);
	add_checksum(response, 36);
	write(fd, response, 36);
	poweredOn = true;
      }
      else if(in[1] == 0x40) //standard command: [50, 40, (size), command] (T0 only read)
      {
#ifdef DEBUG
	for(int j = 0; j < vec.size(); j++)
	{
	  cout << hex << (int)vec[j] << " ";
	}
	cout << endl;
#endif
	unsigned char* out = cardICCWrite(myson_handle, in+3, sz-4);
	
	if(out)
	{
	  unsigned char response[5];
	  response[0] = ASE_RESPONSE_PID;
	  response[1] = 0x02;
	  response[2] = out[0];
	  response[3] = out[1];
	  add_checksum(response, 5);
	  
	  write(fd, response, 5);
	  free(out);
	}
	else //error
	{
	  unsigned char response [] = { ASE_RESPONSE_WITH_STATUS_PID, 0, STS_CMD_FAILED_ERROR };
	  write(fd, response, 3);
	}
      }
      else if(in[1] == 0x41) //get response (T0)
      {
#ifdef DEBUG
	cout << "Reply: asking for " << (int)in[7] << " bytes" << endl;

	for(int j = 0; j < vec.size(); j++)
	{
	  cout << hex << (int)vec[j] << " ";
	}
	cout << endl;
#endif	
	int l = (int)in[7];
	unsigned char* pre_out = output(myson_handle, (unsigned char)sz-4, in+3, true,0); //issue the command
	free(pre_out);
	unsigned char* out;
	if(l == 0) l = 259;
	else l = in[7] + 3;
	unsigned char* stab = input(myson_handle, 2); //status bytes if any
	
	
	if(stab)
	{
	  bool valid = true;
	  if(stab[0] == 0x6C) //length error: correct it before it's too late...
	  {
	    int good_l = (int)stab[1];
	    if(good_l == 0) good_l = 259;
	    else good_l += 3;
	    in[7] = stab[1];
	    
	    l = good_l;
	    
	    pre_out = output(myson_handle, (unsigned char)sz-4, in+3, true,0);
	    free(pre_out);
	    out = input(myson_handle, l);  
	  }
	  else if(stab[0] >= 64 && stab[0] <= 0x6f) //file not found
	  {
	    l = 2; //just pass the status bytes
	    valid = false;
	    unsigned char ret[5];
	    ret[0] = ASE_RESPONSE_PID;
	    ret[1] = 0x02;
	    ret[2] = (unsigned char)stab[0];
	    ret[3] = (unsigned char)stab[1];
	    add_checksum(ret, 5);
	    write(fd, ret, 5);
	  }
	  else out = input(myson_handle, l);

	  free(stab);
	  
	  if(valid && out)
	  {
	    unsigned char* owr =NULL;
	    int len = 0;

	    if(l-1 < 256)
	    {
	      owr = (unsigned char*)malloc((l+2) * sizeof(unsigned char));
	      len = l + 2;
	      owr[0] = ASE_RESPONSE_PID;
	      owr[1] = (unsigned char)l-1;
	      for(int j = 1; j < l; j++)
	      {
		owr[j+1] = (char)out[j];
	      }
	      add_checksum(owr, l+2);
	    }
	    else
	    {
	      owr = (unsigned char*)malloc((l+3) * sizeof(unsigned char));
	      len = l+3;
	      owr[0] = ASE_LONG_RESPONSE_PID;
	      owr[1] = (unsigned char) ((l-1 >> 8) & 0xFF);
	      owr[2] = (unsigned char) (l-1 & 0xFF);
	      for(int j = 1; j < l; j++)
	      {
		owr[j+2] = (char)out[j];
	      }
	      
	      //temporary -may be wrong, but if we're there, it's because everything went right
	      owr[l] = 0x90;
	      owr[l+1] = 0x00;
	      
	      add_checksum(owr, l+3);
	      
	    }
	    //the 2 more are the status bytes
	    if(valid)
	    {
	      write(fd,owr,len);
	    }
	    
	    free(out);
	    free(owr);
	  }
	}
	else //error
	{
	  unsigned char response [] = { ASE_RESPONSE_WITH_STATUS_PID, 0, STS_CMD_FAILED_ERROR };
	  write(fd, response, 3);
	}
      }
      else if(in[1] == 0x43) //PPS
      {
	//unsigned char* out = output(myson_handle, (unsigned char)sz-4, in+3, false, 4); //issue the command
	//unsigned char* out = input(myson_handle, 8); //then reader
	
	// we won't do the pps -just sending back what we got
	//if we do it, cards are strangely behaving
#ifdef DEBUG
	cout << "PPS request" << endl;
#endif
	/*cout << "PPS sent: ";
	for(int j = 0; j < sz-4; j++)
	{
	 cout << hex << (int)in[j+3] << " "; 
	}*/
	unsigned char owr[7];
	owr[0] = ASE_RESPONSE_PID;
	owr[1] = 0x04;
	//cout << endl << "PPS received: ";
	for(int j = 0; j < 4; j++)
	{
	  //cout << hex << (int)out[j] << " ";
	  //owr[j+2] = (char)out[j];
	  if(j < sz-1) owr[j+2] = in[j+3];
	    else owr[j+2] = 0;
	}
	//cout << endl;
	add_checksum(owr, 7);
	write(fd, owr, 7);
      }
      else if(in[1] == 0x11)
      {
	//exit
	connected = false;
	
      }
      else //unhandled
      {
	cout << "Unhandled:";
	for(int i = 0; i < sz; i++)
	{
	  cout << hex << (int)in[i] << " ";
	}
	cout << endl;
      }
      
      //keep it in memory, if a retry command gets issued
      if(prec) free(prec);
      prec = (unsigned char*) malloc(sz * sizeof(unsigned char));
      std::copy(in, in + sz, prec);
      prec_len = sz;
      vec.clear();
      free(in);
    }
    usleep(20);
  }

  //give the interface back to the system
  libusb_attach_kernel_driver(myson_handle,1);
  libusb_close(myson_handle);
  libusb_exit(context);
  
  return 0;
}

unsigned char* input(libusb_device_handle* handle, int len)
{
  int expected_bytes_i = len;
  unsigned char expected = (unsigned char) len;
  unsigned char send[31];
  std::copy(usbc_data, usbc_data + 31, send);
  
  if(len > 255)
  {    
    send[19] = 0xFF;
    send[9] = (unsigned char)((len >> 8) & 0xFF);
    send[8] = (unsigned char)(len & 0xFF);
  }
  else
  {
    send[8] = expected; //quantity of data being passed
    send[19] = expected; //quantity of data requested - must be the same as the above
  }
  send[12] = 0x80;
  send[13] = 0x00;
  send[14] = 0x0a;
  send[15] = 0x08;
  
  int actual_length;
  
  unsigned char* data;
  data = (unsigned char*)malloc(len * sizeof(unsigned char));
  
  int r = libusb_bulk_transfer(handle, 0x03, send, sizeof(send), &actual_length, 0);
  if (r == 0 && actual_length == sizeof(usbc_data)) {      
    r = libusb_bulk_transfer(handle, 0x84, data, len, &actual_length, 00);
    if (r == 0 && actual_length == len) {
    } else cout << "Error " << dec << r << ":" << libusb_error_name(r) << "; actual length = " << actual_length << " vs data = " << sizeof(data) <<  endl;
    
#ifdef DEBUG
    for(int i = 0; i < actual_length; i++)
    {
      cout << hex << (int)data[i] << " ";
    }
    cout << endl;
#endif
    
  }
  else cout << "No bulk!! Error: " << libusb_error_name(r) << endl;
  
  //Closure
  unsigned char usbs[13];
  int usbs_length;
  r = libusb_bulk_transfer(handle, 0x84, usbs, sizeof(usbs), &usbs_length, 00);
  if(r != 0) cout << "USBS error" << endl;

  if(actual_length == 0) return NULL;
  return data;
  
}

unsigned char* output(libusb_device_handle* handle, int len, unsigned char* in_data, bool add_zero, int r_len)
{
  int intinput;
  
  unsigned char expected;
  unsigned char actually_sent;
  int really_sent;
  unsigned char send[31];
  std::copy(usbc_data, usbc_data + 31, send);

  unsigned char* read;
  int total_elems;
  
  expected = (unsigned char)r_len;
  actually_sent = (unsigned char) len;
  total_elems = len + ((add_zero)?1:0);
  read = (unsigned char*) malloc(total_elems * sizeof(unsigned char));
  std::copy(in_data, in_data + len, read);
  if(add_zero) read[len] = 0;

#ifdef DEBUG  
  cout << "Passing ";
  for(int i = 0; i < len + (add_zero)?1:0; i++)
  {
    cout << hex << (int)read[i] << " ";
  }
  cout << endl;
#endif
  
  send[19] = (unsigned char)actually_sent; //how much data is passed to the chip
  send[20] = expected; //how much we expect on reply - this seems to go there...
  send[8] = (unsigned char) total_elems; //quantity of data passed

  int actual_length;
  int r = libusb_bulk_transfer(handle, 0x03, send, sizeof(send), &actual_length, 00);
  if ((r == 0 || r & LIBUSB_TRANSFER_COMPLETED) && actual_length == sizeof(usbc_data)) {
    //Good!
    r = libusb_bulk_transfer(handle, 0x03, read, total_elems, &actual_length, 00);
    if ((r == 0 || r & LIBUSB_TRANSFER_COMPLETED) && actual_length == total_elems) {
    } else cout << "Error " << dec << r << ":" << libusb_error_name(r) << endl;
  }
  else cout << "No bulk!! Error: " << libusb_error_name(r) << endl;
  
  //Closure
  unsigned char usbs[13];
  int usbs_length;
  r = libusb_bulk_transfer(handle, 0x84, usbs, sizeof(usbs), &usbs_length, 00);
  
  if(r != 0) cout << "USBS error" << endl;
  free(read);
  
  if(expected > 0)
  {
#ifdef DEBUG
    cout << "Reply: ";
#endif
    unsigned char* data = input(handle, (int)expected);
    return data;
  }
  else return NULL;
  
}

unsigned char* cardICCWrite(libusb_device_handle* handle, unsigned char* data, int len)
{
  //write the 5 first chars
  if(len < 5){
    unsigned char* v = output(handle, len, data, true, 2);
    free(v);
  } else {
    unsigned char* v = output(handle, 5, data, true, 1); //write the command bytes
#ifdef DEBUG
    cout << "Status byte " << hex << (int) v[0] << endl;
#endif
    free(v);
    v = output(handle, len - 5, data + 5, true, 2); //then the data
    free(v);
  }
  
  //then grab the status bytes
  unsigned char* out = input(handle, 2);
  return out;
}

bool testCardPresence(libusb_device_handle* handle, unsigned char* ATR)
{
  bool inserted = false;
  int j = 0;
  while(j < 31 && !inserted)
  {
    inserted = (ATR[j] != card_presence_data[j])?true:false;
    j++;
  }
  return inserted;
}

int ptym_open(char *pts_name, char *pts_name_s , int pts_namesz)
{
  char *ptr;
  int fdm;

  strncpy(pts_name, "/dev/ptmx", pts_namesz);
  pts_name[pts_namesz - 1] = '\0';

  fdm = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fdm < 0)
  return(-1);
  if (grantpt(fdm) < 0)
  {
    close(fdm);
    return(-2);
  }
  if (unlockpt(fdm) < 0)
  {
    close(fdm);
    return(-3);
  }
  if ((ptr = ptsname(fdm)) == NULL)
  {
    close(fdm);
    return(-4);
  }

  strncpy(pts_name_s, ptr, pts_namesz);
  pts_name[pts_namesz - 1] = '\0';

  cout << "Virtual terminal opened: " <<  ptr << endl;
  
  return(fdm);
}

void add_checksum(unsigned char* array, int len)
{
  int cs = (int)array[0];
  for(int i = 1; i < len-1; i++)
  {
    cs = cs ^ (int)array[i];
  }
  array[len-1] = (unsigned char)cs;
}

unsigned char* getCardATR(libusb_device_handle *handle)
{
  int actual_length;
  unsigned char* data;
  unsigned char pre[64];
  data = (unsigned char*)malloc(33 * sizeof(unsigned char));
  bool result = true;
  int r = libusb_bulk_transfer(handle, 0x03, card_presence_data, sizeof(card_presence_data), &actual_length, 0);
  if (r == 0 && actual_length == sizeof(card_presence_data)) {
    r = libusb_bulk_transfer(handle, 0x84, pre, 64, &actual_length, 00);
    if(r != 0) result = false;
  } else result = false;
  unsigned char usbs[13];
  int usbs_length;
  r = libusb_bulk_transfer(handle, 0x84, usbs, sizeof(usbs), &usbs_length, 00);
  if(r != 0) result = false;
  
  std::copy(pre, pre + 33, data);
  
  return data;
}

void showATR(libusb_device_handle *handle)
{
  unsigned char* ATR = getCardATR(handle);
  cout << "ATR: ";
  for(int i = 0; i < 33; i++)
  {
    cout << hex << (int)ATR[i] << " ";
  }
  cout << endl;
  free(ATR);
}
