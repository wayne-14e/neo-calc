#pragma once

#include <QString>

namespace formula {

struct Evaluation {
    bool ok = false;
    double value = 0.0;
    QString error;
};

// Evaluates an ASCII math expression using x as the current value.
// Supported: + - * / ^, parentheses, x, pi, e, and the functions
// sqrt, abs, sin, cos, tan, log, ln, exp.
[[nodiscard]] Evaluation evaluate(const QString& expression, double x);

} // namespace formula
