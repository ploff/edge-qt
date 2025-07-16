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

void MainWindow::setupUI() {
    setWindowTitle("ZET/ARDOR GAMING Edge Configurator");
    setMinimumSize(500, 597);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QTabWidget *tabWidget = new QTabWidget();

    tabWidget->addTab(DpiEditorTab(), "DPI");
    tabWidget->addTab(LedTab(), "LED");
    tabWidget->addTab(PerformanceTab(), "performance");

    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(ActionsTab());

    statusLabel = new QLabel("connect the mouse.");
    statusLabel->setWordWrap(true);
    mainLayout->addWidget(statusLabel);
}

QWidget* MainWindow::DpiEditorTab() {
    QWidget *tab = new QWidget();
    QGridLayout *layout = new QGridLayout(tab);

    layout->addWidget(new QLabel("ON/OFF"), 1, 0);
    layout->addWidget(new QLabel("level"), 1, 1);
    layout->addWidget(new QLabel("value"), 1, 2, 1, 2);

    dpiEnableChecks.resize(7);
    dpiValueSliders.resize(7);
    dpiValueSpinBoxes.resize(7);

    for (int i = 0; i < 7; ++i) {
        dpiEnableChecks[i] = new QCheckBox();
        QLabel *label = new QLabel(QString("DPI %1").arg(i + 1));
        dpiValueSliders[i] = new QSlider(Qt::Horizontal);
        dpiValueSpinBoxes[i] = new QSpinBox();

        dpiValueSliders[i]->setRange(200, 12400);
        dpiValueSliders[i]->setSingleStep(50);
        dpiValueSpinBoxes[i]->setRange(200, 12400);
        dpiValueSpinBoxes[i]->setSingleStep(50);

        connect(dpiValueSliders[i], &QSlider::valueChanged, dpiValueSpinBoxes[i], &QSpinBox::setValue);
        connect(dpiValueSpinBoxes[i], QOverload<int>::of(&QSpinBox::valueChanged), dpiValueSliders[i], &QSlider::setValue);

        layout->addWidget(dpiEnableChecks[i], i + 2, 0);
        layout->addWidget(label, i + 2, 1);
        layout->addWidget(dpiValueSliders[i], i + 2, 2);
        layout->addWidget(dpiValueSpinBoxes[i], i + 2, 3);
    }
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(layout->rowCount(), 1);

    return tab;
}

