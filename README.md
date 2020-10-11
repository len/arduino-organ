# arduino-organ
Combo organ with 12 arduinos.

This is a project that uses 12 arduinos as tone generator modules for a combo organ of 4 octaves (48 notes, C to B). The design is inspired in the classic Vox Continental and other transistor organs that work with 12 identical tone generator modules, each tuned to a different note, and using frequency dividers to produce the pitches of different octaves.

Each tone generator outputs the combined sound of 7 waves corresponding to a fixed note at 7 different octaves. The whole organ effectively emulates a tonewheel organ with 84 tone wheels.

It was developed on Arduinos Nano, and all the pins are used, so it would have to be adapted (and simplified) in order to use Arduinos with fewer pins.

The tone generator module has the following analogue inputs:
 - wave shape (continuosly varying wave shape, a Hammond-like sine wave in the middle with a special wave for the lowest tonewheel, a Vox Continental-like reed wave to the left, and a Farfisa-like wave to the right);
 - tuning control (wach tone generator for each note can be tuned individually);
 - vibrato (slow and fast, and control over the depth);
 - 5 drawbars (just like a Vox Continental).

The wave shape, vibrato and 5 drawbars are global (7 pots, each with connections to each of the 12 tone generators). The tuning control uses one pot per tone generator (12 pots for tuning).

Each tone generator module uses also 12 digital inputs to receive note-on/note-off information (the keys are just switches to ground). For each for the 4 octaves it receives the the note-on/off for the key corresponding to that tone module's note, the fourth and the flat sixth. (For example, the tone generator module for C is connected to keys C, F and A flat in all 4 octaves). From the opposite point of view, this means that each key switch has 3 cables going to 3 different tone generators: the one corresponding to this note, the fifth and the third (for example a C key is connected to tone generators for C, E and G).

A tone generator module uses the information from the keys pressed and the drawbars to determine the amplitude for each of the 7 waves it generates and combines them in a single output. It also emulates tonewheel crosstalk, an artifact of Hammond organs. Key click is not explicitly modeled, but a click is naturally produced due to the waves not starting at a zero point when a key is pressed (the tonewheels are always turning, regardless of keys pressed or not).

Each tone generator module produces 14-bits audio using dual PWM (for this we use the Mozzi library), but the final mix of the 12 tone generators is analogue. Typically, a single key pressed would produce sound from 3 tone generators.


