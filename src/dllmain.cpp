#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <mutex>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>

#include "nexus/Nexus.h"
#include "imgui.h"
#include "GW2API.h"
#include "IconManager.h"
#include "PermissionManager.h"
#include "ProxyThrottle.h"
#include "HoardAndSeekAPI.h"
#include <nlohmann/json.hpp>

// Version constants
#define V_MAJOR 0
#define V_MINOR 10
#define V_BUILD 1
#define V_REVISION 0

// Quick Access icon identifiers
#define QA_ID "QA_HOARD_AND_SEEK"
#define TEX_ICON "TEX_HOARD_ICON"
#define TEX_ICON_HOVER "TEX_HOARD_ICON_HOVER"

// Embedded 32x32 hs.png icon (normal)
static const unsigned char ICON_SEARCH[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x01, 0xd9, 0xc9, 0x2c, 0x7f, 0x00, 0x00,
    0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc, 0x61, 0x05, 0x00, 0x00,
    0x00, 0x20, 0x63, 0x48, 0x52, 0x4d, 0x00, 0x00, 0x7a, 0x26, 0x00, 0x00, 0x80, 0x84, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x00, 0x80, 0xe8, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0xea, 0x60, 0x00, 0x00,
    0x3a, 0x98, 0x00, 0x00, 0x17, 0x70, 0x9c, 0xba, 0x51, 0x3c, 0x00, 0x00, 0x00, 0x06, 0x62, 0x4b,
    0x47, 0x44, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xa0, 0xbd, 0xa7, 0x93, 0x00, 0x00, 0x00, 0x09,
    0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c,
    0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xea, 0x05, 0x01, 0x02, 0x37, 0x26,
    0x4a, 0xd9, 0x8b, 0xf6, 0x00, 0x00, 0x05, 0x9e, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xc5, 0x97,
    0x7b, 0x4c, 0x53, 0x67, 0x18, 0xc6, 0xdf, 0x73, 0x4e, 0x0b, 0x6d, 0x29, 0xa7, 0x17, 0x5a, 0x5b,
    0x40, 0x2a, 0x01, 0x41, 0x1c, 0x17, 0xb9, 0x86, 0x9b, 0xc8, 0xa2, 0x08, 0xe8, 0x10, 0x9d, 0x3a,
    0x65, 0x11, 0xd1, 0x20, 0x1b, 0xd3, 0x0c, 0xb3, 0xab, 0xf1, 0x12, 0x77, 0x8b, 0x66, 0xcb, 0x12,
    0xe7, 0x2e, 0x4e, 0xcd, 0x74, 0x8a, 0xd3, 0x78, 0x09, 0xdb, 0x98, 0xc2, 0x90, 0x6d, 0xcc, 0x31,
    0xd0, 0x21, 0x5e, 0x40, 0x0a, 0xb4, 0x58, 0xa0, 0x94, 0x5e, 0xe9, 0x95, 0x5e, 0x69, 0x29, 0x6d,
    0xcf, 0xfe, 0x59, 0x96, 0x2d, 0x6e, 0x19, 0x45, 0x70, 0xef, 0x7f, 0x27, 0x39, 0xcf, 0xf7, 0xfc,
    0xf2, 0x9e, 0xef, 0x7c, 0xdf, 0xfb, 0x00, 0xfc, 0xcf, 0x85, 0xcd, 0xd6, 0x42, 0xf7, 0x9a, 0x8e,
    0x04, 0xc8, 0x14, 0xfa, 0xe5, 0xe1, 0xfc, 0x90, 0x35, 0x54, 0x6a, 0x60, 0x59, 0x5a, 0x62, 0xf4,
    0xd2, 0x85, 0x91, 0xa1, 0xf3, 0x2b, 0x36, 0x2e, 0xb7, 0xb4, 0x76, 0xf4, 0x5a, 0xfe, 0x4d, 0x87,
    0x3c, 0xae, 0xf1, 0x8b, 0x5b, 0x8a, 0xf0, 0x9b, 0x77, 0x44, 0xfb, 0xab, 0xcb, 0x8b, 0xaa, 0xf1,
    0xe0, 0x20, 0x66, 0x94, 0x80, 0x07, 0x8e, 0x09, 0x17, 0xd0, 0x69, 0x14, 0xb0, 0x4f, 0xb8, 0xc0,
    0x3e, 0xe1, 0x82, 0x5b, 0x77, 0xc5, 0xed, 0x3e, 0x1f, 0xf1, 0xce, 0x67, 0x67, 0x1b, 0x6f, 0xcc,
    0x6a, 0x07, 0xf2, 0xb3, 0x12, 0x72, 0x32, 0x53, 0x62, 0x7f, 0xa9, 0xa9, 0x2c, 0x59, 0xed, 0xf5,
    0xfa, 0x50, 0xad, 0x7e, 0xfc, 0x1a, 0x99, 0x44, 0x3a, 0x7e, 0x5f, 0x38, 0x7c, 0x5e, 0x6f, 0xb2,
    0x36, 0x06, 0xd1, 0x28, 0xdd, 0xa6, 0x71, 0x1b, 0x3d, 0x37, 0x7d, 0x71, 0x36, 0x87, 0x15, 0xbc,
    0x0d, 0x45, 0xd1, 0xf0, 0xed, 0x9b, 0x56, 0xfc, 0xf0, 0x53, 0xdb, 0x03, 0xef, 0x63, 0x77, 0xa0,
    0x28, 0x3f, 0x25, 0x6f, 0x5d, 0x71, 0x56, 0x73, 0x52, 0xdc, 0x02, 0x9a, 0x68, 0x50, 0xf9, 0xcd,
    0x8d, 0x5b, 0xc2, 0x9a, 0x4b, 0x57, 0xdb, 0x34, 0xff, 0xf4, 0xee, 0xa1, 0x3d, 0xe5, 0x4f, 0x73,
    0xd9, 0xf8, 0x69, 0x9c, 0x4e, 0x8b, 0xbe, 0xd2, 0x70, 0xb3, 0xf9, 0xf5, 0xea, 0xb5, 0x6b, 0xf3,
    0xd6, 0xef, 0x73, 0xcf, 0x18, 0x60, 0x77, 0x65, 0x09, 0x9f, 0x3f, 0x8f, 0x25, 0x4c, 0x4d, 0x88,
    0xe6, 0x8e, 0x28, 0xb4, 0x07, 0x77, 0xee, 0x3b, 0x71, 0x68, 0x1a, 0x1a, 0xba, 0x20, 0x9c, 0xdb,
    0xc4, 0x61, 0xe1, 0x79, 0xe7, 0xbf, 0x6d, 0x3d, 0xfa, 0xf3, 0xcd, 0x9e, 0xd7, 0x66, 0xfc, 0x09,
    0x28, 0x81, 0x01, 0x9f, 0x6f, 0x5e, 0xb3, 0x34, 0x67, 0x44, 0x3e, 0x76, 0x7e, 0xe7, 0xfe, 0x93,
    0xaf, 0x4e, 0x47, 0xd3, 0xd9, 0x2d, 0x71, 0x67, 0x24, 0xc7, 0xd4, 0xd3, 0xa8, 0x81, 0x5b, 0xc2,
    0xf8, 0xec, 0x95, 0x61, 0x3c, 0x56, 0x93, 0x50, 0x2c, 0x53, 0xa3, 0xfe, 0x9a, 0xbf, 0x54, 0x5e,
    0x1c, 0x56, 0xb2, 0x22, 0x7d, 0x8b, 0x4c, 0xa1, 0x1b, 0xef, 0x97, 0x28, 0x6a, 0xfc, 0xd1, 0x1e,
    0xfe, 0xb4, 0xce, 0x4c, 0xf8, 0x88, 0xbd, 0x51, 0x02, 0x1e, 0x6a, 0xb3, 0x3b, 0xf7, 0x03, 0x00,
    0xf8, 0x0d, 0xd0, 0xdd, 0x2f, 0x7d, 0x0e, 0xc7, 0x69, 0x18, 0x86, 0xa1, 0x67, 0x8f, 0xd5, 0x7e,
    0x6f, 0xf1, 0x57, 0xbf, 0xeb, 0xc0, 0xc9, 0x8b, 0x8e, 0x89, 0x49, 0xe5, 0xe2, 0x98, 0x88, 0xd2,
    0x9d, 0x5b, 0x57, 0x31, 0xfc, 0x06, 0x40, 0x51, 0x34, 0x2b, 0x84, 0x19, 0x0c, 0x36, 0xbb, 0xb3,
    0x69, 0xa6, 0x1b, 0x98, 0x4c, 0xc2, 0x9a, 0xb9, 0x6c, 0x1c, 0x1b, 0x55, 0xe9, 0xf2, 0xfc, 0x06,
    0x60, 0x31, 0x82, 0xa2, 0x48, 0x24, 0x0c, 0x04, 0xe1, 0x5c, 0xd1, 0x4c, 0x01, 0xe8, 0x41, 0xd4,
    0xfb, 0x0c, 0x3c, 0x08, 0x10, 0x04, 0x89, 0xf4, 0x1b, 0x80, 0xcd, 0x0c, 0x46, 0x3d, 0x1e, 0x2f,
    0x64, 0xa6, 0xc4, 0xea, 0x66, 0x0a, 0x80, 0x00, 0xf8, 0xbc, 0x5e, 0x1f, 0xb0, 0x18, 0x74, 0x8a,
    0xdf, 0x00, 0x5d, 0xbd, 0xc3, 0x1a, 0x32, 0x99, 0x04, 0x4e, 0x97, 0x3b, 0x6e, 0xa6, 0x00, 0x2c,
    0x26, 0x9d, 0x1b, 0x18, 0x40, 0x82, 0x51, 0xa5, 0xce, 0xe6, 0x37, 0x00, 0x82, 0x20, 0x3d, 0x16,
    0xdb, 0x04, 0x8c, 0x5b, 0xec, 0x4b, 0x66, 0xdc, 0x01, 0x04, 0xc9, 0xd4, 0x1a, 0x2c, 0x80, 0x07,
    0xd3, 0xfa, 0xfd, 0x06, 0x88, 0x12, 0xf0, 0x1a, 0x95, 0x6a, 0x03, 0xb8, 0x26, 0xa7, 0x2a, 0x66,
    0x62, 0x2e, 0x69, 0x3b, 0xc1, 0xc1, 0xe9, 0xd4, 0xc2, 0xc1, 0x11, 0xb5, 0xe9, 0x83, 0x7d, 0x15,
    0x9d, 0x7e, 0x03, 0x34, 0xb4, 0xdc, 0xed, 0x6c, 0xbf, 0x23, 0xea, 0x32, 0x5b, 0x1d, 0x45, 0x63,
    0x5d, 0xe7, 0x72, 0xfd, 0xd5, 0x73, 0xd8, 0xf8, 0x41, 0xb3, 0xd5, 0x11, 0xd8, 0xda, 0xd1, 0x57,
    0x9b, 0x58, 0xb0, 0x7b, 0xca, 0x6f, 0x80, 0xea, 0xf2, 0xa2, 0x58, 0x8f, 0xc7, 0xeb, 0x5b, 0x18,
    0xc9, 0x07, 0xf1, 0x90, 0xe2, 0x02, 0xa1, 0xbf, 0xce, 0x99, 0xae, 0x96, 0xb0, 0xb4, 0x14, 0x32,
    0x82, 0x69, 0xbb, 0xbf, 0xbc, 0xdc, 0x62, 0xcc, 0x49, 0x8b, 0x7b, 0xdf, 0xaf, 0x83, 0xe8, 0x95,
    0xaa, 0x52, 0x5e, 0x7c, 0xac, 0xe0, 0x48, 0x6a, 0x42, 0xb4, 0xa8, 0xe9, 0xab, 0xb7, 0xd2, 0x17,
    0x45, 0x85, 0x83, 0xc9, 0x6c, 0x8f, 0x94, 0x4a, 0xe4, 0xad, 0x84, 0xba, 0x21, 0xf2, 0x3f, 0xcd,
    0x89, 0xdb, 0xeb, 0x61, 0xca, 0x53, 0x7f, 0xea, 0xe2, 0x8f, 0x3e, 0x89, 0x54, 0xb5, 0xfd, 0xdc,
    0xd7, 0x37, 0x0c, 0xd3, 0xbe, 0x8c, 0x08, 0x75, 0x03, 0x56, 0xb2, 0xf5, 0xdd, 0xce, 0x5d, 0x15,
    0xab, 0xd3, 0xe2, 0x17, 0x45, 0xc0, 0x82, 0x18, 0x01, 0x74, 0xdf, 0x11, 0x41, 0x6b, 0x47, 0x2f,
    0x60, 0x18, 0x06, 0x3b, 0xca, 0x0a, 0x1c, 0x41, 0x61, 0x9c, 0x0f, 0x81, 0xce, 0x3f, 0x89, 0x20,
    0xf1, 0x7f, 0xfb, 0x3d, 0x09, 0x6b, 0x4b, 0xfa, 0xa4, 0xde, 0xfc, 0x26, 0x02, 0xb0, 0xe9, 0x93,
    0x33, 0x8d, 0x53, 0x1f, 0x9f, 0xbe, 0xf6, 0x82, 0x5a, 0x6b, 0x3a, 0x37, 0xed, 0xeb, 0x78, 0xfd,
    0xaa, 0x6c, 0x8a, 0xd9, 0xe2, 0xa8, 0x5d, 0x57, 0x9c, 0xb5, 0x19, 0x41, 0x00, 0xf8, 0x5c, 0x26,
    0xa8, 0xc6, 0x4c, 0xe0, 0xf1, 0x7a, 0xe1, 0xe2, 0x77, 0x6d, 0xda, 0x61, 0x99, 0xe6, 0x8b, 0x8c,
    0xe4, 0x98, 0xaa, 0x03, 0x35, 0x9b, 0x42, 0x33, 0x53, 0x62, 0x81, 0xca, 0xc6, 0x45, 0x80, 0x07,
    0x59, 0x01, 0x41, 0x30, 0x70, 0xba, 0xe6, 0x8f, 0x0d, 0x2a, 0x42, 0x5b, 0x3b, 0xfa, 0xe0, 0xe8,
    0xa9, 0xab, 0x42, 0x8b, 0x75, 0x62, 0xc7, 0x43, 0xa9, 0xea, 0xde, 0xb4, 0x27, 0xa2, 0x65, 0x99,
    0xf1, 0xe1, 0x14, 0x4a, 0x40, 0x7d, 0x55, 0xd9, 0xca, 0x0c, 0x3c, 0x98, 0x06, 0x97, 0xaf, 0xb6,
    0x81, 0x54, 0xae, 0x05, 0x06, 0x1e, 0x04, 0x4e, 0xd7, 0x24, 0x0c, 0xc9, 0x34, 0x35, 0x32, 0x85,
    0xee, 0x58, 0x6a, 0x62, 0x34, 0x75, 0x4c, 0x37, 0x5e, 0x86, 0x61, 0xe8, 0x86, 0xf8, 0x58, 0x41,
    0xc6, 0x53, 0xb1, 0x11, 0xf3, 0x30, 0x14, 0x25, 0x7a, 0x44, 0x32, 0xc5, 0x3d, 0xe1, 0xe0, 0x6f,
    0x24, 0x12, 0x76, 0xe9, 0x8d, 0xea, 0x67, 0x1b, 0xf6, 0x1c, 0xae, 0x25, 0xa6, 0x3d, 0x92, 0xad,
    0xc8, 0x4d, 0x5a, 0x1a, 0x11, 0xc6, 0xa9, 0xdb, 0xf0, 0x4c, 0x0e, 0x9f, 0x12, 0x40, 0x86, 0x86,
    0x96, 0xbb, 0xd0, 0xd5, 0x2b, 0x05, 0x9d, 0xd1, 0x0c, 0xe1, 0xfc, 0x10, 0x18, 0x92, 0x69, 0x24,
    0x55, 0xcf, 0xaf, 0x8c, 0x7b, 0xfb, 0xc8, 0xa5, 0x47, 0x16, 0x3d, 0x76, 0xa8, 0x1a, 0x21, 0x93,
    0x30, 0xa8, 0xde, 0x7b, 0x9c, 0x98, 0xc6, 0xa9, 0xf8, 0x68, 0x15, 0x2e, 0x4b, 0x7e, 0x39, 0x71,
    0x71, 0xe4, 0x47, 0x85, 0xcb, 0x92, 0xc9, 0x5e, 0xaf, 0x0f, 0xea, 0x9b, 0x6f, 0xc3, 0xc0, 0x90,
    0x12, 0x9c, 0x2e, 0x37, 0x10, 0x04, 0x41, 0x0c, 0x8f, 0x8e, 0x9d, 0x0e, 0xa6, 0x53, 0xdf, 0x53,
    0xa8, 0x0d, 0xca, 0xc7, 0x9d, 0x29, 0x49, 0x7f, 0x7d, 0xa8, 0xdc, 0x5c, 0x10, 0xf8, 0x40, 0x34,
    0x72, 0x3c, 0x3f, 0x3b, 0xa1, 0x32, 0x3d, 0x69, 0x21, 0xc8, 0x55, 0x06, 0xf8, 0xb5, 0xa3, 0x17,
    0x64, 0x4a, 0x1d, 0x4c, 0xba, 0x3d, 0xe0, 0x9a, 0x74, 0x3b, 0x75, 0x46, 0xcb, 0x36, 0xb3, 0xd5,
    0x51, 0x67, 0xb6, 0x3a, 0x66, 0x65, 0x9a, 0xfe, 0xb3, 0x03, 0x05, 0x79, 0x4b, 0xc2, 0x7d, 0x3e,
    0xa2, 0x6e, 0xc3, 0xea, 0xec, 0x6c, 0x0e, 0x0b, 0x07, 0xc9, 0x88, 0x1a, 0xee, 0x0b, 0x87, 0x41,
    0xae, 0xd2, 0x01, 0x95, 0x12, 0x08, 0xc6, 0x71, 0x9b, 0xd9, 0x68, 0xb6, 0x95, 0xea, 0x8d, 0x96,
    0xf6, 0xd9, 0xcc, 0x05, 0xa4, 0x3f, 0x36, 0x5b, 0x2e, 0x9b, 0x41, 0xaf, 0x2b, 0xcc, 0x4f, 0x09,
    0x0d, 0x0c, 0x20, 0x43, 0xbf, 0x44, 0x0e, 0x5d, 0x7d, 0x52, 0x90, 0x29, 0x75, 0xc0, 0x66, 0xd0,
    0x41, 0xa1, 0x36, 0x28, 0xdc, 0x53, 0x53, 0xc5, 0x7a, 0xa3, 0x45, 0x04, 0xb3, 0x5c, 0x48, 0x51,
    0x7e, 0x4a, 0x49, 0x62, 0x5c, 0x64, 0x5d, 0x5a, 0x52, 0x34, 0xc5, 0x60, 0xb2, 0xc2, 0x90, 0x4c,
    0x03, 0x42, 0xb1, 0x0c, 0x74, 0x06, 0x0b, 0xf0, 0xb9, 0x4c, 0x18, 0x94, 0x69, 0xc4, 0x36, 0xbb,
    0xb3, 0x68, 0xdc, 0x62, 0x57, 0xcc, 0x45, 0x32, 0x22, 0xf1, 0x38, 0xcc, 0x0b, 0x51, 0x0b, 0x78,
    0x14, 0x85, 0xda, 0x00, 0x03, 0xc3, 0x4a, 0x50, 0x6a, 0x8c, 0xa0, 0x37, 0x5a, 0x80, 0xc7, 0x65,
    0x82, 0x78, 0x48, 0xd9, 0xc1, 0x61, 0xe3, 0x25, 0x72, 0x95, 0xde, 0x34, 0x57, 0xd1, 0x8c, 0x44,
    0x26, 0x63, 0x8c, 0xbe, 0x87, 0x72, 0xb0, 0x3b, 0x9c, 0x20, 0x1d, 0xd5, 0x82, 0xc5, 0xe6, 0x80,
    0x79, 0x1c, 0x26, 0xf4, 0x0e, 0x8c, 0x5e, 0x67, 0x31, 0xe8, 0x1b, 0x85, 0x62, 0xd9, 0xc4, 0x5c,
    0x66, 0x43, 0x54, 0x35, 0x66, 0x1a, 0x53, 0xaa, 0x0d, 0xd0, 0xff, 0x50, 0x0e, 0x93, 0xee, 0x29,
    0xe0, 0x71, 0x99, 0x20, 0x14, 0xcb, 0xce, 0x2c, 0xcf, 0x4d, 0x2a, 0x95, 0x48, 0x55, 0x73, 0x6a,
    0x0e, 0x00, 0x80, 0xce, 0x0f, 0x0d, 0xd9, 0x66, 0x34, 0xdb, 0x6c, 0x2c, 0x26, 0x1d, 0x30, 0x0c,
    0x9d, 0xea, 0x11, 0xc9, 0x0e, 0x18, 0x4c, 0xd6, 0x1d, 0x57, 0xae, 0xb5, 0x7b, 0x9e, 0x44, 0x3a,
    0x46, 0x00, 0x00, 0x8a, 0x9e, 0x4e, 0x65, 0x11, 0x04, 0xb1, 0x44, 0xa3, 0x35, 0x0d, 0xf7, 0x0e,
    0x8c, 0x2a, 0xe0, 0x09, 0xd6, 0xef, 0xe0, 0xc9, 0x62, 0xe1, 0xe9, 0xb8, 0x80, 0x54, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_SEARCH_size = 1626;

// Embedded 35x35 hs.png icon (hover - 20% brighter, 10% larger)
static const unsigned char ICON_SEARCH_HOVER[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x23, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1e, 0xd9, 0xb3,
    0x59, 0x00, 0x00, 0x08, 0x5f, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0xbd, 0x98, 0x0b, 0x50, 0x54,
    0xd7, 0x19, 0xc7, 0xcf, 0xb9, 0xf7, 0xee, 0x0b, 0x76, 0x59, 0x9e, 0x2b, 0x20, 0xe1, 0xa9, 0xe1,
    0xbd, 0x8a, 0x8b, 0x92, 0x28, 0x88, 0x18, 0xb5, 0x8d, 0x46, 0x4d, 0x94, 0x8b, 0x6d, 0x8c, 0xb4,
    0xd6, 0x31, 0x1d, 0x3a, 0x6d, 0x89, 0x89, 0x9a, 0xa6, 0xc6, 0xac, 0x64, 0x82, 0x4e, 0xd2, 0x86,
    0x56, 0x53, 0x75, 0x24, 0x4d, 0xb4, 0x63, 0x9b, 0x98, 0x5d, 0x4d, 0xaa, 0x71, 0xd4, 0x8e, 0x0f,
    0x24, 0x54, 0x90, 0xd1, 0x0b, 0x86, 0x05, 0x04, 0xdd, 0x95, 0x05, 0x16, 0x96, 0x65, 0x61, 0x77,
    0xd9, 0x07, 0xfb, 0xbc, 0xf7, 0x74, 0xee, 0xba, 0x5a, 0x33, 0x29, 0xba, 0x10, 0xed, 0x7f, 0xe6,
    0xce, 0xbd, 0xe7, 0xde, 0x73, 0xbe, 0xf3, 0x3b, 0xdf, 0xf9, 0xbe, 0x73, 0xce, 0x2e, 0x00, 0x8f,
    0x51, 0x72, 0x39, 0xc0, 0xe4, 0x72, 0x39, 0x06, 0x21, 0x04, 0xf7, 0x2e, 0x56, 0xec, 0x1d, 0x21,
    0x04, 0xd9, 0x6f, 0x6c, 0x71, 0xa2, 0xf6, 0x13, 0x7e, 0x98, 0x8c, 0x48, 0x92, 0xc4, 0x15, 0x0a,
    0x05, 0x82, 0x10, 0x32, 0x6c, 0x39, 0x73, 0x66, 0x72, 0x81, 0xdb, 0xe3, 0x4c, 0x31, 0x5b, 0xc6,
    0xa7, 0xa7, 0x25, 0x4b, 0x5c, 0x34, 0x62, 0xfa, 0x04, 0x38, 0x97, 0x6a, 0x6c, 0xed, 0x1e, 0x64,
    0xbf, 0xcb, 0xe5, 0xc5, 0x04, 0xa8, 0xaa, 0x67, 0xaa, 0x00, 0xf0, 0xd7, 0x7f, 0x5c, 0x30, 0xf0,
    0xd0, 0xa1, 0x57, 0x89, 0x8a, 0x8a, 0x8f, 0xbd, 0x0c, 0x83, 0xe2, 0x16, 0x17, 0xe4, 0x6c, 0x79,
    0xe3, 0x57, 0x2f, 0x3d, 0xc7, 0xe3, 0xf1, 0x12, 0x12, 0x62, 0x23, 0xc2, 0xc7, 0x6c, 0x4e, 0xae,
    0x58, 0x24, 0x00, 0x2e, 0xb7, 0xd7, 0xce, 0xe3, 0x72, 0x46, 0x2f, 0x35, 0xb6, 0x75, 0x9e, 0xab,
    0x6b, 0x39, 0x72, 0xfa, 0xe2, 0x35, 0x05, 0xdb, 0x75, 0x69, 0x69, 0x29, 0x57, 0xa9, 0x54, 0x7a,
    0x1e, 0x07, 0x0c, 0x86, 0x10, 0x62, 0xa7, 0x80, 0x29, 0x2f, 0x2d, 0x29, 0x5f, 0xbb, 0x7c, 0xfe,
    0x6f, 0x66, 0xa4, 0xc4, 0xe5, 0x7b, 0x3c, 0x3e, 0x30, 0x64, 0x30, 0x75, 0xe5, 0x64, 0x24, 0xb5,
    0xf4, 0xea, 0x8c, 0x16, 0x1e, 0x9f, 0xcb, 0x11, 0x0b, 0xf9, 0x92, 0x41, 0x83, 0xa9, 0x30, 0x36,
    0x26, 0x32, 0x8a, 0x43, 0xe0, 0x23, 0x67, 0xea, 0xa9, 0xb3, 0x7b, 0xfe, 0xfc, 0x45, 0x8d, 0xde,
    0x68, 0xbd, 0x91, 0x36, 0x23, 0x8d, 0xa7, 0x56, 0xab, 0xdd, 0xac, 0x41, 0x62, 0xaa, 0x24, 0x75,
    0x77, 0x63, 0xc3, 0xf7, 0xfb, 0xdf, 0xae, 0xfb, 0xf5, 0x8b, 0x4b, 0xe7, 0xee, 0x48, 0x4f, 0x8b,
    0x8f, 0x1d, 0x1c, 0xb6, 0x5c, 0xec, 0x1b, 0x34, 0x1e, 0x32, 0x19, 0xcd, 0xb7, 0x8d, 0x06, 0xc7,
    0x60, 0xf9, 0xb6, 0x0f, 0xdd, 0x89, 0x89, 0x89, 0xd8, 0xde, 0x5d, 0x1b, 0x84, 0xb6, 0x31, 0x7b,
    0x7c, 0x8c, 0x58, 0x9c, 0xe7, 0x76, 0x7b, 0x5f, 0x2f, 0x94, 0x65, 0x6d, 0xd8, 0x56, 0x51, 0x9a,
    0xb8, 0xff, 0xe8, 0x99, 0x6a, 0x6d, 0x4f, 0xcf, 0x79, 0x76, 0x9a, 0x95, 0x4a, 0x25, 0x3d, 0x25,
    0x10, 0x92, 0x24, 0x71, 0x1c, 0xc3, 0xc0, 0x6b, 0x9b, 0x57, 0x6f, 0xb8, 0xa4, 0xac, 0x36, 0x1a,
    0x5a, 0xff, 0xe6, 0x6a, 0xbb, 0xb0, 0xef, 0x0f, 0xeb, 0x57, 0x95, 0xa4, 0x3f, 0xaa, 0x6d, 0xd5,
    0xd6, 0x0d, 0xcf, 0xd6, 0x2b, 0x77, 0x7f, 0xd5, 0x70, 0x62, 0x0f, 0xaa, 0xde, 0xf6, 0x4a, 0x43,
    0x28, 0x87, 0x93, 0x13, 0x98, 0xa1, 0x40, 0xb8, 0x4f, 0x4e, 0x50, 0xa1, 0x50, 0x60, 0x65, 0x65,
    0x65, 0xe9, 0x9f, 0x1f, 0x78, 0xe3, 0xc4, 0x7c, 0x59, 0x66, 0xc6, 0x9d, 0x5e, 0x43, 0x4d, 0x49,
    0xd9, 0x8e, 0x2a, 0x00, 0x80, 0xb5, 0xa7, 0xee, 0x30, 0x5f, 0x7b, 0x59, 0xeb, 0x33, 0x66, 0x67,
    0xa3, 0x8e, 0x8e, 0x0e, 0x94, 0xdd, 0xd9, 0x09, 0x01, 0x49, 0x82, 0x54, 0xf3, 0x05, 0x4c, 0xb6,
    0x60, 0x09, 0x84, 0x39, 0x65, 0x9e, 0x97, 0xd7, 0x3c, 0x9b, 0xb4, 0x91, 0x7c, 0x7e, 0x3f, 0x81,
    0x63, 0x2b, 0x14, 0xa7, 0xbe, 0x39, 0x7a, 0x60, 0x95, 0x78, 0x23, 0x20, 0x95, 0xdf, 0x09, 0xe6,
    0xa0, 0xd3, 0x17, 0x00, 0x00, 0xd6, 0xad, 0x2c, 0xfc, 0xb8, 0xe9, 0xe4, 0x07, 0xe8, 0xda, 0xe9,
    0x3f, 0x9e, 0xa9, 0xfc, 0xe5, 0xea, 0x64, 0xd6, 0x53, 0x75, 0xf2, 0x9f, 0xf1, 0x1f, 0xd5, 0x5e,
    0xa1, 0xd8, 0x22, 0x60, 0xef, 0xc7, 0x0e, 0xbe, 0x39, 0xeb, 0x92, 0xb2, 0xba, 0xef, 0xd4, 0xa7,
    0x6f, 0x99, 0x97, 0x16, 0x49, 0x17, 0x4f, 0xc9, 0x2d, 0x08, 0x21, 0x18, 0x1b, 0x1b, 0x1b, 0x73,
    0xfc, 0xe0, 0xf6, 0xae, 0x7f, 0x7f, 0xf5, 0xbe, 0xe3, 0xcb, 0xda, 0xdf, 0xbd, 0xc0, 0xbe, 0x3f,
    0xb3, 0x77, 0x2f, 0x2f, 0xd8, 0xf6, 0x75, 0x72, 0x39, 0xb1, 0x85, 0x7c, 0x46, 0x70, 0xfd, 0x74,
    0xcd, 0xd6, 0xc6, 0x93, 0xef, 0xa3, 0x9f, 0x93, 0x8b, 0x8f, 0xb3, 0xdf, 0xfc, 0xa3, 0x9c, 0x8c,
    0x30, 0x0c, 0x43, 0x42, 0x1e, 0x7a, 0x01, 0x41, 0x30, 0xdd, 0xed, 0xf5, 0x34, 0xf1, 0x45, 0xa1,
    0x37, 0xd9, 0xc5, 0x4c, 0x20, 0x35, 0x05, 0x15, 0x80, 0x10, 0x42, 0x24, 0x8a, 0x8f, 0x87, 0x35,
    0x8a, 0x26, 0x17, 0x0d, 0xe0, 0x05, 0xbb, 0xdd, 0xe5, 0x96, 0xe5, 0xa6, 0xcd, 0xab, 0x78, 0x79,
    0x45, 0xc4, 0xa4, 0xb3, 0x09, 0x21, 0x04, 0x78, 0x02, 0x5e, 0xb6, 0x24, 0x5a, 0x2c, 0x8c, 0x0c,
    0x17, 0x36, 0xe6, 0x2e, 0xa9, 0xec, 0x03, 0xfe, 0xc5, 0x4b, 0x1e, 0xb4, 0x0d, 0x59, 0x44, 0x04,
    0xc3, 0x42, 0x51, 0xa7, 0xff, 0x62, 0xe6, 0x70, 0xb1, 0xc6, 0xa8, 0xc8, 0xb0, 0x59, 0xba, 0x91,
    0x1b, 0x0b, 0x26, 0xed, 0x19, 0x56, 0x71, 0x92, 0xc8, 0xe9, 0xc2, 0x50, 0x01, 0x88, 0x8e, 0x0e,
    0xd7, 0x02, 0x00, 0xbc, 0x0a, 0x85, 0x02, 0xdf, 0xb5, 0xab, 0x0a, 0x81, 0x60, 0x45, 0x76, 0xf8,
    0xeb, 0xa6, 0xc4, 0x4a, 0x2c, 0x62, 0x91, 0xa8, 0x33, 0x3c, 0x2c, 0x84, 0x4f, 0x10, 0xf8, 0xf4,
    0x29, 0xc1, 0x3c, 0x35, 0x2d, 0x0a, 0xc7, 0x21, 0x04, 0x8c, 0x97, 0x1e, 0x65, 0xcb, 0x59, 0xa0,
    0x83, 0x2d, 0x06, 0x0f, 0x13, 0x10, 0x13, 0xe6, 0x46, 0x0c, 0xcd, 0xd0, 0x3e, 0x1a, 0xc1, 0xd8,
    0xe8, 0x08, 0xfe, 0xa4, 0x61, 0x20, 0x84, 0xa0, 0xbe, 0xb9, 0x7d, 0xc4, 0xe1, 0x74, 0xa3, 0xc8,
    0x70, 0x71, 0x22, 0xfb, 0x2e, 0x9b, 0xac, 0xf2, 0xb2, 0x81, 0x19, 0xb4, 0x11, 0x65, 0xb6, 0xbf,
    0x6e, 0x54, 0xa8, 0x98, 0x1b, 0x15, 0x1e, 0x2a, 0xe6, 0x12, 0x18, 0x7d, 0x4b, 0x3b, 0xe8, 0x9c,
    0x92, 0x67, 0x86, 0x86, 0xc7, 0xee, 0x18, 0x4d, 0x56, 0x60, 0x75, 0x3a, 0x52, 0x11, 0xd2, 0x87,
    0xb2, 0x5e, 0xd9, 0x35, 0x99, 0xad, 0x25, 0x26, 0x06, 0xfa, 0xe1, 0x89, 0x50, 0xb1, 0xc3, 0xe5,
    0x95, 0x9a, 0xad, 0x4e, 0x17, 0x80, 0xf0, 0xce, 0x94, 0x60, 0x12, 0xe2, 0xa2, 0xeb, 0xcd, 0x66,
    0xbb, 0xd9, 0x38, 0x62, 0x5d, 0x0e, 0xec, 0x5d, 0x29, 0x0a, 0x92, 0xc4, 0x77, 0x51, 0xb5, 0x78,
    0xd0, 0x06, 0x16, 0xdd, 0xcd, 0x2a, 0x10, 0xc1, 0x9f, 0x96, 0x10, 0x17, 0x99, 0xd9, 0xad, 0xd1,
    0x59, 0x96, 0x2d, 0x9f, 0xd3, 0x3c, 0xd9, 0x6c, 0x82, 0xa5, 0xa5, 0xa5, 0x9c, 0xac, 0x08, 0x67,
    0x07, 0xd5, 0x71, 0xa7, 0x21, 0x7d, 0xe6, 0x53, 0xab, 0xc7, 0x8d, 0x96, 0x05, 0x65, 0x4a, 0x65,
    0x3b, 0x52, 0x90, 0x38, 0x3b, 0x5a, 0x7f, 0x27, 0x0f, 0x11, 0x42, 0x75, 0x04, 0x4b, 0x63, 0x42,
    0xe7, 0xc5, 0xc0, 0x31, 0xfe, 0x53, 0x9a, 0xa6, 0xf9, 0xa7, 0x2f, 0x5e, 0xbb, 0x42, 0xb5, 0x69,
    0x6c, 0x93, 0x64, 0x01, 0x18, 0xbb, 0xd2, 0xb2, 0xe2, 0xe1, 0xb0, 0xe6, 0xf2, 0xe7, 0x72, 0x1f,
    0xd2, 0x9f, 0xba, 0xd1, 0xdd, 0xfc, 0xf7, 0xd9, 0xec, 0xbb, 0xbe, 0x3e, 0x85, 0x00, 0x21, 0x34,
    0xa1, 0x87, 0x10, 0x42, 0xc4, 0x5d, 0x18, 0xf6, 0xd9, 0xbc, 0x06, 0x79, 0x9a, 0xcd, 0xfb, 0xde,
    0xd9, 0x68, 0x04, 0x00, 0xcc, 0x09, 0x1c, 0xbc, 0x82, 0x12, 0x64, 0xb7, 0x81, 0xc0, 0x92, 0x9d,
    0xf1, 0xd1, 0xce, 0x8d, 0xaf, 0xed, 0xff, 0xb0, 0x92, 0xd2, 0x75, 0x9f, 0xf2, 0xf8, 0x86, 0xce,
    0xa2, 0x7e, 0xea, 0xf0, 0x39, 0x84, 0xda, 0xb3, 0x1e, 0xe8, 0x14, 0x67, 0x3b, 0xfd, 0xef, 0xc5,
    0x42, 0x90, 0xf7, 0x21, 0x11, 0x42, 0x0b, 0x11, 0x32, 0x5e, 0x6d, 0x6b, 0x3a, 0xc2, 0xcc, 0xcd,
    0x49, 0xad, 0xe6, 0x72, 0x09, 0xc0, 0x9e, 0x46, 0x82, 0xf2, 0x06, 0x0c, 0x54, 0x5b, 0xfd, 0xe3,
    0xfc, 0x95, 0xaf, 0x6f, 0x7e, 0xf1, 0x2c, 0x75, 0xae, 0x06, 0x21, 0x34, 0x80, 0x10, 0xd2, 0x31,
    0x5d, 0x8d, 0x9f, 0x78, 0xb5, 0x57, 0x6b, 0x11, 0xb2, 0x5f, 0x6e, 0x40, 0x08, 0x15, 0x6a, 0x10,
    0x12, 0x4f, 0x64, 0xc8, 0x8e, 0xb4, 0x71, 0x08, 0x21, 0x12, 0x21, 0x5d, 0xa7, 0xe6, 0x86, 0x12,
    0xbd, 0xb5, 0x69, 0xd9, 0x61, 0x20, 0x16, 0x47, 0x14, 0x17, 0xfb, 0x8f, 0x32, 0x8f, 0xdc, 0x9e,
    0x20, 0x3b, 0x10, 0xf6, 0x21, 0x3b, 0x3b, 0x79, 0x56, 0xb1, 0x2c, 0xeb, 0xfc, 0xb6, 0x8a, 0x35,
    0x31, 0xc9, 0x39, 0x89, 0x5e, 0xab, 0x27, 0x02, 0xc3, 0xc6, 0xf4, 0x78, 0x63, 0xc3, 0x55, 0x50,
    0xdf, 0xdc, 0x6d, 0x99, 0xbf, 0x28, 0x4f, 0x34, 0x6f, 0x61, 0x01, 0x1e, 0x13, 0x95, 0xb8, 0x17,
    0x10, 0xa9, 0x27, 0x01, 0x20, 0xf4, 0xec, 0x82, 0x18, 0xb0, 0xc1, 0x03, 0xc0, 0x36, 0x93, 0x71,
    0xaa, 0x5e, 0x19, 0x73, 0xba, 0xd7, 0xde, 0x6e, 0xeb, 0x72, 0x1c, 0xac, 0x39, 0xfa, 0xd9, 0x91,
    0xaf, 0x9b, 0xde, 0x25, 0x08, 0x5c, 0xe7, 0xf3, 0xd1, 0xec, 0x14, 0x31, 0xc1, 0xb8, 0x06, 0xe6,
    0xcf, 0x4e, 0xc9, 0x9f, 0x97, 0x9b, 0x71, 0x90, 0x5c, 0x59, 0x38, 0x67, 0xd0, 0x60, 0x62, 0x22,
    0xc3, 0x42, 0xf0, 0xe1, 0x51, 0x33, 0x83, 0x11, 0x04, 0xbc, 0x78, 0x45, 0xe5, 0x3d, 0x76, 0xb2,
    0xa1, 0xd6, 0xe5, 0xf2, 0x8c, 0x54, 0x96, 0xff, 0xe8, 0x17, 0x3b, 0xde, 0x2c, 0x4f, 0xe0, 0x25,
    0x26, 0x63, 0x36, 0x33, 0xb2, 0xf0, 0x38, 0x61, 0x4e, 0x08, 0x71, 0xe8, 0xa6, 0x5d, 0xd1, 0x22,
    0x81, 0x85, 0x18, 0xee, 0x56, 0x7b, 0x3e, 0xa8, 0x39, 0xa6, 0xad, 0x55, 0xd6, 0x7d, 0x24, 0x47,
    0xe8, 0xc0, 0x7b, 0x04, 0xce, 0xd0, 0x34, 0xe3, 0x07, 0xb9, 0x37, 0xf2, 0x87, 0x89, 0x28, 0x92,
    0x65, 0xae, 0xc8, 0x93, 0xa6, 0xd6, 0x56, 0x6e, 0x5a, 0x25, 0xf1, 0xfa, 0x68, 0xf4, 0xe9, 0x17,
    0xe7, 0x21, 0xd5, 0xa6, 0x01, 0x91, 0x62, 0x11, 0x4d, 0xf0, 0x08, 0x5c, 0xdb, 0x37, 0xf4, 0x65,
    0xa7, 0x7a, 0x68, 0xab, 0xcd, 0x66, 0xed, 0x11, 0xc4, 0x84, 0x48, 0x9c, 0x06, 0x27, 0x19, 0x13,
    0x29, 0x5c, 0xb2, 0x20, 0x3f, 0x73, 0xd6, 0xac, 0x9c, 0xe4, 0x68, 0x1e, 0x8f, 0x03, 0x5a, 0x55,
    0x77, 0x86, 0xfe, 0x55, 0xd7, 0xf2, 0xad, 0xc5, 0xe1, 0x3a, 0x97, 0x94, 0x24, 0x3e, 0xd1, 0xdf,
    0x6f, 0xb3, 0x14, 0x15, 0x15, 0x11, 0xf5, 0xf5, 0xf5, 0xec, 0xe6, 0x7a, 0x3f, 0xfb, 0xbe, 0x07,
    0x23, 0x07, 0x00, 0x63, 0x4f, 0xed, 0xc5, 0x49, 0x49, 0xfc, 0xe8, 0xf4, 0xd8, 0xca, 0xb9, 0xb2,
    0x8c, 0xea, 0xf5, 0x6b, 0x16, 0x42, 0xa7, 0xd3, 0x8d, 0x29, 0xbe, 0xbe, 0x02, 0x9a, 0xa8, 0x6e,
    0xa0, 0xd5, 0x19, 0x50, 0x52, 0x82, 0x84, 0x31, 0x0c, 0x9b, 0xf1, 0xbe, 0xc1, 0x61, 0xd2, 0x68,
    0xb2, 0x1f, 0xcf, 0xca, 0xca, 0xe2, 0x76, 0x76, 0x76, 0x7a, 0xee, 0x9d, 0xda, 0x58, 0x5b, 0x0a,
    0x05, 0xe9, 0xbf, 0x97, 0x95, 0x29, 0xd1, 0x03, 0x9d, 0x4e, 0x98, 0xfa, 0xdf, 0x81, 0x91, 0xc9,
    0x64, 0x9c, 0xd6, 0x96, 0x16, 0x6f, 0x5e, 0x66, 0x4a, 0x62, 0x4e, 0x66, 0xf2, 0x9e, 0xa5, 0x25,
    0xb3, 0xd7, 0x3e, 0x37, 0x5f, 0xca, 0x33, 0x59, 0xec, 0xe0, 0xb3, 0x7f, 0xd6, 0x83, 0x1b, 0xed,
    0x3d, 0xc0, 0x6a, 0x1f, 0xf7, 0x11, 0x38, 0x46, 0xf4, 0x0e, 0x0c, 0x37, 0x8f, 0x99, 0x9d, 0xbb,
    0xd7, 0x3e, 0xfd, 0xf4, 0xd9, 0x5a, 0x8a, 0x62, 0x63, 0xe3, 0xbe, 0xbb, 0xa7, 0xaa, 0x7b, 0x30,
    0xb0, 0xb8, 0x38, 0x89, 0xd7, 0xd0, 0xd0, 0xe7, 0x82, 0x90, 0x5b, 0x52, 0xbe, 0xb6, 0xf0, 0xbd,
    0xf5, 0x2f, 0x2d, 0x2c, 0x98, 0x99, 0x12, 0x8f, 0x77, 0xa9, 0x07, 0xd0, 0xf9, 0x86, 0x56, 0xd8,
    0xde, 0xd5, 0x8b, 0x5c, 0x6e, 0x1f, 0xed, 0xf2, 0x78, 0x09, 0x9b, 0xc3, 0xf9, 0x27, 0xb5, 0xd6,
    0xb0, 0xcf, 0xed, 0x76, 0x6b, 0x1f, 0x66, 0x17, 0x7d, 0x7f, 0xc4, 0xe8, 0x91, 0xeb, 0x47, 0x76,
    0x36, 0x09, 0x7f, 0xb2, 0xee, 0x38, 0x9d, 0x96, 0x14, 0xfb, 0xea, 0xaa, 0x65, 0x05, 0xdb, 0x9e,
    0x2f, 0xc9, 0x9b, 0x11, 0x1e, 0x26, 0x04, 0x5d, 0x1a, 0x1d, 0x53, 0x77, 0x45, 0x85, 0x75, 0x6b,
    0x74, 0x8c, 0x30, 0x34, 0x04, 0x8e, 0x98, 0x6d, 0xd0, 0x66, 0xb3, 0xef, 0xe8, 0xee, 0xd1, 0x1f,
    0x04, 0x00, 0x98, 0x1f, 0xe8, 0x6b, 0xd2, 0x3b, 0xf6, 0x84, 0x23, 0x00, 0x00, 0xf0, 0x97, 0x16,
    0x49, 0xdf, 0x2e, 0x2e, 0xc8, 0xdd, 0xbc, 0x68, 0x41, 0xae, 0x84, 0xc3, 0xc1, 0x51, 0x4b, 0x9b,
    0x06, 0x5d, 0x6c, 0x54, 0x61, 0xfd, 0x83, 0x46, 0x3a, 0x32, 0x4c, 0x88, 0x0f, 0x0d, 0x9b, 0xdd,
    0x4e, 0xb7, 0x77, 0x6b, 0x97, 0x46, 0xf7, 0x57, 0x00, 0x80, 0x2b, 0x30, 0x2d, 0x0f, 0xc6, 0xc2,
    0x0f, 0x87, 0x91, 0x4a, 0xa7, 0x49, 0x16, 0xcf, 0x7b, 0x66, 0x67, 0x7a, 0x6a, 0x7c, 0x85, 0x34,
    0x33, 0x09, 0xb7, 0x3b, 0x5c, 0x34, 0xa5, 0xd2, 0x60, 0x57, 0x5b, 0x6f, 0x81, 0x51, 0x93, 0x95,
    0x09, 0x13, 0x85, 0xe0, 0x86, 0x11, 0x8b, 0x49, 0xaf, 0x37, 0x6f, 0xd7, 0x8f, 0x98, 0x3e, 0x09,
    0xb4, 0xfb, 0xc1, 0xf1, 0xf1, 0xbf, 0x44, 0xe4, 0x67, 0xe4, 0xee, 0x96, 0x66, 0x24, 0x6e, 0x92,
    0x66, 0xa6, 0x80, 0xdb, 0x7d, 0x83, 0xbe, 0xf6, 0x9b, 0xbd, 0x44, 0x8b, 0x4a, 0x83, 0x4c, 0x16,
    0x3b, 0x13, 0x11, 0x26, 0xc4, 0xcd, 0x16, 0xab, 0xb6, 0xb7, 0xdf, 0xf8, 0xf6, 0xa8, 0xc5, 0xfa,
    0x8f, 0x07, 0x32, 0xe5, 0xb1, 0x83, 0xf8, 0x61, 0x24, 0x31, 0xe2, 0x4d, 0x04, 0x07, 0xf7, 0xb5,
    0x74, 0x68, 0x30, 0xd5, 0xcd, 0x5e, 0xe2, 0x56, 0xcf, 0x00, 0xe3, 0x72, 0x79, 0xb0, 0xf0, 0x30,
    0x21, 0x3e, 0x3a, 0x66, 0xbd, 0xf9, 0xad, 0x4a, 0xfd, 0x8e, 0x97, 0x81, 0xc7, 0x65, 0x32, 0xc0,
    0xa1, 0x28, 0xe0, 0x7b, 0x52, 0x20, 0x7e, 0x18, 0xc7, 0xb8, 0x0b, 0xb4, 0xa8, 0x34, 0xb8, 0xd5,
    0x36, 0x8e, 0x06, 0x86, 0xcc, 0xb4, 0x63, 0xdc, 0x8d, 0x8b, 0x84, 0x7c, 0x60, 0x1c, 0xb5, 0x5d,
    0xbf, 0xde, 0xa6, 0xde, 0x89, 0x61, 0xf0, 0x1c, 0x42, 0x48, 0x40, 0x51, 0xc0, 0xf9, 0xa4, 0x20,
    0xee, 0xc3, 0xe8, 0x0d, 0x96, 0xeb, 0x1c, 0x0e, 0x96, 0x3f, 0x6a, 0xb6, 0x41, 0x97, 0xdb, 0x0b,
    0xa2, 0xa3, 0x44, 0x4c, 0xff, 0xc0, 0xe8, 0x65, 0x4a, 0xa5, 0xde, 0x8e, 0x63, 0x18, 0x45, 0x33,
    0x0c, 0xfb, 0xc3, 0xec, 0x89, 0x83, 0xb0, 0xc2, 0xe6, 0xcd, 0x9e, 0xb1, 0x79, 0xd4, 0x62, 0xfb,
    0x46, 0x28, 0x12, 0x0c, 0x89, 0xc3, 0x42, 0xf4, 0x1d, 0xb7, 0x74, 0x87, 0x29, 0x95, 0xba, 0x1c,
    0x42, 0xc8, 0x82, 0x70, 0x03, 0x99, 0xf3, 0x7f, 0x91, 0x3f, 0xb5, 0xe5, 0x24, 0xc9, 0x6d, 0x77,
    0xe8, 0xf3, 0x46, 0x0c, 0x96, 0xb1, 0x45, 0x54, 0xfb, 0xad, 0xc0, 0x9f, 0x38, 0x4f, 0x24, 0x63,
    0x1e, 0xa6, 0xff, 0x00, 0xc5, 0xcf, 0xed, 0x7d, 0x51, 0x00, 0x1b, 0x6b, 0x00, 0x00, 0x00, 0x00,
    0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_SEARCH_HOVER_size = 2200;

// Global variables
HMODULE hSelf;
AddonDefinition_t AddonDef{};
AddonAPI_t* APIDefs = nullptr;
bool g_WindowVisible = false;

// UI State
static char g_SearchFilter[256] = "";
// g_ApiKeyBuf removed — multi-account UI uses local buffers in AddonOptions
static bool g_ShowApiKey = false;
static std::vector<HoardAndSeek::SearchResult> g_SearchResults;
static uint32_t g_SelectedItemId = 0;
static int g_MinSearchLength = 3;
static bool g_ShowQAIcon = true;
static bool g_SearchDirty = false;
static std::chrono::steady_clock::time_point g_SearchLastKeystroke;
static std::vector<HoardAndSeek::SearchResult> g_PendingSearchResults;
static std::atomic<bool> g_SearchPending{false};
static std::atomic<bool> g_SearchResultsReady{false};

// Settings persistence
static void SaveSettings() {
    std::string dir = HoardAndSeek::GW2API::GetDataDirectory();
    std::filesystem::create_directories(dir);
    std::string path = dir + "/settings.json";
    nlohmann::json j;
    j["min_search_length"] = g_MinSearchLength;
    j["show_qa_icon"] = g_ShowQAIcon;
    j["search_filter"] = std::string(g_SearchFilter);
    std::ofstream file(path);
    if (file.is_open()) file << j.dump(2);
}

static void LoadSettings() {
    std::string path = HoardAndSeek::GW2API::GetDataDirectory() + "/settings.json";
    std::ifstream file(path);
    if (!file.is_open()) return;
    try {
        auto j = nlohmann::json::parse(file);
        if (j.contains("min_search_length")) g_MinSearchLength = j["min_search_length"].get<int>();
        if (j.contains("show_qa_icon")) g_ShowQAIcon = j["show_qa_icon"].get<bool>();
        if (j.contains("search_filter")) {
            auto s = j["search_filter"].get<std::string>();
            strncpy(g_SearchFilter, s.c_str(), sizeof(g_SearchFilter) - 1);
            g_SearchFilter[sizeof(g_SearchFilter) - 1] = '\0';
        }
    } catch (...) {}
}

// Context menu hooks registered by other addons
struct ContextMenuItem {
    uint32_t signature;
    std::string id;
    std::string requester;
    std::string label;
    std::string callback_event;
    uint8_t item_types; // HOARD_MENU_ITEMS, HOARD_MENU_WALLET, HOARD_MENU_ALL
};
static std::vector<ContextMenuItem> g_ContextMenuItems;
static std::vector<ContextMenuItem> g_PendingContextMenuItems; // Awaiting permission approval
static std::mutex g_ContextMenuMutex;

// GW2 rarity colors
static ImVec4 GetRarityColor(const std::string& rarity) {
    if (rarity == "Legendary")  return ImVec4(0.63f, 0.39f, 0.78f, 1.0f); // Purple
    if (rarity == "Ascended")   return ImVec4(0.90f, 0.39f, 0.55f, 1.0f); // Pink
    if (rarity == "Exotic")     return ImVec4(1.00f, 0.65f, 0.00f, 1.0f); // Orange
    if (rarity == "Rare")       return ImVec4(1.00f, 0.86f, 0.20f, 1.0f); // Yellow
    if (rarity == "Masterwork") return ImVec4(0.12f, 0.71f, 0.12f, 1.0f); // Green
    if (rarity == "Fine")       return ImVec4(0.35f, 0.63f, 0.90f, 1.0f); // Blue
    if (rarity == "Currency")   return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);  // Gold
    return ImVec4(0.80f, 0.80f, 0.80f, 1.0f); // Basic/Junk
}

// Location icon character (purely decorative, using unicode-safe text)
static const char* GetLocationIcon(const std::string& location) {
    if (location == "Bank") return "[Bank]";
    if (location == "Material Storage") return "[Mats]";
    if (location == "Shared Inventory") return "[Shared]";
    return "[Char]"; // Character name
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
void AddonRender();
static void BuildGW2Theme();
static void PushGW2Theme();
static void PopGW2Theme();
void AddonOptions();

// --- Render Helpers ---

// Base64 encoding table
static const char B64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = ((unsigned int)data[i]) << 16;
        if (i + 1 < len) n |= ((unsigned int)data[i + 1]) << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out += B64_TABLE[(n >> 18) & 0x3F];
        out += B64_TABLE[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? B64_TABLE[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? B64_TABLE[n & 0x3F] : '=';
    }
    return out;
}

// Generate a GW2 item chat link: [&Base64Data]
// Binary: 0x02, count(1 byte, max 250), item_id(4 bytes LE), 0x00 0x00 0x00 0x00
static std::string MakeItemChatLink(uint32_t item_id, int count) {
    unsigned char buf[10] = {0};
    buf[0] = 0x02;
    buf[1] = (unsigned char)std::min(count, 250);
    buf[2] = (unsigned char)(item_id & 0xFF);
    buf[3] = (unsigned char)((item_id >> 8) & 0xFF);
    buf[4] = (unsigned char)((item_id >> 16) & 0xFF);
    buf[5] = (unsigned char)((item_id >> 24) & 0xFF);
    // bytes 6-9 are zero (no skin/upgrade)
    return "[&" + Base64Encode(buf, 10) + "]";
}

// Copy text to Windows clipboard
static void CopyToClipboard(const std::string& text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hMem) {
        char* p = (char*)GlobalLock(hMem);
        if (p) {
            memcpy(p, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }
    CloseClipboard();
}

// Coin currency synthetic ID (currency ID 1)
static const uint32_t COIN_SYNTH_ID = HoardAndSeek::WALLET_ID_BASE | 1;

// Format copper value as gold string (gold only)
static std::string FormatCoin(int copper) {
    int gold = copper / 10000;
    return std::to_string(gold) + "g";
}

// Icon size for item icons in the results table
static const float ICON_SIZE = 20.0f;

// Helper to render an item icon with rarity border and tooltip
static void RenderItemIcon(uint32_t itemId, const std::string& iconUrl,
                           const std::string& name, const std::string& rarity,
                           const std::string& type, const std::string& description) {
    Texture_t* tex = HoardAndSeek::IconManager::GetIcon(itemId);
    if (tex && tex->Resource) {
        ImVec4 borderColor = GetRarityColor(rarity);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(pos.x - 1, pos.y - 1),
            ImVec2(pos.x + ICON_SIZE + 1, pos.y + ICON_SIZE + 1),
            ImGui::ColorConvertFloat4ToU32(borderColor), 0.0f, 0, 2.0f);
        ImGui::Image(tex->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
    } else {
        // Placeholder colored square while loading
        ImVec4 col = GetRarityColor(rarity);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE),
            ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * 0.3f, col.y * 0.3f, col.z * 0.3f, 0.5f)));
        ImGui::Dummy(ImVec2(ICON_SIZE, ICON_SIZE));

        // Request icon download
        if (!iconUrl.empty()) {
            HoardAndSeek::IconManager::RequestIcon(itemId, iconUrl);
        }
    }

    // Tooltip on hover
    if (ImGui::IsItemHovered()) {
        // Read latest description from cache (may have been fetched on-demand)
        std::string liveDesc = description;
        if (liveDesc.empty()) {
            const auto* info = HoardAndSeek::GW2API::GetItemInfo(itemId);
            if (info) liveDesc = info->description;
        }

        ImGui::BeginTooltip();
        // Show larger icon in tooltip if available
        if (tex && tex->Resource) {
            ImGui::Image(tex->Resource, ImVec2(48, 48));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextColored(GetRarityColor(rarity), "%s", name.c_str());
        if (!rarity.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", rarity.c_str());
        }
        if (!liveDesc.empty()) {
            ImGui::PushTextWrapPos(300.0f);
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", liveDesc.c_str());
            ImGui::PopTextWrapPos();
        } else if (itemId < HoardAndSeek::WALLET_ID_BASE) {
            // Trigger on-demand fetch for missing tooltip
            HoardAndSeek::GW2API::FetchTooltipAsync(itemId);
        }
        ImGui::EndGroup();
        ImGui::EndTooltip();
    }
}

