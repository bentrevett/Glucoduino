/*
 *******************************************************************************
 * USB-MIDI class driver for USB Host Shield 2.0 Library
 * Copyright 2012-2013 Yuuichi Akagawa
 *
 * Idea from LPK25 USB-MIDI to Serial MIDI converter
 *   by Collin Cunningham - makezine.com, narbotic.com
 *
 * for use with USB Host Shield 2.0 from Circuitsathome.com
 * https://github.com/felis/USB_Host_Shield_2.0
 *******************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *******************************************************************************
 */

#include "glucoduino.h"

const uint8_t	GLUCODUINO::epDataInIndex  = 1;
const uint8_t	GLUCODUINO::epDataOutIndex = 2;
const uint8_t	GLUCODUINO::epDataInIndexVSP  = 3;
const uint8_t	GLUCODUINO::epDataOutIndexVSP = 4;

GLUCODUINO::GLUCODUINO(USB *p)
{
  pUsb = p;
  bAddress = 0;
  bNumEP = 1;
  bPollEnable  = false;
  isMidiFound = false;
  readPtr = 0;

  // initialize endpoint data structures
  for(uint8_t i=0; i<GLUCO_MAX_ENDPOINTS; i++) {
    epInfo[i].epAddr        = 0;
    epInfo[i].maxPktSize  = (i) ? 0 : 8;
    epInfo[i].epAttribs     = 0;
//    epInfo[i].bmNakPower  = (i) ? USB_NAK_NOWAIT : USB_NAK_MAX_POWER;
    epInfo[i].bmNakPower  = (i) ? USB_NAK_NOWAIT : 4;

  }
  // register in USB subsystem
  if (pUsb) {
    pUsb->RegisterDeviceClass(this);
  }
}

/* Connection initialization of an MIDI Device */
uint8_t GLUCODUINO::Init(uint8_t parent, uint8_t port, bool lowspeed)
{
  uint8_t    buf[DESC_BUFF_SIZE];
  uint8_t    rcode;
  UsbDevice  *p = NULL;
  EpInfo     *oldep_ptr = NULL;
  uint8_t    num_of_conf;  // number of configurations
  
  // get memory address of USB device address pool
  
  AddressPool &addrPool = pUsb->GetAddressPool();
#ifdef DEBUG
  USBTRACE("\rMIDI Init\r\n");
#endif
	//Serial.print("AddressPool");
	//Serial.println(addrPool);
  // check if address has already been assigned to an instance
  if (bAddress) {
    return USB_ERROR_CLASS_INSTANCE_ALREADY_IN_USE;
  }
  // Get pointer to pseudo device with address 0 assigned
  p = addrPool.GetUsbDevicePtr(0);
  if (!p) {
    return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;
  }
  if (!p->epinfo) {
    return USB_ERROR_EPINFO_IS_NULL;
  }

  // Save old pointer to EP_RECORD of address 0
  oldep_ptr = p->epinfo;

  // Temporary assign new pointer to epInfo to p->epinfo in order to avoid toggle inconsistence
  p->epinfo = epInfo;
  p->lowspeed = lowspeed;

  // Get device descriptor
  rcode = pUsb->getDevDescr( 0, 0, sizeof(USB_DEVICE_DESCRIPTOR), (uint8_t*)buf );
  vid = (uint16_t)buf[8]  + ((uint16_t)buf[9]  << 8);
  pid = (uint16_t)buf[10] + ((uint16_t)buf[11] << 8);
  // Restore p->epinfo
  p->epinfo = oldep_ptr;

  if( rcode ){ 
    goto FailGetDevDescr;
  }

  // Allocate new address according to device class
  //Serial.print("port = ");
  //Serial.println(port);
  //Serial.print("parent = ");
  //Serial.println(parent);
  bAddress = addrPool.AllocAddress(parent, false, port);
  if (!bAddress) {
    return USB_ERROR_OUT_OF_ADDRESS_SPACE_IN_POOL;
  }
  //Serial.print("bAddress = ");
  //Serial.println(bAddress);
  // Extract Max Packet Size from device descriptor
  epInfo[0].maxPktSize = (uint8_t)((USB_DEVICE_DESCRIPTOR*)buf)->bMaxPacketSize0; 

  // Assign new address to the device
  rcode = pUsb->setAddr( 0, 0, bAddress );
  if (rcode) {
    p->lowspeed = false;
    addrPool.FreeAddress(bAddress);
    bAddress = 0;
    return rcode;
  }//if (rcode...
#ifdef DEBUG
  USBTRACE2("Addr:", bAddress);
#endif  
  p->lowspeed = false;

  //get pointer to assigned address record
  p = addrPool.GetUsbDevicePtr(bAddress);
  if (!p) {
    return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;
  }
  p->lowspeed = lowspeed;

  num_of_conf = ((USB_DEVICE_DESCRIPTOR*)buf)->bNumConfigurations;

  // Assign epInfo to epinfo pointer
  rcode = pUsb->setEpInfoEntry(bAddress, 1, epInfo);
  if (rcode) {
#ifdef DEBUG
    USBTRACE("setEpInfoEntry failed");
#endif
    goto FailSetDevTblEntry;
  }
#ifdef DEBUG
  USBTRACE2("NC:", num_of_conf);
#endif
  for (uint8_t i=0; i<num_of_conf; i++) {
    parseConfigDescr(bAddress, i);
    if (bNumEP > 1)
      break;
  } // for
 
#ifdef DEBUG
  USBTRACE2("NumEP:", bNumEP);
#endif
  if( bConfNum == 0 ){  //Device not found.
    goto FailGetConfDescr;
  }

  if( !isMidiFound ){ //MIDI Device not found. Try first Bulk transfer device
    epInfo[epDataInIndex].epAddr		= 0x03;//03 instead of 83 as USB Host Sheild is low speed
    epInfo[epDataInIndex].maxPktSize	= epInfo[epDataInIndexVSP].maxPktSize;
    epInfo[epDataOutIndex].epAddr		= 0x04;
    epInfo[epDataOutIndex].maxPktSize	= epInfo[epDataOutIndexVSP].maxPktSize;
  }

  // Assign epInfo to epinfo pointer
  rcode = pUsb->setEpInfoEntry(bAddress, bNumEP, epInfo);
#ifdef DEBUG
  USBTRACE2("Conf:", bConfNum);
#endif
  // Set Configuration Value
  rcode = pUsb->setConf(bAddress, 0, bConfNum);
  if (rcode) {
    goto FailSetConfDescr;
  }
#ifdef DEBUG
  USBTRACE("Init done.");
#endif
  bPollEnable = true;
  return 0;
FailGetDevDescr:
FailSetDevTblEntry:
FailGetConfDescr:
FailSetConfDescr:
  Release();
  return rcode;
}

