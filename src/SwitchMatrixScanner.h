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
*/

#ifndef GH_THIRTYTWOBITS_SWITCH_MATRIX_SCANNER_H
#define GH_THIRTYTWOBITS_SWITCH_MATRIX_SCANNER_H

#include <stdint.h>
#include <stddef.h>

namespace gh
{
namespace thirtytwobits
{
namespace
{
enum SwitchState : uint8_t
{
    UNKNOWN = 0,
    OPEN    = 1,
    CLOSED  = 2
};

struct SwitchDef
{
    uint16_t    scancode;
    SwitchState state;
    uint8_t     sample_buffer;
};
};  // namespace

/**
 * Raw switch matrix scanning logic with optional software debounce for Arduino.
 *
 * Example:
 *
 *      const byte rowPins[ROWS] = {A0, A1};
 *      const byte colPins[COLS] = {A2, 4, 5, 6, 7, 8, 9};
 *
 *      gh::thirtytwobits::SwitchMatrixScanner<ROWS, COLS> scanner(
 *          rowPins,
 *          colPins,
 *          true,  // enable pullups for the column pins.
 *          true   // enable software debouncing logic. Many MCUs can provide debouncing in-hardware but Arduino doesn't
 *                 // support this so we'll do it in software.
 *      );
 *
 */
template <size_t ROW_COUNT, size_t COL_COUNT, size_t EVENT_BUFFER_SIZE = 10>
class SwitchMatrixScanner final
{
public:
    /**
     * Use this as the callback type if using event callbacks.
     *
     * Example:
     *
     *      void onKeyDown(const uint16_t (&scancodes)[decltype(scanner)::event_buffer_size], size_t scancodes_len)
     *      {
     *          for (size_t i = 0; i < scancodes_len; ++i)
     *               {
     *              const char typed_character = my_keymap[scancodes[i] - 1];
     *              // Use Keyboard.h or something else
     *          }
     *      }
     */
    using SwitchHandler = void (*)(const uint16_t (&scancodes)[EVENT_BUFFER_SIZE], size_t scancodes_len);

    static constexpr size_t event_buffer_size = EVENT_BUFFER_SIZE;
    static constexpr size_t row_count         = ROW_COUNT;
    static constexpr size_t col_count         = COL_COUNT;

    /**
     * Required constructor.
     * 
     * @param  row_pins Array of Arduino pin ids for each row in the keyboard matrix.
     * @param  column_pins Array of Arduino pin ids for each column in the keyboard matrix.
     * @param  enable_pullups   If true then INPUT_PULLUPS will be used for row pins otherwise
     *                          INPUT will be used with the expectation that the hardware has
     *                          pullups.
     * @param  enable_software_debounce If true then this object will track samples of switches over
     *                          time adding some hysteresis and deboouncing logic. If false then
     *                          the class will use single samples to determine switch state.
     */
    SwitchMatrixScanner(const uint8_t (&row_pins)[ROW_COUNT],
                        const uint8_t (&column_pins)[COL_COUNT],
                        const bool enable_pullups           = true,
                        const bool enable_software_debounce = true)
        : m_switch_map()
        , m_row_pins()
        , m_col_pins()
        , m_switchhandler_closed(nullptr)
        , m_switchhandler_open(nullptr)
        , m_column_input_type((enable_pullups) ? INPUT_PULLUP : INPUT)
        , m_enable_software_debounce(enable_software_debounce)
        , m_scancode_event_buffer_opened{0}
        , m_scancode_event_buffer_opened_len(0)
        , m_scancode_event_buffer_closed{0}
        , m_scancode_event_buffer_closed_len(0)
    {
        memcpy(m_row_pins, row_pins, sizeof(row_pins));
        memcpy(m_col_pins, column_pins, sizeof(column_pins));
        uint16_t scancode = 1;
        for (size_t r = 0; r < ROW_COUNT; ++r)
        {
            for (size_t c = 0; c < COL_COUNT; ++c)
            {
                m_switch_map[r][c] = {scancode++, SwitchState::UNKNOWN, 0};
            }
        }
    }

    ~SwitchMatrixScanner() {}
    SwitchMatrixScanner(const SwitchMatrixScanner&)  = delete;
    SwitchMatrixScanner(const SwitchMatrixScanner&&) = delete;
    SwitchMatrixScanner& operator=(const SwitchMatrixScanner&) = delete;
    SwitchMatrixScanner& operator=(const SwitchMatrixScanner&&) = delete;

    /**
     * Call from within the Arduino setup function. If handlers are provided they will be invoked
     * from within the scan method. If omitted the sketch must use the isSwitchClosed method to determine
     * switch state.
     */
    void setup(SwitchHandler switchclosed_handler = nullptr, SwitchHandler switchopen_handler = nullptr)
    {
        m_switchhandler_closed = switchclosed_handler;
        m_switchhandler_open   = switchopen_handler;

        for (size_t r = 0; r < ROW_COUNT; ++r)
        {
            pinMode(m_row_pins[r], INPUT);
        }
        for (size_t c = 0; c < COL_COUNT; ++c)
        {
            pinMode(m_col_pins[c], m_column_input_type);
        }
    }

