Custom build that auto-saves sounds. Made to work with [pokefirered-tool](https://github.com/ComplexRobot/pokefirered-tool/tree/audio-tool).

* Reads in [metadata](https://github.com/ComplexRobot/pokegbasoundmetaparser) of music and sounds to perfectly time looping sound effects and music.
* Output files are [here](https://github.com/ComplexRobot/pokefirered-sound-rip).

This version:
* Bug fix for the song metadata. It now takes into account when the tempo changes mid-song for correct timing.
* Metadata for duration and loop points is now in seconds for accuracy and ease-of-use.