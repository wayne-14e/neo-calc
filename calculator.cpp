#include "calculator.h"
#include "formula.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int kMinimumWindowWidth = 350;
constexpr int kMinimumWindowHeight = 620;
constexpr int kInitialWindowWidth = 420;
constexpr int kInitialWindowHeight = 740;
constexpr int kMinimumDisplayFontSize = 18;
constexpr int kMaximumDisplayFontSize = 56;

struct FormulaPreset {
    const char* label;
    const char* expression;
    const char* hint;
};

constexpr std::array<FormulaPreset, 6> kFormulaPresets = {{
    {"TAX +10%", "x*1.10", "Add 10% tax"},
    {"TAX -10%", "x/1.10", "Remove 10% tax"},
    {"USD>EUR", "x*0.92", "US dollars to euros at 0.92"},
    {"EUR>USD", "x*1.09", "Euros to US dollars at 1.09"},
    {"E=MC^2", "x*299792458^2", "Mass in kg to energy in joules"},
    {"KG>LB", "x*2.2046226218", "Kilograms to pounds"},
}};

// The display panel is painted like a CRT terminal: a sparse binary
// pattern sits behind the labels and mode views.
class TerminalDisplayPanel final : public QFrame {
public:
    explicit TerminalDisplayPanel(QWidget* parent = nullptr)
        : QFrame(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QFrame::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setClipRect(contentsRect());

        QFont font = painter.font();
        font.setPixelSize(9);
        painter.setFont(font);
        painter.setPen(QColor(116, 184, 28, 26));

        const QRect area = contentsRect();
        for (int y = area.top() + 6; y < area.bottom(); y += 11) {
            for (int x = area.left() + 4; x < area.right(); x += 15) {
                const quint32 seed = static_cast<quint32>(x * 73856093)
                    ^ static_cast<quint32>(y * 19349663);
                if ((seed % 7) < 2) {
                    painter.drawText(
                        x, y, (seed & 1) ? QStringLiteral("0")
                                         : QStringLiteral("1"));
                }
            }
        }
    }
};

enum class DeveloperConversionStatus {
    Ok,
    NoNumber,
    NotAnInteger,
    OutOfRange
};

struct DeveloperConversion {
    QString decimal;
    QString binary;
    QString hexadecimal;
    QString bits;
};

// Binary and hex use sign-magnitude notation. BITS is a fixed-width
// two's-complement pattern, sized to the smallest byte width that fits.
int bitWidthFor(qint64 value)
{
    if (value >= 0) {
        if (value <= 0xFF) {
            return 8;
        }
        if (value <= 0xFFFF) {
            return 16;
        }
        if (value <= 0xFFFFFFFFLL) {
            return 32;
        }
        return 64;
    }

    if (value >= -128) {
        return 8;
    }
    if (value >= -32768) {
        return 16;
    }
    if (value >= -2147483648LL) {
        return 32;
    }
    return 64;
}

QString groupEveryFourBits(const QString& bits)
{
    QString grouped;
    grouped.reserve(bits.size() + bits.size() / 4);

    for (qsizetype index = 0; index < bits.size(); ++index) {
        if (index > 0 && (bits.size() - index) % 4 == 0) {
            grouped.append(QLatin1Char(' '));
        }
        grouped.append(bits.at(index));
    }

    return grouped;
}

DeveloperConversionStatus convertForDeveloper(
    const QString& displayText, DeveloperConversion& conversion)
{
    conversion = {};
    conversion.decimal = displayText;

    bool parsed = false;
    const double number = QLocale::c().toDouble(displayText, &parsed);
    if (!parsed || !std::isfinite(number)) {
        return DeveloperConversionStatus::NoNumber;
    }
    if (std::trunc(number) != number) {
        return DeveloperConversionStatus::NotAnInteger;
    }
    if (number < -9223372036854775808.0
        || number >= 9223372036854775808.0) {
        return DeveloperConversionStatus::OutOfRange;
    }

    const qint64 value = static_cast<qint64>(number);
    const int bitWidth = bitWidthFor(value);
    const quint64 magnitude = value < 0
        ? static_cast<quint64>(-(value + 1)) + 1
        : static_cast<quint64>(value);

    const QString binaryDigits = QString::number(magnitude, 2);
    const QString paddedBits = QString::number(static_cast<quint64>(value), 2)
                                   .right(bitWidth)
                                   .rightJustified(bitWidth, QLatin1Char('0'));
    const QString hexDigits = QString::number(magnitude, 16)
                                  .toUpper()
                                  .rightJustified(bitWidth / 4, QLatin1Char('0'));

    const QString sign = value < 0 ? QStringLiteral("-") : QString();
    conversion.binary
        = sign + QStringLiteral("0b") + groupEveryFourBits(binaryDigits);
    conversion.hexadecimal = sign + QStringLiteral("0x") + hexDigits;
    conversion.bits = groupEveryFourBits(paddedBits);

    return DeveloperConversionStatus::Ok;
}

} // namespace

