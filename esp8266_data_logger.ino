// ESP8266 Datalogger with realtime clock, command prompt, using internal flash memory and caching data
// by Heinrich Diesinger


#include "LittleFS.h" // LittleFS allows using part of the flash memory as a file system
#include "Wire.h" // I2C support, here for the realtime clock module
#define DS3231_I2C_ADDRESS 0x68

// variables for caching data:
const long lcachesize = 1025; // size of locally defined cache in readall(), readlast()
                              //put here a kB + 1 for the terminating zero
const long gcachesize = 8193; // size of globally defined write cache used by loadcache(), cachedata(char userdata[180]), commitcache()
                              // we put 8 kB +1 for the terminating zero                            
char gcache[gcachesize]; // globally defined cache

// variables for the serial input handler:
const byte numchars = 200;
char inputcharray[numchars];
//char *inputccptr = inputcharray; // LoRa input
char message[numchars];  // all input be it LoRa or local
char *messptr = message;
boolean newdata = false;  // whether the reception is complete
int ndx; // counter used in serial reception
long freespace = gcachesize; // measures how much chars are left in the cache
unsigned long lastchar; // timer when last character was received by serial
char rc; // eceived character
char stringbuf[20]; // diverse stuff, used for concatenating the time stamp
char logfilename[20] = "/log.txt";

// variables for test routine
boolean wrap = false; // flag for cache overflow, set when part of a log entry must be written to the next cache
unsigned long lastentry; // timer when last log entry was created
char userdata[180]; // the char array from which the user creates a log entry ba calling cachedata(userdata)
boolean logging = false; // whether auto logging is on
                          //for standalone operation without command prompt, default to true to start logging on start up


// Convert normal decimal to binary coded decimal numbers
byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );
}


// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return( (val/16*10) + (val%16) );
}



// https://www.instructables.com/Using-DS1307-and-DS3231-Real-time-Clock-Modules-Wi/
void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}



// https://www.instructables.com/Using-DS1307-and-DS3231-Real-time-Clock-Modules-Wi/
void readDS3231time(byte second[], // remark hdi: this is the same as *second
byte *minute,
byte *hour,
byte *dayOfWeek,
byte *dayOfMonth,
byte *month,
byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());  // remark hdi: it is a single byte so why put it into a byte array ?
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
} // end readDS3231time()



// https://www.instructables.com/Using-DS1307-and-DS3231-Real-time-Clock-Modules-Wi/
// now instead of displayTime(), where all is written to the console ala Serial.print(minute, DEC), we concatenate it into a char array
// using the sprintf function
void getTime()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  //char einzel;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);  //remark hdi: since minute seems already to be a single byte, can we eliminate the typecast ? 
  // it seems not or else it is always zero
  // strcpy(&einzel, "hello"); this fills a single char from a string
  // strcpy(&einzel, strtokIndx);
  // the opposite is ???
  // strcat(shadoweeprom, ",");
  // now the single char byte-booleans:
  // cur_len = strlen(shadoweeprom);
  // shadoweeprom[cur_len] = charsercontext; //single char
  // shadoweeprom[cur_len + 1] = ',';
  // shadoweeprom[cur_len + 2] = '\0';
  // // cur_len = strlen(shadoweeprom);
  // cur_len += 2;

/* // the following is writing it to the serial with a bunch of Serial.print() commands 
   // we do not use this and instead concatenate everything into a char array using sprintf()
  // send it to the serial monitor
  if (hour<10)
  {
    Serial.print("0");
  }
  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute<10)
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second<10)
  {
    Serial.print("0");
  }
  Serial.print(second, DEC);
  Serial.print(" ");

  if (dayOfMonth<10)
  {
    Serial.print("0");
  }
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
   Serial.println();

// this can be suppressed because we do not write the weekday into the log file

  Serial.print(" Day of week: ");
  switch(dayOfWeek){
  case 1:
    Serial.println("Sunday");
    break;
  case 2:
    Serial.println("Monday");
    break;
  case 3:
    Serial.println("Tuesday");
    break;
  case 4:
    Serial.println("Wednesday");
    break;
  case 5:
    Serial.println("Thursday");
    break;
  case 6:
    Serial.println("Friday");
    break;
  case 7:
    Serial.println("Saturday");
    break;
  }
  */
