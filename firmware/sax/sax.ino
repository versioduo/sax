#include <V2BHY1.h>
#include <V2Buttons.h>
#include <V2Device.h>
#include <V2Link.h>
#include <V2MIDI.h>

namespace {
  struct Setup {
    enum : uint8_t {
      Button,
      Pressure,
      Orientation,
      Valves,
      nValves = 8,
      size    = Valves + nValves,
    };

    static auto valve(uint8_t index) -> uint8_t {
      return Valves + nValves - index - 1;
    }
  };

  V2Device::Info             Info{V2DeviceInfo("com.versioduo.sax", 28, "versioduo:samd:sax")};
  V2LED::WS2812<Setup::size> LED{PIN_LED_WS2812, sercom2, SPI_PAD_0_SCK_1, PIO_SERCOM};
  std::array                 ADC{
    V2Base::Analog::ADC(0),
    V2Base::Analog::ADC(1),
  };
  V2Link::Port Plug{&SerialPlug, PIN_SERIAL_PLUG_TX_ENABLE, "plug"};

  class Orientation : public V2BHY1 {
  public:
    Orientation() : V2BHY1(&Wire, PIN_SENSOR_INTERRUPT) {}

    // Subtract the recorded home postion to return the relative orientation to it.
    auto getRotation() -> V23D::Quaternion {
      return _home * readCalibrated();
    }

    auto setup(bool compass, const V23D::Quaternion& calibration) {
      _compass     = compass;
      _calibration = calibration;
      home();
    }

    // Record the current orientation. It will be substracted from future measurements
    // to use this as home / zero / start position.
    auto home() -> void {
      _home = readCalibrated().conjugate();
    }

  private:
    bool             _compass{};
    V23D::Quaternion _calibration;
    V23D::Quaternion _home;

    // Read the absolute orientation of the sensor using the magnetometer / earth's magnetic north (it
    // might jump when the magnetic field reading is disturbed or not yet known).
    //
    // Or read the orientation without using the magentometer (uses the rotation after starting up, and
    // the orientation might drift over time).
    auto read() -> V23D::Quaternion {
      return _compass ? getGeoOrientation() : getOrientation();
    }

    // Apply the calibration to set the frame of reference / align the sensor's frame with the earth's.
    auto readCalibrated() -> V23D::Quaternion {
      return _calibration * read() * _calibration.conjugate();
    }
  } Orientation;

  class Device : public V2Device {
  public:
    Device() : V2Device() {
      metadata.vendor      = "Versio Duo";
      metadata.product     = "V2 sax";
      metadata.description = "Saxophone Controller";
      metadata.home        = "https://versioduo.com/#sax";
      system.download      = "https://versioduo.com/download";
      system.configure     = "https://versioduo.com/configure";
      usb.ports.standard   = 0;
      configuration        = {.version{1}, .size{sizeof(config)}, .data{&config}};
    }

    enum class CC {
      Pressure    = V2MIDI::CC::BreathController,
      Home        = V2MIDI::CC::Controller14,
      Orientation = V2MIDI::CC::Controller29,
      Light       = V2MIDI::CC::Controller89,
      Rainbow     = V2MIDI::CC::Controller90,
    };

    // Config, written to EEPROM.
    struct {
      struct {
        struct {
          bool    enable{true};
          uint8_t channel{};
          uint8_t note{};
          float   threshold{0.3};
        } down;

        struct {
          bool    enable{true};
          uint8_t channel{};
          uint8_t note{};
          float   threshold{0.7};
        } up;

        struct {
          float up{};
          float down{1};
        } calibration;
      } valves[Setup::nValves];

      struct {
        bool             enabled{};
        V23D::Quaternion calibration;
        bool             compass;
      } orientation;

