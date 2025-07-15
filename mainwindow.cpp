#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QColorDialog>
#include <QMessageBox>
#include <vector>
#include <string>
#include <cstring>
#include "mainwindow.h"

using namespace std;

const map<int, uint8_t> DpiToWriteValue = {
    {400, 0x09},   {800, 0x12},   {1200, 0x1b}, {1500, 0x22},
    {1800, 0x29},  {2000, 0x2e},  {2100, 0x30}, {2400, 0x37},
    {3000, 0x45},  {3200, 0x4a},  {6200, 0x91}, {12400, 0x94}
};

int findClosestSupportedDpi(int targetDpi) {
    if (DpiToWriteValue.count(targetDpi)) {
        return targetDpi;
    }
    int closestDpi = -1;
    int minDiff = 100000;
    for (const auto& pair : DpiToWriteValue) {
        int diff = abs(pair.first - targetDpi);
        if (diff < minDiff) {
            minDiff = diff;
            closestDpi = pair.first;
        }
    }
    return closestDpi;
}
MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    if (hid_init()) {
        QMessageBox::critical(this, "error", "can't initialize hidapi.");
        QApplication::quit();
    }
    initializeMaps();
    currentPayloadState = factorySettingsPayload();

    setupUI();

    updateUiFromPayload(currentPayloadState);
    statusLabel->setText("ready. default settings was loaded.");
}

MainWindow::~MainWindow()
{
    hid_exit();
}

void MainWindow::setupUI()
{
    setWindowTitle("ZET/ARDOR GAMING Edge configurator");
    setMinimumSize(500, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();

    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(createPerformanceGroup());
    leftLayout->addStretch();

    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(createLedGroup());
    rightLayout->addStretch();

    topLayout->addLayout(leftLayout);
    topLayout->addLayout(rightLayout);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(createDpiEditorGroup());

    mainLayout->addStretch();

    mainLayout->addWidget(createActionsGroup());

    statusLabel = new QLabel("connect mouse.");
    statusLabel->setWordWrap(true);
    mainLayout->addWidget(statusLabel);
}

QGroupBox* MainWindow::createDpiEditorGroup() {
    QGroupBox *groupBox = new QGroupBox("DPI");
    QGridLayout *layout = new QGridLayout;

    layout->addWidget(new QLabel("ON/OFF"), 0, 0);
    layout->addWidget(new QLabel("level"), 0, 1);
    layout->addWidget(new QLabel("DPI value"), 0, 2, 1, 2);

    dpiEnableChecks.resize(7);
    dpiValueSliders.resize(7);
    dpiValueSpinBoxes.resize(7);

    for (int i = 0; i < 7; ++i) {
        dpiEnableChecks[i] = new QCheckBox();
        QLabel *label = new QLabel(QString("DPI %1").arg(i + 1));
        dpiValueSliders[i] = new QSlider(Qt::Horizontal);
        dpiValueSpinBoxes[i] = new QSpinBox();

        dpiValueSliders[i]->setRange(2, 124); // 200/100, 12400/100
        dpiValueSpinBoxes[i]->setRange(200, 12400);
        dpiValueSpinBoxes[i]->setSingleStep(100);

        connect(dpiValueSliders[i], &QSlider::valueChanged, this, [this, i](int value) {
            QSignalBlocker blocker(dpiValueSpinBoxes[i]);
            dpiValueSpinBoxes[i]->setValue(value * 100);
        });
        connect(dpiValueSpinBoxes[i], QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
            QSignalBlocker blocker(dpiValueSliders[i]);
            dpiValueSliders[i]->setValue(value / 100);
        });

        layout->addWidget(dpiEnableChecks[i], i + 1, 0);
        layout->addWidget(label, i + 1, 1);
        layout->addWidget(dpiValueSliders[i], i + 1, 2);
        layout->addWidget(dpiValueSpinBoxes[i], i + 1, 3);
    }

    layout->setColumnStretch(2, 1);
    groupBox->setLayout(layout);
    return groupBox;
}

