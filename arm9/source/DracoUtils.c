// =====================================================================================
// Copyright (c) 2025-2026 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and eyalabraham
// (Dragon 32 emu core) are thanked profusely.
//
// The Draco-DS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <maxmod9.h>

#include "DracoDS.h"
#include "DracoUtils.h"
#include "top_dragon.h"
#include "top_coco.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "pdev_bg0.h"
#include "printf.h"

#include "CRC32.h"
#include "printf.h"

short int   fileCount=0;
short int   ucGameAct=0;
short int   ucGameChoice = -1;
FIDraco     gpFic[MAX_FILES];
char        szName[256];
char        szName2[40];
char        szFile[256];
u32         file_size = 0;
char        strBuf[40];

struct Config_t         AllConfigs[MAX_CONFIGS];
struct Config_t         myConfig __attribute((aligned(4))) __attribute__((section(".dtcm")));
struct GlobalConfig_t   myGlobalConfig;

u16 *pVidFlipBuf  = (u16*) (0x06000000);    // Video flipping buffer

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

u16 JoyState   __attribute__((section(".dtcm"))) = 0;           // Joystick State and Key Bits

u8 option_table=0;
u8 force_vdg_mismatch_lower = 0;

const char szKeyName[MAX_KEY_OPTIONS][16] = {
  "JOYSTICK UP",
  "JOYSTICK DOWN",
  "JOYSTICK LEFT",
  "JOYSTICK RIGHT",
  "JOYSTICK FIRE",

  "KEYBOARD A", //5
  "KEYBOARD B",
  "KEYBOARD C",
  "KEYBOARD D",
  "KEYBOARD E",
  "KEYBOARD F",
  "KEYBOARD G",
  "KEYBOARD H",
  "KEYBOARD I",
  "KEYBOARD J",
  "KEYBOARD K",
  "KEYBOARD L",
  "KEYBOARD M",
  "KEYBOARD N",
  "KEYBOARD O",
  "KEYBOARD P", // 20
  "KEYBOARD Q",
  "KEYBOARD R",
  "KEYBOARD S",
  "KEYBOARD T",
  "KEYBOARD U",
  "KEYBOARD V",
  "KEYBOARD W",
  "KEYBOARD X",
  "KEYBOARD Y",
  "KEYBOARD Z", // 30

  "KEYBOARD 1", // 31
  "KEYBOARD 2",
  "KEYBOARD 3",
  "KEYBOARD 4",
  "KEYBOARD 5",
  "KEYBOARD 6",
  "KEYBOARD 7",
  "KEYBOARD 8",
  "KEYBOARD 9",
  "KEYBOARD 0", // 40

  "KEYBOARD DASH",   // 41
  "KEYBOARD COMMA",
  "KEYBOARD PERIOD",
  "KEYBOARD COLON",
  "KEYBOARD SEMI",   // 45
  "KEYBOARD SLASH",
  "KEYBOARD AT",

  "KEYBOARD ENTER", // 48
  "KEYBOARD SPACE", // 49

  "KEYBOARD UP",    // 50
  "KEYBOARD LEFT",
  "KEYBOARD RIGHT",
  "KEYBOARD DOWN",

  "KEYBOARD CLEAR",
  "KEYBOARD SHIFT", // 55
  "KEYBOARD BREAK",
  "RESERVED",
  "RESERVED",
  "JOYSTICK FIRE 2",// 59

  "ATTACK LEFT",    // 60
  "ATTACK RIGHT",
  "MOVE FORWARD",
  "MOVE BACK",
  "TURN LEFT",
  "TURN RIGHT",     // 65
  "TURN AROUND",
  "PULL LEFT ...",
  "PULL RIGHT ...",
  "EXAMINE",
  "LOOK"            // 70
};