      struct {
        bool enabled{};
      } pressure;
    } config{
      .valves{
        {
          .down{
            .channel{},
            .note{V2MIDI::A(-1) + 0},
            .threshold{0.72},
          },
          .up{
            .channel{1},
            .note{V2MIDI::A(-1) + 1},
            .threshold{0.26},
          },
          .calibration{
            .up{0.53},
            .down{0.25},
          },
        },
        {
          .down{
            .channel{2},
            .note{V2MIDI::A(-1) + 2},
            .threshold{0.73},
          },
          .up{
            .channel{3},
            .note{V2MIDI::A(-1) + 3},
            .threshold{0.36},
          },
          .calibration{
            .up{0.44},
            .down{0.19},
          },
        },
        {
          .down{
            .channel{4},
            .note{V2MIDI::A(-1) + 4},
            .threshold{0.73},
          },
          .up{
            .channel{5},
            .note{V2MIDI::A(-1) + 5},
            .threshold{0.31},
          },
          .calibration{
            .up{0.61},
            .down{0.28},
          },
        },
        {
          .down{
            .channel{6},
            .note{V2MIDI::A(-1) + 6},
            .threshold{0.72},
          },
          .up{
            .channel{7},
            .note{V2MIDI::A(-1) + 7},
            .threshold{0.3},
          },
          .calibration{
            .up{0.48},
            .down{0.2},
          },
        },
        {
          .down{
            .channel{8},
            .note{V2MIDI::A(-1) + 8},
            .threshold{0.72},
          },
          .up{
            .channel{9},
            .note{V2MIDI::A(-1) + 9},
            .threshold{0.32},
          },
          .calibration{
            .up{1},
            .down{0.39},
          },
        },
        {
          .down{
            .channel{10},
            .note{V2MIDI::A(-1) + 10},
            .threshold{0.7},
          },
          .up{
            .channel{11},
            .note{V2MIDI::A(-1) + 11},
            .threshold{0.32},
          },
          .calibration{
            .up{0.81},
            .down{0.36},
          },
        },
        {
          .down{
            .channel{12},
            .note{V2MIDI::A(-1) + 12},
            .threshold{0.74},
          },
          .up{
            .channel{13},
            .note{V2MIDI::A(-1) + 13},
            .threshold{0.27},
          },
          .calibration{
            .up{0.35},
            .down{0.16},
          },
        },
        {
          .down{
            .channel{14},
            .note{V2MIDI::A(-1) + 14},
            .threshold{0.72},
          },
          .up{
            .channel{15},
            .note{V2MIDI::A(-1) + 15},
            .threshold{0.29},
          },
          .calibration{
            .up{0.46},
            .down{0.16},
          },
        },
      },
    };

    auto allNotesOff() -> void {
      for (auto& v : _valves)
        v = {};

      LED.hsv({V2Colour::Cyan, 0.9, 0.4}, Setup::Button);
      updateLEDs();
    }

    auto calibrating() -> bool {
      return _calibrating;
    }

    auto startCalibration() {
      LED.hsv({V2Colour::Magenta, 0.9, 0.6}, Setup::Button);

      for (uint8_t i{}; i < Setup::nValves; i++) {
        config.valves[i].calibration.up = measureAnalog(PIN_CHANNEL_SENSE + i);
        if (config.valves[i].calibration.up > 1.f)
          config.valves[i].calibration.up = 1;

        config.valves[i].calibration.down = config.valves[i].calibration.up;
      }

      _calibrating = true;
    }

    auto hasCalibration(uint8_t index) -> bool {
      return fabs(config.valves[index].calibration.down - config.valves[index].calibration.up) > 0.05f;
    }

    auto storeCalibration() {
      _calibrating = false;
      writeConfiguration();
      allNotesOff();
    }

    auto measureAnalog(uint8_t pin) -> float {
      uint8_t id{V2Base::Analog::ADC::getID(pin)};
      uint8_t channel{V2Base::Analog::ADC::getChannel(pin)};
      return ADC[id].readChannel(channel);
    }

    auto measureValve(uint8_t index) -> float {
      auto  analog{measureAnalog(PIN_CHANNEL_SENSE + index)};
      float min{config.valves[index].calibration.up};
      float max{config.valves[index].calibration.down};
      if (config.valves[index].calibration.down < config.valves[index].calibration.up) {
        analog = 1.f - analog;
        min    = 1.f - min;
        max    = 1.f - max;
      }

      float v{analog - min};
      if (v < 0.f)
        return 0;

      v *= 1.f / (max - min);
      return v;
    }

  private:
    bool           _calibrating{};
    float          _light{100.f / 127.f};
    float          _rainbow{};
    uint32_t       _usec{};
    V2MIDI::Packet _midi;

