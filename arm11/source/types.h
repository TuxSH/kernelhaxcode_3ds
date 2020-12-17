#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/// The maximum value of a u64.
#define U64_MAX	UINT64_MAX

/// would be nice if newlib had this already
#ifndef SSIZE_MAX
#ifdef SIZE_MAX
#define SSIZE_MAX ((SIZE_MAX) >> 1)
#endif
#endif

typedef uint8_t u8;   ///<  8-bit unsigned integer
typedef uint16_t u16; ///< 16-bit unsigned integer
typedef uint32_t u32; ///< 32-bit unsigned integer
typedef uint64_t u64; ///< 64-bit unsigned integer

typedef int8_t s8;   ///<  8-bit signed integer
typedef int16_t s16; ///< 16-bit signed integer
typedef int32_t s32; ///< 32-bit signed integer
typedef int64_t s64; ///< 64-bit signed integer

typedef volatile u8 vu8;   ///<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; ///< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; ///< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; ///< 64-bit volatile unsigned integer.

typedef volatile s8 vs8;   ///<  8-bit volatile signed integer.
typedef volatile s16 vs16; ///< 16-bit volatile signed integer.
typedef volatile s32 vs32; ///< 32-bit volatile signed integer.
typedef volatile s64 vs64; ///< 64-bit volatile signed integer.

typedef u32 Result;

/// Creates a bitmask from a bit number.
#define BIT(n) (1U<<(n))

/// Aligns a struct (and other types?) to m, making sure that the size of the struct is a multiple of m.
#define ALIGN(m)   __attribute__((aligned(m)))
/// Packs a struct (and other types?) so it won't include padding bytes.
#define PACKED     __attribute__((packed))

#define TRY(expr)   if((res = (expr)) & 0x80000000) return res;

typedef struct TakeoverParameters {
    u64 firmTid;
    u8 kernelVersionMajor;
    u8 kernelVersionMinor;
    bool isN3ds;
    size_t payloadFileOffset;
    char payloadFileName[255+1];
} TakeoverParameters;

/// From libnds
typedef struct NDSHeader {
    char gameTitle[12];
    char gameCode[4];
    char makercode[2];
    u8 unitCode;
    u8 deviceType;
    u8 deviceSize;
    u8 reserved1[9];
    u8 romversion;
    u8 flags;
    u32 arm9romOffset;
    void *arm9executeAddress;
    void *arm9destination;
    u32 arm9binarySize;
    u32 arm7romOffset;
    void *arm7executeAddress;
    void *arm7destination;
    u32 arm7binarySize;
    u32 filenameOffset;
    u32 filenameSize;
    u32 fatOffset;
    u32 fatSize;
    u32 arm9overlaySource;
    u32 arm9overlaySize;
    u32 arm7overlaySource;
    u32 arm7overlaySize;
    u32 cardControl13;
    u32 cardControlBF;
    u32 bannerOffset;
    u16 secureCRC16;
    u16 readTimeout;
    u32 unknownRAM1;
    u32 unknownRAM2;
    u32 bfPrime1;
    u32 bfPrime2;
    u32 romSize;
    u32 headerSize;
    u32 zeros88[14];
    u8 gbaLogo[156];
    u16 logoCRC16;
    u16 headerCRC16;
} NDSHeader;

typedef struct DSiHeader {
    NDSHeader ndshdr;
    u32 debugRomSource;
    u32 debugRomSize;
    u32 debugRomDestination;
    u32 offset_0x16C;
    u8 zero[16];
    u8 global_mbk_setting[5][4];
    u32 arm9_mbk_setting[3];
    u32 arm7_mbk_setting[3];
    u32 mbk9_wramcnt_setting;
    u32 region_flags;
    u32 access_control;
    u32 scfg_ext_mask;
    u8 offset_0x1BC[3];
    u8 appflags;
    void *arm9iromOffset;
    u32 offset_0x1C4;
    void *arm9idestination;
    u32 arm9ibinarySize;
    void *arm7iromOffset;
    u32 offset_0x1D4;
    void *arm7idestination;
    u32 arm7ibinarySize;
    u32 digest_ntr_start;
    u32 digest_ntr_size;
    u32 digest_twl_start;
    u32 digest_twl_size;
    u32 sector_hashtable_start;
    u32 sector_hashtable_size;
    u32 block_hashtable_start;
    u32 block_hashtable_size;
    u32 digest_sector_size;
    u32 digest_block_sectorcount;
    u32 banner_size;
    u32 offset_0x20C;
    u32 total_rom_size;
    u32 offset_0x214;
    u32 offset_0x218;
    u32 offset_0x21C;
    u32 modcrypt1_start;
    u32 modcrypt1_size;
    u32 modcrypt2_start;
    u32 modcrypt2_size;
    u32 tid_low;
    u32 tid_high;
    u32 public_sav_size;
    u32 private_sav_size;
    u8 reserved3[176];
    u8 age_ratings[16];
    u8 hmac_arm9[20];
    u8 hmac_arm7[20];
    u8 hmac_digest_master[20];
    u8 hmac_icon_title[20];
    u8 hmac_arm9i[20];
    u8 hmac_arm7i[20];
    u8 reserved4[40];
    u8 hmac_arm9_no_secure[20];
    u8 reserved5[2636];
    u8 debug_args[384];
    u8 rsa_signature[128];
} DSiHeader;