// -------------------------------------------------------------------------------------------------------------------------
// From the MAME SL archive - this is a list of about 1100 known .CAS files for the Dragon32. There is no good reliable way
// to detect a Coco .CAS from a Dragon .CAS file so we use the known CRCs go give us the best guess. User can override.
// -------------------------------------------------------------------------------------------------------------------------
const uint32_t dragon32_known_crcs[] = {
    0x0081feb9,0x00c09c2f,0x00ddeeef,0x01717f0b,0x01bc6c87,0x02237bf7,0x0261546e,0x029e0a74,0x02ff9c41,0x03b34381,0x03d261b4,
    0x03e07941,0x04359c81,0x0454422a,0x047a8cc9,0x04853255,0x04dbad8e,0x050a0a95,0x0558010f,0x05776ec5,0x05d3a07d,0x05e649b0,
    0x061f4e61,0x06687212,0x068e4f1c,0x069c6d5c,0x06ab5c0c,0x06d0651b,0x06d0cf7f,0x07325e17,0x0780025f,0x0829c38a,0x08db2dd3,
    0x08e824e2,0x09260d6c,0x09dd0af3,0x0a31a028,0x0a5ab04a,0x0a6f9bda,0x0a953b59,0x0ac83455,0x0b10deaa,0x0b385feb,0x0bdda329,
    0x0c53fe2c,0x0c7a7f3f,0x0ca9f2f7,0x0cc2d449,0x0cd41f45,0x0d2d3e5a,0x0d59b97d,0x0d6f3db2,0x0dae2693,0x0daecf78,0x0e7a311c,
    0x0ede0e3b,0x0ee0e4d0,0x0ef4f628,0x0f215a8e,0x0f9a548c,0x0fbb53f8,0x0fd5319d,0x1007226e,0x1008e341,0x106b9b85,0x10b2cf2b,
    0x110f1d0b,0x1116b181,0x1259fc25,0x12913cfa,0x12f93613,0x133f4085,0x1364cc37,0x13664c11,0x13e5cf13,0x13fb0252,0x141c01ec,
    0x144d3e5a,0x146a382e,0x147a6e4a,0x14849a57,0x14af3bba,0x1513e0a1,0x156d1e56,0x15bb813b,0x163550dc,0x1649b41f,0x1655218f,
    0x16ad719a,0x16c1d88b,0x16d2b4f8,0x16e5a788,0x16efd516,0x17b4e836,0x187f5b7e,0x188ab362,0x1895688c,0x18a4eef4,0x18a8ec4c,
    0x1934c7dd,0x195ba0f8,0x1980663e,0x19bf0841,0x1a57dfdb,0x1a6cba07,0x1a7ae217,0x1a89891a,0x1a9b451e,0x1aafade8,0x1ac83a3a,
    0x1ae9f1dc,0x1b7f9957,0x1be417e7,0x1bf5ec84,0x1c411cb3,0x1d219b49,0x1d534921,0x1d74339c,0x1d7b11f8,0x1de0ce84,0x1e7dc3b7,
    0x1e8c6f4e,0x1e944014,0x1eab849b,0x1f45bf9b,0x1f49a44e,0x1faa43fc,0x1ff649d7,0x20abaef3,0x20dd1877,0x211fbdfd,0x2138abf1,
    0x21434faf,0x21c0346e,0x21fa22ad,0x223893a5,0x227a13ba,0x22a7a64d,0x23364106,0x23c35d71,0x23c76535,0x23e5fde5,0x2519299b,
    0x255df0d3,0x256d3493,0x25b18d83,0x25c983aa,0x265e2dd9,0x26aa41d4,0x271665e9,0x276268f0,0x27b3fe60,0x27ef3e54,0x281db88a,
    0x2851c224,0x29340136,0x294622ca,0x29f19832,0x2a59ae97,0x2a693750,0x2a901785,0x2adbc35d,0x2b0b00ee,0x2bc5d91c,0x2c9f170a,
    0x2cdd5242,0x2dd723b9,0x2e084eb1,0x2e220c75,0x2e5bcdd3,0x2e93509c,0x2ebc1ea3,0x2edeca0f,0x2efabfa2,0x2f150efb,0x2f54076f,
    0x2f5893c2,0x2fbdfe45,0x301a153f,0x30475884,0x307c2e74,0x308f1f1a,0x309c95ad,0x30bf9349,0x30d6a46c,0x3128d1d8,0x3150ae28,
    0x316e9dbb,0x31dd2fab,0x31ebf63c,0x320cd83b,0x3274e72a,0x327dd85d,0x32acc00f,0x32bcaefa,0x32c75267,0x333b6986,0x3371bfda,
    0x3386d56e,0x33c0600a,0x33cecc34,0x33dfc8cb,0x34611217,0x34b655e1,0x350be703,0x35511c8c,0x35583c39,0x3562f20a,0x35d0d913,
    0x3674729c,0x3675c90b,0x36879442,0x36a0884e,0x3719fa29,0x37db4961,0x38005a90,0x393f6009,0x39661806,0x39887c98,0x39a6432b,
    0x39f13927,0x3a267d9e,0x3a29ef90,0x3a2c40ba,0x3ab8207b,0x3aed670c,0x3b003b88,0x3b554e07,0x3b80fd8a,0x3ba84345,0x3bab0506,
    0x3bbd7cf7,0x3bbed998,0x3c04a28e,0x3c13a3fe,0x3c458118,0x3c643777,0x3cb22e80,0x3cd80458,0x3ce2fa1e,0x3ce5a4b9,0x3d57e8e0,
    0x3dd1fb65,0x3e47a46b,0x3e47aa2f,0x3e5aa392,0x3f29d377,0x3f3f0a1c,0x3f59f8c1,0x3fb7f188,0x3fd40714,0x4054ab8d,0x40623848,
    0x406bf2b0,0x40b58d6a,0x41982aad,0x419bea71,0x41a41356,0x41a938e0,0x421339ad,0x42bc59bb,0x42cac36f,0x42f6c179,0x431a2c9b,
    0x4329d4de,0x433fea2a,0x43587c1f,0x4385b78b,0x4386ce6e,0x43aade07,0x441c69d2,0x444f7a96,0x446509eb,0x4476adf3,0x44a6040a,
    0x44db82ec,0x45069cc9,0x455e8096,0x455f7ef3,0x456b7162,0x45ee8abc,0x45f2eab2,0x45fdbdba,0x46e88279,0x472a5674,0x476b425d,
    0x477c9932,0x47d55c01,0x47fef1f1,0x48036178,0x4862c6ee,0x489682a9,0x48cd73b6,0x491f9109,0x497264c6,0x498f6032,0x49912f68,
    0x4a1bf181,0x4a802247,0x4a858001,0x4ab70fa1,0x4b297bbc,0x4b412e8c,0x4b488269,0x4c1de986,0x4c699232,0x4cae62fc,0x4d296ef8,
    0x4dc667b2,0x4de8a02b,0x4e50068a,0x4e66148c,0x4e801480,0x4e9c30ca,0x4eeda577,0x4f1cdd7a,0x4f33330d,0x4f4e3c9d,0x4fe6e3d1,
    0x5023ba18,0x505bb47b,0x50a01c49,0x50a4adcb,0x50bf451f,0x50e1e14d,0x5178eb0b,0x518136bc,0x51be0f55,0x52073b07,0x520f3b99,
    0x5250ebd7,0x526b42d7,0x5271753b,0x529daa98,0x52eba0c4,0x52f14589,0x5308c311,0x53653368,0x539cdcc7,0x53ac0d6e,0x53fad07a,
    0x5440117d,0x544e4f05,0x545a881e,0x548e0bf5,0x549271e9,0x5495b719,0x54a689b4,0x54e92420,0x5507d406,0x5527964d,0x5576a6c3,
    0x5587438c,0x55b9fd97,0x55e723d6,0x5615775a,0x5641960d,0x56475747,0x56a0708f,0x56aa345a,0x56b64698,0x573bfda2,0x57669af9,
    0x57d25c57,0x586a0e89,0x58866824,0x58b878ba,0x591a5878,0x59406e78,0x59a2bd90,0x59c47811,0x59e32178,0x5a1cf7a2,0x5a8b70f8,
    0x5a926bfb,0x5b5b95c2,0x5b85afa2,0x5bde8f07,0x5c3654ed,0x5ca9f399,0x5cad6cb7,0x5ce86984,0x5d15db43,0x5d221e10,0x5d64f22b,
    0x5d8e93fe,0x5e83bea1,0x5eb2fcb9,0x5ed0197c,0x5f73b828,0x5fa0d11e,0x5fa6144f,0x5fe37d46,0x60057e45,0x600f8588,0x6020f477,
    0x6034e94e,0x60403a48,0x6047c32c,0x60b0d8ab,0x60dfc511,0x6134c7cf,0x61546171,0x61df1f7b,0x620a42d8,0x6294ceb4,0x630309f2,
    0x63033ddb,0x6338e8e1,0x63b9a03c,0x63d8fe33,0x64284d60,0x64377aa0,0x6476f4e3,0x647cba16,0x65adfe0e,0x65d7dd17,0x666b0880,
    0x668cf7eb,0x669c0a0c,0x671f3f59,0x67c1f026,0x67d069f5,0x6836f4dc,0x68b03d6d,0x68c96901,0x6938b186,0x695355c2,0x695afae1,
    0x697b89c9,0x69953abd,0x69ef7492,0x6a4045c1,0x6a6fee69,0x6a7d0b2c,0x6a90657f,0x6afaccf6,0x6b0e4dd4,0x6b3978c9,0x6b54261f,
    0x6b6b4294,0x6b7d8dce,0x6bec4a2d,0x6c4c97a2,0x6cba9cd4,0x6d4ebfe2,0x6d8130e1,0x6d82a09e,0x6dcdd066,0x6de9bf64,0x6df72b84,
    0x6e20c4a9,0x6e994676,0x6eb96872,0x6ecb11a4,0x6ee72f6c,0x6eeb10d8,0x6f77c8b2,0x6f88eba6,0x6f8daee3,0x6f988d47,0x701b657e,
    0x704bf067,0x708e4c17,0x70b76b39,0x70e3b0ef,0x716fdc5e,0x71d41853,0x72033697,0x721d25ad,0x724d9cd9,0x73b6cf5a,0x73d0d9c9,
    0x73fd0b52,0x7405796f,0x740c6aa3,0x74194fa1,0x74bd36ba,0x74c16d53,0x74f68a26,0x74f6ac65,0x750bbf87,0x7537c914,0x753bcb4a,
    0x754091c2,0x75b6156a,0x75c0a307,0x76a14661,0x76acbda2,0x76c28668,0x771e0232,0x774acfd1,0x775b61ec,0x77908fa6,0x7833b79c,
    0x787b5a63,0x78dba786,0x796d2ec2,0x79bb1d7b,0x79d0e911,0x7a322048,0x7a99d898,0x7abb1578,0x7b250a95,0x7b5027f4,0x7b9332f7,
    0x7bdd5276,0x7c343fa2,0x7c42304e,0x7c800cff,0x7cba5f64,0x7ccdd40c,0x7d8676a7,0x7dad8c12,0x7db58b34,0x7e6e1221,0x7eb14466,
    0x7f6289ca,0x7f7ea4c0,0x810e811c,0x8128b517,0x816ee711,0x81766576,0x818523a1,0x82202804,0x83226f7c,0x8343fe00,0x83a89dce,
    0x84135f15,0x84996cd9,0x849c3dd6,0x84cbad5b,0x84d75dc1,0x84e9a29e,0x85435311,0x854bc3bc,0x857d9ff3,0x85993601,0x8602ed9c,
    0x860c7501,0x8639467f,0x866ec4c6,0x8693ba3a,0x86f0df87,0x8758b239,0x878945a2,0x87eceaf1,0x88211fc1,0x88ed4735,0x88fa968f,
    0x89407240,0x897a8f7e,0x89c36e89,0x8a282c48,0x8a2c4686,0x8a30b19c,0x8a36614a,0x8a5c5e66,0x8aa46913,0x8aa94e7b,0x8b2bb9f9,
    0x8b44b64b,0x8b56e0d7,0x8b5d1696,0x8bb075f8,0x8bf5c085,0x8c1efdb4,0x8c8589f7,0x8c882c63,0x8ca48d23,0x8cb84085,0x8cbdcdff,
    0x8ce92305,0x8d2c233c,0x8d5298bc,0x8e23b5b4,0x8e3eb24d,0x8ed8228a,0x8f07be57,0x8f13f8ab,0x8f23ad31,0x8f278a9a,0x8fb2b61a,
    0x907577e4,0x90e0d3bb,0x90e78a88,0x915b2c70,0x919ac354,0x91c2dd88,0x91c3cf84,0x91c43e7c,0x91ed8dbc,0x91f250fe,0x9226ab32,
    0x922b8cbc,0x923a430f,0x92555253,0x92577bc9,0x92631a6f,0x92712a86,0x9277da75,0x92814b3e,0x929a6ce0,0x92f584f1,0x92f96ea5,
    0x93121637,0x9316ae56,0x931e23c4,0x939d3646,0x93c6f6b7,0x9460e933,0x94dcaff5,0x94e04871,0x94eb0724,0x9516da44,0x951f2d09,
    0x95788a72,0x95be250e,0x963d2dfd,0x96c244ec,0x96f0934c,0x97495b42,0x974ff08f,0x976d09b4,0x9830702c,0x98344919,0x987cdd67,
    0x98c319b5,0x990fba91,0x994e1930,0x99694278,0x99996686,0x999df8cc,0x99abff7c,0x99fb09c4,0x9a99bec7,0x9ab6a74d,0x9ada8d16,
    0x9ae7047e,0x9af72c01,0x9bad6f19,0x9c0ea03c,0x9c1d5935,0x9c96f16a,0x9cc6b1e6,0x9d3272d1,0x9deca5c8,0x9e0540d0,0x9e2b3a66,
    0x9e2ccc3c,0x9ea2bd7d,0x9eb144c9,0x9ec418da,0x9ee160b3,0x9efc2dc3,0x9f0c8be2,0x9f1caabc,0x9f42afee,0x9f869b73,0xa039e3ea,
    0xa06a9f34,0xa06cd3e6,0xa0be5877,0xa0d64a22,0xa10fcda8,0xa118ebb9,0xa158a82b,0xa187d112,0xa1c4dccf,0xa1ce7bd8,0xa1ea5f2f,
    0xa2409147,0xa26b98ad,0xa311eff5,0xa3cecb1a,0xa3de5337,0xa3e792d9,0xa402283d,0xa421a02f,0xa4403eee,0xa619deb6,0xa77c7726,
    0xa7ac02bd,0xa806e93e,0xa831acac,0xa8786148,0xa8796800,0xa8c83821,0xa8d0cbe1,0xa8d39adf,0xa8d79555,0xa8f2e9fd,0xa911daf3,
    0xa921bd99,0xa95a026e,0xa9663f35,0xa9f15bf8,0xa9fb69cd,0xaa1251dc,0xab76471b,0xab9517e3,0xaba89899,0xabb6c191,0xabd81742,
    0xabfea4a1,0xac99a073,0xac9aebc1,0xacafd939,0xacc46d40,0xacf475e2,0xad252257,0xad4d61c6,0xad9af402,0xada248b8,0xada71427,
    0xadd33633,0xadf505d1,0xae998e15,0xaeb1c943,0xaebf3796,0xaed4ddc3,0xaed576e1,0xaf31643c,0xaf5a5198,0xaf617500,0xaf66bbdc,
    0xaf6a8423,0xafb9781d,0xafdd2298,0xafec2de1,0xb00f8260,0xb015aaed,0xb0474085,0xb0c2e6d4,0xb17a07f8,0xb186b945,0xb1983f22,
    0xb1dbbd10,0xb2228902,0xb23216cd,0xb26d5b16,0xb2d957c1,0xb2f65ecd,0xb355dd24,0xb398d87c,0xb3b94d63,0xb3bce84b,0xb3eb9dfc,
    0xb44338ed,0xb455334e,0xb47fb5b3,0xb4d01600,0xb5415b5b,0xb5c1558b,0xb5e6cbbb,0xb5e6fdd9,0xb619a0e5,0xb67df3a4,0xb6c49f91,
    0xb72b7e40,0xb740ab41,0xb74c6620,0xb764eb9b,0xb86328b2,0xb868bb6e,0xb869b7a6,0xb8905d27,0xb89510d6,0xb9d8226d,0xb9fde18a,
    0xba61410d,0xba653314,0xba756e78,0xbb015ea0,0xbbc4762d,0xbcdafc42,0xbcea7296,0xbd2a3910,0xbe3595e7,0xbe6951c5,0xbe712e43,
    0xbea80633,0xbeef5ae7,0xbf0e87bf,0xbf271825,0xbf6445eb,0xbf727e5f,0xbff71183,0xc005c740,0xc021ce29,0xc074a6dc,0xc0a2072d,
    0xc10226b9,0xc117ec3c,0xc1287e1d,0xc1503343,0xc1c32512,0xc1e13242,0xc337b47f,0xc35c0da0,0xc36f4237,0xc37bb079,0xc3b2426e,
    0xc4106fec,0xc44a3238,0xc4e24dfa,0xc575f9ab,0xc652a572,0xc65bfe40,0xc7028143,0xc73b32c7,0xc7668680,0xc772e66f,0xc7933365,
    0xc7e4df1f,0xc8088e57,0xc82947fd,0xc863dcf1,0xc86a611b,0xc878bf2b,0xc8853792,0xc8ea799b,0xc8ebc70d,0xc8f25dec,0xc9240326,
    0xc985e04f,0xc98fd306,0xc9b06acd,0xc9cfe469,0xca1f9390,0xca8532a0,0xcab3787d,0xcb485ccb,0xcb86907e,0xcbcb4eee,0xcbf0bb87,
    0xcc4875d5,0xcc9e9602,0xcce7f1b4,0xcceba5f0,0xcd87cfbe,0xcdc4296c,0xcdfdd89a,0xce577e5c,0xce5891ff,0xce7dd409,0xce9f881a,
    0xcec8e00f,0xcf2e51a4,0xcf566ed0,0xcf639113,0xcf7f9f93,0xcfaa2722,0xd078ce20,0xd09961f6,0xd0b476c4,0xd0f7e01f,0xd14e3e0b,
    0xd1e519e1,0xd1f202db,0xd242f856,0xd252721f,0xd26564e4,0xd2f563e3,0xd32875d3,0xd38f12ed,0xd3ac765b,0xd3ca564a,0xd3dc50bf,
    0xd44956e7,0xd45d19a7,0xd4853b33,0xd48d83be,0xd556fdce,0xd58a5800,0xd5dfb42c,0xd603745a,0xd6212114,0xd63a8d5c,0xd671aeef,
    0xd68bf9fa,0xd6b575f9,0xd6ed3c33,0xd6f084cc,0xd70290cc,0xd738be8c,0xd76c209c,0xd7a90f73,0xd7acdfa2,0xd80bd8ec,0xd8842c3d,
    0xd9c3a78f,0xd9ef6284,0xda91b17c,0xdb13f0ad,0xdb4a2374,0xdb4e20ac,0xdb61965f,0xdb934c9f,0xdbc298c7,0xdc02f783,0xdc06f129,
    0xdc785553,0xdc861d7a,0xdca028b3,0xdd632bf8,0xdd8b5b1c,0xde6f4b00,0xdf109d32,0xdf3b737b,0xdf542033,0xdf54ee7c,0xdf66a81e,
    0xdf8c38f2,0xdfb57fa1,0xdfc0be55,0xdfefa4a0,0xe02edcf9,0xe096d887,0xe0acad34,0xe0b14a18,0xe1515a01,0xe1712bd0,0xe1794d18,
    0xe19e8f07,0xe1be80f8,0xe2237da3,0xe2692a32,0xe2de8482,0xe30a3cb4,0xe34e90af,0xe3519260,0xe35e6363,0xe3939579,0xe3f9c70e,
    0xe3fad763,0xe474b3c9,0xe4a3b8d5,0xe5450c9a,0xe54662fc,0xe5aa682c,0xe6361104,0xe64da965,0xe64ee3cc,0xe667eef9,0xe6f7b5a6,
    0xe7611fc1,0xe82df965,0xe87d45b7,0xe8c5e919,0xe8ed1552,0xe8f188b2,0xe8fdb8c6,0xe9126f68,0xe931302b,0xe97f38bb,0xe9b07152,
    0xe9c8ecf2,0xe9ce76bf,0xea106151,0xea10f961,0xea700e70,0xead293a6,0xebae64f1,0xebccf698,0xebe77e4f,0xec037606,0xec7f27e5,
    0xed733b37,0xee12fa0d,0xee181e9c,0xee1e48e5,0xee675c45,0xeea237a7,0xef24141f,0xef4eb038,0xef7e8d9f,0xf011b070,0xf02148bf,
    0xf0f6ed0d,0xf1171d8e,0xf14f7494,0xf16e79a4,0xf170eb1e,0xf1a400b1,0xf1ff7642,0xf249c665,0xf2b51caf,0xf2b7ac0d,0xf30555eb,
    0xf30671be,0xf40902cd,0xf441c8b4,0xf45a0a99,0xf4675d85,0xf500d3b7,0xf522be41,0xf58b4c64,0xf5f76884,0xf65286be,0xf6687676,
    0xf6705663,0xf6ee85b4,0xf7130f57,0xf7d19314,0xf7ef4cd9,0xf829678c,0xf83979c3,0xf84b4aa2,0xf884b189,0xf88db90a,0xf8cb092d,
    0xf8cd442f,0xf8dcb437,0xf8e637fd,0xf8f47d34,0xf90d27be,0xf918d8e2,0xf9338430,0xf97952d3,0xf97be2a9,0xf9a39a56,0xf9a650b0,
    0xf9afdff2,0xf9e3d6c9,0xfa0bbe42,0xfa144b83,0xfa286471,0xfaf9bed6,0xfafeddd4,0xfb6bd2ff,0xfb7631e4,0xfbf78072,0xfc263799,
    0xfc2a4202,0xfc7df4f5,0xfd14bf6c,0xfdecd00f,0xfdf828af,0xfe5ce154,0xfec20a37,0xfedbc7c4,0xfedcec3c,0xfeec4ca5,0xff02bb66,
    0xff24ecb9,0xff306351,0xff31136a,0xff63aa06,0xff7155d6
};


