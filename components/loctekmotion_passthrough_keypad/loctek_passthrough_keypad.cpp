#include "loctek_passthrough_keypad.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h" // Für format_hex_bytes

namespace esphome {
namespace loctek_passthrough_keypad {

static const char *const TAG = "loctek_passthrough_keypad";

void LoctekPassthroughKeypad::setup() {
    ESP_LOGCONFIG(TAG, "Setting up Loctek Passthrough Keypad...");
    this->pin20_pin_ = App.get_pin(this->pin20_gpio_);
    this->pin20_pin_->setup();
    this->pin20_pin_->digital_write(false); // PIN 20 initial auf LOW

    // Registriere Callbacks für M-Taste
    if (this->m_button_!= nullptr) {
        this->m_button_->add_on_press_callback([this]() {
            this->send_stop(); // M-Taste ist der Stopp-Befehl
        });
    }

    // Registriere Callbacks für Wake Up Switch
    if (this->wake_up_switch_!= nullptr) {
        this->wake_up_switch_->add_on_state_callback([this](bool state) {
            if (state) {
                this->send_wake_up();
            } else {
                // Der Schalter wird in loop() oder durch send_command() automatisch deaktiviert
            }
        });
    }

    // Registriere Callbacks für Preset-Buttons
    for (auto &preset : this->preset_buttons_) {
        preset.button->add_on_press_callback([this, preset]() {
            this->send_command(preset.command_payload);
        });
    }
}

void LoctekPassthroughKeypad::loop() {
    // Lese Daten vom Schreibtisch-UART und leite sie an die Tastatur weiter
    this->handle_desk_uart_data();

    // Lese Daten vom Tastatur-UART und leite sie an den Schreibtisch weiter
    this->handle_keypad_uart_data();

    // PIN 20 Management: Deaktiviert PIN 20 nach 1 Sekunde
    if (this->pin20_active_ && millis() - this->last_pin20_high_time_ > 1000) {
        this->pin20_pin_->digital_write(false);
        this->pin20_active_ = false;
        ESP_LOGD(TAG, "PIN 20 deactivated.");
        if (this->wake_up_switch_!= nullptr) {
            this->wake_up_switch_->publish_state(false); // Schalter als "aus" anzeigen
        }
    }

    // Periodisches Senden des Wake Up Befehls, um den Schreibtisch aktiv zu halten
    // Dies ist wichtig, damit der Höhen-Sensor (separate Komponente) kontinuierlich Daten erhält.
    // Die Frequenz kann angepasst werden.
    static uint32_t last_wake_up_send_time = 0;
    if (millis() - last_wake_up_send_time > 5000) { // Alle 5 Sekunden
        this->send_wake_up();
        last_wake_up_send_time = millis();
    }
}

void LoctekPassthroughKeypad::dump_config() {
    ESP_LOGCONFIG(TAG, "Loctek Passthrough Keypad Component:");
    ESP_LOGCONFIG(TAG, "  Desk UART ID: %s", this->desk_uart_->get_uart_id().c_str());
    ESP_LOGCONFIG(TAG, "  Keypad UART ID: %s", this->keypad_uart_->get_uart_id().c_str());
    ESP_LOGCONFIG(TAG, "  PIN 20 GPIO: %d", this->pin20_gpio_);
    if (this->m_button_!= nullptr) {
        ESP_LOGCONFIG(TAG, "  M Button: %s", this->m_button_->get_name().c_str());
    }
    if (this->wake_up_switch_!= nullptr) {
        ESP_LOGCONFIG(TAG, "  Wake Up Switch: %s", this->wake_up_switch_->get_name().c_str());
    }
    for (const auto &preset : this->preset_buttons_) {
        ESP_LOGCONFIG(TAG, "  Preset Button: %s (Command: %s)", preset.button->get_name().c_str(), format_hex_bytes(preset.command_payload).c_str());
    }
}

// Implementierung der Cover-Schnittstelle
void LoctekPassthroughKeypad::control(const cover::CoverCall &call) {
    if (call.get_stop()) {
        this->send_stop();
    } else if (call.get_position().has_value()) {
        // Direkte Positionierung über Slider ist komplex und führt zu Überschießen.
        // Für präzise Steuerung Presets verwenden.
        ESP_LOGW(TAG, "Direct position setting via slider is experimental and may overshoot. Use presets for accuracy.");
        // Hier könnte eine Logik für die Zielhöhe implementiert werden,
        // die den Schreibtisch schrittweise bewegt und die Höhe überwacht.
        // Für dieses Beispiel konzentrieren wir uns auf Up/Down/Stop und Presets.
    } else if (call.get_set_position().has_value()) {
        // Dies wird von ESPHome für Up/Down-Befehle verwendet, wenn keine Position gesetzt wird
        if (call.get_set_position().value() == 1.0f) { // 1.0f bedeutet "öffnen" (hochfahren)
            this->send_up();
        } else if (call.get_set_position().value() == 0.0f) { // 0.0f bedeutet "schließen" (runterfahren)
            this->send_down();
        }
    }
}

cover::CoverTraits LoctekPassthroughKeypad::get_traits() {
    auto traits = cover::CoverTraits();
    traits.set_supports_position(false); // Keine direkte Positionssteuerung über Slider
    traits.set_supports_tilt(false);
    traits.set_has_stop(true);
    return traits;
}

void LoctekPassthroughKeypad::send_command(const std::vector<uint8_t>& command) {
    // PIN 20 für 1 Sekunde auf HIGH setzen, bevor der Befehl gesendet wird
    this->pin20_pin_->digital_write(true);
    this->pin20_active_ = true;
    this->last_pin20_high_time_ = millis();
    ESP_LOGD(TAG, "PIN 20 activated for command: %s", format_hex_bytes(command).c_str());

    // Befehl an den Schreibtisch senden
    this->desk_uart_->write_bytes(command);
    this->desk_uart_->flush(); // Sicherstellen, dass die Daten gesendet werden
}

void LoctekPassthroughKeypad::send_up() {
    ESP_LOGD(TAG, "Sending UP command.");
    this->send_command(CMD_UP);
    this->set_current_operation(cover::COVER_OPERATION_OPENING);
    this->publish_state();
    this->desk_moving_ = true;
}

void LoctekPassthroughKeypad::send_down() {
    ESP_LOGD(TAG, "Sending DOWN command.");
    this->send_command(CMD_DOWN);
    this->set_current_operation(cover::COVER_OPERATION_CLOSING);
    this->publish_state();
    this->desk_moving_ = true;
}

void LoctekPassthroughKeypad::send_stop() {
    ESP_LOGD(TAG, "Sending STOP command.");
    this->send_command(CMD_STOP);
    this->set_current_operation(cover::COVER_OPERATION_IDLE);
    this->publish_state();
    this->desk_moving_ = false;
}

void LoctekPassthroughKeypad::send_wake_up() {
    ESP_LOGD(TAG, "Sending WAKE UP command.");
    this->send_command(CMD_WAKE_UP);
    if (this->wake_up_switch_!= nullptr) {
        this->wake_up_switch_->publish_state(true); // Schalter als "an" anzeigen
    }
}

void LoctekPassthroughKeypad::add_preset_button(button::Button *button, const std::vector<uint8_t>& command_payload) {
    this->preset_buttons_.push_back({button, command_payload});
}

void LoctekPassthroughKeypad::handle_desk_uart_data() {
    // Liest Daten vom Schreibtisch-UART und leitet sie an die Tastatur weiter
    while (this->desk_uart_->available()) {
        uint8_t byte;
        this->desk_uart_->read_byte(&byte);
        // Leite Daten direkt an Tastatur weiter (für Display-Updates)
        if (this->keypad_uart_!= nullptr) {
            this->keypad_uart_->write(byte);
        }
        // Optional: Hier könnten Sie die Daten vom Schreibtisch auch für interne Zwecke parsen,
        // z.B. um den Bewegungsstatus zu aktualisieren, wenn der Schreibtisch von selbst stoppt.
        // Die Höhen-Dekodierung wird jedoch von der separaten loctekmotion_desk_height Komponente übernommen.
    }
}

void LoctekPassthroughKeypad::handle_keypad_uart_data() {
    // Liest Daten vom Tastatur-UART und leitet sie an den Schreibtisch weiter
    while (this->keypad_uart_->available()) {
        uint8_t byte;
        this->keypad_uart_->read_byte(&byte);
        // Leite Daten direkt an Schreibtisch weiter
        this->desk_uart_->write(byte);

        // Optional: Hier könnten Sie die Daten vom Tastenfeld analysieren,
        // um den internen Bewegungsstatus des Covers zu aktualisieren,
        // wenn der Schreibtisch über die physische Tastatur gesteuert wird.
        // Dies hilft, den Status in Home Assistant synchron zu halten.
        // Beispiel: Erkennung von UP/DOWN/STOP Befehlen der Tastatur
        // (Basierend auf den bekannten Befehls-Payloads)
        // Dies ist eine vereinfachte Erkennung, eine robustere würde den gesamten 8-Byte-Befehl parsen.
        if (byte == 0x01) { // UP-Payload-Byte
            this->set_current_operation(cover::COVER_OPERATION_OPENING);
            this->publish_state();
            this->desk_moving_ = true;
        } else if (byte == 0x02) { // DOWN-Payload-Byte
            this->set_current_operation(cover::COVER_OPERATION_CLOSING);
            this->publish_state();
            this->desk_moving_ = true;
        } else if (byte == 0x20) { // STOP-Payload-Byte (M-Taste)
            this->set_current_operation(cover::COVER_OPERATION_IDLE);
            this->publish_state();
            this->desk_moving_ = false;
        }
    }
}

} // namespace loctek_passthrough_keypad
} // namespace esphome