// Location color helper
static ImVec4 GetLocationColor(const std::string& location) {
    if (location == "Bank")              return ImVec4(0.3f, 0.6f, 0.9f, 1.0f);
    if (location == "Material Storage")  return ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
    if (location == "Shared Inventory")  return ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    if (location == "Wallet")            return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
    if (location == "Legendary Armory")  return ImVec4(0.6f, 0.4f, 0.9f, 1.0f);
    return ImVec4(0.8f, 0.5f, 0.8f, 1.0f); // Character
}

static void RenderResultsList() {
    if (g_SearchResults.empty()) {
        if (strlen(g_SearchFilter) > 0 && (int)strlen(g_SearchFilter) >= g_MinSearchLength) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No items found.");
        }
        return;
    }

    // Results table with expandable tree rows
    if (ImGui::BeginTable("##results", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
        ImVec2(0, ImGui::GetContentRegionAvail().y - 4)))
    {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 4.0f);
        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Pre-compute context menu hook availability once (not per row)
        bool has_item_hooks = false;
        bool has_wallet_hooks = false;
        {
            std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
            for (const auto& mi : g_ContextMenuItems) {
                if (mi.item_types & HOARD_MENU_ITEMS) has_item_hooks = true;
                if (mi.item_types & HOARD_MENU_WALLET) has_wallet_hooks = true;
                if (has_item_hooks && has_wallet_hooks) break;
            }
        }

        int max_display = 200;
        int count = 0;
        for (const auto& result : g_SearchResults) {
            if (count++ >= max_display) break;

            ImGui::TableNextRow(0, ICON_SIZE + 4);
            ImGui::TableNextColumn();
            ImGui::PushID((int)result.item_id);

            // Render icon inline, then tree node on same line
            RenderItemIcon(result.item_id, result.icon_url, result.name, result.rarity, result.type, result.description);
            ImGui::SameLine();

            ImVec4 nameColor = GetRarityColor(result.rarity);
            ImGui::PushStyleColor(ImGuiCol_Text, nameColor);
            bool open = ImGui::TreeNodeEx(result.name.c_str(),
                ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                ImGuiTreeNodeFlags_OpenOnArrow);
            ImGui::PopStyleColor();

            // Right-click context menu
            {
                bool is_item = result.item_id < HoardAndSeek::WALLET_ID_BASE;
                uint8_t type_flag = is_item ? HOARD_MENU_ITEMS : HOARD_MENU_WALLET;
                bool has_hooks = is_item ? has_item_hooks : has_wallet_hooks;

                if (is_item || has_hooks) {
                    if (ImGui::BeginPopupContextItem("##ctx_menu")) {
                        if (is_item) {
                            if (ImGui::MenuItem("Copy Chat Link")) {
                                std::string link = MakeItemChatLink(result.item_id, result.total_count);
                                CopyToClipboard(link);
                            }
                        }

                        // Registered context menu items from other addons
                        std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
                        bool need_separator = is_item && !g_ContextMenuItems.empty();
                        for (const auto& mi : g_ContextMenuItems) {
                            if (!(mi.item_types & type_flag)) continue;
                            if (need_separator) { ImGui::Separator(); need_separator = false; }
                            if (ImGui::MenuItem(mi.label.c_str())) {
                                auto* cb = new HoardContextMenuCallback{};
                                cb->api_version = HOARD_API_VERSION;
                                cb->item_id = result.item_id;
                                strncpy(cb->name, result.name.c_str(), sizeof(cb->name) - 1);
                                cb->name[sizeof(cb->name) - 1] = '\0';
                                strncpy(cb->rarity, result.rarity.c_str(), sizeof(cb->rarity) - 1);
                                cb->rarity[sizeof(cb->rarity) - 1] = '\0';
                                strncpy(cb->type, result.type.c_str(), sizeof(cb->type) - 1);
                                cb->type[sizeof(cb->type) - 1] = '\0';
                                cb->total_count = result.total_count;
                                if (APIDefs) APIDefs->Events_Raise(mi.callback_event.c_str(), cb);
                            }
                        }

                        ImGui::EndPopup();
                    }
                }
            }

            // Total count column
            ImGui::TableNextColumn();
            if (result.item_id == COIN_SYNTH_ID) {
                ImGui::Text("%s", FormatCoin(result.total_count).c_str());
            } else {
                ImGui::Text("%d", result.total_count);
            }

            // Expanded: show each location as its own row
            // Group by account when multiple accounts have locations for this item
            if (open) {
                // Check if multiple accounts are present in results
                std::unordered_set<std::string> acct_set;
                for (const auto& loc : result.locations) acct_set.insert(loc.account);
                size_t numAccounts = HoardAndSeek::GW2API::GetAccounts().size();
                bool showAccountHeaders = (numAccounts > 1);

                if (showAccountHeaders) {
                    // Group by account
                    for (const auto& acct_name : acct_set) {
                        // Find label
                        std::string acctDisplay = acct_name;
                        const auto& accts = HoardAndSeek::GW2API::GetAccounts();
                        for (const auto& a : accts) {
                            if (a.account_name == acct_name && !a.label.empty()) {
                                acctDisplay = a.label;
                                break;
                            }
                        }

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TreePush("acct");
                        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "%s", acctDisplay.c_str());
                        ImGui::TreePop();
                        ImGui::TableNextColumn();
                        // Show subtotal on account line only if this account has multiple locations
                        int acctLocCount = 0;
                        int acctTotal = 0;
                        for (const auto& loc : result.locations) {
                            if (loc.account == acct_name) { acctLocCount++; acctTotal += loc.count; }
                        }
                        if (acctLocCount > 1) {
                            if (result.item_id == COIN_SYNTH_ID) {
                                ImGui::Text("%s", FormatCoin(acctTotal).c_str());
                            } else {
                                ImGui::Text("%d", acctTotal);
                            }
                        }

                        for (const auto& loc : result.locations) {
                            if (loc.account != acct_name) continue;
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TreePush("acct");
                            ImGui::TreePush("loc");
                            ImVec4 locColor = GetLocationColor(loc.location);
                            std::string locLabel = loc.sublocation.empty() ? loc.location : loc.location + " (" + loc.sublocation + ")";
                            ImGui::TextColored(locColor, "%s", locLabel.c_str());
                            ImGui::TreePop();
                            ImGui::TreePop();
                            ImGui::TableNextColumn();
                            if (result.item_id == COIN_SYNTH_ID) {
                                ImGui::Text("%s", FormatCoin(loc.count).c_str());
                            } else {
                                ImGui::Text("x%d", loc.count);
                            }
                        }
                    }
                } else {
                    // Single account or filtered: flat list
                    for (const auto& loc : result.locations) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TreePush("loc");
                        ImVec4 locColor = GetLocationColor(loc.location);
                        std::string locLabel = loc.sublocation.empty() ? loc.location : loc.location + " (" + loc.sublocation + ")";
                        ImGui::TextColored(locColor, "%s", locLabel.c_str());
                        ImGui::TreePop();
                        ImGui::TableNextColumn();
                        if (result.item_id == COIN_SYNTH_ID) {
                            ImGui::Text("%s", FormatCoin(loc.count).c_str());
                        } else {
                            ImGui::Text("x%d", loc.count);
                        }
                    }
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (count >= max_display && (int)g_SearchResults.size() > max_display) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "... and %d more (refine your search)",
                (int)g_SearchResults.size() - max_display);
        }

        ImGui::EndTable();
    }
}

