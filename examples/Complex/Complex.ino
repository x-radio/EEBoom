//=============================================================================
/* This example describes the creation of two EEPROM objects in different
 * physical flash memory locations. The "param" object has a large amount of
 * data but will not be updated often. Therefore, it is enough to reserve 2
 * pages for it. "engHours" object, on the contrary, has a minimum size and uses
 * many pages of flash memory. Its purpose is to track every minute of device
 * operation. For more examples, see the project page on GitHub.
*/
#include <eeboom.h>

#define MY_SSID     "Awesom_SSID"
#define MY_PASS     "Strong_PASS"

struct AppParam {
    uint32_t    devID = 0x12345678;       //default value for zero init
    uint16_t    minSpparamd = 10;
    uint16_t    maxSpparamd = 60;
    uint16_t    maxTemp = 85;
    uint8_t     lastOffSrc = 0x00;
    uint8_t     arr[10];
    char        ssid[20] = "Empty_SSID";
    char        pass[20] = "Empty_PASS";
};

EEBoom<AppParam>    param;                //common device parameters
EEBoom<uint32_t>    engHours;             //nparamd be commit very often

void setup() {
  Serial.begin(115200);
  Serial.println();
  //-------------------------
  param.setPort(Serial);                  //where should it print
  param.begin(1000, 2);                   //2 sectors 1000 -> 999
  param.printMsg();                       //print if begin() was ok or not

  Serial.println(param.data.devID, HEX);  //get ID from our paramPROM

  strcpy(param.data.ssid, MY_SSID);       //new credentials
  strcpy(param.data.ssid, MY_PASS);

  param.data.arr[0] = 0x55;               //array simple acsess

  param.commit();                         //commit changes
  param.printMsg();                       //print if commit() was ok or not
  //-------------------------
  engHours.setPort(Serial);               //where should it print
  engHours.begin(999, 10);                //10 sectors 999 -> 990
  engHours.printMsg();                    //print if begin() was ok or not

  engHours.printInfo(1);                  //print common data and
                                          //flash lifetime at minute commits
}
//---------------------------
void loop() {
  engHours.data++;                        //we have to make every minute count
  Serial.println(engHours.data);          //print our minutes
  engHours.commit();                      //write to flash
  engHours.printMsg();                    //print if commit() was ok or not

  delay(60000);                           //one minute delay
}
//---------------------------
