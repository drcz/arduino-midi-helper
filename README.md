# Arduino MIDI helper
![a photo of the gadget](/img/the-gadget.png?raw=true "the gadget")

This is a small gadget which solves the problem of notes "getting lost" over MIDI. It also adds latch mode. I doubt you'd find it interesting, I just write this to not forget what I figured out y'know.


## The problem
When I use Behringer's Solina String Ensemble with either Arturia's Keystep, or their [absolutely brilliant] tiny synth Minibrute (mk1), it all works nicely. The only downside is they only have 32 and 25 keys _resp._, while I wanted to enjoy the full 49-notes range of the device. I plugged in my Yamaha DX27 (glorious 5 octaves, _i.e._ 61 keys) and... it seemed to lose some notes! Especially when playing faster or pressing chords... Not cool.
I searched some forums and it seems to be the case with some of Behringer's gear (didn't happen with my mono synths tho) but I found no solutions other than update the firmware. I didn't find the latter on Uli's website so no cigar.


## The hint
![a photo of MIDI2USB cable](/img/the-cable.png?raw=true "the cable")

At some point I tried to play Yamaha's MIDI Out via laptop: I plugged both Yamaha and Solina to 2xMIDI->USB cable and did the following (I'm using ALSA driver with its basic tools):
```
aconnect -l
```
which lists all the visible MIDI devices and their ALSA ports: my cable was on ports `20:0` (MIDI #1, connected to Yamaha) and `20:1` (MIDI #2, connected to Solina), so I just routed one to the other:
```
aconnect 20:0 20:1
```
...aaand it worked! No notes lost however fast I pressed them. Awesome, but also strange... Earlier I also tried MIDI Thru on some other devices (probably the Keystep and Behringer Crave, maybe something else too), and it was losing the notes just as with direct connection. So it seems there's something ALSA does which solves the problem.

I tried
```
midisnoop
```
to check what comes out of Yamaha and the only difference I noticed was that while Keystep sends `Note off` command (`0x80`) when the key gets released, the Yamaha sends `Note on` (`0x90`) with velocity `0`. According to MIDI spec this is the same, but keep that in mind. I also tried:
```
aseqdump -p 20:0 | grep -v "Active Sensing"
```
(because y'know, it's "sensing" all the time) and it sees the release as `Note off`, but doesn't show if its `0x80` or `0x90` so who knows...

_Nb the command byte contains channel number as the last digit so if instead of channel 1  one uses, say, channel 3, these commands would be `0x82` and `0x92` resp._


## The failure
Most of the time I was just using Keystep, occasionally plugging in the laptop (phew!) and didn't bother until one day I got absurdly cheap MIDI shield "compatible with Arduino", so I gave it a try. As expected the Thru didn't help at all, neither did the obvious byte-by-byte rewrite with Arduino's `Serial` library:
```
void setup() {
  Serial.begin(31250);
}

void loop() {
  while(Serial.available() > 0) Serial.write(Serial.read());
}
```
-- the notes were getting lost. I had some more ambitious takes (grouping by 3 bytes, randomly altering baudrate, shits like that), but each one failed. After a few hours I gave up and took a nap.


## The revelation
I was about to fall asleep with headphones on when I recalled I could never get correct timing of this `loop()` thing. 8 yrs ago I tried to build a digital oscillator bank and they were annoyingly noisy (not the kind of PWM Nick Batt is after, trust me). Some other year I tried generating PAL signal from scratch and the TV set didn't like even the basic sync signal... And that time I ended up with [Myles Metzer's brilliant TVout library](https://docs.arduino.cc/libraries/tvout/).
Surely there must be some MIDI magic by people who know this ATmega timig stuff... I got up and found [Francois Best's library](https://github.com/FortySevenEffects/arduino_midi_library) -- bingo!


## The solution
I took the example and altered it to simply pass the `Note on` and `Note off` commands, like this:
```
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff(); // no thanks, we'll deal with that on our own
  
}

void loop() {
  if(MIDI.read()) {
    switch(MIDI.getType()) {
      case midi::NoteOn:
        MIDI.send(midi::NoteOn, MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
        break;
      case midi::NoteOff:
        MIDI.send(midi::NoteOff, MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
        break;
    }
  }
}
```
...and it worked like a charm! Surely I could also pass `Pitch bend` (`0xE0`) but Solina ignores it, along with velocity byte (which is nice since my Yamaha ain't velocity sensitive anyway).


## The latch.
I then thought that since the solution is just a piece of software, I could make my life [even more] exciting and add the latch mode, y'know that thing which holds the notes you've pressed until you press some new ones. Pretty useful when I need free hands to shred on a synth, or _most importantly_ to do the birdy space lasers thing (that's the whole point of owning a Solina with phaser, isn't it?)
In the past I used the Keystep's arpegiator, setting the clock rate to "insane" by tapping the _Tap_ button as fast as possible, and setting Solina's _Sustain_ to max, which kind of works but also creates funky volume artifacts (some are _really_ nice, e.g. on the opening track for [Perestroika in Reverse](https://archive.org/details/IP-017)) and most of all, the Yamaha doesn't have arp mode...

I experimented with some latch strategies and the solution that felt most natural to use turned out to be just a 2-state machine: initially the controller is in `ACCUMULATING` state: all the `Note on` commands are remembered and passed through, while the `Note off` ones are remembered but blocked. Once all the notes which got `on` got also `off`, _i.e._ when the player released all the keys, the state switches to `WAITING`: once it receives any new `Note on` (mind `Note off` at this state is not possible), it immediately sends one `Note off` for each `Note on` remembered in `ACCUMULATING` state, then passes and remembers the `Note on` which triggered it, and switches to `ACCUMULATING`. Easy.

All I needed was a simple SPDT switch: I soldered its terminals to _+5V_, _Pin 2_ and _Gnd_ respectively, made the controller monitor pin 2's state and life was good!

![a photo of the switch](/img/the-switch.png?raw=true "the switch")

The only tricky part was keeping track of keys when switching from/to the latch mode: when the latch goes on, we want the keys currently pressed (and nothing else) to be held; when the latch goes off, only the currently pressed ones should be preserved, while all the other ones should get `Note off`. The implementation is trivial once you know about these considerations.

As a final touch I got a small plastic kitchen container, cut out holes for MIDI and power socket and a small hole for the switch. As the poet said _"Jest piękny it's beautiful"_.
