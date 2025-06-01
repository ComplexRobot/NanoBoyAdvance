Custom build that auto-saves sounds. Made to work with [pokefirered-tool](https://github.com/ComplexRobot/pokefirered-tool/tree/audio-tool).

* Reads in [metadata](https://github.com/ComplexRobot/pokegbasoundmetaparser) of music and sounds to perfectly time looping sound effects and music.
* Output files are [here](https://github.com/ComplexRobot/pokefirered-sound-rip).

This version:
* Complete rework of the sound ripping algorithm. It now directly accesses the GBA memory to precisely control sounds and find loop points.
  * Uses the export mapping file from the ROM to get data addresses.
* Altered the sound sampling APU to control the exact sampling rate. Now outputs directly at 48KHz (and can be set to any sampling rate desired - currently a compile-time constant).
* Several optimizations implemented to improve performance. (Experimental. Some increase performance by an order of magnitude others do almost nothing.)