#include "calcengine.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <utility>

namespace {

constexpr std::size_t kMaxInputDigits = 15;
constexpr double kRoundingScale = 10'000'000'000.0; // 10 decimal places

double roundToPrecision(double value)
{
    if (!std::isfinite(value)) {
        return value;
    }

    // Avoid overflowing while scaling. Very large finite values are already
    // beyond any useful fractional precision.
    if (std::abs(value) > std::numeric_limits<double>::max() / kRoundingScale) {
        return value;
    }

    double rounded = std::round(value * kRoundingScale) / kRoundingScale;
    if (rounded == 0.0) {
        rounded = 0.0; // Normalise negative zero.
    }
    return rounded;
}

std::string formatNumber(double value)
{
    if (value == 0.0) {
        value = 0.0;
    }

    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(15) << std::defaultfloat << value;
    return stream.str();
}

std::string operationSymbol(calc::BinaryOperation operation)
{
    switch (operation) {
    case calc::BinaryOperation::Add:
        return "+";
    case calc::BinaryOperation::Subtract:
        return "\xE2\x88\x92"; // U+2212 MINUS SIGN
    case calc::BinaryOperation::Multiply:
        return "\xC3\x97"; // U+00D7 MULTIPLICATION SIGN
    case calc::BinaryOperation::Divide:
        return "\xC3\xB7"; // U+00F7 DIVISION SIGN
    }
    return "?";
}

std::size_t countDigits(const std::string& text)
{
    return static_cast<std::size_t>(std::count_if(
        text.cbegin(), text.cend(), [](char character) {
            return character >= '0' && character <= '9';
        }));
}

} // namespace

