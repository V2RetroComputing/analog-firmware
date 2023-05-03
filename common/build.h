#define BUILDDATE 0x20230502
#define BUILDID 0x0182
#define BUILDSTR " 2 May 2023 Build 0182"

#ifdef ANALOG_GS
#define HWSTRING "     V2 Analog GS Rev1"
#define HWBYTE 'G'
#define HWREV '1'
#elif defined(ANALOG_WIFI)
#define HWSTRING "   V2 Analog WiFi Rev1"
#define HWBYTE 'W'
#define HWREV '1'
#else
#define HWSTRING "     V2 Analog LC Rev1"
#define HWBYTE 'L'
#define HWREV '1'
#endif