    /**
     * Call from within the Arduino loop method. When debouncing is enabled it is important to call
     * this in a fast loop and at a regular period. Variable timing will cause weird delays to users
     * of a keyboard where some keystrokes will be missed or will occur late.
     */
    void scan()
    {
        for (size_t r = 0; r < ROW_COUNT; ++r)
        {
            const uint8_t rowpin = m_row_pins[r];
            pinMode(rowpin, OUTPUT);
            digitalWrite(rowpin, LOW);
            for (size_t c = 0; c < COL_COUNT; ++c)
            {
                SwitchDef& swtch = m_switch_map[r][c];
                // Always sample to ensure the timing is stable despite hyteresis settings.
                const int  value             = digitalRead(m_col_pins[c]);
                const bool is_switch_pressed = (value == LOW);
                if (m_enable_software_debounce)
                {
                    handleSoftwareDebounce(swtch, is_switch_pressed);
                }
                else
                {
                    static_assert(sizeof(swtch.sample_buffer) == 1,
                                  "Hardcoded logic expects 1-byte sample_buffer. Modify this line if you changed the "
                                  "sample_buffer type.");
                    swtch.sample_buffer = (is_switch_pressed) ? 0xFF : 0x00;
                }
                if (handleSwitchState(swtch))
                {
                    if (m_scancode_event_buffer_closed_len == EVENT_BUFFER_SIZE)
                    {
                        // We're about to overrun our event buffer so we'll have to flush
                        // before we're done scanning.
                        flush_closed_events();
                    }
                    if (m_scancode_event_buffer_opened_len == EVENT_BUFFER_SIZE)
                    {
                        // We're about to overrun our event buffer so we'll have to flush
                        // before we're done scanning.
                        flush_opened_events();
                    }
                }
            }
            // High-impedence
            pinMode(rowpin, INPUT);
        }
        flush_closed_events();
        flush_opened_events();
    }

    /**
     * Determine the switch state for a given scancode. Scancodes are generated internally
     * based on the row and column count and are 1-based. For example, if a matrix has three
     * rows and three columns the scancodes will be:
     * 
     *     +-----------+
     *     | 1 | 2 | 3 |
     *     +-----------+
     *     | 4 | 5 | 6 |
     *     +-----------+
     *     | 7 | 8 | 9 |
     *     +-----------+
     */
    bool isSwitchClosed(uint16_t scancode)
    {
        if (scancode == 0)
        {
            return false;
        }
        const uint16_t scanindex = scancode - 1;
        if (scanindex >= ROW_COUNT * COL_COUNT)
        {
            return false;
        }
        const uint16_t row = scanindex / COL_COUNT;
        const uint16_t col = scanindex - (row * COL_COUNT);
        return (m_switch_map[row][col].state == SwitchState::CLOSED);
    }

private:
    // +----------------------------------------------------------------------+
    // | SOFTWARE DEBOUNCING PARAMETERS :: ADJUSTABLE
    // +----------------------------------------------------------------------+

    static constexpr const uint8_t DebounceSettleCount = 4;
    static constexpr const uint8_t DebounceSampleCount = 3;
    static constexpr const uint8_t DebounceSettleBits  = 3;
    static constexpr const uint8_t DebounceSampleBits  = 5;

    // +----------------------------------------------------------------------+
    // | SOFTWARE DEBOUNCING PARAMETERS :: VALIDATION
    // +----------------------------------------------------------------------+
    static_assert(DebounceSettleCount <= (1 << DebounceSettleBits) - 1,
                  "DebounceSettleCount must fit in DebounceSettleBits bits.");
    static_assert(DebounceSampleCount <= DebounceSampleBits, "DebounceSampleCount must be <= DebounceSampleBits");
    static_assert(DebounceSettleBits + DebounceSampleBits <= 8, "DebounceSampleBits + DebounceSettleBits must be <=8");

    // +----------------------------------------------------------------------+
    // | SOFTWARE DEBOUNCING PARAMETERS :: COMPUTED
    // +----------------------------------------------------------------------+
    static constexpr const uint8_t DebounceSettleShift = DebounceSampleBits;
    static constexpr const uint8_t DebounceSettleMask  = ((1 << DebounceSettleBits) - 1) << DebounceSettleShift;
    static constexpr const uint8_t DebounceSampleMask  = ((1 << DebounceSampleBits) - 1);
    // No sample shift since we always located it in the LSbs.

    // +----------------------------------------------------------------------+
    // | VALIDATE TEMPLATE PARAMS
    // +----------------------------------------------------------------------+
    static constexpr const size_t ScanCodeMax = 0xFFFF;