// put here a conversion to print it into a string
// formatter "%02u" see https://www.tutorialspoint.com/c_standard_library/c_function_sprintf.htm
// char stringbuf[20]; goes globally
char* buf2 = stringbuf; // pointer to it stays declared here, resets the pointer when called
  // char* endofbuf = stringbuf + sizeof(stringbuf);
    /* i use 5 here since we are going to add at most 
       3 chars, need a space for the end '\n' and need
       a null terminator */
//second, minute, hour, dayOfWeek, dayOfMonth, month, year; convert all this
//buf2 += sprintf(buf2, "%02X", mfrc522.uid.uidByte[i]); from rfidreader
  buf2 += sprintf(buf2, "%02u", dayOfMonth);
  buf2 += sprintf(buf2, "/");
  buf2 += sprintf(buf2, "%02u", month);
  buf2 += sprintf(buf2, "/");  
  buf2 += sprintf(buf2, "%02u", year);
  buf2 += sprintf(buf2, " ");
  buf2 += sprintf(buf2, "%02u", hour);
  buf2 += sprintf(buf2, ":");
  buf2 += sprintf(buf2, "%02u", minute);
  buf2 += sprintf(buf2, ":");
  buf2 += sprintf(buf2, "%02u", second);
  //buf2 += sprintf(buf2, "\n"); //was there initially and works with Serial.print
  buf2 += sprintf(buf2, "\0"); // attention, contrary to receivedchars[ndx] = '\0'; we need here a double quotation mark !
  //i++;
// renamed the function getTime() instead displayTime()
} // end getTime()



void displayTime()
{
getTime();
Serial.print("Current date and time: ");
Serial.println(stringbuf);
} 



byte parse_char_dec(char c)
// convert a hex symbol to byte; here rather a decimal
{ if ( c >= '0' && c <= '9' ) return ( c - '0' );
 // if ( c >= 'a' && c <= 'f' ) return ( c - 'a' + 10 ); //this for hex numbers only
 // if ( c >= 'A' && c <= 'F' ) return ( c - 'A' + 10 ); //this for hex numbers only
  // if nothing,
  return 16;
  // or alternatively
  //  abort()
}



// from hex address converter in mifare card reader and recycled in mqtt address header protocol
byte twodecconvert(char twodecimal[]) {
  // convert two digit decimal charray into a byte (in other words, 0 to 99)
  if (strlen(twodecimal) == 2)
  { // convert the two hex characters into integers from 0 to 15, 16 if not a hex symbol
    byte upperhalfbyte = parse_char_dec(twodecimal[0]);
    byte lowerhalfbyte = parse_char_dec(twodecimal[1]);
    if ((upperhalfbyte == 16) || (lowerhalfbyte == 16))
    {
      Serial.println("Malformatted twodecimal - either char is not a decimal symbol");
    }
    else
    { //localAddress = upperhalfbyte * 0x10 + lowerhalfbyte; // hex version
      return upperhalfbyte * 10 + lowerhalfbyte;
    }
  }
  else
  {
    Serial.println("Malformed twodecimal - length != 2");
  }
}



void settime() {
  // function i wrote myself to set the clock from th ecommand prompt by calling typing settime dd/mm/yy d hh:mm:ss
  // uses a lot of strtok (string token) to decompose the argument
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  if (strstr(message, "settime ")) {
    char *strtokIndx = message + 8; // seems to work, in this context message means address of message
    if (strlen(strtokIndx) <= 20) {
      strcpy(stringbuf, strtokIndx);
     // char *buf2 = stringbuf;
 /* we could do something like this here based on atoi:
    strcpy(shadowchartargetip, chartargetip); // it will be destroyed
    strtokIndx = strtok(shadowchartargetip, ".");
    for (int i = 0; i <= 3; i++) {
      if (strtokIndx != NULL) {
        targetip[i] = atoi(strtokIndx);
      } */
   strtokIndx = strtok(stringbuf, "/");
    if (strtokIndx != NULL) {
       dayOfMonth = twodecconvert(strtokIndx);
    }
   strtokIndx = strtok(NULL, "/");
    if (strtokIndx != NULL) {
       month = twodecconvert(strtokIndx);
    }
    strtokIndx = strtok(NULL, " ");
    if (strtokIndx != NULL) {
       year = twodecconvert(strtokIndx);
    }  
   strtokIndx = strtok(NULL, " ");
    if (strtokIndx != NULL) {
       // dayOfMonth = parse_char_dec(strtokIndx);  // doesnt work because strtokIndex gives to an array 
       // but parse_char_dec() requires single char
        char einzel;
        strcpy(&einzel, strtokIndx); //typecast, take out the firt char at strtokIndx !!!
        dayOfWeek = parse_char_dec(einzel);
    }
    strtokIndx = strtok(NULL, ":");
    if (strtokIndx != NULL) {
       hour = twodecconvert(strtokIndx);
    }
    strtokIndx = strtok(NULL, ":");
    if (strtokIndx != NULL) {
       minute = twodecconvert(strtokIndx);
    }
    strtokIndx = strtok(NULL, ":"); // actually there is no more ":" but a null pointer at the end of stringbuf
    if (strtokIndx != NULL) {
       second = twodecconvert(strtokIndx);
    }         
  setDS3231time(second, minute, hour, dayOfWeek, dayOfMonth, month, year);
  Serial.println("Clock set successfully !");
     } else {
      Serial.println("Error: argument too long");
    }
 } else {
Serial.println("No argument");
 }
} //end settime