/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2)
{
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;

  BottomScreenOptions();

  DSPrint(16-strlen(szCh1)/2,10,6,szCh1);
  DSPrint(16-strlen(szCh2)/2,12,6,szCh2);
  DSPrint(8,14,6,("> YES <"));
  DSPrint(20,14,6,("  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL)
  {
    WAITVBL;
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
          ucGauS = 1;
          if (ucCho == ID_SHM_YES) {
            uRet = ucCho;
          }
          else {
            ucCho  = ID_SHM_YES;
          }
        }
      }
      else
        ucGauS = 0;
      if ( (iTx>20*8) && (iTx<20*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucDroS) {
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
          ucDroS = 1;
          if (ucCho == ID_SHM_NO) {
            uRet = ucCho;
          }
          else {
            ucCho = ID_SHM_NO;
          }
        }
      }
      else
        ucDroS = 0;
    }
    else {
      ucDroS = 0;
      ucGauS = 0;
    }

    if (keysCurrent() & KEY_LEFT){
      if (!ucGau) {
        ucGau = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucGau = 0;
    }
    if (keysCurrent() & KEY_RIGHT) {
      if (!ucDro) {
        ucDro = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho  = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucDro = 0;
    }
    if (keysCurrent() & KEY_A) {
      uRet = ucCho;
    }
  }
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  BottomScreenKeyboard();

  return uRet;
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
void dsDisplayFiles(u16 NoDebGame, u8 ucSel)
{
  u16 ucBcl,ucGame;
  u8 maxLen;

  DSPrint(31,6,0,(NoDebGame>0 ? "<" : " "));
  DSPrint(31,22,0,(NoDebGame+14<fileCount ? ">" : " "));

  for (ucBcl=0;ucBcl<17; ucBcl++)
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < fileCount)
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>30) szName[30]='\0';
      if (gpFic[ucGame].uType == DIRECTORY)
      {
        szName[28] = 0; // Needs to be 2 chars shorter with brackets
        sprintf(szName2, "[%s]",szName);
        sprintf(szName,"%-30s",szName2);
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else
      {
        sprintf(szName,"%-30s",strupr(szName));
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                              ");
    }
  }
}