/* get and parse config descriptor */
void GLUCODUINO::parseConfigDescr( byte addr, byte conf )
{
  uint8_t buf[ DESC_BUFF_SIZE ];
  uint8_t* buf_ptr = buf;
  byte rcode;
  byte descr_length;
  byte descr_type;
  unsigned int total_length;
  USB_ENDPOINT_DESCRIPTOR *epDesc;
  boolean isMidi = false;

  // get configuration descriptor (get descriptor size only)
  rcode = pUsb->getConfDescr( addr, 0, 4, conf, buf );
  if( rcode ){
    return;
  }  
  total_length = buf[2] | ((int)buf[3] << 8);
  if( total_length > DESC_BUFF_SIZE ) {    //check if total length is larger than buffer
    total_length = DESC_BUFF_SIZE;
  }

  // get configuration descriptor (all)
  rcode = pUsb->getConfDescr( addr, 0, total_length, conf, buf ); //get the whole descriptor
  if( rcode ){
    return;
  }  

  //parsing descriptors
  while( buf_ptr < buf + total_length ) {  
    descr_length = *( buf_ptr );
    descr_type   = *( buf_ptr + 1 );
    switch( descr_type ) {
      case USB_DESCRIPTOR_CONFIGURATION :
        bConfNum = buf_ptr[5];
        break;
      case  USB_DESCRIPTOR_INTERFACE :
        if( buf_ptr[5] == USB_CLASS_AUDIO && buf_ptr[6] == USB_SUBCLASS_MIDISTREAMING ) {  //p[5]; bInterfaceClass = 1(Audio), p[6]; bInterfaceSubClass = 3(MIDI Streaming)
          isMidiFound = true; //MIDI device found.
          isMidi      = true;
        }else{
#ifdef DEBUG
          //Serial.print("No MIDI Device\n");
#endif
//          buf_ptr += total_length + 1;
//          bConfNum = 0;
            isMidi = false;
        }
        break;
      case USB_DESCRIPTOR_ENDPOINT :
        epDesc = (USB_ENDPOINT_DESCRIPTOR *)buf_ptr;
        if ((epDesc->bmAttributes & 0x02) == 2) {//bulk
          uint8_t index;
          if( isMidi )
              index = ((epDesc->bEndpointAddress & 0x80) == 0x80) ? epDataInIndex : epDataOutIndex;
          else
              index = ((epDesc->bEndpointAddress & 0x80) == 0x80) ? epDataInIndexVSP : epDataOutIndexVSP;
          epInfo[index].epAddr		= (epDesc->bEndpointAddress & 0x0F);
          epInfo[index].maxPktSize	= (uint8_t)epDesc->wMaxPacketSize;
          bNumEP ++;
#ifdef DEBUG
          PrintEndpointDescriptor(epDesc);
#endif
        }
        break;
      default:
        break;
    }//switch( descr_type  
    buf_ptr += descr_length;    //advance buffer pointer
  }//while( buf_ptr <=...
}