void fdelete() {
  // delete a file with given path
   if (strstr(message, "fdelete ")) {
    char *strtokIndx = message + 8; // seems to work, in this context message means address of message
    if (strlen(strtokIndx) <= 20) {
     // strcpy(stringbuf, strtokIndx);
     // char *buf2 = stringbuf;
     LittleFS.remove(strtokIndx);
     Serial.println();
     Serial.println("file deleted");
} else {
   Serial.println("Error: argument too long");
    }

 } else {
Serial.println("No argument");
 }
} // end fdelete


   
void fsformat() {
 // To format all space in LittleFS
    Serial.println();
    Serial.println("Formatting file system, all data will be lost");
    LittleFS.format();
} // end fsformat



void logfile() {
// sets the logfile path or name  
 if (strstr(message, "logfile ")) {
    char *strtokIndx = message + 8; // seems to work, in this context message means address of message
    if (strlen(strtokIndx) <= 20) {
      strcpy(logfilename, strtokIndx);
      Serial.println();
      Serial.println("log file set");
      } else {
   Serial.println("Error: argument too long");
    }
 } else {
Serial.println("No argument");
 }
} // end logfile

  

  void fsinfo() {
    // Get all information of your LittleFS
    // https://www.mischianti.org/2020/06/22/wemos-d1-mini-esp8266-integrated-littlefs-filesystem-part-5/
    FSInfo fs_info;
    LittleFS.info(fs_info);
    Serial.println();
    Serial.println("File system info");
    Serial.print("Total space:      ");
    Serial.print(fs_info.totalBytes);
    Serial.println(" Byte");
    Serial.print("Space used:       ");
    Serial.print(fs_info.usedBytes);
    Serial.println(" Byte");
    Serial.print("Block size:       ");
    Serial.print(fs_info.blockSize);
    Serial.println(" Byte");
    Serial.print("Page size:        ");
    Serial.print(fs_info.pageSize);  // corrected hdi: here was also totalBytes before
    Serial.println(" Byte");
    Serial.print("Max open files:   ");
    Serial.println(fs_info.maxOpenFiles);
    Serial.print("Max path lenght:  ");
    Serial.println(fs_info.maxPathLength);
    Serial.println();
    // Open dir folder
    Dir dir = LittleFS.openDir("/");
    // Cycle all the content
    while (dir.next()) {
        // get filename
        Serial.print(dir.fileName());
        Serial.print(" - ");
        // If element have a size display It else write 0
        if(dir.fileSize()) {
            File f = dir.openFile("r");
            Serial.print(f.size()); Serial.print(" Byte ("); Serial.print((f.size()/1024)); Serial.println(" kB)");
            f.close();
            }else{
            Serial.println("0");
        }
    }
   Serial.println();
  }  // end fsinfo



