# Kosmo 16 channel midi sequencer user manual
The Kosmo 16 channel midi sequencer allows you to program sequences, chords, drones, drum machines etc in 16 individual channels. Each channel has a specific machine attached to it that allows for specific behaviours.

<img width="800" alt="image" src="https://github.com/user-attachments/assets/46affeb4-1ce6-47f8-8de6-8da6acbe8301" />

*Front panel mock up*

## Getting started
To get started you must connect an external clock source. The Kosmo 16 channel midi sequencer expected clock signal with 24ppqn.
Also, make sure that you have at least one external device connected via either midi or USB.

Sequencer channels are by default mapped to separate midi channels so sequencer channel 1 sends and receives midi messages on midi channel 1. Sequencer channel 2 send and receive on midi channel 2 etc.

## Your first sequence
1. Power on the device and ensure the external clock is connected to clock-in and that the external device is listening on midi channel 1.
2. Long press the yellow button for channel 1 (channel select-button). This selects the channel and set it in program mode. Since the default machine is **Sequencer** the LED is yellow (other machines has other colors) and because you are in programming mode, the led is blinking.
3. Press the green button (channel config-button) to enable channel sending output. The LED will light green.
4. In the step rows the first 16 steps are lit in white indicating that the sequence length is set to 16 and that there are no active steps.
5. Press any of the step buttons. They will shift from white to yellow to indicate that they are active/will create sound when the clocked step reaches the step in the sequence.
6. Long press the channel select-button to exit programming mode. The LED is solid yellow.
7. When you start the clock, midi notes will be sent to the external midi device.
8. If you stay in programming mode, you can modify the sequence while it is playing.

## Changing the sequence
Changes are only possible while the channel is in edit mode. Press and hold the channel select-button until the LED starts to blink.

### Sequence length
To change the length of the sequence, press and hold the channel config-button and rotate the **Length**-button. You can set it to any length between 1 and 128 steps. As you rotate the button the length is indicated by inactive steps lit in white. If you reduce the length below an active led it will still be lit but the sequence will not get to it when playing.

### Change notes via step settings panel
When adding steps by pressing the step buttons, the default value for **Note 1** is A3. To change the value, press and hold the step button and rotate the Note1-button. You can set the note to any note within the channel *Scale*.

Up to four notes can be stored pr step.

### Change notes via external midi keyboard
If you have attached an external midi keyboard you can set the note value by striking a key on the midi keyboard while press and holding the step button. 

Up to four notes can be stored pr step. If you play a fifth note, it will replace the first note.

## Editing a step
Each step has a number of configuration options that will effect how the notes in the step behave.

### Disable a step
When pressing an active step button, while in edit mode, the step is deactivated. All settings are left untouched but no midi messages will be sent from that step.

### Resetting a step
When long pressing Note 1-button, while in edit mode, all step values are set to their default values.

### Copy a step
In edit mode you can long press one step and press another step button to copy all settings to that step. This operation can be repeated without releasing the original step to quickly get many copies of a single step.

### Step length
Determines when the note-off command is sent. The value is relative to the channel divider-value, so if set to 1 (one step=one 16th note) a step length of 2 means that the step will play for 2 16th notes. If the length of a step extends into other active steps, these will not produce note-on events. Other midi messages will be sent even though the notes themselves are ignored.

### Step volume
Determines the volume of all notes in the step. This is baked into the note-on messages as the intensity value.

### Step envelope
Tightly coupled to the step volume, the step envelope allows you to specify a volume algorithm for the step The volume algorithm starts with the note on message and ends with the note off message. Min value is 0 and the max value is the step volume. The volume changes will be sent as CC messages on CC#7.

| Envelope type   | Volume behavior |
|-----------------|---------------------------|
| Gat (gate)      | Plays the sound with full volume |
| SaV (saw)       | Plays the sound witha a increasing volume |
| Ra2 (racthet 2) | Repeats the sound 2 times within the step length |
| Ra3 (racthet 3) | Repeats the sound 3 times within the step length |
| R43 (racthet 4) | Repeats the sound 4 times within the step length |

### Step CCs
You can configure up to 3 midi CCs pr step. These will be sent with the note-on message(s). Message for CC#7 will be ignored as they are reserved by the envelope setting.

### Step program change
You can configure one midi program change pr step. Note that not all external hardware can comply with many fast program changes. The program change will be sent before any other midi messages. The intended use is to set it at the beginning of a sequence if required.

### Step trigger
The trigger decides under which circumstance the step will send midi messages. 
| Trigger | Behavior | Comments |
|---------|----------|----------|
| 1.1     | Plays every time | (default) |
| 1.2     | Plays on 2nd repeat | |
| 1.3     | Plays on 3rd repeat | |
| ...     | ... | |
| 1.8     | Plays on 8th repeat | |
| FSt     | Plays first time | |
| LSt     | Plays last time | This only works on combination with Song Manager repeats |
| FSt2    | Plays first two | |
| LSt2    | Plays last two | This only works on combination with Song Manager repeats |
| FSt3    | Plays first three | |
| LSt3    | Plays last three | This only works on combination with Song Manager repeats |

