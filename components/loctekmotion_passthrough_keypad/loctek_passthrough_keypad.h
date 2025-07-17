#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/button/button.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/hal.h" // Für GPIO-Steuerung

namespace esphome {
namespace loctek_passthrough_keypad {

// Befehls-Payloads (Hexadezimalwerte) - Aus Tabelle 1 des Berichts
const std::vector<uint8_t> CMD_WAKE_UP = {0x9b, 0x06, 0x02, 0x00, 0x00, 0x6c, 0xa1, 0x9d};
const std::vector<uint8_t> CMD_UP = {0x9b, 0x06, 0x02, 0x01, 0x00, 0xfc, 0xa0, 0x9d};
const std::vector<uint8_t> CMD_DOWN = {0x9b, 0x06, 0x02, 0x02, 0x00, 0x0c, 0xa0, 0x9d};
const std::vector<uint8_t> CMD_STOP = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d}; // "M" command for stop
const std::vector<uint8_t> CMD_PRESET_1 = {0x9b, 0x06, 0x02, 0x04, 0x00, 0xac, 0xa3, 0x9d};
const std::vector<uint8_t> CMD_PRESET_2 = {0x9b, 0x06, 0x02, 0x08, 0x00, 0xac, 0xa6, 0x9d};
const std::vector<uint8_t> CMD_PRESET_3_STAND = {0x9b, 0x06, 0x02, 0x10, 0x00, 0xac, 0xac, 0x9d};
const std::vector<uint8_t> CMD_PRESET_4_SIT = {0x9b, 0x06, 0x02, 0x00, 0x01, 0xac, 0x60, 0x9d};


class LoctekPassthroughKeypad : public Component, public cover::Cover {
public:
    // ESPHome-Komponenten-Methoden
    void setup() override;
    void loop() override;
    void dump_config() override;

    // Setter für die YAML-Konfiguration (aufgerufen von __init__.py)
    void set_desk_uart(uart::UARTComponent *uart) { this->desk_uart_ = uart; }
    void set_keypad_uart(uart::UARTComponent *uart) { this->keypad_uart_ = uart; }
    void set_pin20_gpio(int pin) { this->pin20_gpio_ = pin; }
    void set_cover(cover::Cover *cover) { this->cover_ = cover; } // This component IS the cover
    void add_preset_button(button::Button *button, const std::vector<uint8_t>& command_payload);
    void set_m_button(button::Button *button) { this->m_button_ = button; }
    void set_wake_up_switch(switch_::Switch *sw) { this->wake_up_switch_ = sw; }

    // Methoden zur Steuerung des Schreibtisches (von Home Assistant aufgerufen)
    void control(const cover::CoverCall &call) override;
    cover::CoverTraits get_traits() override;

    // Methoden zum Senden von Befehlen (intern und von Buttons aufgerufen)
    void send_command(const std::vector<uint8_t>& command);
    void send_up();
    void send_down();
    void send_stop();
    void send_wake_up();

protected:
    uart::UARTComponent *desk_uart_{nullptr}; // UART für die Kommunikation mit der Schreibtischsteuerung
    uart::UARTComponent *keypad_uart_{nullptr}; // UART für die Kommunikation mit der Tastatur
    int pin20_gpio_{-1};
    GPIOPin *pin20_pin_{nullptr}; // ESPHome GPIO Pin Objekt

    button::Button *m_button_{nullptr};
    switch_::Switch *wake_up_switch_{nullptr};

    // Für Passthrough-Logik und PIN 20-Management
    uint32_t last_pin20_high_time_{0};
    bool pin20_active_{false};
    bool desk_moving_{false}; // Interner Status, ob der Schreibtisch sich bewegt

    // Hilfsfunktionen für UART-Datenverarbeitung
    void handle_desk_uart_data();
    void handle_keypad_uart_data();

    // Struktur für Presets
    struct PresetButton {
        button::Button *button;
        std::vector<uint8_t> command_payload;
    };
    std::vector<PresetButton> preset_buttons_;
};

} // namespace loctek_passthrough_keypad
} // namespace esphome