void readall() {
// from martinayotte
// outputs the entire file with the given path, kB by kB, uses read cache; attention to the delay, it avoids 
// UART overflow, probably a 1 kB fifo buffer
// https://www.esp8266.com/viewtopic.php?f=32&t=4570
 if (strstr(message, "readall ")) {
    char *strtokIndx = message + 8; // seems to work, in this context message means address of message
    if (strlen(strtokIndx) <= 20) {
   // strcpy(logfilename, strtokIndx);
   // File readFile = LittleFS.open("/log.txt", "r");
File readFile = LittleFS.open(strtokIndx, "r");
    if (!readFile) {
      Serial.println();
      Serial.println("Can't open littleFS file !");         
    }
    else {
      char rcache[lcachesize];
      long siz = readFile.size();
      Serial.println();
      Serial.print("reading entire file : "); Serial.print(siz); Serial.println(" Byte");
      while(siz > 0) {
        // size_t len = std::min((int)(sizeof(buf) - 1), siz);
        //  size_t len = std::min((int)(sizeof(writebuf) - 1), siz);
        size_t len = std::min((lcachesize-1), siz);
        readFile.read((uint8_t *)rcache, len);
        // cache[len] = 0; 
        rcache[len] = '\0';
        //str += buf;
        Serial.print(rcache);
        delay(500); // afraid of UART overflow !
        // siz -= sizeof(cache) - 1;
        siz -= (lcachesize-1);
      }
      readFile.close();
      //server.send(200, "text/plain", str);
    }
   } else {
   Serial.println("Error: argument too long");
    }
 } else {
Serial.println("No argument");
 }
} // end readall



void readlast() {
// reads up to 1 lcachesize (aka kB) into cache, if file is bigger, 1 lcachesize, if smaller, filesize  
 if (strstr(message, "readlast ")) {
    char *strtokIndx = message + 9; // seems to work, in this context message means address of message
    if (strlen(strtokIndx) <= 20) {
   // strcpy(logfilename, strtokIndx);
   // File readFile = LittleFS.open("/log.txt", "r");
File readFile = LittleFS.open(strtokIndx, "r");
    if (!readFile) {
      Serial.println();
      Serial.println("Can't open LittleFS file !");         
    }
    else {
      Serial.println();
      Serial.println("reading last kB of file");
      char rlcache[lcachesize];
      long siz = readFile.size();
       // Serial.println("read filesize : "); Serial.println(siz); // this to check whether it goes until here
      // while(siz > 0) { not used in readlast because there is only one buffer filling read cycle with min(buffer_length, siz) chars
        // size_t len = std::min((int)(sizeof(buf) - 1), siz);
        // size_t len = std::min((int)(sizeof(writebuf) - 1), siz);
        size_t len = std::min((lcachesize-1), siz);
        // new in readlast: set the cursor back from the end by min(buffer_length, siz):
        readFile.seek(len, SeekEnd);
        readFile.read((uint8_t *)rlcache, len);
        // cache[len] = 0; 
        rlcache[len] = '\0';
        //str += buf;
        Serial.print(rlcache);
        // siz -= sizeof(cache) - 1;
        // siz -= (lcachesize-1);
      // } // end while(siz > 0) loop, not used in readlast
      readFile.close();
      //server.send(200, "text/plain", str);
    }
   } else {
   Serial.println("Error: argument too long");
    }
 } else {
Serial.println("No argument");
 }
} // end readlast



void readkbn() {
 char readfilename[20];
 long kbn;
// reads up to 1 lcachesize from cursor position (in lcachesize) into cache, if remainder of file is bigger, 1 lcachesize, if smaller, remainder  
 if (strstr(message, "readkbn ")) {
    char *strtokIndx = message + 8; // seems to work, in this context message means address of message
        if (strlen(strtokIndx) <= 25) {
        strcpy(stringbuf, strtokIndx);  
        strtokIndx = strtok(stringbuf, " ");
         if (strtokIndx != NULL) {
         strcpy(readfilename, strtokIndx);
         }
   // strcpy(readfilename, strtokIndx);
   // File readFile = LittleFS.open("/log.txt", "r");
File readFile = LittleFS.open(readfilename, "r");
    if (!readFile) {
      Serial.println("Can't open LittleFS file !");         
    }
    else {
       // Serial.println("beginning, next is reading filesize");
      strtokIndx = strtok(NULL, " ");
      if (strtokIndx != NULL) {
      kbn = atoi(strtokIndx);
      }
      char rkbcache[lcachesize];
      long siz = readFile.size();
      if (kbn*(lcachesize-1)>siz) {
        Serial.println("Error, position > filesize");
      } else {
      Serial.println();
      Serial.print("reading file from kB : "); Serial.print(kbn); Serial.print("/"); Serial.println((siz/(lcachesize-1))); // this to check whether it goes until here
      // while(siz > 0) { not used in readlast because there is only one buffer filling read cycle with min(buffer_length, siz) chars
        // size_t len = std::min((int)(sizeof(buf) - 1), siz);
        // size_t len = std::min((int)(sizeof(writebuf) - 1), siz);
        size_t len = std::min((lcachesize-1), (siz-kbn*(lcachesize-1)));
        readFile.seek(kbn*(lcachesize-1), SeekSet);
        readFile.read((uint8_t *)rkbcache, len);
        // rkbcache[len] = 0; 
        rkbcache[len] = '\0';
        //str += buf;
        Serial.print(rkbcache);
      }
      readFile.close();
      //server.send(200, "text/plain", str);
    }
   } else {
   Serial.println("Error: argument too long");
    }
 } else {
Serial.println("No argument");
 }
} // end readkbn



