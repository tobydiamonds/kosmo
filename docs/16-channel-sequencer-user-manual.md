# Kosmo 16 channel midi sequencer user manual
The Kosmo 16 channel midi sequencer allows you to program sequences, chords, drones, drum machines etc in 16 individual channels. Each channel has a specific machine attached to it, the allows for specific behaviors.

To get started you must connect an external clock source. The Kosmo 16 channel midi sequencer expected clock signal with 24ppqn.
Also, make sure that you have at least one external device connected via either midi or USB.

Sequencer channels are by default mapped to separate midi channels so sequencer channel 1 sends and receives midi messages on midi channel 1. Sequencer channel 2 send and receive on midi channel 2 etc.

## Your first sequence.
Power on the device and ensure the external clock is connected to clock-in and that the external device is listening on midi channel 1.

Long press the yellow button for channel 1 (channel select-button). This selects the channel and set it in program mode. Since the default machine is **Sequencer** the LED is yellow (other machines has other colors) and because you are in programming mode, the led is blinking.

Press the green button (channel config-button) to enable channel sending output.

In the step rows the first 16 steps are lit in white indicating that the sequence length is set to 16 and that there are not active steps.

Press any of the step buttons. They will shift from white to yellow to indicate that they are active/will create sound when the clocked step reaches the step in the sequence.

Long press the channel select-button to exit programming mode.

When you start the clock, midi notes will be sent to the external midi device.

If you stay in programming mode, you can modify the sequence while it is playing.

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

### Saving and discarding changes
When in edit mode you can save changes by pressing the channel select-button. To discard changes since the last change you can long press the channel select-button. In both cases the LED stops blinking.