// -------------------------------------------------------------------------
// Standard qsort routine for the games - we sort all directory
// listings first and then a case-insenstive sort of all games.
// -------------------------------------------------------------------------
int Filescmp (const void *c1, const void *c2)
{
  FIDraco *p1 = (FIDraco *) c1;
  FIDraco *p2 = (FIDraco *) c2;

  if (p1->szName[0] == '.' && p2->szName[0] != '.')
      return -1;
  if (p2->szName[0] == '.' && p1->szName[0] != '.')
      return 1;
  if ((p1->uType == DIRECTORY) && !(p2->uType == DIRECTORY))
      return -1;
  if ((p2->uType == DIRECTORY) && !(p1->uType == DIRECTORY))
      return 1;
  return strcasecmp (p1->szName, p2->szName);
}

/*********************************************************************************
 * Find game/program files available - sort them for display.
 ********************************************************************************/
void DracoDSFindFiles(u8 bDiskOnly)
{
  u32 uNbFile;
  DIR *dir;
  struct dirent *pent;

  uNbFile=0;
  fileCount=0;

  dir = opendir(".");
  while (((pent=readdir(dir))!=NULL) && (uNbFile<MAX_FILES))
  {
    strcpy(szFile,pent->d_name);

    if(pent->d_type == DT_DIR)
    {
      if (!((szFile[0] == '.') && (strlen(szFile) == 1)))
      {
        // Do not include the [sav] and [pok] directories
        if ((strcasecmp(szFile, "sav") != 0) && (strcasecmp(szFile, "pok") != 0))
        {
            strcpy(gpFic[uNbFile].szName,szFile);
            gpFic[uNbFile].uType = DIRECTORY;
            uNbFile++;
            fileCount++;
        }
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_FILENAME_LEN-4)) && (szFile[0] != '.') && (szFile[0] != '_'))  // For MAC don't allow files starting with an underscore
      {
        if ( (strcasecmp(strrchr(szFile, '.'), ".ccc") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = DRACO_FILE;
          uNbFile++;
          fileCount++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".rom") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = DRACO_FILE;
          uNbFile++;
          fileCount++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".cas") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = DRACO_FILE;
          uNbFile++;
          fileCount++;
        }

        if (bDISKBIOS_found)
        {
            if ( (strcasecmp(strrchr(szFile, '.'), ".dsk") == 0) )  {
              strcpy(gpFic[uNbFile].szName,szFile);
              gpFic[uNbFile].uType = DRACO_FILE;
              uNbFile++;
              fileCount++;
            }
        }
      }
    }
  }
  closedir(dir);

  // ----------------------------------------------
  // If we found any files, go sort the list...
  // ----------------------------------------------
  if (fileCount)
  {
    qsort (gpFic, fileCount, sizeof(FIDraco), Filescmp);
  }
}

// ----------------------------------------------------------------
// Let the user select a new game (rom) file and load it up!
// ----------------------------------------------------------------
u8 DracoDSLoadFile(u8 bDiskOnly)
{
  bool bDone=false;
  u16 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00, romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s16 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);

  BottomScreenOptions();

  DracoDSFindFiles(bDiskOnly);

  ucGameChoice = -1;

  nbRomPerPage = (fileCount>=17 ? 17 : fileCount);
  uNbRSPage = (fileCount>=5 ? 5 : fileCount);

  if (ucGameAct>fileCount-nbRomPerPage)
  {
    firstRomDisplay=fileCount-nbRomPerPage;
    romSelected=ucGameAct-fileCount+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucGameAct;
    romSelected=0;
  }

  if (romSelected >= fileCount) romSelected = 0; // Just start at the top

  dsDisplayFiles(firstRomDisplay,romSelected);

  // -----------------------------------------------------
  // Until the user selects a file or exits the menu...
  // -----------------------------------------------------
  while (!bDone)
  {
    if (keysCurrent() & KEY_UP)
    {
      if (!ucHaut)
      {
        ucGameAct = (ucGameAct>0 ? ucGameAct-1 : fileCount-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=fileCount-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {

        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else
    {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        ucGameAct = (ucGameAct< fileCount-1 ? ucGameAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<fileCount-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_RIGHT)
    {
      if (!ucSBas)
      {
        ucGameAct = (ucGameAct< fileCount-nbRomPerPage ? ucGameAct+nbRomPerPage : fileCount-nbRomPerPage);
        if (firstRomDisplay<fileCount-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = fileCount-nbRomPerPage; }
        if (ucGameAct == fileCount-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_LEFT)
    {
      if (!ucSHaut)
      {
        ucGameAct = (ucGameAct> nbRomPerPage ? ucGameAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucGameAct == 0) romSelected = 0;
        if (romSelected > ucGameAct) romSelected = ucGameAct;
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSHaut = 0;
    }

    // -------------------------------------------------------------------------
    // They B key will exit out of the ROM selection without picking a new game
    // -------------------------------------------------------------------------
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }

    // -------------------------------------------------------------------
    // Any of these keys will pick the current ROM and try to load it...
    // -------------------------------------------------------------------
    if (keysCurrent() & KEY_A || keysCurrent() & KEY_Y || keysCurrent() & KEY_X)
    {
      if (gpFic[ucGameAct].uType != DIRECTORY)
      {
          u8 isDisk = strcasecmp(strrchr(gpFic[ucGameAct].szName, '.'), ".dsk");
          u8 isCass = strcasecmp(strrchr(gpFic[ucGameAct].szName, '.'), ".cas");
          if (!bDiskOnly || (isDisk == 0) || (isCass == 0))
          {
              bDone=true;
              ucGameChoice = ucGameAct;
              WAITVBL;
          }
      }
      else
      {
        chdir(gpFic[ucGameAct].szName);
        DracoDSFindFiles(bDiskOnly);
        ucGameAct = 0;
        nbRomPerPage = (fileCount>=17 ? 17 : fileCount);
        uNbRSPage = (fileCount>=5 ? 5 : fileCount);
        if (ucGameAct>fileCount-nbRomPerPage) {
          firstRomDisplay=fileCount-nbRomPerPage;
          romSelected=ucGameAct-fileCount+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucGameAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }

    // --------------------------------------------
    // If the filename is too long... scroll it.
    // --------------------------------------------
    if (strlen(gpFic[ucGameAct].szName) > 30)
    {
      ucFlip++;
      if (ucFlip >= 25)
      {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+30)>strlen(gpFic[ucGameAct].szName))
        {
          ucFlop++;
          if (ucFlop >= 15)
          {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,gpFic[ucGameAct].szName+uLenFic,30);
        szName[30] = '\0';
        DSPrint(1,6+romSelected,2,szName);
      }
    }
    swiWaitForVBlank();
  }

  // Wait for some key to be pressed before returning
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);

  return 0x01;
}


// ---------------------------------------------------------------------------
// Write out the DracoDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings ~1000 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;

    if (bShow) DSPrint(1,23,0, (char*)"     SAVING CONFIGURATION     ");

    // Set the global configuration version number...
    myGlobalConfig.config_ver = CONFIG_VERSION;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;

    // Find the slot we should save into...
    for (slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == myConfig.game_crc)  // Got a match?!
        {
            break;
        }
        if (AllConfigs[slot].game_crc == 0x00000000)  // Didn't find it... use a blank slot...
        {
            break;
        }
    }

    // --------------------------------------------------------------------------
    // Copy our current game configuration to the main configuration database...
    // --------------------------------------------------------------------------
    if (myConfig.game_crc != 0x00000000)
    {
        memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));
    }

    // Grab the directory we are currently in so we can restore it
    getcwd(myGlobalConfig.szLastPath, MAX_FILENAME_LEN);

    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // directory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }
    fp = fopen("/data/DracoDS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp); // Write the global config
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);         // Write the array of all configurations
        fclose(fp);
    }
    else
    DSPrint(1,23,0, (char*)"   ERROR SAVING CONFIG FILE!   ");

    if (bShow)
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(1,23, 0, (char *)" B=EXIT, X=GLOBAL, START=SAVE  ");
    }
}


// Return values for file type detection
typedef enum {
    TAPE_ERROR_NO_HEADER = -1,
    TAPE_ERROR_BAD_CHECKSUM = -2,
    TAPE_TYPE_BASIC = 0,      // Requires CLOAD
    TAPE_TYPE_DATA = 1,
    TAPE_TYPE_MACHINE = 2     // Requires CLOADM
} CoCoTapeType;

/**
 * Detects the type of a CoCo/Dragon 32 tape file from a .CAS memory buffer.
 * @param buffer Pointer to the raw .CAS data array.
 * @param size Size of the data array in bytes.
 * @return CoCoTapeType enum value indicating the file format or error.
 */
int detect_cas_file_type(const uint8_t *buffer, size_t size)
{
    // A valid header block needs at least: Sync(1), Type(1), Length(1), Payload(15), Checksum(1), Sync(1)
    if (size < 21) {
        return TAPE_ERROR_NO_HEADER;
    }

    size_t i = 0;
    bool header_found = false;

    // Scan the array for the Standard CoCo Leader/Sync marker ($55 followed by $3C)
    // and ensure the next byte is the Header Block ID ($00)
    while (i < size - 20) {
        if (buffer[i] == 0x55 && buffer[i+1] == 0x3C && buffer[i+2] == 0x00) {
            header_found = true;
            i += 2; // Move pointer to the Block Type byte ($00)
            break;
        }
        i++;
    }

    if (!header_found) {
        return TAPE_ERROR_NO_HEADER;
    }

    // At this point:
    // buffer[i]   -> Block Type ($00 for Header)
    // buffer[i+1] -> Block Length (Must be $0F / 15 bytes for a Namefile header)
    //uint8_t block_type = buffer[i];
    uint8_t block_len  = buffer[i+1];

    if (block_len != 0x0F) {
        return TAPE_ERROR_NO_HEADER;
    }

    // Verify block checksum to ensure data integrity.
    // CoCo checksums add all bytes in the block (Type + Length + Payload) discarding overflows.
    uint8_t calculated_checksum = 0;
    for (size_t k = 0; k < (size_t)(2 + block_len); k++) {
        calculated_checksum += buffer[i + k];
    }

    uint8_t stored_checksum = buffer[i + 2 + block_len];
    if (calculated_checksum != stored_checksum) {
        return TAPE_ERROR_BAD_CHECKSUM;
    }

    // The 15-byte payload starts at buffer[i+2]
    // Byte 8 of the payload is the File Type:
    // 0 = BASIC, 1 = Data, 2 = Machine Code
    uint8_t file_type_byte = buffer[i + 2 + 8];

    return (int)file_type_byte;
}


