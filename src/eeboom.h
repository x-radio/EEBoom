//=============================================================================
//=============================================================================
/**/
//=============================================================================
#ifndef EEBOOM_H
#define EEBOOM_H
//=============================================================================
class EEBoomBase
{
protected:
    enum{NO_MSG, INIT_OK, CANT_ERASE, ZERO_INIT, CANT_WRITE, NO_PARTITION, COMMIT_OK};
    static constexpr uint16_t   sectSize = SPI_FLASH_SEC_SIZE;
    uint16_t                    firstSect;
    uint16_t                    sectCnt;
    uint32_t                    eeBase;
    uint16_t                    slotSize;
    uint16_t                    dataSize;
    uint8_t                     ds;
    uint16_t                    slotCnt = sectSize / slotSize;
    uint8_t                     *slot;
    uint8_t                     *data;
    uint16_t                    *crc;
    uint16_t                    crntSect;
    uint16_t                    crntAddr;
    uint8_t                     lastMsg = NO_MSG;
    Stream                      *stream = nullptr;

    EEBoomBase(uint16_t SlotSize, const uint16_t DataSize, uint8_t Ds, uint8_t *Slot, uint8_t *Data, uint16_t *Crc):
        slotSize(SlotSize), dataSize(DataSize), ds(Ds),
        slot(Slot), data(Data), crc(Crc) {};

    uint16_t getCheckSumm(uint8_t* addr);
    bool sectIsClr();

    public:
    bool begin(uint16_t FirstSect, uint16_t SectCnt);
    bool begin(uint16_t SectCnt);
    bool begin();
    #if ESP32
    bool begin(const char* Partition);
    bool begin(const char* Partition, uint16_t SectCnt);
    #endif

    bool commit();
    bool goZero();

    void dump();
    void dump(int16_t Sector);
    void dump(int16_t Sector, int16_t FirstLine, uint16_t NmbLine);
    void dump(int16_t Sector, int16_t FirstLine, uint16_t NmbLine, uint8_t FirstColumn, uint8_t NmbColumn);

    void printInfo(uint32_t CommitIntrvlMin = 0);
    void setPort(Stream& Port);
    void printMsg();
    uint16_t currentSect();
};
//----------------------------
template <typename Type>
class EEBoom : public EEBoomBase
{
    private:
    static constexpr uint16_t dataSize = sizeof(Type);
    static constexpr uint16_t ds = (dataSize % 4 != 3) ? 2 - (dataSize % 4) : 3;

    #pragma pack(push, 1)
    struct Slot
    {
        Type        dt;
        uint8_t     dummy[ds];
        uint16_t    crc;
    };
    #pragma pack(pop)

    Slot            slot;
    uint8_t         *dataPntr = (uint8_t*)&slot.dt;

    public:
    EEBoom(): EEBoomBase(sizeof(Slot), dataSize, ds, (uint8_t*)&slot, (uint8_t*)&slot.dt, &slot.crc) {};
    Type&           data = slot.dt;

    static_assert(sizeof(Slot) % 4 == 0, "Caution, EEBoom slot is not a multiple of 4");
    static_assert(sizeof(Slot) < sectSize-2, "Caution, EEBoom slot is too large");
};
//=============================================================================
class EETool
{
    public:
    #if ESP32
    enum{NOT_FOUND = 0xFFFF};
    static uint16_t spiffsLastSector();
    static uint16_t app1LastSector();
    static void printRanges(Stream& Port);
    static uint16_t lastSectorByName(const char* Name);
    #endif
};
//=============================================================================
#endif
