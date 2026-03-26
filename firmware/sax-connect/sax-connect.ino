#include <V2Buttons.h>
#include <V2Device.h>
#include <V2Link.h>
#include <V2MIDI.h>

V2DEVICE_METADATA("com.versioduo.sax-connect", 1, "versioduo:samd:connect");

namespace {
  V2LED::WS2812        LED(20, PIN_LED_WS2812, &sercom2, SPI_PAD_0_SCK_1, PIO_SERCOM);
  V2Base::Analog::ADC  ADC[]{0, 1};
  V2Link::Port         Socket(&SerialSocket, PIN_SERIAL_SOCKET_TX_ENABLE);
  V2MIDI::SerialDevice MIDISerial(&SerialMIDI);

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
      usb.pid            = 0xea00;
      usb.ports.standard = 2;

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

    void handleReset() {
      LED.reset();
    }

    bool handleSend(V2MIDI::Packet* midi) override {
      led.flash(0.03, 0.3);
      usb.midi.send(midi);
      return true;
    }

    void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
      LED.splashHSV(0.5, V2Colour::Orange, 1, 0.25);
    }

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
        auto address{packet->getAddress()};
        if (address == 0x0f)
          return;

        if (address > 0)
          return;

        packet->receive(&_midi);
        MIDISerial.send(&_midi);

        static constexpr std::array<uint8_t, 16> channel{7, 11, 15, 19, 6, 10, 14, 18, 5, 9, 13, 17, 4, 8, 12, 16};
        if (_midi.getType() == V2MIDI::Packet::Status::NoteOn)
          LED.setHSV(channel[_midi.getChannel()], _midi.getChannel() % 2 == 0 ? V2Colour::Cyan : V2Colour::Orange, 0.9, 0.8);
        else if (_midi.getType() == V2MIDI::Packet::Status::NoteOff)
          LED.setBrightness(channel[_midi.getChannel()], 0);

        if (!Device.usb.midi.connected())
          return;

        _midi.setPort(address + 1);
        Device.usb.midi.send(&_midi);
      }
    }
  } Link;

  // Dispatch MIDI packets
  class MIDI {
  public:
    void loop() {
      if (Device.usb.midi.receive(&_midi)) {
        if (_midi.getPort() == 0) {
          Device.dispatch(&Device.usb.midi, &_midi);

        } else {
          _midi.setPort(_midi.getPort() - 1);
          Socket.send(&_midi);
        }
      }

      if (MIDISerial.receive(&_midi))
        Socket.send(&_midi);
    }

  private:
    V2MIDI::Packet _midi;
  } MIDI;

  class Button : public V2Buttons::Button {
  public:
    Button(uint8_t index, uint8_t pin) : V2Buttons::Button(&_config, pin), _index{index} {}

  private:
    const uint8_t           _index;
    const V2Buttons::Config _config{.clickUsec{200 * 1000}, .holdUsec{500 * 1000}};

    void handleHold(uint8_t count) override {
      switch (_index) {
        case 0:
          switch (count) {
            case 0:
              Device.send(V2MIDI::Packet().setControlChange(0, 3, 0));
              LED.rainbow(1, 2, 0.8);
              break;
          }
          break;

        case 1:
          LED.setHSV(1, V2Colour::Red, 0.9, 1);
          break;

        case 2:
          LED.setHSV(2, V2Colour::Green, 0.9, 1);
          break;

        case 3:
          LED.setHSV(3, V2Colour::Blue, 0.9, 1);
          break;
      }
    }

    void handleClick(uint8_t count) override {
      Device.reset();
    }
  };

  std::array Buttons{
    Button{0, PIN_BUTTON + 0},
    Button{1, PIN_BUTTON + 1},
    Button{2, PIN_BUTTON + 2},
    Button{3, PIN_BUTTON + 3},
  };
}

void setup() {
  Serial.begin(9600);
  LED.begin();
  LED.setMaxBrightness(0.2);
  Link.begin();
  setSerialPriority(&SerialSocket, 2);
  MIDISerial.begin();
  Device.serial = &MIDISerial;
  for (auto& b : Buttons)
    b.begin();
  Device.usb.midi.setPortName(1, "Connector");
  Device.usb.midi.setPortName(2, "Saxophone");
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
