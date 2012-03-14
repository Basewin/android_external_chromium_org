// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data in this file was created by referencing:
//  USB HID Usage Tables (v1.11) 27 June 2001
//  HIToolbox/Events.h (Mac)

typedef struct {
  // USB keycode:
  //  Upper 16-bits: USB Usage Page.
  //  Lower 16-bits: USB Usage Id: Assigned ID within this usage page.
  uint32_t usb_keycode;

  // Contains one of the following:
  //  On Linux: XKB scancode
  //  On Windows: Windows OEM scancode TODO(garykac)
  //  On Mac: Mac keycode TODO(garykac)
  uint16_t native_keycode;
} usb_keymap;

const usb_keymap usb_keycode_map[] = {

  // =========================================
  // USB Usage Page 0x01: Generic Desktop Page
  // =========================================

  // Sleep could be encoded as USB#0c0032, but there's no corresponding WakeUp
  // in the 0x0c USB page.

  //            USB      XKB     Win     Mac
  USB_KEYMAP(0x010082, 0x0096, 0x0000, 0xffff),  // SystemSleep
  USB_KEYMAP(0x010083, 0x0097, 0x0000, 0xffff),  // SystemWakeUp

  // =========================================
  // USB Usage Page 0x07: Keyboard/Keypad Page
  // =========================================

  // TODO(garykac):
  // XKB#005c ISO Level3 Shift
  // XKB#005e <>||
  // XKB#006d Linefeed
  // XKB#008a SunProps cf. USB#0700a3 CrSel/Props
  // XKB#008e SunOpen
  // Mac#003f kVK_Function
  // Mac#000a kVK_ISO_Section (ISO keyboards only)
  // Mac#0066 kVK_JIS_Eisu (USB#07008a Henkan?)

  //            USB      XKB     Win     Mac
  USB_KEYMAP(0x070000, 0x0000, 0x0000, 0xffff),  // Reserved
  USB_KEYMAP(0x070001, 0x0000, 0x0000, 0xffff),  // ErrorRollOver
  USB_KEYMAP(0x070002, 0x0000, 0x0000, 0xffff),  // POSTFail
  USB_KEYMAP(0x070003, 0x0000, 0x0000, 0xffff),  // ErrorUndefined
  USB_KEYMAP(0x070004, 0x0026, 0x0000, 0x0000),  // aA
  USB_KEYMAP(0x070005, 0x0038, 0x0000, 0x000b),  // bB
  USB_KEYMAP(0x070006, 0x0036, 0x0000, 0x0008),  // cC
  USB_KEYMAP(0x070007, 0x0028, 0x0000, 0x0002),  // dD

  USB_KEYMAP(0x070008, 0x001a, 0x0000, 0x000e),  // eE
  USB_KEYMAP(0x070009, 0x0029, 0x0000, 0x0003),  // fF
  USB_KEYMAP(0x07000a, 0x002a, 0x0000, 0x0005),  // gG
  USB_KEYMAP(0x07000b, 0x002b, 0x0000, 0x0004),  // hH
  USB_KEYMAP(0x07000c, 0x001f, 0x0000, 0x0022),  // iI
  USB_KEYMAP(0x07000d, 0x002c, 0x0000, 0x0026),  // jJ
  USB_KEYMAP(0x07000e, 0x002d, 0x0000, 0x0028),  // kK
  USB_KEYMAP(0x07000f, 0x002e, 0x0000, 0x0025),  // lL

  USB_KEYMAP(0x070010, 0x003a, 0x0000, 0x002e),  // mM
  USB_KEYMAP(0x070011, 0x0039, 0x0000, 0x002d),  // nN
  USB_KEYMAP(0x070012, 0x0020, 0x0000, 0x001f),  // oO
  USB_KEYMAP(0x070013, 0x0021, 0x0000, 0x0023),  // pP
  USB_KEYMAP(0x070014, 0x0018, 0x0000, 0x000c),  // qQ
  USB_KEYMAP(0x070015, 0x001b, 0x0000, 0x000f),  // rR
  USB_KEYMAP(0x070016, 0x0027, 0x0000, 0x0001),  // sS
  USB_KEYMAP(0x070017, 0x001c, 0x0000, 0x0011),  // tT

  USB_KEYMAP(0x070018, 0x001e, 0x0000, 0x0020),  // uU
  USB_KEYMAP(0x070019, 0x0037, 0x0000, 0x0009),  // vV
  USB_KEYMAP(0x07001a, 0x0019, 0x0000, 0x000d),  // wW
  USB_KEYMAP(0x07001b, 0x0035, 0x0000, 0x0007),  // xX
  USB_KEYMAP(0x07001c, 0x001d, 0x0000, 0x0010),  // yY
  USB_KEYMAP(0x07001d, 0x0034, 0x0000, 0x0006),  // zZ
  USB_KEYMAP(0x07001e, 0x000a, 0x0000, 0x0012),  // 1!
  USB_KEYMAP(0x07001f, 0x000b, 0x0000, 0x0013),  // 2@

  USB_KEYMAP(0x070020, 0x000c, 0x0000, 0x0014),  // 3#
  USB_KEYMAP(0x070021, 0x000d, 0x0000, 0x0015),  // 4$
  USB_KEYMAP(0x070022, 0x000e, 0x0000, 0x0017),  // 5%
  USB_KEYMAP(0x070023, 0x000f, 0x0000, 0x0016),  // 6^
  USB_KEYMAP(0x070024, 0x0010, 0x0000, 0x001a),  // 7&
  USB_KEYMAP(0x070025, 0x0011, 0x0000, 0x001c),  // 8*
  USB_KEYMAP(0x070026, 0x0012, 0x0000, 0x0019),  // 9(
  USB_KEYMAP(0x070027, 0x0013, 0x0000, 0x001d),  // 0)

  USB_KEYMAP(0x070028, 0x0024, 0x0000, 0x0024),  // Return
  USB_KEYMAP(0x070029, 0x0009, 0x0000, 0x0035),  // Escape
  USB_KEYMAP(0x07002a, 0x0016, 0x0000, 0x0033),  // Backspace
  USB_KEYMAP(0x07002b, 0x0017, 0x0000, 0x0030),  // Tab
  USB_KEYMAP(0x07002c, 0x0041, 0x0000, 0x0031),  // Spacebar
  USB_KEYMAP(0x07002d, 0x0014, 0x0000, 0x001b),  // -_
  USB_KEYMAP(0x07002e, 0x0015, 0x0000, 0x0018),  // =+
  USB_KEYMAP(0x07002f, 0x0022, 0x0000, 0x0021),  // [{

  USB_KEYMAP(0x070030, 0x0023, 0x0000, 0x001e),  // }]
  USB_KEYMAP(0x070031, 0x0033, 0x0000, 0x002a),  // \| (US keyboard only)
  // USB#070032 is not present on US keyboard.
  // The keycap varies on international keyboards:
  //   Dan: '*  Dutch: <>  Ger: #'  UK: #~
  // For XKB, it uses the same scancode as the US \| key.
  // TODO(garykac): Verify Mac intl keyboard.
  USB_KEYMAP(0x070032, 0x0033, 0x0000, 0x002a),  // #~ (Non-US)
  USB_KEYMAP(0x070033, 0x002f, 0x0000, 0x0029),  // ;:
  USB_KEYMAP(0x070034, 0x0030, 0x0000, 0x0027),  // '"
  USB_KEYMAP(0x070035, 0x0031, 0x0000, 0x0032),  // `~
  USB_KEYMAP(0x070036, 0x003b, 0x0000, 0x002b),  // ,<
  USB_KEYMAP(0x070037, 0x003c, 0x0000, 0x002f),  // .>

  USB_KEYMAP(0x070038, 0x003d, 0x0000, 0x002c),  // /?
  USB_KEYMAP(0x070039, 0x0042, 0x0000, 0x0039),  // CapsLock
  USB_KEYMAP(0x07003a, 0x0043, 0x0000, 0x007a),  // F1
  USB_KEYMAP(0x07003b, 0x0044, 0x0000, 0x0078),  // F2
  USB_KEYMAP(0x07003c, 0x0045, 0x0000, 0x0063),  // F3
  USB_KEYMAP(0x07003d, 0x0046, 0x0000, 0x0076),  // F4
  USB_KEYMAP(0x07003e, 0x0047, 0x0000, 0x0060),  // F5
  USB_KEYMAP(0x07003f, 0x0048, 0x0000, 0x0061),  // F6

  USB_KEYMAP(0x070040, 0x0049, 0x0000, 0x0062),  // F7
  USB_KEYMAP(0x070041, 0x004a, 0x0000, 0x0064),  // F8
  USB_KEYMAP(0x070042, 0x004b, 0x0000, 0x0065),  // F9
  USB_KEYMAP(0x070043, 0x004c, 0x0000, 0x006d),  // F10
  USB_KEYMAP(0x070044, 0x005f, 0x0000, 0x0067),  // F11
  USB_KEYMAP(0x070045, 0x0060, 0x0000, 0x006f),  // F12
  USB_KEYMAP(0x070046, 0x006b, 0x0000, 0xffff),  // PrintScreen
  USB_KEYMAP(0x070047, 0x004e, 0x0000, 0xffff),  // ScrollLock

  USB_KEYMAP(0x070048, 0x007f, 0x0000, 0xffff),  // Pause
  // Labeled "Help/Insert" on Mac.
  USB_KEYMAP(0x070049, 0x0076, 0x0000, 0x0072),  // Insert
  USB_KEYMAP(0x07004a, 0x006e, 0x0000, 0x0073),  // Home
  USB_KEYMAP(0x07004b, 0x0070, 0x0000, 0x0074),  // PageUp
  USB_KEYMAP(0x07004c, 0x0077, 0x0000, 0x0075),  // Delete (Forward Delete)
  USB_KEYMAP(0x07004d, 0x0073, 0x0000, 0x0077),  // End
  USB_KEYMAP(0x07004e, 0x0075, 0x0000, 0x0079),  // PageDown
  USB_KEYMAP(0x07004f, 0x0072, 0x0000, 0x007c),  // RightArrow

  USB_KEYMAP(0x070050, 0x0071, 0x0000, 0x007b),  // LeftArrow
  USB_KEYMAP(0x070051, 0x0074, 0x0000, 0x007d),  // DownArrow
  USB_KEYMAP(0x070052, 0x006f, 0x0000, 0x007e),  // UpArrow
  USB_KEYMAP(0x070053, 0x004d, 0x0000, 0x0047),  // Keypad_NumLock Clear
  USB_KEYMAP(0x070054, 0x006a, 0x0000, 0x004b),  // Keypad_/
  USB_KEYMAP(0x070055, 0x003f, 0x0000, 0x0043),  // Keypad_*
  USB_KEYMAP(0x070056, 0x0052, 0x0000, 0x004e),  // Keypad_-
  USB_KEYMAP(0x070057, 0x0056, 0x0000, 0x0045),  // Keypad_+

  USB_KEYMAP(0x070058, 0x0068, 0x0000, 0x004c),  // Keypad_Enter
  USB_KEYMAP(0x070059, 0x0057, 0x0000, 0x0053),  // Keypad_1 End
  USB_KEYMAP(0x07005a, 0x0058, 0x0000, 0x0054),  // Keypad_2 DownArrow
  USB_KEYMAP(0x07005b, 0x0059, 0x0000, 0x0055),  // Keypad_3 PageDown
  USB_KEYMAP(0x07005c, 0x0053, 0x0000, 0x0056),  // Keypad_4 LeftArrow
  USB_KEYMAP(0x07005d, 0x0054, 0x0000, 0x0057),  // Keypad_5
  USB_KEYMAP(0x07005e, 0x0055, 0x0000, 0x0058),  // Keypad_6 RightArrow
  USB_KEYMAP(0x07005f, 0x004f, 0x0000, 0x0059),  // Keypad_7 Home

  USB_KEYMAP(0x070060, 0x0050, 0x0000, 0x005b),  // Keypad_8 UpArrow
  USB_KEYMAP(0x070061, 0x0051, 0x0000, 0x005c),  // Keypad_9 PageUp
  USB_KEYMAP(0x070062, 0x005a, 0x0000, 0x0052),  // Keypad_0 Insert
  USB_KEYMAP(0x070063, 0x005b, 0x0000, 0x0041),  // Keypad_. Delete
  // USB#070064 is not present on US keyboard.
  // This key is typically located near LeftShift key.
  // The keycap varies on international keyboards:
  //   Dan: <> Dutch: ][ Ger: <> UK: \|
  // TODO(garykac) Determine correct XKB scancode.
  USB_KEYMAP(0x070064, 0x0000, 0x0000, 0xffff),  // Non-US \|
  USB_KEYMAP(0x070065, 0x0087, 0x0000, 0xffff),  // AppMenu (next to RWin key)
  USB_KEYMAP(0x070066, 0x007c, 0x0000, 0xffff),  // Power
  USB_KEYMAP(0x070067, 0x007d, 0x0000, 0x0051),  // Keypad_=

  USB_KEYMAP(0x070068, 0x0000, 0x0000, 0x0069),  // F13
  USB_KEYMAP(0x070069, 0x0000, 0x0000, 0x006b),  // F14
  USB_KEYMAP(0x07006a, 0x0000, 0x0000, 0x0071),  // F15
  USB_KEYMAP(0x07006b, 0x0000, 0x0000, 0x006a),  // F16
  USB_KEYMAP(0x07006c, 0x0000, 0x0000, 0x0040),  // F17
  USB_KEYMAP(0x07006d, 0x0000, 0x0000, 0x004f),  // F18
  USB_KEYMAP(0x07006e, 0x0000, 0x0000, 0x0050),  // F19
  USB_KEYMAP(0x07006f, 0x0000, 0x0000, 0x005a),  // F20

  USB_KEYMAP(0x070070, 0x0000, 0x0000, 0xffff),  // F21
  USB_KEYMAP(0x070071, 0x0000, 0x0000, 0xffff),  // F22
  USB_KEYMAP(0x070072, 0x0000, 0x0000, 0xffff),  // F23
  USB_KEYMAP(0x070073, 0x0000, 0x0000, 0xffff),  // F24
  USB_KEYMAP(0x070074, 0x0000, 0x0000, 0xffff),  // Execute
  USB_KEYMAP(0x070075, 0x0092, 0x0000, 0xffff),  // Help
  USB_KEYMAP(0x070076, 0x0093, 0x0000, 0xffff),  // Menu
  USB_KEYMAP(0x070077, 0x0000, 0x0000, 0xffff),  // Select

  USB_KEYMAP(0x070078, 0x0000, 0x0000, 0xffff),  // Stop
  USB_KEYMAP(0x070079, 0x0089, 0x0000, 0xffff),  // Again (Redo)
  USB_KEYMAP(0x07007a, 0x008b, 0x0000, 0xffff),  // Undo
  USB_KEYMAP(0x07007b, 0x0091, 0x0000, 0xffff),  // Cut
  USB_KEYMAP(0x07007c, 0x008d, 0x0000, 0xffff),  // Copy
  USB_KEYMAP(0x07007d, 0x008f, 0x0000, 0xffff),  // Paste
  USB_KEYMAP(0x07007e, 0x0090, 0x0000, 0xffff),  // Find
  USB_KEYMAP(0x07007f, 0x0079, 0xe020, 0x004a),  // Mute

  USB_KEYMAP(0x070080, 0x007b, 0xe030, 0x0048),  // VolumeUp
  USB_KEYMAP(0x070081, 0x007a, 0xe02e, 0x0049),  // VolumeDown
  USB_KEYMAP(0x070082, 0x0000, 0x0000, 0xffff),  // LockingCapsLock
  USB_KEYMAP(0x070083, 0x0000, 0x0000, 0xffff),  // LockingNumLock
  USB_KEYMAP(0x070084, 0x0000, 0x0000, 0xffff),  // LockingScrollLock
  // USB#070085 is used as Brazilian Keypad_.
  USB_KEYMAP(0x070085, 0x0000, 0x0000, 0x005f),  // Keypad_Comma
  // USB#070086 is used on AS/400 keyboards. Standard Keypad_= is USB#070067.
  //USB_KEYMAP(0x070086, 0x0000, 0x0000, 0xffff),  // Keypad_=
  // USB#070087 is used for Brazilian /? and Japanese _ 'ro'.
  USB_KEYMAP(0x070087, 0x0000, 0x0000, 0x005e),  // International1

  // USB#070088 is used as Japanese Hiragana/Katakana key.
  USB_KEYMAP(0x070088, 0x0065, 0x0000, 0x0068),  // International2
  // USB#070089 is used as Japanese Yen key.
  USB_KEYMAP(0x070089, 0x0000, 0x007d, 0x005d),  // International3
  // USB#07008a is used as Japanese Henkan (Convert) key.
  USB_KEYMAP(0x07008a, 0x0064, 0x0000, 0xffff),  // International4
  // USB#07008b is used as Japanese Muhenkan (No-convert) key.
  USB_KEYMAP(0x07008b, 0x0066, 0x0000, 0xffff),  // International5
  USB_KEYMAP(0x07008c, 0x0000, 0x0000, 0xffff),  // International6
  USB_KEYMAP(0x07008d, 0x0000, 0x0000, 0xffff),  // International7
  USB_KEYMAP(0x07008e, 0x0000, 0x0000, 0xffff),  // International8
  USB_KEYMAP(0x07008f, 0x0000, 0x0000, 0xffff),  // International9

  // USB#070090 is used as Korean Hangul/English toggle key.
  USB_KEYMAP(0x070090, 0x0082, 0x0000, 0xffff),  // LANG1
  // USB#070091 is used as Korean Hanja conversion key.
  USB_KEYMAP(0x070091, 0x0083, 0x0000, 0xffff),  // LANG2
  // USB#070092 is used as Japanese Katakana key.
  USB_KEYMAP(0x070092, 0x0062, 0x0000, 0xffff),  // LANG3
  // USB#070093 is used as Japanese Hiragana key.
  USB_KEYMAP(0x070093, 0x0063, 0x0000, 0xffff),  // LANG4
  // USB#070094 is used as Japanese Zenkaku/Hankaku (Fullwidth/halfwidth) key.
  USB_KEYMAP(0x070094, 0x0000, 0x0000, 0xffff),  // LANG5
  USB_KEYMAP(0x070095, 0x0000, 0x0000, 0xffff),  // LANG6
  USB_KEYMAP(0x070096, 0x0000, 0x0000, 0xffff),  // LANG7
  USB_KEYMAP(0x070097, 0x0000, 0x0000, 0xffff),  // LANG8

  USB_KEYMAP(0x070098, 0x0000, 0x0000, 0xffff),  // LANG9
  USB_KEYMAP(0x070099, 0x0000, 0x0000, 0xffff),  // AlternateErase
  USB_KEYMAP(0x07009a, 0x0000, 0x0000, 0xffff),  // SysReq/Attention
  USB_KEYMAP(0x07009b, 0x0088, 0x0000, 0xffff),  // Cancel
  USB_KEYMAP(0x07009c, 0x0000, 0x0000, 0xffff),  // Clear
  USB_KEYMAP(0x07009d, 0x0000, 0x0000, 0xffff),  // Prior
  USB_KEYMAP(0x07009e, 0x0000, 0x0000, 0xffff),  // Return
  USB_KEYMAP(0x07009f, 0x0000, 0x0000, 0xffff),  // Separator

  USB_KEYMAP(0x0700a0, 0x0000, 0x0000, 0xffff),  // Out
  USB_KEYMAP(0x0700a1, 0x0000, 0x0000, 0xffff),  // Oper
  USB_KEYMAP(0x0700a2, 0x0000, 0x0000, 0xffff),  // Clear/Again
  USB_KEYMAP(0x0700a3, 0x0000, 0x0000, 0xffff),  // CrSel/Props
  USB_KEYMAP(0x0700a4, 0x0000, 0x0000, 0xffff),  // ExSel

  //USB_KEYMAP(0x0700b0, 0x0000, 0x0000, 0xffff),  // Keypad_00
  //USB_KEYMAP(0x0700b1, 0x0000, 0x0000, 0xffff),  // Keypad_000
  //USB_KEYMAP(0x0700b2, 0x0000, 0x0000, 0xffff),  // ThousandsSeparator
  //USB_KEYMAP(0x0700b3, 0x0000, 0x0000, 0xffff),  // DecimalSeparator
  //USB_KEYMAP(0x0700b4, 0x0000, 0x0000, 0xffff),  // CurrencyUnit
  //USB_KEYMAP(0x0700b5, 0x0000, 0x0000, 0xffff),  // CurrencySubunit
  USB_KEYMAP(0x0700b6, 0x00bb, 0x0000, 0xffff),  // Keypad_(
  USB_KEYMAP(0x0700b7, 0x00bc, 0x0000, 0xffff),  // Keypad_)

  //USB_KEYMAP(0x0700b8, 0x0000, 0x0000, 0xffff),  // Keypad_{
  //USB_KEYMAP(0x0700b9, 0x0000, 0x0000, 0xffff),  // Keypad_}
  //USB_KEYMAP(0x0700ba, 0x0000, 0x0000, 0xffff),  // Keypad_Tab
  //USB_KEYMAP(0x0700bb, 0x0000, 0x0000, 0xffff),  // Keypad_Backspace
  //USB_KEYMAP(0x0700bc, 0x0000, 0x0000, 0xffff),  // Keypad_A
  //USB_KEYMAP(0x0700bd, 0x0000, 0x0000, 0xffff),  // Keypad_B
  //USB_KEYMAP(0x0700be, 0x0000, 0x0000, 0xffff),  // Keypad_C
  //USB_KEYMAP(0x0700bf, 0x0000, 0x0000, 0xffff),  // Keypad_D

  //USB_KEYMAP(0x0700c0, 0x0000, 0x0000, 0xffff),  // Keypad_E
  //USB_KEYMAP(0x0700c1, 0x0000, 0x0000, 0xffff),  // Keypad_F
  //USB_KEYMAP(0x0700c2, 0x0000, 0x0000, 0xffff),  // Keypad_Xor
  //USB_KEYMAP(0x0700c3, 0x0000, 0x0000, 0xffff),  // Keypad_^
  //USB_KEYMAP(0x0700c4, 0x0000, 0x0000, 0xffff),  // Keypad_%
  //USB_KEYMAP(0x0700c5, 0x0000, 0x0000, 0xffff),  // Keypad_<
  //USB_KEYMAP(0x0700c6, 0x0000, 0x0000, 0xffff),  // Keypad_>
  //USB_KEYMAP(0x0700c7, 0x0000, 0x0000, 0xffff),  // Keypad_&

  //USB_KEYMAP(0x0700c8, 0x0000, 0x0000, 0xffff),  // Keypad_&&
  //USB_KEYMAP(0x0700c9, 0x0000, 0x0000, 0xffff),  // Keypad_|
  //USB_KEYMAP(0x0700ca, 0x0000, 0x0000, 0xffff),  // Keypad_||
  //USB_KEYMAP(0x0700cb, 0x0000, 0x0000, 0xffff),  // Keypad_:
  //USB_KEYMAP(0x0700cc, 0x0000, 0x0000, 0xffff),  // Keypad_#
  //USB_KEYMAP(0x0700cd, 0x0000, 0x0000, 0xffff),  // Keypad_Space
  //USB_KEYMAP(0x0700ce, 0x0000, 0x0000, 0xffff),  // Keypad_@
  //USB_KEYMAP(0x0700cf, 0x0000, 0x0000, 0xffff),  // Keypad_!

  //USB_KEYMAP(0x0700d0, 0x0000, 0x0000, 0xffff),  // Keypad_MemoryStore
  //USB_KEYMAP(0x0700d1, 0x0000, 0x0000, 0xffff),  // Keypad_MemoryRecall
  //USB_KEYMAP(0x0700d2, 0x0000, 0x0000, 0xffff),  // Keypad_MemoryClear
  //USB_KEYMAP(0x0700d3, 0x0000, 0x0000, 0xffff),  // Keypad_MemoryAdd
  //USB_KEYMAP(0x0700d4, 0x0000, 0x0000, 0xffff),  // Keypad_MemorySubtract
  //USB_KEYMAP(0x0700d5, 0x0000, 0x0000, 0xffff),  // Keypad_MemoryMultiply
  //USB_KEYMAP(0x0700d6, 0x0000, 0x0000, 0xffff),  // Keypad_MemoryDivide
  USB_KEYMAP(0x0700d7, 0x007e, 0x0000, 0xffff),  // Keypad_+/-

  //USB_KEYMAP(0x0700d8, 0x0000, 0x0000, 0xffff),  // Keypad_Clear
  //USB_KEYMAP(0x0700d9, 0x0000, 0x0000, 0xffff),  // Keypad_ClearEntry
  //USB_KEYMAP(0x0700da, 0x0000, 0x0000, 0xffff),  // Keypad_Binary
  //USB_KEYMAP(0x0700db, 0x0000, 0x0000, 0xffff),  // Keypad_Octal
  USB_KEYMAP(0x0700dc, 0x0081, 0x0000, 0xffff),  // Keypad_Decimal
  //USB_KEYMAP(0x0700dd, 0x0000, 0x0000, 0xffff),  // Keypad_Hexadecimal
  // USB#0700de - #0700df are reserved.

  USB_KEYMAP(0x0700e0, 0x0025, 0x0000, 0x003b),  // LeftControl
  USB_KEYMAP(0x0700e1, 0x0032, 0x0000, 0x0038),  // LeftShift
  USB_KEYMAP(0x0700e2, 0x0040, 0x0000, 0x003a),  // LeftAlt/Option
  USB_KEYMAP(0x0700e3, 0x0085, 0x0000, 0x0037),  // LeftGUI/Super/Win/Cmd
  USB_KEYMAP(0x0700e4, 0x0069, 0x0000, 0x003e),  // RightControl
  USB_KEYMAP(0x0700e5, 0x003e, 0x0000, 0x003c),  // RightShift
  USB_KEYMAP(0x0700e6, 0x006c, 0x0000, 0x003d),  // RightAlt/Option
  USB_KEYMAP(0x0700e7, 0x0086, 0x0000, 0x0036),  // RightGUI/Super/Win/Cmd

  // USB#0700e8 - #07ffff are reserved

  // ==================================
  // USB Usage Page 0x0c: Consumer Page
  // ==================================
  // AL = Application Launch
  // AC = Application Control

  // TODO(garykac): Many XF86 keys have multiple scancodes mapping to them.
  // We need to map all of these into a canonical USB scancode without
  // confusing the reverse-lookup - most likely by simply returning the first
  // found match.

  // TODO(garykac): Find appropriate mappings for:
  // Win#e06b LaunchApp1 (My Computer?)
  // Win#e021 LaunchApp2 (Calculator?)
  // Win#e03c Music - USB#0c0193 is AL_AVCapturePlayback
  // Win#e06d Video - USB#0c0193 is AL_AVCapturePlayback
  // Win#e064 Pictures
  // XKB#0080 XF86LaunchA
  // XKB#0097 XF86WakeUp
  // XKB#0099 XF86Send
  // XKB#009b XF86Xfer
  // XKB#009c XF86Launch1
  // XKB#009d XF86Launch2
  // XKB... remaining XF86 keys

  //            USB      XKB     Win     Mac
  USB_KEYMAP(0x0c00b5, 0x0000, 0xe019, 0xffff),  // ScanNextTrack
  USB_KEYMAP(0x0c00b6, 0x0000, 0xe010, 0xffff),  // ScanPreviousTrack
  USB_KEYMAP(0x0c00b7, 0x0000, 0xe024, 0xffff),  // Stop
  USB_KEYMAP(0x0c00b8, 0x0000, 0xe02c, 0xffff),  // Eject
  USB_KEYMAP(0x0c00cd, 0x0000, 0xe022, 0xffff),  // Play/Pause
  USB_KEYMAP(0x0c018a, 0x0000, 0xe01e, 0xffff),  // AL_EmailReader
  USB_KEYMAP(0x0c0192, 0x0094, 0x0000, 0xffff),  // AL_Calculator
  // USB#0c0194: My Computer
  USB_KEYMAP(0x0c0194, 0x00a5, 0x0000, 0xffff),  // AL_LocalMachineBrowser
  USB_KEYMAP(0x0c01a7, 0x00f3, 0x0000, 0xffff),  // AL_Documents
  // USB#0c01b4: Home Directory
  USB_KEYMAP(0x0c01b4, 0x0098, 0x0000, 0xffff),  // AL_FileBrowser (Explorer)
  USB_KEYMAP(0x0c0221, 0x0000, 0xe065, 0xffff),  // AC_Search
  USB_KEYMAP(0x0c0223, 0x0000, 0xe032, 0xffff),  // AC_Home
  USB_KEYMAP(0x0c0224, 0x00a6, 0xe06a, 0xffff),  // AC_Back
  USB_KEYMAP(0x0c0225, 0x00a7, 0xe069, 0xffff),  // AC_Forward
  USB_KEYMAP(0x0c0226, 0x0000, 0xe068, 0xffff),  // AC_Stop
  USB_KEYMAP(0x0c0227, 0x00b5, 0xe067, 0xffff),  // AC_Refresh (Reload)
  USB_KEYMAP(0x0c022a, 0x00a4, 0xe066, 0xffff),  // AC_Bookmarks (Favorites)
  USB_KEYMAP(0x0c0289, 0x00f0, 0xe000, 0xffff),  // AC_Reply
  USB_KEYMAP(0x0c028b, 0x00f1, 0xe000, 0xffff),  // AC_ForwardMsg (MailForward)
  USB_KEYMAP(0x0c028c, 0x00ef, 0xe000, 0xffff),  // AC_Send
};