// --- Cross-addon events ---

void OnSearchRequest(void* eventArgs) {
    if (!eventArgs) return;
    const char* name = (const char*)eventArgs;
    if (!name || name[0] == '\0') return;

    // Copy search term into filter buffer
    strncpy(g_SearchFilter, name, sizeof(g_SearchFilter) - 1);
    g_SearchFilter[sizeof(g_SearchFilter) - 1] = '\0';

    // Run the search
    std::string query(g_SearchFilter);
    if ((int)query.length() >= g_MinSearchLength) {
        g_SearchResults = HoardAndSeek::GW2API::SearchItems(query);
    }

    // Show the window
    g_WindowVisible = true;

    if (APIDefs) {
        std::string msg = "Search requested: " + std::string(name);
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", msg.c_str());
    }
}

// Helper: check permission for a requester/event pair.
// Returns HOARD_STATUS_OK if allowed, HOARD_STATUS_DENIED if denied,
// HOARD_STATUS_PENDING if not yet decided (popup queued).
static uint8_t CheckAddonPermission(const char* requester, const char* event_name) {
    if (!requester || requester[0] == '\0') return HOARD_STATUS_DENIED;
    std::string req_str(requester);
    std::string ev_str(event_name);
    auto state = HoardAndSeek::PermissionManager::Check(req_str, ev_str);
    if (state == HoardAndSeek::PermissionState::Allowed) return HOARD_STATUS_OK;
    if (state == HoardAndSeek::PermissionState::Unknown) {
        HoardAndSeek::PermissionManager::RequestPermission(req_str, ev_str);
        return HOARD_STATUS_PENDING;
    }
    return HOARD_STATUS_DENIED;
}