void loadcache() {
  // this function only checks if logfile exists, and empties the cache and resets the freespace counter
File readFile = LittleFS.open(logfilename, "r");
    if (!readFile) {
      Serial.println("Can't open log file. For a new file this is normal.");  
      Serial.println("It will be created during first commit.");  
      // in this case the file doesnt exist but will be created 
      // in the commitcache fct; therefore also empty the cache and set the freespace
      strcpy(gcache, ""); 
      freespace = gcachesize -1;
    }
    else {
      strcpy(gcache, ""); // making the cache empty
      freespace = gcachesize -1;
      readFile.close();
    }
} // end loadcache



void commitcache() {
// open file for write and append cache
// note that this is irrespective of whether the cache is a full block or not
// close file
File writeFile = LittleFS.open(logfilename, "a"); //a instead w appends
    if (writeFile){
        writeFile.print(gcache);
        writeFile.close();
    }else{
        Serial.println("Problem on opening file for appending in commitcache()");
    }
} // end commitcache()



void cachedata(char userdata[180]) {
// append data to the cache in RAM until it is full; if data exceeds free cache, then fill until the end of cache, 
// save it to flash, and continue appending the cache thereafter
  getTime();
  strcpy(inputcharray, stringbuf);
  strcat(inputcharray, " ");
  strcat(inputcharray, userdata);
  strcat(inputcharray, "\r\n");
  if (freespace >= strlen(inputcharray)) { 
  // Serial.print("Appending to RAM cache, "); Serial.print(freespace);  Serial.println(" Bytes available"); 
  strcat(gcache, inputcharray); // using global variable
  freespace -= strlen(inputcharray);
  wrap = false; // clear the wrapparaound flag
    } else {
  Serial.println("RAM Cache overflow, committing to flash and wrapping around");
  strncat(gcache, inputcharray, freespace); // watch the "n" in strncat
  long oldfreespace = freespace; // how much of the inputcharray has now been cached
  commitcache(); // then after committing a full block, 
  loadcache(); //   the freespace is reset to cachesize in the loadcache fct and gcache cleared
  strcat(gcache, inputcharray + oldfreespace); // cache the remainder of inputcharray
  // same kind of pointer arithmetics as in: char *strtokIndx = message + 8; 
  // in this context, inputcharray means address of inputcharray
  freespace -= (strlen(inputcharray) - oldfreespace);
  wrap = true; // set the wrapparaound flag
    }
} // end cachedata



void customentry() {
   // creating a custom entry with text from the keyboard
   if (strstr(message, "customentry ")) {
    char *strtokIndx = message + 12; // seems to work, in this context message means address of message
    if (strlen(strtokIndx) <= 180) {
     // strcpy(stringbuf, strtokIndx);
     // char *buf2 = stringbuf;
     cachedata(strtokIndx); // take the argument behind the customentry command and generate a log entry from it
     Serial.println();
     Serial.println("log entry created");
} else {
   Serial.println("Error: argument too long");
    }
 } else {
Serial.println("No argument");
 }
} // end customentry



void controlledshutdown() {
// if desired, create a logfile entry to protocolize the shutdown itself:
// timestamp, circumstances of shutdown (upon user request, voltage drop etc.);
// we put one into the command parser to protocolize a user requested shutdown
// and another in the routine that shuts down upon pushbuton pressed
Serial.println();

// write back cache into flash
commitcache();
Serial.println("Cache committed to flash memory");

// terminate other open routines, housekeeping
Serial.println("Sending all processes the term signal");

// if the hardware features power management, suspend the power hold pin, enable input of converter etc
Serial.println("Suspending processor power");
// e.g., reset the pin for the autopowerhold
// digitalWrite(POWERHOLD, LOW);

// If not, deepsleep, halt etc., also additionally to hardware shutdown doesnt hurt
// in case of successful hardware shutdown, the following will be skipped :)))))))))
Serial.println("Going to deepsleep, halting system...");
ESP.deepSleep(0);
}  // end controlledshutdown()