QWidget* MainWindow::createActionsGroup() {
    QWidget *actionsPanel = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(actionsPanel);

    QPushButton *writeButton = new QPushButton("save");
    connect(writeButton, &QPushButton::clicked, this, &MainWindow::writeToDevice);

    QPushButton *defaultsButton = new QPushButton("restore");
    connect(defaultsButton, &QPushButton::clicked, this, &MainWindow::restoreDefaults);

    layout->addStretch();
    layout->addWidget(writeButton);
    layout->addWidget(defaultsButton);
    layout->addStretch();

    return actionsPanel;
}

QGroupBox* MainWindow::createPerformanceGroup() {
    QGroupBox *groupBox = new QGroupBox("other");
    QFormLayout *layout = new QFormLayout;

    pollRateCombo = new QComboBox();
    for(auto const& [hz, index] : pollingRateHzToIndex) {
        pollRateCombo->addItem(QString::number(hz) + " Hz", QVariant(hz));
    }

    debounceCombo = new QComboBox();
    vector<int> debounce_keys;
    for(auto const& pair : debounceMsToIndex) debounce_keys.push_back(pair.first);
    sort(debounce_keys.begin(), debounce_keys.end());
    for(int ms : debounce_keys) {
        debounceCombo->addItem(QString::number(ms) + " ms", QVariant(ms));
    }

    angleSnapCheck = new QCheckBox("angle snap");
    rippleControlCheck = new QCheckBox("ripple control");

    layout->addRow("poll:", pollRateCombo);
    layout->addRow("debounce time:", debounceCombo);
    layout->addRow(angleSnapCheck);
    layout->addRow(rippleControlCheck);

    groupBox->setLayout(layout);
    return groupBox;
}

QGroupBox* MainWindow::createLedGroup() {
    QGroupBox *groupBox = new QGroupBox("LED");
    QFormLayout *layout = new QFormLayout;

    ledModeCombo = new QComboBox();
    for(auto const& [name, id] : ledModeIDs) {
        ledModeCombo->addItem(QString::fromStdString(name), QVariant(id));
    }

    ledSpeedSlider = new QSlider(Qt::Horizontal);
    ledSpeedSlider->setRange(0, 2);
    ledSpeedSlider->setTickPosition(QSlider::TicksBelow);
    ledSpeedSlider->setTickInterval(1);

    ledBrightnessSlider = new QSlider(Qt::Horizontal);
    ledBrightnessSlider->setRange(0, 10);
    ledBrightnessSlider->setTickPosition(QSlider::TicksBelow);
    ledBrightnessSlider->setTickInterval(1);

    ledColorButton = new QPushButton("choose color");
    ledColorSwatch = new QLabel();
    ledColorSwatch->setFixedSize(40, 20);
    ledColorSwatch->setAutoFillBackground(true);
    ledColorSwatch->setStyleSheet("border: 1px solid black;");
    connect(ledColorButton, &QPushButton::clicked, this, &MainWindow::selectLedColor);

    layout->addRow("mode:", ledModeCombo);
    layout->addRow("speed:", ledSpeedSlider);
    layout->addRow("brightness:", ledBrightnessSlider);

    groupBox->setLayout(layout);
    return groupBox;
}

void MainWindow::writeToDevice()
{
    statusLabel->setText("writing config onto device...");
    QApplication::processEvents();

    updatePayloadFromUi();

    if (sendHidReport(currentPayloadState)) {
        statusLabel->setText("writing successfully done.");
    }
}

void MainWindow::restoreDefaults()
{
    statusLabel->setText("factory reset...");
    QApplication::processEvents();

    vector<uint8_t> factoryPayload = factorySettingsPayload();
    if (sendHidReport(factoryPayload)) {
        currentPayloadState = factoryPayload;
        updateUiFromPayload(currentPayloadState);
        statusLabel->setText("factory reset successfully done.");
    }
}