void OnQueryItem(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryItemRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0') return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_ITEM);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryItemResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    // v3+: read account filter
    std::string account_filter;
    if (req->api_version >= 3 && req->account_filter[0] != '\0') {
        account_filter = std::string(req->account_filter);
    }

    auto* resp = new HoardQueryItemResponse{};
    resp->api_version = HOARD_API_VERSION;
    resp->status = HOARD_STATUS_OK;
    resp->item_id = req->item_id;
    resp->total_count = 0;
    resp->location_count = 0;

    // Fill item info
    const auto* info = HoardAndSeek::GW2API::GetItemInfo(req->item_id);
    if (info) {
        strncpy(resp->name, info->name.c_str(), sizeof(resp->name) - 1);
        strncpy(resp->rarity, info->rarity.c_str(), sizeof(resp->rarity) - 1);
        strncpy(resp->type, info->type.c_str(), sizeof(resp->type) - 1);

    }

    // Fill locations
    auto locations = HoardAndSeek::GW2API::GetItemLocations(req->item_id, account_filter);
    for (const auto& loc : locations) {
        resp->total_count += loc.count;
        if (resp->location_count < 64) {
            auto& entry = resp->locations[resp->location_count];
            strncpy(entry.location, loc.location.c_str(), sizeof(entry.location) - 1);
            strncpy(entry.sublocation, loc.sublocation.c_str(), sizeof(entry.sublocation) - 1);
            entry.count = loc.count;
            strncpy(entry.account_name, loc.account.c_str(), sizeof(entry.account_name) - 1);
            resp->location_count++;
        }
    }

    APIDefs->Events_Raise(req->response_event, resp);
    // Caller is responsible for freeing resp via delete
}