void readcache() {
  // reads out the cache by 1 kB (lcachesize-1) pieces
  Serial.println();
  // Serial.print("reading cache, "); Serial.print(gcachesize-freespace-1); Serial.println(" Bytes");
      char rbcache[lcachesize];
      long siz = strlen(gcache);
      char *gcacheptr = gcache;
      Serial.println();
      Serial.print("reading cache, "); Serial.print(siz); Serial.println(" Byte");
      //Serial.println();
      // it always crashes when reading more than 2 lcachesizes, so we put a delay between Serial.print() commands
        while(siz > 0) {
        size_t len = std::min((lcachesize-1), siz);
        //readFile.read((uint8_t *)rcache, len);
        strncpy(rbcache, gcacheptr, len); // note the "n" in strncpy; this is not terminated
        //rbcache[len] = 0; 
        rbcache[len] = '\0'; // therefore terminate it
        //strcat(rbcache, "\0"); //???
        Serial.print(rbcache);
        delay(1000); // afraid of saturating the UART buffer
        siz -= (len);
        gcacheptr += (lcachesize-1);
      }
} // end readcache()



void loggingon() {
  // turns on auto logging
lastentry = millis(); // reset timer so it doesnt show a huge time lapse on the first entry of the test routine 
cachedata("logging started"); // also leave a log entry to keep track of what time auto logging was switched on
logging = true;
Serial.println();
Serial.println("Logging enabled");
}



void loggingoff() {
 // turns off auto logging
logging = false;
cachedata("logging stopped"); // also leave a log entry to keep track of what time auto logging was switched ff
Serial.println();
Serial.println("Logging disabled");
}



void helpscreen() {
 Serial.println();
 Serial.println();
 Serial.println("Data logger with command prompt, realtime clock and cache memory");
 Serial.println("Recording your log data on ESP8266 internal flash memory"); 
 Serial.println("Prior RAM caching allows writing larger portions, reducing wear and fragmentation"); 
 Serial.println();
 Serial.println("help - displays (this) help screen");
 Serial.println("fsinfo - shows file system and directory info");
 Serial.println("showtime - retrieves and displays date and time from the realtime clock module");
 Serial.println("settime <dd/mm/yy d hh:mm:ss> - sets the clock module");
 Serial.println("                  ^ weekday d: 1=sunday to 7=saturday");
 Serial.println("logging on/off - switches logging on or off");
 Serial.println("readlast </path> - reads out the last kB of the file");
 Serial.println("readall </path> - reads out the whole file");
 Serial.println("readkbn </path> <n> - reads out a kB of the file from position n in kB");
 Serial.println("fdelete </path> - deletes file");
 Serial.println("logfile </path> - sets the logfile; defaults to /log.txt");
 Serial.println("fsformat - format file system");
 Serial.println("shutdown - commit and prepare system halt");
 Serial.println();
 Serial.println("Functions for test purposes :");
 Serial.println("customentry <text> - creates a log entry with timestamp and user text");
 Serial.println("readcache - display cache content");
 Serial.println("commit - manually forces dumping the cache content to flash");
 Serial.println("         makes sense before reading the log file");
 Serial.println();
  }  // end helpscreen()