    static_assert(ROW_COUNT * COL_COUNT < ScanCodeMax - 1, "This class can only scan up to ScanCodeMax - 1 switches.");
    static_assert(ROW_COUNT > 0, "ROW_COUNT cannot be 0");
    static_assert(COL_COUNT > 0, "COL_COUNT cannot be 0");
    static_assert(EVENT_BUFFER_SIZE > 0, "EVENT_BUFFER_SIZE cannot be 0");

    // +----------------------------------------------------------------------+
    // | HELPERS
    // +----------------------------------------------------------------------+
    void flush_opened_events()
    {
        if (m_scancode_event_buffer_opened_len > 0)
        {
            onSwitchOpen(m_scancode_event_buffer_opened, m_scancode_event_buffer_opened_len);
            m_scancode_event_buffer_opened_len = 0;
        }
    }

    void flush_closed_events()
    {
        if (m_scancode_event_buffer_closed_len > 0)
        {
            onSwitchClosed(m_scancode_event_buffer_closed, m_scancode_event_buffer_closed_len);
            m_scancode_event_buffer_closed_len = 0;
        }
    }

    static constexpr bool is_closed(const uint8_t sample_mask, const uint8_t sample_buffer)
    {
        return (sample_mask & sample_buffer) == sample_mask;
    }

    static constexpr bool is_open(const uint8_t sample_mask, const uint8_t sample_buffer)
    {
        return (sample_mask & sample_buffer) == 0;
    }

    static constexpr uint8_t reset_sample_count(const SwitchDef& swtch)
    {
        return (DebounceSampleMask & swtch.sample_buffer);
    }

    void handleSoftwareDebounce(SwitchDef& swtch, bool is_switch_pressed)
    {
        const uint8_t settle_count = (swtch.sample_buffer & DebounceSettleMask) >> DebounceSettleShift;
        if (settle_count < DebounceSettleCount)
        {
            // We're still in the settle window. Increment the settle count.
            swtch.sample_buffer =
                ((settle_count + 1) << DebounceSettleShift) | (DebounceSampleMask & swtch.sample_buffer);
        }
        else
        {
            swtch.sample_buffer = (settle_count << DebounceSettleShift) |
                                  (DebounceSampleMask & (swtch.sample_buffer << 1)) | ((is_switch_pressed) ? 1 : 0);
        }
    }

    bool handleSwitchState(SwitchDef& swtch)
    {
        bool pending_event = false;
        if (is_closed(DebounceSampleMask, swtch.sample_buffer) && swtch.state != SwitchState::CLOSED)
        {
            swtch.state                                                          = SwitchState::CLOSED;
            m_scancode_event_buffer_closed[m_scancode_event_buffer_closed_len++] = swtch.scancode;
            swtch.sample_buffer                                                  = reset_sample_count(swtch);
            pending_event                                                        = true;
        }
        else if (is_open(DebounceSampleMask, swtch.sample_buffer) && swtch.state != SwitchState::OPEN)
        {
            const SwitchState oldState = swtch.state;
            swtch.state                = SwitchState::OPEN;
            if (oldState == SwitchState::UNKNOWN)
            {
                m_scancode_event_buffer_opened[m_scancode_event_buffer_opened_len++] = swtch.scancode;
                pending_event                                                        = true;
            }
            swtch.sample_buffer = reset_sample_count(swtch);
        }
        return pending_event;
    }

    void onSwitchClosed(const uint16_t (&scancodes)[EVENT_BUFFER_SIZE], size_t scancodes_len)
    {
        const SwitchHandler switchhandler_closed = m_switchhandler_closed;
        if (switchhandler_closed != nullptr)
        {
            switchhandler_closed(scancodes, scancodes_len);
        }
    }

    void onSwitchOpen(const uint16_t (&scancodes)[EVENT_BUFFER_SIZE], size_t scancodes_len)
    {
        const SwitchHandler switchhandler_open = m_switchhandler_open;
        if (switchhandler_open != nullptr)
        {
            switchhandler_open(scancodes, scancodes_len);
        }
    }

    SwitchDef     m_switch_map[ROW_COUNT][COL_COUNT];
    uint8_t       m_row_pins[ROW_COUNT];
    uint8_t       m_col_pins[COL_COUNT];
    SwitchHandler m_switchhandler_closed;
    SwitchHandler m_switchhandler_open;
    const uint8_t m_column_input_type;
    const bool    m_enable_software_debounce;
    uint16_t      m_scancode_event_buffer_opened[EVENT_BUFFER_SIZE];
    size_t        m_scancode_event_buffer_opened_len;
    uint16_t      m_scancode_event_buffer_closed[EVENT_BUFFER_SIZE];
    size_t        m_scancode_event_buffer_closed_len;
};

};  // namespace thirtytwobits
};  // namespace gh

#endif  // GH_THIRTYTWOBITS_SWITCH_MATRIX_SCANNER_H