Calculator::Calculator(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("calculatorWindow"));
    setWindowTitle(tr("Neo"));
    setMinimumSize(kMinimumWindowWidth, kMinimumWindowHeight);
    resize(kInitialWindowWidth, kInitialWindowHeight);
    setFocusPolicy(Qt::StrongFocus);

    initializeHistoryLog();
    buildUi();
    refreshUi();
}

void Calculator::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(10);

    auto* statusBar = new QFrame(this);
    statusBar->setObjectName(QStringLiteral("statusBar"));
    statusBar->setFixedHeight(28);

    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(10, 3, 10, 3);
    statusLayout->setSpacing(8);

    auto* brandLabel = new QLabel(QStringLiteral("NEO//CALC"), statusBar);
    brandLabel->setObjectName(QStringLiteral("brandLabel"));
    brandLabel->setAccessibleName(tr("Neo calculator"));
    if (!historyFilePath_.isEmpty()) {
        brandLabel->setToolTip(
            tr("Calculation history: %1").arg(historyFilePath_));
        brandLabel->setAccessibleDescription(
            tr("Calculations are saved to %1").arg(historyFilePath_));
    }

    modeLabel_ = new QLabel(QStringLiteral("READY"), statusBar);
    modeLabel_->setObjectName(QStringLiteral("modeLabel"));
    modeLabel_->setAccessibleName(tr("Mode status"));

    developerButton_ = new QPushButton(tr("DEV"), statusBar);
    developerButton_->setObjectName(QStringLiteral("button_developer"));
    developerButton_->setCheckable(true);
    developerButton_->setFocusPolicy(Qt::NoFocus);
    developerButton_->setCursor(Qt::PointingHandCursor);
    developerButton_->setFixedHeight(20);
    developerButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    developerButton_->setAccessibleName(tr("Toggle developer conversions"));
    developerButton_->setAccessibleDescription(
        tr("Displays decimal, binary, hexadecimal, and bit representations "
           "of the current value."));
    developerButton_->setToolTip(
        tr("Show binary, hex, and bit conversions (Ctrl+D)"));
    connect(developerButton_, &QPushButton::toggled, this,
        &Calculator::onDeveloperToggled);

    formulaButton_ = new QPushButton(tr("FX"), statusBar);
    formulaButton_->setObjectName(QStringLiteral("button_formula"));
    formulaButton_->setCheckable(true);
    formulaButton_->setFocusPolicy(Qt::NoFocus);
    formulaButton_->setCursor(Qt::PointingHandCursor);
    formulaButton_->setFixedHeight(20);
    formulaButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    formulaButton_->setAccessibleName(tr("Toggle custom formulas"));
    formulaButton_->setAccessibleDescription(
        tr("Applies tax, currency, physics, and custom formulas to the "
           "current value."));
    formulaButton_->setToolTip(tr("Open the formula bank (Ctrl+F)"));
    connect(formulaButton_, &QPushButton::toggled, this,
        &Calculator::onFormulaToggled);

    statusLayout->addWidget(brandLabel);
    statusLayout->addWidget(modeLabel_);
    statusLayout->addStretch(1);
    statusLayout->addWidget(developerButton_);
    statusLayout->addWidget(formulaButton_);
    rootLayout->addWidget(statusBar);

    displayPanel_ = new TerminalDisplayPanel(this);
    displayPanel_->setObjectName(QStringLiteral("displayPanel"));
    displayPanel_->setMinimumHeight(176);
    displayPanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    displayPanel_->setAccessibleName(tr("Display panel"));

    auto* displayLayout = new QVBoxLayout(displayPanel_);
    displayLayout->setContentsMargins(16, 12, 16, 14);
    displayLayout->setSpacing(4);

    historyLabel_ = new QLabel(QStringLiteral("> READY"), displayPanel_);
    historyLabel_->setObjectName(QStringLiteral("historyLabel"));
    historyLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    historyLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    historyLabel_->setMinimumHeight(18);
    historyLabel_->setAccessibleName(tr("Calculation history"));

    displayLabel_ = new QLabel(QStringLiteral("0"), displayPanel_);
    displayLabel_->setObjectName(QStringLiteral("displayLabel"));
    displayLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    displayLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    displayLabel_->setMinimumHeight(68);
    displayLabel_->setAccessibleName(tr("Display"));

    developerView_ = new QWidget(displayPanel_);
    developerView_->setObjectName(QStringLiteral("developerView"));
    developerView_->setMinimumHeight(76);
    developerView_->setAccessibleName(tr("Developer conversions"));
    developerView_->hide();

    auto* developerViewLayout = new QVBoxLayout(developerView_);
    developerViewLayout->setContentsMargins(0, 0, 0, 0);
    developerViewLayout->setSpacing(4);

    developerGrid_ = new QWidget(developerView_);
    developerGrid_->setObjectName(QStringLiteral("developerGrid"));

    auto* developerGridLayout = new QGridLayout(developerGrid_);
    developerGridLayout->setContentsMargins(0, 0, 0, 0);
    developerGridLayout->setHorizontalSpacing(10);
    developerGridLayout->setVerticalSpacing(2);

    QFont developerValueFont
        = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    developerValueFont.setFamilies({QStringLiteral("Classic Console"),
        QStringLiteral("gallant12x22"), QStringLiteral("DejaVu Sans Mono")});
    developerValueFont.setPixelSize(13);
    developerValueFont.setWeight(QFont::Medium);

    const auto addConversionRow = [this, developerGridLayout,
                                      &developerValueFont](int row,
                                      const QString& key,
                                      const QString& format,
                                      QLabel*& valueLabel) {
        auto* keyLabel = new QLabel(key, developerGrid_);
        keyLabel->setObjectName(QStringLiteral("developerKey"));
        keyLabel->setProperty("conversionKey", format);
        keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        keyLabel->setMinimumWidth(32);

        valueLabel = new QLabel(QStringLiteral("-"), developerGrid_);
        valueLabel->setObjectName(QStringLiteral("developerValue"));
        valueLabel->setProperty("conversionKey", format);
        valueLabel->setFont(developerValueFont);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        valueLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        valueLabel->setWordWrap(true);
        valueLabel->setTextFormat(Qt::PlainText);
        valueLabel->setAccessibleName(tr("%1 conversion").arg(key));

        developerGridLayout->addWidget(keyLabel, row, 0, Qt::AlignTop);
        developerGridLayout->addWidget(valueLabel, row, 1);
    };

    addConversionRow(0, tr("DEC"), QStringLiteral("dec"),
        developerDecimalLabel_);
    addConversionRow(1, tr("BIN"), QStringLiteral("bin"),
        developerBinaryLabel_);
    addConversionRow(2, tr("HEX"), QStringLiteral("hex"),
        developerHexLabel_);
    addConversionRow(3, tr("BITS"), QStringLiteral("bits"),
        developerBitsLabel_);
    developerGridLayout->setColumnStretch(1, 1);

    developerMessageLabel_ = new QLabel(developerView_);
    developerMessageLabel_->setObjectName(QStringLiteral("developerMessage"));
    developerMessageLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    developerMessageLabel_->setSizePolicy(
        QSizePolicy::Ignored, QSizePolicy::Preferred);
    developerMessageLabel_->setWordWrap(true);
    developerMessageLabel_->setTextFormat(Qt::PlainText);
    developerMessageLabel_->setAccessibleName(tr("Conversion status"));
    developerMessageLabel_->hide();

    developerViewLayout->addWidget(developerGrid_, 1);
    developerViewLayout->addWidget(developerMessageLabel_, 1);

    formulaView_ = new QWidget(displayPanel_);
    formulaView_->setObjectName(QStringLiteral("formulaView"));
    formulaView_->setAccessibleName(tr("Formula bank"));
    formulaView_->hide();

    auto* formulaViewLayout = new QVBoxLayout(formulaView_);
    formulaViewLayout->setContentsMargins(0, 0, 0, 0);
    formulaViewLayout->setSpacing(4);

    auto* formulaGrid = new QGridLayout;
    formulaGrid->setContentsMargins(0, 0, 0, 0);
    formulaGrid->setHorizontalSpacing(4);
    formulaGrid->setVerticalSpacing(4);

    int formulaIndex = 0;
    for (const FormulaPreset& preset : kFormulaPresets) {
        const QString label = QString::fromLatin1(preset.label);
        const QString expression = QString::fromLatin1(preset.expression);

        auto* button = new QPushButton(label, formulaView_);
        button->setObjectName(
            QStringLiteral("button_formula_%1").arg(formulaIndex));
        button->setProperty("buttonRole", QStringLiteral("formula"));
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(28);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setAccessibleName(tr("%1 formula").arg(label));
        button->setToolTip(tr("%1\nf(x) = %2\n%3")
                .arg(label, expression,
                    QString::fromLatin1(preset.hint)));
        connect(button, &QPushButton::clicked, this,
            [this, expression, label] { applyFormula(expression, label); });

        formulaGrid->addWidget(button, formulaIndex / 2, formulaIndex % 2);
        ++formulaIndex;
    }
    formulaGrid->setColumnStretch(0, 1);
    formulaGrid->setColumnStretch(1, 1);
    formulaViewLayout->addLayout(formulaGrid);

    auto* customRow = new QHBoxLayout;
    customRow->setContentsMargins(0, 0, 0, 0);
    customRow->setSpacing(6);

    auto* formulaPrompt = new QLabel(QStringLiteral("f(x)="), formulaView_);
    formulaPrompt->setObjectName(QStringLiteral("formulaPrompt"));

    formulaExpressionEdit_ = new QLineEdit(formulaView_);
    formulaExpressionEdit_->setObjectName(QStringLiteral("formulaExpression"));
    formulaExpressionEdit_->setText(QStringLiteral("x*1.08"));
    formulaExpressionEdit_->setPlaceholderText(QStringLiteral("x*1.08"));
    formulaExpressionEdit_->setMaxLength(64);
    formulaExpressionEdit_->setFixedHeight(26);
    formulaExpressionEdit_->setAccessibleName(tr("Custom formula"));
    formulaExpressionEdit_->setToolTip(
        tr("Use x, pi, e, + - * / ^, sqrt, abs, sin, cos, tan, log, ln, exp"));
    connect(formulaExpressionEdit_, &QLineEdit::returnPressed, this, [this] {
        const QString expression = formulaExpressionEdit_->text().trimmed();
        applyFormula(expression, tr("f(x)=%1").arg(expression));
    });

    auto* runButton = new QPushButton(tr("RUN"), formulaView_);
    runButton->setObjectName(QStringLiteral("button_formula_run"));
    runButton->setProperty("buttonRole", QStringLiteral("formulaRun"));
    runButton->setFocusPolicy(Qt::NoFocus);
    runButton->setCursor(Qt::PointingHandCursor);
    runButton->setFixedSize(52, 26);
    runButton->setAccessibleName(tr("Run custom formula"));
    runButton->setToolTip(tr("Evaluate f(x) with the current display value"));
    connect(runButton, &QPushButton::clicked, this, [this] {
        const QString expression = formulaExpressionEdit_->text().trimmed();
        applyFormula(expression, tr("f(x)=%1").arg(expression));
    });

    customRow->addWidget(formulaPrompt);
    customRow->addWidget(formulaExpressionEdit_, 1);
    customRow->addWidget(runButton);
    formulaViewLayout->addLayout(customRow);

    displayLayout->addWidget(historyLabel_);
    displayLayout->addWidget(displayLabel_, 1);
    displayLayout->addWidget(developerView_, 1);
    displayLayout->addWidget(formulaView_, 1);

    rootLayout->addWidget(displayPanel_);

    auto* buttonGrid = new QGridLayout;
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->setHorizontalSpacing(8);
    buttonGrid->setVerticalSpacing(8);
    for (int column = 0; column < 4; ++column) {
        buttonGrid->setColumnStretch(column, 1);
    }
    for (int row = 0; row < 5; ++row) {
        buttonGrid->setRowStretch(row, 1);
    }

    buildButtonGrid(buttonGrid);
    rootLayout->addLayout(buttonGrid, 1);

    auto* footerLabel = new QLabel(
        tr("^D DEV   ^F FORMULA   DEL CLR   ESC AC"), this);
    footerLabel->setObjectName(QStringLiteral("footerLabel"));
    footerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    footerLabel->setAccessibleName(tr("Keyboard shortcuts"));
    rootLayout->addWidget(footerLabel);

    setStyleSheet(QStringLiteral(R"(
        QWidget#calculatorWindow {
            background-color: #000300;
        }

        QFrame#statusBar {
            background-color: #020701;
            border: 1px solid #2b4c08;
            border-radius: 0px;
        }

        QLabel#brandLabel {
            color: #b7ff3c;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#modeLabel {
            color: #5f8a22;
            font-size: 11px;
            font-weight: 600;
        }

        QFrame#displayPanel {
            background-color: #020701;
            border: 2px solid #3f6f0b;
            border-radius: 0px;
        }

        QFrame#displayPanel[error="true"] {
            background-color: #061001;
            border-color: #d5ff5e;
        }

        QLabel {
            background: transparent;
        }

        QLabel#historyLabel {
            color: #6f9a33;
            font-size: 13px;
            font-weight: 500;
        }

        QLabel#historyLabel[error="true"] {
            color: #d5ff5e;
        }

        QLabel#displayLabel {
            color: #c8ff55;
            padding: 2px;
        }

        QLabel#displayLabel[error="true"] {
            color: #eaffb0;
        }

        QLabel#developerKey {
            color: #7da935;
            font-size: 11px;
            font-weight: 700;
            padding-top: 2px;
        }

        QLabel#developerKey[conversionKey="dec"] {
            color: #4c6e1c;
        }

        QLabel#developerKey[conversionKey="bin"] {
            color: #a6ff35;
        }

        QLabel#developerKey[conversionKey="hex"] {
            color: #c8ff55;
        }

        QLabel#developerKey[conversionKey="bits"] {
            color: #d6ff88;
        }

        QLabel#developerValue {
            color: #b7e85c;
        }

        QLabel#developerMessage {
            color: #b7e85c;
            font-size: 12px;
            font-weight: 500;
        }

        QLabel#formulaPrompt {
            color: #7da935;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#footerLabel {
            color: #6f9a33;
            font-size: 10px;
            font-weight: 500;
        }

        QPushButton {
            min-width: 54px;
            min-height: 48px;
            border: 2px solid #030700;
            border-radius: 0px;
            padding: 4px;
            font-size: 24px;
            font-weight: 600;
        }

        QPushButton#button_developer,
        QPushButton#button_formula {
            min-width: 0px;
            min-height: 0px;
            max-height: 20px;
            padding: 0px 7px;
            border: 1px solid #4a760d;
            border-radius: 0px;
            background-color: transparent;
            color: #a6e645;
            font-size: 11px;
            font-weight: 700;
        }

        QPushButton#button_developer:hover,
        QPushButton#button_formula:hover {
            background-color: #a6ff00;
            border-color: #d5ff7a;
            color: #010400;
        }

        QPushButton#button_developer:pressed,
        QPushButton#button_formula:pressed {
            background-color: #7bc400;
            color: #010400;
        }

        QPushButton#button_developer:checked,
        QPushButton#button_formula:checked {
            background-color: #a6ff00;
            border-color: #d5ff7a;
            color: #010400;
        }

        QPushButton[buttonRole="formula"] {
            min-width: 0px;
            min-height: 0px;
            max-height: 28px;
            padding: 0px 4px;
            border: 1px solid #365b08;
            border-radius: 0px;
            background-color: #050e02;
            color: #b7ff3c;
            font-size: 13px;
            font-weight: 600;
        }

        QPushButton[buttonRole="formula"]:hover {
            background-color: #a6ff00;
            border-color: #d5ff7a;
            color: #010400;
        }

        QPushButton[buttonRole="formula"]:pressed {
            background-color: #d5ff6b;
            color: #010400;
        }

        QPushButton[buttonRole="formulaRun"] {
            min-width: 52px;
            min-height: 0px;
            max-width: 52px;
            max-height: 26px;
            padding: 0px;
            border: 1px solid #c8ff55;
            border-radius: 0px;
            background-color: #a6ff00;
            color: #010400;
            font-size: 12px;
            font-weight: 700;
        }

        QPushButton[buttonRole="formulaRun"]:hover {
            background-color: #c8ff55;
        }

        QPushButton[buttonRole="formulaRun"]:pressed {
            background-color: #7bc400;
        }

        QLineEdit#formulaExpression {
            background-color: #010400;
            border: 1px solid #365b08;
            border-radius: 0px;
            padding: 2px 6px;
            color: #c8ff55;
            font-size: 13px;
            selection-background-color: #a6ff00;
            selection-color: #010400;
        }

        QLineEdit#formulaExpression:focus {
            border-color: #a6ff00;
        }

        QPushButton[buttonRole="number"] {
            background-color: #071003;
            border: 2px solid #030700;
            border-top-color: #345b09;
            border-left-color: #345b09;
            color: #c0f25e;
        }

        QPushButton[buttonRole="number"]:hover {
            background-color: #132603;
            border-color: #6f9e16;
            color: #e2ff9e;
        }

        QPushButton[buttonRole="number"]:pressed {
            background-color: #a6ff00;
            border-color: #c8ff55;
            color: #010400;
        }

        QPushButton[buttonRole="utility"] {
            background-color: #0b1703;
            border: 2px solid #030700;
            border-top-color: #3b660a;
            border-left-color: #3b660a;
            color: #a4d64a;
            font-size: 18px;
            font-weight: 700;
        }

        QPushButton[buttonRole="utility"]:hover {
            background-color: #1a2f04;
            border-color: #6f9e16;
            color: #e2ff9e;
        }

        QPushButton[buttonRole="utility"]:pressed {
            background-color: #a6ff00;
            border-color: #c8ff55;
            color: #010400;
        }

        QPushButton#button_backspace {
            font-size: 14px;
        }

        QPushButton[buttonRole="operator"] {
            background-color: #9dff00;
            border: 2px solid #c8ff55;
            border-right-color: #4d7a00;
            border-bottom-color: #4d7a00;
            color: #010400;
            font-size: 27px;
            font-weight: 700;
        }

        QPushButton[buttonRole="operator"]:hover {
            background-color: #c3ff5e;
            border-color: #eaffb0;
        }

        QPushButton[buttonRole="operator"]:pressed {
            background-color: #76c400;
            border-color: #a6ff00;
            color: #010400;
        }

        QPushButton[buttonRole="equals"] {
            background-color: #d5ff6b;
            border: 2px solid #eaffb0;
            border-right-color: #8fb800;
            border-bottom-color: #8fb800;
            color: #010400;
            font-size: 28px;
            font-weight: 700;
        }

        QPushButton[buttonRole="equals"]:hover {
            background-color: #eaffb0;
            border-color: #f5ffd6;
        }

        QPushButton[buttonRole="equals"]:pressed {
            background-color: #b0e600;
            border-color: #d5ff6b;
        }

        QPushButton:disabled {
            background-color: #020600;
            border: 2px solid #111f04;
            color: #35500f;
        }

        QToolTip {
            background-color: #010400;
            color: #c8ff55;
            border: 1px solid #6f9e16;
            padding: 4px;
        }
    )"));
}

