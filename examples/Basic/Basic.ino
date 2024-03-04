//=============================================================================
/* This example describes the basic functionality and the simplest use of the
 * EEBoom library. For more examples, see the project page on GitHub.
*/
#include <eeboom.h>

struct EE {
    int         devID = 0XEFBEADDA;     //it will be a default value
    short       step;                   //at the first init will be equal to zero
};

EEBoom<EE>      ee;

void setup() {
    ee.begin();                         //will use two default sectors
}

void loop() {
    ee.data.step++;                     //you can do it realy often -
    ee.commit();                        //this case about 340 times per sector!!!
    delay(1000);
}
