#pragma once

#include <QStringList>
#include <QWidget>

#include <vector>

#include "calcengine.h"

class QFrame;
class QGridLayout;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QString;

class Calculator final : public QWidget {
    Q_OBJECT

public:
    explicit Calculator(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void buildUi();
    void buildButtonGrid(QGridLayout* grid);
    QPushButton* createButton(const QString& text, const QString& objectName,
        const QString& role, const QString& accessibleName);

    void onDigit(int digit);
    void onDecimalPoint();
    void onOperation(calc::BinaryOperation operation);
    void onEquals();
    void onPercent();
    void onBackspace();
    void onClearEntry();
    void onAllClear();
    void onDeveloperToggled(bool enabled);
    void onFormulaToggled(bool enabled);
    void onHistoryToggled(bool enabled);
    void applyFormula(const QString& expression, const QString& label);

    void refreshUi();
    void recordCompletedCalculation();
    void updateDisplayFont();
    void updateHistoryText();
    void updateHistoryView();
    void updateDeveloperView();
    void updateModeViews();
    void setErrorAppearance(bool hasError);
    static void setErrorProperty(QWidget* widget, bool hasError);

    calc::CalcEngine engine_;
    QFrame* displayPanel_ = nullptr;
    QLabel* displayLabel_ = nullptr;
    QLabel* historyLabel_ = nullptr;
    QLabel* modeLabel_ = nullptr;
    QPushButton* developerButton_ = nullptr;
    QPushButton* formulaButton_ = nullptr;
    QPushButton* historyButton_ = nullptr;
    QWidget* developerView_ = nullptr;
    QWidget* developerGrid_ = nullptr;
    QLabel* developerMessageLabel_ = nullptr;
    QLabel* developerDecimalLabel_ = nullptr;
    QLabel* developerBinaryLabel_ = nullptr;
    QLabel* developerHexLabel_ = nullptr;
    QLabel* developerBitsLabel_ = nullptr;
    QWidget* formulaView_ = nullptr;
    QLineEdit* formulaExpressionEdit_ = nullptr;
    QWidget* historyView_ = nullptr;
    QPlainTextEdit* historyViewText_ = nullptr;
    QString formulaStatus_;
    QStringList calculationHistory_;
    quint64 recordedCalculationCount_ = 0;
    bool developerMode_ = false;
    bool formulaMode_ = false;
    bool historyMode_ = false;
    std::vector<QPushButton*> calculationButtons_;
};