void OnQueryWallet(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryWalletRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0') return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_WALLET);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryWalletResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    // v3+: read account filter
    std::string account_filter;
    if (req->api_version >= 3 && req->account_filter[0] != '\0') {
        account_filter = std::string(req->account_filter);
    }

    auto* resp = new HoardQueryWalletResponse{};
    resp->api_version = HOARD_API_VERSION;
    resp->status = HOARD_STATUS_OK;
    resp->currency_id = req->currency_id;
    resp->found = false;
    resp->amount = 0;

    // Look up using synthetic wallet ID
    uint32_t synth_id = HoardAndSeek::WALLET_ID_BASE | req->currency_id;
    const auto* info = HoardAndSeek::GW2API::GetItemInfo(synth_id);
    if (info) {
        strncpy(resp->name, info->name.c_str(), sizeof(resp->name) - 1);
        resp->found = true;
    }

    resp->amount = HoardAndSeek::GW2API::GetTotalCount(synth_id, account_filter);

    APIDefs->Events_Raise(req->response_event, resp);
    // Caller is responsible for freeing resp via delete
}

void OnQueryAchievement(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryAchievementRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0' || req->id_count == 0) return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_ACHIEVEMENT);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryAchievementResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        strncpy(resp->account_name, req->account_name, sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    std::vector<uint32_t> ids(req->ids, req->ids + std::min(req->id_count, (uint32_t)200));
    std::string response_event(req->response_event);
    std::string account_name(req->account_name);
    std::string requester(req->requester);

    // Build comma-separated ID list for URL and cache key
    std::string ids_param;
    for (size_t i = 0; i < ids.size(); i++) {
        if (i > 0) ids_param += ",";
        ids_param += std::to_string(ids[i]);
    }
    std::string cache_endpoint = "/v2/account/achievements?ids=" + ids_param;

    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, cache_endpoint, cached)) {
        auto* resp = new HoardQueryAchievementResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        resp->entry_count = 0;
        try {
            auto j = nlohmann::json::parse(cached);
            if (j.is_array()) {
                for (const auto& entry : j) {
                    if (resp->entry_count >= 200) break;
                    auto& e = resp->entries[resp->entry_count];
                    e.id = entry.value("id", (uint32_t)0);
                    e.current = entry.value("current", -1);
                    e.max = entry.value("max", -1);
                    e.done = entry.value("done", false);
                    e.bit_count = 0;
                    if (entry.contains("bits") && entry["bits"].is_array()) {
                        for (const auto& bit : entry["bits"]) {
                            if (e.bit_count >= 64) break;
                            e.bits[e.bit_count++] = bit.get<uint32_t>();
                        }
                    }
                    resp->entry_count++;
                }
            }
        } catch (...) {}
        APIDefs->Events_Raise(response_event.c_str(), resp);
        return;
    }

    auto fetch = [ids_param, account_name]() -> std::string {
        std::string url = "https://api.guildwars2.com/v2/account/achievements?ids=" + ids_param;
        return HoardAndSeek::GW2API::AuthenticatedGet(url, account_name);
    };
    auto complete = [=](const std::string& body) {
        auto* resp = new HoardQueryAchievementResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        resp->entry_count = 0;
        if (!body.empty()) {
            try {
                auto j = nlohmann::json::parse(body);
                if (j.is_array()) {
                    for (const auto& entry : j) {
                        if (resp->entry_count >= 200) break;
                        auto& e = resp->entries[resp->entry_count];
                        e.id = entry.value("id", (uint32_t)0);
                        e.current = entry.value("current", -1);
                        e.max = entry.value("max", -1);
                        e.done = entry.value("done", false);
                        e.bit_count = 0;
                        if (entry.contains("bits") && entry["bits"].is_array()) {
                            for (const auto& bit : entry["bits"]) {
                                if (e.bit_count >= 64) break;
                                e.bits[e.bit_count++] = bit.get<uint32_t>();
                            }
                        }
                        resp->entry_count++;
                    }
                }
            } catch (...) {}
        }
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, cache_endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        auto* resp = new HoardQueryAchievementResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_BUSY;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(response_event.c_str(), resp);
    }
}

void OnQueryMastery(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryMasteryRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0' || req->id_count == 0) return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_MASTERY);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryMasteryResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        strncpy(resp->account_name, req->account_name, sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    std::vector<uint32_t> ids(req->ids, req->ids + std::min(req->id_count, (uint32_t)200));
    std::string response_event(req->response_event);
    std::string account_name(req->account_name);
    std::string requester(req->requester);
    const std::string cache_endpoint = "/v2/account/masteries";

    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, cache_endpoint, cached)) {
        auto* resp = new HoardQueryMasteryResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        resp->entry_count = 0;
        try {
            auto j = nlohmann::json::parse(cached);
            if (j.is_array()) {
                std::unordered_set<uint32_t> wanted(ids.begin(), ids.end());
                for (const auto& entry : j) {
                    if (resp->entry_count >= 200) break;
                    uint32_t id = entry.value("id", (uint32_t)0);
                    if (wanted.count(id)) {
                        auto& e = resp->entries[resp->entry_count];
                        e.id = id;
                        e.level = entry.value("level", 0);
                        resp->entry_count++;
                    }
                }
            }
        } catch (...) {}
        APIDefs->Events_Raise(response_event.c_str(), resp);
        return;
    }

    auto fetch = [account_name]() -> std::string {
        return HoardAndSeek::GW2API::AuthenticatedGet(
            "https://api.guildwars2.com/v2/account/masteries", account_name);
    };
    auto complete = [=](const std::string& body) {
        auto* resp = new HoardQueryMasteryResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        resp->entry_count = 0;
        if (!body.empty()) {
            try {
                auto j = nlohmann::json::parse(body);
                if (j.is_array()) {
                    std::unordered_set<uint32_t> wanted(ids.begin(), ids.end());
                    for (const auto& entry : j) {
                        if (resp->entry_count >= 200) break;
                        uint32_t id = entry.value("id", (uint32_t)0);
                        if (wanted.count(id)) {
                            auto& e = resp->entries[resp->entry_count];
                            e.id = id;
                            e.level = entry.value("level", 0);
                            resp->entry_count++;
                        }
                    }
                }
            } catch (...) {}
        }
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, cache_endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        auto* resp = new HoardQueryMasteryResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_BUSY;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(response_event.c_str(), resp);
    }
}

