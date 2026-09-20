# Where the audio stops being flash independent

The sidetone path is in IRAM now and the crunch is gone. This is the part that
is still flash resident, written down so it is not a surprise later.

## What IRAM actually bought

Moving `continueTone`, `startToneInternal`, `stopTone`, `getVolumeScale`,
`toneAmp` and the audio task itself into IRAM means a flash cache stall cannot
stall the code that decides what the next sample is. Combined with the DMA ring
going from 8 buffers to 16, about 46ms of audio, a stall has to be long and
unlucky before anything is audible.

On hardware that was enough. The crunch that prompted the work is gone,
including on the panel that forced the SPI clock down to 40MHz.

## Two places it does not reach

**`i2s_write` is in flash.** Every tone refill calls it with `portMAX_DELAY`.
If a cache stall lands while the CPU is inside that call, it stalls there, and
IRAM_ATTR on the caller does not change that.

There is no fix short of driving the I2S registers directly from IRAM. Do not
reach for `ESP_INTR_FLAG_IRAM` as a shortcut. The legacy IDF I2S handler is not
IRAM safe and adding that flag crashes. The 16 buffer ring is the mitigation,
not a workaround waiting to be replaced.

**The paddle callback is in flash.** `samplePaddleInput` is in IRAM but calls
whatever is in `paddleCallback`, which is ordinary application code.

Scope is narrow: there is exactly one callback in the tree,
`composePaddleCallback`. It goes in when the Morse Note compose screen is
built and comes out in `cleanupComposeKeyer`, so it is live for that whole
screen, not just the moments you are actually keying. It also comes out while
the text field has focus and goes back in when the field is defocused, since
you are typing then, not keying. Every other mode leaves it null.

Worth being precise about the consequence. This runs in a task, not an
interrupt, so a cache stall there stalls, it does not fault. Faulting on flash
access needs the cache actually disabled, which is the NVS commit case, and
that is why per keypress settings go through `markDeferredSave` instead of
writing straight through.

Pulling that callback into IRAM means pulling the keyer class in with it, since
it calls `key()` and `tick()`. That is an IRAM budget question, not a one line
change.

## If the crunch ever comes back

Check these in order before touching the audio:

1. Did something start writing to NVS on a keypress without going through
   `markDeferredSave`? That disables the cache on both cores and no amount of
   IRAM helps.
2. Did a mode lose its `MODE_FLAG_AUDIO_CRITICAL` flag? Without it the loop
   runs at 10ms instead of 1ms.
3. Did something start logging from the audio task? Serial writes block on USB
   CDC and that is worse than the problem being debugged. This was measured,
   the instrumented build was audibly worse than the clean one.