    struct Valve {
      enum State { Idle, Down, Up } state{};
      float    max{};
      uint32_t downUsec{};
      uint32_t length{};
      uint32_t upUsec{};
    } _valves[Setup::nValves];

    struct {
      uint32_t msec{};

      struct {
        uint8_t yaw{};
        uint8_t roll{};
        uint8_t pitch{};
      } orientation;

      uint8_t pressure{};
    } _sensors;

    auto updateLEDs() -> void {
      for (uint8_t i{}; i < Setup::nValves; i++)
        if (hasCalibration(i))
          LED.brightness(0, Setup::valve(i));
        else
          LED.hsv({V2Colour::Magenta, 0.95, 0.5}, Setup::valve(i));
    }

    auto handleReset() -> void override {
      LED.reset();
      allNotesOff();
      LED.flash({V2Colour::Cyan, 0.9, 0.2}, 0.3);

      _calibrating = false;
      _light       = 100.f / 127.f;
      _rainbow     = 0;
      _sensors     = {};
    }

    auto handleLoop() -> void override {
      if (V2Base::getUsecSince(_usec) < 1000)
        return;

      _usec = V2Base::getUsec();

      if (_calibrating) {
        for (uint8_t i{}; i < Setup::nValves; i++) {
          auto analog{measureAnalog(PIN_CHANNEL_SENSE + i)};
          LED.brightness(analog, Setup::valve(i));
          if (fabs(analog - config.valves[i].calibration.up) > fabs(config.valves[i].calibration.up - config.valves[i].calibration.down))
            config.valves[i].calibration.down = analog;
        }

        return;
      }

      for (uint8_t i{}; i < Setup::nValves; i++) {
        if (fabs(config.valves[i].calibration.down - config.valves[i].calibration.up) < 0.1f)
          continue;

        if (_valves[i].upUsec > 0 && V2Base::getUsecSince(_valves[i].upUsec) > _valves[i].length) {
          send(_midi.setNoteOff(config.valves[i].up.channel, config.valves[i].up.note, 64));
          LED.brightness(0, Setup::valve(i));
          _valves[i].upUsec = 0;
        }

        switch (_valves[i].state) {
          case Valve::State::Idle:
            if (auto v{measureValve(i)}; v > config.valves[i].down.threshold) {
              if (_valves[i].upUsec > 0)
                send(_midi.setNoteOff(config.valves[i].up.channel, config.valves[i].up.note, 64));

              send(_midi.setNote(config.valves[i].down.channel, config.valves[i].down.note, 64));
              LED.hsv({V2Colour::Orange, 0.7, 0.4}, Setup::valve(i));
              _valves[i].max      = v;
              _valves[i].downUsec = V2Base::getUsec();
              _valves[i].state    = Valve::State::Down;
            }
            break;

          case Valve::State::Down: {
            auto v{measureValve(i)};
            if (v > _valves[i].max)
              _valves[i].max = v;

            // The threshold for "Up" is the fraction of the way back to the resting position.
            if (v > _valves[i].max * config.valves[i].up.threshold)
              break;

            send(_midi.setNoteOff(config.valves[i].down.channel, config.valves[i].down.note, 64));
            send(_midi.setNote(config.valves[i].up.channel, config.valves[i].up.note, 64));
            LED.hsv({V2Colour::Cyan, 0.9, 0.6}, Setup::valve(i));
            _valves[i].length = std::min(V2Base::getUsecSince(_valves[i].downUsec), uint32_t(3 * 1000 * 1000));
            _valves[i].upUsec = V2Base::getUsec();
            _valves[i].state  = Valve::State::Up;
          } break;

          case Valve::State::Up:
            if (measureValve(i) < config.valves[i].down.threshold / 2.f)
              _valves[i].state = Valve::State::Idle;
            break;
        }
      }

      if (_sensors.msec++; _sensors.msec > 20) {
        _sensors.msec = 0;

        if (config.orientation.enabled) {
          auto  q{Orientation.getRotation()};
          auto  e{V23D::Euler::quaternion({q.w, q.y, q.x, -q.z})};
          float colour{};

          if (auto y{uint8_t((e.yaw / std::numbers::pi_v<float> + 1.f) / 2.f * 127.f)}; _sensors.orientation.yaw != y) {
            colour = V2Colour::Orange;
            send(_midi.setControlChange(0, uint8_t(CC::Orientation) + 0, y));
            _sensors.orientation.yaw = y;
          }

          if (auto p{uint8_t((e.pitch / std::numbers::pi_v<float> + 1.f) / 2.f * 127.f)}; _sensors.orientation.pitch != p) {
            colour = V2Colour::Green;
            send(_midi.setControlChange(0, uint8_t(CC::Orientation) + 1, p));
            _sensors.orientation.pitch = p;
          }

          if (auto r{uint8_t((e.roll / std::numbers::pi_v<float> + 1.f) / 2.f * 127.f)}; _sensors.orientation.roll != r) {
            colour = V2Colour::Cyan;
            send(_midi.setControlChange(0, uint8_t(CC::Orientation) + 2, r));
            _sensors.orientation.roll = r;
          }

          if (colour > 0.f)
            LED.flash({colour, 0.9, 0.25}, 0.1, Setup::Orientation);
        }

        if (config.pressure.enabled) {
          auto                analog{measureAnalog(PIN_PRESSURE)};
          constexpr std::pair range{0.25f, 0.95f};
          if (analog < range.first)
            analog = 0;
          else if (analog > range.second)
            analog = 1;
          else
            analog = (analog - range.first) / (range.second - range.first);

          if (auto p{uint8_t(analog * 127.f)}; _sensors.pressure != p) {
            LED.hsv({V2Colour::Orange, 0.9, analog}, Setup::Pressure);
            send(_midi.setControlChange(0, uint8_t(CC::Pressure), p));
            _sensors.pressure = p;
          }
        }
      }
    }

