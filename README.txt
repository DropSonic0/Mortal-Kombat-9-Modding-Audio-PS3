MK9Tool for PS3 - Quick Guide
============================

1. EXTRACTION
Run: MK9Tool.exe <file.xxx> OR MK9Tool.exe <file.fsb>
If extracting .XXX:
- header.bin / data.bin (The raw UE3 package parts)
- audio_X.fsb (Internal FMOD sound banks)
- audio_X_samples/ (Folder with individual audio tracks names)

If extracting .FSB:
- Folder with individual .bin audio tracks.

2. HOW TO CREATE NEW AUDIO (FSB)
To replace audio, you need to encode your WAVs into the FMOD format used by the game:
a) Download "FMOD Designer" (Version 4.30 - 4.34 is recommended).
b) Create a new project and add your WAV files.
c) In the "Music" or "SoundBank" properties, set the compression format to "ADPCM" or "PCM16".
d) Build the project. This will generate a .fsb file.

3. REPLACING AUDIO
Option A (Easy): Replace a single sound by its name.
Run: MK9Tool.exe patch <file.xxx> <sample_name> <your_new_sample.bin>
*The new sample must be the raw data from your new FSB or a compatible encoded file.

Option B (Advanced): Replace an entire sound bank.
Run: MK9Tool.exe inject <file.xxx> <your_new_bank.fsb> <offset_hex>
*Use the offset shown by MK9Tool during extraction.

4. REBUILDING .XXX
Run: MK9Tool.exe <header.bin> <data.bin> <new_file.xxx>