### Saving and discarding changes
When in edit mode you can save changes by pressing the channel select-button. To discard changes since the last change you can long press the channel select-button. In both cases the LED stops blinking.

## Channel settings
Each channel can be toggled on or off where off effectively prevents midi messages to be sent and there by muting any external devices on that midi channel. This happens when pressing the channel edit-button. A channel can be toggled on or off without it being selected.

Other channel settings are accessed by pressing and holding the channel edit button while changing the parameters.

### MIDI Channel
Determines the midi channel on which the channel send and receive midi messages. The default is channel 1 => MIDI channel 1, channel 2 => MIDI channel 2 etc.

### Channel divider
Determines the tempo the channel advances one step.
| Divider | Note length |
|---------|-------------|
| 1       | 1/16 (default) |
| 2       | 1/8 |
| 4       | 1/4 |
| 8       | 1/1 |
| 16      | 2/1 |
| 32      | 4/1 |

### Channel length
Determines the maximum length of the sequence. Min value is 1, max value is 128.

### Channel volume
Set the max volume for any step in the sequence. If steps has a value higher than the channel volume, the step volume is cut at the channel volume. The stored step value is not effected. 

### Channel scale
Determines which scale to lock step notes to.
| Name | Scale | Tones from base tone | Notes in C-scale |
|------|-------|----------------------|------------------|
| CHr  | Chromatic (default) | all 12 | c c# d d# e f f# g g# a a# b |
| PMa  | Pentatonic Major | 0 2 4 7 9 | c d e g a |
| PMI  | Pentatonic Minor | 0 3 5 7 10 | c d# f g a# |
| BLU  | Blues | 0 3 5 6 7 10 | c d# e f# g a# |

# Play Mode
When channels are playing the different machines can react to incoming midi notes. Here the channel scale setting plays a role in what happens when midi notes arrives.

Sequencer: Single note transposes the entire sequence to the closes note in the scale relative to the incoming note. If 2 notes are evenly close to the incoming note, the lower note is chosen. Multiple notes - the last note is used as the single note. Sequencer does not support multiple notes for transposing.

Drum machine: not effect

Chord machine: Single note transposes the root note and uses the channel scale to determine the other notes as per their individual settings.

# Machines

## Sequencer
| Step parameter | What it does | Comments |
|----------------|--------------|----------|
| Note 1 | Define the note to play | |
| Note 2 | Define the note to play with note 1 | |
| Note 3 | Define the note to play with note 1 and 2 | |
| Note 4 | Define the note to play with note 1, 2 and 3 | |
| Length | Sets the length of the note | (see above ) |
| Volume | Sets the volume of the note | (see above) |
| CC1 | Define which CC to sent | |
| CC2 | Define which CC to sent | |
| CC3 | Define which CC to sent | |
| ENV | Defines the envelope of the note(s) volume | (see above) |
| Program | Defines which program change to send | (see above )|
| Trigger | Defines when to send the midi messages | (see above) |

| Channel parameter | What it does | Comments |
|-------------------|--------------|----------|
| Divider | Defines the tempo with which the step advances | (see above) |
| Length | Defines the length of the sequence | |
| Volume | Defines the maximum volume of notes in the sequence | |
| Scale | Defines the notes that can be in a sequence | |


## Chord
Default channel divider is 4 (every step is a 1/4 note).

| Step parameter | What it does | Comments |
|----------------|--------------|----------|
| Note 1 | Define the root note. Scale determines the next 2 notes of the base chord | |
| Note 2 | Define the 4th note (1-48) relative to the root and scale | |
| Note 3 | Define the 5th note (1-48) relative to root and scale | |
| Note 4 | Define a bass note, 1-3 octaves below the root| |

## Drone

## Arpeggio
The arpegiator machine uses the ENV to decide patterns. The list of patterns have not yet been defined, but it will have the useual suspects.

The 4 notes and the scale will be the foundation of which notes to be in the arpeggio.

Example:
Channel Scale = Pentatonic Minor
Channel Divider = 1 (1/16 notes)
Channel Length = 16
Channel Pattern = ud1 (up-down-1-octave)
Step 1 Note 1 = C3
This will automatically fill note2-16 as follows
| Step | Note |
|------|------|
| 1 | c3 |
| 2 | d#3 |
| 3 | f3 |
| 4 | g3 |
| 5 | a#3 |
| 6 | g3 |
| 7 | f3 |
| 8 | d#3 |
| 9 | c3 |
| 10 | d#3 |
| 11 | f3 |
| 12 | g3 |
| 13 | a#3 |
| 14 | g3 |
| 15 | f3 |
| 16 | d#3 |


## Drum
The drum machine arranges the channel in 8 individual tracks where Note 1 defines how the track maps to the external midi device.

Step 1, 17, 33, 49, 65, 81, 97 and 113 acts as track edit buttons as well as the first step in each track. Long pressing enables the following
Set track length - min 1 max 64 steps
Set track volume (backed into the individual midi note on messages)
Set CCs
Set Trigger

We need some way to page through the pages (1-4) if any track has a length longer than 16. TBD
