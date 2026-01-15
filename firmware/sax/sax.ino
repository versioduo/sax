#include <V2BHY1.h>
#include <V2Buttons.h>
#include <V2Device.h>
#include <V2Drum.h>
#include <V2Link.h>
#include <V2MIDI.h>

V2DEVICE_METADATA("com.versioduo.sax", 17, "versioduo:samd:sax");

namespace {
  constexpr struct {
    uint8_t count{8};
  } Ports;

  V2LED::WS2812       LED(Ports.count + 2, PIN_LED_WS2812, &sercom2, SPI_PAD_0_SCK_1, PIO_SERCOM);
  V2Base::Analog::ADC ADC[]{0, 1};
  V2Link::Port        Plug(&SerialPlug, PIN_SERIAL_PLUG_TX_ENABLE);

  class Sensor : public V2BHY1 {
  public:
    Sensor() : V2BHY1(&Wire, PIN_SENSOR_INTERRUPT) {}

    // Subtract the recorded home postion to return the relative orientation to it.
    auto getRotation() -> V23D::Quaternion {
      return _setup.home * readCalibrated();
    }

    auto getAcceleration() -> V23D::Vector3 {
      return getGyroscope();
    }

    void updateLED() {
      switch (_setup.state) {
        case State::Init:
          LED.setHSV(V2Colour::Cyan, 0.9, 0.2);
          break;

        case State::Up:
          LED.setHSV(V2Colour::Green, 0.9, 0.6);
          break;

        case State::Calibrated:
          LED.setHSV(V2Colour::Orange, 0.9, 0.4);
          break;
      }
    }

    // The device points upwards. It is not required to point to the exact Z axis, an
    // angle smaller or larger than 90 degrees from the zero / Forward positon leads
    // to the same result. Only the rotation axis between the Tip and Forward matters.
    auto tip() {
      _setup = {
        .state = State::Up,
        .up    = getGravity().normalize(),
      };
      updateLED();
    }
    // The device points forward, the zero position. The rotation axis from Up
    // to Forward defines the Y (Pitch) axis of the device, the gravity defines
    // the Z (Yaw) axis. The button is pressed once again, the LED of the
    // now calibrated device turns orange.
    auto forward() -> bool {
      auto zAxis{getGravity().normalize()};

      // Require a defined / significant movement / angle to prevent wrong
      // calibration values.
      if (auto angle{V23D::radToDeg(_setup.up.angleBetween(zAxis))}; angle < 45.f || angle > 135.f) {
        _setup.state = State::Init;
        LED.splashHSV(0.3, V2Colour::Red, 1, 0.6);
        updateLED();
        return false;
      }

      auto yAxis{_setup.up.cross(zAxis).normalize()};

      // Calculate the calibration / rotation quaternion to move the sensor's frame
      // into the device's frame.
      _setup.calibration = V23D::Attitude::accelerometerMagnetometer(zAxis, yAxis);
      _setup.state       = State::Calibrated;

      updateLED();
      return true;
    }

    // Record the current orientation. It will be substracted from future measurements
    // to use this as home / zero / start position.
    auto home() {
      if (_setup.state == State::Up)
        forward();

      _setup.home = readCalibrated().conjugate();
    }

    auto setup(bool compass, const V23D::Quaternion& calibration) {
      _compass = compass;

      _setup = {.calibration{calibration}};
      if (!calibration.equal(V23D::Quaternion()))
        _setup.state = State::Calibrated;

      home();
      updateLED();
    }

    auto calibration() const -> const V23D::Quaternion& {
      return _setup.calibration;
    }

  private:
    enum class State { Init, Up, Calibrated };
    bool _compass{};
    struct {
      State            state{};
      V23D::Vector3    up;
      V23D::Quaternion calibration;
      V23D::Quaternion home;
    } _setup;

    // Read the absolute orientation of the sensor using the magnetometer /
    // earth's magnetic north (it might jump when the magnetic field reading
    // is disturbed or not yet known).
    // Or read the orientation without using the magentometer (uses the rotation
    // after starting up, and the orientation might drift over time).
    auto read() -> V23D::Quaternion {
      return _compass ? getGeoOrientation() : getOrientation();
    }