void MapPlayer1(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped Joystick Fire

    myConfig.keymap[5]   = 0;    // NDS B Button mapped Joystick UP
    myConfig.keymap[6]   = 49;   // NDS X Button mapped to SPACE
    myConfig.keymap[7]   = 48;   // NDS Y Button mapped to RETURN
    myConfig.keymap[8]   = 5;    // NDS R Button mapped to 'A'
    myConfig.keymap[9]   = 55;   // NDS L Button mapped to SHIFT
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}


void MapQAOP(void)
{
    myConfig.keymap[0]   = 21;   // Q
    myConfig.keymap[1]   =  5;   // A
    myConfig.keymap[2]   = 19;   // O
    myConfig.keymap[3]   = 20;   // P
    myConfig.keymap[4]   = 49;   // NDS A Button mapped to Space
    myConfig.keymap[5]   = 43;   // NDS B Button mapped to Period
    myConfig.keymap[6]   = 30;   // NDS X Button mapped to Z
    myConfig.keymap[7]   = 28;   // NDS Y Button mapped to X
    myConfig.keymap[8]   = 5;    // NDS R Button mapped to 'A'
    myConfig.keymap[9]   = 6;    // NDS L Button mapped to 'B'
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}


void Cursors(void)
{
    myConfig.keymap[0]   = 50;   // UP
    myConfig.keymap[1]   = 53;   // DOWN
    myConfig.keymap[2]   = 51;   // LEFT
    myConfig.keymap[3]   = 52;   // RIGHT
    myConfig.keymap[4]   = 48;   // NDS A Button mapped Return
    myConfig.keymap[5]   = 49;   // NDS B Button mapped Space
    myConfig.keymap[6]   = 43;   // NDS X Button mapped Period
    myConfig.keymap[7]   = 54;   // NDS Y Button mapped Clear
    myConfig.keymap[8]   = 5;    // NDS R Button mapped to 'A'
    myConfig.keymap[9]   = 6;    // NDS L Button mapped to 'B'
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}


void SetDefaultGlobalConfig(void)
{
    // A few global defaults...
    memset(&myGlobalConfig, 0x00, sizeof(myGlobalConfig));
    myGlobalConfig.showFPS        = 0;    // Don't show FPS counter by default
    myGlobalConfig.lastDir        = 0;    // Default is to start in /roms/dragon
    myGlobalConfig.debugger       = 0;    // Debugger is not shown by default
    myGlobalConfig.defMachine     = 1;    // Set to Tandy by default (0=Dragon)
    myGlobalConfig.defDiskSave    = 1;    // Default is to auto-save disk files
}

void SetDefaultGameConfig(void)
{
    myConfig.game_crc    = 0;    // No game in this slot yet

    MapPlayer1();                // Default to Player 1 mapping

    myConfig.machine        = myGlobalConfig.defMachine;   // Default is Tandy Coco (0=Dragon)
    myConfig.joystick       = 0;                           // Right Joystick by default
    myConfig.joyType        = 0;                           // Joystick is Digital
    myConfig.autoFire       = 0;                           // Default to no auto-fire on either button
    myConfig.dpad           = DPAD_NORMAL;                 // Normal DPAD use - mapped to joystick
    myConfig.loadType       = 1;                           // Default is to to auto-load games as CLOADM
    myConfig.gameSpeed      = 0;                           // Default is 100% game speed
    myConfig.forceCSS       = 0;                           // Normal - not forced Color Select
    myConfig.diskSave       = myGlobalConfig.defDiskSave;  // Default is to auto-save disk files
    myConfig.analogCenter   = 1;                           // Default is center=32
    myConfig.artifacts      = 0;                           // Default is BLUE/ORANGE
    myConfig.soundVolume    = 0;                           // Default is normal sound
    myConfig.sensitivityX   = 0;                           // Normal Analog X Sensitivity
    myConfig.sensitivityY   = 0;                           // Normal Analog Y Sensitivity
    myConfig.clickFilter    = 1;                           // Sound click filter (for games like Androne but not for Demon Attack)
    myConfig.reserved1      = 0;
    myConfig.reserved2      = 0;

    // We only support TANDY in disk mode
    if ((draco_mode == MODE_DSK) || (draco_mode == MODE_CART))
    {
        myConfig.machine = 1; // CoCo only
    }

    // If CAS, we do our best to match the known Dragon32 games...
    if (draco_mode == MODE_CAS)
    {
        for (int i=0; i < (sizeof(dragon32_known_crcs)/sizeof(uint32_t)); i++)
        {
            if (file_crc == dragon32_known_crcs[i])
            {
                myConfig.machine = 0;
                break;
            }
        }
    }

    if (myConfig.machine == 0)
    {
        myConfig.artifacts = 2; // Default to Black/White if the machine is PAL Dragon32
    }

    // Now some special overrides for known games that need it - all these games use Left Joystick
    if ((file_crc == 0x6f1e913a) || (file_crc == 0x3ee6ed00) || (file_crc == 0xEF6D0774) || (file_crc == 0x2ADF1579) ||
        (file_crc == 0xA50F46B1) || (file_crc == 0xE14BF7F6) || (file_crc == 0xF4F2B0A0) || (file_crc == 0x7D1CAC0E) ||
        (file_crc == 0x8CD56308) || (file_crc == 0xAC4DE2DA) || (file_crc == 0xEDA97D6F) || (file_crc == 0x044605D2) ||
        (file_crc == 0xF4700120) || (file_crc == 0x0AD7855B) || (file_crc == 0x65EDB45C) || (file_crc == 0x984EE0D9) ||
        (file_crc == 0xAB96914A) || (file_crc == 0xACC9F6AD) || (file_crc == 0x644CEF62) || (file_crc == 0x7FBD7156) ||
        (file_crc == 0xD2C69D4A) || (file_crc == 0xA8BD5404) || (file_crc == 0x8A36614A) || (file_crc == 0xF14F7494) ||
        (file_crc == 0x2E93509C) || (file_crc == 0x849C3DD6) || (file_crc == 0x9316AE56) || (file_crc == 0xFA0BBE42) ||
        (file_crc == 0xB740AB41) || (file_crc == 0xDD632BF8) || (file_crc == 0xF659A608) || (file_crc == 0x195BA0F8) ||
        (file_crc == 0x7A6D6D5F) || (file_crc == 0x7F1C75F7) || (file_crc == 0x261831C5) || (file_crc == 0x7A6D6D5F) ||
        (file_crc == 0x54094DEA))
    {
        myConfig.joystick = 1;   // Uses Left Joystick
    }

    if ((file_crc == 0xd45e59e3) || (file_crc == 0xc985282a) || strstr(initial_file, "DAGGORATH"))  // Dungeons of Daggorath
    {
        myConfig.keymap[0]   = 62;   // NDS D-Pad mapped to MOVE (FORWARD)
        myConfig.keymap[1]   = 66;   // NDS D-Pad mapped to TURN AROUND
        myConfig.keymap[2]   = 64;   // NDS D-Pad mapped to TURN LEFT
        myConfig.keymap[3]   = 65;   // NDS D-Pad mapped to TURN RIGHT
        myConfig.keymap[4]   = 61;   // NDS A Button mapped ATTACK RIGHT

        myConfig.keymap[5]   = 63;   // NDS B Button mapped to MOVE BACK
        myConfig.keymap[6]   = 49;   // NDS X Button mapped to SPACE
        myConfig.keymap[7]   = 48;   // NDS Y Button mapped to RETURN
        myConfig.keymap[8]   = 68;   // NDS R Button mapped to PULL RIGHT ...
        myConfig.keymap[9]   = 67;   // NDS L Button mapped to PULL LEFT ...
        myConfig.keymap[10]  = 69;   // NDS START mapped to EXAMINE
        myConfig.keymap[11]  = 70;   // NDS SELECT mapped to LOOK
    }

    // This just makes it easier to search the filename if we are all caps...
    for (int i=0; i<strlen(initial_file); i++)
    {
        initial_file[i] = toupper(initial_file[i]);
    }

    // These games generally want the 'Digital plus Offset' handling (offset type 1)
    if (strstr(initial_file, "BANDITO"))
    {
        myConfig.joyType = 3;
    }

    // These games generally want the 'Digital plus Offset' handling (offset type 2)
    if (strstr(initial_file, "NERBLE") || strstr(initial_file, "AVENGER") || strstr(initial_file, "SHOCK") || strstr(initial_file, "FANGMAN") || strstr(initial_file, "STARBLAZE"))
    {
        myConfig.joyType = 4;
    }

    if (strstr(initial_file, "CHUCKIE"))
    {
        myConfig.dpad = DPAD_SLIDE_N_GLIDE;
        Cursors();
    }

    if ((strstr(initial_file, "DRAGONFIRE")) || (strstr(initial_file, "DRAGON FIRE")))
    {
        myConfig.forceCSS = 1;
    }

    force_vdg_mismatch_lower = 0;
    if (strstr(initial_file, "GALACTICAT") || strstr(initial_file, "GALACTIC AT"))
    {
        force_vdg_mismatch_lower = 1;
    }

    if ((strstr(initial_file, "DEMON ATTACK")) || (strstr(initial_file, "DEMONATTACK")))
    {
        myConfig.clickFilter = 0;   // Demon Attack does magic with the enable/disable of speaker to produce sounds and we can't filter it
    }
        
    if (strstr(initial_file, "BUZZARD"))
    {
        myConfig.joystick = 1; // Uses Left Joystick
    }

    if (strstr(initial_file, "ANDRONE"))  // Analog Center
    {
        myConfig.joyType = 2;
    }

    if ((strstr(initial_file, "SPACE ASSAULT")) || (strstr(initial_file, "SPACEASSAULT")))  // Analog Slow
    {
        myConfig.joyType = 1;
    }

    if (strstr(initial_file, "POLARIS"))
    {
        myConfig.joyType = 1;
        myConfig.keymap[7]   = 30;   // NDS Y Button mapped to Z
        myConfig.keymap[5]   = 28;   // NDS B Button mapped to X
        myConfig.keymap[4]   = 7;    // NDS A Button mapped to C
    }

    if (detect_cas_file_type(TapeCartDiskBuffer, file_size) == TAPE_TYPE_BASIC)
    {
        myConfig.loadType    = 2;    // Looks like a BASIC loader... Use CLOAD vs CLOADM
    }
}