QWidget* MainWindow::ActionsTab() {
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

QWidget* MainWindow::PerformanceTab() {
    QWidget *tab = new QWidget();
    QFormLayout *layout = new QFormLayout(tab);

    pollRateCombo = new QComboBox();
    for(const auto& pair : pollingRateHzToIndex) {
        pollRateCombo->addItem(QString::number(pair.first) + " Hz", QVariant(pair.first));
    }

    debounceCombo = new QComboBox();
    std::vector<int> debounce_keys;
    for(const auto& pair : debounceMsToIndex) debounce_keys.push_back(pair.first);
    std::sort(debounce_keys.begin(), debounce_keys.end());
    for(int ms : debounce_keys) {
        debounceCombo->addItem(QString::number(ms) + " ms", QVariant(ms));
    }

    angleSnapCheck = new QCheckBox("angle snapping");
    rippleControlCheck = new QCheckBox("ripple control");

    layout->addRow("polling rate:", pollRateCombo);
    layout->addRow("debounce time:", debounceCombo);
    layout->addRow(angleSnapCheck);
    layout->addRow(rippleControlCheck);

    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    return tab;
}

QWidget* MainWindow::LedTab() {
    QWidget *tab = new QWidget();
    QVBoxLayout *mainLedLayout = new QVBoxLayout(tab);

    QGroupBox *settingsGroup = new QGroupBox("LED settings");
    QFormLayout *settingsLayout = new QFormLayout;

    ledModeCombo = new QComboBox();
    for(const auto& pair : ledModeIDs) {
        ledModeCombo->addItem(QString::fromStdString(pair.first), QVariant(pair.second));
    }
    connect(ledModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateColorPickersVisibility);

    ledSpeedSlider = new QSlider(Qt::Horizontal);
    ledSpeedSlider->setRange(0, 2);
    ledSpeedSlider->setTickPosition(QSlider::TicksBelow);
    ledSpeedSlider->setTickInterval(1);

    ledBrightnessSlider = new QSlider(Qt::Horizontal);
    ledBrightnessSlider->setRange(0, 10);
    ledBrightnessSlider->setTickPosition(QSlider::TicksBelow);
    ledBrightnessSlider->setTickInterval(1);

    settingsLayout->addRow("mode:", ledModeCombo);
    settingsLayout->addRow("speed:", ledSpeedSlider);
    settingsLayout->addRow("brightness:", ledBrightnessSlider);
    settingsGroup->setLayout(settingsLayout);
    mainLedLayout->addWidget(settingsGroup);

    ledColorGroup = new QGroupBox("color palette");
    QGridLayout* colorLayout = new QGridLayout;
    ledColorButtons.resize(7);
    ledColorSwatches.resize(7);

    for(int i = 0; i < 7; ++i) {
        QLabel* colorLabel = new QLabel(QString("color %1:").arg(i+1));
        colorLayout->addWidget(colorLabel, i, 0);

        ledColorSwatches[i] = new QLabel();
        ledColorSwatches[i]->setFixedSize(40, 20);
        ledColorSwatches[i]->setAutoFillBackground(true);

        ledColorButtons[i] = new QPushButton("select...");
        connect(ledColorButtons[i], &QPushButton::clicked, this, [this, i]() {
            this->selectLedPaletteColor(i);
        });

        colorLayout->addWidget(ledColorSwatches[i], i, 1);
        colorLayout->addWidget(ledColorButtons[i], i, 2);
    }
    ledColorGroup->setLayout(colorLayout);
    mainLedLayout->addWidget(ledColorGroup);
    mainLedLayout->addStretch();

    return tab;
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

void MainWindow::updateColorPickersVisibility(int index) {
    if (index < 0) return;

    int modeId = ledModeCombo->itemData(index).toInt();

    int colors_to_show = 0;
    if (ledModeColorCount.count(modeId)) {
        colors_to_show = ledModeColorCount.at(modeId);
    }

    if (colors_to_show == 0) {
        ledColorGroup->setVisible(false);
    } else {
        ledColorGroup->setVisible(true);

        QGridLayout *colorLayout = qobject_cast<QGridLayout*>(ledColorGroup->layout());
        if (!colorLayout) {
            // if for "some reason" layout is wrong type, exit for crash avoid
            return;
        }

        for(int i = 0; i < 7; ++i) {
            bool visible = (i < colors_to_show);

            // hiding/showing entire row
            if (auto item = colorLayout->itemAtPosition(i, 0)) {
                if (auto widget = item->widget()) {
                    widget->setVisible(visible);
                }
            }
            if (auto item = colorLayout->itemAtPosition(i, 1)) {
                if (auto widget = item->widget()) {
                    widget->setVisible(visible);
                }
            }
            if (auto item = colorLayout->itemAtPosition(i, 2)) {
                if (auto widget = item->widget()) {
                    widget->setVisible(visible);
                }
            }
        }
    }
}

void MainWindow::selectLedPaletteColor(int colorIndex) {
    if (colorIndex < 0 || colorIndex >= 7) return;

    QPalette palette = ledColorSwatches[colorIndex]->palette();
    QColor initialColor = palette.color(QPalette::Window);
    QColor color = QColorDialog::getColor(initialColor, this, QString("select color %1").arg(colorIndex + 1));

    if (color.isValid()) {
        palette.setColor(QPalette::Window, color);
        ledColorSwatches[colorIndex]->setPalette(palette);
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
        statusLabel->setText("error: mouse found, but can't find working interface. have you set udev rules?");
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

    int poll_rate_index = payload[PollingRate];
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

    // updating colors in UI palette
    for (int i = 0; i < 7; ++i) {
        size_t offset = DPIColorsStart + (i * 3);
        if (offset + 2 < payload.size()) {
            uint8_t r = payload[offset];
            uint8_t g = payload[offset + 1];
            uint8_t b = payload[offset + 2];
            QPalette p;
            p.setColor(QPalette::Window, QColor(r, g, b));
            ledColorSwatches[i]->setPalette(p);
        }
    }

    // ура костыли!
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

    updateColorPickersVisibility(ledModeCombo->currentIndex());
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

    // advanced
    int poll_hz = pollRateCombo->currentData().toInt();
    currentPayloadState[PollingRate] = static_cast<uint8_t>(pollingRateHzToIndex.at(poll_hz));

    uint8_t& sensor_flags = currentPayloadState[SensorPerf];
    sensor_flags &= ~0x03; // resetting anglesnap and ripple bits
    if (angleSnapCheck->isChecked()) sensor_flags |= 0x01;
    if (rippleControlCheck->isChecked()) sensor_flags |= 0x02;
    int debounce_ms = debounceCombo->currentData().toInt();
    sensor_flags &= ~(0x3F << 2); // resetting debounce bits
    sensor_flags |= (static_cast<uint8_t>(debounceMsToIndex.at(debounce_ms)) << 2);

    // LED
    int mode_id = ledModeCombo->currentData().toInt();
    currentPayloadState[LEDModeID] = static_cast<uint8_t>(mode_id);
    currentPayloadState[LEDSpeed] = ledModeSpeedMap.at(ledSpeedSlider->value());
    currentPayloadState[LEDBrightness] = static_cast<uint8_t>(ledBrightnessSlider->value());

    // setting palette flag depending on mode
    uint8_t palette_flag = 0x7f; // default for modes without color set
    if (ledModeColorCount.count(mode_id)) {
        int color_count = ledModeColorCount.at(mode_id);
        if (color_count == 1) {
            // for Steady and Heart flag is 0x00
            // for Breathe and Tail modes flag is 0x01
            if (mode_id == 1 || mode_id == 4) palette_flag = 0x01;
            else if (mode_id == 2 || mode_id == 8) palette_flag = 0x00;
        } else if (mode_id == 5) { // Colorful Tail
            palette_flag = 0x4f;
        }
    }
    currentPayloadState[LEDPaletteFlag] = palette_flag;

    // writing colors from palette to the payload
    for (int i = 0; i < 7; ++i) {
        QColor color = ledColorSwatches[i]->palette().color(QPalette::Window);
        size_t offset = DPIColorsStart + (i * 3);
        if (offset + 2 < currentPayloadState.size()) {
            currentPayloadState[offset]     = static_cast<uint8_t>(color.red());
            currentPayloadState[offset + 1] = static_cast<uint8_t>(color.green());
            currentPayloadState[offset + 2] = static_cast<uint8_t>(color.blue());
        }
    }
}

vector<uint8_t> MainWindow::factorySettingsPayload() {
    return {
        0xa0, 0x01, 0x02, // WriteCFG command
        0x01,             // ActiveDPIIndex: active level 2
        0x02,             // unknown
        0xa5,             // magic byte
        0x01,             // PollingRate: index 1 -> 250 Hz
        0x7f,             // DPIEnableMask: 01111111 -> all 7 levels are enabled
        0x00,             // unknown

        // DPI levels
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
        0xa5,             // magic byte

        // led parameters
        0x00,             // LEDModeID: mode 0 - prismo
        0x01,             // LEDSpeed: medium
        0x0a,             // LEDBrightness: brightness 10
        0x01,             // unknown
        0x7f,             // LEDPaletteFlag

        // default color palette
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
    ledModeIDs = {{"Prismo", 0}, {"Breathe", 1}, {"Steady", 2}, {"Neon", 3}, {"Tail", 4}, {"Colorful Tail", 5}, {"Stream", 6}, {"Reaction", 7}, {"Heart", 8}, {"OFF", 9}};
    pollingRateMapToHz = {{0, 125}, {1, 250}, {2, 500}, {3, 1000}};
    for (const auto& pair : pollingRateMapToHz) { pollingRateHzToIndex[pair.second] = pair.first; }
    for (int i = 0; i < 15; ++i) { int ms = (i + 1) * 2; debounceMsToIndex[ms] = i; debounceIndexToMs[i] = ms; }

    ledModeColorCount = {
        {0, 0}, // prismo
        {1, 1}, // breathe
        {2, 1}, // steady
        {3, 0}, // neon
        {4, 1}, // tail
        {5, 7}, // colorful tail
        {6, 0}, // stream
        {7, 7}, // reaction
        {8, 1}, // heart
        {9, 0}  // off
    };
}
