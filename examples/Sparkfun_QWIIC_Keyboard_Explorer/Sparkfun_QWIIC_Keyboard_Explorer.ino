/*
    MIT License

    Copyright (c) 2021 Scott A Dixon

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    Adapted from:
    Basic Arduino USB Keyboard
    By: Nick Poole
    SparkFun Electronics
    Date: August 12th 2020
    License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

    -------------------------------------------------------------------------------------------------------------------

    This example is adapted from the [Sparkfun QWIIC Keyboard
    Explorer](https://github.com/sparkfunX/Qwiic_Keyboard_Explorer) examples and demonstrates using the
    SwitchMatrixScanner with this product instead of the Keypad_Matrix.h library (by
    [Nick Gammon](https://github.com/nickgammon/Keypad_Matrix)) which the original demo used.

*/

#include <SwitchMatrixScanner.h>  // This library
#include <Keyboard.h>             // From Arduino

// Declare how many keys are on the keyboard
const byte ROWS = 2;
const byte COLS = 7;

// Define English/ASCII characters we'll use for each switch.
const char keymap[] = {'1', '2', '3', '4', '5', '6', '7', 'a', 'b', 'c', 'd', 'e', 'f', 'g'};

// These are the pin declarations for the Qwiic Keyboard Explorer
const byte rowPins[ROWS] = {A0, A1};
const byte colPins[COLS] = {A2, 4, 5, 6, 7, 8, 9};

// Keep track of which mode we're currently in
boolean kbdMode = false;

// Create the Keypad scanner.
//
// This might look odd to you if you are new to C++. The SwitchMatrixScanner is a C++ template. What this means is we
// tell the compiler the actual value of certain things in the SwitchMatrixScanner class before compiling it but not as
// part of the class definition. This allows the author of classes to change the size or type of things based on how
// the class is used before the code is compiled. This saves a system from having to use CPU time to figure this stuff
// out and, if the class can't handle the types or if the size of something is too big or small Arduino will tell you
// this when if verifies the program which is better then trying to figure out why your project isn't working after you
// upload it. For the SwitchMatrixScanner template, we use the parameters (these are the numbers in the brackets <>) to
// tell C++ to reserve `ROWS * COLS * 4` bytes of memory inside of the scanner object (where `4` is a coefficient inside
// of the class: the class uses 4 bytes to track the state of a switch). If your target doesn't have that much memory
// the sketch verification will fail.
//
// The `gh::thirtytwobits::` prefixes are called "namespaces" and are used to ensure that my class can be used even if
// you want to use something else call "SwitchMatrixScanner" in your sketch. It uses my Github username which is
// unlikely to conflict since Github doesn't allow duplicate usernames.
//
// And that's all that's weird about this class. Everything else should look normal. Onwards!
//
gh::thirtytwobits::SwitchMatrixScanner<ROWS, COLS> scanner(
    rowPins,
    colPins,
    true,  // enable pullups for the column pins since the Keyboard Explorer doesn't include them on the PCB.
    true   // enable software debouncing logic. Many MCUs can provide debouncing in-hardware but Arduino doesn't support
           // this so we'll do it in software.
);