void Calculator::buildButtonGrid(QGridLayout* grid)
{
    const auto addCalculationButton = [this](QPushButton* button) {
        calculationButtons_.push_back(button);
    };

    const auto addDigit = [this, grid, &addCalculationButton](
                              int digit, int row, int column) {
        const QString number = QString::number(digit);
        auto* button = createButton(
            number, QStringLiteral("button_%1").arg(number),
            QStringLiteral("number"), number);
        connect(button, &QPushButton::clicked, this, [this, digit] {
            onDigit(digit);
        });
        grid->addWidget(button, row, column);
        addCalculationButton(button);
    };

    const auto addOperator = [this, grid, &addCalculationButton](
                                 const QString& text, const QString& objectName,
                                 const QString& accessibleName,
                                 calc::BinaryOperation operation, const QString& shortcut,
                                 int row, int column) {
        auto* button = createButton(
            text, objectName, QStringLiteral("operator"), accessibleName);
        button->setToolTip(tr("%1 (%2)").arg(accessibleName, shortcut));
        connect(button, &QPushButton::clicked, this, [this, operation] {
            onOperation(operation);
        });
        grid->addWidget(button, row, column);
        addCalculationButton(button);
    };

    auto* clearButton = createButton(
        tr("C"), QStringLiteral("button_clear"), QStringLiteral("utility"),
        tr("Clear current entry"));
    clearButton->setToolTip(tr("Clear current entry (Delete)"));
    connect(clearButton, &QPushButton::clicked, this, &Calculator::onClearEntry);
    grid->addWidget(clearButton, 0, 0);

    auto* allClearButton = createButton(
        tr("AC"), QStringLiteral("button_all_clear"), QStringLiteral("utility"),
        tr("Clear everything"));
    allClearButton->setToolTip(tr("Clear everything (Esc)"));
    connect(allClearButton, &QPushButton::clicked, this, &Calculator::onAllClear);
    grid->addWidget(allClearButton, 0, 1);

    auto* backspaceButton = createButton(
        QStringLiteral("DEL"), QStringLiteral("button_backspace"),
        QStringLiteral("utility"), tr("Backspace"));
    backspaceButton->setToolTip(tr("Delete the last digit (Backspace)"));
    connect(backspaceButton, &QPushButton::clicked, this, &Calculator::onBackspace);
    grid->addWidget(backspaceButton, 0, 2);
    addCalculationButton(backspaceButton);

    addOperator(QStringLiteral("/"), QStringLiteral("button_divide"),
        tr("Divide"), calc::BinaryOperation::Divide, QStringLiteral("/"), 0, 3);

    addDigit(7, 1, 0);
    addDigit(8, 1, 1);
    addDigit(9, 1, 2);
    addOperator(QStringLiteral("*"), QStringLiteral("button_multiply"),
        tr("Multiply"), calc::BinaryOperation::Multiply, QStringLiteral("*"), 1, 3);

    addDigit(4, 2, 0);
    addDigit(5, 2, 1);
    addDigit(6, 2, 2);
    addOperator(QStringLiteral("-"), QStringLiteral("button_subtract"),
        tr("Subtract"), calc::BinaryOperation::Subtract, QStringLiteral("-"), 2, 3);

    addDigit(1, 3, 0);
    addDigit(2, 3, 1);
    addDigit(3, 3, 2);
    addOperator(QStringLiteral("+"), QStringLiteral("button_add"),
        tr("Add"), calc::BinaryOperation::Add, QStringLiteral("+"), 3, 3);

    addDigit(0, 4, 0);

    auto* decimalButton = createButton(
        QStringLiteral("."), QStringLiteral("button_decimal"),
        QStringLiteral("number"), tr("Decimal point"));
    decimalButton->setToolTip(tr("Decimal point (.)"));
    connect(decimalButton, &QPushButton::clicked, this, &Calculator::onDecimalPoint);
    grid->addWidget(decimalButton, 4, 1);
    addCalculationButton(decimalButton);

    auto* percentButton = createButton(
        tr("%"), QStringLiteral("button_percent"), QStringLiteral("utility"),
        tr("Percent"));
    connect(percentButton, &QPushButton::clicked, this, &Calculator::onPercent);
    grid->addWidget(percentButton, 4, 2);
    addCalculationButton(percentButton);

    auto* equalsButton = createButton(
        QStringLiteral("="), QStringLiteral("button_equals"),
        QStringLiteral("equals"), tr("Equals"));
    equalsButton->setToolTip(tr("Calculate (Enter or =)"));
    connect(equalsButton, &QPushButton::clicked, this, &Calculator::onEquals);
    grid->addWidget(equalsButton, 4, 3);
    addCalculationButton(equalsButton);
}

