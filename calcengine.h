#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace calc {

enum class BinaryOperation {
    Add,
    Subtract,
    Multiply,
    Divide
};

enum class ErrorCode {
    None,
    DivisionByZero,
    Overflow,
    InvalidInput
};

class CalcEngine final {
public:
    CalcEngine() = default;

    void inputDigit(int digit);
    void inputDecimalPoint();
    void inputOperation(BinaryOperation operation);
    void evaluate();
    void applyPercent();
    void backspace();
    void clearEntry();
    void clearAll();
    void setResult(double value, std::string history);

    [[nodiscard]] const std::string& display() const noexcept { return display_; }
    [[nodiscard]] const std::string& history() const noexcept { return history_; }
    [[nodiscard]] ErrorCode errorCode() const noexcept { return errorCode_; }
    [[nodiscard]] bool hasError() const noexcept { return errorCode_ != ErrorCode::None; }
    [[nodiscard]] std::uint64_t completedCalculations() const noexcept
    {
        return completedCalculations_;
    }

private:
    void prepareForNewNumber();
    bool readDisplay(double& value);
    bool calculate(double left, double right, BinaryOperation operation, double& result);
    void setError(ErrorCode code);
    void reset();

    std::string display_ = "0";
    std::string history_;
    double accumulator_ = 0.0;

    std::optional<BinaryOperation> pendingOperation_;
    bool enteringNumber_ = false;
    bool hasPendingOperand_ = false;
    bool justEvaluated_ = false;
    ErrorCode errorCode_ = ErrorCode::None;
    std::uint64_t completedCalculations_ = 0;
};

} // namespace calc