/* Performs a cleanup after failed Init() attempt */
uint8_t GLUCODUINO::Release()
{
  pUsb->GetAddressPool().FreeAddress(bAddress);
  bNumEP       = 1;		//must have to be reset to 1	
  bAddress     = 0;
  bPollEnable  = false;
  readPtr      = 0;
  return 0;
}

/* Receive data from MIDI device */
uint8_t GLUCODUINO::RecvData(uint16_t *bytes_rcvd, uint8_t *dataptr)
{
  bytes_rcvd[0] = (uint16_t)64;//epInfo[epDataInIndex].maxPktSize;
  //Serial.print("baddress = ");
  //Serial.println(bAddress);
  //Serial.print("epInfo[epDataInIndex].epAddr = ");
  //Serial.println(epInfo[epDataInIndex].epAddr);
  return pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, bytes_rcvd, dataptr);
}

/* Receive data from MIDI device */
uint8_t GLUCODUINO::RecvData(uint8_t *outBuf)
{
	//Serial.println("RecvData 2");
  byte rcode = 0;     //return code
  uint16_t  rcvd;

  if( bPollEnable == false ) return false;

  //Checking unprocessed message in buffer.
  if( readPtr != 0 && readPtr < GLUCO_EVENT_PACKET_SIZE ){
    if(recvBuf[readPtr] == 0 && recvBuf[readPtr+1] == 0) {
      //no unprocessed message left in the buffer.
    }else{
      goto RecvData_return_from_buffer;
    }
  }

  readPtr = 0;
  rcode = RecvData( &rcvd, recvBuf);
  if( rcode != 0 ) {
    return 0;
  }
  
  //if all data is zero, no valid data received.
  if( recvBuf[0] == 0 && recvBuf[1] == 0 && recvBuf[2] == 0 && recvBuf[3] == 0 ) {
    return 0;
  }

RecvData_return_from_buffer:
  readPtr++;
  outBuf[0] = recvBuf[readPtr];
  readPtr++;
  outBuf[1] = recvBuf[readPtr];
  readPtr++;
  outBuf[2] = recvBuf[readPtr];
  readPtr++;
  return lookupMsgSize(outBuf[0]);
}

/* Send data to MIDI device */
uint8_t GLUCODUINO::SendData(uint8_t *dataptr, uint8_t nBytes)
{
	
  //Serial.println("Start of SendData");
  /*byte buf[4];
  byte msg;

  msg = dataptr[0];
  // SysEx long message ?
  if( msg == 0xf0 )
  {
	 //Serial.println("returning SysEx");
     return SendSysEx(dataptr, countSysExDataSize(dataptr), nCable);
  }

  buf[0] = (nCable << 4) | (msg >> 4);
  if( msg < 0xf0 ) msg = msg & 0xf0;
  

  //Building USB-MIDI Event Packets
  buf[1] = dataptr[0];
  buf[2] = dataptr[1];
  buf[3] = dataptr[2];
  //Serial.print("Got to switch in SendData, outcome is: ");
  //Serial.println(lookupMsgSize(msg));
  switch(lookupMsgSize(msg)) {
    //3 bytes message
    case 3 :
      if(msg == 0xf2) {//system common message(SPP)
        buf[0] = (nCable << 4) | 3;
      }
      break;

    //2 bytes message
    case 2 :
      if(msg == 0xf1 || msg == 0xf3) {//system common message(MTC/SongSelect)
        buf[0] = (nCable << 4) | 2;
      }
      buf[3] = 0;
      break;

    //1 bytes message
    case 1 :
    default :
      buf[2] = 0;
      buf[3] = 0;
      break;
  }*/
  //Serial.println("ep address sent to");
  //Serial.println(epInfo[epDataOutIndex].epAddr);
  return pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, nBytes, dataptr);
}

