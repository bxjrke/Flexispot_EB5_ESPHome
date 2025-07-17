import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, button, uart, switch
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_PIN,
    CONF_NAME,
    CONF_DATA,
)

# Definiere den C++ Namespace und die Klasse
loctek_passthrough_keypad_ns = cg.esphome_ns.namespace('loctek_passthrough_keypad')
LoctekPassthroughKeypad = loctek_passthrough_keypad_ns.class_('LoctekPassthroughKeypad',
                                                               cg.Component, uart.UARTDevice)

# Schema für die Preset-Buttons
PRESET_BUTTON_SCHEMA = button.BUTTON_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(button.Button),
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_DATA): cv.All(cv.list(cv.hex_uint8), cv.Length(min=8, max=8)), # Hex-Payload für den Befehl
    }
)

# Konfigurationsschema für die Hauptkomponente
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LoctekPassthroughKeypad),
        cv.Required("desk_uart_id"): cv.use_id(uart.UARTComponent),  # UART für Schreibtisch-Steuerbox
        cv.Required("keypad_uart_id"): cv.use_id(uart.UARTComponent),  # UART für Tastatur (Passthrough)
        cv.Required(CONF_PIN): cv.int_,  # GPIO für PIN 20 (Wake Up)
        
        # Cover-Entität für Hoch/Runter/Stopp
        cv.Optional(cover.CONF_COVER): cover.COVER_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(cover.Cover),
                cv.Required(CONF_NAME): cv.string,
                cv.Optional(cover.CONF_DEVICE_CLASS, default="awning"): cover.DEVICE_CLASSES,
            }
        ),

        # Button-Entitäten für Presets
        cv.Optional("presets"): cv.All(
            cv.ensure_list(PRESET_BUTTON_SCHEMA),
            cv.Length(min=1, max=4)  # Bis zu 4 Presets
        ),
        
        # Button-Entität für M-Taste (Stopp)
        cv.Optional("m_button"): button.BUTTON_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(button.Button),
                cv.Required(CONF_NAME): cv.string,
            }
        ),

        # Switch-Entität für Wake Up (optional, kann auch intern verwaltet werden)
        cv.Optional("wake_up_switch"): switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(switch.Switch),
                cv.Required(CONF_NAME): cv.string,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

# Code generation function
async def to_code(config):
    var = cg.new_Pvariable(config)
    await cg.register_component(var, config)

    # Hole UART-Instanzen
    desk_uart_var = await cg.get_variable(config)
    keypad_uart_var = await cg.get_variable(config)
    
    cg.add(var.set_desk_uart(desk_uart_var))
    cg.add(var.set_keypad_uart(keypad_uart_var))

    # Setze den PIN 20 GPIO
    cg.add(var.set_pin20_gpio(config[CONF_PIN]))

    # Cover-Generierung
    if conf_cover := config.get(cover.CONF_COVER):
        cov = await cover.new_cover(conf_cover)
        cg.add(var.set_cover(cov))

    # Button-Generierung für Presets
    if conf_presets := config.get("presets"):
        for preset_conf in conf_presets:
            btn = await button.new_button(preset_conf)
            cg.add(var.add_preset_button(btn, preset_conf))

    # Button-Generierung für M-Taste
    if conf_m_button := config.get("m_button"):
        m_btn = await button.new_button(conf_m_button)
        cg.add(var.set_m_button(m_btn))

    # Switch-Generierung für Wake Up
    if conf_wake_up_switch := config.get("wake_up_switch"):
        wake_up_sw = await switch.new_switch(conf_wake_up_switch)
        cg.add(var.set_wake_up_switch(wake_up_sw))
