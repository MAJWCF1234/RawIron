# Native shader-port notices

Raw Iron's native shader sources contain clean integrations or permitted ports of the effects listed below.
The original ReShade-format sources are removed only after the replacement is built and covered by regression tests.

## Crop and Resize

The centered crop/resize mapping in `NativeComposite.frag` and `NativeHybridComposite.frag` is based on
`CropResize/Resizer.fx` by Edward Jeffrey and its VirtualResolution-derived portions by Lucas Melo.

Copyright (C) 2025 Edward Jeffrey
Copyright (c) 2017 Lucas Melo

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

# Barbatos uFakeHDR

`NativeEffects/barbatos_fake_hdr.rishader`, its native composite implementation, and
`NativeEffects/Textures/Barbatos_LUT_Atlas.png` are derived from uFakeHDR version 3.2 by Barbatos.
The source declares the work CC0 (public-domain dedication).