#ifdef DEBUG
void GLUCODUINO::PrintEndpointDescriptor( const USB_ENDPOINT_DESCRIPTOR* ep_ptr )
{
	Notify(PSTR("Endpoint descriptor:"));
	Notify(PSTR("\r\nLength:\t\t"));
	PrintHex<uint8_t>(ep_ptr->bLength);
	Notify(PSTR("\r\nType:\t\t"));
	PrintHex<uint8_t>(ep_ptr->bDescriptorType);
	Notify(PSTR("\r\nAddress:\t"));
	PrintHex<uint8_t>(ep_ptr->bEndpointAddress);
	Notify(PSTR("\r\nAttributes:\t"));
	PrintHex<uint8_t>(ep_ptr->bmAttributes);
	Notify(PSTR("\r\nMaxPktSize:\t"));
	PrintHex<uint16_t>(ep_ptr->wMaxPacketSize);
	Notify(PSTR("\r\nPoll Intrv:\t"));
	PrintHex<uint8_t>(ep_ptr->bInterval);
	Notify(PSTR("\r\n"));
}
#endif

/* look up a MIDI message size from spec */
/*Return                                 */
/*  0 : undefined message                */
/*  0<: Vaild message size(1-3)          */
uint8_t GLUCODUINO::lookupMsgSize(uint8_t midiMsg)
{
  uint8_t msgSize = 0;

  if( midiMsg < 0xf0 ) midiMsg &= 0xf0;
  //Serial.print("Got to switch in lookupMsgSize, midiMsg = ");
  //Serial.println(midiMsg);
  switch(midiMsg) {
    //3 bytes messages
    case 0xf2 : //system common message(SPP)
    case 0x80 : //Note off
    case 0x90 : //Note on
    case 0xa0 : //Poly KeyPress
    case 0xb0 : //Control Change
    case 0xe0 : //PitchBend Change
      msgSize = 3;
      break;

    //2 bytes messages
    case 0xf1 : //system common message(MTC)
    case 0xf3 : //system common message(SongSelect)
    case 0xc0 : //Program Change
    case 0xd0 : //Channel Pressure
      msgSize = 2;
      break;

    //1 bytes messages
    case 0xf8 : //system realtime message
    case 0xf9 : //system realtime message
    case 0xfa : //system realtime message
    case 0xfb : //system realtime message
    case 0xfc : //system realtime message
    case 0xfe : //system realtime message
    case 0xff : //system realtime message
      msgSize = 1;
      break;

    //undefine messages
    default :
      break;
  }
  return msgSize;
}

/* SysEx data size counter */
unsigned int GLUCODUINO::countSysExDataSize(uint8_t *dataptr)
{
  unsigned int c = 1;

  if( *dataptr != 0xf0 ){ //not SysEx
    return 0;
  }

  //Search terminator(0xf7)
  while(*dataptr != 0xf7)
  {
    dataptr++;
    c++;
    
    //Limiter (upto 256 bytes)
    if(c > 256){
      c = 0;
      break;
	}
  }
  return c;
}

/* Send SysEx message to MIDI device */
uint8_t GLUCODUINO::SendSysEx(uint8_t *dataptr, unsigned int datasize, byte nCable)
{
  byte buf[4];
  uint8_t rc;
  unsigned int n = datasize;

  //Byte 0
  buf[0] = (nCable << 4) | 0x4;         //x4 SysEx starts or continues

  while(n > 0) {
    switch ( n ) {
      case 1 :
        buf[0] = (nCable << 4) | 0x5;   //x5 SysEx ends with following single byte.
        buf[1] = *(dataptr++);
        buf[2] = 0x00;
        buf[3] = 0x00;
        n = n - 1;
        break;
      case 2 :
        buf[0] = (nCable << 4) | 0x6;   //x6 SysEx ends with following two bytes.
        buf[1] = *(dataptr++);
        buf[2] = *(dataptr++);
        buf[3] = 0x00;
        n = n - 2;
        break;
      case 3 :
        buf[0] = (nCable << 4) | 0x7;   //x7 SysEx ends with following three bytes.
      default :
        buf[1] = *(dataptr++);
        buf[2] = *(dataptr++);
        buf[3] = *(dataptr++);
        n = n - 3;
        break;
    }
    rc = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, 4, buf);
    if(rc != 0)
     break;
  }
  return(rc);
}
