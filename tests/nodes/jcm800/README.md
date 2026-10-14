# tests/nodes/jcm800

## `test_jcm_tone_stack.cpp`

### `JcmToneStackAtEightTimesRateMatchesBaseRateWithinFifthOfADecibel`

`// Float coefficient quantisation stays within 0.2 dB.`

The stack moves inside the 8x region in D6a; at fs/f0 \~ 3840 the float biquads' coefficient
quantisation must not shift the response by more than 0.2 dB.
