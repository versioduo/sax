#include <V2Buttons.h>
#include <V2Device.h>
#include <V2Link.h>
#include <V2MIDI.h>

V2DEVICE_METADATA("com.versioduo.sax-connect", 1, "versioduo:samd:connect");

namespace {
  V2LED::WS2812       LED(20, PIN_LED_WS2812, &sercom2, SPI_PAD_0_SCK_1, PIO_SERCOM);
  V2Base::Analog::ADC ADC[]{0, 1};
  V2Link::Port        Socket(&SerialSocket, PIN_SERIAL_SOCKET_TX_ENABLE);

  class Device : public V2Device {
  public:
    Device() : V2Device(30 * 1024) {
      metadata.vendor      = "Versio Duo";
      metadata.product     = "V2 sax-connect";
      metadata.description = "Saxophone Connector";
      metadata.home        = "https://versioduo.com/#sax";

      system.download  = "https://versioduo.com/download";
      system.configure = "https://versioduo.com/configure";

      // https://github.com/versioduo/arduino-board-package/blob/main/boards.txt
      usb.pid = 0xea00;

      configuration = {.version{0}, .size{sizeof(config)}, .data{&config}};
    }

    enum class CC {
      BeatLength = V2MIDI::CC::Controller14,
      Rainbow    = V2MIDI::CC::Controller90,
    };

    // Config, written to EEPROM.
    struct {
    } config{};

    void allNotesOff() {}

    bool handleSend(V2MIDI::Packet* midi) override {
      usb.midi.send(midi);
      return true;
    }

    void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {}

    void handleSystemReset() override {
      reset();
    }

    void exportInput(JsonObject json) override {}

    void exportOutput(JsonObject json) override {}
  } Device;

  // Dispatch Link packets.
  class Link : public V2Link {
  public:
    Link() : V2Link(nullptr, &Socket) {
      Device.link = this;
    }

  private:
    V2MIDI::Packet _midi{};

    // Forward children device events to the host.
    void receiveSocket(V2Link::Packet* packet) override {
      if (packet->getType() == V2Link::Packet::Type::MIDI) {
        uint8_t address = packet->getAddress();
        if (address == 0x0f)
          return;

        if (Device.usb.midi.connected()) {
          packet->receive(&_midi);
          _midi.setPort(address + 1);
          Device.usb.midi.send(&_midi);
        }
      }
    }
  } Link;

  // Dispatch MIDI packets
  class MIDI {
  public:
    void loop() {
      if (!Device.usb.midi.receive(&_midi))
        return;

      if (_midi.getPort() == 0)
        Device.dispatch(&Device.usb.midi, &_midi);
    }

  private:
    V2MIDI::Packet _midi;
  } MIDI;

  class Button : public V2Buttons::Button {
  public:
    Button() : V2Buttons::Button(&_config, PIN_BUTTON) {}

  private:
    const V2Buttons::Config _config{.clickUsec{200 * 1000}, .holdUsec{500 * 1000}};

    void handleHold(uint8_t count) override {
      switch (count) {
        case 0:
          LED.rainbow(1, 2, 0.8);
          break;
      }
    }

    void handleClick(uint8_t count) override {
      Device.reset();
    }
  } Button;
}

void setup() {
  Serial.begin(9600);
  LED.begin();
  LED.setMaxBrightness(0.2);

  Link.begin();
  setSerialPriority(&SerialSocket, 2);

  Button.begin();
  Device.begin();
  Device.reset();
}

void loop() {
  LED.loop();
  MIDI.loop();
  Link.loop();
  V2Buttons::loop();
  Device.loop();

  if (Device.idle())
    Device.sleep();
}