void MainWindow::selectLedColor()
{
    QPalette palette = ledColorSwatch->palette();
    QColor initialColor = palette.color(QPalette::Window);
    QColor color = QColorDialog::getColor(initialColor, this, "choose color for backlight");

    if (color.isValid()) {
        palette.setColor(QPalette::Window, color);
        ledColorSwatch->setPalette(palette);
    }
}

hid_device* MainWindow::findAndOpenDevice()
{
    if (!workingDevicePath.empty()) {
        hid_device* dev = hid_open_path(workingDevicePath.c_str());
        if (dev) return dev;
    }

    hid_device_info* devs = hid_enumerate(VID, PID);
    if (!devs) {
        statusLabel->setText("error: mouse not found. are you sure that mouse is connected?");
        return nullptr;
    }

    vector<uint8_t> test_cmd = {ReportID, 0xa0, 0x01, 0x00};
    test_cmd.resize(ReportBufferLength, 0);

    hid_device* opened_device = nullptr;
    for(hid_device_info* cur_dev = devs; cur_dev; cur_dev = cur_dev->next) {
        if (!cur_dev->path) continue;

        hid_device* dev = hid_open_path(cur_dev->path);
        if (!dev) continue;

        if (hid_write(dev, test_cmd.data(), test_cmd.size()) < 0) {
            hid_close(dev);
            continue;
        }

        vector<uint8_t> read_buf(ReportBufferLength, 0);
        int bytes_read = hid_read_timeout(dev, read_buf.data(), read_buf.size(), 500);

        if (bytes_read > 0) {
            vector<uint8_t> expected_response = {0x04, 0xa0, 0x01, 0x00, 0x00, 0x0a, 0x23};
            if (memcmp(read_buf.data(), expected_response.data(), expected_response.size()) == 0) {
                workingDevicePath = cur_dev->path;
                opened_device = dev;
                break;
            }
        }
        hid_close(dev);
    }
    hid_free_enumeration(devs);

    if (!opened_device) {
        statusLabel->setText("error: mouse founded, but can't find working interface. do you set udev rules?");
    }
    return opened_device;
}

vector<uint8_t> MainWindow::readHidReport()
{
    hid_device* dev = findAndOpenDevice();
    if (!dev) return {};

    vector<uint8_t> cmd_read = {ReportID, 0xa0, 0x01, 0x01};
    cmd_read.resize(ReportBufferLength, 0);

    if (hid_write(dev, cmd_read.data(), cmd_read.size()) < 0) {
        statusLabel->setText(QString("sending read command error: %1").arg(QString::fromWCharArray(hid_error(dev))));
        hid_close(dev);
        return {};
    }

    vector<uint8_t> read_buf1(ReportBufferLength, 0);
    if (hid_read_timeout(dev, read_buf1.data(), read_buf1.size(), 1000) < 0) {
        statusLabel->setText("error: timeout when reading first answer packet.");
        hid_close(dev);
        return {};
    }

    vector<uint8_t> read_buf2(ReportBufferLength, 0);
    if (hid_read_timeout(dev, read_buf2.data(), read_buf2.size(), 1000) < 0) {
        statusLabel->setText("error: timeout when reading second answer packet.");
        hid_close(dev);
        return {};
    }
    hid_close(dev);

    vector<uint8_t> expected_header = {0x04, 0xa0, 0x01, 0x01, 0x01};
    if (memcmp(read_buf1.data(), expected_header.data(), expected_header.size()) != 0) {
        statusLabel->setText("error: wrong header in first answer packet.");
        return {};
    }

    vector<uint8_t> payload;
    payload.insert(payload.end(), read_buf1.begin() + 5, read_buf1.end());
    payload.insert(payload.end(), read_buf2.begin() + 5, read_buf2.end());

    return payload;
}

bool MainWindow::sendHidReport(const vector<uint8_t>& payload_data)
{
    if (payload_data.size() != WritePayloadLength) {
        statusLabel->setText("error: wrong payload length.");
        return false;
    }

    hid_device* dev = findAndOpenDevice();
    if (!dev) return false;

    vector<uint8_t> report_buffer = {ReportID};
    report_buffer.insert(report_buffer.end(), payload_data.begin(), payload_data.end());

    int bytes_written = hid_write(dev, report_buffer.data(), report_buffer.size());
    hid_close(dev);

    if (bytes_written < 0) {
        statusLabel->setText("error writing data onto mouse.");
        return false;
    }

    return true;
}