void OnQuerySkins(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQuerySkinsRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0' || req->id_count == 0) return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_SKINS);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQuerySkinsResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        strncpy(resp->account_name, req->account_name, sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    std::vector<uint32_t> ids(req->ids, req->ids + std::min(req->id_count, (uint32_t)200));
    std::string response_event(req->response_event);
    std::string account_name(req->account_name);
    std::string requester(req->requester);
    const std::string cache_endpoint = "/v2/account/skins";

    auto make_resp = [](const std::string& body, const std::vector<uint32_t>& id_list,
                        const std::string& acct) -> HoardQuerySkinsResponse* {
        auto* resp = new HoardQuerySkinsResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, acct.c_str(), sizeof(resp->account_name) - 1);
        resp->entry_count = 0;
        std::unordered_set<uint32_t> unlocked;
        if (!body.empty()) {
            try {
                auto j = nlohmann::json::parse(body);
                if (j.is_array()) {
                    for (const auto& id : j) unlocked.insert(id.get<uint32_t>());
                }
            } catch (...) {}
        }
        for (size_t i = 0; i < id_list.size() && resp->entry_count < 200; i++) {
            auto& e = resp->entries[resp->entry_count];
            e.id = id_list[i];
            e.unlocked = unlocked.count(id_list[i]) ? 1 : 0;
            resp->entry_count++;
        }
        return resp;
    };

    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, cache_endpoint, cached)) {
        auto* resp = make_resp(cached, ids, account_name);
        APIDefs->Events_Raise(response_event.c_str(), resp);
        return;
    }

    auto fetch = [account_name]() -> std::string {
        return HoardAndSeek::GW2API::AuthenticatedGet(
            "https://api.guildwars2.com/v2/account/skins", account_name);
    };
    auto complete = [=](const std::string& body) {
        auto* resp = make_resp(body, ids, account_name);
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, cache_endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        auto* resp = new HoardQuerySkinsResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_BUSY;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(response_event.c_str(), resp);
    }
}

void OnQueryRecipes(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryRecipesRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0' || req->id_count == 0) return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_RECIPES);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryRecipesResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        strncpy(resp->account_name, req->account_name, sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    std::vector<uint32_t> ids(req->ids, req->ids + std::min(req->id_count, (uint32_t)200));
    std::string response_event(req->response_event);
    std::string account_name(req->account_name);
    std::string requester(req->requester);
    const std::string cache_endpoint = "/v2/account/recipes";

    auto make_resp = [](const std::string& body, const std::vector<uint32_t>& id_list,
                        const std::string& acct) -> HoardQueryRecipesResponse* {
        auto* resp = new HoardQueryRecipesResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, acct.c_str(), sizeof(resp->account_name) - 1);
        resp->entry_count = 0;
        std::unordered_set<uint32_t> unlocked;
        if (!body.empty()) {
            try {
                auto j = nlohmann::json::parse(body);
                if (j.is_array()) {
                    for (const auto& id : j) unlocked.insert(id.get<uint32_t>());
                }
            } catch (...) {}
        }
        for (size_t i = 0; i < id_list.size() && resp->entry_count < 200; i++) {
            auto& e = resp->entries[resp->entry_count];
            e.id = id_list[i];
            e.unlocked = unlocked.count(id_list[i]) ? 1 : 0;
            resp->entry_count++;
        }
        return resp;
    };

    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, cache_endpoint, cached)) {
        auto* resp = make_resp(cached, ids, account_name);
        APIDefs->Events_Raise(response_event.c_str(), resp);
        return;
    }

    auto fetch = [account_name]() -> std::string {
        return HoardAndSeek::GW2API::AuthenticatedGet(
            "https://api.guildwars2.com/v2/account/recipes", account_name);
    };
    auto complete = [=](const std::string& body) {
        auto* resp = make_resp(body, ids, account_name);
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, cache_endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        auto* resp = new HoardQueryRecipesResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_BUSY;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(response_event.c_str(), resp);
    }
}

void OnQueryWizardsVault(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryWizardsVaultRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0') return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_WIZARDSVAULT);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryWizardsVaultResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        strncpy(resp->account_name, req->account_name, sizeof(resp->account_name) - 1);
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    uint8_t type = req->type;
    std::string response_event(req->response_event);
    std::string account_name(req->account_name);
    std::string requester(req->requester);

    // Validate type up front
    if (type > 2) {
        auto* resp = new HoardQueryWizardsVaultResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        resp->type = type;
        resp->objective_count = 0;
        APIDefs->Events_Raise(response_event.c_str(), resp);
        return;
    }

    const char* type_strings[] = { "daily", "weekly", "special" };
    std::string type_string = type_strings[type];
    std::string cache_endpoint = "/v2/account/wizardsvault/" + type_string;
    std::string url = "https://api.guildwars2.com" + cache_endpoint;

    auto parse_body = [](const std::string& body, uint8_t t,
                         const std::string& acct) -> HoardQueryWizardsVaultResponse* {
        auto* resp = new HoardQueryWizardsVaultResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_OK;
        strncpy(resp->account_name, acct.c_str(), sizeof(resp->account_name) - 1);
        resp->type = t;
        resp->objective_count = 0;
        if (!body.empty()) {
            try {
                auto j = nlohmann::json::parse(body);
                resp->meta_progress_current = j.value("meta_progress_current", 0);
                resp->meta_progress_complete = j.value("meta_progress_complete", 0);
                resp->meta_reward_astral = j.value("meta_reward_astral", 0);
                resp->meta_reward_claimed = j.value("meta_reward_claimed", false) ? 1 : 0;
                if (j.contains("objectives") && j["objectives"].is_array()) {
                    for (const auto& obj : j["objectives"]) {
                        if (resp->objective_count >= 16) break;
                        auto& o = resp->objectives[resp->objective_count];
                        o.id = obj.value("id", (uint32_t)0);
                        std::string title = obj.value("title", "");
                        strncpy(o.title, title.c_str(), sizeof(o.title) - 1);
                        o.title[sizeof(o.title) - 1] = '\0';
                        std::string track = obj.value("track", "");
                        strncpy(o.track, track.c_str(), sizeof(o.track) - 1);
                        o.track[sizeof(o.track) - 1] = '\0';
                        o.acclaim = obj.value("acclaim", 0);
                        o.progress_current = obj.value("progress_current", 0);
                        o.progress_complete = obj.value("progress_complete", 0);
                        o.claimed = obj.value("claimed", false) ? 1 : 0;
                        resp->objective_count++;
                    }
                }
            } catch (...) {}
        }
        return resp;
    };

    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, cache_endpoint, cached)) {
        auto* resp = parse_body(cached, type, account_name);
        APIDefs->Events_Raise(response_event.c_str(), resp);
        return;
    }

    auto fetch = [url, account_name]() -> std::string {
        return HoardAndSeek::GW2API::AuthenticatedGet(url, account_name);
    };
    auto complete = [=](const std::string& body) {
        auto* resp = parse_body(body, type, account_name);
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, cache_endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        auto* resp = new HoardQueryWizardsVaultResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = HOARD_STATUS_BUSY;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        resp->type = type;
        APIDefs->Events_Raise(response_event.c_str(), resp);
    }
}

void OnQueryApi(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryApiRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0' || req->endpoint[0] == '\0') return;

    const bool wants_v4 = (req->api_version >= 4);
    const std::string requester(req->requester);
    const std::string endpoint(req->endpoint);
    const std::string response_event(req->response_event);
    std::string account_name;
    if (req->api_version >= 3 && req->account_name[0] != '\0') {
        account_name = std::string(req->account_name);
    }

    auto build_resp = [=](uint8_t status, const std::string& body,
                          uint32_t retry_after_ms, uint32_t qdepth) {
        auto* resp = new HoardQueryApiResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = status;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        strncpy(resp->endpoint, endpoint.c_str(), sizeof(resp->endpoint) - 1);
        resp->endpoint[sizeof(resp->endpoint) - 1] = '\0';
        resp->json_length = (uint32_t)body.length();
        if (body.length() >= sizeof(resp->json)) {
            resp->truncated = 1;
            memcpy(resp->json, body.c_str(), sizeof(resp->json) - 1);
            resp->json[sizeof(resp->json) - 1] = '\0';
        } else {
            resp->truncated = 0;
            memcpy(resp->json, body.c_str(), body.length());
            resp->json[body.length()] = '\0';
        }
        if (wants_v4) {
            resp->retry_after_ms = retry_after_ms;
            resp->queue_depth    = qdepth;
            resp->tokens_remaining = 0;
        }
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_API);
    if (perm != HOARD_STATUS_OK) {
        build_resp(perm, "", 0, 0);
        return;
    }

    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, endpoint, cached)) {
        build_resp(HOARD_STATUS_OK, cached, 0,
                   HoardAndSeek::ProxyThrottle::CurrentQueueDepth());
        return;
    }

    auto fetch = [endpoint, account_name]() -> std::string {
        std::string url = "https://api.guildwars2.com" + endpoint;
        return HoardAndSeek::GW2API::AuthenticatedGet(url, account_name);
    };
    auto complete = [build_resp](const std::string& body) {
        build_resp(HOARD_STATUS_OK, body, 0,
                   HoardAndSeek::ProxyThrottle::CurrentQueueDepth());
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        uint32_t retry_ms = (uint32_t)(
            (double)sr.queue_depth_after / HoardAndSeek::ProxyThrottle::RATE_REFILL_PER_SEC_MAX * 1000.0);
        if (retry_ms < 500) retry_ms = 500;
        build_resp(HOARD_STATUS_BUSY, "", retry_ms, sr.queue_depth_after);
    }
}

void OnContextMenuRegister(void* eventArgs) {
    if (!eventArgs) return;
    auto* reg = (HoardContextMenuRegister*)eventArgs;
    if (reg->api_version > HOARD_API_VERSION || reg->api_version == 0) return;
    if (reg->id[0] == '\0' || reg->requester[0] == '\0' || reg->label[0] == '\0' || reg->callback_event[0] == '\0') return;
    ContextMenuItem item;
    item.signature = reg->signature;
    item.id = reg->id;
    item.requester = reg->requester;
    item.label = reg->label;
    item.callback_event = reg->callback_event;
    item.item_types = reg->item_types ? reg->item_types : HOARD_MENU_ALL;

    uint8_t perm = CheckAddonPermission(reg->requester, EV_HOARD_CONTEXT_MENU_REGISTER);
    if (perm == HOARD_STATUS_PENDING) {
        // Queue for later — will be applied when permission is granted
        std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
        for (auto& existing : g_PendingContextMenuItems) {
            if (existing.id == item.id && existing.requester == item.requester) {
                existing = item;
                return;
            }
        }
        g_PendingContextMenuItems.push_back(item);
        return;
    }
    if (perm != HOARD_STATUS_OK) return;

    std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
    // Replace if same id+requester already exists
    for (auto& existing : g_ContextMenuItems) {
        if (existing.id == item.id && existing.requester == item.requester) {
            existing = item;
            return;
        }
    }
    g_ContextMenuItems.push_back(item);

    if (APIDefs) {
        std::string msg = "Context menu registered: \"" + item.label + "\" by " + item.requester;
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", msg.c_str());
    }
}

void OnContextMenuRemove(void* eventArgs) {
    if (!eventArgs) return;
    auto* rem = (HoardContextMenuRemove*)eventArgs;
    if (rem->api_version != HOARD_API_VERSION) return;
    if (rem->requester[0] == '\0') return;

    std::string requester(rem->requester);
    std::string id(rem->id);

    std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
    g_ContextMenuItems.erase(
        std::remove_if(g_ContextMenuItems.begin(), g_ContextMenuItems.end(),
            [&](const ContextMenuItem& item) {
                if (item.requester != requester) return false;
                return id.empty() || item.id == id;
            }),
        g_ContextMenuItems.end());
}

void OnAddonUnloaded(void* eventArgs) {
    if (!eventArgs) return;
    uint32_t signature = *(uint32_t*)eventArgs;
    if (signature == 0) return;

    std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
    auto before = g_ContextMenuItems.size();
    g_ContextMenuItems.erase(
        std::remove_if(g_ContextMenuItems.begin(), g_ContextMenuItems.end(),
            [signature](const ContextMenuItem& item) {
                return item.signature == signature;
            }),
        g_ContextMenuItems.end());

    if (g_ContextMenuItems.size() < before && APIDefs) {
        std::string msg = "Removed context menu items for unloaded addon (signature " + std::to_string(signature) + ")";
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", msg.c_str());
    }
}

void OnPing(void* eventArgs) {
    if (!APIDefs) return;
    static HoardPongPayload pong{};
    pong.api_version = HOARD_API_VERSION;
    pong.last_updated = (int64_t)HoardAndSeek::GW2API::GetLastUpdated();
    pong.refresh_available_at = (int64_t)HoardAndSeek::GW2API::GetRefreshAvailableAt();
    pong.has_data = HoardAndSeek::GW2API::HasAccountData() ? 1 : 0;
    pong.account_count = (uint32_t)HoardAndSeek::GW2API::GetAccountCount();
    APIDefs->Events_Raise(EV_HOARD_PONG, &pong);
}

void OnRefreshRequest(void* eventArgs) {
    if (!APIDefs) return;
    auto status = HoardAndSeek::GW2API::GetFetchStatus();
    if (status == HoardAndSeek::FetchStatus::InProgress) return;
    if (HoardAndSeek::GW2API::IsRefreshOnCooldown()) {
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Refresh request ignored (cooldown)");
        return;
    }
    HoardAndSeek::GW2API::FetchAccountDataAsync();
    APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Refresh triggered by external addon");
}

static void BroadcastDataUpdated() {
    if (!APIDefs) return;
    static HoardDataReadyPayload payload{};
    payload.api_version = HOARD_API_VERSION;
    // Count unique items and currencies from search
    // This is a lightweight broadcast, exact counts are informational
    payload.item_count = 0;
    payload.currency_count = 0;
    payload.last_updated = (int64_t)HoardAndSeek::GW2API::GetLastUpdated();
    payload.refresh_available_at = (int64_t)HoardAndSeek::GW2API::GetRefreshAvailableAt();
    payload.account_name[0] = '\0'; // all accounts
    payload.account_count = (uint32_t)HoardAndSeek::GW2API::GetAccountCount();
    APIDefs->Events_Raise(EV_HOARD_DATA_UPDATED, &payload);
}

void OnQueryAccounts(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryAccountsRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0') return;
    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_ACCOUNTS);
    if (perm != HOARD_STATUS_OK) {
        auto* resp = new HoardQueryAccountsResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = perm;
        APIDefs->Events_Raise(req->response_event, resp);
        return;
    }

    auto* resp = new HoardQueryAccountsResponse{};
    resp->api_version = HOARD_API_VERSION;
    resp->status = HOARD_STATUS_OK;
    resp->account_count = 0;

    const auto& accounts = HoardAndSeek::GW2API::GetAccounts();
    for (size_t i = 0; i < accounts.size() && i < 16; i++) {
        auto& entry = resp->accounts[resp->account_count];
        strncpy(entry.account_name, accounts[i].account_name.c_str(), sizeof(entry.account_name) - 1);
        strncpy(entry.label, accounts[i].label.c_str(), sizeof(entry.label) - 1);
        entry.last_updated = (int64_t)accounts[i].last_updated;
        entry.validated = accounts[i].validated ? 1 : 0;
        entry.character_count = 0;
        for (size_t c = 0; c < accounts[i].characters.size() && c < 80; c++) {
            strncpy(entry.characters[entry.character_count], accounts[i].characters[c].c_str(),
                    sizeof(entry.characters[0]) - 1);
            entry.characters[entry.character_count][sizeof(entry.characters[0]) - 1] = '\0';
            entry.character_count++;
        }
        resp->account_count++;
    }

    APIDefs->Events_Raise(req->response_event, resp);
}