// ----------------------------------------------------------
// Load configuration into memory where we can use it.
// The configuration is stored in DracoDS.DAT
// ----------------------------------------------------------
void LoadConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    if (ReadFileCarefully("/data/DracoDS.DAT", (u8*)&myGlobalConfig, sizeof(myGlobalConfig), 0))  // Read Global Config
    {
        ReadFileCarefully("/data/DracoDS.DAT", (u8*)&AllConfigs, sizeof(AllConfigs), sizeof(myGlobalConfig)); // Read the full game array of configs

        if (myGlobalConfig.config_ver != CONFIG_VERSION)
        {
            memset(&AllConfigs, 0x00, sizeof(AllConfigs));
            SetDefaultGameConfig();
            SetDefaultGlobalConfig();
            SaveConfig(FALSE);
        }
    }
    else    // Not found... init the entire database...
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SetDefaultGlobalConfig();
        SaveConfig(FALSE);
    }
}

// -------------------------------------------------------------------------
// Try to match our loaded game to a configuration my matching CRCs
// -------------------------------------------------------------------------
u8 clear_firq_immediate = 0;
void FindConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    for (u16 slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
        {
            memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
            break;
        }
    }

    clear_firq_immediate = 0;
    if (strstr(initial_file, "SHAMUS"))
    {
        clear_firq_immediate = 1;
    }
}


// ------------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off
// their option choices for the currently running game into the DracoDS.DAT
// configuration database. When games are loaded back up, DracodS.DAT is read
// to see if we have a match and the user settings can be restored for the game.
// ------------------------------------------------------------------------------
struct options_t
{
    const char  *label;
    const char  *option[12];
    u8          *option_val;
    u8           option_max;
};

const struct options_t Option_Table[2][20] =
{
    // Game Specific Configuration
    {
        {"MACHINE TYPE",   {"DRAGON 32", "TANDY COCO"},                                &myConfig.machine,           2},
        {"CASS LOAD",      {"MANUAL", "CLOADM [EXEC]", "CLOAD [RUN]"},                 &myConfig.loadType,          3},
        {"AUTO FIRE",      {"OFF", "ON"},                                              &myConfig.autoFire,          2},
        {"GAME SPEED",     {"100%", "110%", "120%", "130%", "90%", "80%"},             &myConfig.gameSpeed,         6},
        {"DISK WRITE",     {"OFF", "ON"},                                              &myConfig.diskSave,          2},
        {"FORCE CSS",      {"NORMAL", "COLOR SET 0", "COLOR SET 1"},                   &myConfig.forceCSS,          3},
        {"ARTIFACTS",      {"BLUE/ORANGE", "ORANGE/BLUE", "OFF (BW)"},                 &myConfig.artifacts,         3},
        {"NDS D-PAD",      {"NORMAL", "SLIDE-N-GLIDE", "DIAGONALS"},                   &myConfig.dpad,              3},
        {"JOYSTICK",       {"RIGHT", "LEFT"},                                          &myConfig.joystick,          2},
        {"JOY TYPE",       {"DIGITAL", "ANALOG", "ANALOG CENTER", 
                            "DIG OFFSET 1", "DIG OFFSET 2"},                           &myConfig.joyType,           5},
        {"ANALG SENS X",   {"LOW", "MEDIUM", "HIGH"},                                  &myConfig.sensitivityX,      3},
        {"ANALG SENS Y",   {"LOW", "MEDIUM", "HIGH"},                                  &myConfig.sensitivityY,      3},
        {"ANALG CENTER",   {"31", "32", "33"},                                         &myConfig.analogCenter,      3},
        {"CLICK FILTER",   {"DISABLED", "ENABLED"},                                    &myConfig.clickFilter,       2},
        {"SOUND VOLUME",   {"NORMAL", "LOUDER"},                                       &myConfig.soundVolume,       2},
        
        {NULL,             {"",      ""},                                              NULL,                        1},
    },
    // Global Options
    {
        {"MACHINE TYPE",   {"DRAGON 32", "TANDY COCO"},                                &myGlobalConfig.defMachine,  2},
        {"DISK WRITE",     {"OFF", "ON"},                                              &myGlobalConfig.defDiskSave, 2},
        {"START DIR",      {"/ROMS/DRAGON",  "/ROMS/COCO", "LAST USED DIR"},           &myGlobalConfig.lastDir,     3},
        {"FPS",            {"OFF", "ON", "ON FULLSPEED"},                              &myGlobalConfig.showFPS,     3},
        {"DEBUGGER",       {"OFF", "ON"},                                              &myGlobalConfig.debugger,    2},
        {NULL,             {"",      ""},                                              NULL,                        1},
    }
};


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    s16 len=0;

    DSPrint(1,21, 0, (char *)"                              ");
    DSPrint(1,22, 0, (char *)"                              ");
    
    if (bFullDisplay)
    {
        while (true)
        {
            sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            DSPrint(1,6+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<15; i++)
        {
            DSPrint(1,6+i, 0, (char *)"                               ");
        }
    }

    DSPrint(1,23, 0, (char *)" B=EXIT, X=GLOBAL, START=SAVE  ");
    return len;
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void DracoDSGameOptions(bool bIsGlobal)
{
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;

    option_table = (bIsGlobal ? 1:0);

    idx=display_options_list(true);
    optionHighlighted = 0;
    while (keysCurrent() != 0)
    {
        WAITVBL;
    }

    u8 last_machine_type = myConfig.machine;
    while (!bDone)
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            u8 optionChanged = 0;

            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
                optionChanged = 1;
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[option_table][optionHighlighted].option_val)) == 0)
                    *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max -1;
                else
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) - 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
                optionChanged = 1;
            }

            if (optionChanged)
            {
                if (myConfig.machine != last_machine_type) // Machine type changed
                {
                    last_machine_type = myConfig.machine;
                    if (myConfig.machine == 0)
                    {
                        myConfig.artifacts = 2; // Force Black/White - user can override
                    }
                    else
                    {
                        myConfig.artifacts = 0; // Force Orange/Blue - user can override
                    }
                    idx=display_options_list(true);
                }
            }

            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if (keysCurrent() & (KEY_X)) // Toggle Table
            {
                option_table ^= 1;
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                option_table = 0;   // Reset for next time
                break;
            }
        }
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }

    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
char szCha[34];
void DisplayKeymapName(u32 uY)
{
  sprintf(szCha," PAD UP    : %-17s",szKeyName[myConfig.keymap[0]]);
  DSPrint(1, 6,(uY== 6 ? 2 : 0),szCha);
  sprintf(szCha," PAD DOWN  : %-17s",szKeyName[myConfig.keymap[1]]);
  DSPrint(1, 7,(uY== 7 ? 2 : 0),szCha);
  sprintf(szCha," PAD LEFT  : %-17s",szKeyName[myConfig.keymap[2]]);
  DSPrint(1, 8,(uY== 8 ? 2 : 0),szCha);
  sprintf(szCha," PAD RIGHT : %-17s",szKeyName[myConfig.keymap[3]]);
  DSPrint(1, 9,(uY== 9 ? 2 : 0),szCha);
  sprintf(szCha," KEY A     : %-17s",szKeyName[myConfig.keymap[4]]);
  DSPrint(1,10,(uY== 10 ? 2 : 0),szCha);
  sprintf(szCha," KEY B     : %-17s",szKeyName[myConfig.keymap[5]]);
  DSPrint(1,11,(uY== 11 ? 2 : 0),szCha);
  sprintf(szCha," KEY X     : %-17s",szKeyName[myConfig.keymap[6]]);
  DSPrint(1,12,(uY== 12 ? 2 : 0),szCha);
  sprintf(szCha," KEY Y     : %-17s",szKeyName[myConfig.keymap[7]]);
  DSPrint(1,13,(uY== 13 ? 2 : 0),szCha);
  sprintf(szCha," KEY R     : %-17s",szKeyName[myConfig.keymap[8]]);
  DSPrint(1,14,(uY== 14 ? 2 : 0),szCha);
  sprintf(szCha," KEY L     : %-17s",szKeyName[myConfig.keymap[9]]);
  DSPrint(1,15,(uY== 15 ? 2 : 0),szCha);
  sprintf(szCha," START     : %-17s",szKeyName[myConfig.keymap[10]]);
  DSPrint(1,16,(uY== 16 ? 2 : 0),szCha);
  sprintf(szCha," SELECT    : %-17s",szKeyName[myConfig.keymap[11]]);
  DSPrint(1,17,(uY== 17 ? 2 : 0),szCha);
}