    auto handleSend(V2MIDI::Packet* midi) -> bool override {
      usb.midi.send(*midi);
      Plug.send(*midi);
      return true;
    }

    auto handleControlChange(uint8_t channel, uint8_t controller, uint8_t value) -> void override {
      if (channel > 0)
        return;

      switch (controller) {
        case (uint8_t)CC::Home:
          Orientation.home();
          break;

        case (uint8_t)CC::Light:
          _light = (float)value / 127.f;
          if (_rainbow > 0.f)
            LED.rainbow(1, 4.5f - (_rainbow * 4.f), 0.2f + (0.8f * _light));
          break;

        case (uint8_t)CC::Rainbow:
          _rainbow = (float)value / 127.f;
          if (_rainbow <= 0.f)
            LED.reset();
          else
            LED.rainbow(1, 4.5f - (_rainbow * 4.f), 0.2f + (0.8f * _light));
          break;

        case V2MIDI::CC::AllNotesOff:
        case V2MIDI::CC::AllSoundOff:
          allNotesOff();
          break;
      }
    }

    auto handleSystemReset() -> void override {
      reset();
    }

    auto exportSettings(JsonArray json) -> void override {
      for (uint8_t i{}; i < Setup::nValves; i++) {
        {
          auto j{json.add<JsonObject>()};
          j["type"] = "title";
          char name[32];
          sprintf(name, "Valve %d", i + 1);
          j["title"] = name;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "toggle";
          j["label"] = "Down";
          char path[64];
          sprintf(path, "valves[%d]/down/enable", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "number";
          j["label"] = "Channel";
          j["min"]   = 1;
          j["max"]   = 16;
          j["input"] = "select";
          char path[64];
          sprintf(path, "valves[%d]/down/channel", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "note";
          j["label"] = "Note";
          char path[64];
          sprintf(path, "valves[%d]/down/note", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "number";
          j["label"] = "Threshold";
          j["text"]  = "Position";
          j["max"]   = 1;
          j["step"]  = 0.01;
          char path[64];
          sprintf(path, "valves[%d]/down/threshold", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "toggle";
          j["ruler"] = true;
          j["label"] = "Up";
          char path[64];
          sprintf(path, "valves[%d]/up/enable", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "number";
          j["label"] = "Channel";
          j["min"]   = 1;
          j["max"]   = 16;
          j["input"] = "select";
          char path[64];
          sprintf(path, "valves[%d]/up/channel", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "note";
          j["label"] = "Note";
          char path[64];
          sprintf(path, "valves[%d]/up/note", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "number";
          j["label"] = "Threshold";
          j["text"]  = "Position";
          j["max"]   = 1;
          j["step"]  = 0.01;
          char path[64];
          sprintf(path, "valves[%d]/up/threshold", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "number";
          j["ruler"] = true;
          j["label"] = "Calibration";
          j["text"]  = "Up";
          j["max"]   = 1;
          j["step"]  = 0.01;
          char path[64];
          sprintf(path, "valves[%d]/calibration/up", i);
          j["path"] = path;
        }
        {
          auto j{json.add<JsonObject>()};
          j["type"]  = "number";
          j["label"] = "Calibration";
          j["text"]  = "Down";
          j["max"]   = 1;
          j["step"]  = 0.01;
          char path[64];
          sprintf(path, "valves[%d]/calibration/down", i);
          j["path"] = path;
        }
      }

      {
        JsonObject j{json.add<JsonObject>()};
        j["type"]  = "title";
        j["title"] = "Sensors";
      }
      {
        JsonObject j{json.add<JsonObject>()};
        j["type"]  = "toggle";
        j["label"] = "Enable";
        j["text"]  = "Orientation";
        j["path"]  = "orientation/enabled";
      }
      {
        JsonObject j{json.add<JsonObject>()};
        j["type"]  = "toggle";
        j["label"] = "Enable";
        j["text"]  = "Pressure";
        j["path"]  = "pressure/enabled";
      }
    }

    auto importConfiguration(JsonObject json) -> void override {
      JsonArray jsonValves{json["valves"]};
      if (jsonValves) {
        for (uint8_t i{}; i < Setup::nValves; i++) {
          JsonObject jsonValve{jsonValves[i]};

          if (!jsonValve["down"].isNull()) {
            JsonObject j{jsonValve["down"]};

            if (!j["enable"].isNull())
              config.valves[i].down.enable = j["enable"];

            if (!j["channel"].isNull()) {
              uint8_t channel{j["channel"]};
              if (channel < 1)
                config.valves[i].down.channel = 0;
              else if (channel > 16)
                config.valves[i].down.channel = 15;
              else
                config.valves[i].down.channel = channel - 1;
            }

            if (!j["note"].isNull()) {
              config.valves[i].down.note = j["note"];
              if (config.valves[i].down.note < 0)
                config.valves[i].down.note = 0;
              else if (config.valves[i].down.note > 127)
                config.valves[i].down.note = 127;
            }

            if (!j["threshold"].isNull()) {
              config.valves[i].down.threshold = j["threshold"];
              if (config.valves[i].down.threshold < 0)
                config.valves[i].down.threshold = 0;
              else if (config.valves[i].down.threshold > 1)
                config.valves[i].down.threshold = 1;
            }
          }

          if (!jsonValve["up"].isNull()) {
            JsonObject j{jsonValve["up"]};

            if (!j["enable"].isNull())
              config.valves[i].up.enable = j["enable"];

            if (!j["channel"].isNull()) {
              uint8_t channel = j["channel"];
              if (channel < 1)
                config.valves[i].up.channel = 0;
              else if (channel > 16)
                config.valves[i].up.channel = 15;
              else
                config.valves[i].up.channel = channel - 1;
            }

            if (!j["note"].isNull()) {
              config.valves[i].up.note = j["note"];
              if (config.valves[i].up.note > 127)
                config.valves[i].up.note = 127;
            }

            if (!j["threshold"].isNull()) {
              config.valves[i].up.threshold = j["threshold"];
              if (config.valves[i].up.threshold < 0)
                config.valves[i].up.threshold = 0;
              else if (config.valves[i].up.threshold > 1)
                config.valves[i].up.threshold = 1;
            }
          }

          if (!jsonValve["calibration"].isNull()) {
            JsonObject j{jsonValve["calibration"]};
            if (!j["down"].isNull()) {
              float v{j["down"]};
              if (v < 0.f || v > 1.f)
                v = 1;
              config.valves[i].calibration.down = v;
            }

            if (!j["up"].isNull()) {
              float v{j["up"]};
              if (v < 0.f || v > 1.f)
                v = 0;
              config.valves[i].calibration.up = v;
            }
          }
        }
      }

      JsonObject jsonOrientation{json["orientation"]};
      if (jsonOrientation) {
        if (!jsonOrientation["enabled"].isNull())
          config.orientation.enabled = jsonOrientation["enabled"];
      }

      JsonObject jsonPressure{json["pressure"]};
      if (jsonPressure) {
        if (!jsonPressure["enabled"].isNull())
          config.pressure.enabled = jsonPressure["enabled"];
      }
    }

    auto exportConfiguration(JsonObject json) -> void override {
      auto jsonValves{json["valves"].to<JsonArray>()};
      for (uint8_t i{}; i < Setup::nValves; i++) {
        auto jsonValve{jsonValves.add<JsonObject>()};
        {
          auto j{jsonValve["down"].to<JsonObject>()};

          if (i == 0)
            j["#enable"] = "Send beat on key down";
          j["enable"] = config.valves[i].down.enable;

          if (i == 0)
            j["#channel"] = "The channel to send beats to";
          j["channel"] = config.valves[i].down.channel + 1;

          if (i == 0)
            j["#note"] = "The note number";
          j["note"] = config.valves[i].down.note;

          if (i == 0)
            j["#threshold"] = "The absolute valve position";
          j["threshold"] = config.valves[i].down.threshold;
        }

        {
          auto j{jsonValve["up"].to<JsonObject>()};

          if (i == 0)
            j["#enable"] = "Send beat on key up";
          j["enable"] = config.valves[i].up.enable;

          if (i == 0)
            j["#channel"] = "The channel to send beats to";
          j["channel"] = config.valves[i].up.channel + 1;

          if (i == 0)
            j["#note"] = "The note number";
          j["note"] = config.valves[i].up.note;

          if (i == 0)
            j["#threshold"] = "The relative valve position to the maximum position of this movement";
          j["threshold"] = config.valves[i].up.threshold;
        }

        {
          auto j{jsonValve["calibration"].to<JsonObject>()};

          if (i == 0)
            j["#down"] = "The maximum / 127 position (0 .. 1)";
          j["down"] = serialized(String(config.valves[i].calibration.down, 2));

          if (i == 0)
            j["#up"] = "The idle / minimum / 0 position (0 .. 1)";
          j["up"] = serialized(String(config.valves[i].calibration.up, 2));
        }
      }

      auto jsonOrientation{json["orientation"].to<JsonObject>()};
      jsonOrientation["enabled"] = config.orientation.enabled;

      auto jsonPressure{json["pressure"].to<JsonObject>()};
      jsonPressure["enabled"] = config.pressure.enabled;
    }

    auto exportInput(JsonObject json) -> void override {
      auto jsonControllers{json["controllers"].to<JsonArray>()};
      {
        auto j{jsonControllers.add<JsonObject>()};
        j["name"]   = "Home";
        j["type"]   = "momentary";
        j["number"] = uint8_t(CC::Home);
      }
      {
        auto j{jsonControllers.add<JsonObject>()};
        j["name"]   = "Light";
        j["number"] = (uint8_t)CC::Light;
        j["value"]  = (uint8_t)(_light * 127.f);
      }
      {
        auto j{jsonControllers.add<JsonObject>()};
        j["name"]   = "Rainbow";
        j["number"] = (uint8_t)CC::Rainbow;
        j["value"]  = (uint8_t)(_rainbow * 127.f);
      }
    }

    auto exportOutput(JsonObject json) -> void override {
      JsonArray jsonChannels = json["channels"].to<JsonArray>();
      for (uint8_t ch = 0; ch < 16; ch++) {
        JsonObject jsonChannel = jsonChannels.add<JsonObject>();
        jsonChannel["number"]  = ch;

        if (ch == 0) {
          auto jsonControllers{jsonChannel["controllers"].to<JsonArray>()};
          {
            auto j{jsonControllers.add<JsonObject>()};
            j["name"]   = "Pressure";
            j["number"] = uint8_t(CC::Pressure);
            j["value"]  = _sensors.pressure;
          }

          {
            auto j{jsonControllers.add<JsonObject>()};
            j["name"]   = "Orientation Yaw";
            j["number"] = uint8_t(CC::Orientation) + 0;
            j["value"]  = _sensors.orientation.yaw;
          }
          {
            auto j{jsonControllers.add<JsonObject>()};
            j["name"]   = "Orientation Pitch";
            j["number"] = uint8_t(CC::Orientation) + 1;
            j["value"]  = _sensors.orientation.pitch;
          }
          {
            auto j{jsonControllers.add<JsonObject>()};
            j["name"]   = "Orientation Roll";
            j["number"] = uint8_t(CC::Orientation) + 2;
            j["value"]  = _sensors.orientation.roll;
          }
        }

        {
          auto jsonNotes{jsonChannel["notes"].to<JsonArray>()};
          for (uint8_t i{}; i < Setup::nValves; i++) {
            if (config.valves[i].down.channel == ch && config.valves[i].down.enable) {
              char name[32];
              sprintf(name, "Valve %d – Down", i + 1);

              auto j{jsonNotes.add<JsonObject>()};
              j["name"]   = name;
              j["number"] = config.valves[i].down.note;
            }

            if (config.valves[i].up.channel == ch && config.valves[i].up.enable) {
              char name[32];
              sprintf(name, "Valve %d – Up", i + 1);

              auto j{jsonNotes.add<JsonObject>()};
              j["name"]   = name;
              j["number"] = config.valves[i].up.note;
            }
          }
        }
      }
    }

    auto exportSystem(JsonObject json) -> void override {
      JsonObject j{json["orientation"].to<JsonObject>()};
      j["product"]  = Orientation.getProductID();
      j["revision"] = Orientation.getRevisionID();
      j["software"] = Orientation.getRAMVersion();
    }
  } Device;

  // Dispatch Link packets.
  class Link : public V2Link {
  public:
    Link() : V2Link(&Plug, nullptr) {
      Device.link = this;
    }

  private:
    // Receive a host event from our parent device.
    auto receivePlug(V2Link::Packet& p) -> void override {
      switch (p.type) {
        case V2Link::Packet::Type::MIDI:
          Device.dispatch(&Plug, &p.midi);
          break;

        case V2Link::Packet::Type::Number: {
          // The sender pings with even numbers, we reply with an odd number.
          p.number(p.number() + 1);
          Plug.send(p);
          break;
        }
      }
    }
  } Link;

  // Dispatch MIDI packets
  class MIDI {
  public:
    void loop() {
      if (!Device.usb.midi.receive(_midi))
        return;

      if (_midi.port == 0)
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
    V2MIDI::Packet          _midi;

    void handleHold(uint8_t count) override {
      switch (count) {
        case 0:
          LED.rainbow(1, 2, 0.8);
          break;

        case 1:
          if (!Device.calibrating()) {
            Device.startCalibration();
            break;
          }

          Device.storeCalibration();
          break;
      }
    }

    void handleClick(uint8_t count) override {
      Device.reset();
      Orientation.home();

      for (uint8_t i{}; i < 16; i++) {
        Device.send(_midi.setControlChange(i, V2MIDI::CC::AllSoundOff, 0));
        Device.send(_midi.setControlChange(i, V2MIDI::CC::AllNotesOff, 0));
      }
    }
  } Button;
}

auto setup() -> void {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(800000);
  Wire.setTimeout(1);
  LED.begin();
  LED.brightnessMax(0.2);

  Link.begin();
  setSerialPriority(&SerialPlug, 2);

  for (auto& a : ADC)
    a.begin();

  for (uint8_t i = 0; i < Setup::nValves; i++) {
    auto id{V2Base::Analog::ADC::getID(PIN_CHANNEL_SENSE + i)};
    auto channel{V2Base::Analog::ADC::getChannel(PIN_CHANNEL_SENSE + i)};
    ADC[id].addChannel(channel);
  }

  ADC[V2Base::Analog::ADC::getID(PIN_PRESSURE)].addChannel(V2Base::Analog::ADC::getChannel(PIN_PRESSURE));

  // The fixed rotation quaternion was created by calibrating a V2 axis.
  Orientation.begin();
  Orientation.setup(true, V23D::Quaternion{-0.4234, -0.5965, 0.5438, -0.4114});

  Button.begin();
  Device.begin();
  Device.reset();
}

auto loop() -> void {
  LED.loop();
  MIDI.loop();
  Link.loop();
  Orientation.loop();
  V2Buttons::loop();
  Device.loop();

  if (Device.idle())
    Device.sleep();
}