QPushButton* Calculator::createButton(const QString& text,
    const QString& objectName, const QString& role,
    const QString& accessibleName)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName(objectName);
    button->setProperty("buttonRole", role);
    button->setAccessibleName(accessibleName);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button->setMinimumSize(54, 48);
    return button;
}

void Calculator::onDigit(int digit)
{
    engine_.inputDigit(digit);
    refreshUi();
}

void Calculator::onDecimalPoint()
{
    engine_.inputDecimalPoint();
    refreshUi();
}

void Calculator::onOperation(calc::BinaryOperation operation)
{
    engine_.inputOperation(operation);
    refreshUi();
}

void Calculator::onEquals()
{
    engine_.evaluate();
    refreshUi();
}

void Calculator::onPercent()
{
    engine_.applyPercent();
    refreshUi();
}

void Calculator::onBackspace()
{
    engine_.backspace();
    refreshUi();
}

void Calculator::onClearEntry()
{
    engine_.clearEntry();
    refreshUi();
}

void Calculator::onAllClear()
{
    engine_.clearAll();
    refreshUi();
}

void Calculator::onDeveloperToggled(bool enabled)
{
    if (enabled && formulaMode_) {
        formulaMode_ = false;
        formulaStatus_.clear();
        const QSignalBlocker blocker(formulaButton_);
        formulaButton_->setChecked(false);
    }

    developerMode_ = enabled;
    updateModeViews();
    updateDeveloperView();
    updateHistoryText();
    setErrorAppearance(engine_.hasError());

    if (!enabled) {
        setFocus(Qt::OtherFocusReason);
    }
}

