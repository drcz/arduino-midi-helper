#include<MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

#define MIDI_OUT_CHANNEL 1
#define LATCH_SWITCH_PIN 2

struct {
  union {
    struct {
      byte is_pressed : 1;
      byte is_held : 1;
    };
    byte bits;
  };
} keys_state[256] = { 0 };

enum { LATCH_OFF, LATCH_WAITING, LATCH_ACCUMULATING };
byte latch_mode = LATCH_OFF;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LATCH_SWITCH_PIN, INPUT);
  MIDI.begin(MIDI_CHANNEL_OMNI); // nb it listens to all channels
  MIDI.turnThruOff(); // we'll take care of it instead
}

void loop() {
  byte note, velocity;
  if(digitalRead(LATCH_SWITCH_PIN)==HIGH) latch_on(); else latch_off();
  if(MIDI.read()) {
    switch(MIDI.getType()) {
      case midi::NoteOn:
        note = MIDI.getData1();
        velocity = MIDI.getData2();
        if(velocity==0) { key_unpress(note); }
                   else { key_press(note); }
        break;
      case midi::NoteOff:
        note = MIDI.getData1();
        key_unpress(note);
        break;
    }
  }
}

inline void key_press(byte note) {
  switch(latch_mode) {
    case LATCH_WAITING:
      latch_mode = LATCH_ACCUMULATING;
      unpress_held_keys();
      /// fallthrough...
    case LATCH_ACCUMULATING:
      keys_state[note].is_held = 1;
      /// fallthrough...
    case LATCH_OFF:
      keys_state[note].is_pressed = 1;
      MIDI.send(midi::NoteOn, note, 64, MIDI_OUT_CHANNEL);
  }
}

inline void key_unpress(byte note) {
  keys_state[note].is_pressed = 0;
  switch(latch_mode) {
    case LATCH_OFF:
      MIDI.send(midi::NoteOff, note, 0, MIDI_OUT_CHANNEL);
      break;
    case LATCH_ACCUMULATING:
      if(are_all_keys_unpressed()) latch_mode = LATCH_WAITING;
      break;
    case LATCH_WAITING:
      break;
  }
}

inline void latch_off() {
  if(latch_mode==LATCH_OFF) return;
  digitalWrite(LED_BUILTIN, LOW);
  latch_mode = LATCH_OFF;
  unhold_all_keys();
}

inline void latch_on() {
  if(latch_mode!=LATCH_OFF) return;
  digitalWrite(LED_BUILTIN, HIGH);
  latch_mode = LATCH_ACCUMULATING;
  set_all_pressed_as_held();
}

inline void unpress_held_keys() {
  int i;
  for(i=0;i<256;i++) {
    if(keys_state[i].is_held) {
      keys_state[i].is_held = 0;
      MIDI.send(midi::NoteOff, i, 0, MIDI_OUT_CHANNEL);
    }
  }
}

int are_all_keys_unpressed() {
  int i;
  for(i=0;i<256;i++) {
    if(keys_state[i].is_pressed) return 0; 
  }
  return 1;
}

void set_all_pressed_as_held() {
  int i;
  for(i=0;i<256;i++) {
    keys_state[i].is_held = keys_state[i].is_pressed; 
  }
}

/// the whole point of this is to leave the pressed ones when turning latch off y'know
void unhold_all_keys() {
  int i;
  for(i=0;i<256;i++) {
    if(keys_state[i].is_held) {
      keys_state[i].is_held = 0;
      if(!keys_state[i].is_pressed) MIDI.send(midi::NoteOff, i, 0, MIDI_OUT_CHANNEL);
    }
  }  
}
