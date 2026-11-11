// Generated file -- do not edit by hand.
// Regenerate with: python tools/generate_keynames.py
// Requirements: pip install pywin32
//
// Names are queried from the Windows GetKeyNameTextW API and therefore
// match what the user sees on their own layout. The output is committed
// so that builds stay reproducible across Windows versions.

#pragma once

struct KeyNameEntry {
    unsigned vk;
    const wchar_t *name;
};

inline constexpr KeyNameEntry kKeyNames[] = {
        {0x08, L"Backspace"}, {0x09, L"Tab"},         {0x0D, L"Enter"},     {0x14, L"Caps Lock"}, {0x1B, L"Esc"},
        {0x20, L"Space"},     {0x21, L"Page Up"},     {0x22, L"Page Down"}, {0x23, L"End"},       {0x24, L"Home"},
        {0x25, L"Left"},      {0x26, L"Up"},          {0x27, L"Right"},     {0x28, L"Down"},      {0x2C, L"Sys Req"},
        {0x2D, L"Insert"},    {0x2E, L"Delete"},      {0x30, L"0"},         {0x31, L"1"},         {0x32, L"2"},
        {0x33, L"3"},         {0x34, L"4"},           {0x35, L"5"},         {0x36, L"6"},         {0x37, L"7"},
        {0x38, L"8"},         {0x39, L"9"},           {0x41, L"A"},         {0x42, L"B"},         {0x43, L"C"},
        {0x44, L"D"},         {0x45, L"E"},           {0x46, L"F"},         {0x47, L"G"},         {0x48, L"H"},
        {0x49, L"I"},         {0x4A, L"J"},           {0x4B, L"K"},         {0x4C, L"L"},         {0x4D, L"M"},
        {0x4E, L"N"},         {0x4F, L"O"},           {0x50, L"P"},         {0x51, L"Q"},         {0x52, L"R"},
        {0x53, L"S"},         {0x54, L"T"},           {0x55, L"U"},         {0x56, L"V"},         {0x57, L"W"},
        {0x58, L"X"},         {0x59, L"Y"},           {0x5A, L"Z"},         {0x60, L"Num 0"},     {0x61, L"Num 1"},
        {0x62, L"Num 2"},     {0x63, L"Num 3"},       {0x64, L"Num 4"},     {0x65, L"Num 5"},     {0x66, L"Num 6"},
        {0x67, L"Num 7"},     {0x68, L"Num 8"},       {0x69, L"Num 9"},     {0x6A, L"Num *"},     {0x6B, L"Num +"},
        {0x6D, L"Num -"},     {0x6E, L"Num Del"},     {0x6F, L"Num /"},     {0x70, L"F1"},        {0x71, L"F2"},
        {0x72, L"F3"},        {0x73, L"F4"},          {0x74, L"F5"},        {0x75, L"F6"},        {0x76, L"F7"},
        {0x77, L"F8"},        {0x78, L"F9"},          {0x79, L"F10"},       {0x7A, L"F11"},       {0x7B, L"F12"},
        {0x90, L"Num Lock"},  {0x91, L"Scroll Lock"}, {0xBA, L";"},         {0xBB, L"="},         {0xBC, L","},
        {0xBD, L"-"},         {0xBE, L"."},           {0xBF, L"/"},         {0xC0, L"`"},         {0xDB, L"["},
        {0xDC, L"\\"},        {0xDD, L"]"},           {0xDE, L"'"},
};