void Calculator::onFormulaToggled(bool enabled)
{
    if (enabled && developerMode_) {
        developerMode_ = false;
        const QSignalBlocker blocker(developerButton_);
        developerButton_->setChecked(false);
    }

    formulaMode_ = enabled;
    formulaStatus_.clear();
    updateModeViews();
    updateHistoryText();
    setErrorAppearance(engine_.hasError());

    if (!enabled) {
        setFocus(Qt::OtherFocusReason);
    }
}

void Calculator::applyFormula(const QString& expression, const QString& label)
{
    const QString displayText = QString::fromStdString(engine_.display());

    bool parsed = false;
    const double x = QLocale::c().toDouble(displayText, &parsed);
    if (!parsed || !std::isfinite(x)) {
        formulaStatus_ = tr("ERR: NO NUMERIC VALUE");
        refreshUi();
        return;
    }

    const formula::Evaluation result = formula::evaluate(expression, x);
    if (!result.ok) {
        formulaStatus_ = tr("ERR: %1").arg(result.error);
        refreshUi();
        return;
    }

    engine_.setResult(
        result.value, tr("%1  %2 =").arg(displayText, label).toStdString());
    formulaStatus_.clear();

    {
        const QSignalBlocker blocker(formulaButton_);
        formulaButton_->setChecked(false);
    }
    formulaMode_ = false;
    updateModeViews();
    setFocus(Qt::OtherFocusReason);
    refreshUi();
}