    // Apply the calibration recorded by tip + forward sequence.
    auto readCalibrated() -> V23D::Quaternion {
      return _setup.calibration * read() * _setup.calibration.conjugate();
    }
  } Sensor;

  class Device : public V2Device {
  public:
    Device() : V2Device(30 * 1024) {
      metadata.vendor      = "Versio Duo";
      metadata.product     = "V2 sax";
      metadata.description = "Saxophone Expression Controller";
      metadata.home        = "https://versioduo.com/#sax";

      system.download  = "https://versioduo.com/download";
      system.configure = "https://versioduo.com/configure";

      // https://github.com/versioduo/arduino-board-package/blob/main/boards.txt
      usb.pid = 0xef40;

      configuration = {.version{3}, .size{sizeof(config)}, .data{&config}};
    }

    enum class CC {
      BeatLength = V2MIDI::CC::Controller14,
      Rainbow    = V2MIDI::CC::Controller90,
    };

    // Config, written to EEPROM.
    struct {
      struct {
        uint8_t channel{};

        struct Note {
          uint8_t number;
          bool    enable{true};
          bool    aftertouch{true};
        } note;

        struct {
          uint8_t number;
          bool    enable{true};
        } controller;

        struct {
          struct {
            bool    enable{};
            uint8_t channel{8};
            uint8_t note;
          } down;

          struct {
            bool    enable{};
            uint8_t channel{8};
            uint8_t note;
          } up;
        } beat;

        struct {
          bool  invert{true};
          float min{};
          float max{1};
        } range;
      } ports[Ports.count];