void MainWindow::updateUiFromPayload(const std::vector<uint8_t>& payload)
{
    // blocking all signals at window as protection from recursive calls
    const QSignalBlocker blocker(this);

    uint8_t poll_rate_index = payload[PollingRate];
    if (pollingRateMapToHz.count(poll_rate_index)) {
        int poll_hz = pollingRateMapToHz.at(poll_rate_index);
        int combo_index = pollRateCombo->findData(poll_hz);
        if (combo_index != -1) {
            pollRateCombo->setCurrentIndex(combo_index);
        }
    }

    uint8_t sensor_perf_byte = payload[SensorPerf];
    angleSnapCheck->setChecked((sensor_perf_byte & 0x01) != 0);
    rippleControlCheck->setChecked((sensor_perf_byte & 0x02) != 0);
    int debounce_idx = (sensor_perf_byte >> 2) & 0x3F;
    if (debounceIndexToMs.count(debounce_idx)) {
        int debounce_ms = debounceIndexToMs.at(debounce_idx);
        int combo_index = debounceCombo->findData(debounce_ms);
        if(combo_index != -1) {
            debounceCombo->setCurrentIndex(combo_index);
        }
    }

    int led_mode_id = payload[LEDModeID];
    int combo_led_index = ledModeCombo->findData(led_mode_id);
    if (combo_led_index != -1) {
        ledModeCombo->setCurrentIndex(combo_led_index);
    }

    uint8_t speed_val = payload[LEDSpeed];
    int speed_key = 2;
    for(const auto& pair : ledModeSpeedMap) {
        if (pair.second == speed_val) {
            speed_key = pair.first;
            break;
        }
    }
    ledSpeedSlider->setValue(speed_key);
    ledBrightnessSlider->setValue(payload[LEDBrightness]);

    // ура костыли
    dpiValueSpinBoxes[0]->setValue(400);
    dpiValueSpinBoxes[1]->setValue(800);
    dpiValueSpinBoxes[2]->setValue(1200);
    dpiValueSpinBoxes[3]->setValue(2400);
    dpiValueSpinBoxes[4]->setValue(3200);
    dpiValueSpinBoxes[5]->setValue(6200);
    dpiValueSpinBoxes[6]->setValue(12400);

    uint8_t dpi_mask = payload[DPIEnableMask];
    for (int i = 0; i < 7; ++i) {
        dpiEnableChecks[i]->setChecked((dpi_mask >> i) & 0x01);
    }
}

void MainWindow::updatePayloadFromUi()
{
    // DPI
    uint8_t dpi_mask = 0x00;
    for (int i = 0; i < 7; ++i) {
        if (dpiEnableChecks[i]->isChecked()) {
            dpi_mask |= (1 << i);
        }
        int ui_dpi = dpiValueSpinBoxes[i]->value();
        int supported_dpi = findClosestSupportedDpi(ui_dpi);
        if(ui_dpi != supported_dpi){
            dpiValueSpinBoxes[i]->setValue(supported_dpi);
        }

        uint8_t raw_value = DpiToWriteValue.at(supported_dpi);

        size_t offset = DPIValuesStart + (i * 3);
        currentPayloadState[offset] = raw_value;
        currentPayloadState[offset + 1] = raw_value;
        currentPayloadState[offset + 2] = 0x00;
    }
    currentPayloadState[DPIEnableMask] = dpi_mask;

    // polling
    int poll_hz = pollRateCombo->currentData().toInt();
    currentPayloadState[PollingRate] = static_cast<uint8_t>(pollingRateHzToIndex.at(poll_hz));

    // sensor flags
    uint8_t& sensor_flags = currentPayloadState[SensorPerf];
    if (angleSnapCheck->isChecked()) sensor_flags |= 0x01; else sensor_flags &= ~0x01;
    if (rippleControlCheck->isChecked()) sensor_flags |= 0x02; else sensor_flags &= ~0x02;
    int debounce_ms = debounceCombo->currentData().toInt();
    sensor_flags &= ~(0x3F << 2);
    sensor_flags |= (static_cast<uint8_t>(debounceMsToIndex.at(debounce_ms)) << 2);

    // led backlight
    int mode_id = ledModeCombo->currentData().toInt();
    currentPayloadState[LEDModeID] = static_cast<uint8_t>(mode_id);
    currentPayloadState[LEDSpeed] = ledModeSpeedMap.at(ledSpeedSlider->value());
    currentPayloadState[LEDBrightness] = static_cast<uint8_t>(ledBrightnessSlider->value());

    // palette flag
    uint8_t palette_flag = 0x7f;
    if (mode_id == 1 || mode_id == 4) palette_flag = 0x01; // breathe, tail
    else if (mode_id == 2 || mode_id == 8) palette_flag = 0x00; // steady, heart
    else if (mode_id == 5) palette_flag = 0x4f; // colorful_tail
    currentPayloadState[LEDPaletteFlag] = palette_flag;
}