// --- Addon Lifecycle ---

void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))APIDefs->ImguiMalloc,
                                 (void(*)(void*, void*))APIDefs->ImguiFree);

    // Build GW2 theme (must be after ImGui context is set)
    BuildGW2Theme();

    // Initialize icon manager
    HoardAndSeek::IconManager::Initialize(APIDefs);

    // Load saved accounts and account data
    if (HoardAndSeek::GW2API::LoadAccounts()) {
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Accounts loaded from config");
        HoardAndSeek::GW2API::ValidateAllAccountsAsync();
        HoardAndSeek::GW2API::FetchMissingCharacterListsAsync();
    }
    HoardAndSeek::GW2API::LoadAccountData();
    HoardAndSeek::GW2API::LoadTooltips();
    HoardAndSeek::PermissionManager::Init();

    // Register render functions
    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    // Register keybind
    APIDefs->InputBinds_RegisterWithString("KB_HOARD_TOGGLE", ProcessKeybind, "CTRL+SHIFT+F");

    // Load icon textures from embedded PNG data
    APIDefs->Textures_LoadFromMemory(TEX_ICON, (void*)ICON_SEARCH, ICON_SEARCH_size, nullptr);
    APIDefs->Textures_LoadFromMemory(TEX_ICON_HOVER, (void*)ICON_SEARCH_HOVER, ICON_SEARCH_HOVER_size, nullptr);

    // Load settings (restores search filter among other things)
    LoadSettings();
    if ((int)strlen(g_SearchFilter) >= g_MinSearchLength) {
        g_SearchDirty = true;
        g_SearchLastKeystroke = std::chrono::steady_clock::now() - std::chrono::milliseconds(200);
    }

    // Register quick access shortcut
    if (g_ShowQAIcon) {
        APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON_HOVER, "KB_HOARD_TOGGLE", "Hoard & Seek");
    }

    // Register close-on-escape
    APIDefs->GUI_RegisterCloseOnEscape("Hoard & Seek", &g_WindowVisible);

    // Subscribe to cross-addon events
    APIDefs->Events_Subscribe(EV_HOARD_SEARCH, OnSearchRequest);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_ITEM, OnQueryItem);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_WALLET, OnQueryWallet);
    APIDefs->Events_Subscribe(EV_HOARD_REFRESH, OnRefreshRequest);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_ACHIEVEMENT, OnQueryAchievement);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_MASTERY, OnQueryMastery);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_SKINS, OnQuerySkins);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_RECIPES, OnQueryRecipes);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_WIZARDSVAULT, OnQueryWizardsVault);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_API, OnQueryApi);
    APIDefs->Events_Subscribe(EV_HOARD_CONTEXT_MENU_REGISTER, OnContextMenuRegister);
    APIDefs->Events_Subscribe(EV_HOARD_CONTEXT_MENU_REMOVE, OnContextMenuRemove);
    APIDefs->Events_Subscribe(EV_ADDON_UNLOADED, OnAddonUnloaded);
    APIDefs->Events_Subscribe(EV_HOARD_PING, OnPing);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_ACCOUNTS, OnQueryAccounts);

    HoardAndSeek::ProxyThrottle::Start();

    // Notify other addons that cached data is available
    if (HoardAndSeek::GW2API::HasAccountData()) {
        BroadcastDataUpdated();
    }

    APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Addon loaded successfully");
}

void AddonUnload() {
    HoardAndSeek::ProxyThrottle::Stop();
    HoardAndSeek::IconManager::Shutdown();

    APIDefs->Events_Unsubscribe(EV_HOARD_SEARCH, OnSearchRequest);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_ITEM, OnQueryItem);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_WALLET, OnQueryWallet);
    APIDefs->Events_Unsubscribe(EV_HOARD_REFRESH, OnRefreshRequest);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_ACHIEVEMENT, OnQueryAchievement);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_MASTERY, OnQueryMastery);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_SKINS, OnQuerySkins);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_RECIPES, OnQueryRecipes);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_WIZARDSVAULT, OnQueryWizardsVault);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_API, OnQueryApi);
    APIDefs->Events_Unsubscribe(EV_HOARD_CONTEXT_MENU_REGISTER, OnContextMenuRegister);
    APIDefs->Events_Unsubscribe(EV_HOARD_CONTEXT_MENU_REMOVE, OnContextMenuRemove);
    APIDefs->Events_Unsubscribe(EV_ADDON_UNLOADED, OnAddonUnloaded);
    APIDefs->Events_Unsubscribe(EV_HOARD_PING, OnPing);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_ACCOUNTS, OnQueryAccounts);
    APIDefs->GUI_DeregisterCloseOnEscape("Hoard & Seek");
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->GUI_Deregister(AddonRender);

    SaveSettings();
    APIDefs = nullptr;
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;

    if (strcmp(aIdentifier, "KB_HOARD_TOGGLE") == 0) {
        g_WindowVisible = !g_WindowVisible;
        if (APIDefs) {
            APIDefs->Log(LOGL_INFO, "HoardAndSeek",
                g_WindowVisible ? "Window shown" : "Window hidden");
        }
    }
}

// --- GW2 Theme (matches Alter Ego) ---

static ImGuiStyle g_GW2Style;
static std::vector<ImGuiStyle> g_StyleStack;

static void PushGW2Theme() {
    g_StyleStack.push_back(ImGui::GetStyle());
    ImGui::GetStyle() = g_GW2Style;
}

static void PopGW2Theme() {
    if (!g_StyleStack.empty()) {
        ImGui::GetStyle() = g_StyleStack.back();
        g_StyleStack.pop_back();
    }
}

struct ThemeGuard {
    ThemeGuard()  { PushGW2Theme(); }
    ~ThemeGuard() { PopGW2Theme(); }
};

static void BuildGW2Theme() {
    g_GW2Style = ImGui::GetStyle();
    ImGuiStyle& s = g_GW2Style;

    // Rounding
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;

    // Spacing & padding
    s.WindowPadding     = ImVec2(10, 10);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(8, 5);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 8.0f;
    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    // Colors — dark slate base with warm gold accents
    ImVec4* c = s.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.08f, 0.08f, 0.10f, 0.96f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.07f, 0.07f, 0.09f, 0.80f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);

    // Borders
    c[ImGuiCol_Border]               = ImVec4(0.28f, 0.25f, 0.18f, 0.50f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames (input boxes, combos)
    c[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.13f, 0.11f, 0.80f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.20f, 0.14f, 0.80f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.25f, 0.16f, 0.90f);

    // Title bar
    c[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.09f, 0.07f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.16f, 0.14f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.08f, 0.07f, 0.05f, 0.75f);

    // Menu bar
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.11f, 0.09f, 1.00f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.06f, 0.07f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.27f, 0.18f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.36f, 0.22f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.44f, 0.26f, 1.00f);

    // Checkmark, slider
    c[ImGuiCol_CheckMark]            = ImVec4(0.90f, 0.75f, 0.25f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.70f, 0.58f, 0.20f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.85f, 0.70f, 0.25f, 1.00f);

    // Buttons — warm gold
    c[ImGuiCol_Button]               = ImVec4(0.22f, 0.20f, 0.12f, 0.80f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.38f, 0.16f, 1.00f);

    // Headers (selectables, collapsing headers)
    c[ImGuiCol_Header]               = ImVec4(0.18f, 0.16f, 0.10f, 0.70f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.24f, 0.12f, 0.80f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);

    // Separator
    c[ImGuiCol_Separator]            = ImVec4(0.28f, 0.25f, 0.18f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.50f, 0.42f, 0.20f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.65f, 0.55f, 0.25f, 1.00f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.27f, 0.18f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.44f, 0.26f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.65f, 0.55f, 0.25f, 0.90f);

    // Tabs — gold accent for active
    c[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.13f, 0.10f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.28f, 0.24f, 0.10f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.10f, 0.09f, 0.07f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.18f, 0.16f, 0.10f, 1.00f);

    // Text
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.87f, 0.78f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.47f, 0.40f, 1.00f);

    // Modal dim background
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

    // Nav highlight
    c[ImGuiCol_NavHighlight]         = ImVec4(0.70f, 0.58f, 0.20f, 1.00f);

    // Table
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.13f, 0.10f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.28f, 0.25f, 0.18f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.22f, 0.20f, 0.15f, 0.40f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.10f, 0.10f, 0.08f, 0.30f);

    // Plot (progress bars)
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.65f, 0.55f, 0.15f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.80f, 0.68f, 0.20f, 1.00f);
}

// --- Main Render ---

