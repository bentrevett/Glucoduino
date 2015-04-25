/*
 *******************************************************************************
 * USB-MIDI dump utility
 * Copyright 2013 Yuuichi Akagawa
 *
 * for use with USB Host Shield 2.0 from Circuitsathome.com
 * https://github.com/felis/USB_Host_Shield_2.0
 *
 * This is sample program. Do not expect perfect behavior.
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

#include <Usb.h>
#include <glucoduino.h>

USB  Usb;
GLUCODUINO  Midi(&Usb);

void MIDI_poll();

boolean hasReceived=false;
uint16_t pid, vid;

void setup()
{
  vid = pid = 0;
  Serial.begin(115200);
  Serial.println("Program beginning...");
  
  //Workaround for non UHS2.0 Shield 
  pinMode(7,OUTPUT);
  digitalWrite(7,HIGH);

  if (Usb.Init() == -1) {
    while(1); //halt
  }//if (Usb.Init() == -1...
  delay(200);
}

void loop()
{
  Usb.Task();
  if( Usb.getUsbTaskState() == USB_STATE_RUNNING )
  {
    MIDI_poll();
  }
  else{
  Serial.println("Waiting for USB to be connected...");
  delay(1000);
  }
}

// Poll USB MIDI Controler and send to serial MIDI

void MIDI_poll()
{
    char buf[20];
    uint8_t bufMidi[64];
    uint8_t ack[5]={0x00,0x00,0x00,0x01,0x06};
    uint8_t toBluetooth[64];
    memset(bufMidi, 1, sizeof(bufMidi));
    uint16_t  rcvd;
    int readingStart=0, x=0, valueStart=0, valueEnd=0, carriageReturn=0, n=0;

    if(Midi.vid != vid || Midi.pid != pid){
      sprintf(buf, "VID:%04X, PID:%04X", Midi.vid, Midi.pid);
      Serial.println(buf);
      vid = Midi.vid;
      pid = Midi.pid;
      //VID:1A79, PID:6300 BAYER CONTOUR NEXT LINK
      //VID:1A79, PID:7410 BAYER CONTOUR NEXT USB
      //VID:1A61, PID:3850 FREESTYLE OPTIUM NEO
    }
    
//    Serial.println("Sending data...");
//    if(hasReceived==false)
//    Midi.SendData(bufMidi,64);
//    
//    Serial.println("Receiving data...");
//    while(1){
//    delay(10);
//    if(Midi.RecvData( &rcvd,  bufMidi) == 0 ){
//        for(int i=0; i<64; i++){
//          sprintf(buf, "%c", bufMidi[i]);
//          Serial.print(buf);
//        }
//        Serial.println("");
//        
//        if(bufMidi[0]==0x41&&bufMidi[1]==0x42&&bufMidi[2]==0x43&&bufMidi[3]==0x01&&bufMidi[4]==0x05) //end of header
//        break;
//        
//        hasReceived=true;
//    }
//    }
    
    Serial.println("Sending ack");

    while(1){
    delay(10);
    Midi.SendData(ack,5);
    if(Midi.RecvData( &rcvd,  bufMidi) == 0 ){
        for(int i=0; i<64; i++){
          if(bufMidi[i-1]==0x5E&&bufMidi[i-2]==0x5E&&bufMidi[i-3]==0x5E){
          readingStart=i;
          }
          sprintf(buf, "%c", bufMidi[i]);
          Serial.print(buf);
        }
        Serial.println("");
        
        if(readingStart!=0){
        if(bufMidi[readingStart]==0x47){
        Serial.print("glucose reading, ");
        toBluetooth[0]=0x47;
        }
        if(bufMidi[readingStart]==0x43){
        Serial.print("carb reading, ");
        toBluetooth[0]=0x43;
        }
        
        if(bufMidi[readingStart]==0x49){
        Serial.print("insulin reading, ");
        toBluetooth[0]=0x49;
        }
        
        //starts looking for '|' character from 0, valueStart is position in array of '|'
        x=readingStart;
        while(1){
        if(bufMidi[x]==0x7C){
        valueStart=x;
        break;
        }
        x++;
        }
        
        //starts looking for '|' character from valueStart, valueEnd is position in array of '|'
        x++;
        while(1){
        if(bufMidi[x]==0x7C){
        valueEnd=x;
        break;
        }
        x++;
        }
        
        n=1;
        //prints whatever is between the two '|'s of valueStart and valueEnd
        Serial.print("value is: ");
        for(x=valueStart+1;x<valueEnd;x++){
        toBluetooth[n]=bufMidi[x];
        sprintf(buf, "%c", bufMidi[x]);
        Serial.print(buf);
        n++;
        }
        
        //starts looking for <CR> from valueEnd, carriageReturn is position in array of <CR>
        x++;
        while(1){
        if(bufMidi[x]==0xD){
        carriageReturn=x;
        break;
        }
        x++;
        }
        n++;
        Serial.print(", date is: ");
        for(x=carriageReturn-12;x<carriageReturn;x++){
        toBluetooth[n]=bufMidi[x];
        sprintf(buf, "%c", bufMidi[x]);
        Serial.print(buf);
        n++;
        }
        
        readingStart=0;
        Serial.println("");
        }
        
        
        
        if(bufMidi[0]==0x41&&bufMidi[1]==0x42&&bufMidi[2]==0x43&&bufMidi[3]==0x01&&bufMidi[4]==0x04) //end of transmission
        break;
        
        hasReceived=true;
    } //end of if(Midi.RecvData)
    }//end of while(1)
    
    Serial.println("End of transmission");
    while(1){}
}
