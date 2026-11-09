# Drumulator sounds go here

Put the twelve WAV files of the E-mu Drumulator sounds (`drumu bass drm`, `snare`, `clap`, `hi hat cld`, `hi hat opn`,
`hi tom`, `mid tom`, `lo tom`, `rim`, `clave`, `cow bell`, `cymbal`) in this folder. If the 16k set (`...-16k.wav`, 15.6 kHz)
is there it is used, at its own rate. `*.wav` files here are git-ignored: the pack is not part of this repository, and its
licence is not known to me. Check it before you share a firmware image that contains the sounds.

Three kits are made from them, `DRMU1` to `DRMU3` (twelve sounds, eight lanes): the same bass drum, snare, claps and hats, and

| kit | TOM | RIM | BELL |
|---|---|---|---|
| `DRMU1` | mid tom | rim | cow bell |
| `DRMU2` | lo tom | clave | cymbal |
| `DRMU3` | hi tom | rim | cymbal |
