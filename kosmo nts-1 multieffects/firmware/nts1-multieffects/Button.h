#ifndef Button_h
#define Button_h

class Button {
public:
  Button(uint8_t pin, unsigned long longPressMs = 600, unsigned long debounceMs = 40)
    : _pin(pin), _longPressMs(longPressMs), _debounceMs(debounceMs) {}

  void begin() {
    pinMode(_pin, INPUT_PULLUP);
  }

  void update(unsigned long nowMs) {
    bool raw = !digitalRead(_pin);  // active LOW with pull-up

    if (raw != _lastRaw) {
      _lastDebounceTime = nowMs;
      _lastRaw = raw;
    }

    if ((nowMs - _lastDebounceTime) >= _debounceMs) {
      if (raw != _stable) {
        _stable = raw;
        if (_stable) {
          _pressTime = nowMs;
          _longFired = false;
        } else {
          if (!_longFired) {
            _shortPressEvent = true;
          }
        }
      }
    }

    if (_stable && !_longFired && (nowMs - _pressTime >= _longPressMs)) {
      _longFired = true;
      _longPressEvent = true;
    }
  }

  bool wasShortPress() {
    if (_shortPressEvent) { _shortPressEvent = false; return true; }
    return false;
  }

  bool wasLongPress() {
    if (_longPressEvent) { _longPressEvent = false; return true; }
    return false;
  }

  bool isDown() const { return _stable; }

private:
  uint8_t _pin;
  unsigned long _longPressMs;
  unsigned long _debounceMs;

  bool _lastRaw = false;
  bool _stable = false;
  bool _longFired = false;
  bool _shortPressEvent = false;
  bool _longPressEvent = false;
  unsigned long _lastDebounceTime = 0;
  unsigned long _pressTime = 0;
};

#endif
