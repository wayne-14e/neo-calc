#include "formula.h"

#include <QLocale>
#include <QString>

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace {

class Parser final {
public:
    Parser(std::string_view source, double x)
        : source_(source)
        , x_(x)
    {
    }

    formula::Evaluation run()
    {
        skipSpaces();
        if (atEnd()) {
            return fail(QStringLiteral("EMPTY EXPRESSION"));
        }

        const double result = parseAdditive();
        if (!ok_) {
            return {false, 0.0, error_};
        }

        skipSpaces();
        if (!atEnd()) {
            return fail(QStringLiteral("UNEXPECTED '%1'").arg(peek()));
        }
        if (!std::isfinite(result)) {
            return fail(QStringLiteral("RESULT OUT OF RANGE"));
        }

        return {true, result, {}};
    }

private:
    double parseAdditive()
    {
        double value = parseMultiplicative();

        while (ok_) {
            skipSpaces();
            const char operation = peek();
            if (operation != '+' && operation != '-') {
                break;
            }
            consume();

            const double right = parseMultiplicative();
            if (!ok_) {
                return 0.0;
            }

            value = operation == '+' ? value + right : value - right;
            if (!std::isfinite(value)) {
                setError(QStringLiteral("RESULT OUT OF RANGE"));
            }
        }

        return value;
    }

    double parseMultiplicative()
    {
        double value = parseUnary();

        while (ok_) {
            skipSpaces();
            const char operation = peek();
            if (operation != '*' && operation != '/') {
                break;
            }
            consume();

            const double right = parseUnary();
            if (!ok_) {
                return 0.0;
            }
            if (operation == '/' && right == 0.0) {
                setError(QStringLiteral("DIVISION BY ZERO"));
                return 0.0;
            }

            value = operation == '*' ? value * right : value / right;
            if (!std::isfinite(value)) {
                setError(QStringLiteral("RESULT OUT OF RANGE"));
            }
        }

        return value;
    }

    double parseUnary()
    {
        skipSpaces();
        if (peek() == '+' || peek() == '-') {
            const char sign = consume();
            const double value = parseUnary();
            return sign == '-' ? -value : value;
        }
        return parsePower();
    }

    double parsePower()
    {
        const double base = parsePrimary();
        if (!ok_) {
            return 0.0;
        }

        skipSpaces();
        if (peek() != '^') {
            return base;
        }
        consume();

        const double exponent = parseUnary();
        if (!ok_) {
            return 0.0;
        }

        const double result = std::pow(base, exponent);
        if (!std::isfinite(result)) {
            setError(QStringLiteral("RESULT OUT OF RANGE"));
            return 0.0;
        }
        return result;
    }

    double parsePrimary()
    {
        skipSpaces();
        if (atEnd()) {
            setError(QStringLiteral("MISSING VALUE"));
            return 0.0;
        }

        const char token = peek();
        if (token == '(') {
            consume();
            const double value = parseAdditive();
            if (!ok_) {
                return 0.0;
            }
            skipSpaces();
            if (peek() != ')') {
                setError(QStringLiteral("MISSING ')'"));
                return 0.0;
            }
            consume();
            return value;
        }

        if (isDigit(token) || token == '.') {
            return parseNumber();
        }
        if (isLetter(token)) {
            return parseIdentifier();
        }

        setError(QStringLiteral("UNEXPECTED '%1'").arg(token));
        return 0.0;
    }

    double parseNumber()
    {
        const std::size_t start = position_;
        std::size_t end = start;
        bool sawDigit = false;
        bool sawDot = false;
        bool sawExponent = false;
        std::size_t exponentDigits = 0;

        while (end < source_.size()) {
            const char character = source_[end];
            if (isDigit(character)) {
                sawDigit = true;
                if (sawExponent) {
                    ++exponentDigits;
                }
                ++end;
                continue;
            }
            if (character == '.' && !sawDot && !sawExponent) {
                sawDot = true;
                ++end;
                continue;
            }
            if ((character == 'e' || character == 'E') && !sawExponent
                && sawDigit) {
                sawExponent = true;
                ++end;
                if (end < source_.size()
                    && (source_[end] == '+' || source_[end] == '-')) {
                    ++end;
                }
                continue;
            }
            break;
        }

        if (!sawDigit || (sawExponent && exponentDigits == 0)) {
            setError(QStringLiteral("INVALID NUMBER"));
            return 0.0;
        }

        // QLocale::c() keeps the parser's decimal point and exponent handling
        // deterministic, unlike std::from_chars for floating point which is not
        // available in Apple's libc++ yet.
        const QString token = QString::fromLatin1(source_.data() + start,
            static_cast<qsizetype>(end - start));
        bool parsed = false;
        const double value = QLocale::c().toDouble(token, &parsed);
        if (!parsed || !std::isfinite(value)) {
            setError(QStringLiteral("INVALID NUMBER"));
            return 0.0;
        }

        position_ = end;
        return value;
    }

    double parseIdentifier()
    {
        std::string name;
        while (!atEnd() && isLetter(peek())) {
            const char letter = consume();
            name.push_back(letter >= 'A' && letter <= 'Z'
                    ? static_cast<char>(letter - 'A' + 'a')
                    : letter);
        }

        if (name == "x") {
            return x_;
        }
        if (name == "pi") {
            return 3.14159265358979323846;
        }
        if (name == "e") {
            return 2.71828182845904523536;
        }

        skipSpaces();
        if (peek() != '(') {
            setError(QStringLiteral("EXPECTED '(' AFTER %1")
                         .arg(QString::fromStdString(name).toUpper()));
            return 0.0;
        }
        consume();

        const double argument = parseAdditive();
        if (!ok_) {
            return 0.0;
        }
        skipSpaces();
        if (peek() != ')') {
            setError(QStringLiteral("MISSING ')'"));
            return 0.0;
        }
        consume();

        double result = 0.0;
        if (name == "sqrt") {
            if (argument < 0.0) {
                setError(QStringLiteral("SQRT OF NEGATIVE"));
                return 0.0;
            }
            result = std::sqrt(argument);
        } else if (name == "abs") {
            result = std::abs(argument);
        } else if (name == "sin") {
            result = std::sin(argument);
        } else if (name == "cos") {
            result = std::cos(argument);
        } else if (name == "tan") {
            result = std::tan(argument);
        } else if (name == "log") {
            result = std::log10(argument);
        } else if (name == "ln") {
            result = std::log(argument);
        } else if (name == "exp") {
            result = std::exp(argument);
        } else {
            setError(QStringLiteral("UNKNOWN FUNCTION %1")
                         .arg(QString::fromStdString(name).toUpper()));
            return 0.0;
        }

        if (!std::isfinite(result)) {
            setError(QStringLiteral("RESULT OUT OF RANGE"));
            return 0.0;
        }
        return result;
    }

    formula::Evaluation fail(const QString& message)
    {
        setError(message);
        return {false, 0.0, error_};
    }

    void setError(const QString& message)
    {
        ok_ = false;
        error_ = message;
    }

    void skipSpaces()
    {
        while (!atEnd() && (peek() == ' ' || peek() == '\t')) {
            ++position_;
        }
    }

    bool atEnd() const noexcept { return position_ >= source_.size(); }

    char peek() const noexcept
    {
        return atEnd() ? '\0' : source_[position_];
    }

    char consume() noexcept
    {
        return atEnd() ? '\0' : source_[position_++];
    }

    static bool isDigit(char character) noexcept
    {
        return character >= '0' && character <= '9';
    }

    static bool isLetter(char character) noexcept
    {
        return (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z');
    }

    std::string_view source_;
    double x_ = 0.0;
    std::size_t position_ = 0;
    bool ok_ = true;
    QString error_;
};

} // namespace

namespace formula {

Evaluation evaluate(const QString& expression, double x)
{
    const std::string source = expression.trimmed().toStdString();
    return Parser(source, x).run();
}

} // namespace formula