      uint8_t beatLength{};
    } config{.ports{
      {
        .note{.number{V2MIDI::C(3) + 0}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 0}},
        .beat{
          .down{.note{V2MIDI::C(5) + 0}},
          .up{.note{V2MIDI::C(5) + 0}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 1}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 1}},
        .beat{
          .down{.note{V2MIDI::C(5) + 1}},
          .up{.note{V2MIDI::C(5) + 1}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 2}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 2}},
        .beat{
          .down{.note{V2MIDI::C(5) + 2}},
          .up{.note{V2MIDI::C(5) + 2}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 3}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 3}},
        .beat{
          .down{.note{V2MIDI::C(5) + 3}},
          .up{.note{V2MIDI::C(5) + 3}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 4}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 4}},
        .beat{
          .down{.note{V2MIDI::C(5) + 4}},
          .up{.note{V2MIDI::C(5) + 4}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 5}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 5}},
        .beat{
          .down{.note{V2MIDI::C(5) + 5}},
          .up{.note{V2MIDI::C(5) + 5}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 6}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 6}},
        .beat{
          .down{.note{V2MIDI::C(5) + 6}},
          .up{.note{V2MIDI::C(5) + 6}},
        },
      },
      {
        .note{.number{V2MIDI::C(3) + 7}},
        .controller{.number{V2MIDI::CC::GeneralPurpose1 + 7}},
        .beat{
          .down{.note{V2MIDI::C(5) + 7}},
          .up{.note{V2MIDI::C(5) + 7}},
        },
      },
    }};

    bool calibrating{};

    struct Playing {
      struct Port {
        uint8_t noteVelocity{};
        uint8_t controller{};
        struct {
          uint32_t downUsec{};
          uint32_t upUsec{};
        } beat;
      } ports[Ports.count];

      int8_t next{};
    } playing;

    void play(int8_t note, int8_t velocity) {
      if (note < V2MIDI::C(3))
        return;

      size_t i = note - V2MIDI::C(3);
      if (i >= Ports.count)
        return;

      if (float fraction = (float)velocity / 127; velocity > 0)
        LED.setHSV(2 + i, V2Colour::Orange, 0.5, fraction);

      else
        LED.setBrightness(2 + i, 0);
    }

    void startCalibration() {
      LED.setHSV(0, V2Colour::Magenta, 0.9, 0.6);
      LED.setHSV(1, V2Colour::Magenta, 0.9, 0.6);
      calibrating = true;
    }

    void storeConfiguration() {
      calibrating = false;
      writeConfiguration();
      LED.setBrightness(0);
      LED.splashHSV(0.3, V2Colour::Magenta, 0.8, 0.5);
    }

  private:
    uint32_t _usec{};
    struct {
      uint8_t  value{};
      uint32_t durationUsec{};
    } _beatLength;

    uint32_t _msec{};
    struct {
      uint8_t x{};
      uint8_t y{};
      uint8_t z{};
    } _gyroscope;

    float          _rainbow{};
    V2MIDI::Packet _midi;

    void handleReset() override {
      LED.reset();
      LED.splashHSV(0.3, V2Colour::Cyan, 0.8, 0.2);
      calibrating = false;
      _beatLength = {
        .value{config.beatLength},
        .durationUsec{uint32_t(1000.f * 1000.f * ((float)config.beatLength / 127.f))},
      };
      _rainbow = 0;
      _midi    = {};

      for (auto& p : playing.ports)
        p = {};

      playing.next = 0;
    }

    void handleLoop() override {
      if (V2Base::getUsecSince(_usec) < 1000)
        return;

      _usec = V2Base::getUsec();

      Playing::Port& port{playing.ports[playing.next]};
      if (port.beat.downUsec > 0 && V2Base::getUsecSince(port.beat.downUsec) > _beatLength.durationUsec) {
        _midi.setNoteOff(config.ports[playing.next].beat.down.channel, config.ports[playing.next].beat.down.note, 64);
        send(&_midi);
        playing.ports[playing.next].beat.downUsec = 0;
      }

      if (port.beat.upUsec > 0 && V2Base::getUsecSince(port.beat.upUsec) > _beatLength.durationUsec) {
        _midi.setNoteOff(config.ports[playing.next].beat.up.channel, config.ports[playing.next].beat.up.note, 64);
        send(&_midi);
        playing.ports[playing.next].beat.upUsec = 0;
      }

      playing.next++;
      if (playing.next == Ports.count)
        playing.next = 0;

      if (_msec++; _msec > 20) {
        _msec = 0;

        auto g{Sensor.getAcceleration()};

        auto center{[](float v) -> uint8_t {
          return ceilf((std::clamp((v + 1.f) / 2.f, 0.f, 1.f)) * 127.f);
        }};

        if (auto x{center(g.x)}; _gyroscope.x != x) {
          send(_midi.setControlChange(0, V2MIDI::CC::Controller29, x));
          _gyroscope.x = x;
        }

        if (auto y{center(g.y)}; _gyroscope.y != y) {
          send(_midi.setControlChange(0, V2MIDI::CC::Controller30, y));
          _gyroscope.y = y;
        }

        if (auto z{center(g.z)}; _gyroscope.z != z) {
          send(_midi.setControlChange(0, V2MIDI::CC::Controller31, z));
          _gyroscope.z = z;
        }
      }
    }

    void allNotesOff() {
      for (auto& p : playing.ports) {
        if (!config.ports[_index].controller.enable)
          continue;

        _midi.setControlChange(config.ports[_index].channel, config.ports[_index].controller.number, p.controller);
        send(&_midi);
      }
    }

    bool handleSend(V2MIDI::Packet* midi) override {
      usb.midi.send(midi);
      Plug.send(midi);
      return true;
    }

    void handleNote(uint8_t channel, uint8_t note, uint8_t velocity) override {
      play(note, velocity);
    }

    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
      play(note, 0);
    }

    void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
      if (channel > 0)
        return;

      switch (controller) {
        case (uint8_t)CC::BeatLength:
          _beatLength.value        = value;
          _beatLength.durationUsec = 1000.f * 1000.f * ((float)value / 127.f);
          break;

        case (uint8_t)CC::Rainbow:
          _rainbow = (float)value / 127.f;
          if (_rainbow <= 0.f)
            LED.reset();
          else
            LED.rainbow(1, 4.5f - (_rainbow * 4.f));
          break;

        case V2MIDI::CC::AllNotesOff:
        case V2MIDI::CC::AllSoundOff:
          allNotesOff();
          break;
      }
    }

    void handleSystemReset() override {
      reset();
    }

    void exportSettings(JsonArray json) override {
      {
        JsonObject setting = json.add<JsonObject>();
        setting["title"]   = "Beat";
        setting["type"]    = "number";
        setting["max"]     = 127;
        setting["label"]   = "Note";
        setting["text"]    = "Length";
        setting["path"]    = "beatLength";
      }

      for (uint8_t i{}; i < Ports.count; i++) {
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"] = "title";
          char name[32];
          sprintf(name, "Port %d", i + 1);
          setting["title"] = name;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "number";
          setting["label"] = "Channel";
          setting["min"]   = 1;
          setting["max"]   = 16;
          setting["input"] = "select";
          char path[64];
          sprintf(path, "ports[%d]/channel", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "toggle";
          setting["label"] = "Enable";
          setting["text"]  = "Note";
          char path[64];
          sprintf(path, "ports[%d]/note/enable", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "toggle";
          setting["label"] = "Enable";
          setting["text"]  = "Aftertouch";
          char path[64];
          sprintf(path, "ports[%d]/note/aftertouch", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "note";
          setting["label"] = "Note";
          char path[64];
          sprintf(path, "ports[%d]/note/number", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "toggle";
          setting["label"] = "Enable";
          setting["text"]  = "Controller";
          char path[64];
          sprintf(path, "ports[%d]/controller/enable", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "controller";
          setting["label"] = "Controller";
          char path[64];
          sprintf(path, "ports[%d]/controller/number", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["ruler"] = true;
          setting["type"]  = "toggle";
          setting["label"] = "Enable";
          setting["text"]  = "Beat Down";
          char path[64];
          sprintf(path, "ports[%d]/beat/down/enable", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "number";
          setting["label"] = "Channel";
          setting["min"]   = 1;
          setting["max"]   = 16;
          setting["input"] = "select";
          char path[64];
          sprintf(path, "ports[%d]/beat/down/channel", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "note";
          setting["label"] = "Note";
          char path[64];
          sprintf(path, "ports[%d]/beat/down/note", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["ruler"] = true;
          setting["type"]  = "toggle";
          setting["label"] = "Enable";
          setting["text"]  = "Beat Up";
          char path[64];
          sprintf(path, "ports[%d]/beat/up/enable", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "number";
          setting["label"] = "Channel";
          setting["min"]   = 1;
          setting["max"]   = 16;
          setting["input"] = "select";
          char path[64];
          sprintf(path, "ports[%d]/beat/up/channel", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "note";
          setting["label"] = "Note";
          char path[64];
          sprintf(path, "ports[%d]/beat/up/note", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["ruler"] = true;
          setting["type"]  = "toggle";
          setting["label"] = "Direction";
          setting["text"]  = "Invert";
          char path[64];
          sprintf(path, "ports[%d]/range/invert", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "number";
          setting["label"] = "Position";
          setting["text"]  = "Up";
          setting["max"]   = 1;
          setting["step"]  = 0.01;
          char path[64];
          sprintf(path, "ports[%d]/range/min", i);
          setting["path"] = path;
        }
        {
          JsonObject setting{json.add<JsonObject>()};
          setting["type"]  = "number";
          setting["label"] = "Position";
          setting["text"]  = "Down";
          setting["max"]   = 1;
          setting["step"]  = 0.01;
          char path[64];
          sprintf(path, "ports[%d]/range/max", i);
          setting["path"] = path;
        }
      }
    }

    void importConfiguration(JsonObject json) override {
      if (!json["beatLength"].isNull())
        config.beatLength = json["beatLength"];

      JsonArray jsonPorts = json["ports"];
      if (jsonPorts) {
        for (uint8_t i = 0; i < Ports.count; i++) {
          JsonObject jsonPort = jsonPorts[i];

          if (!jsonPort["channel"].isNull()) {
            uint8_t channel = jsonPort["channel"];

            if (channel < 1)
              config.ports[i].channel = 0;
            else if (channel > 16)
              config.ports[i].channel = 95;
            else
              config.ports[i].channel = channel - 1;
          }

          if (!jsonPort["note"].isNull()) {
            JsonObject jsonNote = jsonPort["note"];

            if (!jsonNote["enable"].isNull())
              config.ports[i].note.enable = jsonNote["enable"];

            if (!jsonNote["aftertouch"].isNull())
              config.ports[i].note.aftertouch = jsonNote["aftertouch"];

            if (!jsonNote["number"].isNull()) {
              config.ports[i].note.number = jsonNote["number"];
              if (config.ports[i].note.number > 127)
                config.ports[i].note.number = 127;
            }
          }

          if (!jsonPort["controller"].isNull()) {
            JsonObject jsonController = jsonPort["controller"];

            if (!jsonController["enable"].isNull())
              config.ports[i].controller.enable = jsonController["enable"];

            if (!jsonController["number"].isNull()) {
              config.ports[i].controller.number = jsonController["number"];
              if (config.ports[i].controller.number > 127)
                config.ports[i].controller.number = 127;
            }
          }

          if (!jsonPort["beat"].isNull()) {
            JsonObject jsonBeat = jsonPort["beat"];

            if (!jsonBeat["down"].isNull()) {
              JsonObject jsonDown = jsonBeat["down"];

              if (!jsonDown["enable"].isNull())
                config.ports[i].beat.down.enable = jsonDown["enable"];

              if (!jsonDown["channel"].isNull()) {
                uint8_t channel = jsonDown["channel"];
                if (channel < 1)
                  config.ports[i].beat.down.channel = 0;
                else if (channel > 16)
                  config.ports[i].beat.down.channel = 95;
                else
                  config.ports[i].beat.down.channel = channel - 1;
              }

              if (!jsonDown["note"].isNull()) {
                config.ports[i].beat.down.note = jsonDown["note"];
                if (config.ports[i].beat.down.note > 127)
                  config.ports[i].beat.down.note = 127;
              }
            }

            if (!jsonBeat["up"].isNull()) {
              JsonObject jsonUp = jsonBeat["up"];

              if (!jsonUp["enable"].isNull())
                config.ports[i].beat.up.enable = jsonUp["enable"];

              if (!jsonUp["channel"].isNull()) {
                uint8_t channel = jsonUp["channel"];
                if (channel < 1)
                  config.ports[i].beat.up.channel = 0;
                else if (channel > 16)
                  config.ports[i].beat.up.channel = 95;
                else
                  config.ports[i].beat.up.channel = channel - 1;
              }

              if (!jsonUp["note"].isNull()) {
                config.ports[i].beat.up.note = jsonUp["note"];
                if (config.ports[i].beat.up.note > 127)
                  config.ports[i].beat.up.note = 127;
              }
            }
          }

          if (!jsonPort["range"].isNull()) {
            JsonObject jsonRange = jsonPort["range"];

            if (!jsonRange["invert"].isNull())
              config.ports[i].range.invert = jsonRange["invert"];

            if (!jsonRange["min"].isNull()) {
              float value = jsonRange["min"];
              if (value < 0.f || value > 1.f)
                value = 0;

              config.ports[i].range.min = value;
              if (config.ports[i].range.min >= config.ports[i].range.max)
                config.ports[i].range.max = 1;
            }

            if (!jsonRange["max"].isNull()) {
              float value = jsonRange["max"];
              if (value < 0.f || value > 1.f)
                value = 1;

              config.ports[i].range.max = value;
              if (config.ports[i].range.min >= config.ports[i].range.max)
                config.ports[i].range.min = 0;
            }
          }
        }
      }
    }

    void exportConfiguration(JsonObject json) override {
      json["beatLength"] = config.beatLength;

      JsonArray jsonPorts = json["ports"].to<JsonArray>();
      for (uint8_t i = 0; i < Ports.count; i++) {
        JsonObject jsonPort = jsonPorts.add<JsonObject>();

        if (i == 0)
          jsonPort["#channel"] = "The channel to send notes and control values to";
        jsonPort["channel"] = config.ports[i].channel + 1;
        {
          JsonObject jsonNote = jsonPort["note"].to<JsonObject>();

          if (i == 0)
            jsonNote["#enable"] = "Send note";
          jsonNote["enable"] = config.ports[i].note.enable;

          if (i == 0)
            jsonNote["#aftertouch"] = "Send note aftertouch";
          jsonNote["aftertouch"] = config.ports[i].note.aftertouch;

          if (i == 0)
            jsonNote["#number"] = "The note number";
          jsonNote["number"] = config.ports[i].note.number;
        }
        {
          JsonObject jsonController = jsonPort["controller"].to<JsonObject>();

          if (i == 0)
            jsonController["#enable"] = "Send controller messages";
          jsonController["enable"] = config.ports[i].controller.enable;

          if (i == 0)
            jsonController["#controller"] = "The controller number";
          jsonController["number"] = config.ports[i].controller.number;
        }

        {
          JsonObject jsonBeat = jsonPort["beat"].to<JsonObject>();
          {
            JsonObject jsonDown = jsonBeat["down"].to<JsonObject>();
            if (i == 0)
              jsonDown["#enable"] = "Send beat on key down";
            jsonDown["enable"] = config.ports[i].beat.down.enable;

            if (i == 0)
              jsonDown["#channel"] = "The channel to send beats to";
            jsonDown["channel"] = config.ports[i].beat.down.channel + 1;

            if (i == 0)
              jsonDown["#note"] = "The note number";
            jsonDown["note"] = config.ports[i].beat.down.note;
          }

          {
            JsonObject jsonUp = jsonBeat["up"].to<JsonObject>();
            if (i == 0)
              jsonUp["#enable"] = "Send beat on key up";
            jsonUp["enable"] = config.ports[i].beat.up.enable;

            if (i == 0)
              jsonUp["#channel"] = "The channel to send beats to";
            jsonUp["channel"] = config.ports[i].beat.up.channel + 1;

            if (i == 0)
              jsonUp["#note"] = "The note number";
            jsonUp["note"] = config.ports[i].beat.up.note;
          }
        }

        {
          JsonObject jsonRange = jsonPort["range"].to<JsonObject>();
          if (i == 0)
            jsonRange["#invert"] = "Change the direction of the measurement";
          jsonRange["invert"] = config.ports[i].range.invert;

          if (i == 0)
            jsonRange["#min"] = "The idle / minimum / 0 position (0 .. 1)";
          jsonRange["min"] = serialized(String(config.ports[i].range.min, 2));

          if (i == 0)
            jsonRange["#max"] = "The maximum / 127 position (0 .. 1)";
          jsonRange["max"] = serialized(String(config.ports[i].range.max, 2));
        }
      }
    }

    void exportInput(JsonObject json) override {
      JsonArray jsonControllers = json["controllers"].to<JsonArray>();
      {
        JsonObject jsonController = jsonControllers.add<JsonObject>();
        jsonController["name"]    = "Beat Length";
        jsonController["number"]  = (uint8_t)CC::BeatLength;
        jsonController["value"]   = _beatLength.value;
      }
      {
        JsonObject jsonController = jsonControllers.add<JsonObject>();
        jsonController["name"]    = "Rainbow";
        jsonController["number"]  = (uint8_t)CC::Rainbow;
        jsonController["value"]   = (uint8_t)(_rainbow * 127.f);
      }

      JsonObject json_chromatic = json["chromatic"].to<JsonObject>();
      json_chromatic["start"]   = V2MIDI::C(3);
      json_chromatic["count"]   = Ports.count;
    }

    void exportOutput(JsonObject json) override {
      auto json_controllers{json["controllers"].to<JsonArray>()};
      for (uint8_t i = 0; i < Ports.count; i++) {
        if (!config.ports[i].controller.enable)
          continue;

        char name[16];
        sprintf(name, "Port %d", i + 1);

        JsonObject json_controller = json_controllers.add<JsonObject>();
        json_controller["name"]    = name;
        json_controller["number"]  = config.ports[i].controller.number;
        json_controller["value"]   = playing.ports[i].controller;
      }

      JsonArray json_notes = json["notes"].to<JsonArray>();
      for (uint8_t i = 0; i < Ports.count; i++) {
        if (!config.ports[i].note.enable)
          continue;

        char name[16];
        sprintf(name, "Port %d", i + 1);

        JsonObject json_note = json_notes.add<JsonObject>();
        json_note["name"]    = name;
        json_note["number"]  = config.ports[i].note.number;
        if (config.ports[i].note.aftertouch)
          json_note["aftertouch"] = true;
      }

      {
        {
          auto c{json_controllers.add<JsonObject>()};
          c["name"]   = "Gyroscope X";
          c["number"] = V2MIDI::CC::Controller29;
          c["value"]  = _gyroscope.x;
        }
        {
          auto c{json_controllers.add<JsonObject>()};
          c["name"]   = "Gyroscope Y";
          c["number"] = V2MIDI::CC::Controller30;
          c["value"]  = _gyroscope.y;
        }
        {
          auto c{json_controllers.add<JsonObject>()};
          c["name"]   = "Gyroscope Z";
          c["number"] = V2MIDI::CC::Controller31;
          c["value"]  = _gyroscope.z;
        }
      }
    }

    auto exportSystem(JsonObject json) -> void override {
      JsonObject jsonPower{json["sensor"].to<JsonObject>()};
      jsonPower["product"]  = Sensor.getProductID();
      jsonPower["revision"] = Sensor.getRevisionID();
      jsonPower["software"] = Sensor.getRAMVersion();
    }
  } Device;

  class InputPort : public V2Drum {
  public:
    InputPort() = delete;
    constexpr InputPort(uint8_t index) : V2Drum(&_config), _index(index) {}

    float handleMeasurement() override {
      const uint8_t id{V2Base::Analog::ADC::getID(PIN_CHANNEL_SENSE + _index)};
      const uint8_t channel{V2Base::Analog::ADC::getChannel(PIN_CHANNEL_SENSE + _index)};

      float measure{ADC[id].readChannel(channel)};
      if (Device.config.ports[_index].range.invert)
        measure = 1.f - measure;

      float value{measure - Device.config.ports[_index].range.min};
      if (value < 0.f)
        return 0;

      value *= 1.f / (Device.config.ports[_index].range.max - Device.config.ports[_index].range.min);

      if (Device.calibrating) {
        if (value > Device.config.ports[_index].range.max)
          Device.config.ports[_index].range.max = value;
      }

      return value;
    }

    void update() {
      handlePressureRaw(getFraction(), getStep());
      handlePressure(getFraction(), getStep());
    }

  private:
    uint8_t              _index;
    const V2Drum::Config _config{
      .nSteps{128},
      .alpha{0.3},
      .lag{0.01},
      .pressure{
        .min{0.1},
        .max{0.9},
        .exponent{0.5},
      },
      .hit{
        .min{0},
        .max{0.2},
        .exponent{2},
        .risingUsec{2000},
        .holdUsec{1000},
        .pressureDelayUsec{3000},
        .releaseUsec{2000},
      },
      .release{
        .minUsec{5000},
        .maxUsec{50 * 1000},
      },
    };

    V2MIDI::Packet _midi;

    void handlePressureRaw(float fraction, uint16_t step) override {
      LED.setBrightness(2 + _index, fraction);
    }

    void handlePressure(float fraction, uint16_t step) override {
      if (Device.config.ports[_index].note.aftertouch && Device.playing.ports[_index].noteVelocity > 0) {
        _midi.setAftertouch(Device.config.ports[_index].channel, Device.config.ports[_index].note.number, step);
        Device.send(&_midi);
      }

      if (Device.config.ports[_index].controller.enable) {
        Device.playing.ports[_index].controller = step;
        _midi.setControlChange(Device.config.ports[_index].channel, Device.config.ports[_index].controller.number, step);
        Device.send(&_midi);
      }
    }

    void handleHit(uint8_t velocity) override {
      Device.led.flash(0.03, 0.1);
      Device.playing.ports[_index].noteVelocity = velocity;

      if (Device.config.ports[_index].note.enable) {
        _midi.setNote(Device.config.ports[_index].channel, Device.config.ports[_index].note.number, velocity);
        Device.send(&_midi);
      }

      if (Device.config.ports[_index].beat.down.enable) {
        if (Device.playing.ports[_index].beat.downUsec > 0) {
          _midi.setNoteOff(Device.config.ports[_index].beat.down.channel, Device.config.ports[_index].beat.down.note, 64);
          Device.send(&_midi);
        }

        Device.playing.ports[_index].beat.downUsec = V2Base::getUsec();

        _midi.setNote(Device.config.ports[_index].beat.down.channel, Device.config.ports[_index].beat.down.note, velocity);
        Device.send(&_midi);
      }
    }

    void handleRelease(uint8_t velocity) override {
      Device.playing.ports[_index].noteVelocity = 0;

      if (Device.config.ports[_index].note.enable) {
        _midi.setNoteOff(Device.config.ports[_index].channel, Device.config.ports[_index].note.number, velocity);
        Device.send(&_midi);
      }

      if (Device.config.ports[_index].beat.up.enable) {
        if (Device.playing.ports[_index].beat.upUsec > 0) {
          _midi.setNoteOff(Device.config.ports[_index].beat.up.channel, Device.config.ports[_index].beat.up.note, 64);
          Device.send(&_midi);
        }

        Device.playing.ports[_index].beat.upUsec = V2Base::getUsec();

        _midi.setNote(Device.config.ports[_index].beat.up.channel, Device.config.ports[_index].beat.up.note, velocity);
        Device.send(&_midi);
      }
    }
  } InputPorts[Ports.count]{0, 1, 2, 3, 4, 5, 6, 7};

  // Dispatch Link packets.
  class Link : public V2Link {
  public:
    Link() : V2Link(&Plug, nullptr) {
      Device.link = this;
    }

  private:
    V2MIDI::Packet _midi{};

    // Receive a host event from our parent device.
    auto receivePlug(V2Link::Packet* packet) -> void override {
      if (packet->getType() == V2Link::Packet::Type::MIDI) {
        packet->receive(&_midi);
        Device.dispatch(&Plug, &_midi);
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
          if (!Device.calibrating) {
            Device.startCalibration();
            for (uint8_t i = 0; i < Ports.count; i++) {
              Device.config.ports[i].range.min = 0;
              Device.config.ports[i].range.max = 1;

              Device.config.ports[i].range.min = InputPorts[i].handleMeasurement() + 0.025f;
              if (Device.config.ports[i].range.min > 1.f)
                Device.config.ports[i].range.min = 1;

              Device.config.ports[i].range.max = Device.config.ports[i].range.min + 0.10f;
              if (Device.config.ports[i].range.max > 1.f)
                Device.config.ports[i].range.max = 1;
            }
          } else {
            for (uint8_t i = 0; i < Ports.count; i++)
              Device.config.ports[i].range.max -= 0.025f;
            Device.storeConfiguration();
          }
          break;
      }
    }

    void handleClick(uint8_t count) override {
      Device.reset();

      for (auto& p : InputPorts)
        p.update();
    }
  } Button;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(1000000);
  Wire.setTimeout(1);
  LED.begin();
  LED.setMaxBrightness(0.2);

  Link.begin();
  setSerialPriority(&SerialPlug, 2);

  for (uint8_t i = 0; i < V2Base::countof(ADC); i++)
    ADC[i].begin();

  for (uint8_t i = 0; i < Ports.count; i++) {
    auto id{V2Base::Analog::ADC::getID(PIN_CHANNEL_SENSE + i)};
    auto channel{V2Base::Analog::ADC::getChannel(PIN_CHANNEL_SENSE + i)};
    ADC[id].addChannel(channel);
  }

  for (auto& p : InputPorts)
    p.begin();

  Sensor.begin();
  Button.begin();
  Device.begin();
  Device.reset();
}

void loop() {
  for (auto& p : InputPorts)
    p.loop();

  LED.loop();
  MIDI.loop();
  Link.loop();
  Sensor.loop();
  V2Buttons::loop();
  Device.loop();

  if (Device.idle())
    Device.sleep();
}