// command parser
void cmdparse() {
  if (strstr(message, "help") == messptr) {
    helpscreen();
  }
  else if (strstr(message, "showtime") == messptr) {
    displayTime();
  }
  else if (strstr(message, "settime") == messptr) {
     settime();
  }
  else if (strstr(message, "readall") == messptr) {
     readall();
  }
   else if (strstr(message, "fdelete") == messptr) {
     fdelete();
  }
   else if (strstr(message, "fsinfo") == messptr) {
     fsinfo();
  }
  else if (strstr(message, "customentry") == messptr) {
     customentry();
  }
  else if (strstr(message, "logfile") == messptr) {
     commitcache();
     logfile();
     loadcache(); // 
  }
  else if (strstr(message, "readlast") == messptr) {
     readlast();
  }
   else if (strstr(message, "commit") == messptr) {
     commitcache(); loadcache();
  }
  else if (strstr(message, "shutdown") == messptr) {
     cachedata("shutdown from command prompt");
     controlledshutdown();
  }
   else if (strstr(message, "readcache") == messptr) {
     readcache();
  }
  else if (strstr(message, "fsformat") == messptr) {
     fsformat();
  }
  else if (strstr(message, "logging on") == messptr) {
     loggingon();
  }
  else if (strstr(message, "logging off") == messptr) {
     loggingoff();
  }
   else if (strstr(message, "readkbn") == messptr) {
     readkbn();
  }
  else Serial.println("Command unknown");
 }  // end cmdparse()




void setup()
{
  Wire.begin();
  Serial.begin(9600);
  // set the initial time here:
  // DS3231 seconds, minutes, hours, day, date, month, year
  // setDS3231time(30,42,21,4,26,11,14);
 
delay(500);

helpscreen();
 
    Serial.print("Inizializing FS...  ");
    if (LittleFS.begin()){
        Serial.println("- done.");
    }else{
        Serial.println("fail.");
    }

fsinfo(); // show file system info

displayTime(); // show current time

Serial.println();
Serial.print("Current log file is: "); Serial.println(logfilename);

loadcache(); // reset the cache
lastentry = millis(); // reset the timer of last log entry

pinMode(D4, OUTPUT); //gpio 2, one of the mode determining pins at startup, but also the one to which the integrated LED is connected
// configure it as output to be able to blink the blue LED, but turn it off for the moment
digitalWrite(D4, HIGH); //inverse logic, off at high

// configure pins for the pushbutton that is normally open and ties D5 to Gnd if pressed
pinMode(D5, INPUT_PULLUP); // gpio14, pullup, this input causes a shutdown if set low
pinMode(D0, OUTPUT); // gpio16
digitalWrite(D0, LOW); // create an artificial Gnd for the pushbutton, because this pin is the neighbor of D5 and Gnd is already used to
// power the real time clock module
} // end setup




void loop()
{
  // input routines

  // handle incoming data from serial
  while (Serial.available() && newdata == false) {
    lastchar = millis();
    rc = Serial.read();
    if (rc != '\n') {
      //if (rc != '\r')  //suppress carriage return !
      { message[ndx] = rc;
        ndx++;
      }
      if (ndx >= numchars) {
        ndx = numchars - 1;
      }
    }
    else {
      message[ndx] = '\0';
      ndx = 0;
      newdata = true;
    }
  }
  // put the 2s timeout in case the android terminal cannot terminate
  if (ndx != 0 && millis() - lastchar > 2000) {
    message[ndx] = '\0';
    ndx = 0;
    newdata = true;
  }

// when a command is received, parse it
 if (newdata) {
 cmdparse();
 newdata = false;
 }


 // here the test routine, a stupid program that creates a log entry every 2 seconds and measures if the previous log entry
 // was really created 2 seconds ago; with this we will be able to see if execution of some functions, in particular the flash
 // writing that occurs at full cache memory, take excessively long;
 // this is the part that the user can later replace by his/her own program that reads sensor data every interval and
 // creates a log entry from it;
 
if (logging) {
  if (millis() - lastentry > 2000) {
  
  // toggle LED so it blinks slowly, 2s on 2s off
  if (digitalRead(D4)) {
  digitalWrite(D4, LOW);} else {
     digitalWrite(D4, HIGH);
    } 
   dtostrf((millis() - lastentry), 0, 0, userdata); // measure the time really elapsed since last log entry and convert to char array
   // if flash writing takes long or the user enters commands that take time, it will show in the log as >> 2000
   strcat(userdata, " ms delay since last entry");
   if (wrap) {
    strcat(userdata, "; newcache"); // appends "newcache" if a flash commit was necessary due to full cache
  }
  cachedata(userdata);
  lastentry = millis();
} // end if millis...
} // end if logging
else {
  // turn off the LED so it doesnt stay on 
  digitalWrite(D4, HIGH);
}



// hardware shutdown with switch, check if switch pressed and if so, shut down the data logger
  if (!digitalRead(D5)) {
    cachedata("shutdown by hardware switch");
     controlledshutdown();
  }


  
 } // end main loop