void AddonRender() {
    // Process icon download queue every frame (even when window hidden)
    HoardAndSeek::IconManager::Tick();

    ThemeGuard themeGuard;

    // Render permission popup (always, even if main window is hidden)
    HoardAndSeek::PermissionManager::RenderPopup();

    // Flush pending context menu registrations whose permissions were granted
    {
        std::lock_guard<std::mutex> lock(g_ContextMenuMutex);
        auto it = g_PendingContextMenuItems.begin();
        while (it != g_PendingContextMenuItems.end()) {
            auto state = HoardAndSeek::PermissionManager::Check(it->requester, EV_HOARD_CONTEXT_MENU_REGISTER);
            if (state == HoardAndSeek::PermissionState::Allowed) {
                // Move to active list
                bool replaced = false;
                for (auto& existing : g_ContextMenuItems) {
                    if (existing.id == it->id && existing.requester == it->requester) {
                        existing = *it;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced) {
                    g_ContextMenuItems.push_back(*it);
                    if (APIDefs) {
                        std::string msg = "Context menu registered: \"" + it->label + "\" by " + it->requester;
                        APIDefs->Log(LOGL_INFO, "HoardAndSeek", msg.c_str());
                    }
                }
                it = g_PendingContextMenuItems.erase(it);
            } else if (state == HoardAndSeek::PermissionState::Denied) {
                it = g_PendingContextMenuItems.erase(it);
            } else {
                ++it; // Still pending
            }
        }
    }

    // Broadcast fetch progress and data-updated events
    {
        static HoardAndSeek::FetchStatus s_prevFetchStatus = HoardAndSeek::FetchStatus::Idle;
        static std::string s_prevFetchMessage;
        auto curStatus = HoardAndSeek::GW2API::GetFetchStatus();
        std::string curMessage = HoardAndSeek::GW2API::GetFetchStatusMessage();

        // Broadcast progress updates during fetch
        if (curStatus == HoardAndSeek::FetchStatus::InProgress && curMessage != s_prevFetchMessage) {
            static HoardFetchProgressPayload progress{};
            progress.api_version = HOARD_API_VERSION;
            strncpy(progress.message, curMessage.c_str(), sizeof(progress.message) - 1);
            progress.message[sizeof(progress.message) - 1] = '\0';
            progress.progress = -1.0f;
            APIDefs->Events_Raise(EV_HOARD_FETCH_PROGRESS, &progress);
        }
        s_prevFetchMessage = curMessage;

        // Broadcast completion
        if (curStatus == HoardAndSeek::FetchStatus::Success &&
            s_prevFetchStatus != HoardAndSeek::FetchStatus::Success) {
            BroadcastDataUpdated();
        }

        // Broadcast error
        if (curStatus == HoardAndSeek::FetchStatus::Error &&
            s_prevFetchStatus != HoardAndSeek::FetchStatus::Error) {
            static HoardFetchErrorPayload errPayload{};
            errPayload.api_version = HOARD_API_VERSION;
            strncpy(errPayload.message, curMessage.c_str(), sizeof(errPayload.message) - 1);
            errPayload.message[sizeof(errPayload.message) - 1] = '\0';
            APIDefs->Events_Raise(EV_HOARD_FETCH_ERROR, &errPayload);
        }

        s_prevFetchStatus = curStatus;
    }

    if (!g_WindowVisible) return;

    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 150), ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::Begin("Hoard & Seek", &g_WindowVisible,
        ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    // Row 1: Refresh button + status message (always on same line)
    auto fetchStatus = HoardAndSeek::GW2API::GetFetchStatus();
    bool scanning = (fetchStatus == HoardAndSeek::FetchStatus::InProgress);
    bool allOnCooldown = HoardAndSeek::GW2API::IsRefreshOnCooldown();
    bool disabled = scanning || allOnCooldown;

    // Refresh checklist popup state
    static bool s_showRefreshPopup = false;
    static std::vector<std::pair<std::string, bool>> s_refreshChecklist; // account_name, selected

    if (disabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    const auto& accounts = HoardAndSeek::GW2API::GetAccounts();
    if (accounts.size() <= 1) {
        // Single account: simple refresh button
        if (ImGui::Button("Refresh Account Data") && !disabled) {
            HoardAndSeek::GW2API::FetchAccountDataAsync();
        }
    } else {
        // Multi-account: refresh button opens checklist popup
        if (ImGui::Button("Refresh Account Data") && !disabled) {
            s_refreshChecklist.clear();
            for (const auto& acct : accounts) {
                bool onCooldown = HoardAndSeek::GW2API::IsRefreshOnCooldown(acct.account_name);
                s_refreshChecklist.push_back({acct.account_name, !onCooldown});
            }
            s_showRefreshPopup = true;
            ImGui::OpenPopup("Refresh Accounts");
        }
    }
    if (disabled) ImGui::PopStyleVar();

    // Refresh checklist popup
    if (ImGui::BeginPopup("Refresh Accounts")) {
        ImGui::Text("Select accounts to refresh:");
        ImGui::Separator();
        for (auto& [name, selected] : s_refreshChecklist) {
            bool onCooldown = HoardAndSeek::GW2API::IsRefreshOnCooldown(name);
            // Find label for display
            std::string display = name;
            for (const auto& acct : accounts) {
                if (acct.account_name == name && !acct.label.empty()) {
                    display = acct.label;
                    break;
                }
            }
            if (onCooldown) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::Checkbox(display.c_str(), &selected);
                ImGui::PopStyleVar();
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(cooldown)");
                selected = false; // Can't select accounts on cooldown
            } else {
                ImGui::Checkbox(display.c_str(), &selected);
            }
        }
        ImGui::Separator();
        // Check if any selected
        bool anySelected = false;
        for (const auto& [name, selected] : s_refreshChecklist) {
            if (selected) { anySelected = true; break; }
        }
        if (!anySelected) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        if (ImGui::Button("Refresh Selected") && anySelected) {
            std::vector<std::string> selected_accounts;
            for (const auto& [name, selected] : s_refreshChecklist) {
                if (selected) selected_accounts.push_back(name);
            }
            HoardAndSeek::GW2API::FetchAccountDataAsync(selected_accounts);
            ImGui::CloseCurrentPopup();
        }
        if (!anySelected) ImGui::PopStyleVar();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Timer flags for transient status messages (must be file-scope statics so InProgress can reset them)
    static bool s_successTimerStarted = false;
    static bool s_successExpired = false;
    static auto s_successTime = std::chrono::steady_clock::now();
    static bool s_errorTimerStarted = false;
    static bool s_errorExpired = false;
    static auto s_errorTime = std::chrono::steady_clock::now();

    ImGui::SameLine();
    if (fetchStatus == HoardAndSeek::FetchStatus::InProgress) {
        // Reset timers so next success/error shows fresh
        s_successTimerStarted = false;
        s_errorTimerStarted = false;
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s",
            HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
    } else if (fetchStatus == HoardAndSeek::FetchStatus::Error) {
        if (!s_errorTimerStarted) {
            s_errorTime = std::chrono::steady_clock::now();
            s_errorTimerStarted = true;
            s_errorExpired = false;
        }
        if (!s_errorExpired) {
            auto errElapsed = std::chrono::steady_clock::now() - s_errorTime;
            if (std::chrono::duration_cast<std::chrono::seconds>(errElapsed).count() >= 5) {
                s_errorExpired = true;
            }
        }
        if (!s_errorExpired) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
        } else {
            time_t last = HoardAndSeek::GW2API::GetLastUpdated();
            if (last > 0) {
                time_t now = std::time(nullptr);
                int el = (int)difftime(now, last);
                std::string ago;
                if (el < 60) ago = "just now";
                else if (el < 3600) ago = std::to_string(el / 60) + "m ago";
                else if (el < 86400) ago = std::to_string(el / 3600) + "h ago";
                else ago = std::to_string(el / 86400) + "d ago";
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Last updated %s", ago.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                    HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
            }
        }
    } else if (fetchStatus == HoardAndSeek::FetchStatus::Success) {
        // Show green "Done" message for 5s, then show "Last updated"
        if (!s_successTimerStarted) {
            s_successTime = std::chrono::steady_clock::now();
            s_successTimerStarted = true;
            s_successExpired = false;
        }
        if (!s_successExpired) {
            auto elapsed = std::chrono::steady_clock::now() - s_successTime;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 5) {
                s_successExpired = true;
            }
        }
        if (!s_successExpired) {
            ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.35f, 1.0f), "%s",
                HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
        } else {
            time_t last = HoardAndSeek::GW2API::GetLastUpdated();
            if (last > 0) {
                time_t now = std::time(nullptr);
                int el = (int)difftime(now, last);
                std::string ago;
                if (el < 60) ago = "just now";
                else if (el < 3600) ago = std::to_string(el / 60) + "m ago";
                else if (el < 86400) ago = std::to_string(el / 3600) + "h ago";
                else ago = std::to_string(el / 86400) + "d ago";
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Last updated %s", ago.c_str());
            } else {
                ImGui::TextUnformatted("");
            }
        }
    } else if (!HoardAndSeek::GW2API::HasAccountData()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            accounts.empty() ? "No data. Add an account in settings." : "No data. Press Refresh.");
    } else if (HoardAndSeek::GW2API::HasAccountData()) {
        time_t last = HoardAndSeek::GW2API::GetLastUpdated();
        if (last > 0) {
            time_t now = std::time(nullptr);
            int elapsed = (int)difftime(now, last);
            std::string ago;
            if (elapsed < 60) ago = "just now";
            else if (elapsed < 3600) ago = std::to_string(elapsed / 60) + "m ago";
            else if (elapsed < 86400) ago = std::to_string(elapsed / 3600) + "h ago";
            else ago = std::to_string(elapsed / 86400) + "d ago";
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Last updated %s", ago.c_str());
        } else {
            ImGui::TextUnformatted("");
        }
    } else {
        ImGui::TextUnformatted("");
    }

    // Account filter combo (only shown when multiple accounts exist)
    static std::string g_AccountFilter; // empty = all accounts
    if (accounts.size() > 1) {
        std::string combo_label = g_AccountFilter.empty() ? "All Accounts" : g_AccountFilter;
        // Find display label
        if (!g_AccountFilter.empty()) {
            for (const auto& acct : accounts) {
                if (acct.account_name == g_AccountFilter && !acct.label.empty()) {
                    combo_label = acct.label;
                    break;
                }
            }
        }
        ImGui::PushItemWidth(-1);
        if (ImGui::BeginCombo("##acctfilter", combo_label.c_str())) {
            if (ImGui::Selectable("All Accounts", g_AccountFilter.empty())) {
                g_AccountFilter.clear();
                // Re-run search with new filter
                if ((int)strlen(g_SearchFilter) >= g_MinSearchLength) {
                    g_SearchResults = HoardAndSeek::GW2API::SearchItems(std::string(g_SearchFilter), g_AccountFilter);
                }
            }
            for (const auto& acct : accounts) {
                std::string display = acct.label.empty() ? acct.account_name : acct.label;
                bool selected = (g_AccountFilter == acct.account_name);
                if (ImGui::Selectable(display.c_str(), selected)) {
                    g_AccountFilter = acct.account_name;
                    // Re-run search with new filter
                    if ((int)strlen(g_SearchFilter) >= g_MinSearchLength) {
                        g_SearchResults = HoardAndSeek::GW2API::SearchItems(std::string(g_SearchFilter), g_AccountFilter);
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
    }

    // Row 2: Search bar with clear button
    float clearBtnWidth = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::PushItemWidth(-(clearBtnWidth + ImGui::GetStyle().ItemSpacing.x));
    bool searchChanged = ImGui::InputTextWithHint("##search", "Search items (min 3 chars)...",
        g_SearchFilter, sizeof(g_SearchFilter));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("x")) {
        g_SearchFilter[0] = '\0';
        g_SearchResults.clear();
        g_SearchDirty = false;
        SaveSettings();
    }

    if (searchChanged) {
        g_SearchDirty = true;
        g_SearchLastKeystroke = std::chrono::steady_clock::now();
        // Clear immediately if below min length
        if ((int)strlen(g_SearchFilter) < g_MinSearchLength) {
            g_SearchResults.clear();
            g_SearchDirty = false;
        }
    }

    // Debounce: dispatch search to background thread 200ms after last keystroke
    if (g_SearchDirty) {
        auto elapsed = std::chrono::steady_clock::now() - g_SearchLastKeystroke;
        if (elapsed >= std::chrono::milliseconds(200)) {
            g_SearchDirty = false;
            std::string query(g_SearchFilter);
            if ((int)query.length() >= g_MinSearchLength && !g_SearchPending) {
                g_SearchPending = true;
                std::string acctFilter = g_AccountFilter;
                std::thread([query, acctFilter]() {
                    auto results = HoardAndSeek::GW2API::SearchItems(query, acctFilter);
                    g_PendingSearchResults = std::move(results);
                    g_SearchResultsReady = true;
                    g_SearchPending = false;
                }).detach();
            } else if ((int)query.length() < g_MinSearchLength) {
                g_SearchResults.clear();
            }
            SaveSettings();
        }
    }

    // Pick up search results from background thread
    if (g_SearchResultsReady) {
        g_SearchResults = std::move(g_PendingSearchResults);
        g_SearchResultsReady = false;
    }

    ImGui::Separator();

    // Results
    RenderResultsList();

    ImGui::End();
}

// --- Options/Settings Render ---

void AddonOptions() {
    ThemeGuard themeGuard;
    ImGui::Text("Hoard & Seek Settings");
    if (ImGui::SmallButton("Homepage")) {
        ShellExecuteA(NULL, "open", "https://pie.rocks.cc/", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Buy me a coffee!")) {
        ShellExecuteA(NULL, "open", "https://ko-fi.com/pieorcake", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::Separator();

    // --- Account Management ---
    ImGui::Text("GW2 Accounts:");
    ImGui::Text("Required permissions: account, inventories, characters");
    ImGui::Spacing();

    auto valStatus = HoardAndSeek::GW2API::GetValidationStatus();
    bool validating = (valStatus == HoardAndSeek::FetchStatus::InProgress);
    static bool pendingSave = false;

    // Add new account
    static char g_NewApiKeyBuf[256] = "";
    static char g_NewLabelBuf[64] = "";
    if (ImGui::CollapsingHeader("Add Account")) {
        ImGui::Indent();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("API Key:");
        ImGui::SameLine();
        if (ImGui::SmallButton(g_ShowApiKey ? "Hide" : "Show")) {
            g_ShowApiKey = !g_ShowApiKey;
        }
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
        if (!g_ShowApiKey) flags |= ImGuiInputTextFlags_Password;
        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##newkey", "Paste API key here...", g_NewApiKeyBuf, sizeof(g_NewApiKeyBuf), flags);
        ImGui::PopItemWidth();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Label:  ");
        ImGui::SameLine();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##newlabel", "Optional, e.g. 'Main Account'", g_NewLabelBuf, sizeof(g_NewLabelBuf));
        ImGui::PopItemWidth();

        if (validating) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        if (ImGui::Button("Add & Validate") && !validating) {
            std::string key(g_NewApiKeyBuf);
            std::string label(g_NewLabelBuf);
            int idx = HoardAndSeek::GW2API::AddAccount(key, label);
            if (idx >= 0) {
                // Validate using the raw key since account_name isn't known yet
                HoardAndSeek::GW2API::ValidateAccountAsync(key);
                pendingSave = true;
                g_NewApiKeyBuf[0] = '\0';
                g_NewLabelBuf[0] = '\0';
                if (APIDefs) APIDefs->Events_Raise(EV_HOARD_ACCOUNTS_CHANGED, nullptr);
            }
        }
        if (validating) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    // Auto-save when validation succeeds
    if (pendingSave && !validating) {
        if (valStatus == HoardAndSeek::FetchStatus::Success || valStatus == HoardAndSeek::FetchStatus::Error) {
            HoardAndSeek::GW2API::SaveAccounts();
            pendingSave = false;
        }
    }

    if (validating) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Validating...");
    }

    // Account list
    ImGui::Spacing();
    const auto& accounts = HoardAndSeek::GW2API::GetAccounts();
    static std::string s_removeConfirm; // account_name pending removal confirmation

    for (size_t i = 0; i < accounts.size(); i++) {
        const auto& acct = accounts[i];
        std::string display = acct.label.empty()
            ? (acct.account_name.empty() ? ("Account " + std::to_string(i + 1)) : acct.account_name)
            : acct.label;

        ImGui::PushID((int)i);
        if (ImGui::CollapsingHeader(display.c_str())) {
            ImGui::Indent();
            if (acct.validated) {
                ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.35f, 1.0f), "Valid");
            } else if (!acct.error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", acct.error.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Not validated");
            }

            if (!acct.account_name.empty()) {
                ImGui::Text("Account: %s", acct.account_name.c_str());
            }

            // Editable API Key
            {
                static std::unordered_map<std::string, std::array<char, 256>> s_keyBufs;
                static std::unordered_map<std::string, bool> s_keyInit;
                std::string id = acct.account_name.empty() ? acct.api_key : acct.account_name;
                auto& buf = s_keyBufs[id];
                if (!s_keyInit[id]) {
                    strncpy(buf.data(), acct.api_key.c_str(), buf.size() - 1);
                    buf[buf.size() - 1] = '\0';
                    s_keyInit[id] = true;
                }
                float inputH = ImGui::GetFrameHeight();
                float textH = ImGui::GetTextLineHeight();
                float pad = (inputH - textH) * 0.5f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad);
                ImGui::Text("API Key:");
                ImGui::SameLine();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - pad);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputText("##apikey", buf.data(), buf.size());
                ImGui::PopItemWidth();
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string newKey(buf.data());
                    HoardAndSeek::GW2API::UpdateAccountKey(acct.account_name, newKey);
                    HoardAndSeek::GW2API::SaveAccounts();
                }
            }

            // Editable label — use a dummy framed widget height so text aligns vertically
            {
                static std::unordered_map<std::string, std::array<char, 64>> s_labelBufs;
                static std::unordered_map<std::string, bool> s_labelInit;
                std::string key = acct.account_name.empty() ? acct.api_key : acct.account_name;
                auto& buf = s_labelBufs[key];
                if (!s_labelInit[key]) {
                    strncpy(buf.data(), acct.label.c_str(), buf.size() - 1);
                    buf[buf.size() - 1] = '\0';
                    s_labelInit[key] = true;
                }
                float inputH = ImGui::GetFrameHeight();
                float textH = ImGui::GetTextLineHeight();
                float pad = (inputH - textH) * 0.5f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad);
                ImGui::Text("Label:");
                ImGui::SameLine();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - pad);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputTextWithHint("##label", "Custom label...", buf.data(), buf.size());
                ImGui::PopItemWidth();
                // Save only when the user finishes editing (clicks away, tabs out, presses Enter)
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string newLabel(buf.data());
                    HoardAndSeek::GW2API::UpdateAccountLabel(acct.account_name, newLabel);
                    HoardAndSeek::GW2API::SaveAccounts();
                }
            }

            if (!acct.permissions.empty()) {
                std::string perms;
                for (size_t pi = 0; pi < acct.permissions.size(); pi++) {
                    if (pi > 0) perms += ", ";
                    perms += acct.permissions[pi];
                }
                ImGui::Text("Scopes: %s", perms.c_str());

                bool hasAccount = false, hasInventories = false, hasCharacters = false;
                for (const auto& p : acct.permissions) {
                    if (p == "account") hasAccount = true;
                    if (p == "inventories") hasInventories = true;
                    if (p == "characters") hasCharacters = true;
                }
                if (!hasAccount || !hasInventories || !hasCharacters) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                        "Warning: Missing required permissions");
                }
            }

            if (acct.last_updated > 0) {
                time_t now = std::time(nullptr);
                int el = (int)difftime(now, acct.last_updated);
                std::string ago;
                if (el < 60) ago = "just now";
                else if (el < 3600) ago = std::to_string(el / 60) + "m ago";
                else if (el < 86400) ago = std::to_string(el / 3600) + "h ago";
                else ago = std::to_string(el / 86400) + "d ago";
                ImGui::Text("Last updated: %s", ago.c_str());
            }

            // Re-validate button
            if (!validating) {
                if (ImGui::SmallButton("Re-validate")) {
                    std::string name = acct.account_name.empty() ? acct.api_key : acct.account_name;
                    HoardAndSeek::GW2API::ValidateAccountAsync(name);
                    pendingSave = true;
                }
                ImGui::SameLine();
            }

            // Remove button with confirmation
            if (s_removeConfirm == acct.account_name) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Remove this account?");
                ImGui::SameLine();
                if (ImGui::SmallButton("Yes")) {
                    HoardAndSeek::GW2API::RemoveAccount(acct.account_name);
                    HoardAndSeek::GW2API::SaveAccounts();
                    if (APIDefs) APIDefs->Events_Raise(EV_HOARD_ACCOUNTS_CHANGED, nullptr);
                    s_removeConfirm.clear();
                    ImGui::Unindent();
                    ImGui::PopID();
                    break; // Iterator invalidated
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("No")) {
                    s_removeConfirm.clear();
                }
            } else {
                if (ImGui::SmallButton("Remove")) {
                    s_removeConfirm = acct.account_name;
                }
            }

            ImGui::Unindent();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Search Settings:");
    ImGui::SliderInt("Min search length", &g_MinSearchLength, 1, 5);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("UI Settings:");
    if (ImGui::Checkbox("Show Quick Access icon", &g_ShowQAIcon)) {
        if (g_ShowQAIcon) {
            APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON_HOVER, "KB_HOARD_TOGGLE", "Hoard & Seek");
        } else {
            APIDefs->QuickAccess_Remove(QA_ID);
        }
        SaveSettings();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Addon Permissions:");
    HoardAndSeek::PermissionManager::RenderSettings();
}

// --- Export: GetAddonDef ---

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = 0x5f45d669;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Hoard & Seek";
    AddonDef.Version.Major = V_MAJOR;
    AddonDef.Version.Minor = V_MINOR;
    AddonDef.Version.Build = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author = "PieOrCake.7635";
    AddonDef.Description = "Find where your items are hiding across your GW2 account";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/PieOrCake/hoard_and_seek";

    return &AddonDef;
}