// key-up event handler. Okay, this does look weird, right?
// More of my C++11 verification-time specifications. What the `scancode` argument means is that, for the parameters
// we used when we created the `scanner` object, use the value calculated for the `event_buffer_size` variable to
// specify the *maximum* number of switch state-changes we will be informed of at once (scancodes_len is the number of
// switches that changed for a given callback).
//
// Why do you get more than one switch event per call? This is a form of [Vector
// IO](https://en.wikipedia.org/wiki/Vectored_I/O) which is a more efficient then making a function call per switch.
//
// Scancode to Key-switch Map for the [Sparkfun QWIIC Keyboard Explorer](https://www.sparkfun.com/products/17251)
//      _______ _______ _______ _______ _______ _______ _______
//     |\     /|\     /|\     /|\     /|\     /|\     /|\     /|
//     | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ |
//     | |   | | |   | | |   | | |   | | |   | | |   | | |   | |
//     | |1  | | |2  | | |3  | | |4  | | |5  | | |6  | | |7  | |
//     | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ |
//     |/_____\|/_____\|/_____\|/_____\|/_____\|/_____\|/_____\|
//
//      _______ _______ _______ _______ _______ _______ _______
//     |\     /|\     /|\     /|\     /|\     /|\     /|\     /|
//     | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ |
//     | |   | | |   | | |   | | |   | | |   | | |   | | |   | |
//     | |8  | | |9  | | |10 | | |11 | | |12 | | |13 | | |14 | |
//     | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ |
//     |/_____\|/_____\|/_____\|/_____\|/_____\|/_____\|/_____\|
//
// > NOTE: scancodes start at 1. 0 Is not a legal scancode for the SwitchMatrixScanner class.
//
void onKeyUp(const uint16_t (&scancodes)[decltype(scanner)::event_buffer_size], size_t scancodes_len)
{
    for (size_t i = 0; i < scancodes_len; ++i)
    {
        // 12345617gfed
        // scancodes are 1-based and 0 is not a valid scancode.
        //
        // We map the scancode, which is a number assigned to the key-switch by the scanner, to an English character
        // here. It is good Human Interface Device (HID) design to avoid polluting low-level hardware drivers, like our
        // scanner, with human language details. By using a consistent, abstract set of key identifiers in the scanner
        // we can write higher-level logic that handles mapping keyboard interactions into human language output. For
        // example, you could choose to map scancode 1 to the Greek letter lambda or the Latin character e but with a
        // French accent, etc. This type of mapping is called Internationalization (sometimes abbreviated I18N) and is
        // what your computer is doing when you type based on what language you told your operating system to use.
        //
        const char which = keymap[scancodes[i] - 1];
        if (kbdMode)
        {
            Keyboard.release(which);
        }
        else
        {
            SerialUSB.print(F("Key up: "));
            SerialUSB.println(which);
        }
    }
}

// Same as the onKeyUp handler but for keydown.
//
// Note that you'll get these callback if the key is down when the sketch starts running. You will *not* get key-up
// events at the start of the sketch so you are guaranteed to never get a key-up before a key-down for a given
// key/scancode.
//
void onKeyDown(const uint16_t (&scancodes)[decltype(scanner)::event_buffer_size], size_t scancodes_len)
{
    for (size_t i = 0; i < scancodes_len; ++i)
    {
        // scancodes are 1-based.
        const char which = keymap[scancodes[i] - 1];
        if (kbdMode)
        {
            Keyboard.press(which);
        }
        else
        {
            SerialUSB.print(F("Key down: "));
            SerialUSB.println(which);
        }
    }
}

// Check the mode switch position and change between USB-HID mode and Serial mode
void checkMode()
{
    if (kbdMode)
    {
        if (!digitalRead(A3))
        {
            kbdMode = false;
            Keyboard.releaseAll();
            Keyboard.end();
        }
    }
    else
    {
        if (digitalRead(A3))
        {
            kbdMode = true;
            Keyboard.begin();
        }
    }
}

void setup()
{
    SerialUSB.begin(115200);            // baud rate for Serial mode
    scanner.setup(onKeyDown, onKeyUp);  // tell the SwitchMatrixScanner object how to tell us about keyboard events.
    pinMode(A3, INPUT_PULLUP);          // initialize mode switch pin
}

void loop()
{
    // How fast you call this is important to the performance of your keyboard. Faster is better, for the most part but
    // the SwitchMatrixScanner object documents how to setup, tune, and debug your project. Also note, that, if there
    // are key down or up events your callbacks will be invoked from within this method. This isn't a pattern you'd find
    // in traditional device drivers but it greatly simplifies things for Arduino projects. Again, the
    // SwitchMatrixScanner object documentation discusses this design tradeoff.
    scanner.scan();
    checkMode();  // Check mode switch
}
