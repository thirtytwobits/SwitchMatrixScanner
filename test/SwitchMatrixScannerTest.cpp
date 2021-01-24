
#include "gmock/gmock.h"
#include <memory>

using ::testing::Return;
using ::testing::_;

// +--------------------------------------------------------------------------+
// | Arduino mocking
// +--------------------------------------------------------------------------+

#define INPUT 1
#define INPUT_PULLUP 2
#define OUTPUT 3
#define LOW 0

void digitalWrite(int pin, int level);

int digitalRead(int pin);

void pinMode(int pin, int mode);

#include "SwitchMatrixScanner.h"

struct ArduinoState
{
    virtual ~ArduinoState()                       = default;
    virtual int  digitalRead(int pin) const       = 0;
    virtual void digitalWrite(int pin, int state) = 0;
    virtual int  getPinMode(int pin) const        = 0;
    virtual void pinMode(int pin, int mode)       = 0;
};

// +--------------------------------------------------------------------------+
// | TEST FIXTURE
// +--------------------------------------------------------------------------+

class MockArduinoState : public ArduinoState
{
public:
    virtual ~MockArduinoState(){};
    MOCK_METHOD(int, digitalRead, (int pin), (const, override));
    MOCK_METHOD(void, digitalWrite, (int pin, int state), (override));
    MOCK_METHOD(int, getPinMode, (int pin), (const, override));
    MOCK_METHOD(void, pinMode, (int pin, int mode), (override));
};

std::shared_ptr<MockArduinoState> mock_state;

template <typename T>
class SwitchMatrixScannerTest : public testing::Test
{
public:
    SwitchMatrixScannerTest()
        : col{}
        , row{}
    {
        memset(pin_state, 0, sizeof(pin_state));
        for (size_t i = 0; i < T::col_count; ++i)
        {
            col[i] = i;
        }
        for (size_t i = 0; i < T::row_count; ++i)
        {
            row[i] = i;
        }
        current_test = nullptr;
    }

    virtual void SetUp() override
    {
        current_test = this;
        mock_state.reset(new MockArduinoState());
    }

    virtual void TearDown() override
    {
        mock_state.reset();
        current_test = nullptr;
    }

    void get_mock(MockArduinoState*& out) const
    {
        out = mock_state.get();
        ASSERT_NE(out, nullptr);
    }

    uint8_t col[T::col_count];
    uint8_t row[T::row_count];

    uint8_t pin_state[T::col_count * T::row_count];

    static SwitchMatrixScannerTest<T>* current_test;

    static void onSwitchClosed(const uint16_t (&scancodes)[T::event_buffer_size], size_t scancodes_len)
    {
        ASSERT_NE(current_test, nullptr);
    }

    static void onSwitchOpen(const uint16_t (&scancodes)[T::event_buffer_size], size_t scancodes_len)
    {
        ASSERT_NE(current_test, nullptr);
    }
};

template <typename T>
SwitchMatrixScannerTest<T>* SwitchMatrixScannerTest<T>::current_test = nullptr;

void digitalWrite(int pin, int level)
{
    ArduinoState* const mock = mock_state.get();
    ASSERT_NE(mock, nullptr);
    mock->digitalWrite(pin, level);
}

int digitalRead(int pin)
{
    return mock_state->digitalRead(pin);
}

void pinMode(int pin, int mode)
{
    ArduinoState* const mock = mock_state.get();
    ASSERT_NE(mock, nullptr);
    mock->pinMode(pin, mode);
}

TYPED_TEST_SUITE_P(SwitchMatrixScannerTest);

// +--------------------------------------------------------------------------+
// | TESTS
// +--------------------------------------------------------------------------+
/**
 * Verify that we've setup the matrix correctly.
 */
TYPED_TEST_P(SwitchMatrixScannerTest, Setup)
{
    TypeParam         test_subject(this->row, this->col);
    MockArduinoState* mock;
    this->get_mock(mock);
    EXPECT_CALL(*mock, pinMode(_, _)).Times(TypeParam::col_count + TypeParam::row_count);
    test_subject.setup(SwitchMatrixScannerTest<TypeParam>::onSwitchClosed,
                       SwitchMatrixScannerTest<TypeParam>::onSwitchOpen);
}

// +--------------------------------------------------------------------------+
REGISTER_TYPED_TEST_SUITE_P(SwitchMatrixScannerTest, Setup);

using MatrixTestTypes = ::testing::Types<gh::thirtytwobits::SwitchMatrixScanner<1, 1>,
                                         gh::thirtytwobits::SwitchMatrixScanner<2, 2>,
                                         gh::thirtytwobits::SwitchMatrixScanner<3, 104>>;

INSTANTIATE_TYPED_TEST_SUITE_P(My, SwitchMatrixScannerTest, MatrixTestTypes);
