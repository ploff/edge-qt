#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <vector>
#include <string>
#include <map>
#include "hidapi/hidapi.h"

// Прямые объявления классов Qt для уменьшения времени компиляции
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSlider;
class QSpinBox;
class QGroupBox;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void writeToDevice();
    void restoreDefaults();
    void selectLedColor();

private:
    // UI
    void setupUI();
    QWidget* createActionsGroup();
    QGroupBox* createPerformanceGroup();
    QGroupBox* createLedGroup();
    QGroupBox* createDpiEditorGroup();

    QLabel* statusLabel;

    // advanced
    QComboBox* pollRateCombo;
    QComboBox* debounceCombo;
    QCheckBox* angleSnapCheck;
    QCheckBox* rippleControlCheck;
    QSpinBox* activeDpiSpinBox;

    // LED
    QComboBox* ledModeCombo;
    QSlider* ledSpeedSlider;
    QSlider* ledBrightnessSlider;
    QPushButton* ledColorButton;
    QLabel* ledColorSwatch;

    // DPI editor
    std::vector<QCheckBox*> dpiEnableChecks;
    std::vector<QSlider*> dpiValueSliders;
    std::vector<QSpinBox*> dpiValueSpinBoxes;

    void initializeMaps();
    void updateUiFromPayload(const std::vector<uint8_t>& payload);
    void updatePayloadFromUi();

    hid_device* findAndOpenDevice();
    bool sendHidReport(const std::vector<uint8_t>& payload);
    std::vector<uint8_t> readHidReport();
    std::vector<uint8_t> factorySettingsPayload();

    std::vector<uint8_t> currentPayloadState;
    std::string workingDevicePath;

    // conversion maps
    std::map<int, uint8_t> ledModeSpeedMap;
    std::map<std::string, int> ledModeIDs;
    std::map<int, int> pollingRateMapToHz;
    std::map<int, int> pollingRateHzToIndex;
    std::map<int, int> debounceMsToIndex;
    std::map<int, int> debounceIndexToMs;

    // device data
    static const unsigned short VID = 0x2EA8;
    static const unsigned short PID = 0x2203;
    static const uint8_t ReportID = 0x04;
    static const size_t WritePayloadLength = 63;
    static const size_t ReportBufferLength = WritePayloadLength + 1;

    // offsets
    static const size_t ActiveDPIIndex = 3;
    static const size_t PollingRate = 6;
    static const size_t DPIEnableMask = 7;
    static const size_t DPIValuesStart = 8;
    static const size_t SensorPerf = 34;
    static const size_t LEDModeID = 36;
    static const size_t LEDSpeed = 37;
    static const size_t LEDBrightness = 38;
    static const size_t LEDPaletteFlag = 40;
    static const size_t DPIColorsStart = 41;
};

#endif // MAINWINDOW_H