void Calculator::initializeHistoryLog()
{
    const QString dataRoot
        = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataRoot.isEmpty()) {
        return;
    }

    // Keep the log in a stable per-user data directory rather than next to the
    // executable, so it works from any launch directory and survives updates.
    QDir directory(QDir(dataRoot).filePath(QStringLiteral("Neo")));
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return;
    }

    historyFilePath_ = directory.filePath(QStringLiteral("history.txt"));
}

void Calculator::logCompletedCalculation()
{
    const quint64 completed = engine_.completedCalculations();
    if (completed == loggedCalculationCount_) {
        return;
    }

    // Advance even if the log cannot be written so a later UI refresh does not
    // duplicate an entry that was already attempted.
    loggedCalculationCount_ = completed;
    if (historyFilePath_.isEmpty()) {
        return;
    }

    const QString entry
        = QStringLiteral("[%1] %2 %3\n")
              .arg(QDateTime::currentDateTime().toString(
                       QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                  QString::fromStdString(engine_.history()),
                  QString::fromStdString(engine_.display()));

    QFile file(historyFilePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    file.write(entry.toUtf8());
}

void Calculator::refreshUi()
{
    const QString displayText = QString::fromStdString(engine_.display());
    const QString historyText = QString::fromStdString(engine_.history());
    const bool hasError = engine_.hasError();
    const bool hasFormulaError = formulaMode_
        && formulaStatus_.startsWith(QStringLiteral("ERR"));

    displayLabel_->setText(displayText);
    displayLabel_->setToolTip(displayText);
    historyLabel_->setToolTip(historyText);

    for (QPushButton* button : calculationButtons_) {
        button->setEnabled(!hasError);
    }

    setErrorAppearance(hasError || hasFormulaError);
    updateHistoryText();
    if (developerMode_) {
        updateDeveloperView();
    }
    updateDisplayFont();
    logCompletedCalculation();
}

void Calculator::updateModeViews()
{
    const bool auxiliaryView = developerMode_ || formulaMode_;

    displayLabel_->setVisible(!auxiliaryView);
    developerView_->setVisible(developerMode_);
    formulaView_->setVisible(formulaMode_);

    if (modeLabel_) {
        modeLabel_->setText(developerMode_
                ? QStringLiteral("DEV")
                : formulaMode_ ? QStringLiteral("FX")
                               : QStringLiteral("READY"));
    }

    if (!auxiliaryView) {
        updateDisplayFont();
    }
}

void Calculator::updateDeveloperView()
{
    const QString displayText = QString::fromStdString(engine_.display());
    DeveloperConversion conversion;
    const DeveloperConversionStatus status
        = convertForDeveloper(displayText, conversion);

    if (status == DeveloperConversionStatus::Ok) {
        developerGrid_->show();
        developerMessageLabel_->hide();

        developerDecimalLabel_->setText(conversion.decimal);
        developerBinaryLabel_->setText(conversion.binary);
        developerHexLabel_->setText(conversion.hexadecimal);
        developerBitsLabel_->setText(conversion.bits);

        developerDecimalLabel_->setToolTip(conversion.decimal);
        developerBinaryLabel_->setToolTip(conversion.binary);
        developerHexLabel_->setToolTip(conversion.hexadecimal);
        developerBitsLabel_->setToolTip(conversion.bits);
        return;
    }

    developerGrid_->hide();
    developerMessageLabel_->show();

    switch (status) {
    case DeveloperConversionStatus::NoNumber:
        developerMessageLabel_->setText(tr("No value to convert."));
        break;
    case DeveloperConversionStatus::NotAnInteger:
        developerMessageLabel_->setText(
            tr("\"%1\" is not a whole number.").arg(displayText));
        break;
    case DeveloperConversionStatus::OutOfRange:
        developerMessageLabel_->setText(
            tr("\"%1\" is outside the 64-bit integer range.")
                .arg(displayText));
        break;
    case DeveloperConversionStatus::Ok:
        break;
    }
}

void Calculator::updateDisplayFont()
{
    if (developerMode_ || formulaMode_) {
        return;
    }

    const int availableWidth = displayLabel_->contentsRect().width();
    if (availableWidth <= 0 || displayLabel_->text().isEmpty()) {
        return;
    }

    QFont font = displayLabel_->font();
    font.setWeight(QFont::Bold);

    int pixelSize = kMaximumDisplayFontSize;
    for (; pixelSize > kMinimumDisplayFontSize; --pixelSize) {
        font.setPixelSize(pixelSize);
        const QFontMetrics metrics(font);
        if (metrics.horizontalAdvance(displayLabel_->text()) <= availableWidth) {
            break;
        }
    }

    font.setPixelSize(pixelSize);
    displayLabel_->setFont(font);
}

void Calculator::updateHistoryText()
{
    if (formulaMode_) {
        const QString displayText = QString::fromStdString(engine_.display());
        const QString status = formulaStatus_.isEmpty()
            ? tr("> FX BANK :: x = %1").arg(displayText)
            : tr("> %1").arg(formulaStatus_);
        historyLabel_->setToolTip(status);

        const int availableWidth = historyLabel_->contentsRect().width();
        if (availableWidth <= 0) {
            historyLabel_->setText(status);
            return;
        }

        const QFontMetrics metrics(historyLabel_->font());
        historyLabel_->setText(
            metrics.elidedText(status, Qt::ElideRight, availableWidth));
        return;
    }

    const QString history = QString::fromStdString(engine_.history());
    const QString text = history.isEmpty()
        ? QStringLiteral("> READY")
        : QStringLiteral("> ") + history;
    historyLabel_->setToolTip(text);

    const int availableWidth = historyLabel_->contentsRect().width();
    if (availableWidth <= 0) {
        historyLabel_->setText(text);
        return;
    }

    const QFontMetrics metrics(historyLabel_->font());
    historyLabel_->setText(
        metrics.elidedText(text, Qt::ElideRight, availableWidth));
}

void Calculator::setErrorAppearance(bool hasError)
{
    setErrorProperty(displayPanel_, hasError);
    setErrorProperty(displayLabel_, hasError);
    setErrorProperty(historyLabel_, hasError);
}

void Calculator::setErrorProperty(QWidget* widget, bool hasError)
{
    if (!widget || widget->property("error").toBool() == hasError) {
        return;
    }

    widget->setProperty("error", hasError);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void Calculator::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateDisplayFont();
    updateHistoryText();
}

void Calculator::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::ControlModifier
        && event->key() == Qt::Key_D) {
        developerButton_->setChecked(!developerButton_->isChecked());
        event->accept();
        return;
    }

    if (event->modifiers() == Qt::ControlModifier
        && event->key() == Qt::Key_F) {
        formulaButton_->setChecked(!formulaButton_->isChecked());
        event->accept();
        return;
    }

    if (event->modifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_0:
        onDigit(0);
        break;
    case Qt::Key_1:
        onDigit(1);
        break;
    case Qt::Key_2:
        onDigit(2);
        break;
    case Qt::Key_3:
        onDigit(3);
        break;
    case Qt::Key_4:
        onDigit(4);
        break;
    case Qt::Key_5:
        onDigit(5);
        break;
    case Qt::Key_6:
        onDigit(6);
        break;
    case Qt::Key_7:
        onDigit(7);
        break;
    case Qt::Key_8:
        onDigit(8);
        break;
    case Qt::Key_9:
        onDigit(9);
        break;
    case Qt::Key_Period:
    case Qt::Key_Comma:
        onDecimalPoint();
        break;
    case Qt::Key_Plus:
        onOperation(calc::BinaryOperation::Add);
        break;
    case Qt::Key_Minus:
        onOperation(calc::BinaryOperation::Subtract);
        break;
    case Qt::Key_Asterisk:
        onOperation(calc::BinaryOperation::Multiply);
        break;
    case Qt::Key_Slash:
        onOperation(calc::BinaryOperation::Divide);
        break;
    case Qt::Key_Percent:
        onPercent();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Equal:
        onEquals();
        break;
    case Qt::Key_Backspace:
        onBackspace();
        break;
    case Qt::Key_Delete:
        onClearEntry();
        break;
    case Qt::Key_Escape:
        onAllClear();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    event->accept();
}
