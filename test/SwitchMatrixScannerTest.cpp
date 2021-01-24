
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
#define LOW 4
#define HIGH 5

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
        current_test = nullptr;
    }

    virtual void SetUp() override
    {
        current_test = this;
        memset(pin_state, 0, sizeof(pin_state));
        for (size_t i = 0; i < T::col_count; ++i)
        {
            col[i] = i;
        }
        for (size_t i = 0; i < T::row_count; ++i)
        {
            row[i] = i + T::col_count;
        }
        mock_state.reset(new ::testing::NiceMock<MockArduinoState>());
        ON_CALL(*mock_state, digitalRead(_)).WillByDefault(Return(HIGH));
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
TYPED_TEST_P(SwitchMatrixScannerTest, KeyUP)
{
    TypeParam         test_subject(this->row, this->col, true, false);
    MockArduinoState* mock;
    this->get_mock(mock);
    EXPECT_CALL(*mock, digitalRead(this->col[0]))
        .Times(TypeParam::row_count)
        .WillRepeatedly(Return(LOW));
    EXPECT_CALL(*mock, digitalRead(::testing::Ne(this->col[0])))
        .WillRepeatedly(Return(HIGH));
    test_subject.setup(SwitchMatrixScannerTest<TypeParam>::onSwitchClosed,
                       SwitchMatrixScannerTest<TypeParam>::onSwitchOpen);
    test_subject.scan();
    ASSERT_TRUE(test_subject.isSwitchClosed(1));
}

TYPED_TEST_P(SwitchMatrixScannerTest, SetupPullups)
{
    TypeParam         test_subject(this->row, this->col, true);
    MockArduinoState* mock;
    this->get_mock(mock);
    for (size_t i = 0; i < TypeParam::col_count; ++i)
    {
        EXPECT_CALL(*mock, pinMode(this->col[i], INPUT_PULLUP));
    }
    for (size_t i = 0; i < TypeParam::row_count; ++i)
    {
        EXPECT_CALL(*mock, pinMode(this->row[i], INPUT));
    }
    test_subject.setup();
}

TYPED_TEST_P(SwitchMatrixScannerTest, SetupNoPullups)
{
    TypeParam         test_subject(this->row, this->col, false);
    MockArduinoState* mock;
    this->get_mock(mock);
    for (size_t i = 0; i < TypeParam::col_count; ++i)
    {
        EXPECT_CALL(*mock, pinMode(this->col[i], INPUT));
    }
    for (size_t i = 0; i < TypeParam::row_count; ++i)
    {
        EXPECT_CALL(*mock, pinMode(this->row[i], INPUT));
    }
    test_subject.setup();
}

// +--------------------------------------------------------------------------+
REGISTER_TYPED_TEST_SUITE_P(SwitchMatrixScannerTest, SetupPullups, SetupNoPullups, KeyUP);

using MatrixTestTypes = ::testing::Types<gh::thirtytwobits::SwitchMatrixScanner<1, 1>,
                                         gh::thirtytwobits::SwitchMatrixScanner<2, 3>>;

INSTANTIATE_TYPED_TEST_SUITE_P(My, SwitchMatrixScannerTest, MatrixTestTypes);
