
//=============================================================================
/**/
//=============================================================================
#ifndef EEBOOM_CPP
#define EEBOOM_CPP
//=============================================================================
#include "Arduino.h"
#include "eeboom.h"
//=============================================================================
#define DFLT_SECT_CNT           2
#define FLASH_LIFE_CYCLES       10000
#define CHUNK_SIZE              (1<<6)
//=============================================================================
bool EEBoomBase::begin(uint16_t FirstSect, uint16_t SectCnt)
{
    uint8_t             temp;                                           //valid slot flag
    uint16_t            oldSect = 0;
    uint16_t            oldAddr = 0;
    uint32_t            fullAddr;
    uint32_t            oldFullAddr = 0;
    uint32_t            offset;                                         //fullAddr offset - step through whole slot
    uint8_t             chunk[CHUNK_SIZE];
    uint16_t            crntSumm = 0;                                   //check summ accum
    uint16_t            size = 0;                                       //size of crnt chunk

    if(SectCnt == 0) return false;

    sectCnt = SectCnt;
    firstSect = FirstSect;
    eeBase = firstSect * sectSize;

	crntSect = temp = 0;
    for(uint8_t i= 0; i < sectCnt; i++)
    {
        crntAddr = 0;
        fullAddr = eeBase-sectSize*i;
        for(uint16_t x = 0; x < slotCnt; x++)
        {
            offset = crntSumm = 0;
            while (offset < slotSize)
            {
                if(offset+CHUNK_SIZE < slotSize)
                    size = CHUNK_SIZE;
                else
                    size = slotSize - offset;                           //if last chunk

                spi_flash_read(fullAddr+offset, (uint32_t*)chunk, size);

                if(offset+CHUNK_SIZE < slotSize)
                    calculateSumm(&crntSumm, chunk, size);
                else
                    calculateSumm(&crntSumm, chunk, size-ds-2);         //exept dummy and crc fields in last chunk

                offset += CHUNK_SIZE;
            }
            if(crntSumm == *(uint16_t*)(chunk+size-2))                  //if checksumm from last chunk is equal to calculated one
                temp = 1;                                               //in case of a good copy leave a flag
            else
                if(temp == 1) goto profit;                              //if the copy is broken but there have been some good ones brake cycles
                else break;                                             //if the copy is broken and there was no valid one then next page

            oldFullAddr = fullAddr;
            oldSect = crntSect;                                         //if it has not left the loop after the first pass (zero address), then the page will not need to be changed
            oldAddr = crntAddr;
            fullAddr += slotSize;
            crntAddr++;
        }
        crntSect++;
    }
    if(temp == 1) goto profit;                                          //in case of single-page working
    lastMsg = ZERO_INIT;
    crntAddr = -1;
    crntSect = 0;
    return true;
    profit:                                                             //if the data is found, load it
    crntAddr = oldAddr;                                                 //return the addresses to the last valid ones
    crntSect = oldSect;
    spi_flash_read(oldFullAddr, (uint32_t*)slot, slotSize);             //load the finished slot
    lastMsg = INIT_OK;
    return true;
}
//----------------------------
bool EEBoomBase::begin(uint16_t SectCnt)
{
    #if ESP8266
    return begin((ESP.getFlashChipSize()>>12)-5, SectCnt);
    #elif ESP32
    uint16_t sect = EETool::spiffsLastSector();                         //section search
    if(sect == EETool::NOT_FOUND){lastMsg = NO_PARTITION; return false;}//if not found
    return begin(sect, SectCnt);                                        //if found
    #endif
}
//----------------------------
bool EEBoomBase::begin()
{
    #if ESP8266
    return begin((ESP.getFlashChipSize()>>12)-5, DFLT_SECT_CNT);
    #elif ESP32
    uint16_t sect = EETool::spiffsLastSector();                         //section search
    if(sect == EETool::NOT_FOUND){lastMsg = NO_PARTITION; return false;}//if not found
    return begin(sect, DFLT_SECT_CNT);                                  //if found
    #endif
}
//----------------------------
#if ESP32
bool EEBoomBase::begin(const char* Partition)
{
    uint16_t sect = EETool::lastSectorByName(Partition);                //section search
    if(sect == EETool::NOT_FOUND){lastMsg = NO_PARTITION; return false;}//if not found
    return begin(sect, DFLT_SECT_CNT);                                  //if found
}
//----------------------------
bool EEBoomBase::begin(const char* Partition, uint16_t SectCnt)
{
    uint16_t sect = EETool::lastSectorByName(Partition);                //section search
    if(sect == EETool::NOT_FOUND){lastMsg = NO_PARTITION; return false;}//if not found
    return begin(sect, DFLT_SECT_CNT);                                  //if found
}
#endif
//----------------------------
bool EEBoomBase::commit()
{
    uint8_t   oldSect = crntSect;                                       //save old addresses
    uint16_t  oldAddr = crntAddr;
    uint8_t   flag = 0;                                                 //the page has changed and we have to erase the flash after recording, the next intit can stay on the old...

    *crc = 0;
    calculateSumm(crc, data, dataSize);

    if(lastMsg == NO_PARTITION) return false;

    if(++crntAddr == slotCnt)                                           //change address
    {                                                                   //if sector is finished
        crntAddr = 0;                                                   //address will be a null
        flag = 1;
        if(++crntSect == sectCnt)                                       //change sector
            crntSect = 0;
    }
    if(crntAddr == 0 && !sectIsClr())                                   //if it's a new page and it's not blank.
    {
        spi_flash_erase_sector(firstSect - crntSect);                   //erase it
        if(!sectIsClr())                                                //if still not blank
        {
            crntAddr = oldAddr;                                         //rollback addresses
            crntSect = oldSect;
            lastMsg = CANT_ERASE;
            return false;
        }
    }
    uint32_t fullAddr = eeBase - sectSize*crntSect + slotSize*crntAddr;
    if(spi_flash_write(fullAddr, (uint32_t*)slot, slotSize) != 0)
    {
        crntAddr = oldAddr;                                             //if commit fails, roll back
        crntSect = oldSect;
        lastMsg = CANT_WRITE;
        return false;
    }

    if(flag && sectCnt > 1)                                             //if page is changed, erase the old one (if there is only one page, it has already been erased)
        spi_flash_erase_sector(firstSect - oldSect);

    lastMsg = COMMIT_OK;
    return true;
}
//----------------------------
bool EEBoomBase::goZero()
{
    if(crntSect == 0) return true;
    crntAddr = -1;
    crntSect = 0;
    return(commit());
}
//----------------------------
void EEBoomBase::dump()
{
    dump(-1, -1, 3);
}
//----------------------------
void EEBoomBase::dump(int16_t Sector)
{
    dump(Sector, 0, sectSize/32, 0, 32);
}
//----------------------------
void EEBoomBase::dump(int16_t Sector, int16_t FirstLine, uint16_t NmbLine)
{
    dump(Sector, FirstLine, NmbLine, 0, 32);
}
//----------------------------
void EEBoomBase::dump(int16_t Sector,
                        int16_t FirstLine, uint16_t NmbLine,
                        uint8_t FirstColumn, uint8_t NmbColumn){
    uint8_t data;
    uint32_t sectorAddr;
    uint16_t lineTotal = sectSize>>5;
    uint16_t crntLine;
    uint16_t hnl = NmbLine>>1;

    if(!stream) return;

    if(FirstLine == -1)
    {
        crntLine = (crntAddr*slotSize)>>5;
        FirstLine = crntLine - hnl;
        if(FirstLine < 0) FirstLine = 0;
    }

    if(FirstLine+NmbLine > lineTotal) FirstLine = lineTotal-NmbLine;

    if(Sector == -1) Sector = firstSect-crntSect;
    sectorAddr = Sector*sectSize;


    stream->printf("Sector %d:\r\n", Sector);

    for(uint16_t l=FirstLine; l<FirstLine+NmbLine; l++)
    {
        stream->printf("%03d: ", l);

        uint32_t lineAddr = sectorAddr + l*32;

        for(uint8_t c=FirstColumn; c<FirstColumn + NmbColumn; c++)
        {
            #if ESP32
            ESP.flashRead(lineAddr+c, (uint32_t*)&data, 1);
            #elif ESP8266
            ESP.flashRead(lineAddr+c, &data, 1);
            #endif
            stream->printf("%02X ", data);
        }
        stream->println();
    }
}
//----------------------------
void EEBoomBase::printInfo(uint32_t CommitIntrvlMin)
{
    if(!stream) return;
    stream->printf("User data size: %d\r\n", dataSize);
    stream->printf("Whole slot size: %d\r\n", slotSize);
    stream->printf("First sector: %d\r\n", firstSect);
    stream->printf("Sector count: %d\r\n", sectCnt);
    stream->printf("Slots per sector: %d\r\n", slotCnt);
    stream->printf("Current sect: %d\r\n", firstSect-crntSect);

    if(!CommitIntrvlMin) return;
    uint32_t lt = CommitIntrvlMin*sectCnt*slotCnt*FLASH_LIFE_CYCLES;

    stream->printf("Approximate flash life time: %d days\r\n", lt/24/60);
}
//----------------------------
void EEBoomBase::calculateSumm(uint16_t *summ, uint8_t *chunk, uint16_t size)
{
    for(uint16_t i = 0; i < size; i++)
    {
        *summ = (*summ + *chunk) * 44111 + 1;
        chunk++;
    }
}
//----------------------------
bool EEBoomBase::sectIsClr()
{
    uint32_t buff[16];
    uint8_t cycles = sectSize/sizeof(buff);
    uint32_t fullAddr = eeBase - sectSize*crntSect;
    uint32_t *pntr;
    while(cycles--)
    {
        ESP.flashRead(fullAddr, (uint32_t*)buff, sizeof(buff));
        pntr = buff;
        for(uint16_t i = 0; i < sizeof(buff)/4; i++)
            if(*pntr++ != 0xffffffff) return false;
    }
    return true;
}
//----------------------------
uint16_t EEBoomBase::currentSect()
{
    return firstSect-crntSect;
}
//----------------------------
void EEBoomBase::setPort(Stream& Port)
{
    stream = &Port;
}
//----------------------------
void EEBoomBase::printMsg()
{
    if(!stream) return;                                                 //if there's no stream

    switch(lastMsg)
    {
        case NO_MSG:        break;
        case INIT_OK:       stream->println("EEBoom init ok"); break;
        case COMMIT_OK:     stream->println("EEBoom commited"); break;
        case ZERO_INIT:     stream->println("EEBoom zero init"); break;
        case CANT_ERASE:    stream->println("EEBoom can't erase"); break;
        case CANT_WRITE:    stream->println("EEBoom can't write"); break;
        case NO_PARTITION:  stream->println("EEBoom partition not found"); break;
    }
}
//----------------------------
#if ESP32
uint16_t EETool::spiffsLastSector()
{
    const esp_partition_t *spiffs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if(spiffs == NULL) return NOT_FOUND;
    return (spiffs->address>>12) + (spiffs->size>>12) - 1;
}
//----------------------------
uint16_t EETool::app1LastSector()
{
    const esp_partition_t *app1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "app1");
    if(app1 == NULL) return NOT_FOUND;
    return (app1->address>>12) + (app1->size>>12) - 1;
}
//----------------------------
uint16_t EETool::lastSectorByName(const char* Name)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, Name);
    if(part == NULL) return NOT_FOUND;
    return (part->address>>12) + (part->size>>12) - 1;
}
//----------------------------
void EETool::printRanges(Stream& Port)
{
    const esp_partition_t *spiffs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    const esp_partition_t *app1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "app1");
    if(spiffs) Port.printf("spiffs sectors range: %d - %d\r\n", spiffs->address>>12, (spiffs->address>>12) + (spiffs->size>>12) - 1);
    else Port.println("spiffs not found");
    if(app1) Port.printf("app1 sectors range: %d - %d\r\n", app1->address>>12, (app1->address>>12) + (app1->size>>12) - 1);
    else Port.println("app1 not found");
}
#endif
//=============================================================================
#endif