namespace calc {

void CalcEngine::inputDigit(int digit)
{
    if (hasError() || digit < 0 || digit > 9) {
        return;
    }

    if (!enteringNumber_) {
        prepareForNewNumber();
        display_ = "0";
    }

    const char character = static_cast<char>('0' + digit);
    if (display_ == "0") {
        if (digit != 0) {
            display_ = character;
        }
    } else if (countDigits(display_) < kMaxInputDigits) {
        display_.push_back(character);
    }

    enteringNumber_ = true;
    justEvaluated_ = false;
    if (pendingOperation_) {
        hasPendingOperand_ = true;
    }
}

void CalcEngine::inputDecimalPoint()
{
    if (hasError()) {
        return;
    }

    if (!enteringNumber_) {
        prepareForNewNumber();
        display_ = "0";
    }

    if (display_.find('.') == std::string::npos) {
        display_.push_back('.');
    }

    enteringNumber_ = true;
    justEvaluated_ = false;
    if (pendingOperation_) {
        hasPendingOperand_ = true;
    }
}

void CalcEngine::inputOperation(BinaryOperation operation)
{
    if (hasError()) {
        return;
    }

    if (pendingOperation_) {
        if (hasPendingOperand_) {
            double right = 0.0;
            if (!readDisplay(right)) {
                return;
            }

            const double left = accumulator_;
            double result = 0.0;
            if (!calculate(left, right, *pendingOperation_, result)) {
                return;
            }

            history_ = formatNumber(left) + " " + operationSymbol(*pendingOperation_)
                + " " + formatNumber(right) + " =";
            accumulator_ = result;
            display_ = formatNumber(result);
            pendingOperation_ = operation;
            hasPendingOperand_ = false;
            enteringNumber_ = false;
            justEvaluated_ = false;
            ++completedCalculations_;
        } else {
            // The user changed their mind before typing the right-hand operand.
            pendingOperation_ = operation;
            display_ = formatNumber(accumulator_);
            enteringNumber_ = false;
            justEvaluated_ = false;
            history_ = formatNumber(accumulator_) + " " + operationSymbol(operation);
        }
        return;
    }

    double current = 0.0;
    if (!readDisplay(current)) {
        return;
    }

    accumulator_ = current;
    pendingOperation_ = operation;
    hasPendingOperand_ = false;
    enteringNumber_ = false;
    justEvaluated_ = false;
    display_ = formatNumber(current);
    history_ = formatNumber(current) + " " + operationSymbol(operation);
}

void CalcEngine::evaluate()
{
    if (hasError() || !pendingOperation_) {
        return;
    }

    double right = 0.0;
    if (!readDisplay(right)) {
        return;
    }

    const double left = accumulator_;
    double result = 0.0;
    if (!calculate(left, right, *pendingOperation_, result)) {
        return;
    }

    history_ = formatNumber(left) + " " + operationSymbol(*pendingOperation_)
        + " " + formatNumber(right) + " =";
    display_ = formatNumber(result);
    accumulator_ = result;
    pendingOperation_.reset();
    hasPendingOperand_ = false;
    enteringNumber_ = false;
    justEvaluated_ = true;
    ++completedCalculations_;
}

void CalcEngine::applyPercent()
{
    if (hasError()) {
        return;
    }

    double current = 0.0;
    if (!readDisplay(current)) {
        return;
    }

    double value = current / 100.0;

    // For addition and subtraction, a percentage is relative to the pending
    // left-hand operand (200 + 10% = 220), matching common desktop calculators.
    if (pendingOperation_ && hasPendingOperand_
        && (*pendingOperation_ == BinaryOperation::Add
            || *pendingOperation_ == BinaryOperation::Subtract)) {
        value = (accumulator_ * current) / 100.0;
    }

    if (!std::isfinite(value)) {
        setError(ErrorCode::Overflow);
        return;
    }

    value = roundToPrecision(value);
    display_ = formatNumber(value);
    enteringNumber_ = false;
    justEvaluated_ = false;
    if (pendingOperation_) {
        hasPendingOperand_ = true;
    }
}

void CalcEngine::backspace()
{
    if (hasError() || !enteringNumber_) {
        return;
    }

    if (display_.size() <= 1) {
        display_ = "0";
        return;
    }

    display_.pop_back();
    if (display_.empty() || display_ == "-") {
        display_ = "0";
    }
}

void CalcEngine::clearEntry()
{
    if (hasError()) {
        clearAll();
        return;
    }

    display_ = "0";
    enteringNumber_ = false;
    justEvaluated_ = false;
    hasPendingOperand_ = false;

    if (!pendingOperation_) {
        accumulator_ = 0.0;
    }
}

void CalcEngine::clearAll()
{
    reset();
}

void CalcEngine::setResult(double value, std::string history)
{
    if (!std::isfinite(value)) {
        setError(ErrorCode::Overflow);
        return;
    }

    display_ = formatNumber(value);
    history_ = std::move(history);
    accumulator_ = value;
    pendingOperation_.reset();
    enteringNumber_ = false;
    hasPendingOperand_ = false;
    justEvaluated_ = true;
    errorCode_ = ErrorCode::None;
    ++completedCalculations_;
}

void CalcEngine::prepareForNewNumber()
{
    if (justEvaluated_ || !pendingOperation_) {
        accumulator_ = 0.0;
        pendingOperation_.reset();
        hasPendingOperand_ = false;
        history_.clear();
        justEvaluated_ = false;
    }
    enteringNumber_ = true;
}

bool CalcEngine::readDisplay(double& value)
{
    std::istringstream stream(display_);
    stream.imbue(std::locale::classic());

    double parsed = 0.0;
    stream >> parsed;
    if (stream.fail()) {
        setError(ErrorCode::InvalidInput);
        return false;
    }

    char trailing = '\0';
    if (stream >> trailing) {
        setError(ErrorCode::InvalidInput);
        return false;
    }

    if (!std::isfinite(parsed)) {
        setError(ErrorCode::Overflow);
        return false;
    }

    value = parsed;
    return true;
}

bool CalcEngine::calculate(
    double left, double right, BinaryOperation operation, double& result)
{
    if (operation == BinaryOperation::Divide && right == 0.0) {
        setError(ErrorCode::DivisionByZero);
        return false;
    }

    double rawResult = 0.0;
    switch (operation) {
    case BinaryOperation::Add:
        rawResult = left + right;
        break;
    case BinaryOperation::Subtract:
        rawResult = left - right;
        break;
    case BinaryOperation::Multiply:
        rawResult = left * right;
        break;
    case BinaryOperation::Divide:
        rawResult = left / right;
        break;
    }

    if (!std::isfinite(rawResult)) {
        setError(ErrorCode::Overflow);
        return false;
    }

    result = roundToPrecision(rawResult);
    return true;
}

void CalcEngine::setError(ErrorCode code)
{
    errorCode_ = code;
    display_ = "Error";
    pendingOperation_.reset();
    accumulator_ = 0.0;
    enteringNumber_ = false;
    hasPendingOperand_ = false;
    justEvaluated_ = false;

    switch (code) {
    case ErrorCode::DivisionByZero:
        history_ = "Cannot divide by zero";
        break;
    case ErrorCode::Overflow:
        history_ = "Result is out of range";
        break;
    case ErrorCode::InvalidInput:
        history_ = "Invalid input";
        break;
    case ErrorCode::None:
        history_.clear();
        break;
    }
}

void CalcEngine::reset()
{
    display_ = "0";
    history_.clear();
    accumulator_ = 0.0;
    pendingOperation_.reset();
    enteringNumber_ = false;
    hasPendingOperand_ = false;
    justEvaluated_ = false;
    errorCode_ = ErrorCode::None;
}

} // namespace calc