u8 keyMapType = 0;
void SwapKeymap(void)
{
    keyMapType = (keyMapType+1) % 3;
    switch (keyMapType)
    {
        case 0: MapPlayer1();  DSPrint(12,23,0,("JOYSTICK")); break;
        case 1: Cursors();     DSPrint(12,23,0,("CURSORS ")); break;
        case 2: MapQAOP();     DSPrint(12,23,0,("QAOP-ZX ")); break;
    }
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(12,23,0,("         "));
}


// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void DracoDSChangeKeymap(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY= 6, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);

  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  DSPrint(1 ,19,0,("   D-PAD : CHANGE KEY MAP    "));
  DSPrint(1 ,20,0,("       B : RETURN MAIN MENU  "));
  DSPrint(1 ,21,0,("       X : SWAP KEYMAP TYPE  "));
  DSPrint(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);

  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_X | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;

  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 6 ? 17 : ucY -1);
        bIndTch = myConfig.keymap[ucY-6];
        ucHaut=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        DisplayKeymapName(32);
        ucY = (ucY == 17 ? 6 : ucY +1);
        bIndTch = myConfig.keymap[ucY-6];
        ucBas=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }

    if (keysCurrent() & KEY_START)
    {
        SaveConfig(true); // Save options
    }

    if (keysCurrent() & KEY_B)
    {
      bOK = 1;  // Exit menu
    }

    if (keysCurrent() & KEY_LEFT)
    {
        if (ucL == 0) {
          bIndTch = (bIndTch == 0 ? (MAX_KEY_OPTIONS-1) : bIndTch-1);
          ucL=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 7) ucL = 0;
        }
    }
    else
    {
        ucL = 0;
    }

    if (keysCurrent() & KEY_RIGHT)
    {
        if (ucR == 0)
        {
          bIndTch = (bIndTch == (MAX_KEY_OPTIONS-1) ? 0 : bIndTch+1);
          ucR=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else
        {
          ucR++;
          if (ucR > 7) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }

    // Swap Player 1 and Player 2 keymap
    if (keysCurrent() & KEY_X)
    {
        SwapKeymap();
        bIndTch = myConfig.keymap[ucY-6];
        DisplayKeymapName(ucY);
        while (keysCurrent() & KEY_X)
            ;
        WAITVBL
    }
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// -----------------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename, size and CRC32
// -----------------------------------------------------------------------------------------
void DisplayFileName(void)
{
    sprintf(szName, "[%d K] [CRC: %08X]", file_size/1024, file_crc);
    DSPrint((16 - (strlen(szName)/2)),19,0,szName);

    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    DSPrint((16 - (strlen(szName)/2)),21,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 35)   // If there is more than a few characters left, show it on the 2nd line
    {
        if (strlen(gpFic[ucGameChoice].szName) <= 60)
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+30);
        }
        else
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+strlen(gpFic[ucGameChoice].szName)-30);
        }

        if (strlen(szName)>30) szName[30]='\0';
        DSPrint((16 - (strlen(szName)/2)),22,0,szName);
    }
}

//*****************************************************************************
// Display info screen and change options "main menu"
//*****************************************************************************
void dispInfoOptions(u32 uY)
{
    DSPrint(2, 7,(uY== 7 ? 2 : 0),("         LOAD  GAME         "));
    DSPrint(2, 9,(uY== 9 ? 2 : 0),("         PLAY  GAME         "));
    DSPrint(2,11,(uY==11 ? 2 : 0),("       DEFINE  KEYS         "));
    DSPrint(2,13,(uY==13 ? 2 : 0),("         GAME  OPTIONS      "));
    DSPrint(2,15,(uY==15 ? 2 : 0),("       GLOBAL  OPTIONS      "));
    DSPrint(2,17,(uY==17 ? 2 : 0),("         QUIT  EMULATOR     "));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    DSPrint(5,10,0,("   NO GAME SELECTED   "));
    DSPrint(5,12,0,("  PLEASE, USE OPTION  "));
    DSPrint(5,14,0,("      LOAD  GAME      "));
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    dispInfoOptions(ucY);
}


void ReadFileCRCAndConfig(void)
{
    // Reset the mode related vars...
    keyMapType = 0;

    // ----------------------------------------------------------------------------------
    // Clear the entire ROM buffer[] - fill with 0xFF to emulate non-responsive memory
    // ----------------------------------------------------------------------------------
    memset(TapeCartDiskBuffer, 0xFF, MAX_FILE_SIZE);

    // Determine the file type based on the filename extension
    if (strstr(gpFic[ucGameChoice].szName, ".ccc") != 0) draco_mode = MODE_CART;
    if (strstr(gpFic[ucGameChoice].szName, ".CCC") != 0) draco_mode = MODE_CART;
    if (strstr(gpFic[ucGameChoice].szName, ".rom") != 0) draco_mode = MODE_CART;
    if (strstr(gpFic[ucGameChoice].szName, ".ROM") != 0) draco_mode = MODE_CART;
    if (strstr(gpFic[ucGameChoice].szName, ".cas") != 0) draco_mode = MODE_CAS;
    if (strstr(gpFic[ucGameChoice].szName, ".CAS") != 0) draco_mode = MODE_CAS;
    if (strstr(gpFic[ucGameChoice].szName, ".dsk") != 0) draco_mode = MODE_DSK;
    if (strstr(gpFic[ucGameChoice].szName, ".DSK") != 0) draco_mode = MODE_DSK;

    // Save the initial filename and file - we need it for save/restore of state
    strcpy(initial_file, gpFic[ucGameChoice].szName);
    strcpy(last_file, gpFic[ucGameChoice].szName);
    getcwd(initial_path, MAX_FILENAME_LEN);
    getcwd(last_path, MAX_FILENAME_LEN);

    // Grab the all-important file CRC - this also loads the file into TapeCartDiskBuffer[]
    getfile_crc(gpFic[ucGameChoice].szName);

    FindConfig();    // Try to find keymap and config for this file...
}


