/*
 *******************************************************************************
 * Glucoduino read sketch
 * Copyright 2015 Ben Trevett, University of Sheffield - btrevett1@sheffield.ac.uk
 *
 * An advanced modification of a sketch to read and write data to USB Midi 
 * devices, found at: https://github.com/YuuichiAkagawa/USBH_MIDI
 * 
 * For more information, go to: https://github.com/bentrevett/Glucoduino
 *
 * This is sample program. Do not expect perfect behavior.
 *******************************************************************************
 */

#include <Usb.h>
#include <glucoduino.h>

USB  Usb;
GLUCODUINO  Gluco(&Usb);

void setup()
{
  //set up baud rate for printing to serial, print to display to user program has began
  Serial.begin(115200);
  Serial.println("Program beginning...");
  
  //must be included so that usb host shield version 1 will work
  pinMode(7,OUTPUT);
  digitalWrite(7,HIGH);

  //initialises USB, Usb.Init() returns -1 if initialisation fails
  if (Usb.Init() == -1) {
    Serial.print("USB Initialisation failed...");
    while(1); 
  }
  delay(200);
}

void loop()
{
  //Usb.Task() begins USB tasks
  Usb.Task();
  
  //Checks the state of the USB, if running can carry out the gluco_read() function to read data from the glucometer
  if( Usb.getUsbTaskState() == USB_STATE_RUNNING )
  {
    gluco_read();
  }
  else{
  Serial.println("Waiting for USB to be connected...");
  delay(1000);
  }
}

// Poll glucometer and send to serial
void gluco_read()
{
    //variable declaration - uint16_t = unsigned int, uint8_t = unsigned char
    uint16_t pid=0, vid=0; //variables to store product ID (pid) and vendor ID (vid)
    char buf[20]; //buffer to hold characters to print to serial in readable text
    uint8_t bufGluco[64]; //buffer to hold characters read from glucometer
    uint8_t toBluetooth[64]; //buffer to hold variables to send over bluetooth, will be in shorter form than plain text to reduce send bits and increase transfer speed
    uint8_t ack[5]={0x00,0x00,0x00,0x01,0x06}; //sends acknowledgement character to the glucometer so the glucometer knows that it's ok to send data
    uint16_t  rcvd; //holds address of bytes received from glucometer
    int readingStart=0, valueStart=0, valueEnd=0, carriageReturn=0, n=0,x=0; //various variables for counting
    boolean hasReceived=false;
    
    //set bufGluco to 64 0's to effectively empty it between each transfer
    memset(bufGluco, 0, sizeof(bufGluco)); 

    //get glucometers PID and VID and store them in the variables vid and pid, will be used in the future to differentiate between glucometers
    sprintf(buf, "VID:%04X, PID:%04X", Gluco.vid, Gluco.pid);
    Serial.println(buf);
    vid = Gluco.vid;
    pid = Gluco.pid;
    //VID:1A79, PID:6300 BAYER CONTOUR NEXT LINK
    //VID:1A79, PID:7410 BAYER CONTOUR NEXT USB
    //VID:1A61, PID:3850 FREESTYLE OPTIUM NEO
   
    //while loop for data transfer, will continue to do so until end of transmission is received from glucometer, at which point hasReceived goes to true
    while(hasReceived==false){
    
    //will send an acknowledgement to the glucometer, to which the glucometer will respond by sending data to the arduino, short delay is needed between each send
    delay(10);
    Gluco.SendData(ack,5); //first parameter is data to send, second parameter is size of data to send (in bytes)
    
    //if the RecvData function returns 0, data has been received successfully, first parameter is address of data and second is where to store the data
    if(Gluco.RecvData( &rcvd,  bufGluco) == 0 ){
        
        //NOTE: THE FOLLOWING IS UNIQUE TO BAYER'S CONTOUR NEXT USB AND COUNTER NEXT LINK GLUCOMETERS DUE TO THE WAY THE DATA RECEIVED FROM THOSE GLUCOMETERS IS FORMATTED
        //example readings:
        //ABC;5R|11|^^^Glucose|4.0|mmol/L^P||B/M0/T1||201404021052 40
        //ABC)6R|12|^^^Carb|8|3^||||201404021053 A0
        //ABC-7R|13|^^^Insulin|80|1^||||201404021053 3A
        
        //loops through all of data received and prints it out
        for(int i=0; i<64; i++){
          //the if statement is looking for the sequence of characters '^^^' (0x5E in hex), the following character is the type of reading, i.e. ^^^Glucose, ^^^Insulun, etc.
          //and the position in the array of the first letter of the reading, G, I, or C, is stored in the variable 'readingStart'.
          if(bufGluco[i-1]==0x5E&&bufGluco[i-2]==0x5E&&bufGluco[i-3]==0x5E){
          readingStart=i;
          }
          sprintf(buf, "%c", bufGluco[i]);
          Serial.print(buf);
        }
        
        Serial.println("");
        
        //three if statements to check what type of reading the value is, taken from the first letter following the '^^^' received
        if(readingStart!=0){ //makes sure '^^^' has actually been read, as readingStart sets itself to 0 at the end of each read event
        Serial.print("type: ");
        if(bufGluco[readingStart]==0x47){ 
        Serial.print("glucose, "); //if letter is G (0x47 in hex), glucose reading
        toBluetooth[0]=0x47;
        }
        else if(bufGluco[readingStart]==0x43){
        Serial.print("carb, "); //if letter is C (0x43 in hex), carb reading
        toBluetooth[0]=0x43;
        }
        else if(bufGluco[readingStart]==0x49){
        Serial.print("insulin, "); //if letter is I (0x49 in hex), glucose reading
        toBluetooth[0]=0x49;
        }
        else{
        Serial.print("unknown, "); //if letter neither G, C or I, is an unknown reading, letter U (0x55 in hex) is written to bluetooth
        toBluetooth[0]=0x55; 
        }
        
        
        //starts looking for '|' character from reading start, valueStart is position in array of '|'
        x=readingStart;
        while(1){
        if(bufGluco[x]==0x7C){
        valueStart=x;
        break;
        }
        x++;
        }

        //starts looking for '|' character from valueStart, valueEnd is position in array of '|'
        x++;
        while(1){
        if(bufGluco[x]==0x7C){
        valueEnd=x;
        break;
        }
        x++;
        }
        
        n=1;
        //prints whatever is between the two '|'s of valueStart and valueEnd
        Serial.print("value: ");
        for(x=valueStart+1;x<valueEnd;x++){
        toBluetooth[n]=bufGluco[x];
        sprintf(buf, "%c", bufGluco[x]);
        Serial.print(buf);
        n++;
        }
        Serial.print(", ");
        
        //starts looking for <CR> from valueEnd, carriageReturn is position in array of <CR>
        x++;
        while(1){
        if(bufGluco[x]==0xD){
        carriageReturn=x;
        break;
        }
        x++;
        }
        n++;
        Serial.print("date: ");
        for(x=carriageReturn-12;x<carriageReturn;x++){
        toBluetooth[n]=bufGluco[x];
        sprintf(buf, "%c", bufGluco[x]);
        Serial.print(buf);
        n++;
        }
        
        readingStart=0;
        Serial.println("\n");
        }
        
        
        
        if(bufGluco[0]==0x41&&bufGluco[1]==0x42&&bufGluco[2]==0x43&&bufGluco[3]==0x01&&bufGluco[4]==0x04) //end of transmission
        hasReceived=true;
        
        
    } //end of if(Gluco.RecvData)
    }//end of while(hasReceived==false)
    
    Serial.println("End of transmission");
    while(1){}
}//end of gluco_read();