vector<uint8_t> MainWindow::factorySettingsPayload() {
    return {
        0xa0, 0x01, 0x02, // WriteCFG command
        0x01,             // ActiveDPIIndex: active level 2
        0x02,             // unknown
        0xa5,             // unknown magic byte
        0x01,             // PollingRate: index 1 -> 250 Hz
        0x7f,             // DPIEnableMask: 01111111 -> all 7 levels are enabled
        0x00,             // unknown

        // DPI levels at custom 3-byte format
        0x09, 0x09, 0x00, // 1 level (400 DPI)
        0x12, 0x12, 0x00, // 2 level (800 DPI)
        0x1b, 0x1b, 0x00, // 3 level (1200 DPI)
        0x37, 0x37, 0x00, // 4 level (2400 DPI)
        0x4a, 0x4a, 0x00, // 5 level (3200 DPI)
        0x91, 0x91, 0x00, // 6 level (6200 DPI)
        0x94, 0x94, 0x00, // 7 level (12400 DPI)

        // sensor parameters
        0x00, 0x00, 0x02, // unknown
        0x18,             // AngleLOD: AngleAdj=6, LOD=0
        0x14,             // SensorPerf: Debounce=12ms, Snap/Ripple=OFF
        0xa5,             // unknown magic byte

        // led parameters
        0x00,             // LEDModeID: mode 0 -> "prismo"
        0x01,             // LEDSpeed: speed "medium"
        0x0a,             // LEDBrightness: brightness 10
        0x01,             // unknown
        0x7f,             // LEDPaletteFlag: flag for "prismo"

        // DPI level colors
        0xff, 0x00, 0x00, // red
        0x00, 0xff, 0x00, // green
        0x00, 0x00, 0xff, // blue
        0xff, 0xff, 0x00, // yellow
        0x00, 0xff, 0xff, // cyan
        0xff, 0x00, 0xff, // purple
        0xff, 0xff, 0xff, // white

        // completing byte
        0x00
    };
}

void MainWindow::initializeMaps() {
    ledModeSpeedMap = {{0, 0x02}, {1, 0x01}, {2, 0x00}};
    ledModeIDs = {{"prismo", 0}, {"breathe", 1}, {"steady", 2}, {"neon", 3}, {"tail", 4}, {"colorful_tail", 5}, {"stream", 6}, {"reaction", 7}, {"heart", 8}, {"off", 9}};
    pollingRateMapToHz = {{0, 125}, {1, 250}, {2, 500}, {3, 1000}};
    for (const auto& pair : pollingRateMapToHz) { pollingRateHzToIndex[pair.second] = pair.first; }
    for (int i = 0; i < 15; ++i) { int ms = (i + 1) * 2; debounceMsToIndex[ms] = i; debounceIndexToMs[i] = ms; }
}