// ----------------------------------------------------------------------
// Read file twice and ensure we get the same CRC... if not, do it again
// until we get a clean read. Return the filesize to the caller...
// ----------------------------------------------------------------------
u32 ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset)
{
    u32 crc1 = 0;
    u32 crc2 = 1;
    u32 fileSize = 0;

    // --------------------------------------------------------------------------------------------
    // I've seen some rare issues with reading files from the SD card on a DSi so we're doing
    // this slow and careful - we will read twice and ensure that we get the same CRC both times.
    // --------------------------------------------------------------------------------------------
    do
    {
        // Read #1
        crc1 = 0xFFFFFFFF;
        FILE* file = fopen(filename, "rb");
        if (file)
        {
            if (buf_offset) fseek(file, buf_offset, SEEK_SET);
            fileSize = fread(buf, 1, buf_size, file);
            crc1 = getCRC32(buf, buf_size);
            fclose(file);
        }

        // Read #2
        crc2 = 0xFFFFFFFF;
        FILE* file2 = fopen(filename, "rb");
        if (file2)
        {
            if (buf_offset) fseek(file2, buf_offset, SEEK_SET);
            fread(buf, 1, buf_size, file2);
            crc2 = getCRC32(buf, buf_size);
            fclose(file2);
        }
   } while (crc1 != crc2); // If the file couldn't be read, file_size will be 0 and the CRCs will both be 0xFFFFFFFF

   return fileSize;
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void DracoDSChangeOptions(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 7, bOK=0;

  // Upper Screen Background
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  if (myGlobalConfig.defMachine)
  {
      decompress(top_cocoTiles, bgGetGfxPtr(bg0), LZ77Vram);
      decompress(top_cocoMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
      dmaCopy((void*) top_cocoPal,(void*) BG_PALETTE,256*2);
  }
  else
  {
      decompress(top_dragonTiles, bgGetGfxPtr(bg0), LZ77Vram);
      decompress(top_dragonMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
      dmaCopy((void*) top_dragonPal,(void*) BG_PALETTE,256*2);
  }
  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);

  // Lower Screen Background
  BottomScreenOptions();

  dispInfoOptions(ucY);

  if (ucGameChoice != -1)
  {
      DisplayFileName();
  }

  while (!bOK) {
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        dispInfoOptions(32);
        ucY = (ucY == 7 ? 17 : ucY -2);
        ucHaut=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent()  & KEY_DOWN) {
      if (!ucBas) {
        dispInfoOptions(32);
        ucY = (ucY == 17 ? 7 : ucY +2);
        ucBas=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }
    if (keysCurrent()  & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        switch (ucY) {
          case 7 :      // LOAD GAME
            DracoDSLoadFile(FALSE);
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            BottomScreenOptions();
            if (ucGameChoice != -1)
            {
                ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
                DisplayFileName();    // And put up the filename on the bottom screen
            }
            ucY = 9;
            dispInfoOptions(ucY);
            break;
          case 9 :     // PLAY GAME
            if (ucGameChoice != -1)
            {
                bOK = 1;
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 11 :     // REDEFINE KEYS
            if (1==1)
            {
                DracoDSChangeKeymap();
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 13 :     // GAME OPTIONS
            if (1==1)
            {
                DracoDSGameOptions(false);
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
               NoGameSelected(ucY);
            }
            break;

          case 15 :     // GLOBAL OPTIONS
            DracoDSGameOptions(true);
            BottomScreenOptions();
            dispInfoOptions(ucY);
            DisplayFileName();
            break;

          case 17 :     // QUIT EMULATOR
            exit(1);
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1)
      {
        bOK = 1;
      }
      else
      {
        NoGameSelected(ucY);
      }
    }
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************
void DSPrint(int iX,int iY,int iScr,char *szMessage)
{
  u16 *pusScreen,*pusMap;
  u16 usCharac;
  char *pTrTxt=szMessage;

  pusScreen=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );

  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt++;
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // Faster than strcpy/strtoupper

    if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);                   // Will render as a vertical bar
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');          // Number from 0-9 or punctuation
    else
      usCharac=*(pusMap+32+(ch)-'@');       // Character from A-Z
    *pusScreen++=usCharac;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait)
{
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out to black
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}


/*********************************************************************************
 * Keyboard Key Buffering Engine...
 ********************************************************************************/
u8 BufferedKeys[32];
u8 BufferedKeysWriteIdx=0;
u8 BufferedKeysReadIdx=0;
void BufferKey(u8 key)
{
    BufferedKeys[BufferedKeysWriteIdx] = key;
    BufferedKeysWriteIdx = (BufferedKeysWriteIdx+1) % 32;
}

// ---------------------------------------------------------------------------------------
// Called every frame... so 1/50th or 1/60th of a second. We will virtually 'press' and
// hold the key for roughly a tenth of a second and be smart about shift keys...
// ---------------------------------------------------------------------------------------
void ProcessBufferedKeys(void)
{
    static u8 next_dampen_time = 8;
    static u8 dampen = 0;
    static u8 buf_held = 0;

    if (++dampen >= next_dampen_time) // Roughly 150ms... experimentally good enough for Tandy and Dragon
    {
        kbd_keys_pressed = 0;
        if (dampen == next_dampen_time)
        {
            buf_held = 0x00;
        }
        else
        {
            if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
            {
                buf_held = BufferedKeys[BufferedKeysReadIdx];
                BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
                next_dampen_time = 8;
                if (buf_held == 255) {buf_held = 0; kbd_key = 0;}

                if (buf_held == 55) // Shift Key? Grab the next one to go with it...
                {
                    kbd_keys[kbd_keys_pressed++] = buf_held;
                    buf_held = BufferedKeys[BufferedKeysReadIdx];
                    BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
                }
            }
            else
            {
                buf_held = 0x00;
            }
            dampen = 0;
        }
    }

    // See if the shift key should be virtually pressed along with this buffered key...
    if (buf_held) {kbd_key = buf_held; kbd_keys[kbd_keys_pressed++] = buf_held;}
}


/*********************************************************************************
 * Init Dragon/Tandy Emulation Engine for that game
 ********************************************************************************/
u8 DragonTandyInit(char *szGame)
{
  u8 RetFct,uBcl;
  u16 uVide;

  // We've got some debug data we can use for development... reset these.
  memset(debug, 0x00, sizeof(debug));
  DX = DY = 0;

  // -----------------------------------------------------------------
  // Change graphic mode to initiate emulation.
  // Here we can claim back 128K of VRAM which is otherwise unused
  // but we can use it for fast memory swaps and look-up-tables.
  // -----------------------------------------------------------------
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);      // This is our top emulation screen (where the game is played)
  vramSetBankB(VRAM_B_LCD);

  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8);
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;

  // Init the page flipping buffer...
  for (uBcl=0;uBcl<192;uBcl++)
  {
     uVide=(uBcl/12);
     dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
  }

  RetFct = loadgame(szGame);      // Load up the .ccc or .cas game

  ResetDragonTandy();

  // Return with result
  return (RetFct);
}

/*********************************************************************************
 * Run the emul
 ********************************************************************************/
void DragonTandyRun(void)
{
  dragon_reset();                         // Ensure the Dragon/Tandy Emulation is ready
  BottomScreenKeyboard();                 // Show the game-related screen with keypad / keyboard
}

// ------------------------------------------------------------------------------------
// These colors were derived by using other emulators and taking screenshots and then
// using GIMPs color-picker to try and get as close as possible. At first I was just
// assigning RGB values that made sense - for example FB_CYAN was 0x00FFFF but in
// reality the color names are only approximations of the actual colors rendered by
// the Motorola video chip... these aren't perfect but they will be good enough.
// ------------------------------------------------------------------------------------
u8 Dragon_Coco_palette[16*3] =
{
  0x00, 0x00, 0x00, // FB_BLACK

  0x00, 0xFF, 0x00, // FB_GREEN
  0xFF, 0xFF, 0x83, // FB_YELLOW
  0x1B, 0x16, 0xEB, // FB_BLUE
  0xC0, 0x0E, 0x24, // FB_RED

  0xF0, 0xF0, 0xF0, // FB_BUFF (White-ish)
  0x1D, 0x9C, 0x5D, // FB_CYAN (slightly more greenish)
  0xFD, 0x25, 0xFF, // FB_MAGENTA (slightly more purplish)
  0xFE, 0x42, 0x0D, // FB_ORANGE (slightly more reddish)

  0x00, 0x80, 0xFF, // Artifact BLUE
  0xFF, 0x80, 0x00, // Artifact ORANGE
  0x00, 0x80, 0x00, // Artifact Green

  0x10, 0x40, 0x10, // Dark  Green Text
  0x78, 0x50, 0x20, // Dark  Orange Text
  0x00, 0xFF, 0x00, // Light Green Text
  0xF0, 0xB0, 0x40, // Light Orange Text
};


/**********************************************************************************
 * Set Dragon / Tandy color palette... 9 colors (black plus 2 palettes of 4 colors)
 * plus we map some alternate artifact colors for our high-rez rendering emulation.
 *********************************************************************************/
void DragonTandySetPalette(void)
{
  u8 uBcl,r,g,b;

  for (uBcl=0;uBcl<16;uBcl++)
  {
    r = (u8) ((float) Dragon_Coco_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) Dragon_Coco_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) Dragon_Coco_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving HI SCORES and Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *filename)
{
    DSPrint(11,13,6, "LOADING...");
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;

    file_crc = getFileCrc(filename);        // The CRC is used as a unique ID to save out High Scores and Configuration...

    // For .DSK based games, since the disk can be written, we have to base the CRC32 on
    // the filename instead. We use the initial file here in case we swapped disks...
    if (draco_mode >= MODE_DSK)
    {
        file_crc = getCRC32((u8 *)initial_file, strlen(initial_file));
    }

    DSPrint(11,13,6, "          ");
}


/** loadgame() ******************************************************************/
/* Open a rom file from file system and load it into the TapeCartDiskBuffer[] buffer    */
/********************************************************************************/
u8 loadgame(const char *filename)
{
  u8 bOK = 0;
  int romSize = 0;

  FILE* handle = fopen(filename, "rb");
  if (handle != NULL)
  {
    // -----------------------------------------------------------------------
    // See if we are loading a file from a directory different than our
    // last saved directory... if so, we save this new directory as default.
    // -----------------------------------------------------------------------
    getcwd(last_path, MAX_FILENAME_LEN);
    if (myGlobalConfig.lastDir)
    {
        if (strcmp(initial_path, myGlobalConfig.szLastPath) != 0)
        {
            SaveConfig(FALSE);
        }
    }

    // Get file size the 'fast' way - use fstat() instead of fseek() or ftell()
    struct stat stbuf;
    (void)fstat(fileno(handle), &stbuf);
    romSize = stbuf.st_size;
    fclose(handle); // We only need to close the file - the game ROM is now sitting in TapeCartDiskBuffer[] from the getFileCrc() handler

    last_file_size = (u32)romSize;
  }

  return bOK;
}

void vblankIntro()
{
  vusCptVBL++;
}

// --------------------------------------------------------------
// Intro with portabledev logo and new PHEONIX-EDITION version
// --------------------------------------------------------------
void intro_logo(void)
{
  bool bOK;

  // Init graphics
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  irqSet(IRQ_VBLANK, vblankIntro);
  irqEnable(IRQ_VBLANK);

  // Init BG
  int bg1 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  // Init sub BG
  int bg1s = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  REG_BLDCNT = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY = 16;
  REG_BLDCNT_SUB = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY_SUB = 16;

  mmEffect(SFX_MUS_INTRO);

  // Show splash
  if (myGlobalConfig.defMachine)
  {
      decompress(top_cocoTiles, bgGetGfxPtr(bg1), LZ77Vram);
      decompress(top_cocoMap, (void*) bgGetMapPtr(bg1), LZ77Vram);
      dmaCopy((void *) top_cocoPal,(u16*) BG_PALETTE,256*2);
  }
  else
  {
      decompress(top_dragonTiles, bgGetGfxPtr(bg1), LZ77Vram);
      decompress(top_dragonMap, (void*) bgGetMapPtr(bg1), LZ77Vram);
      dmaCopy((void *) top_dragonPal,(u16*) BG_PALETTE,256*2);
  }

  decompress(pdev_bg0Tiles, bgGetGfxPtr(bg1s), LZ77Vram);
  decompress(pdev_bg0Map, (void*) bgGetMapPtr(bg1s), LZ77Vram);
  dmaCopy((void *) pdev_bg0Pal,(u16*) BG_PALETTE_SUB,256*2);

  FadeToColor(0,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,0,3);

  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; } // 0x1FFF = key or touch screen
  vusCptVBL=0;bOK=false;
  while (!bOK && (vusCptVBL<3*60)) { if (keysCurrent() & 0x1FFF ) bOK=true; }
  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; }

  FadeToColor(1,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,16,3);
}


void _putchar(char character) {}

// End of file
