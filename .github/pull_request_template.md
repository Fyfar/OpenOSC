## What this changes

<!-- Describe the change and why it is needed. Link the issue: Fixes #123 -->

## Testing

- [ ] `pio run` builds all four environments without new warnings
- [ ] Tested on ZT703S
- [ ] Tested on ZT706S
- [ ] Not tested on hardware (explain why)

<!-- Describe what you checked on the device. -->

## Checklist

- [ ] The change does not write to the factory block (`0x08000000`–`0x0801FFFF`)
- [ ] `vendor/` is unchanged, or a third-party component is updated as a whole
- [ ] README updated if the behaviour or the installation changed
